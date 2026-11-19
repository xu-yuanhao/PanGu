#include <pangu/agent/app.h>
#include <pangu/services/model.h>
#include <stdio.h>
#include <string.h>
#define S(x) ((pangu_slice){x,sizeof(x)-1})
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"installed agent check: %d\n",__LINE__); goto failed; } } while (0)
static pangu_status request(void *ctx,pangu_core *owner,const pangu_core *input,pangu_model_event_sink sink,void *target) {
    (void)ctx; (void)owner;
    if (!pangu_core_message_count(input)) return PANGU_INVALID;
    pangu_content text={.kind=PANGU_TEXT,.data=S("installed reply")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_STOP};
    pangu_assistant_event event={.kind=PANGU_ASSISTANT_DONE,.reason=PANGU_STOP};
    return sink(target,&reply,&event);
}
static const pg_model_service model={request};
typedef struct { size_t count; } history;
static pg_session_status original_input(void *ctx,pangu_slice input) {
    history *seen=ctx;
    const char expected[]="/template 原始\nsecond line";
    if(input.len!=sizeof(expected)-1 || memcmp(input.data,expected,input.len)) return PG_SESSION_INVALID;
    ++seen->count;return PG_SESSION_OK;
}
static pg_session_status reject_history(void *ctx,pangu_slice input) {
    (void)input;++((history *)ctx)->count;return PG_SESSION_INVALID;
}
static pg_session_status app_input(void *ctx,pangu_slice input) {
    if(input.len!=16 || memcmp(input.data,"installed prompt",16)) return PG_SESSION_INVALID;
    ++((history *)ctx)->count;return PG_SESSION_OK;
}
static pg_status install(pg_scope *scope,const void *config,size_t size) {
    (void)config; (void)size;
    return pg_service_register(scope,PG_NAME(PG_MODEL_SERVICE),1,&model,NULL);
}
int main(int argc,char **argv) {
    pg_agent_app *app=NULL;
    CHECK(argc==4);
    remove(argv[1]); /* A dedicated consumer-build fixture path. */
    pg_plugin plugin={.abi=PANGU_SERVICES_ABI,.size=sizeof(pg_plugin),.name=PG_NAME("installed.model"),.apply=install};
    pg_app_plugin selection={.plugin=&plugin};
    pg_session_options options={.path={argv[1],strlen(argv[1])},.create=1,
        .backend=!strcmp(argv[2],"sqlite")?PG_STORE_SQLITE:PG_STORE_JSONL};
    CHECK(pg_agent_app_open(&options,&selection,1,&app)==PG_OK);
    pg_dynamic_diagnostic diagnostic={0};
    CHECK(pg_agent_app_load_plugin(app,(pg_name){0},(pg_name){argv[3],strlen(argv[3])},NULL,0,&diagnostic)==PG_OK);
    pangu_content text={.kind=PANGU_TEXT,.data=S("installed prompt")};
    pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
    pangu_loop_config config={0};
    CHECK(pg_agent_app_run(app,&prompt,1,&config)==PG_SESSION_OK);
    pangu_session *session=pg_agent_app_session(app);
    CHECK(pangu_core_message_count(pg_session_messages(session))==2);
    history seen={0};CHECK(pg_session_input_history(session,app_input,&seen)==PG_SESSION_OK && seen.count==1);
    uint64_t sequence=pg_session_sequence(session);
    pg_agent_app_close(app);app=NULL;
    options.create=0;
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    session=pg_agent_app_session(app);
    CHECK(pg_session_sequence(session)==sequence);
    const pangu_core *messages=pg_session_messages(session);
    CHECK(pangu_core_message_count(messages)==2);
    const pangu_message *reply=pangu_core_message_at(messages,1);
    CHECK(reply->role==PANGU_ASSISTANT && reply->content_count==1);
    CHECK(reply->content[0].data.len==22 && !memcmp(reply->content[0].data.data,"installed native reply",22));
    const char handle[]="{\"provider\":\"fixture\",\"api\":\"mock\",\"modelId\":\"native\",\"id\":\"installed-handle\",\"data\":{\"zero\":0}}";
    CHECK(reply->stop_reason==PANGU_DEFERRED && reply->has_end_turn && !reply->end_turn);
    CHECK(reply->diagnostics_json.len==2 && !memcmp(reply->diagnostics_json.data,"[]",2));
    CHECK(reply->deferred_json.len==sizeof(handle)-1 && !memcmp(reply->deferred_json.data,handle,sizeof(handle)-1));
    seen=(history){0};CHECK(pg_session_input_history(session,app_input,&seen)==PG_SESSION_OK && seen.count==1);
    pg_agent_app_close(app);app=NULL;
    CHECK(!remove(argv[1]));
    options.create=1;
    CHECK(pg_agent_app_open(&options,&selection,1,&app)==PG_OK);
    session=pg_agent_app_session(app);
    pangu_content expanded={.kind=PANGU_TEXT,.data=S("expanded model instructions")};
    pangu_content raw={.kind=PANGU_TEXT,.data=S("/template 原始\nsecond line")};
    pangu_message input={.role=PANGU_USER,.content=&expanded,.content_count=1};
    pangu_message original={.role=PANGU_USER,.content=&raw,.content_count=1};
    pangu_loop_config direct={.request_events=request};
    CHECK(pg_session_run_inputs(session,&input,1,&direct,&original)==PG_SESSION_OK);
    seen=(history){0};CHECK(pg_session_input_history(session,original_input,&seen)==PG_SESSION_OK && seen.count==1);
    CHECK(pg_session_enqueue_input(session,PANGU_FOLLOW_UP,&input,&original)==PG_SESSION_OK);
    CHECK(pg_session_queue_count(session,PANGU_FOLLOW_UP)==1);
    seen=(history){0};CHECK(pg_session_input_history(session,original_input,&seen)==PG_SESSION_OK && seen.count==1);
    sequence=pg_session_sequence(session);
    pg_agent_app_close(app);app=NULL;options.create=0;
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    session=pg_agent_app_session(app);
    CHECK(pg_session_sequence(session)==sequence);
    CHECK(pg_session_queue_count(session,PANGU_FOLLOW_UP)==1);
    seen=(history){0};CHECK(pg_session_input_history(session,original_input,&seen)==PG_SESSION_OK && seen.count==1);
    CHECK(pg_session_run(session,NULL,0,&direct)==PG_SESSION_OK);
    CHECK(pg_session_queue_count(session,PANGU_FOLLOW_UP)==0);
    seen=(history){0};CHECK(pg_session_input_history(session,original_input,&seen)==PG_SESSION_OK && seen.count==2);
    uint64_t before_read=pg_session_sequence(session);
    seen=(history){0};CHECK(pg_session_input_history(session,reject_history,&seen)==PG_SESSION_INVALID && seen.count==1);
    CHECK(pg_session_sequence(session)==before_read);
    sequence=pg_session_sequence(session);
    pg_agent_app_close(app);app=NULL;
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    session=pg_agent_app_session(app);CHECK(pg_session_sequence(session)==sequence);
    seen=(history){0};CHECK(pg_session_input_history(session,original_input,&seen)==PG_SESSION_OK && seen.count==2);
    const pangu_message *expanded_saved=pangu_core_message_at(pg_session_messages(session),0);
    CHECK(expanded_saved->content[0].data.len==27 && !memcmp(expanded_saved->content[0].data.data,"expanded model instructions",27));
    /* A direct legacy submission carries no raw provenance. Mixed history
     * must be unavailable atomically, never return a misleading prefix. */
    CHECK(pg_session_run(session,&input,1,&direct)==PG_SESSION_OK);
    seen=(history){0};CHECK(pg_session_input_history(session,original_input,&seen)==PG_SESSION_NOT_FOUND && !seen.count);
    sequence=pg_session_sequence(session);
    pg_agent_app_close(app);app=NULL;
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    session=pg_agent_app_session(app);CHECK(pg_session_sequence(session)==sequence);
    seen=(history){0};CHECK(pg_session_input_history(session,original_input,&seen)==PG_SESSION_NOT_FOUND && !seen.count);
    pg_agent_app_close(app);app=NULL;CHECK(!remove(argv[1]));
    return 0;
failed:
    pg_agent_app_close(app);
    return 1;
}
