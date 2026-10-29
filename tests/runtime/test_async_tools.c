#define _GNU_SOURCE
#include "../../src/runtime/agent.h"
#include "../../src/runtime/tools_layout.h"
#include "../../src/runtime/process.h"
#include "../../src/client/tui/runtime_bridge.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
typedef struct {
 pg_process process;
 const char *command;
 int root,mode,delay,close_fault,closed;
 unsigned starts,polls,cancels,closes;
 char result[4096];size_t size;
} worker;
static worker w;
static pg_tool_registry registry;
static pg_tools service;
static pg_agent agent;
static unsigned model_step,model_answer,model_starts;
static pg_slice s(const char *p){return (pg_slice){p,strlen(p)};}
static uint64_t now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static int valid(void *p,pg_slice args){(void)p;return args.size==2&&!memcmp(args.data,"{}",2);}
static int execute(void *p,pg_slice args,char *out,size_t cap,size_t *size){worker *x=p;assert(valid(p,args));++x->starts;x->closed=0;*size=0;if(x->mode==4){memcpy(out,"early error",11);*size=11;return PG_TOOL_ERROR;}pg_process_request request={.directory=x->root};assert(!pg_process_start(&x->process,&request,s(x->command)));if(x->mode==2){*size=cap;return PG_TOOL_OUTPUT;}if(x->mode==3){out[0]='x';*size=1;return PG_TOOL_WAIT;}if(x->mode==5)return 99;if(x->mode==1){memcpy(out,"initial",7);*size=7;return PG_TOOL_OUTPUT;}return PG_TOOL_WAIT;}
static int next(void *p,char *out,size_t cap,size_t *size){worker *x=p;++x->polls;*size=0;pg_process_event e;int rc=pg_process_next(&x->process,&e);assert(rc>=0);if(!rc)return PG_TOOL_WAIT;if(e.kind==PG_PROCESS_OUTPUT){assert(x->size+e.bytes.size<sizeof(x->result)&&e.bytes.size<cap);memcpy(x->result+x->size,e.bytes.data,e.bytes.size);x->size+=e.bytes.size;memcpy(out,e.bytes.data,e.bytes.size);*size=e.bytes.size;return PG_TOOL_OUTPUT;}assert(e.kind==PG_PROCESS_EXIT&&x->size<cap);memcpy(out,x->result,x->size);*size=x->size;return e.reason||e.exit_code?PG_TOOL_ERROR:PG_TOOL_OK;}
static void cancel(void *p){worker *x=p;++x->cancels;pg_process_cancel(&x->process);}
static int close_worker(void *p){worker *x=p;++x->closes;if(x->delay){--x->delay;return 1;}if(x->close_fault){x->close_fault=0;return -1;}int rc=pg_process_close(&x->process);if(!rc)x->closed=1;return rc;}
static const pg_tool_async async={next,cancel,close_worker,0,0};
static int model_start(void *p,const pg_session *session){(void)p;++model_starts;model_step=0;model_answer=session->messages[session->count-1].role==PG_TOOL_RESULT;if(model_answer){assert(w.closed&&!registry.active&&w.process.pid==-1);}return 0;}
static int model_next(void *p,pg_model_event *e){(void)p;*e=(pg_model_event){0};if(!model_step++){e->kind=PG_MODEL_TEXT;e->text=s(model_answer?"done":"working");}else if(model_answer)e->kind=PG_MODEL_DONE;else{e->kind=PG_MODEL_CALL;e->text=s("{}");e->call_id=s("job-1");e->tool_name=s("job");}return 0;}
static void model_noop(void *p){(void)p;}
static pg_model model(void){return (pg_model){0,model_start,model_next,model_noop,model_noop};}
static pg_client setup(int mode,const char *command){int root=w.root;memset(&w,0,sizeof(w));w.root=root;w.mode=mode;w.command=command;pg_process_init(&w.process);pg_tool_definition d={"job","unit operation with real process lifecycle","{\"type\":\"object\"}",&w,valid,valid,execute,0,&async};assert(!pg_tool_registry_init(&registry,&d,1));service=pg_tool_registry_service(&registry);model_starts=0;assert(!pg_agent_init(&agent,model(),service));return pg_agent_client(&agent);}
static pg_outcome drain(pg_client client,unsigned *progress){uint64_t until=now()+5000;for(;;){assert(now()<until);pg_event e;if(!client.poll(client.context,&e)){usleep(1000);continue;}if(e.kind==PG_TOOL_OUTPUT_DELTA){++*progress;assert(e.text.size<4096&&e.call_id.size==5&&e.tool_name.size==3);assert(agent.session.count==3);}if(e.kind==PG_TOOL_FINISHED){assert(w.closed&&!registry.active&&w.process.pid==-1);}if(e.kind==PG_RUN_FINISHED){assert(w.closed&&!registry.active&&w.process.pid==-1);pg_outcome result=e.outcome;assert(!client.poll(client.context,&e));return result;}}}
static void success_and_errors(void){for(int mode=0;mode<=5;++mode){pg_client client=setup(mode,mode==2||mode==3||mode==5?"sleep 2":"printf first; sleep .03; printf last");w.delay=2;assert(!client.submit(client.context,s("run")));unsigned progress=0;assert(drain(client,&progress)==PG_SUCCESS);assert(w.starts==1&&w.closed&&w.closes>=3&&model_starts==2);assert(agent.session.messages[3].role==PG_TOOL_RESULT);assert(agent.session.messages[3].is_error==(mode>=2));if(mode<2){assert(progress>0&&agent.session.messages[3].size==9&&!memcmp(agent.session.messages[3].text,"firstlast",9));}else assert(w.cancels==(mode==4?0u:1u));}
 pg_client client=setup(0,"printf complete");w.close_fault=1;assert(!client.submit(client.context,s("run")));unsigned progress=0;assert(drain(client,&progress)==PG_SUCCESS);assert(agent.session.messages[3].is_error&&strstr(agent.session.messages[3].text,"cleanup failed"));
}
static void cancellation(void){pg_client client=setup(0,"printf begin; sleep 2; printf late");assert(!client.submit(client.context,s("run")));uint64_t until=now()+1000;unsigned progress=0;for(;;){assert(now()<until);pg_event e;if(client.poll(client.context,&e)&&e.kind==PG_TOOL_OUTPUT_DELTA){++progress;break;}usleep(1000);}client.cancel(client.context);client.cancel(client.context);assert(drain(client,&progress)==PG_CANCELLED&&w.cancels==1&&model_starts==1);assert(agent.commits.run.phase==PG_RUN_TERMINAL&&agent.session.messages[3].is_error&&agent.session.messages[3].size==5&&!memcmp(agent.session.messages[3].text,"begin",5));}
static void suspend_and_resume(void){pg_client client=setup(0,"printf begin; sleep 2");assert(!client.submit(client.context,s("run")));uint64_t until=now()+1000;for(;;){assert(now()<until);pg_event e;if(client.poll(client.context,&e)&&e.kind==PG_TOOL_OUTPUT_DELTA)break;usleep(1000);}assert(agent.commits.run.phase==PG_RUN_TOOL_UNSAFE);int rc=pg_agent_suspend(&agent);assert(rc==0||rc==2);while(rc==2){assert(now()<until);pg_event e;assert(!client.poll(client.context,&e));usleep(1000);rc=pg_agent_suspend(&agent);}assert(!rc&&w.closed&&!registry.active&&agent.commits.run.phase==PG_RUN_TOOL_UNSAFE);assert(!pg_agent_resume(&agent,0));unsigned progress=0;assert(drain(client,&progress)==PG_SUCCESS&&w.starts==1);assert(agent.session.messages[3].is_error&&strstr(agent.session.messages[3].text,"interrupted"));}
static pg_store_result rejected_abort(void *p,const pg_session_change *change,uint64_t seq,uint64_t *next_seq){(void)p;if(change->checkpoint&&change->checkpoint->phase==PG_RUN_ABORTING)return PG_STORE_REJECTED;return pg_session_volatile_commit(0,change,seq,next_seq);}
static void cancel_store_failure(void){pg_client client=setup(0,"printf begin; sleep 2");assert(!pg_agent_bind_storage(&agent,(pg_storage){0,rejected_abort}));assert(!client.submit(client.context,s("run")));uint64_t until=now()+1000;for(;;){assert(now()<until);pg_event e;if(client.poll(client.context,&e)&&e.kind==PG_TOOL_OUTPUT_DELTA)break;usleep(1000);}client.cancel(client.context);unsigned progress=0;assert(drain(client,&progress)==PG_ERROR&&w.closed&&agent.commits.failed);}
static void registry_ownership(void){setup(0,"sleep .03; printf done");char out[64];size_t n=999;assert(service.execute(service.context,s("job"),s("{}"),out,sizeof(out),&n)==PG_TOOL_WAIT&&!n);assert(registry.active);unsigned starts=w.starts;assert(service.execute(service.context,s("job"),s("{}"),out,sizeof(out),&n)==PG_TOOL_ERROR&&w.starts==starts);service.cancel(service.context);service.cancel(service.context);assert(w.cancels==1);uint64_t until=now()+1000;while(service.close(service.context)){assert(now()<until);usleep(1000);}assert(w.closed&&!registry.active&&!service.close(service.context));assert(service.next(service.context,out,sizeof(out),&n)==PG_TOOL_ERROR);
 pg_tools partial=service;partial.next=0;assert(pg_agent_init(&agent,model(),partial));pg_tool_definition bad=registry.entries[0];pg_tool_async broken=async;broken.close=0;bad.async=&broken;assert(pg_tool_registry_init(&registry,&bad,1));}
static void bridge_shutdown(void){
 for(unsigned detach=0;detach<2;++detach){
  pg_client client=setup(0,"printf begin; sleep 2");pg_tui_bridge b;
  assert(!pg_tui_bridge_init(&b,client));
  if(detach)assert(!pg_tui_bridge_set_detach(&b,pg_agent_suspend,&agent));
  assert(b.ops.submit(&b,"run",3)==TUI_SUBMIT_OK);
  uint64_t until=now()+3000;
  for(;;){assert(now()<until);tui_event e;if(b.ops.poll(&b,&e)&&e.kind==TUI_EV_TOOL_OUTPUT)break;usleep(1000);}
  w.delay=20;unsigned pending=0;
  while(b.ops.shutdown(&b)){++pending;assert(now()<until&&b.stopped==2&&!w.closed);assert(b.ops.submit(&b,"late",4)==TUI_SUBMIT_ERROR);usleep(1000);}
  assert(pending&&b.stopped==1&&w.closed&&!registry.active&&w.process.pid==-1);
  unsigned closes=w.closes;assert(!b.ops.shutdown(&b)&&w.closes==closes);
  if(detach){assert(agent.commits.run.phase==PG_RUN_TOOL_UNSAFE&&!pg_agent_resume(&agent,0));unsigned progress=0;assert(drain(client,&progress)==PG_SUCCESS&&w.starts==1);}
  else assert(!b.active&&agent.commits.run.phase==PG_RUN_TERMINAL&&agent.commits.run.outcome==PG_CANCELLED);
 }
}
int main(void){char path[]="/tmp/pangu-async-XXXXXX";assert(mkdtemp(path));w.root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(w.root>=0);success_and_errors();cancellation();suspend_and_resume();cancel_store_failure();registry_ownership();bridge_shutdown();assert(!close(w.root)&&!rmdir(path));puts("async tools: real process progress/final, busy ownership, deferred/error cleanup, cancellation, suspend/unsafe resume and failed abort storage OK");return 0;}
