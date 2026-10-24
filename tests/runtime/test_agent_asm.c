#define _GNU_SOURCE
#include "../../src/runtime/agent.h"
#include "../../src/runtime/profile_layout.h"
#include "../../src/runtime/agent_layout.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define OFFSET(type,field,value) _Static_assert(offsetof(type,field)==value,#type "." #field)
_Static_assert(sizeof(pg_agent)==PG_A_BYTES,"agent size");
_Static_assert(sizeof(pg_message)==PG_MSG_BYTES,"message size");
_Static_assert(sizeof(pg_model)==40 && sizeof(pg_tools)==24 && sizeof(pg_client)==32,"aggregate ABI");
_Static_assert(sizeof(pg_event)==56 && sizeof(pg_model_event)==56,"event size");
OFFSET(pg_message,size,PG_MSG_SIZE);OFFSET(pg_message,text,PG_MSG_TEXT);
OFFSET(pg_message,call_id,PG_MSG_ID);OFFSET(pg_message,tool_name,PG_MSG_NAME);OFFSET(pg_message,is_error,PG_MSG_ERROR);
OFFSET(pg_session,count,PG_A_COUNT);OFFSET(pg_session,system_prompt,PG_A_SYSTEM);OFFSET(pg_session,system_size,PG_A_SYSTEM_SIZE);
OFFSET(pg_agent,model,PG_A_MODEL);OFFSET(pg_agent,tools,PG_A_TOOLS);OFFSET(pg_agent,state,PG_A_STATE);
OFFSET(pg_agent,turns,PG_A_TURNS);OFFSET(pg_agent,cancelled,PG_A_CANCEL);OFFSET(pg_agent,request_open,PG_A_OPEN);
OFFSET(pg_agent,outcome,PG_A_OUTCOME);OFFSET(pg_agent,assistant_index,PG_A_ASSISTANT);OFFSET(pg_agent,call_index,PG_A_CALL);
OFFSET(pg_agent,draft,PG_A_DRAFT);OFFSET(pg_agent,pending,PG_A_PENDING);OFFSET(pg_agent,commits,PG_A_COMMITS);OFFSET(pg_agent,next_checkpoint,PG_A_NEXT);
OFFSET(pg_agent,event_text,PG_A_TEXT);OFFSET(pg_agent,error_text,PG_A_ERROR);
OFFSET(pg_event,text,8);OFFSET(pg_event,call_id,24);OFFSET(pg_event,tool_name,40);
OFFSET(pg_model_event,text,8);OFFSET(pg_model_event,call_id,24);OFFSET(pg_model_event,tool_name,40);
_Static_assert(PG_ROLE_USER==PG_USER && PG_ROLE_ASSISTANT==PG_ASSISTANT && PG_ROLE_CALL==PG_TOOL_CALL && PG_ROLE_RESULT==PG_TOOL_RESULT,"roles");
_Static_assert(PG_ME_WAIT==PG_MODEL_WAIT && PG_ME_TEXT==PG_MODEL_TEXT && PG_ME_CALL==PG_MODEL_CALL && PG_ME_DONE==PG_MODEL_DONE && PG_ME_ERROR==PG_MODEL_ERROR,"model events");
_Static_assert(PG_EV_BEGIN==PG_RUN_STARTED && PG_EV_TEXT==PG_TEXT_DELTA && PG_EV_TOOL_BEGIN==PG_TOOL_STARTED && PG_EV_TOOL_END==PG_TOOL_FINISHED && PG_EV_FINISH==PG_RUN_FINISHED,"client events");
_Static_assert(PG_RESULT_OK==PG_SUCCESS && PG_RESULT_ERROR==PG_ERROR && PG_RESULT_CANCELLED==PG_CANCELLED,"outcomes");
typedef struct {unsigned requests,step,closes,cancels,executes;int large;} fixture;
int pg_test_start(void *,const pg_session *);
int pg_test_next(void *,pg_model_event *);
void pg_test_cancel(void *);
void pg_test_close(void *);
int pg_test_authorize(void *,pg_slice,pg_slice);
int pg_test_execute(void *,pg_slice,pg_slice,char *,size_t,size_t *);
typedef void (*probe_fn)(void);
unsigned pg_abi_probe(probe_fn,void *,const void *,size_t);
unsigned pg_abi_probe4(probe_fn,void *,void *,size_t,void *);
void pg_abi_corrupt(void);
static char full[PG_TEXT_LIMIT];
pg_store_result pg_test_storage(void *,const pg_session_change *,uint64_t,uint64_t *);
pg_store_result pg_fixture_storage(void *ctx,const pg_session_change *change,uint64_t previous,uint64_t *sequence){(void)ctx;assert(change->kind<=PG_SESSION_CHECKPOINT);*sequence=previous+1;return PG_STORE_OK;}
int pg_fixture_start(void *ctx,const pg_session *session){fixture *f=ctx;++f->requests;f->step=0;assert(session->count);return 0;}
int pg_fixture_next(void *ctx,pg_model_event *event){fixture *f=ctx;*event=(pg_model_event){0};
 if(f->large){event->kind=f->step++?PG_MODEL_DONE:PG_MODEL_TEXT;event->text=(pg_slice){full,sizeof(full)-1};return 0;}
 if(f->requests==1){event->kind=f->step++?PG_MODEL_CALL:PG_MODEL_TEXT;event->text=(pg_slice){f->step==1?"check":"{}",f->step==1?5:2};event->call_id=(pg_slice){"abi-7",5};event->tool_name=(pg_slice){"echo",4};}
 else{event->kind=f->step++?PG_MODEL_DONE:PG_MODEL_TEXT;event->text=(pg_slice){"done",4};}return 0;}
void pg_fixture_cancel(void *ctx){++((fixture*)ctx)->cancels;}
void pg_fixture_close(void *ctx){++((fixture*)ctx)->closes;}
int pg_fixture_authorize(void *ctx,pg_slice name,pg_slice args){(void)ctx;assert(name.size==4&&!memcmp(name.data,"echo",4));assert(args.size==2&&!memcmp(args.data,"{}",2));return 1;}
int pg_fixture_execute(void *ctx,pg_slice name,pg_slice args,char *out,size_t capacity,size_t *size){fixture *f=ctx;assert(name.size==4&&args.size==2&&capacity==PG_TEXT_LIMIT);memcpy(out,"ok",2);*size=2;++f->executes;return 0;}
static void exercise(pg_agent *agent){fixture f={0};pg_model model={&f,pg_test_start,pg_test_next,pg_test_cancel,pg_test_close};pg_tools tools={&f,pg_test_authorize,pg_test_execute};
 assert(!pg_agent_init(agent,model,tools));
 assert(!pg_agent_bind_storage(agent,(pg_storage){0,pg_test_storage}));
 pg_client client=pg_agent_client(agent);
 assert(!pg_abi_probe((probe_fn)pg_agent_set_system,agent,"test",4));assert(agent->session.system_size==4);
 assert(!pg_abi_probe((probe_fn)client.submit,client.context,"input",5));
 struct {uint64_t before;pg_event event;uint64_t after;} guarded={.before=0x148194,.after=0x832985};unsigned terminal=0,events=0;
 for(unsigned i=0;i<40&&!terminal;++i){guarded.event=(pg_event){.kind=(pg_event_kind)99};assert(!pg_abi_probe((probe_fn)client.poll,client.context,&guarded.event,0));
  assert(guarded.before==0x148194&&guarded.after==0x832985);
  if(guarded.event.kind==PG_RUN_FINISHED){assert(guarded.event.outcome==PG_SUCCESS);++terminal;}
  if(guarded.event.kind==PG_TOOL_FINISHED){assert(guarded.event.text.size==2&&!memcmp(guarded.event.text.data,"ok",2));++events;}
 }
 /* No-event polls clear kind to zero; the terminal has to be observed explicitly. */
 assert(terminal==1&&events==1&&f.requests==2&&f.executes==1&&f.closes==2);
 memset(full,'x',sizeof(full)-1);full[sizeof(full)-1]=0;
 assert(!pg_agent_set_system(agent,(pg_slice){full,sizeof(full)-1}));
 assert(!pg_agent_set_system(agent,(pg_slice){agent->session.system_prompt+1,sizeof(full)-2}));assert(agent->session.system_size==sizeof(full)-2);
 assert(!pg_abi_probe((probe_fn)pg_agent_suspend,agent,0,0));
 assert(!pg_abi_probe((probe_fn)pg_agent_resume,agent,0,0));
 assert(!pg_abi_probe((probe_fn)pg_agent_reset_session,agent,0,0));assert(!agent->session.count&&agent->session.system_size==sizeof(full)-2);
 f=(fixture){.large=1};assert(!client.submit(client.context,(pg_slice){full,sizeof(full)-1}));terminal=0;
 for(unsigned i=0;i<12;++i){pg_event event;if(client.poll(client.context,&event)&&event.kind==PG_RUN_FINISHED){assert(event.outcome==PG_SUCCESS);terminal=1;break;}}
 assert(terminal&&agent->session.messages[1].size==sizeof(full)-1);
 assert(!client.submit(client.context,(pg_slice){"cancel",6}));pg_event event;assert(client.poll(client.context,&event));assert(!client.poll(client.context,&event));
 assert(!pg_abi_probe((probe_fn)client.cancel,client.context,0,0));assert(client.poll(client.context,&event)&&event.kind==PG_RUN_FINISHED&&event.outcome==PG_CANCELLED);assert(f.cancels==1);
 f=(fixture){0};assert(!pg_agent_init(agent,model,tools));client=pg_agent_client(agent);
 assert(!client.submit(client.context,(pg_slice){"sequence",8}));
 const pg_event_kind expected[]={PG_RUN_STARTED,PG_TEXT_DELTA,PG_TOOL_STARTED,PG_TOOL_FINISHED,PG_TEXT_DELTA,PG_RUN_FINISHED};size_t seen=0;
 for(unsigned i=0;i<40&&seen<sizeof(expected)/sizeof(expected[0]);++i){pg_event emitted;if(client.poll(client.context,&emitted)){assert(emitted.kind==expected[seen]);++seen;}}
 assert(seen==sizeof(expected)/sizeof(expected[0]));
 pg_session_control local={.storage={0,pg_test_storage}};
 pg_session_change change={.kind=PG_SESSION_SYSTEM,.text={"probe",5}};
 assert(!pg_abi_probe((probe_fn)pg_session_commit,&agent->session,&local,(size_t)&change));
 assert(local.sequence==1&&!local.busy&&!strcmp(agent->session.system_prompt,"probe"));
}
int main(void){assert(pg_abi_probe(pg_abi_corrupt,0,0,0)==65);assert(pg_abi_probe4(pg_abi_corrupt,0,0,0,0)==65);
 pg_profile_spec profile={.provider={"a",1},.model={"b",1},.workspace={"c",1},.implementation={"d",1}};unsigned char bytes[52];size_t length=0;
 assert(!pg_abi_probe4((probe_fn)pg_profile_encode,&profile,bytes,sizeof(bytes),&length)&&length==sizeof(bytes));
long page=sysconf(_SC_PAGESIZE);assert(page>0);size_t accessible=((sizeof(pg_agent)+(size_t)page-1)/(size_t)page)*(size_t)page,total=accessible+2*(size_t)page;
 for(unsigned side=0;side<2;++side){char *mapping=mmap(0,total,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(mapping!=MAP_FAILED);assert(!mprotect(mapping+page,accessible,PROT_READ|PROT_WRITE));pg_agent *agent=(pg_agent*)(mapping+page+(side?accessible-sizeof(pg_agent):0));exercise(agent);assert(!munmap(mapping,total));}
 puts("PASS: Agent ASM layout, register/DF/stack guards, callback clobbers, two guard placements and full text capacity");return 0;}
