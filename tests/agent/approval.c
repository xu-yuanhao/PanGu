#include <pangu/agent/app.h>
#include <pangu/agent/approval.h>
#include <pangu/agent/tree.h>
#include <pangu/services/approval.h>
#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define S(x) ((pangu_slice){x,sizeof(x)-1})
typedef struct {
  pg_agent_app *app;
  int mode, effects, asked, decided, answer_calls, observed;
  uint64_t id;
  pg_approval_outcome want;
  const char *path;
  pg_session_backend backend;
  sqlite3 *db;
} fixture;
static pg_session_status scan(void *ctx,const pg_approval_event *e) {
  fixture *f=ctx;
  if(!e->decided) {
    f->asked++;
    assert(e->question.tool_name.len==5 && !memcmp(e->question.tool_name.data,"guard",5));
    assert(e->question.reason.len==7 && !memcmp(e->question.reason.data,"fixture",7));
  } else {f->decided++; assert(e->outcome==f->want);}
  return PG_SESSION_OK;
}
static pg_session_status answer(void *ctx,pangu_session *s,uint64_t id,const pg_approval_question *q,pg_approval_outcome *out) {
  fixture *f=ctx; f->answer_calls++; f->id=id;
  assert(pg_session_approval_pending(s)==id && q->tool_name.len==5);
  assert(pg_session_approval_reply(s,id+999,PG_APPROVAL_ALLOWED_ONCE)==PG_SESSION_NOT_FOUND);
  assert(pg_session_close(s)==PG_SESSION_BUSY);
  if(f->mode==2) {*out=(pg_approval_outcome)99; return PG_SESSION_OK;}
  if(f->mode==3) {*out=PG_APPROVAL_ALLOWED_ONCE; return PG_SESSION_INVALID;}
  if(f->mode==4) {pg_session_cancel(s); *out=PG_APPROVAL_ALLOWED_ONCE; return PG_SESSION_OK;}
  if(f->mode==5) {
    assert(pg_session_approval_reply(s,id,f->want)==PG_SESSION_OK);
    assert(pg_session_approval_reply(s,id,PG_APPROVAL_ALLOWED_ONCE)==PG_SESSION_NOT_FOUND);
    *out=PG_APPROVAL_ALLOWED_ONCE; return PG_SESSION_OK;
  }
  if(f->mode==7) exit(0); /* real process dies between asked and decided */
  if(f->mode==8) {
    if(f->backend==PG_STORE_JSONL) {
      assert(!rename(f->path,"approval-failure.old"));
      uv_fs_t req; assert(!uv_fs_mkdir(NULL,&req,f->path,0700,NULL)); uv_fs_req_cleanup(&req);
    } else {
      assert(sqlite3_open(f->path,&f->db)==SQLITE_OK);
      assert(sqlite3_exec(f->db,"CREATE TRIGGER fail_approval BEFORE INSERT ON pangu_session BEGIN SELECT RAISE(ABORT,'fixture'); END",NULL,NULL,NULL)==SQLITE_OK);
    }
  }
  *out=f->want; return PG_SESSION_OK;
}
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args) {
  (void)ctx;(void)schema;(void)args; return PANGU_OK;
}
static pangu_status execute(void *ctx,pangu_core *core,const pangu_content *call,pangu_tool_sink sink,void *data) {
  (void)core;
  fixture *f=ctx; pangu_session *s=pg_agent_app_session(f->app);
  pg_approval_question q={call->name,call->id,S("fixture")};
  pg_approval_outcome outcome=PG_APPROVAL_UNAVAILABLE;
  pg_session_status z=pg_session_approval_request(s,&q,f->mode==1?NULL:answer,f,&outcome);
  if(f->mode==8) {assert(z==PG_SESSION_IO); assert(!f->effects); return PANGU_TOOL_ERROR;}
  assert(z==PG_SESSION_OK && outcome==f->want && !pg_session_approval_pending(s));
  if(outcome==PG_APPROVAL_ALLOWED_ONCE) {
    f->asked=f->decided=0;
    assert(pg_session_approval_events(s,scan,f)==PG_SESSION_OK);
    assert(f->asked==1 && f->decided==1); /* commit precedes side effect */
    f->effects++;
  }
  pangu_content text={.kind=PANGU_TEXT,.data=S("guard result")};
  pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1,.is_error=outcome!=PG_APPROVAL_ALLOWED_ONCE};
  return sink(data,&result,1);
}
static pangu_status model(void *ctx,pangu_core *c,pangu_model_sink sink,void *data) {
  (void)ctx;
  const pangu_message *last=pangu_core_message_at(c,pangu_core_message_count(c)-1);
  pangu_content content=last->role==PANGU_USER
      ?(pangu_content){.kind=PANGU_TOOL_CALL,.id=S("call-guard"),.name=S("guard"),.data=S("{}")}
      :(pangu_content){.kind=PANGU_TEXT,.data=S("finished")};
  pangu_message m={.role=PANGU_ASSISTANT,.content=&content,.content_count=1,.stop_reason=content.kind==PANGU_TOOL_CALL?PANGU_TOOL_USE:PANGU_STOP};
  return sink(data,&m,1);
}
static void observe(void *ctx,pangu_session *s,const pg_session_event *e) {
  fixture *f=ctx;
  if(e->kind!=PG_SESSION_COMMITTED) return;
  uint64_t id=pg_session_approval_pending(s);
  if(f->mode==6 && id) {
    f->id=id; f->observed++;
    assert(pg_session_approval_reply(s,id,f->want)==PG_SESSION_OK);
  }
}
static void test(pg_session_backend backend,int mode) {
  const char *path=backend==PG_STORE_JSONL?"approval.jsonl":"approval.db";
  remove(path); fixture f={.mode=mode,.path=path,.backend=backend};
  f.want=mode==0 || mode==5 || mode==6 || mode==8?PG_APPROVAL_ALLOWED_ONCE:mode==4?PG_APPROVAL_CANCELLED:mode==9?PG_APPROVAL_REJECTED:PG_APPROVAL_UNAVAILABLE;
  pg_session_options opts={{path,strlen(path)},backend,1,0};
  assert(pg_agent_app_open(&opts,NULL,0,&f.app)==PG_OK);
  pangu_session *s=pg_agent_app_session(f.app);
  pg_approval_question q={S("guard"),S("call"),S("fixture")}; pg_approval_outcome out=PG_APPROVAL_REJECTED;
  assert(pg_session_approval_request(s,&q,answer,&f,&out)==PG_SESSION_BUSY);
  uint64_t sub; assert(pg_session_subscribe(s,observe,&f,&sub)==PG_SESSION_OK);
  pangu_tool tool={.name=S("guard"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.execute=execute};
  pangu_loop_config cfg={.ctx=&f,.model=model,.tools=&tool,.tool_count=1};
  pangu_content text={.kind=PANGU_TEXT,.data=S("raw prompt")};
  pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
  pg_session_status z=pg_agent_app_run(f.app,&input,1,&cfg);
  assert(z==(mode==4?PG_SESSION_CANCELLED:mode==8?PG_SESSION_IO:PG_SESSION_OK));
  if(mode==8) {
    if(backend==PG_STORE_JSONL) {uv_fs_t req; assert(!uv_fs_rmdir(NULL,&req,path,NULL));uv_fs_req_cleanup(&req);assert(!rename("approval-failure.old",path));}
    else {assert(sqlite3_exec(f.db,"DROP TRIGGER fail_approval",NULL,NULL,NULL)==SQLITE_OK);sqlite3_close(f.db);}
    f.want=PG_APPROVAL_CANCELLED;
  }
  assert(f.effects==(mode==0 || mode==5 || mode==6));
  if(mode==6) assert(f.observed==1 && !f.answer_calls);
  pg_agent_app_close(f.app); opts.create=0;
  assert(pg_agent_app_open(&opts,NULL,0,&f.app)==PG_OK); s=pg_agent_app_session(f.app);
  f.asked=f.decided=0;
  assert(pg_session_approval_events(s,scan,&f)==PG_SESSION_OK && f.asked==1 && f.decided==1);
  assert(pg_session_approval_reply(s,f.id?f.id:1,PG_APPROVAL_ALLOWED_ONCE)==PG_SESSION_NOT_FOUND);
  assert(!pg_session_approval_pending(s));
  assert(pg_session_reset(s,S("handoff"))==PG_SESSION_OK);
  f.asked=f.decided=0;
  assert(pg_session_approval_events(s,scan,&f)==PG_SESSION_OK && f.asked==1 && f.decided==1);
  pg_agent_app_close(f.app); remove(path);
}
static pg_session_status bridge_scan(void *ctx, const pg_approval_event *e) {
  fixture *f=ctx;
  if (e->decided) { f->decided++; assert(e->outcome==f->want); }
  else { f->asked++; assert(e->question.tool_name.len==5); }
  return PG_SESSION_OK;
}
static pangu_status bridge_effect(void *ctx,pangu_core *core,const pangu_content *call,
    pangu_tool_sink sink,void *data) {
  (void)core; fixture *f=ctx;
  f->asked=f->decided=0;
  assert(pg_session_approval_events(pg_agent_app_session(f->app),bridge_scan,f)==PG_SESSION_OK);
  assert(f->asked==f->decided && f->decided>0); /* durable before actual source */
  f->effects++;
  pangu_content text={.kind=PANGU_TEXT,.data=S("effect")};
  pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1,
      .tool_call_id=call->id,.tool_name=call->name};
  return sink(data,&result,1);
}
static pg_status bridge_answer(void *ctx,pg_approval_provider *p,uint64_t id,
    pangu_core *core,const pg_service_approval_question *q,pg_service_approval_outcome *out) {
  fixture *f=ctx; f->answer_calls++; f->id=id;
  assert(q->tool_name.len==5);
  assert(pg_session_approval_pending(pg_agent_app_session(f->app)));
  assert(pg_agent_app_set_approval(f->app,PG_APP_APPROVAL_DISABLED)==PG_FAILED);
  *out=PG_SERVICE_ALLOWED_ONCE;
  if(f->mode==1) *out=PG_SERVICE_REJECTED;
  if(f->mode==2) pg_session_cancel(pg_agent_app_session(f->app));
  if(f->mode==4) return PG_FAILED;
  if(f->mode==5) *out=(pg_service_approval_outcome)99;
  if(f->mode==6) pg_approval_provider_revoke(p);
  if(f->mode==0) {
    assert(pg_approval_provider_reply(p,id,PG_SERVICE_ALLOWED_ONCE)==PG_OK);
    assert(pg_approval_provider_reply(p,id,PG_SERVICE_ALLOWED_ONCE)==PG_NOT_FOUND);
  }
  (void)core; return PG_OK;
}
static void bridge_test(pg_session_backend backend,int mode) {
  const char *path=backend==PG_STORE_JSONL?"approval-bridge.jsonl":"approval-bridge.db";
  remove(path);
  fixture f={.mode=mode,.want=mode==0?PG_APPROVAL_ALLOWED_ONCE:
      mode==1 || mode==7?PG_APPROVAL_REJECTED:mode==2?PG_APPROVAL_CANCELLED:PG_APPROVAL_UNAVAILABLE};
  pg_session_options opts={{path,strlen(path)},backend,1,0};
  assert(pg_agent_app_open(&opts,NULL,0,&f.app)==PG_OK);
  pg_approval_provider *provider=NULL;
  if(mode!=3) assert(pg_approval_provider_register(pg_agent_app_scope(f.app),bridge_answer,&f,&provider)==PG_OK);
  assert(pg_agent_app_set_approval(f.app,mode==7?PG_APP_APPROVAL_NEVER:PG_APP_APPROVAL_ASK)==PG_OK);
  pangu_tool tool={.name=S("guard"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.execute=bridge_effect};
  pangu_loop_config cfg={.model=model,.tools=&tool,.tool_count=1};
  pangu_content text={.kind=PANGU_TEXT,.data=S("bridge raw")};
  pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
  /* Same call id in successive runs must ask again; each grant is one-shot. */
  for(int run=0;run<2;run++) {
    assert(pg_agent_app_run(f.app,&input,1,&cfg)==(mode==2?PG_SESSION_CANCELLED:PG_SESSION_OK));
    assert(f.effects==(mode==0?run+1:0));
  }
  assert(f.answer_calls==(mode==3 || mode==7?0:mode==6?1:2));
  if(provider) {
    assert(pg_approval_provider_reply(provider,f.id,PG_SERVICE_ALLOWED_ONCE)==PG_NOT_FOUND);
    pg_approval_provider_revoke(provider);
  }
  pg_agent_app_close(f.app); opts.create=0;
  assert(pg_agent_app_open(&opts,NULL,0,&f.app)==PG_OK);
  f.asked=f.decided=0;
  assert(pg_session_approval_events(pg_agent_app_session(f.app),bridge_scan,&f)==PG_SESSION_OK);
  assert(f.asked==2 && f.decided==2 && !pg_session_approval_pending(pg_agent_app_session(f.app)));
  /* Re-enable ASK after reopen, with no provider: old grants never authorize. */
  f.want=PG_APPROVAL_UNAVAILABLE;
  assert(pg_agent_app_set_approval(f.app,PG_APP_APPROVAL_ASK)==PG_OK);
  assert(pg_agent_app_run(f.app,&input,1,&cfg)==PG_SESSION_OK);
  assert(f.effects==(mode==0?2:0));
  pg_agent_app_close(f.app); remove(path);
}
typedef struct {
  fixture f;
  int prepares, requests, clear, fail, next_hook, ended, asynchronous, started, released;
  pangu_tool initial;
  struct { void *ctx; pangu_validate_fn validate; } held;
} dynamic_fixture;
static pangu_status dynamic_pump(void *ctx,pangu_core *core) {
  (void)ctx; (void)core;
  assert(0 && "synchronous final does not require pumping");
  return PANGU_INVALID;
}
static pangu_status dynamic_start(void *ctx,pangu_core *core,const pangu_content *call,
    pangu_tool_sink sink,void *out,void **operation) {
  dynamic_fixture *d=ctx;
  d->started++;
  *operation=d;
  return bridge_effect(&d->f,core,call,sink,out);
}
static void dynamic_release(void *ctx,void *operation) {
  dynamic_fixture *d=ctx;
  assert(operation==d);
  d->released++;
}
static pangu_status dynamic_prepare(void *ctx,pangu_core *core,
    pangu_request_update_sink sink,void *out) {
  dynamic_fixture *d=ctx;
  assert(d && d->f.app);
  assert(d->started==d->released);
  int turn=d->prepares++;
  if(turn==0) {
    assert(pangu_core_active_tool_count(core)==(d->initial.name.len?1u:0u));
    if(d->initial.name.len) {
      const pangu_tool *active=pangu_core_active_tool_at(core,0);
      d->held.ctx=active->ctx; d->held.validate=active->validate;
    }
  }
  /* Callback-local declaration bytes must survive subsequent callbacks. */
  char name[]="guard", schema[]="{}";
  pangu_tool tool={.name={name,5},.schema_json={schema,2},.ctx=&d->f,
      .validate=validate,.execute=bridge_effect};
  if(d->asynchronous) { tool.ctx=d; tool.execute=NULL; tool.start=dynamic_start; tool.release=dynamic_release; }
  if(turn==1) tool.description=S("second generation");
  pangu_request_update update={.fields=PANGU_UPDATE_TOOLS,.tools=&tool,
      .tool_count=d->clear && turn>0?0:1};
  pangu_status z=sink(out,&update);
  memset(name,'x',5); memset(schema,'x',2);
  assert(z==PANGU_OK);
  if(d->fail==2) {
    assert(sink(out,&update)==PANGU_INVALID); /* duplicate remains sticky even if ignored */
    return PANGU_OK;
  }
  return d->fail?PANGU_INVALID:PANGU_OK;
}
static pangu_status dynamic_model(void *ctx,pangu_core *core,pangu_model_sink sink,void *out) {
  dynamic_fixture *d=ctx;
  size_t count=d->clear && d->requests>0?0:1;
  /* Next-turn hook leaves the initial selection for the first request. */
  assert(pangu_core_active_tool_count(core)==count);
  if(count) {
    const pangu_tool *t=pangu_core_active_tool_at(core,0);
    assert(t->name.len==5 && !memcmp(t->name.data,"guard",5));
    assert(t->ctx!=&d->f && t->execute!=bridge_effect);
    if(d->requests==0) {
      d->held.ctx=t->ctx; d->held.validate=t->validate;
    }
    if(d->requests==1 && !d->next_hook)
      assert(t->description.len==17 && !memcmp(t->description.data,"second generation",17));
  }
  pangu_content content=d->requests++<2
      ?(pangu_content){.kind=PANGU_TOOL_CALL,.id=S("dynamic"),.name=S("guard"),.data=S("{}")}
      :(pangu_content){.kind=PANGU_TEXT,.data=S("done")};
  pangu_message m={.role=PANGU_ASSISTANT,.content=&content,.content_count=1,
      .stop_reason=content.kind==PANGU_TOOL_CALL?PANGU_TOOL_USE:PANGU_STOP};
  return sink(out,&m,1);
}
static pangu_status dynamic_request(void *ctx,pangu_core *core,const pangu_core *request,
    pangu_model_sink sink,void *out) {
  dynamic_fixture *d=(dynamic_fixture *)((char *)ctx - offsetof(dynamic_fixture,requests));
  assert(ctx==&d->requests && request);
  return dynamic_model(d,core,sink,out);
}
static void dynamic_event(void *ctx,pangu_core *core,const pangu_event *event) {
  dynamic_fixture *d=ctx;
  if(event->kind!=PANGU_AGENT_END) return;
  d->ended++;
  if(d->fail) {
    assert(pangu_core_active_tool_count(core)==(d->initial.name.len?1u:0u));
    assert(!d->requests && !d->f.effects);
  }
  /* Retain only the callback/context pair. Core active declaration slices are
   * borrowed until the next committed preparation, even though the app keeps
   * the wrapper/source context alive through run return. */
  if(d->held.validate) assert(d->held.validate(d->held.ctx,S("{}"),S("{}"))==PANGU_OK);
}
static void dynamic_test(pg_session_backend backend,int initial,int mode,int clear,int fail,int next,int asynchronous) {
  const char *path=backend==PG_STORE_JSONL?"approval-dynamic.jsonl":"approval-dynamic.db";
  remove(path);
  dynamic_fixture d={.f={.mode=mode,.want=mode==0?PG_APPROVAL_ALLOWED_ONCE:
      mode==1 || mode==7?PG_APPROVAL_REJECTED:mode==2?PG_APPROVAL_CANCELLED:PG_APPROVAL_UNAVAILABLE},
      .clear=clear,.fail=fail,.next_hook=next,.asynchronous=asynchronous};
  pg_session_options opts={{path,strlen(path)},backend,1,0};
  assert(pg_agent_app_open(&opts,NULL,0,&d.f.app)==PG_OK);
  pg_approval_provider *provider=NULL;
  if(mode!=3) assert(pg_approval_provider_register(pg_agent_app_scope(d.f.app),bridge_answer,&d.f,&provider)==PG_OK);
  assert(pg_agent_app_set_approval(d.f.app,mode==7?PG_APP_APPROVAL_NEVER:PG_APP_APPROVAL_ASK)==PG_OK);
  if(initial) d.initial=(pangu_tool){.name=S("guard"),.schema_json=S("{}"),.ctx=&d.f,.validate=validate,.execute=bridge_effect};
  if(initial && asynchronous) { d.initial.ctx=&d; d.initial.execute=NULL; d.initial.start=dynamic_start; d.initial.release=dynamic_release; }
  pangu_loop_config cfg={.ctx=&d,.request=dynamic_request,.has_request_ctx=1,.request_ctx=&d.requests,.tools=initial?&d.initial:NULL,.tool_count=initial,
      .event=dynamic_event,.pump=dynamic_pump};
  if(next) cfg.prepare_next_turn_update=dynamic_prepare;
  else cfg.prepare_request_update=dynamic_prepare;
  pangu_content text={.kind=PANGU_TEXT,.data=S("dynamic input")};
  pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
  pg_session_status z=pg_agent_app_run(d.f.app,&input,1,&cfg);
  assert(z==(fail?PG_SESSION_CORE_ERROR:mode==2?PG_SESSION_CANCELLED:PG_SESSION_OK));
  int calls=fail?0:mode==2?1:clear?1:2;
  assert(d.f.effects==(mode==0?calls:0));
  assert(d.ended==1 && d.started==d.released);
  assert(d.started==(asynchronous && mode==0?calls:0));
  assert(d.f.answer_calls==(mode==3 || mode==7?0:calls));
  pg_agent_app_close(d.f.app); opts.create=0;
  assert(pg_agent_app_open(&opts,NULL,0,&d.f.app)==PG_OK);
  d.f.asked=d.f.decided=0;
  assert(pg_session_approval_events(pg_agent_app_session(d.f.app),bridge_scan,&d.f)==PG_SESSION_OK);
  assert(d.f.asked==calls && d.f.decided==calls);
  pg_agent_app_close(d.f.app); remove(path);
}
int main(int argc,char **argv) {
  if(argc==3) { /* subprocess crash fixture receives isolated CTest directory */
    pg_session_backend b=atoi(argv[1])?PG_STORE_SQLITE:PG_STORE_JSONL;
    fixture f={.mode=7,.path=argv[2],.backend=b};
    pg_session_options opts={{argv[2],strlen(argv[2])},b,1,0};
    assert(pg_agent_app_open(&opts,NULL,0,&f.app)==PG_OK);
    pangu_tool tool={.name=S("guard"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.execute=execute};
    pangu_loop_config cfg={.ctx=&f,.model=model,.tools=&tool,.tool_count=1};
    pangu_content text={.kind=PANGU_TEXT,.data=S("crash")};
    pangu_message m={.role=PANGU_USER,.content=&text,.content_count=1};
    (void)pg_agent_app_run(f.app,&m,1,&cfg); abort();
  }
  for(int b=0;b<2;b++) for(int mode=0;mode<10;mode++) if(mode!=7) test((pg_session_backend)b,mode);
  for(int b=0;b<2;b++) for(int mode=0;mode<8;mode++) bridge_test((pg_session_backend)b,mode);
  for(int b=0;b<2;b++) for(int initial=0;initial<2;initial++) {
    for(int mode=0;mode<8;mode++) if(mode<=3 || mode==7)
      for(int clear=0;clear<2;clear++) dynamic_test((pg_session_backend)b,initial,mode,clear,0,0,initial);
    dynamic_test((pg_session_backend)b,initial,0,0,1,0,initial);
    dynamic_test((pg_session_backend)b,initial,0,0,2,0,initial);
    if(initial) dynamic_test((pg_session_backend)b,initial,0,0,0,1,1);
  }
  puts("app/Core real guarded tool: one-shot decisions, pending/reply, fail closed, cancellation, audit IO/reopen passed");
  return 0;
}
