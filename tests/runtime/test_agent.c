#include "../../src/runtime/agent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {int turn,step,closed,cancelled,executed,mode,denied;} fixture;
static pg_slice s(const char *p){return (pg_slice){p,strlen(p)};}
static int start(void *p,const pg_session *session){fixture *f=p;f->step=0;++f->turn;
 if(f->turn==2){assert(session->messages[session->count-1].role==PG_TOOL_RESULT);assert(session->messages[session->count-1].is_error==(f->denied || f->mode==6));}
 return f->mode==3;}
static int next(void *p,pg_model_event *e){fixture *f=p;++f->step;
 if(f->mode==1){e->kind=PG_MODEL_WAIT;return 0;}
 if(f->mode==2){e->kind=PG_MODEL_ERROR;return 0;}
 if(f->mode==7 && f->step>1){e->kind=PG_MODEL_ERROR;return 0;}
 if(f->mode==8 && f->step>1){e->kind=PG_MODEL_CALL;e->text=(pg_slice){0,1};e->call_id=s("bad");e->tool_name=s("echo");return 0;}
 if(f->mode==4){e->kind=(pg_model_kind)99;return 0;}
 if(f->step==1){e->kind=PG_MODEL_TEXT;e->text=s(f->turn==1?"checking":"done");}
 else if(f->turn==1 || f->mode==5){e->kind=PG_MODEL_CALL;e->text=s("{}");e->call_id=s("call-1");e->tool_name=s("echo");}
 else e->kind=PG_MODEL_DONE;
 return 0;}
static void cancel_model(void *p){++((fixture*)p)->cancelled;}
static void close_model(void *p){++((fixture*)p)->closed;}
static int allow(void *p,pg_slice name,pg_slice args){fixture *f=p;assert(name.size==4 && args.size==2);return !f->denied;}
static int execute(void *p,pg_slice name,pg_slice args,char *out,size_t cap,size_t *n){fixture *f=p;(void)name;(void)args;assert(cap>=3);++f->executed;if(f->mode==6){*n=cap;return 0;}memcpy(out,"ok",2);*n=2;return 0;}
static pg_agent agent;
static pg_client setup(fixture *f){assert(!pg_agent_init(&agent,(pg_model){f,start,next,cancel_model,close_model},(pg_tools){f,allow,execute,0,0,0,0}));return pg_agent_client(&agent);}
static pg_outcome drain(pg_client c,int *count){pg_event e;int i;*count=0;for(i=0;i<256;++i)if(c.poll(c.context,&e)){++*count;if(e.kind==PG_RUN_FINISHED){pg_outcome outcome=e.outcome;if(outcome==PG_ERROR)assert(e.text.size>0);assert(!c.poll(c.context,&e));return outcome;}}assert(0);return PG_ERROR;}
int main(void){fixture f={0};pg_client c=setup(&f);int n;pg_event e;
 assert(!c.submit(c.context,s("hello")));assert(c.submit(c.context,s("busy")));
 assert(drain(c,&n)==PG_SUCCESS);assert(n==6 && f.turn==2 && f.executed==1 && f.closed==2);assert(agent.session.count==5);
 assert(!pg_agent_set_system(&agent,s("system")));assert(!pg_agent_reset_session(&agent));assert(agent.session.count==0 && agent.session.system_size==6);
 assert(!c.submit(c.context,s("second")));assert(pg_agent_reset_session(&agent));assert(pg_agent_set_system(&agent,s("busy")));assert(agent.session.system_size==6);assert(drain(c,&n)==PG_SUCCESS);
 assert(!pg_agent_set_system(&agent,s("")));assert(!agent.session.system_size);
 f=(fixture){.denied=1};c=setup(&f);assert(!c.submit(c.context,s("hello")));assert(drain(c,&n)==PG_SUCCESS);assert(!f.executed && f.closed==2);
 f=(fixture){.mode=6};c=setup(&f);assert(!c.submit(c.context,s("oversized output")));assert(drain(c,&n)==PG_SUCCESS);assert(agent.session.messages[3].role==PG_TOOL_RESULT&&agent.session.messages[3].is_error);assert(f.executed==1);
 for(int mode=2;mode<=4;++mode){f=(fixture){.mode=mode};c=setup(&f);assert(!c.submit(c.context,s("hello")));pg_outcome outcome=drain(c,&n);assert(outcome==PG_ERROR);assert(f.closed==1);}
 f=(fixture){.mode=1};c=setup(&f);assert(!c.submit(c.context,s("hello")));assert(c.poll(c.context,&e)&&e.kind==PG_RUN_STARTED);assert(!c.poll(c.context,&e));assert(!c.poll(c.context,&e));c.cancel(c.context);c.cancel(c.context);assert(drain(c,&n)==PG_CANCELLED);assert(f.cancelled==1 && f.closed==1);
 f=(fixture){0};c=setup(&f);assert(!c.submit(c.context,s("hello")));c.cancel(c.context);assert(drain(c,&n)==PG_CANCELLED);assert(!f.turn && !f.executed);
 assert(c.submit(c.context,(pg_slice){0,1}));assert(c.submit(c.context,s("")));assert(agent.session.count==1);
 f=(fixture){0};c=setup(&f);assert(!c.submit(c.context,s("cancel tool")));
 for(int j=0;j<16;++j)if(c.poll(c.context,&e)&&e.kind==PG_TOOL_STARTED)break;
 assert(e.kind==PG_TOOL_STARTED);c.cancel(c.context);
 assert(c.poll(c.context,&e)&&e.kind==PG_TOOL_FINISHED&&e.outcome==PG_CANCELLED);
 assert(agent.session.messages[agent.session.count-1].role==PG_TOOL_RESULT);
 assert(agent.session.messages[agent.session.count-1].is_error && !f.executed);
 assert(drain(c,&n)==PG_CANCELLED);
 f=(fixture){.mode=5};c=setup(&f);assert(!c.submit(c.context,s("bounded")));assert(drain(c,&n)==PG_ERROR);assert(f.turn==PG_TURN_LIMIT && f.executed==PG_TURN_LIMIT && f.closed==PG_TURN_LIMIT);
 f=(fixture){0};c=setup(&f);agent.session.count=PG_MESSAGE_LIMIT-3;assert(c.submit(c.context,s("full")));assert(agent.session.count==PG_MESSAGE_LIMIT-3);
 char oversized[PG_TEXT_LIMIT+1];memset(oversized,'x',sizeof(oversized));c=setup(&f);assert(c.submit(c.context,(pg_slice){oversized,sizeof(oversized)}));assert(agent.session.count==0);
 /* Preview is not accepted history. Error/abort/invalid tool arguments cannot admit it. */
 for(int mode=0;mode<3;++mode){f=(fixture){.mode=mode==0?0:mode==1?7:8};c=setup(&f);
  assert(!c.submit(c.context,s("preview")));pg_message admitted=agent.session.messages[0];
  assert(c.poll(c.context,&e)&&e.kind==PG_RUN_STARTED);assert(!c.poll(c.context,&e));
  assert(c.poll(c.context,&e)&&e.kind==PG_TEXT_DELTA&&e.text.size==8);
  assert(agent.session.count==1&&!memcmp(&admitted,&agent.session.messages[0],sizeof(admitted)));
  if(mode==0)c.cancel(c.context);
  assert(drain(c,&n)==(mode==0?PG_CANCELLED:PG_ERROR));
  assert(agent.session.count==1&&!memcmp(&admitted,&agent.session.messages[0],sizeof(admitted)));
  assert(agent.draft.size==0&&f.executed==0&&f.closed==1);
 }
 f=(fixture){0};c=setup(&f);assert(!c.submit(c.context,s("settled")));
 for(int j=0;j<16;++j)if(c.poll(c.context,&e)&&e.kind==PG_TOOL_STARTED)break;
 assert(e.kind==PG_TOOL_STARTED&&agent.session.count==3);
 pg_message settled=agent.session.messages[1];assert(settled.role==PG_ASSISTANT&&settled.size==8);
 assert(drain(c,&n)==PG_SUCCESS);
 assert(!memcmp(&settled,&agent.session.messages[1],sizeof(settled))&&agent.draft.size==0);
 puts("PASS: tool round-trip, denied side effects, model failures, cancellation, busy and invalid input");return 0;}
