/* Installed C-only SDK consumer: shared discovery/result protocol over HTTP. */
#include <pangu/services/mcp.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define S(x) ((pangu_slice){x,sizeof(x)-1})
#define CHECK(x) do{if(!(x)){fprintf(stderr,"installed HTTP check %d\n",__LINE__);goto fail;}}while(0)
int main(int argc,char **argv){
    pg_host *host=NULL;pg_scope *scope=NULL;pg_lease *lease=NULL;pangu_core *core=NULL;
    pangu_tool_outcome outcome={0};CHECK(argc==2);
    CHECK(pg_host_create(&host)==PG_OK && pangu_core_create(NULL,&core)==PANGU_OK);
    const pg_mcp_http_header headers[]={
        {PG_NAME("X-PanGu-Fixture"),PG_NAME("controlled")},
        {PG_NAME("Authorization"),PG_NAME("Basic Zml4dHVyZTpwYXNz")}};
    pg_mcp_http_config config={.service_name=PG_NAME("installed.http"),.tool_prefix=PG_NAME("sdk_"),
        .url={argv[1],strlen(argv[1])},.validate=pg_schema_validate,.timeout_ms=2000,.close_timeout_ms=500,
        .disable_get_stream=1,.max_message_bytes=16384,.headers=headers,.header_count=2};
    CHECK(pg_plugin_install(pg_host_root(host),&pg_mcp_http_plugin,&config,sizeof(config),&scope)==PG_OK);
    CHECK(pg_service_acquire(pg_host_root(host),PG_NAME("installed.http"),1,&lease)==PG_OK);
    const pg_tools_service *tools=pg_lease_table(lease);CHECK(tools && tools->count==8);
    const pangu_tool *echo=NULL;
    for(size_t i=0;i<tools->count;++i)if(tools->tools[i].name.len==8 && !memcmp(tools->tools[i].name.data,"sdk_echo",8))echo=&tools->tools[i];
    CHECK(echo);
    pangu_content call={.kind=PANGU_TOOL_CALL,.name=echo->name,.id=S("installed-http"),.data=S("{\"text\":\"installed HTTP reply\"}")};
    pangu_message assistant={.role=PANGU_ASSISTANT,.content=&call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
    pangu_loop_config loop={.tools=tools->tools,.tool_count=tools->count};
    CHECK(pangu_core_call_tool(core,&assistant,&call,&loop,NULL,NULL,&outcome)==PANGU_OK);
    const pangu_message *result=pangu_core_message_at(outcome.result,0);
    CHECK(result && !result->is_error && result->content_count==1 && result->content[0].data.len==20 &&
        !memcmp(result->content[0].data.data,"installed HTTP reply",20));
    pg_lease_release(lease);lease=NULL;CHECK(pg_scope_dispose(scope)==PG_OK);scope=NULL;
    CHECK(pg_service_acquire(pg_host_root(host),PG_NAME("installed.http"),1,&lease)==PG_NOT_FOUND);
    CHECK(result->content[0].data.len==20 && !memcmp(result->content[0].data.data,"installed HTTP reply",20));
    pangu_core_destroy(outcome.result);pangu_core_destroy(core);pg_host_destroy(host);return 0;
fail:
    if(lease)pg_lease_release(lease);
    pangu_core_destroy(outcome.result);pangu_core_destroy(core);pg_host_destroy(host);return 1;
}
