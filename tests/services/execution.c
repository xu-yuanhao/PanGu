#include <pangu/services/execution.h>
#include <pangu/services/schema.h>
#include <uv.h>
#include <yyjson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SL(s) ((pangu_slice){s,sizeof(s)-1})
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static char *captured, *details; static int error;
static pangu_status capture(void *ctx,const pangu_message *m,int final) {
  (void)ctx; CHECK(final); CHECK(m->content_count==1);
  free(captured); free(details);
  captured=malloc(m->content[0].data.len+1); CHECK(captured);
  memcpy(captured,m->content[0].data.data,m->content[0].data.len); captured[m->content[0].data.len]=0;
  details=malloc(m->details_json.len+1); CHECK(details);
  memcpy(details,m->details_json.data,m->details_json.len); details[m->details_json.len]=0;
  error=m->is_error; return PANGU_OK;
}
static pangu_status invoke(const pg_tools_service *s,pangu_core *c,const char *name,const char *json) {
  for (size_t i=0;i<s->count;++i) {
    const pangu_tool *t=&s->tools[i];
    if (t->name.len!=strlen(name)||memcmp(t->name.data,name,strlen(name))) continue;
    pangu_content call={.kind=PANGU_TOOL_CALL,.name={name,strlen(name)},.data={json,strlen(json)}};
    pangu_status st=t->validate(t->ctx,t->schema_json,call.data);
    return st?st:t->execute(t->ctx,c,&call,capture,NULL);
  }
  return PANGU_INVALID;
}
static int cancelling;
static pangu_status model(void *ctx,pangu_core *c,pangu_model_sink sink,void *out) {
  if (pangu_core_is_cancelled(c)) return PANGU_CANCELLED;
  int *step=ctx;
  pangu_content b; pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1};
  if ((*step)++==0) {
    b=(pangu_content){.kind=PANGU_TOOL_CALL,.id=SL("shell"),.name=SL("bash"),.data=SL("{\"command\":\"printf 'Core执行'\",\"timeout\":\"2\"}")}; if(cancelling) b.data=SL("{\"command\":\"sleep .6; printf survived > cancelled-child\"}"); m.stop_reason=PANGU_TOOL_USE;
  } else {
    const pangu_message *last=pangu_core_message_at(c,pangu_core_message_count(c)-1);
    if(last->is_error) fprintf(stderr,"tool error: %.*s\n",(int)last->content[0].data.len,last->content[0].data.data);
    CHECK(last->role==PANGU_TOOL_RESULT && !last->is_error);
    CHECK(last->content[0].data.len==strlen("Core执行"));
    b=(pangu_content){.kind=PANGU_TEXT,.data=SL("done")}; m.stop_reason=PANGU_STOP;
  }
  return sink(out,&m,1);
}
typedef struct { pangu_core *core; uint64_t start; int enabled; } polling;
static void poll(void *ctx) {
  polling *p=ctx;
  if(p->enabled && uv_hrtime()-p->start>100000000) pangu_core_cancel(p->core);
}
static pangu_status injected(void *ctx,pangu_core *c,const pg_process_request *r,pg_process_output emit,void *out,int *code) {
  (void)ctx; (void)c; CHECK(!strcmp(r->argv[0],"host-shell")); *code=0;
  return emit(out,SL("mobile allowed execution"));
}
int main(int argc,char **argv) {
  CHECK(argc==2);
  pg_host *host; pg_scope *scope; pg_lease *lease;
  CHECK(pg_host_create(&host)==PG_OK);
  const char *env[]={"PANGU_TEST_VALUE=中文 env", "PATH=/usr/bin:/bin",NULL};
  polling polling_state={0};
  pg_execution_tools_config config={.environment=pg_local_execution_environment(),.cwd=argv[1],.env=env,
    .temp_directory=argv[1],.validate=pg_schema_validate,.poll=poll,.poll_ctx=&polling_state};
  CHECK(pg_plugin_install(pg_host_root(host),&pg_execution_tools_plugin,&config,sizeof(config),&scope)==PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host),PG_NAME(PG_EXECUTION_TOOLS_SERVICE),1,&lease)==PG_OK);
  const pg_tools_service *s=pg_lease_table(lease); CHECK(s->count==4);
  pangu_core *c; CHECK(pangu_core_create(NULL,&c)==PANGU_OK); polling_state.core=c;
  int step=0; pangu_loop_config loop={.ctx=&step,.model=model,.tools=s->tools,.tool_count=s->count};
  pangu_content b={.kind=PANGU_TEXT,.data=SL("execute")}; pangu_message m={.role=PANGU_USER,.content=&b,.content_count=1};
  CHECK(pangu_core_run(c,&m,1,&loop)==PANGU_OK); CHECK(step==2);
  CHECK(invoke(s,c,"bash","{\"command\":\"printf '%s' \\\"$PANGU_TEST_VALUE\\\"; printf A; sleep .03; printf B >&2; sleep .03; printf C\"}")==PANGU_OK);
  CHECK(!strcmp(captured,"中文 envABC"));
  CHECK(invoke(s,c,"bash","{\"command\":\"pwd\"}")==PANGU_OK); CHECK(strstr(captured,argv[1]));
  CHECK(invoke(s,c,"bash","{\"command\":\"printf failure; exit 7\"}")==PANGU_OK); CHECK(error && strstr(captured,"code 7"));
  CHECK(invoke(s,c,"bash","{\"command\":\"i=0; while [ $i -lt 12000 ]; do printf 'row%06d abcdefghijklmnopqrstuvwxyz\\n' $i; i=$((i+1)); done\"}")==PANGU_OK);
  CHECK(strstr(captured,"row011999")); CHECK(!strstr(captured,"row000000"));
  yyjson_doc *d=yyjson_read(details,strlen(details),0); CHECK(d);
  const char *path=yyjson_get_str(yyjson_obj_get(yyjson_doc_get_root(d),"fullOutputPath")); CHECK(path);
  FILE *f=fopen(path,"rb"); CHECK(f); char first[10]={0}; CHECK(fread(first,1,9,f)==9); CHECK(!memcmp(first,"row000000",9)); fclose(f); yyjson_doc_free(d);
  CHECK(invoke(s,c,"bash","{\"command\":\"(i=0; while [ $i -lt 6 ]; do sleep .04; printf tick; i=$((i+1)); done) & exit 0\"}")==PANGU_OK); CHECK(!strcmp(captured,"ticktickticktickticktick"));
  CHECK(invoke(s,c,"bash","{\"command\":\"sleep 10 & wait\",\"timeout\":0.1}")==PANGU_TOOL_ERROR);
  CHECK(invoke(s,c,"grep","{\"pattern\":\"needle\",\"path\":\"搜索 空格\",\"literal\":true}")==PANGU_OK); CHECK(strstr(captured,"a.txt:1: needle")); CHECK(!strstr(captured,"ignored"));
  CHECK(invoke(s,c,"grep","{\"pattern\":\"absent-pattern\"}")==PANGU_OK); CHECK(!strcmp(captured,"No matches found"));
  CHECK(invoke(s,c,"find","{\"pattern\":\"搜索 空格/**/*.txt\"}")==PANGU_OK); CHECK(strstr(captured,"搜索 空格/sub/b.txt"));
  CHECK(invoke(s,c,"ls","{\"path\":\"搜索 空格\",\"limit\":2}")==PANGU_OK); CHECK(strstr(captured,"limit"));
  polling_state.enabled=1; polling_state.start=uv_hrtime();
  pangu_core_destroy(c); CHECK(pangu_core_create(NULL,&c)==PANGU_OK); polling_state.core=c;
  step=0; cancelling=1;
  CHECK(pangu_core_run(c,&m,1,&loop)==PANGU_CANCELLED);
  polling_state.enabled=0; uv_sleep(650);
  char marker[4096]; snprintf(marker,sizeof(marker),"%s/cancelled-child",argv[1]); f=fopen(marker,"r"); CHECK(!f);
  pangu_core_destroy(c); pg_lease_release(lease); CHECK(pg_scope_dispose(scope)==PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host),PG_NAME(PG_EXECUTION_TOOLS_SERVICE),1,&lease)==PG_NOT_FOUND);
  config.environment=(pg_execution_environment){NULL,injected}; config.shell="host-shell";
  CHECK(pg_plugin_install(pg_host_root(host),&pg_execution_tools_plugin,&config,sizeof(config),&scope)==PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host),PG_NAME(PG_EXECUTION_TOOLS_SERVICE),1,&lease)==PG_OK);
  CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
  CHECK(invoke(pg_lease_table(lease),c,"bash","{\"command\":\"allowed\"}")==PANGU_OK); CHECK(!strcmp(captured,"mobile allowed execution"));
  pangu_core_destroy(c); pg_lease_release(lease); pg_host_destroy(host); free(captured); free(details);
  puts("execution: Core consumer, cwd/env/order, tail/spill, grace, timeout/tree cancel, search, injected host passed");
}
