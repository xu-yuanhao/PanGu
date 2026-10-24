#include "runtime_bridge.h"
#include <string.h>
static tui_submit_result submit(void *ctx,const char *text,size_t size){
 pg_tui_bridge *b=ctx;if(b->stopped)return TUI_SUBMIT_ERROR;if(b->active)return TUI_SUBMIT_BUSY;
 if(b->client.submit(b->client.context,(pg_slice){text,size}))return TUI_SUBMIT_ERROR;
 b->active=1;return TUI_SUBMIT_OK;
}
static int cancel(void *ctx){pg_tui_bridge *b=ctx;if(!b->active||b->stopped)return 0;b->client.cancel(b->client.context);return 1;}
static int field(char *out,pg_slice value){if(!value.data||!value.size||value.size>=64||memchr(value.data,0,value.size))return 1;memcpy(out,value.data,value.size);out[value.size]=0;return 0;}
static tui_run_status status(pg_outcome value){switch(value){case PG_SUCCESS:return TUI_RUN_SUCCESS;case PG_ERROR:return TUI_RUN_ERROR;case PG_CANCELLED:return TUI_RUN_CANCELLED;}return 0;}
static int poll_event(void *ctx,tui_event *out){
 pg_tui_bridge *b=ctx;pg_event event={0};if(!out)return -1;*out=(tui_event){0};if(b->stopped)return 0;
 int rc=b->client.poll(b->client.context,&event);if(rc<=0)return rc;
 switch(event.kind){
 case PG_RUN_STARTED:out->kind=TUI_EV_RUN_STARTED;break;
 case PG_TEXT_DELTA:out->kind=TUI_EV_TEXT_DELTA;out->text=event.text.data;out->text_len=event.text.size;break;
 case PG_TOOL_STARTED:case PG_TOOL_FINISHED:
  if(field(b->id,event.call_id)||field(b->name,event.tool_name))return -1;
  out->kind=event.kind==PG_TOOL_STARTED?TUI_EV_TOOL_STARTED:TUI_EV_TOOL_FINISHED;
  out->tool_call_id=b->id;out->tool_name=b->name;out->tool_ok=event.outcome==PG_SUCCESS;
  out->detail=event.text.data;out->detail_len=event.text.size;break;
 case PG_RUN_FINISHED:
  out->kind=TUI_EV_RUN_FINISHED;out->run_status=status(event.outcome);if(!out->run_status)return -1;
  out->detail=event.text.data;out->detail_len=event.text.size;b->active=0;break;
 default:return -1;
 }return 1;
}
static void shutdown_bridge(void *ctx){
 pg_tui_bridge *b=ctx;if(b->stopped)return;
 if(b->active){b->client.cancel(b->client.context);
  /* The bounded local Agent acknowledges cancellation in at most three polls.
     Do not fabricate a terminal; drain only to release resources on client exit. */
  for(unsigned i=0;i<8 && b->active;++i){tui_event event;if(poll_event(b,&event)<0)break;}
 }
 b->stopped=1;
}
int pg_tui_bridge_init(pg_tui_bridge *b,pg_client client){
 if(!b||!client.submit||!client.cancel||!client.poll)return 1;
 *b=(pg_tui_bridge){0};b->client=client;b->ops=(tui_runtime_ops){"native-agent / MiMo",b,submit,cancel,poll_event,shutdown_bridge};return 0;
}
