/* Installed public C app -> model -> HTTP tools -> durable reopen. */
#include <pangu/agent/app.h>
#include <pangu/services/mcp.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define S(x) ((pangu_slice){x,sizeof(x)-1})
#define CHECK(x) do{if(!(x)){fprintf(stderr,"installed app HTTP check %d\n",__LINE__);goto fail;}}while(0)
static int same(pangu_slice value,const char *text){return value.len==strlen(text) && !memcmp(value.data,text,value.len);}
static pangu_status model(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target){
    int *requests=ctx;
    pangu_content text={.kind=PANGU_TEXT,.data=S("HTTP complete")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_STOP};
    if((*requests)++==0){
        if(pangu_core_active_tool_count(owner)!=8)return PANGU_MODEL_ERROR;
        text=(pangu_content){.kind=PANGU_TOOL_CALL,.name=S("sdk_echo"),.id=S("installed-http"),.data=S("{\"text\":\"installed HTTP reply\"}")};
        reply.stop_reason=PANGU_TOOL_USE;
    }else{
        const pangu_message *result=NULL;
        for(size_t i=pangu_core_message_count(view);i>0;--i){
            const pangu_message *m=pangu_core_message_at(view,i-1);
            if(m->role==PANGU_TOOL_RESULT){result=m;break;}
        }
        if(!result || result->is_error || result->content_count!=1 || !same(result->content[0].data,"installed HTTP reply"))return PANGU_MODEL_ERROR;
    }
    return sink(target,&reply,1);
}
static int saved(pg_agent_app *app){
    const pangu_core *view=pg_session_messages(pg_agent_app_session(app));
    int tools=0,finals=0,users=0;
    for(size_t i=0;i<pangu_core_message_count(view);++i){
        const pangu_message *m=pangu_core_message_at(view,i);
        if(m->role==PANGU_TOOL_RESULT){
            if(m->is_error || m->content_count!=1 || !same(m->content[0].data,"installed HTTP reply"))return 0;
            ++tools;
        }
        if(m->role==PANGU_USER)++users;
        if(m->role==PANGU_ASSISTANT && m->stop_reason==PANGU_STOP && m->content_count==1 && same(m->content[0].data,"HTTP complete"))++finals;
    }
    return tools==1 && users==1 && finals==1;
}
int main(int argc,char **argv){
    pg_agent_app *app=NULL;CHECK(argc==2);
    const char *backend=getenv("PANGU_HTTP_STORE");int sqlite=backend && !strcmp(backend,"sqlite");
    const char *path=sqlite?"installed-http.db":"installed-http.jsonl";remove(path);
    const pg_mcp_http_header headers[]={
        {PG_NAME("X-PanGu-Fixture"),PG_NAME("controlled")},
        {PG_NAME("Authorization"),PG_NAME("Basic Zml4dHVyZTpwYXNz")}};
    pg_mcp_http_config cfg={.service_name=PG_NAME(PG_TOOLS_SERVICE),.tool_prefix=PG_NAME("sdk_"),
        .url={argv[1],strlen(argv[1])},.headers=headers,.header_count=2,.validate=pg_schema_validate,
        .timeout_ms=2000,.close_timeout_ms=500,.disable_get_stream=1,.max_message_bytes=16384};
    pg_app_plugin plugin={&pg_mcp_http_plugin,&cfg,sizeof(cfg)};
    pg_session_options options={.path={path,strlen(path)},.backend=sqlite?PG_STORE_SQLITE:PG_STORE_JSONL,.create=1};
    CHECK(pg_agent_app_open(&options,&plugin,1,&app)==PG_OK);
    int requests=0;pangu_loop_config loop={.ctx=&requests,.request=model};
    pangu_content text={.kind=PANGU_TEXT,.data=S("HTTP app input")};
    pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
    CHECK(pg_agent_app_run(app,&input,1,&loop)==PG_SESSION_OK && requests==2 && saved(app));
    uint64_t sequence=pg_session_sequence(pg_agent_app_session(app));
    pg_agent_app_close(app);app=NULL;options.create=0;
    /* Stored conversation remains usable without reconnecting the server. */
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK && saved(app));
    CHECK(pg_session_sequence(pg_agent_app_session(app))==sequence);
    pg_agent_app_close(app);remove(path);return 0;
fail:
    pg_agent_app_close(app);return 1;
}
