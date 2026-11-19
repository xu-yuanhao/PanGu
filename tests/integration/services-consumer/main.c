#include <pangu/services/host.h>
#include <pangu/services/schema.h>
#include <pangu/services/model.h>
#include <pangu/services/dynamic.h>
#include <pangu/services/tools.h>
#ifdef PANGU_CONSUMER_MCP
#include <pangu/services/mcp.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef PANGU_CONSUMER_RESOURCES
#include <pangu/services/resources.h>
#endif
#define S(x) ((pangu_slice){x,sizeof(x)-1})
typedef struct { pangu_validate_fn validate; } validators;
static const validators table={pg_schema_validate};
static pg_status install(pg_scope *scope,const void *config,size_t len) {
    (void)config; (void)len;
    return pg_service_register(scope,PG_NAME("consumer.schema"),1,&table,0);
}
int main(int argc,char **argv) {
    if (argc!=3 && argc!=5) return 30;
    pg_host *host=0; pg_scope *scope=0; pg_lease *lease=0;
    pg_plugin plugin={.abi=PANGU_SERVICES_ABI,.size=sizeof(pg_plugin),.name=PG_NAME("consumer"),.apply=install};
    if (pg_host_create(&host)!=PG_OK) return 1;
    pg_dynamic_diagnostic diagnostic={0};pg_scope *native=0;
    if (pg_dynamic_plugin_load(pg_host_root(host),(pg_name){0},
        (pg_name){argv[2],strlen(argv[2])},0,0,&native,&diagnostic)!=PG_ABI || native ||
        diagnostic.stage!=PG_DYNAMIC_DESCRIPTOR) return 50;
    if (pg_dynamic_plugin_load(pg_host_root(host),(pg_name){0},
        (pg_name){argv[1],strlen(argv[1])},0,0,&native,&diagnostic)!=PG_OK) return 31;
    if (pg_service_acquire(pg_host_root(host),PG_NAME(PG_TOOLS_SERVICE),1,&lease)!=PG_OK) return 32;
    const pg_tools_service *native_tools=pg_lease_table(lease);
    if (!native_tools || native_tools->count!=1) return 33;
    pangu_core *core=0;if (pangu_core_create(0,&core)!=PANGU_OK) return 34;
    pangu_content call={.kind=PANGU_TOOL_CALL,.id=S("sdk-call"),.name=S("installed_echo"),.data=S("{}")};
    pangu_message assistant={.role=PANGU_ASSISTANT,.content=&call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
    pangu_tool native_tool=native_tools->tools[0];native_tool.validate=pg_schema_validate;
    pangu_loop_config config={.tools=&native_tool,.tool_count=1};
    pangu_tool_outcome outcome={0};
    if (pangu_core_call_tool(core,&assistant,&call,&config,0,0,&outcome)!=PANGU_OK) return 35;
    const pangu_message *reply=pangu_core_message_at(outcome.result,0);
    if (!reply || reply->is_error || reply->content_count!=1 || reply->content[0].data.len!=22 ||
        memcmp(reply->content[0].data.data,"installed dynamic tool",22)) return 36;
    pg_lease_release(lease);
    if (pg_scope_dispose(native)!=PG_OK) return 37;
    if (pg_service_acquire(pg_host_root(host),PG_NAME(PG_TOOLS_SERVICE),1,&lease)!=PG_NOT_FOUND) return 38;
    /* The result owns copies even after its producer's library is unmapped. */
    reply=pangu_core_message_at(outcome.result,0);
    if (reply->content[0].data.len!=22 || memcmp(reply->content[0].data.data,"installed dynamic tool",22)) return 39;
    if (pg_schema_validate(NULL,S("{\"type\":\"object\",\"required\":[\"calls\",\"complete\"],\"properties\":{\"complete\":{\"const\":true},\"calls\":{\"type\":\"array\",\"minItems\":1}}}"),
                           reply->nested_calls_json)!=PANGU_OK) return 49;
    pangu_core_destroy(outcome.result);pangu_core_destroy(core);
#ifdef PANGU_CONSUMER_MCP
    if (argc!=5) return 40;
    const char *command[]={argv[3],argv[4],NULL};
    pg_mcp_stdio_config mcp={.service_name=PG_NAME("installed.mcp.tools"),.tool_prefix=PG_NAME("sdk_"),
        .argv=command,.validate=pg_schema_validate,.timeout_ms=2000,.close_timeout_ms=500};
    pg_scope *remote=NULL;
    if (pg_plugin_install(pg_host_root(host),&pg_mcp_stdio_plugin,&mcp,sizeof(mcp),&remote)!=PG_OK) return 41;
    if (pg_service_acquire(pg_host_root(host),mcp.service_name,1,&lease)!=PG_OK) return 42;
    const pg_tools_service *remote_tools=pg_lease_table(lease);
    if (!remote_tools || remote_tools->count!=1) return 43;
    if (pangu_core_create(NULL,&core)!=PANGU_OK) return 44;
    call=(pangu_content){.kind=PANGU_TOOL_CALL,.id=S("mcp-call"),.name=S("sdk_echo"),.data=S("{\"text\":\"SDK\"}")};
    assistant=(pangu_message){.role=PANGU_ASSISTANT,.content=&call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
    config=(pangu_loop_config){.tools=remote_tools->tools,.tool_count=remote_tools->count};
    outcome=(pangu_tool_outcome){0};
    if (pangu_core_call_tool(core,&assistant,&call,&config,NULL,NULL,&outcome)!=PANGU_OK) return 45;
    pg_lease_release(lease);if (pg_scope_dispose(remote)!=PG_OK) return 46;
    reply=pangu_core_message_at(outcome.result,0);
    if (!reply || reply->is_error || reply->content_count!=2 || reply->content[0].data.len!=19 ||
        memcmp(reply->content[0].data.data,"installed MCP reply",19) || reply->content[1].kind!=PANGU_IMAGE ||
        reply->content[1].data.len!=4 || memcmp(reply->content[1].data.data,"AAAA",4) ||
        pg_schema_validate(NULL,S("{\"type\":\"object\",\"properties\":{\"sdk\":{\"const\":true}},\"required\":[\"sdk\"],\"additionalProperties\":false}"),
                           reply->structured_json)!=PANGU_OK) return 47;
    if (pg_service_acquire(pg_host_root(host),mcp.service_name,1,&lease)!=PG_NOT_FOUND) return 48;
    pangu_core_destroy(outcome.result);pangu_core_destroy(core);
#endif
    if (pg_plugin_install(pg_host_root(host),&plugin,0,0,&scope)!=PG_OK) return 2;
    if (pg_service_acquire(scope,PG_NAME("consumer.schema"),1,&lease)!=PG_OK) return 3;
    const validators *service=pg_lease_table(lease);
    pangu_slice schema=S("{\"type\":\"integer\",\"minimum\":2}");
    if (service->validate(0,schema,S("3"))!=PANGU_OK) return 4;
    if (service->validate(0,schema,S("1"))==PANGU_OK) return 5;
    pg_lease_release(lease);
    if (pg_scope_dispose(scope)!=PG_OK) return 6;
    pg_deepseek_config model={0}; pg_scope *model_scope=0;
    if (pg_plugin_install(pg_host_root(host),&pg_deepseek_plugin,&model,sizeof(model),&model_scope)!=PG_OK) return 7;
    if (pg_service_acquire(pg_host_root(host),PG_NAME(PG_MODEL_SERVICE),1,&lease)!=PG_OK) return 8;
    const pg_model_service *provider=pg_lease_table(lease);
    if (!provider || !provider->request_events) return 9;
    pg_lease_release(lease);
    if (pg_scope_dispose(model_scope)!=PG_OK) return 10;
#ifdef PANGU_CONSUMER_RESOURCES
    const char *directory=getenv("PANGU_CONSUMER_RESOURCES");
    if (!directory) return 11;
    char user[4096],project[4096];
    if (snprintf(user,sizeof(user),"%s/user",directory)>=(int)sizeof(user) ||
        snprintf(project,sizeof(project),"%s/project",directory)>=(int)sizeof(project)) return 12;
    pg_resources_config resources={.user_directory=user,.cwd=project};
    pg_scope *resource_scope=0;
    if (pg_plugin_install(pg_host_root(host),&pg_resources_plugin,&resources,sizeof(resources),&resource_scope)!=PG_OK) return 13;
    if (pg_service_acquire(pg_host_root(host),PG_NAME(PG_RESOURCES_SERVICE),1,&lease)!=PG_OK) return 14;
    const pg_resources_service *api=pg_lease_table(lease);
    pg_resource_snapshot *snapshot=0;
    if (api->snapshot(pg_lease_context(lease),&snapshot)!=PG_OK) return 15;
    char *expanded=0; size_t length=0;
    if (pg_resource_expand(snapshot,S("/hello SDK"),&expanded,&length)!=PG_OK) return 16;
    int matches=length==9 && !memcmp(expanded,"Hello SDK",9);
    free(expanded);
    pg_lease_release(lease);
    if (pg_service_acquire(pg_host_root(host),PG_NAME(PG_CREDENTIALS_SERVICE),1,&lease)!=PG_OK) return 21;
    const pg_credentials_service *credentials=pg_lease_table(lease);
    void *credential_context=pg_lease_context(lease);
    const pangu_slice expected[]={S("synthetic-installed-one"),S("synthetic-installed-two")};
    for (size_t i=0;i<2;++i) {
        if (credentials->put_api_key(credential_context,S("installed-consumer"),expected[i])!=PG_OK) return 22;
        char *key=NULL; size_t key_length=0;
        if (credentials->get_api_key(credential_context,S("installed-consumer"),&key,&key_length)!=PG_OK) return 23;
        int equal=key_length==expected[i].len && !memcmp(key,expected[i].data,key_length);
        volatile unsigned char *wipe=(volatile unsigned char *)key;
        for (size_t j=0;j<key_length;++j) wipe[j]=0;
        free(key);
        if (!equal) return 24;
    }
    if (credentials->put_api_key(credential_context,S("installed-consumer"),(pangu_slice){0})!=PG_OK) return 25;
    pg_lease_release(lease);
    if (pg_scope_dispose(resource_scope)!=PG_OK) return 17;
    /* Immutable snapshots remain valid after scope unload. */
    if (!matches || !pg_resource_settings(snapshot).len || !pg_resource_count(snapshot)) return 18;
    int kept=0;
    for (size_t i=0;i<pg_resource_count(snapshot);++i) {
        const pg_resource *entry=pg_resource_at(snapshot,i);
        if (entry->name.len==7 && !memcmp(entry->name.data,"ignored",7)) return 19;
        if (entry->kind==PG_RESOURCE_SKILL && entry->name.len==4 && !memcmp(entry->name.data,"kept",4)) ++kept;
    }
    if (kept!=1) return 20;
    pg_resource_snapshot_release(snapshot);
#endif
    pg_host_destroy(host); return 0;
}
