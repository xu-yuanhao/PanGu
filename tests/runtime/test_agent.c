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
static pg_model_call batch_calls[8];
typedef struct {unsigned n,turn,executed,closed;int safe;unsigned phase,cancelled,close_wait;char ids[8][16];} batch_fixture;
static int batch_start(void *v,const pg_session *history){batch_fixture *f=v;++f->turn;if(f->turn==2){assert(history->count==2+2*f->n);for(unsigned i=0;i<f->n;++i){assert(history->messages[2+i].role==PG_TOOL_CALL);assert(history->messages[2+f->n+i].role==PG_TOOL_RESULT);assert(!strcmp(history->messages[2+i].call_id,history->messages[2+f->n+i].call_id));}}return 0;}
static int batch_next(void *v,pg_model_event *e){batch_fixture *f=v;if(f->turn==1){e->kind=PG_MODEL_BATCH;e->batch=(pg_model_batch){batch_calls,f->n};}else e->kind=PG_MODEL_DONE;return 0;}
static int batch_allow(void *v,pg_slice name,pg_slice args){(void)v;assert(name.size==4&&args.size==2);return 1;}
static int batch_safe(void *v,pg_slice name){(void)name;return ((batch_fixture*)v)->safe;}
static void batch_cancel(void *v){(void)v;}
static void batch_close(void *v){++((batch_fixture*)v)->closed;memset(batch_calls,0,sizeof(batch_calls));}
static int batch_execute(void *v,pg_slice name,pg_slice args,char *out,size_t cap,size_t *n){batch_fixture *f=v;assert(name.size==4&&args.size==2&&cap>2);assert(agent.session.count>=2+f->n+f->executed);++f->executed;memcpy(out,"ok",2);*n=2;return 0;}
static void batch_cases(void){
 for(unsigned n=2;n<=8;++n)for(unsigned cancel=0;cancel<=n;++cancel)for(unsigned timing=0;timing<3;++timing){batch_fixture f={.n=n};for(unsigned j=0;j<n;++j){snprintf(f.ids[j],16,"batch-%u",j);batch_calls[j]=(pg_model_call){s("{}"),s(f.ids[j]),s("echo")};}assert(!pg_agent_init(&agent,(pg_model){&f,batch_start,batch_next,batch_cancel,batch_close},(pg_tools){&f,batch_allow,batch_execute,0,0,0,0}));pg_client c=pg_agent_client(&agent);assert(!c.submit(c.context,s("batch")));unsigned ends=0,starts=0;int done=0;pg_event e;
 for(unsigned t=0;t<256;++t){if(!c.poll(c.context,&e)){if(cancel&&timing==1&&agent.state==5&&starts==cancel)c.cancel(c.context);continue;}if(e.kind==PG_TOOL_STARTED){++starts;if(cancel&&starts==cancel&&timing==0)c.cancel(c.context);}if(e.kind==PG_TOOL_FINISHED){assert(ends<n);assert(e.call_id.size==strlen(f.ids[ends])&&!memcmp(e.call_id.data,f.ids[ends],e.call_id.size));++ends;if(cancel&&timing==2&&ends==cancel)c.cancel(c.context);}if(e.kind==PG_RUN_FINISHED){assert(e.outcome==(cancel?PG_CANCELLED:PG_SUCCESS));done=1;break;}}
 assert(done&&ends==n&&f.executed==(cancel?(timing==0?cancel-1:cancel):n));assert(agent.commits.run.phase==PG_RUN_TERMINAL);assert(agent.session.count==(cancel?2+2*n:3+2*n));
 }
}
static int batch_async_execute(void *v,pg_slice name,pg_slice args,char *out,size_t cap,size_t *n){batch_fixture *f=v;(void)out;assert(name.size==4&&args.size==2&&cap>=2);++f->executed;f->phase=f->cancelled=f->close_wait=0;*n=0;return PG_TOOL_WAIT;}
static int batch_async_next(void *v,char *out,size_t cap,size_t *n){batch_fixture *f=v;assert(cap>=2);if(f->cancelled){*n=0;return PG_TOOL_ERROR;}if(!f->phase++){memcpy(out,"io",2);*n=2;return PG_TOOL_OUTPUT;}memcpy(out,"ok",2);*n=2;return PG_TOOL_OK;}
static void batch_async_cancel(void *v){((batch_fixture*)v)->cancelled=1;}
static int batch_async_close(void *v){return ((batch_fixture*)v)->close_wait++?0:1;}
static void batch_async_cases(void){
 for(unsigned target=0;target<=8;++target){batch_fixture f={.n=8};for(unsigned j=0;j<8;++j){snprintf(f.ids[j],16,"id-%u",j);batch_calls[j]=(pg_model_call){s("{}"),s(f.ids[j]),s("echo")};}assert(!pg_agent_init(&agent,(pg_model){&f,batch_start,batch_next,batch_cancel,batch_close},(pg_tools){&f,batch_allow,batch_async_execute,0,batch_async_next,batch_async_cancel,batch_async_close}));pg_client c=pg_agent_client(&agent);assert(!c.submit(c.context,s("async batch")));unsigned ends=0,output=0;int done=0;
 for(unsigned i=0;i<256;++i){pg_event e;if(!c.poll(c.context,&e))continue;if(e.kind==PG_TOOL_OUTPUT_DELTA){++output;if(output==target)c.cancel(c.context);}if(e.kind==PG_TOOL_FINISHED)++ends;if(e.kind==PG_RUN_FINISHED){assert(e.outcome==(target?PG_CANCELLED:PG_SUCCESS));done=1;break;}}
 assert(done&&ends==8&&f.executed==(target?target:8)&&!agent.tool_open);assert(agent.session.count==(target?18:19));
 }
}
static pg_session saved_batch_session;
static pg_session_control saved_batch_control;
static void batch_resume_cases(void){
 for(unsigned safe=0;safe<2;++safe)for(unsigned k=0;k<8;++k)for(unsigned aborting=0;aborting<2;++aborting){
 batch_fixture f={.n=8,.safe=(int)safe};for(unsigned j=0;j<8;++j){snprintf(f.ids[j],16,"batch-%u",j);batch_calls[j]=(pg_model_call){s("{}"),s(f.ids[j]),s("echo")};}
 pg_model m={&f,batch_start,batch_next,batch_cancel,batch_close};pg_tools tools={&f,batch_allow,batch_execute,batch_safe,0,0,0};assert(!pg_agent_init(&agent,m,tools));pg_client c=pg_agent_client(&agent);assert(!c.submit(c.context,s("resume batch")));pg_event e;unsigned polls=0;
 while(!(agent.session.count==10+k&&agent.commits.run.call_index==2+k&&agent.state==4)){assert(++polls<256);c.poll(c.context,&e);}
 assert(f.executed==k);if(aborting)c.cancel(c.context);assert(!pg_agent_suspend(&agent));saved_batch_session=agent.session;saved_batch_control=agent.commits;
 assert(!pg_agent_init(&agent,m,tools));agent.session=saved_batch_session;agent.commits=saved_batch_control;assert(!pg_agent_resume(&agent,0));int count;assert(drain(pg_agent_client(&agent),&count)==(aborting?PG_CANCELLED:PG_SUCCESS));assert(f.executed==(aborting?k:safe?8:7));assert(agent.session.count==(aborting?18:19));for(unsigned j=0;j<8;++j){assert(agent.session.messages[10+j].role==PG_TOOL_RESULT);assert(!strcmp(agent.session.messages[10+j].call_id,f.ids[j]));assert(agent.session.messages[10+j].is_error==(aborting?j>=k:(!safe&&j==k)));}
 }
}
static void batch_invalid(void){
 for(unsigned bad=0;bad<5;++bad){batch_fixture f={.n=8};for(unsigned j=0;j<8;++j){snprintf(f.ids[j],16,"id-%u",j);batch_calls[j]=(pg_model_call){s("{}"),s(f.ids[j]),s("echo")};}if(bad==0)batch_calls[7].arguments=(pg_slice){0,1};if(bad==1)batch_calls[7].call_id=batch_calls[0].call_id;if(bad==2)batch_calls[7].tool_name=s("");if(bad==3)f.n=9;if(bad==4)f.n=1;assert(!pg_agent_init(&agent,(pg_model){&f,batch_start,batch_next,batch_cancel,batch_close},(pg_tools){&f,batch_allow,batch_execute,0,0,0,0}));pg_client c=pg_agent_client(&agent);assert(!c.submit(c.context,s("invalid batch")));int count;assert(drain(c,&count)==PG_ERROR);assert(!f.executed&&agent.session.count==1);}
}
int main(void){batch_cases();batch_resume_cases();batch_invalid();batch_async_cases();fixture f={0};pg_client c=setup(&f);int n;pg_event e;
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
