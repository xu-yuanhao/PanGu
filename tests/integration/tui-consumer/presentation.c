#include <pangu/agent/app.h>
#include <pangu/tui/plugin.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
static pangu_status model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    (void)ctx; (void)owner;
    const char reply[]="installed response";
    pangu_content text={.kind=PANGU_TEXT,.data={reply,sizeof(reply)-1}};
    pangu_message message={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_STOP};
    return sink(target,&message,1);
}
static pg_status direct_present(pg_agent_app *app,const void *config,size_t size) {
    pg_scope *scope=NULL; pg_lease *lease=NULL;
    pg_status status=pg_plugin_install(pg_agent_app_scope(app),pg_tui_plugin(),config,size,&scope);
    if (status==PG_OK) status=pg_service_acquire(pg_agent_app_scope(app),PG_NAME(PG_TUI_SERVICE),PG_TUI_ABI,&lease);
    if (status==PG_OK) {
        const pg_tui_service *api=pg_lease_table(lease);
        void *ctx=pg_lease_context(lease);
        if (api->start(ctx)!=PANGU_TUI_OK) status=PG_FAILED;
        else if (api->run(ctx)!=PANGU_TUI_OK) status=PG_FAILED;
        api->stop(ctx);
    }
    if (lease) pg_lease_release(lease);
    if (scope) pg_scope_dispose(scope);
    return status;
}
int main(int argc,char **argv) {
    if (argc!=2 && argc!=3) return 1;
    remove(argv[1]); /* Dedicated test fixture path. */
    pg_session_options options={.path={argv[1],strlen(argv[1])},.create=1,.backend=PG_STORE_JSONL};
    pg_agent_app *app=NULL;
    if (pg_agent_app_open(&options,NULL,0,&app)!=PG_OK) return 2;
    pg_tui_config config={.loop={.model=model}};
    size_t size=argc==3?offsetof(pg_tui_config,actions):sizeof(config);
    void *legacy=NULL;
    if (argc==3) {
        /* Allocate exactly the historical prefix so ASan catches tail reads. */
        legacy=malloc(size);
        if (!legacy) { pg_agent_app_close(app); return 7; }
        memcpy(legacy,&config,size);
    }
    const void *input=legacy?legacy:&config;
    pg_status status=argc==3 && !strcmp(argv[2],"direct")?direct_present(app,input,size):
        pg_agent_app_present(app,pg_tui_plugin(),input,size);
    free(legacy);
    size_t count=pangu_core_message_count(pg_session_messages(pg_agent_app_session(app)));
    pg_agent_app_close(app);
    if (status!=PG_OK || count!=2) return 3;
    options.create=0;
    if (pg_agent_app_open(&options,NULL,0,&app)!=PG_OK) return 4;
    count=pangu_core_message_count(pg_session_messages(pg_agent_app_session(app)));
    pg_agent_app_close(app);
    if (count!=2) return 5;
    return remove(argv[1])?6:0;
}
