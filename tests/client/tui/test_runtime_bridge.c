#include "../../../src/client/tui/runtime_bridge.h"
#include "../../../src/runtime/agent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static pg_agent agent;static int closed,cancelled,step;
static int start(void *c,const pg_session *s){(void)c;(void)s;return 0;}
static int next(void *c,pg_model_event *e){(void)c;*e=(pg_model_event){.kind=step++?PG_MODEL_DONE:PG_MODEL_TEXT,.text={"ok",2}};return 0;}
static void cancel_model(void *c){(void)c;++cancelled;}
static void close_model(void *c){(void)c;++closed;}
int main(void){assert(!pg_agent_init(&agent,(pg_model){0,start,next,cancel_model,close_model},(pg_tools){0}));
 pg_tui_bridge bridge;assert(!pg_tui_bridge_init(&bridge,pg_agent_client(&agent)));tui_runtime_ops *ops=&bridge.ops;tui_event e;unsigned starts=0,texts=0,ends=0;
 assert(ops->submit(ops->ctx,"test",4)==TUI_SUBMIT_OK);assert(ops->submit(ops->ctx,"busy",4)==TUI_SUBMIT_BUSY);
 for(unsigned i=0;i<16;++i)if(ops->poll(ops->ctx,&e)){if(e.kind==TUI_EV_RUN_STARTED)++starts;if(e.kind==TUI_EV_TEXT_DELTA){++texts;assert(e.text_len==2&&!memcmp(e.text,"ok",2));}if(e.kind==TUI_EV_RUN_FINISHED){++ends;assert(e.run_status==TUI_RUN_SUCCESS);break;}}
 assert(starts==1&&texts==1&&ends==1&&!bridge.active&&closed==1);
 assert(ops->cancel(ops->ctx)==0);assert(ops->submit(ops->ctx,"new",3)==TUI_SUBMIT_OK);assert(ops->poll(ops->ctx,&e)==1);assert(ops->poll(ops->ctx,&e)==0);
 ops->shutdown(ops->ctx);ops->shutdown(ops->ctx);assert(cancelled==1&&closed==2&&!bridge.active&&bridge.stopped);assert(ops->submit(ops->ctx,"after",5)==TUI_SUBMIT_ERROR);
 puts("PASS: native Runtime/TUI bridge event mapping, busy state, terminal and idempotent shutdown cancellation");return 0;}
