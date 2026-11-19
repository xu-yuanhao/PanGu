/* Test module uses only installed ABI and injected host; no host/Core linkage. */
#include "native.h"
#include <pangu/services/dynamic.h>
#include <pangu/services/model.h>
#include <pangu/services/tools.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define S(x) ((pangu_slice){x, sizeof(x)-1})
static const pg_plugin_host_api *host;
typedef struct {
  agent_native_config config;
  unsigned requests;
  pangu_tool tool;
  pg_tools_service tools;
} instance;
static void cleanup(void *ctx) {
  instance *i = ctx;
  if (i->config.cleaned) ++*i->config.cleaned;
  puts("native cleanup");
  free(i);
}
/* Fixed scripted fixture input only, not a production Schema implementation. */
static pangu_status validate(void *ctx, pangu_slice schema, pangu_slice args) {
  (void)ctx; (void)schema;
  return args.len == 2 && !memcmp(args.data,"{}",2) ? PANGU_OK : PANGU_INVALID;
}
static pangu_status execute(void *ctx,pangu_core *core,const pangu_content *call,
                            pangu_tool_sink sink,void *out) {
  (void)ctx; (void)core; (void)call;
  pangu_content c={.kind=PANGU_TEXT,.data=S("native executed")};
  pangu_message m={.role=PANGU_TOOL_RESULT,.content=&c,.content_count=1};
  return sink(out,&m,1);
}
static pangu_status request(void *ctx,pangu_core *owner,const pangu_core *input,
                            pangu_model_event_sink sink,void *out) {
  (void)owner; (void)input;
  instance *i=ctx;
  if (i->config.on_request) i->config.on_request(i->config.ctx);
  int first=i->requests++==0;
  pangu_content c=first
      ? (pangu_content){.kind=PANGU_TOOL_CALL,.id=S("native-call"),
                         .name=S("native_echo"),.data=S("{}")}
      : (pangu_content){.kind=PANGU_TEXT,.data=S("native completed")};
  pangu_message m={.role=PANGU_ASSISTANT,.content=&c,.content_count=1,
                   .stop_reason=first?PANGU_TOOL_USE:PANGU_STOP};
  pangu_assistant_event e={.kind=PANGU_ASSISTANT_START};
  pangu_status st=sink(out,&m,&e);
  e.kind=PANGU_ASSISTANT_DONE; e.reason=m.stop_reason;
  return st==PANGU_OK?sink(out,&m,&e):st;
}
static pg_status apply(pg_scope *scope,const void *config,size_t size) {
  if (size && (!config || size!=sizeof(agent_native_config))) return PG_INVALID;
  instance *i=calloc(1,sizeof(*i));
  if (!i) return PG_NOMEM;
  if (size) i->config=*(const agent_native_config *)config;
  pg_status st=host->defer(scope,cleanup,i);
  if (st) {free(i); return st;}
  i->tool=(pangu_tool){.name=S("native_echo"),.schema_json=S("{}"),
                        .validate=validate,.execute=execute};
  i->tools=(pg_tools_service){&i->tool,1};
  st=host->register_service(scope,PG_NAME(PG_TOOLS_SERVICE),1,&i->tools,i);
  if (st) return st;
  static const pg_model_service model={request};
  st=host->register_service(scope,PG_NAME(PG_MODEL_SERVICE),1,&model,i);
  return st?st:i->config.fail?PG_FAILED:PG_OK;
}
static const pg_plugin plugin={.abi=PANGU_SERVICES_ABI,.size=sizeof(pg_plugin),
                               .name={"agent.native.test",17},.apply=apply};
static const pg_plugin *factory(const pg_plugin_host_api *api) {host=api;return &plugin;}
PG_PLUGIN_EXPORT const pg_dynamic_descriptor pangu_plugin_descriptor_v1={
    .abi=PG_DYNAMIC_ABI,.size=sizeof(pg_dynamic_descriptor),.factory=factory};
