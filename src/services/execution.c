#include <pangu/services/execution.h>
#include <pangu/services/schema.h>
#include <uv.h>
#include <yyjson.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SL(s) ((pangu_slice){s,sizeof(s)-1})
#define MAX_BYTES 51200u
typedef struct {
  pg_execution_tools_config config;
  pg_tools_service service;
  pangu_tool tools[4];
} execution;
typedef struct {
  char bytes[MAX_BYTES+1];
  size_t size, total, lines;
  int fd;
  char *path;
  const char *directory;
} accumulator;
static int write_all(int fd, const char *data, size_t size) {
  uv_fs_t req;
  while (size) {
    size_t amount = size > 65536 ? 65536 : size;
    uv_buf_t b = uv_buf_init((char *)data, (unsigned)amount);
    int n = uv_fs_write(NULL, &req, fd, &b, 1, -1, NULL);
    uv_fs_req_cleanup(&req);
    if (n <= 0) return 0;
    data += n; size -= (size_t)n;
  }
  return 1;
}
static pangu_status append(void *ctx, pangu_slice data) {
  accumulator *a = ctx;
  for (size_t i=0; i<data.len; ++i) a->lines += data.data[i]=='\n';
  if (data.len > SIZE_MAX-a->total) return PANGU_NOMEM;
  a->total += data.len;
  if (a->fd < 0 && (a->total > MAX_BYTES || a->lines >= 2000)) {
    char tmp[4096]; size_t size=sizeof(tmp);
    const char *dir=a->directory;
    if (!dir) { if (uv_os_tmpdir(tmp,&size)) return PANGU_TOOL_ERROR; dir=tmp; }
    size_t n=strlen(dir);
    a->path=malloc(n+32);
    if (!a->path) return PANGU_NOMEM;
    snprintf(a->path,n+32,"%s/pangu-output-XXXXXX",dir);
    uv_fs_t req;
    a->fd=uv_fs_mkstemp(NULL,&req,a->path,NULL);
    if (a->fd>=0) strcpy(a->path,req.path);
    uv_fs_req_cleanup(&req);
    if (a->fd<0 || !write_all(a->fd,a->bytes,a->size)) return PANGU_TOOL_ERROR;
  }
  if (a->fd>=0 && !write_all(a->fd,data.data,data.len)) return PANGU_TOOL_ERROR;
  if (data.len>=MAX_BYTES) {
    memcpy(a->bytes,data.data+data.len-MAX_BYTES,MAX_BYTES); a->size=MAX_BYTES;
  } else {
    size_t drop=a->size+data.len>MAX_BYTES?a->size+data.len-MAX_BYTES:0;
    memmove(a->bytes,a->bytes+drop,a->size-drop); a->size-=drop;
    memcpy(a->bytes+a->size,data.data,data.len); a->size+=data.len;
  }
  a->bytes[a->size]=0;
  return PANGU_OK;
}
static void finish(accumulator *a) {
  if (a->fd>=0) { uv_fs_t req; uv_fs_close(NULL,&req,a->fd,NULL); uv_fs_req_cleanup(&req); a->fd=-1; }
}
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args) {
  execution *e=ctx; return e->config.validate(e->config.validator_ctx,schema,args);
}
static pangu_status validate_arguments(void *ctx,pangu_slice schema,pangu_slice args,
    pangu_arguments_sink sink,void *sc) {
  execution *e=ctx;
  return e->config.validate_arguments ?
    e->config.validate_arguments(e->config.validator_ctx,schema,args,sink,sc) :
    pg_schema_normalize(NULL,schema,args,sink,sc);
}
static const char *string(yyjson_val *root,const char *key,const char *fallback) {
  yyjson_val *v=yyjson_obj_get(root,key);
  if (!v) return fallback;
  if (!yyjson_is_str(v) || memchr(yyjson_get_str(v),0,yyjson_get_len(v))) return NULL;
  return yyjson_get_str(v);
}
typedef struct {
  char output[MAX_BYTES+1]; size_t size, count, limit;
  char *pending; size_t pending_size;
  int grep, capped, long_line;
  const char *base;
} search_output;
static pangu_status search_line(search_output *s,const char *line,size_t n) {
  yyjson_doc *d=NULL;
  char prefix[4096]; const char *text=line; size_t len=n;
  if (s->grep) {
    d=yyjson_read(line,n,0);
    if (!d) return PANGU_TOOL_ERROR;
    yyjson_val *r=yyjson_doc_get_root(d);
    const char *type=yyjson_get_str(yyjson_obj_get(r,"type"));
    if (!type || strcmp(type,"match")) { yyjson_doc_free(d); return PANGU_OK; }
    yyjson_val *data=yyjson_obj_get(r,"data");
    const char *path=yyjson_get_str(yyjson_obj_get(yyjson_obj_get(data,"path"),"text"));
    yyjson_val *v=yyjson_obj_get(yyjson_obj_get(data,"lines"),"text");
    text=yyjson_get_str(v); len=yyjson_get_len(v);
    if (!path || !text) { yyjson_doc_free(d); return PANGU_TOOL_ERROR; }
    size_t b=strlen(s->base);
    if (!strncmp(path,s->base,b) && path[b]=='/') path+=b+1;
    if (!strncmp(path,"./",2)) path+=2;
    int written=snprintf(prefix,sizeof(prefix),"%s:%llu: ",path,
      (unsigned long long)yyjson_get_uint(yyjson_obj_get(data,"line_number")));
    if (written<0 || (size_t)written>=sizeof(prefix)) { yyjson_doc_free(d); return PANGU_TOOL_ERROR; }
  } else prefix[0]=0;
  while (len && (text[len-1]=='\n' || text[len-1]=='\r')) --len;
  if (!s->grep) {
    size_t b=strlen(s->base);
    if (len>b && !memcmp(text,s->base,b) && text[b]=='/') { text+=b+1; len-=b+1; }
    if (len>2 && !memcmp(text,"./",2)) { text+=2; len-=2; }
  }
  size_t p=strlen(prefix);
  if (len>500 && s->grep) {
    len=500;
    while (len && ((unsigned char)text[len]&0xc0)==0x80) --len;
    s->long_line=1;
  }
  if (s->count>=s->limit || p+len+1>MAX_BYTES-s->size) s->capped=1;
  else {
    if (s->size) s->output[s->size++]='\n';
    memcpy(s->output+s->size,prefix,p); s->size+=p;
    memcpy(s->output+s->size,text,len); s->size+=len;
    s->output[s->size]=0; ++s->count;
  }
  yyjson_doc_free(d); return PANGU_OK;
}
static pangu_status search_append(void *ctx,pangu_slice data) {
  search_output *s=ctx;
  /* Bound one protocol record; reject pathological records rather than allocate
   * unbounded memory. rg lines can otherwise be arbitrarily large. */
  if (data.len>16*1024*1024-s->pending_size) return PANGU_TOOL_ERROR;
  char *p=realloc(s->pending,s->pending_size+data.len+1);
  if (!p) return PANGU_NOMEM;
  s->pending=p; memcpy(p+s->pending_size,data.data,data.len); s->pending_size+=data.len;
  size_t start=0;
  for (size_t i=0; i<s->pending_size; ++i) if (p[i]=='\n') {
    pangu_status st=search_line(s,p+start,i-start);
    if (st) return st;
    start=i+1;
  }
  memmove(p,p+start,s->pending_size-start); s->pending_size-=start;
  return PANGU_OK;
}
static pangu_status list_directory(const char *cwd,const char *path,search_output *s) {
  size_t n=strlen(cwd)+strlen(path)+2;
  char *name=malloc(n);
  if (!name) return PANGU_NOMEM;
  if (path[0]=='/' || (strlen(path)>2 && path[1]==':')) snprintf(name,n,"%s",path);
  else snprintf(name,n,"%s/%s",cwd,path);
  uv_fs_t req;
  int rc=uv_fs_scandir(NULL,&req,name,0,NULL); free(name);
  if (rc<0) { uv_fs_req_cleanup(&req); return PANGU_TOOL_ERROR; }
  uv_dirent_t entry;
  while (uv_fs_scandir_next(&req,&entry)==0) {
    size_t len=strlen(entry.name);
    char *text=malloc(len+2);
    if (!text) { uv_fs_req_cleanup(&req); return PANGU_NOMEM; }
    memcpy(text,entry.name,len);
    if (entry.type==UV_DIRENT_DIR) text[len++]='/';
    pangu_status st=search_line(s,text,len); free(text);
    if (st) { uv_fs_req_cleanup(&req); return st; }
  }
  uv_fs_req_cleanup(&req); return PANGU_OK;
}
static pangu_status execute(void *ctx,pangu_core *owner,const pangu_content *call,
                            pangu_tool_sink sink,void *sink_ctx) {
  execution *e=ctx;
  if (pangu_core_is_cancelled(owner)) return PANGU_CANCELLED;
  yyjson_doc *d=yyjson_read(call->data.data,call->data.len,0);
  if (!d) return PANGU_TOOL_ERROR;
  yyjson_val *root=yyjson_doc_get_root(d);
  int bash=call->name.len==4&&!memcmp(call->name.data,"bash",4);
  int grep=call->name.len==4&&!memcmp(call->name.data,"grep",4);
  int ls=call->name.len==2&&!memcmp(call->name.data,"ls",2);
  const char *path=string(root,"path",".");
  const char *pattern=string(root,"pattern",NULL), *command=string(root,"command",NULL);
  pg_process_request request={.cwd=e->config.cwd,.env=e->config.env,
    .poll=e->config.poll,.poll_ctx=e->config.poll_ctx};
  int code=0; pangu_status st=PANGU_OK;
  accumulator a={.fd=-1,.directory=e->config.temp_directory};
  search_output s={.grep=grep,.base=path?path:"",.limit=grep?100:ls?500:1000};
  yyjson_val *v=yyjson_obj_get(root,"limit");
  if (v) s.limit=(size_t)yyjson_get_uint(v);
  const char *argv[24]; size_t argc=0; char *full_pattern=NULL;
  if (bash) {
    double timeout=yyjson_get_num(yyjson_obj_get(root,"timeout"));
    if (!command || timeout<0 || timeout>2147483.647 || (yyjson_obj_get(root,"timeout") && timeout==0)) st=PANGU_TOOL_ERROR;
    else {
      request.timeout_ms=(uint64_t)(timeout*1000);
      argv[argc++]=e->config.shell?e->config.shell:"bash";
      argv[argc++]="-c"; argv[argc++]=command; argv[argc]=NULL;
      request.argv=argv;
      st=e->config.environment.run(e->config.environment.ctx,owner,&request,append,&a,&code);
    }
  } else if (!path || (!ls && !pattern)) st=PANGU_TOOL_ERROR;
  else if (ls) st=list_directory(e->config.cwd,path,&s);
  else {
    argv[argc++]=grep?"rg":"fd";
    if (grep) {
      argv[argc++]="--no-config"; argv[argc++]="--json"; argv[argc++]="--line-number";
      argv[argc++]="--color=never"; argv[argc++]="--hidden";
      if (yyjson_get_bool(yyjson_obj_get(root,"ignoreCase"))) argv[argc++]="--ignore-case";
      if (yyjson_get_bool(yyjson_obj_get(root,"literal"))) argv[argc++]="--fixed-strings";
      const char *glob=string(root,"glob",NULL);
      if (glob) { argv[argc++]="--glob"; argv[argc++]=glob; }
    } else {
      argv[argc++]="--glob"; argv[argc++]="--color=never"; argv[argc++]="--hidden";
      argv[argc++]="--no-require-git";
      if (strchr(pattern,'/')) {
        argv[argc++]="--full-path";
        full_pattern=malloc(strlen(pattern)+4);
        if (!full_pattern) st=PANGU_NOMEM;
        else { strcpy(full_pattern,"**/"); strcat(full_pattern,pattern); pattern=full_pattern; }
      }
    }
    argv[argc++]="--"; argv[argc++]=pattern; argv[argc++]=path; argv[argc]=NULL;
    request.argv=argv;
    if (!st) st=e->config.environment.run(e->config.environment.ctx,owner,&request,search_append,&s,&code);
    if (!st && s.pending_size) st=search_line(&s,s.pending,s.pending_size);
    if (!st && code && !(grep && code==1)) st=PANGU_TOOL_ERROR;
  }
  finish(&a);
  if (!st && pangu_core_is_cancelled(owner)) st=PANGU_CANCELLED;
  yyjson_mut_doc *details=yyjson_mut_doc_new(NULL);
  char *json=NULL; size_t json_len=0;
  char *text=NULL;
  if (!st && !details) st=PANGU_NOMEM;
  if (!st) {
    yyjson_mut_val *obj=yyjson_mut_obj(details); yyjson_mut_doc_set_root(details,obj);
    size_t start=0;
    if (bash) {
      if (a.total>a.size) {
        while (start<a.size && ((unsigned char)a.bytes[start]&0xc0)==0x80) ++start;
        /* Prefer complete lines; retain a suffix for an overlong final line. */
        size_t first=start; while (first<a.size && a.bytes[first]!='\n') ++first;
        if (first+1<a.size) start=first+1;
      }
      size_t lines=0;
      for (size_t i=a.size; i>start; --i) if (a.bytes[i-1]=='\n' && ++lines>2000) { start=i; break; }
      int truncated=start>0 || a.total>a.size;
      yyjson_mut_obj_add_int(details,obj,"exitCode",code);
      yyjson_mut_obj_add_uint(details,obj,"totalBytes",a.total);
      yyjson_mut_obj_add_bool(details,obj,"truncated",truncated);
      if (truncated && a.path) yyjson_mut_obj_add_strcpy(details,obj,"fullOutputPath",a.path);
      size_t capacity=a.size-start+(a.path?strlen(a.path):0)+256;
      text=malloc(capacity);
      if (text) {
        size_t n=a.size-start; memcpy(text,a.bytes+start,n); text[n]=0;
        if (!n) { strcpy(text,"(no output)"); n=strlen(text); }
        if (truncated) n+=(size_t)snprintf(text+n,capacity-n,"\n\n[Output truncated to last 2000 lines or 50.0KB. Full output: %s]",a.path?a.path:"");
        if (code) snprintf(text+n,capacity-n,"\n\nCommand exited with code %d",code);
      }
    } else {
      yyjson_mut_obj_add_bool(details,obj,"limitReached",s.capped);
      yyjson_mut_obj_add_bool(details,obj,"linesTruncated",s.long_line);
      text=malloc(s.size+512);
      if (text) {
        strcpy(text,s.size?s.output:grep?"No matches found":ls?"(empty directory)":"No files found matching pattern");
        if (s.capped) snprintf(text+strlen(text),200,"\n\n[%zu %s limit or 50.0KB limit reached. Increase limit or refine pattern.]",s.limit,grep?"matches":ls?"entries":"results");
        if (s.long_line) strcat(text,"\n[Some lines truncated to 500 chars. Use read tool to see full lines.]");
      }
    }
    json=yyjson_mut_write(details,0,&json_len);
    if (!text || !json) st=PANGU_NOMEM;
  }
  if (!st) {
    pangu_content content={.kind=PANGU_TEXT,.data={text,strlen(text)}};
    pangu_message result={.role=PANGU_TOOL_RESULT,.content=&content,.content_count=1,
      .details_json={json,json_len},.is_error=bash&&code!=0};
    st=sink(sink_ctx,&result,1);
  }
  free(text); free(json); yyjson_mut_doc_free(details);
  free(a.path); free(s.pending); free(full_pattern); yyjson_doc_free(d);
  return st;
}
static void destroy(void *ctx) { free(ctx); }
static pg_status install(pg_scope *scope,const void *cfg,size_t size) {
  if (!cfg || size!=sizeof(pg_execution_tools_config)) return PG_INVALID;
  const pg_execution_tools_config *c=cfg;
  if (!c->cwd || (!c->validate && !c->validate_arguments) || !c->environment.run) return PG_INVALID;
  execution *e=calloc(1,sizeof(*e)); if (!e) return PG_NOMEM;
  e->config=*c;
  static const char *names[]={"bash","grep","find","ls"};
  static const char *schemas[]={
    "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"},\"timeout\":{\"type\":\"number\",\"exclusiveMinimum\":0,\"maximum\":2147483.647}},\"required\":[\"command\"],\"additionalProperties\":false}",
    "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},\"glob\":{\"type\":\"string\"},\"ignoreCase\":{\"type\":\"boolean\"},\"literal\":{\"type\":\"boolean\"},\"limit\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"pattern\"],\"additionalProperties\":false}",
    "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"pattern\"],\"additionalProperties\":false}",
    "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\",\"minimum\":1}},\"additionalProperties\":false}"};
  for (size_t i=0;i<4;++i) e->tools[i]=(pangu_tool){.name={names[i],strlen(names[i])},
    .description=SL("Native execution/search: bash tail 2000 lines/50KB with full spill; searches head 50KB with result limits"),
    .schema_json={schemas[i],strlen(schemas[i])},.ctx=e,.validate=c->validate?validate:NULL,.validate_call=(!c->validate_arguments && c->validate==pg_schema_validate)?pg_schema_normalize_tool:NULL,.validate_arguments=(c->validate_arguments || c->validate==pg_schema_validate)?validate_arguments:NULL,.execute=execute};
  e->service=(pg_tools_service){e->tools,4};
  pg_status st=pg_scope_defer(scope,destroy,e); if (st) { free(e); return st; }
  return pg_service_register(scope,PG_NAME(PG_EXECUTION_TOOLS_SERVICE),1,&e->service,e);
}
const pg_plugin pg_execution_tools_plugin={1,sizeof(pg_plugin),{"pangu.tools.execution",sizeof("pangu.tools.execution")-1},NULL,0,install};
