/* Pi d5b3de22 ai/utils/validation.ts (MIT): serialized JSON Schema path.
 * JSON ownership/encoding: yyjson; all schema decisions: Valijson bridge. */
#include <pangu/services/schema.h>
#include <pangu/core/state.h>
#include <yyjson.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct { yyjson_mut_doc *doc; pangu_status status; } conversion;
static pangu_status check(conversion *c, yyjson_val *s, yyjson_mut_val *v) {
  size_t sn=0, vn=0;
  char *sj=yyjson_val_write(s,0,&sn);
  char *vj=yyjson_mut_val_write(v,0,&vn);
  if (!sj || !vj) { free(sj); free(vj); c->status=PANGU_NOMEM; return PANGU_NOMEM; }
  pangu_status st=pg_schema_check((pangu_slice){sj,sn},(pangu_slice){vj,vn},NULL,NULL);
  free(sj); free(vj);
  if (st==PANGU_NOMEM) c->status=st;
  return st;
}
static int type_is(yyjson_mut_val *v, const char *t) {
  if (!strcmp(t,"null")) return yyjson_mut_is_null(v);
  if (!strcmp(t,"string")) return yyjson_mut_is_str(v);
  if (!strcmp(t,"number")) return yyjson_mut_is_num(v);
  if (!strcmp(t,"integer")) return yyjson_mut_is_num(v) && floor(yyjson_mut_get_num(v))==yyjson_mut_get_num(v);
  if (!strcmp(t,"boolean")) return yyjson_mut_is_bool(v);
  if (!strcmp(t,"object")) return yyjson_mut_is_obj(v);
  if (!strcmp(t,"array")) return yyjson_mut_is_arr(v);
  return 0;
}
/* Number(string) adaptation; decimal/hex parsing stays in yyjson. Binary and
 * octal digits are accumulated as the JS Number (binary64) scalar. */
static yyjson_mut_val *numeric_string(conversion *c,const char *p,size_t n,const char *type) {
  int signed_value=n && (*p=='+' || *p=='-');
  const char *digits=p+(signed_value?1:0); size_t count=n-(signed_value?1:0);
  if (count>=2 && digits[0]=='0' && (digits[1]=='x' || digits[1]=='X' || digits[1]=='o' || digits[1]=='O' || digits[1]=='b' || digits[1]=='B')) {
    if (signed_value) return NULL;
    unsigned base=(digits[1]=='b' || digits[1]=='B')?2:(digits[1]=='o' || digits[1]=='O')?8:16;
    if (base!=16) {
      if (count==2) return NULL;
      double number=0;
      for(size_t i=2;i<count;++i) {
        if (digits[i]<'0' || (unsigned)(digits[i]-'0')>=base) return NULL;
        number=number*base+(digits[i]-'0');
      }
      if (!isfinite(number)) return NULL;
      yyjson_mut_val *out=yyjson_mut_real(c->doc,number);
      if (!out) c->status=PANGU_NOMEM;
      return out;
    }
  }
  char *text=malloc(n+1);
  if (!text) { c->status=PANGU_NOMEM; return NULL; }
  size_t used=0;
  if (signed_value && p[0]=='-') text[used++]='-';
  while(count>1 && digits[0]=='0' && digits[1]>='0' && digits[1]<='9') { ++digits; --count; }
  memcpy(text+used,digits,count); used+=count; text[used]=0;
  yyjson_read_err error={0};
  yyjson_doc *parsed=used?yyjson_read_opts(text,used,YYJSON_READ_ALLOW_EXT_NUMBER,NULL,&error):NULL;
  free(text);
  if (error.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION) c->status=PANGU_NOMEM;
  yyjson_val *r=parsed?yyjson_doc_get_root(parsed):NULL;
  yyjson_mut_val *out=NULL;
  if (yyjson_is_num(r) && (strcmp(type,"integer") || floor(yyjson_get_num(r))==yyjson_get_num(r))) {
    out=yyjson_val_mut_copy(c->doc,r);
    if (!out) c->status=PANGU_NOMEM;
  }
  yyjson_doc_free(parsed); return out;
}
static yyjson_mut_val *primitive(conversion *c, yyjson_mut_val *v,const char *t) {
  yyjson_mut_doc *d=c->doc;
  int null=yyjson_mut_is_null(v), boolean=yyjson_mut_is_bool(v), number=yyjson_mut_is_num(v);
  if (!strcmp(t,"number") || !strcmp(t,"integer")) {
    if (null || boolean) return yyjson_mut_uint(d,boolean && yyjson_mut_get_bool(v));
    if (yyjson_mut_is_str(v)) {
      const char *p=yyjson_mut_get_str(v); size_t n=yyjson_mut_get_len(v);
      while (n && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r' || *p=='\v' || *p=='\f')) { ++p; --n; }
      while (n && (p[n-1]==' ' || p[n-1]=='\t' || p[n-1]=='\n' || p[n-1]=='\r' || p[n-1]=='\v' || p[n-1]=='\f')) --n;
      if (n) {
        yyjson_mut_val *out=numeric_string(c,p,n,t);
        if (out || c->status!=PANGU_OK) return out;
      }
    }
  } else if (!strcmp(t,"boolean")) {
    if (null) return yyjson_mut_bool(d,false);
    if (number && (yyjson_mut_get_num(v)==0 || yyjson_mut_get_num(v)==1))
      return yyjson_mut_bool(d,yyjson_mut_get_num(v)==1);
    if (yyjson_mut_equals_str(v,"true")) return yyjson_mut_bool(d,true);
    if (yyjson_mut_equals_str(v,"false")) return yyjson_mut_bool(d,false);
  } else if (!strcmp(t,"string")) {
    if (null) return yyjson_mut_str(d,"");
    if (boolean) return yyjson_mut_str(d,yyjson_mut_get_bool(v)?"true":"false");
    if (number) {
      size_t len; char *text=yyjson_mut_val_write(v,0,&len);
      if (!text) return NULL;
      /* JSON writers preserve a real-valued .0; JavaScript String does not. */
      if (len>=2 && text[len-2]=='.' && text[len-1]=='0') len-=2;
      if (yyjson_mut_get_num(v)==0) { text[0]='0'; len=1; }
      char *exponent=memchr(text,'e',len);
      if (exponent && exponent[1]!='-' && exponent[1]!='+') {
        char *expanded=malloc(len+2);
        if (!expanded) { free(text); return NULL; }
        size_t prefix=(size_t)(exponent-text)+1;
        memcpy(expanded,text,prefix); expanded[prefix]='+';
        memcpy(expanded+prefix+1,text+prefix,len-prefix); free(text); text=expanded; ++len;
      }
      yyjson_mut_val *out=yyjson_mut_strncpy(d,text,len); free(text); return out;
    }
  } else if (!strcmp(t,"null")) {
    if ((number && yyjson_mut_get_num(v)==0) || (boolean && !yyjson_mut_get_bool(v)) || yyjson_mut_equals_str(v,""))
      return yyjson_mut_null(d);
  }
  return v;
}
static yyjson_mut_val *walk(conversion *,yyjson_mut_val *,yyjson_val *,int,unsigned);
static yyjson_mut_val *unions(conversion *c,yyjson_mut_val *v,yyjson_val *schemas,unsigned depth) {
  size_t i,m; yyjson_val *s;
  yyjson_arr_foreach(schemas,i,m,s) if (check(c,s,v)==PANGU_OK) return v;
  yyjson_arr_foreach(schemas,i,m,s) {
    yyjson_mut_val *candidate=yyjson_mut_val_mut_copy(c->doc,v);
    if (!candidate) { c->status=PANGU_NOMEM; return NULL; }
    candidate=walk(c,candidate,s,0,depth+1);
    if (!candidate) return NULL;
    if (check(c,s,candidate)==PANGU_OK) return candidate;
  }
  return v;
}
static yyjson_mut_val *walk(conversion *c,yyjson_mut_val *v,yyjson_val *s,int normalize,unsigned depth) {
  if (depth>256) { c->status=PANGU_TOOL_ERROR; return NULL; }
  if (!v || c->status!=PANGU_OK) return NULL;
  yyjson_val *types=yyjson_obj_get(s,"type");
  if (!normalize) {
    size_t i,m; yyjson_val *sub;
    yyjson_arr_foreach(yyjson_obj_get(s,"allOf"),i,m,sub) {
      v=walk(c,v,sub,0,depth+1); if (!v) return NULL;
    }
    const char *keys[]={"anyOf","oneOf"};
    for (i=0;i<2;++i) if (yyjson_is_arr(yyjson_obj_get(s,keys[i]))) {
      v=unions(c,v,yyjson_obj_get(s,keys[i]),depth); if (!v) return NULL;
    }
    int matches=0;
    if (yyjson_arr_size(types)>1) yyjson_arr_foreach(types,i,m,sub)
      if (yyjson_is_str(sub) && type_is(v,yyjson_get_str(sub))) matches=1;
    if (!matches) {
      if (yyjson_is_str(types)) v=primitive(c,v,yyjson_get_str(types));
      else yyjson_arr_foreach(types,i,m,sub) {
        if (!yyjson_is_str(sub)) continue;
        yyjson_mut_val *next=primitive(c,v,yyjson_get_str(sub));
        if (!next || next!=v) { v=next; break; }
      }
      if (!v) { c->status=PANGU_NOMEM; return NULL; }
    }
  }
  int object=normalize, array=normalize;
  if (!normalize) {
    if (yyjson_equals_str(types,"object")) object=1;
    if (yyjson_equals_str(types,"array")) array=1;
    size_t i,m; yyjson_val *t;
    yyjson_arr_foreach(types,i,m,t) { if (yyjson_equals_str(t,"object")) object=1; if (yyjson_equals_str(t,"array")) array=1; }
  }
  if (object && yyjson_mut_is_obj(v)) {
    yyjson_val *properties=yyjson_obj_get(s,"properties"), *required=yyjson_obj_get(s,"required");
    yyjson_mut_val *out=yyjson_mut_obj(c->doc), *key;
    if (!out) { c->status=PANGU_NOMEM; return NULL; }
    yyjson_mut_obj_iter it=yyjson_mut_obj_iter_with(v);
    while ((key=yyjson_mut_obj_iter_next(&it))) {
      yyjson_mut_val *value=yyjson_mut_obj_iter_get_val(key);
      yyjson_val *property=yyjson_obj_getn(properties,yyjson_mut_get_str(key),yyjson_mut_get_len(key));
      if (normalize && property && yyjson_mut_is_null(value) && !yyjson_is_str(yyjson_obj_get(property,"$ref"))) {
        int req=0; size_t i,m; yyjson_val *r;
        yyjson_arr_foreach(required,i,m,r) if (yyjson_is_str(r) && yyjson_get_len(r)==yyjson_mut_get_len(key) && !memcmp(yyjson_get_str(r),yyjson_mut_get_str(key),yyjson_get_len(r))) req=1;
        if (!req && check(c,property,value)==PANGU_TOOL_ERROR) continue;
      }
      if (!property && !normalize && yyjson_is_obj(yyjson_obj_get(s,"additionalProperties"))) property=yyjson_obj_get(s,"additionalProperties");
      value=yyjson_mut_val_mut_copy(c->doc,value);
      if (!value) { c->status=PANGU_NOMEM; return NULL; }
      if (property) value=walk(c,value,property,normalize,depth+1);
      yyjson_mut_val *new_key=yyjson_mut_strncpy(c->doc,yyjson_mut_get_str(key),yyjson_mut_get_len(key));
      if (!value || !new_key || !yyjson_mut_obj_add(out,new_key,value)) { if (c->status==PANGU_OK) c->status=PANGU_NOMEM; return NULL; }
    }
    v=out;
  }
  if (array && yyjson_mut_is_arr(v)) {
    yyjson_val *items=yyjson_obj_get(s,"items");
    yyjson_mut_val *out=yyjson_mut_arr(c->doc), *value; size_t i=0;
    if (!out) { c->status=PANGU_NOMEM; return NULL; }
    yyjson_mut_arr_iter it=yyjson_mut_arr_iter_with(v);
    while ((value=yyjson_mut_arr_iter_next(&it))) {
      yyjson_val *item=yyjson_is_arr(items)?yyjson_arr_get(items,i):items;
      value=yyjson_mut_val_mut_copy(c->doc,value);
      if (!value) { c->status=PANGU_NOMEM; return NULL; }
      if (item) value=walk(c,value,item,normalize,depth+1);
      if (!value || !yyjson_mut_arr_append(out,value)) { if (c->status==PANGU_OK) c->status=PANGU_NOMEM; return NULL; }
      ++i;
    }
    v=out;
  }
  return v;
}
typedef struct { char *text; size_t len; } detail;
static pangu_status collect(void *ctx,pangu_slice s) {
  detail *d=ctx; d->text=malloc(s.len+1); if (!d->text) return PANGU_NOMEM;
  memcpy(d->text,s.data,s.len); d->text[s.len]=0; d->len=s.len; return PANGU_OK;
}
pangu_status pg_schema_normalize(void *ctx,pangu_slice schema,pangu_slice args,pangu_arguments_sink sink,void *sc) {
  if (!sink || (!schema.data && schema.len) || (!args.data && args.len)) return PANGU_INVALID;
  pg_schema_diagnostics *diag=ctx;
  if (diag && !diag->tool_name.data && diag->tool_name.len) return PANGU_INVALID;
  yyjson_read_err se={0},ae={0};
  yyjson_doc *sd=yyjson_read_opts((char *)schema.data,schema.len,0,NULL,&se), *ad=yyjson_read_opts((char *)args.data,args.len,0,NULL,&ae);
  yyjson_mut_doc *md=yyjson_mut_doc_new(NULL);
  conversion c={md,PANGU_OK}; detail error={0}; char *json=NULL; size_t len=0;
  pangu_status st=PANGU_TOOL_ERROR;
  if (!md || se.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION || ae.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION) { st=PANGU_NOMEM; goto done; }
  if (!sd || !ad) { st=pg_schema_check(schema,args,collect,&error); goto diagnose; }
  yyjson_mut_val *v=yyjson_val_mut_copy(md,yyjson_doc_get_root(ad));
  if (!v) { st=PANGU_NOMEM; goto done; }
  v=walk(&c,v,yyjson_doc_get_root(sd),1,0);
  if (v) v=walk(&c,v,yyjson_doc_get_root(sd),0,0);
  if (!v || c.status!=PANGU_OK) { st=c.status; goto done; }
  json=yyjson_mut_val_write(v,0,&len);
  if (!json) { st=PANGU_NOMEM; goto done; }
  st=pg_schema_check(schema,(pangu_slice){json,len},collect,&error);
  if (st==PANGU_OK) { st=sink(sc,(pangu_slice){json,len}); goto done; }
diagnose:
  if (st==PANGU_INVALID) st=PANGU_TOOL_ERROR;
  if (st==PANGU_TOOL_ERROR && diag && diag->diagnostic) {
    const char *prefix="Validation failed for tool \"", *middle="\":\n", *received="\n\nReceived arguments:\n";
    size_t raw_len=args.len;
    char *pretty=ad?yyjson_val_write(yyjson_doc_get_root(ad),YYJSON_WRITE_PRETTY_TWO_SPACES,&raw_len):NULL;
    if (ad && !pretty) { st=PANGU_NOMEM; goto done; }
    const char *raw=pretty?pretty:args.data;
    size_t fixed=strlen(prefix)+strlen(middle)+strlen(received);
    if (diag->tool_name.len>SIZE_MAX-fixed || error.len>SIZE_MAX-fixed-diag->tool_name.len || raw_len>=SIZE_MAX-fixed-diag->tool_name.len-error.len) { free(pretty); st=PANGU_NOMEM; goto done; }
    size_t n=fixed+diag->tool_name.len+error.len+raw_len;
    char *message=malloc(n+1);
    if (!message) { free(pretty); st=PANGU_NOMEM; goto done; }
    char *p=message;
#define APPEND(ptr,count) do { size_t amount=(count); if(amount) memcpy(p,(ptr),amount); p+=amount; } while(0)
    APPEND(prefix,strlen(prefix)); APPEND(diag->tool_name.data,diag->tool_name.len); APPEND(middle,strlen(middle));
    APPEND(error.text,error.len); APPEND(received,strlen(received)); APPEND(raw,raw_len); free(pretty);
#undef APPEND
    *p=0; pangu_status reported=diag->diagnostic(diag->diagnostic_ctx,(pangu_slice){message,n}); free(message);
    if (reported!=PANGU_OK) st=reported;
  }
done:
  free(error.text); free(json); yyjson_mut_doc_free(md); yyjson_doc_free(sd); yyjson_doc_free(ad); return st;
}

static pangu_status report(void *ctx,pangu_slice text) {
  return pangu_core_report_error(ctx,text);
}
pangu_status pg_schema_normalize_tool(void *ctx,pangu_core *owner,
    const pangu_content *call,pangu_slice schema,pangu_slice args,
    pangu_arguments_sink sink,void *sc) {
  (void)ctx;
  if (!owner || !call) return PANGU_INVALID;
  pg_schema_diagnostics d={call->name,report,owner};
  return pg_schema_normalize(&d,schema,args,sink,sc);
}
