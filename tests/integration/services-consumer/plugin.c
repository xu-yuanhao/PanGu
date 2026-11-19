/* Compiled against installed headers only: no duplicate host implementation. */
#include <pangu/services/dynamic.h>
#include <pangu/services/tools.h>
#include <stdlib.h>
static const pg_plugin_host_api *host;
static pangu_status execute(void *ctx,pangu_core *owner,const pangu_content *call,
                            pangu_tool_sink sink,void *target) {
    (void)ctx;(void)owner;(void)call;
    pangu_content content={.kind=PANGU_TEXT,.data={"installed dynamic tool",22}};
    const char nested[]="{\"calls\":[{\"id\":\"read-one\",\"name\":\"read\",\"arguments\":{\"path\":\"sdk.txt\"},\"status\":\"ok\"}],\"complete\":true}";
    pangu_message result={.role=PANGU_TOOL_RESULT,.content=&content,.content_count=1,
        .nested_calls_json={nested,sizeof(nested)-1}};
    return sink(target,&result,1);
}
static const pangu_tool tool={.name={"installed_echo",14},.schema_json={"{}",2},.execute=execute};
static const pg_tools_service tools={.tools=&tool,.count=1};
static pg_status apply(pg_scope *scope,const void *config,size_t size) {
    (void)config;(void)size;
    return host->register_service(scope,PG_NAME(PG_TOOLS_SERVICE),1,&tools,0);
}
static const pg_plugin plugin={.abi=PANGU_SERVICES_ABI,.size=sizeof(pg_plugin),
    .name={"installed.native",16},.apply=apply};
static const pg_plugin *factory(const pg_plugin_host_api *api) {
#ifdef PANGU_TEST_LEGACY_ABI
    (void)api;abort(); /* ABI rejection must happen before this old factory. */
#else
    host=api;return &plugin;
#endif
}
PG_PLUGIN_EXPORT const pg_dynamic_descriptor pangu_plugin_descriptor_v1={
#ifdef PANGU_TEST_LEGACY_ABI
    .abi=2,
#else
    .abi=PG_DYNAMIC_ABI,
#endif
    .size=sizeof(pg_dynamic_descriptor),.factory=factory};
