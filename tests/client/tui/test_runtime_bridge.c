#define _GNU_SOURCE
#include "../../../src/client/tui/runtime_bridge_layout.h"
#include "../../../src/runtime/agent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
static pg_agent agent;static int closed,cancelled,step;
static int start(void *c,const pg_session *s){(void)c;(void)s;return 0;}
static int next(void *c,pg_model_event *e){(void)c;*e=(pg_model_event){.kind=step++?PG_MODEL_DONE:PG_MODEL_TEXT,.text={"ok",2}};return 0;}
static void cancel_model(void *c){(void)c;++cancelled;}
static void close_model(void *c){(void)c;++closed;}
static pg_event source;
static int source_rc=1,submit_rc,submit_calls,cancel_calls,poll_calls;
int pg_bridge_fixture_submit(void *ctx,pg_slice text){(void)ctx;(void)text;++submit_calls;return submit_rc;}
void pg_bridge_fixture_cancel(void *ctx){(void)ctx;++cancel_calls;}
int pg_bridge_fixture_poll(void *ctx,pg_event *out){(void)ctx;++poll_calls;*out=source;return source_rc;}
static int detach_calls,detach_result;
int pg_bridge_fixture_detach(void *ctx){assert(ctx==&detach_calls);++detach_calls;return detach_result;}
int pg_bridge_test_detach(void *);
int pg_bridge_test_submit(void *,pg_slice);
void pg_bridge_test_cancel(void *);
int pg_bridge_test_poll(void *,pg_event *);
typedef void (*probe_fn)(void);
unsigned pg_bridge_probe(probe_fn,void *,const void *,size_t);
static void boundary_abi(void){long page=sysconf(_SC_PAGESIZE);assert(page>PG_B_BYTES);char *mapping=mmap(0,(size_t)page*3,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(mapping!=MAP_FAILED);assert(!mprotect(mapping+page,(size_t)page,PROT_READ|PROT_WRITE));pg_tui_bridge *b=(pg_tui_bridge*)(mapping+2*page-PG_B_BYTES);
 pg_client client={0,pg_bridge_test_submit,pg_bridge_test_cancel,pg_bridge_test_poll};assert(!pg_tui_bridge_init(b,client));pg_tui_bridge saved=*b;pg_client bad=client;bad.cancel=0;assert(pg_tui_bridge_init(b,bad)&&!memcmp(&saved,b,sizeof(saved)));assert(pg_tui_bridge_init(0,client));
 struct{uint64_t before;tui_event event;uint64_t after;} guarded={.before=0x874192,.after=0x194582};
 assert(!pg_bridge_probe((probe_fn)b->ops.submit,b,"u",1));assert(b->active&&submit_calls==1);assert(b->ops.submit(b,"busy",4)==TUI_SUBMIT_BUSY&&submit_calls==1);
 source=(pg_event){.kind=PG_RUN_STARTED};assert(!pg_bridge_probe((probe_fn)b->ops.poll,b,&guarded.event,0));assert(guarded.event.kind==TUI_EV_RUN_STARTED&&b->active);
 char *ids=mmap(0,(size_t)page*3,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(ids!=MAP_FAILED);assert(!mprotect(ids+page,(size_t)page,PROT_READ|PROT_WRITE));char *tail=ids+2*page-63;memset(tail,'x',63);
 source=(pg_event){.kind=PG_TOOL_STARTED,.call_id={tail,63},.tool_name={tail,63}};assert(!pg_bridge_probe((probe_fn)b->ops.poll,b,&guarded.event,0));assert(strlen(b->id)==63&&strlen(b->name)==63&&guarded.event.tool_call_id==b->id&&guarded.event.tool_name==b->name);
 source.call_id.size=64;assert(b->ops.poll(b,&guarded.event)<0);source.call_id.size=63;tail[62]=0;assert(b->ops.poll(b,&guarded.event)<0);tail[62]='x';source.tool_name=(pg_slice){0,1};assert(b->ops.poll(b,&guarded.event)<0);
 source=(pg_event){.kind=PG_TEXT_DELTA,.text={0,1}};assert(b->ops.poll(b,&guarded.event)<0);source=(pg_event){.kind=(pg_event_kind)99};assert(b->ops.poll(b,&guarded.event)<0);source=(pg_event){.kind=PG_RUN_FINISHED,.outcome=(pg_outcome)99};assert(b->ops.poll(b,&guarded.event)<0&&b->active);
 assert(!pg_bridge_probe((probe_fn)b->ops.cancel,b,0,0));assert(cancel_calls==1);
 source=(pg_event){.kind=PG_RUN_FINISHED,.outcome=PG_CANCELLED};assert(b->ops.poll(b,&guarded.event)==1&&!b->active&&guarded.event.run_status==TUI_RUN_CANCELLED);
 source_rc=-7;assert(b->ops.poll(b,&guarded.event)==-7);int before=poll_calls;assert(b->ops.poll(b,0)<0&&poll_calls==before);source_rc=0;assert(b->ops.poll(b,&guarded.event)==0&&!guarded.event.kind);
 submit_rc=1;assert(b->ops.submit(b,"reject",6)==TUI_SUBMIT_ERROR&&!b->active);submit_rc=0;assert(b->ops.submit(b,"run",3)==TUI_SUBMIT_OK);before=poll_calls;
 assert(!pg_bridge_probe((probe_fn)b->ops.shutdown,b,0,0));assert(b->stopped&&b->active&&poll_calls==before+8);b->ops.shutdown(b);assert(poll_calls==before+8);assert(b->ops.poll(b,&guarded.event)==0&&b->ops.submit(b,"end",3)==TUI_SUBMIT_ERROR);
 assert(guarded.before==0x874192&&guarded.after==0x194582);assert(!munmap(ids,(size_t)page*3));assert(!munmap(mapping,(size_t)page*3));
}
static void detach_lifecycle(void){
 for(int result=0;result<2;++result){pg_tui_bridge b;pg_client client={0,pg_bridge_test_submit,pg_bridge_test_cancel,pg_bridge_test_poll};assert(!pg_tui_bridge_init(&b,client));
 assert(pg_tui_bridge_set_detach(0,pg_bridge_test_detach,0));assert(pg_tui_bridge_set_detach(&b,0,0));
 assert(!pg_tui_bridge_set_detach(&b,pg_bridge_test_detach,&detach_calls));detach_result=result;int before=detach_calls,cancels=cancel_calls,polls=poll_calls;
 b.active=1;assert(pg_tui_bridge_set_detach(&b,pg_bridge_test_detach,&detach_calls));assert(!pg_bridge_probe((probe_fn)b.ops.shutdown,&b,0,0));
 assert(b.stopped&&b.active&&b.stop_error==result&&detach_calls==before+1&&cancel_calls==cancels&&poll_calls==polls);b.ops.shutdown(&b);assert(detach_calls==before+1);assert(pg_tui_bridge_set_detach(&b,pg_bridge_test_detach,&detach_calls));}
}
static void history_snapshot(void){
 memset(&agent.session,0,sizeof(agent.session));agent.session.count=4;
 for(unsigned i=0;i<4;++i){pg_message *m=&agent.session.messages[i];m->role=(pg_role)i;m->size=4;memcpy(m->text,"fact",5);}
 strcpy(agent.session.messages[2].call_id,"read-1");strcpy(agent.session.messages[2].tool_name,"read");strcpy(agent.session.messages[3].call_id,"read-1");agent.session.messages[3].is_error=1;
 pg_tui_bridge bridge;pg_client client={0,pg_bridge_test_submit,pg_bridge_test_cancel,pg_bridge_test_poll};assert(!pg_tui_bridge_init(&bridge,client));tui_history_entry out;assert(bridge.ops.history(&bridge,0,&out)==0);
 assert(pg_tui_bridge_set_history(0,&agent.session));assert(pg_tui_bridge_set_history(&bridge,0));assert(!pg_bridge_probe((probe_fn)pg_tui_bridge_set_history,&bridge,&agent.session,0));assert(bridge.history_count==4);
 int submits=submit_calls,polls=poll_calls,cancels=cancel_calls;
 for(unsigned i=0;i<4;++i){assert(!pg_bridge_probe((probe_fn)bridge.ops.history,&bridge,(void*)(uintptr_t)i,(size_t)&out));assert(out.kind==i&&out.text_len==4&&!memcmp(out.text,"fact",4));if(i>=2)assert(out.call_id_len==6&&out.tool_name_len==4&&!memcmp(out.tool_name,"read",4));if(i==3)assert(!out.tool_ok);}
 assert(bridge.ops.history(&bridge,4,&out)==0&&bridge.ops.history(&bridge,0,0)<0);assert(submits==submit_calls&&polls==poll_calls&&cancels==cancel_calls);
 agent.session.count=5;assert(bridge.ops.history(&bridge,0,&out)<0);agent.session.count=65;assert(pg_tui_bridge_set_history(&bridge,&agent.session));agent.session.count=4;assert(bridge.history_count==4);
 agent.session.messages[3].call_id[0]='x';assert(bridge.ops.history(&bridge,3,&out)<0);agent.session.messages[3].call_id[0]='r';agent.session.messages[0].size=4096;assert(bridge.ops.history(&bridge,0,&out)<0);agent.session.messages[0].size=4;
 long page=sysconf(_SC_PAGESIZE);char *map=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page,(size_t)page,PROT_NONE));tui_history_entry *tail=(tui_history_entry*)(map+page-sizeof(*tail));assert(bridge.ops.history(&bridge,3,tail)==1&&!tail->tool_ok&&tail->kind==3);assert(!munmap(map,(size_t)page*2));
 bridge.active=1;assert(pg_tui_bridge_set_history(&bridge,&agent.session));bridge.stopped=1;assert(bridge.ops.history(&bridge,0,&out)<0);
}
int main(void){boundary_abi();detach_lifecycle();history_snapshot();assert(!pg_agent_init(&agent,(pg_model){0,start,next,cancel_model,close_model},(pg_tools){0}));
 pg_tui_bridge bridge;assert(!pg_tui_bridge_init(&bridge,pg_agent_client(&agent)));tui_runtime_ops *ops=&bridge.ops;tui_event e;unsigned starts=0,texts=0,ends=0;
 assert(ops->submit(ops->ctx,"test",4)==TUI_SUBMIT_OK);assert(ops->submit(ops->ctx,"busy",4)==TUI_SUBMIT_BUSY);
 for(unsigned i=0;i<16;++i)if(ops->poll(ops->ctx,&e)){if(e.kind==TUI_EV_RUN_STARTED)++starts;if(e.kind==TUI_EV_TEXT_DELTA){++texts;assert(e.text_len==2&&!memcmp(e.text,"ok",2));}if(e.kind==TUI_EV_RUN_FINISHED){++ends;assert(e.run_status==TUI_RUN_SUCCESS);break;}}
 assert(starts==1&&texts==1&&ends==1&&!bridge.active&&closed==1);
 assert(ops->cancel(ops->ctx)==0);assert(ops->submit(ops->ctx,"new",3)==TUI_SUBMIT_OK);assert(ops->poll(ops->ctx,&e)==1);assert(ops->poll(ops->ctx,&e)==0);
 ops->shutdown(ops->ctx);ops->shutdown(ops->ctx);assert(cancelled==1&&closed==2&&!bridge.active&&bridge.stopped);assert(ops->submit(ops->ctx,"after",5)==TUI_SUBMIT_ERROR);
 assert(!pg_agent_init(&agent,(pg_model){0,start,next,cancel_model,close_model},(pg_tools){0}));
 pg_client raw=pg_agent_client(&agent);assert(!raw.submit(raw.context,(pg_slice){"pending",7}));assert(raw.poll(raw.context,(pg_event[1]){{0}}));assert(!raw.poll(raw.context,(pg_event[1]){{0}}));assert(!pg_agent_suspend(&agent));assert(!pg_agent_resume(&agent,0));
 assert(!pg_tui_bridge_init(&bridge,raw));assert(!bridge.active);assert(ops->poll(ops->ctx,&e)==1&&e.kind==TUI_EV_RUN_STARTED&&bridge.active);assert(ops->submit(ops->ctx,"busy",4)==TUI_SUBMIT_BUSY);assert(ops->cancel(ops->ctx)==1);
 ends=0;for(unsigned i=0;i<8;++i)if(ops->poll(ops->ctx,&e)&&e.kind==TUI_EV_RUN_FINISHED){assert(e.run_status==TUI_RUN_CANCELLED);++ends;break;}assert(ends==1&&!bridge.active);ops->shutdown(ops->ctx);
 assert(!pg_agent_init(&agent,(pg_model){0,start,next,cancel_model,close_model},(pg_tools){0}));assert(!pg_tui_bridge_init(&bridge,pg_agent_client(&agent)));assert(!pg_tui_bridge_set_detach(&bridge,pg_agent_suspend,&agent));
 assert(ops->submit(ops->ctx,"suspend",7)==TUI_SUBMIT_OK);assert(ops->poll(ops->ctx,&e)==1);assert(!ops->poll(ops->ctx,&e));int before_cancel=cancelled,before_close=closed;uint64_t before_sequence=agent.commits.sequence;
 ops->shutdown(ops->ctx);assert(bridge.stopped&&!bridge.stop_error&&cancelled==before_cancel&&closed==before_close+1&&agent.commits.run.phase==PG_RUN_MODEL&&agent.commits.sequence==before_sequence);assert(!pg_agent_resume(&agent,0));assert(!pg_tui_bridge_init(&bridge,pg_agent_client(&agent)));assert(ops->poll(ops->ctx,&e)==1&&e.kind==TUI_EV_RUN_STARTED);ops->shutdown(ops->ctx);
 puts("PASS: native Runtime/TUI bridge mapping, detach preserves work, explicit cancel and idempotent lifecycle");return 0;}
