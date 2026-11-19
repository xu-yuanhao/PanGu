/* Harness 22ddee37 user-approval: grants are per invocation, failures closed. */
#include <pangu/services/approval.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"approval:%d: %s\n",__LINE__,#x); abort(); } } while (0)
#define SL(x) ((pangu_slice){x,sizeof(x)-1})
static int equal(pangu_slice a, pangu_slice b) {
  return a.len == b.len && (!a.len || !memcmp(a.data,b.data,a.len));
}
typedef struct {
  pg_scope *scope;
  pg_service_approval_outcome outcome;
  int mode, asked, bridge_calls;
  uint64_t last;
  pg_approval_provider *provider;
} answers;
static pg_status answer(void *ctx, pg_approval_provider *p, uint64_t id,
    pangu_core *core, const pg_service_approval_question *q,
    pg_service_approval_outcome *out) {
  answers *a=ctx;
  CHECK(id > a->last); a->last=id; a->asked++;
  CHECK(q->tool_name.len && q->call_id.len);
  CHECK(equal(q->reason,SL("confirm mutation")));
  *out=a->outcome;
  if(a->mode==1) {
    CHECK(pg_approval_provider_reply(p,id,a->outcome)==PG_OK);
    CHECK(pg_approval_provider_reply(p,id,PG_SERVICE_ALLOWED_ONCE)==PG_NOT_FOUND);
    *out=PG_SERVICE_ALLOWED_ONCE; /* first explicit answer wins */
  } else if(a->mode==2) pangu_core_cancel(core);
  else if(a->mode==3) pg_approval_provider_revoke(p);
  else if(a->mode==4) return PG_FAILED;
  else if(a->mode==5) *out=(pg_service_approval_outcome)99;
  else if(a->mode==6 || a->mode==7) {
    CHECK(pg_approval_provider_reply(p,id,PG_SERVICE_ALLOWED_ONCE)==PG_OK);
    if(a->mode==6) *out=(pg_service_approval_outcome)99;
    else return PG_FAILED;
  }
  return PG_OK;
}
static pg_status bridge(void *ctx,pangu_core *core,
    const pg_service_approval_question *q,pg_service_approval_outcome *out) {
  answers *a=ctx; a->bridge_calls++;
  return pg_approval_ask(a->scope,core,q,out);
}
static pg_status bad_bridge(void *ctx,pangu_core *core,
    const pg_service_approval_question *q,pg_service_approval_outcome *out) {
  (void)ctx; (void)core; (void)q;
  *out=PG_SERVICE_ALLOWED_ONCE; return PG_FAILED;
}
static int result_count, result_error;
static const char *expected_text;
static pangu_status capture(void *ctx,const pangu_message *m,int final) {
  (void)ctx;
  CHECK(final && m->role==PANGU_TOOL_RESULT && m->content_count==1);
  result_count++; result_error=m->is_error;
  if(expected_text && !equal(m->content[0].data,(pangu_slice){expected_text,strlen(expected_text)})) { fprintf(stderr,"expected %s, got %.*s\n",expected_text,(int)m->content[0].data.len,m->content[0].data.data); abort(); }
  return PANGU_OK;
}
static const pangu_tool *find(const pg_tools_service *s,const char *name) {
  for(size_t i=0;i<s->count;i++) if(equal(s->tools[i].name,(pangu_slice){name,strlen(name)})) return &s->tools[i];
  abort();
}
static void file_is(const char *path,const char *text) {
  FILE *f=fopen(path,"rb"); CHECK(f);
  char buf[64]; size_t n=fread(buf,1,sizeof(buf),f); CHECK(!ferror(f)); CHECK(!fclose(f));
  CHECK(n==strlen(text) && !memcmp(buf,text,n));
}
static void invoke(const pangu_tool *t,pangu_core *c,const char *json,int error,const char *message) {
  pangu_content call={.kind=PANGU_TOOL_CALL,.name=t->name,.id=SL("call-1"),.data={json,strlen(json)}};
  result_count=0; expected_text=message;
  CHECK(t->execute(t->ctx,c,&call,capture,NULL)==PANGU_OK);
  CHECK(result_count==1 && result_error==error);
}
/* Actual Core -> production Schema -> wrapped local write -> tool result. */
typedef struct { int step; } model_state;
static pangu_status model(void *ctx,pangu_core *core,pangu_model_sink sink,void *sc) {
  model_state *s=ctx;
  pangu_content b={.kind=PANGU_TEXT,.data=SL("done")};
  pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1,.stop_reason=PANGU_STOP};
  if(!s->step++) {
    b=(pangu_content){.kind=PANGU_TOOL_CALL,.name=SL("write"),.id=SL("model-write"),
      .data=SL("{\"path\":\"approval.txt\",\"content\":42}")};
    m.stop_reason=PANGU_TOOL_USE;
  } else {
    const pangu_message *last=pangu_core_message_at(core,pangu_core_message_count(core)-1);
    CHECK(last && last->role==PANGU_TOOL_RESULT && !last->is_error);
  }
  return sink(sc,&m,1);
}
/* Async callback writes a real file; release must receive original context/op. */
typedef struct { const char *path; int starts,releases,prepared,validated,null_op,fail_start; } async_state;
static pangu_status prepare(void *ctx,pangu_slice args,pangu_arguments_sink sink,void *sc) {
  async_state *s=ctx; s->prepared++; return sink(sc,args);
}
static pangu_status validate_args(void *ctx,pangu_slice schema,pangu_slice args,pangu_arguments_sink sink,void *sc) {
  async_state *s=ctx; s->validated++;
  pangu_status z=pg_schema_validate(NULL,schema,args); return z ? z : sink(sc,args);
}
static pangu_status start(void *ctx,pangu_core *c,const pangu_content *call,pangu_tool_sink sink,void *sc,void **op) {
  (void)c; async_state *s=ctx; s->starts++; *op=s->null_op?NULL:s;
  if (s->fail_start) return PANGU_TOOL_ERROR;
  FILE *f=fopen(s->path,"wb"); CHECK(f); CHECK(fwrite("async",1,5,f)==5); CHECK(!fclose(f));
  pangu_content b={.kind=PANGU_TEXT,.data=SL("async done")};
  pangu_message m={.role=PANGU_TOOL_RESULT,.tool_call_id=call->id,.tool_name=call->name,.content=&b,.content_count=1};
  return sink(sc,&m,1);
}
static void release(void *ctx,void *op) { async_state *s=ctx; CHECK(op==(s->null_op?NULL:s)); s->releases++; }
static pangu_status pump(void *ctx,pangu_core *core) { (void)ctx; (void)core; return PANGU_OK; }
static void async_test(pg_scope *root,answers *a,pangu_core *c,const char *path) {
  async_state state={.path=path};
  pangu_tool original={.name=SL("async-write"),.schema_json=SL("{\"type\":\"object\"}"),.ctx=&state,
    .prepare_arguments=prepare,.validate_arguments=validate_args,.start=start,.release=release};
  pg_tools_service source={&original,1}; pg_scope *src=NULL,*wrapped=NULL;
  CHECK(pg_scope_create(root,&src)==PG_OK);
  CHECK(pg_service_register(src,PG_NAME("async.source"),1,&source,NULL)==PG_OK);
  pg_approval_tools_config cfg={.policy=PG_APPROVAL_ASK,.reason=SL("confirm mutation"),.request=bridge,.context=a};
  CHECK(pg_approval_tools_install(src,PG_NAME("async.wrapped"),PG_NAME("async.source"),&cfg,&wrapped)==PG_OK);
  pg_lease *l=NULL; CHECK(pg_service_acquire(src,PG_NAME("async.wrapped"),1,&l)==PG_OK);
  const pg_tools_service *s=pg_lease_table(l);
  pangu_loop_config loop={.tools=s->tools,.tool_count=s->count,.pump=pump};
  pangu_content call={.kind=PANGU_TOOL_CALL,.name=original.name,.id=SL("async-1"),.data=SL("{}")};
  pangu_message assistant={.role=PANGU_ASSISTANT,.content=&call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
  for(int allowed=0;allowed<2;allowed++) {
    a->outcome=allowed?PG_SERVICE_ALLOWED_ONCE:PG_SERVICE_REJECTED;
    pangu_tool_outcome out={0};
    CHECK(pangu_core_call_tool(c,&assistant,&call,&loop,NULL,NULL,&out)==PANGU_OK);
    CHECK(out.result);
    CHECK(pangu_core_message_at(out.result,0)->is_error==!allowed);
    pangu_core_destroy(out.result);
    CHECK(state.starts==allowed && state.releases==allowed);
  }
  CHECK(state.prepared==2 && state.validated==2); file_is(path,"async");
  /* Successful NULL handles still require source release; failed starts with
   * owned handles also release, but a denial never releases source state. */
  for (int kind=0;kind<3;kind++) {
    state.null_op=kind!=1; state.fail_start=kind!=0;
    int releases=state.releases;
    pangu_tool_outcome out={0};
    CHECK(pangu_core_call_tool(c,&assistant,&call,&loop,NULL,NULL,&out)==PANGU_OK);
    CHECK(out.result && pangu_core_message_at(out.result,0)->is_error==state.fail_start);
    CHECK(state.releases==releases+(kind!=2));
    pangu_core_destroy(out.result);
  }
  pg_lease_release(l); CHECK(pg_scope_dispose(src)==PG_OK);
  l=NULL; CHECK(pg_service_acquire(src,PG_NAME("async.wrapped"),1,&l)==PG_CLOSED);
}
int main(void) {
  uv_fs_t req; CHECK(uv_fs_mkdtemp(NULL,&req,"/tmp/pangu-approval-XXXXXX",NULL)==0);
  char dir[1024],path[1100]; CHECK(strlen(req.path)<sizeof(dir)); strcpy(dir,req.path); uv_fs_req_cleanup(&req);
  snprintf(path,sizeof(path),"%s/approval.txt",dir);
  pg_host *h=NULL; CHECK(pg_host_create(&h)==PG_OK); pg_scope *root=pg_host_root(h),*files=NULL,*provider_scope=NULL,*wrapped=NULL;
  pg_file_tools_config fc={.environment=pg_local_file_environment(dir),.validate=pg_schema_validate};
  CHECK(pg_plugin_install(root,&pg_file_tools_plugin,&fc,sizeof(fc),&files)==PG_OK);
  CHECK(pg_scope_create(root,&provider_scope)==PG_OK);
  answers a={.scope=provider_scope,.outcome=PG_SERVICE_ALLOWED_ONCE};
  CHECK(pg_approval_provider_register(provider_scope,answer,&a,&a.provider)==PG_OK);
  char reason[]="confirm mutation";
  pg_approval_tools_config cfg={.policy=PG_APPROVAL_ASK,.reason={reason,sizeof(reason)-1},.request=bridge,.context=&a};
  CHECK(pg_approval_tools_install(root,PG_NAME("approved.tools"),PG_NAME(PG_TOOLS_SERVICE),&cfg,&wrapped)==PG_OK);
  reason[0]='X'; /* copied at install */
  pg_lease *l=NULL; CHECK(pg_service_acquire(root,PG_NAME("approved.tools"),1,&l)==PG_OK);
  const pg_tools_service *s=pg_lease_table(l); const pangu_tool *write=find(s,"write");
  pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
  model_state ms={0}; pangu_loop_config loop={.ctx=&ms,.model=model,.tools=s->tools,.tool_count=s->count};
  pangu_content b={.kind=PANGU_TEXT,.data=SL("write")}; pangu_message prompt={.role=PANGU_USER,.content=&b,.content_count=1};
  CHECK(pangu_core_run(c,&prompt,1,&loop)==PANGU_OK); CHECK(ms.step==2 && a.asked==1); file_is(path,"42");
  const char *mutation="{\"path\":\"approval.txt\",\"content\":\"forbidden\"}";
  const pg_service_approval_outcome outcomes[]={PG_SERVICE_REJECTED,PG_SERVICE_CANCELLED,PG_SERVICE_UNAVAILABLE};
  const char *messages[]={"Tool approval rejected","Tool approval cancelled","Tool approval unavailable"};
  for(size_t i=0;i<3;i++) { a.outcome=outcomes[i]; invoke(write,c,mutation,1,messages[i]); file_is(path,"42"); }
  a.mode=1; a.outcome=PG_SERVICE_REJECTED; invoke(write,c,mutation,1,messages[0]);
  CHECK(pg_approval_provider_reply(a.provider,a.last,PG_SERVICE_ALLOWED_ONCE)==PG_NOT_FOUND);
  a.outcome=PG_SERVICE_ALLOWED_ONCE;
  for(int mode=4;mode<=7;mode++) { a.mode=mode; invoke(write,c,mutation,1,messages[2]); file_is(path,"42"); }
  a.mode=0; async_test(root,&a,c,path); file_is(path,"async");
  a.mode=2;
  pangu_content cancel_call={.kind=PANGU_TOOL_CALL,.name=SL("write"),.id=SL("cancel-write"),.data={mutation,strlen(mutation)}};
  pangu_message assistant={.role=PANGU_ASSISTANT,.content=&cancel_call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
  pangu_tool_outcome cancelled={0};
  CHECK(pangu_core_call_tool(c,&assistant,&cancel_call,&loop,NULL,NULL,&cancelled)==PANGU_OK);
  CHECK(cancelled.result && pangu_core_is_cancelled(c));
  const pangu_message *cancel_result=pangu_core_message_at(cancelled.result,0);
  CHECK(cancel_result->is_error && equal(cancel_result->content[0].data,SL("Tool approval cancelled")));
  pangu_core_destroy(cancelled.result); file_is(path,"async");
  int asked=a.asked; invoke(write,c,mutation,1,messages[1]); CHECK(a.asked==asked);
  pangu_core_destroy(c); CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
  a.mode=3; invoke(write,c,mutation,1,messages[2]); file_is(path,"async");
  invoke(write,c,mutation,1,messages[2]); CHECK(a.asked==asked+1);
  /* Missing bridge, NEVER, and a bridge error holding a grant all fail closed. */
  pg_lease_release(l); CHECK(pg_scope_dispose(wrapped)==PG_OK);
  for(int policy=0;policy<3;policy++) {
    cfg=(pg_approval_tools_config){.policy=policy==1?PG_APPROVAL_NEVER:PG_APPROVAL_ASK,.reason=SL("confirm mutation"),.request=policy==2?bad_bridge:policy==1?bridge:NULL,.context=&a};
    CHECK(pg_approval_tools_install(root,PG_NAME("approved.tools"),PG_NAME(PG_TOOLS_SERVICE),&cfg,&wrapped)==PG_OK);
    CHECK(pg_service_acquire(root,PG_NAME("approved.tools"),1,&l)==PG_OK);
    int before=a.bridge_calls;
    invoke(find(pg_lease_table(l),"write"),c,mutation,1,messages[policy==1?0:2]);
    CHECK(a.bridge_calls==before); file_is(path,"async");
    pg_lease_release(l); CHECK(pg_scope_dispose(wrapped)==PG_OK);
  }
  pg_scope *child=NULL; CHECK(pg_scope_create(provider_scope,&child)==PG_OK);
  answers child_answers={.scope=child,.outcome=PG_SERVICE_REJECTED};
  CHECK(pg_approval_provider_register(child,answer,&child_answers,&child_answers.provider)==PG_OK);
  pg_service_approval_question routed={SL("write"),SL("child"),SL("confirm mutation")};
  pg_service_approval_outcome routed_out=PG_SERVICE_ALLOWED_ONCE;
  CHECK(pg_approval_ask(child,c,&routed,&routed_out)==PG_OK && routed_out==PG_SERVICE_REJECTED);
  CHECK(child_answers.asked==1 && a.asked==asked+1);
  pg_approval_provider_revoke(child_answers.provider);
  CHECK(pg_scope_dispose(child)==PG_OK);
  pg_service_approval_question q={SL("write"),SL("late"),SL("confirm mutation")};
  pg_service_approval_outcome out=PG_SERVICE_ALLOWED_ONCE;
  CHECK(pg_approval_ask(root,c,&q,&out)==PG_OK && out==PG_SERVICE_UNAVAILABLE);
  CHECK(pg_scope_dispose(provider_scope)==PG_OK);
  CHECK(pg_approval_ask(provider_scope,c,&q,&out)==PG_OK && out==PG_SERVICE_UNAVAILABLE);
  /* Source withdrawal automatically removes its wrapper, restoring a previous
   * output binding. No lease may remain while disposing either scope. */
  int sentinel=7;
  CHECK(pg_service_register(root,PG_NAME("approved.tools"),1,&sentinel,NULL)==PG_OK);
  cfg=(pg_approval_tools_config){.policy=PG_APPROVAL_NEVER};
  CHECK(pg_approval_tools_install(root,PG_NAME("approved.tools"),PG_NAME(PG_TOOLS_SERVICE),&cfg,&wrapped)==PG_OK);
  CHECK(pg_scope_dispose(files)==PG_OK);
  CHECK(pg_service_acquire(root,PG_NAME("approved.tools"),1,&l)==PG_OK);
  CHECK(pg_lease_table(l)==&sentinel); pg_lease_release(l);
  CHECK(pg_scope_dispose(wrapped)==PG_OK);
  pg_scope *bad=NULL; CHECK(pg_scope_create(root,&bad)==PG_OK);
  pangu_tool malformed={.name=SL("broken"),.start=start};
  pg_tools_service bad_source={&malformed,1};
  CHECK(pg_service_register(bad,PG_NAME("bad.source"),1,&bad_source,NULL)==PG_OK);
  wrapped=(pg_scope *)1;
  CHECK(pg_approval_tools_install(bad,PG_NAME("bad.wrapped"),PG_NAME("bad.source"),&cfg,&wrapped)==PG_INVALID && !wrapped);
  CHECK(pg_service_acquire(bad,PG_NAME("bad.wrapped"),1,&l)==PG_NOT_FOUND);
  CHECK(pg_scope_dispose(bad)==PG_OK);
  pangu_core_destroy(c); pg_host_destroy(h);
  CHECK(uv_fs_unlink(NULL,&req,path,NULL)==0); uv_fs_req_cleanup(&req);
  CHECK(uv_fs_rmdir(NULL,&req,dir,NULL)==0); uv_fs_req_cleanup(&req);
  puts("approval: Core/local write, closed outcomes, cancellation, revocation, async ownership passed");
  return 0;
}
