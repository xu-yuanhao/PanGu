#include <pangu/services/dynamic.h>
#include <pangu/services/model.h>
#include <pangu/services/tools.h>
#include <string.h>
static const pg_plugin_host_api *host;
static pg_scope *installed_scope;
static int requests;
static pangu_status request(void *ctx,pangu_core *owner,const pangu_core *input,
                            pangu_model_event_sink sink,void *target) {
    (void)ctx;(void)owner;(void)input;
    pangu_content content={.kind=PANGU_TEXT,.data={"installed native reply",22}};
    const char handle[]="{\"provider\":\"fixture\",\"api\":\"mock\",\"modelId\":\"native\",\"id\":\"installed-handle\",\"data\":{\"zero\":0}}";
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&content,.content_count=1,.stop_reason=PANGU_DEFERRED,
        .diagnostics_json={"[]",2},.deferred_json={handle,sizeof(handle)-1},.has_end_turn=1};
    pangu_assistant_event event={.kind=PANGU_ASSISTANT_DONE,.reason=PANGU_DEFERRED};
    char name[256];pg_lease *lease=0;
    if (requests++==0 && host->acquire(installed_scope,PG_NAME(PG_TOOLS_SERVICE),1,&lease)==PG_OK) {
        const pg_tools_service *tools=host->table(lease);
        if (tools && tools->count && tools->tools[0].name.len<sizeof(name)) {
            size_t len=tools->tools[0].name.len;memcpy(name,tools->tools[0].name.data,len);
            content=(pangu_content){.kind=PANGU_TOOL_CALL,.name={name,len},.id={"installed-mcp",13},
                .data={"{\"text\":\"SDK\"}",14}};
            reply.stop_reason=PANGU_TOOL_USE;event.reason=PANGU_TOOL_USE;
            reply.deferred_json=(pangu_slice){0};
        }
        host->release(lease);
    }
    return sink(target,&reply,&event);
}
static const pg_model_service model={request};
static pg_status apply(pg_scope *scope,const void *config,size_t size) {
    (void)config;(void)size;
    installed_scope=scope;requests=0;
    return host->register_service(scope,PG_NAME(PG_MODEL_SERVICE),1,&model,0);
}
static const pg_plugin plugin={.abi=PANGU_SERVICES_ABI,.size=sizeof(pg_plugin),
    .name={"installed.model.native",22},.apply=apply};
static const pg_plugin *factory(const pg_plugin_host_api *api) {host=api;return &plugin;}
PG_PLUGIN_EXPORT const pg_dynamic_descriptor pangu_plugin_descriptor_v1={
    .abi=PG_DYNAMIC_ABI,.size=sizeof(pg_dynamic_descriptor),.factory=factory};
