/* Cross-component offline protocol fixture using the production Schema bridge. */
#include <pangu/agent/app.h>
#include <pangu/services/model.h>
#include <pangu/services/tools.h>
#include <pangu/services/schema.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"product check at %d\n",__LINE__); abort(); } } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
typedef struct { int cancel,commits,live,trace; } observer;
static void event(void *ctx,pangu_session *session,const pg_session_event *event) {
    observer *o=ctx;
    if (event->kind==PG_SESSION_COMMITTED) ++o->commits;
    if (event->kind!=PG_SESSION_LIVE) return;
    ++o->live;
    const pangu_event *e=event->live;
    if (o->trace && (e->kind==PANGU_MESSAGE_START || e->kind==PANGU_MESSAGE_END))
        printf("state:%d:%d:%zu\n",e->kind,e->message->role,pangu_core_message_count(pg_session_messages(session)));
    if (o->cancel==1 && e->kind==PANGU_MESSAGE_UPDATE && e->assistant_event &&
        e->assistant_event->kind==PANGU_ASSISTANT_TEXT_DELTA) {
        o->cancel=2; pg_session_cancel(session);
    }
}
static pg_session_status prompt(pg_agent_app *app,const char *input,const pangu_loop_config *cfg) {
    pangu_content b={.kind=PANGU_TEXT,.data={input,strlen(input)}};
    pangu_message m={.role=PANGU_USER,.content=&b,.content_count=1};
    return pg_agent_app_run(app,&m,1,cfg);
}
static pangu_status prepare_failure(void *ctx,pangu_core *owner) {
    (void)ctx; (void)owner; return PANGU_MODEL_ERROR;
}
static pangu_status prepare_cancelled(void *ctx,pangu_core *owner) {
    (void)ctx; (void)owner; return PANGU_CANCELLED;
}
static pangu_status context_failure(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *output) {
    (void)input; (void)output; return prepare_failure(ctx,owner);
}
static pangu_status key_failure(void *ctx,pangu_core *owner,pangu_slice provider,pangu_api_key_sink sink,void *target) {
    (void)provider; (void)sink; (void)target; return prepare_failure(ctx,owner);
}
static pangu_status abort_before_stream(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)ctx; (void)owner; (void)request; (void)sink; (void)target;
    /* Transport-origin cancellation does not require a host abort call. */
    return PANGU_CANCELLED;
}
#ifdef PANGU_INTEGRATION_OVERFLOW
static void recovery_roundtrip(const char *endpoint,const char *path,pg_session_backend backend,int repeat) {
    size_t n=strlen(path)+32;char *file=malloc(n),*url=malloc(strlen(endpoint)+32);CHECK(file && url);
    snprintf(file,n,"%s-recovery%s",path,repeat?"-repeat":"");sprintf(url,"%s/recovery%s",endpoint,repeat?"-repeat":"");
    pg_session_options options={.path={file,strlen(file)},.backend=backend,.create=1};
    pg_agent_app *app=NULL;CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    pg_deepseek_config model={.endpoint={url,strlen(url)},.timeout_ms=5000};pg_scope *scope=NULL;
    CHECK(pg_plugin_install(pg_agent_app_scope(app),&pg_deepseek_plugin,&model,sizeof(model),&scope)==PG_OK);
    pangu_loop_config cfg={.api_key=S("redacted-integration"),.model_config={.id=S("deepseek-chat"),.provider=S("deepseek"),.api=S("openai-completions")}};
    CHECK(prompt(app,"seed one",&cfg)==PG_SESSION_OK);
    CHECK(prompt(app,"seed two",&cfg)==PG_SESSION_OK);
    pg_compaction_policy policy={.enabled=1,.context_tokens=1000000,.reserve_tokens=1,.keep_recent_tokens=1};
    pangu_session *session=pg_agent_app_session(app);
    CHECK(pg_session_set_compaction_policy(session,&policy)==PG_SESSION_OK);
    CHECK(prompt(app,"overflow",&cfg)==(repeat?PG_SESSION_CORE_ERROR:PG_SESSION_OK));
    size_t omissions=0,summaries=0,archived_errors=0;
    for(size_t i=0;i<pg_session_entry_count(session);++i){
        pg_session_entry entry;CHECK(pg_session_entry_at(session,i,&entry)==PG_SESSION_OK);
        omissions+=entry.kind==PG_ENTRY_OMISSION;summaries+=entry.kind==PG_ENTRY_COMPACTION;
        if(entry.message && entry.message->role==PANGU_ASSISTANT && entry.message->stop_reason==PANGU_ERROR){
            ++archived_errors;
            const char diagnostic[]="deepseek API error (413): request too large";
            CHECK(entry.message->error.len==sizeof(diagnostic)-1 && !memcmp(entry.message->error.data,diagnostic,sizeof(diagnostic)-1));
        }
    }
    CHECK(omissions==1 && summaries==1 && archived_errors==(size_t)(repeat?2:1));
    const pangu_core *view=pg_session_messages(session);size_t count=pangu_core_message_count(view);
    const pangu_message *last=pangu_core_message_at(view,count-1);
    CHECK(last && last->stop_reason==(repeat?PANGU_ERROR:PANGU_STOP));
    if(!repeat)CHECK(last->content_count==1 && last->content[0].data.len==9 && !memcmp(last->content[0].data.data,"recovered",9));
    uint64_t sequence=pg_session_sequence(session),leaf=pg_session_leaf(session);size_t entries=pg_session_entry_count(session);
    pg_agent_app_close(app);options.create=0;
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);session=pg_agent_app_session(app);
    CHECK(pg_session_sequence(session)==sequence && pg_session_leaf(session)==leaf && pg_session_entry_count(session)==entries);
    view=pg_session_messages(session);CHECK(pangu_core_message_count(view)==count);
    last=pangu_core_message_at(view,count-1);CHECK(last->stop_reason==(repeat?PANGU_ERROR:PANGU_STOP));
    if(!repeat)CHECK(last->content[0].data.len==9 && !memcmp(last->content[0].data.data,"recovered",9));
    pg_agent_app_close(app);free(file);free(url);
    puts(repeat?"HTTP413/one-retry-limit/reopen passed":"HTTP413/omission/summary/retry/reopen passed");
}
#endif
static void file_summary_roundtrip(const char *endpoint,const char *path,
                                   const pg_file_tools_config *files,pg_session_backend backend) {
    size_t n=strlen(path)+16;char *file=malloc(n),*url=malloc(strlen(endpoint)+16);CHECK(file && url);
    snprintf(file,n,"%s-files",path);sprintf(url,"%s/files",endpoint);
    pg_session_options options={.path={file,strlen(file)},.backend=backend,.create=1};
    pg_app_plugin plugin={.plugin=&pg_file_tools_plugin,.config=files,.config_size=sizeof(*files)};
    pg_agent_app *app=NULL;CHECK(pg_agent_app_open(&options,&plugin,1,&app)==PG_OK);
    pg_lease *tools=NULL;CHECK(pg_service_acquire(pg_agent_app_scope(app),PG_NAME(PG_TOOLS_SERVICE),1,&tools)==PG_OK);
    const pg_tools_service *table=pg_lease_table(tools);CHECK(table);
    pg_deepseek_config model={.endpoint={url,strlen(url)},.tools=table->tools,.tool_count=table->count,.timeout_ms=5000};
    pg_scope *scope=NULL;CHECK(pg_plugin_install(pg_agent_app_scope(app),&pg_deepseek_plugin,&model,sizeof(model),&scope)==PG_OK);
    pangu_loop_config cfg={.api_key=S("redacted-integration"),.tools=table->tools,.tool_count=table->count,
        .model_config={.id=S("deepseek-chat"),.provider=S("deepseek"),.api=S("openai-completions")}};
    CHECK(prompt(app,"write",&cfg)==PG_SESSION_OK);
    CHECK(prompt(app,"resume",&cfg)==PG_SESSION_OK);
    CHECK(pg_agent_app_compact(app,1,&cfg)==PG_SESSION_OK);
    pangu_session *session=pg_agent_app_session(app);size_t markers=0;
    const char details[]="{\"readFiles\":[],\"modifiedFiles\":[\"result.txt\"]}";
    const char suffix[]="\n\n<modified-files>\nresult.txt\n</modified-files>";
    for(size_t i=0;i<pg_session_entry_count(session);++i){
        pg_session_entry entry;CHECK(pg_session_entry_at(session,i,&entry)==PG_SESSION_OK);
        if(entry.kind!=PG_ENTRY_COMPACTION)continue;
        ++markers;CHECK(entry.message && entry.message->details_json.len==sizeof(details)-1);
        CHECK(!memcmp(entry.message->details_json.data,details,sizeof(details)-1));
        CHECK(entry.message->content_count==1);
        pangu_slice text=entry.message->content[0].data;int found=0;
        for(size_t j=0;j+sizeof(suffix)-1<=text.len;++j)
            if(!memcmp(text.data+j,suffix,sizeof(suffix)-1)){found=1;break;}
        CHECK(found);
    }
    CHECK(markers==1);uint64_t sequence=pg_session_sequence(session),leaf=pg_session_leaf(session);
    CHECK(pg_scope_dispose(scope)==PG_OK);pg_lease_release(tools);pg_agent_app_close(app);
    options.create=0;CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    session=pg_agent_app_session(app);CHECK(pg_session_sequence(session)==sequence && pg_session_leaf(session)==leaf);
    markers=0;
    for(size_t i=0;i<pg_session_entry_count(session);++i){
        pg_session_entry entry;CHECK(pg_session_entry_at(session,i,&entry)==PG_SESSION_OK);
        if(entry.kind==PG_ENTRY_COMPACTION){++markers;CHECK(entry.message->details_json.len==sizeof(details)-1);
            CHECK(!memcmp(entry.message->details_json.data,details,sizeof(details)-1));}
    }
    CHECK(markers==1);pg_agent_app_close(app);free(file);free(url);
    puts("file-tool/summary/metadata/reopen passed");
}
int main(int argc,char **argv) {
    CHECK((argc==5 || argc==6) && curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK);
    pg_session_options options={.path={argv[2],strlen(argv[2])},.backend=!strcmp(argv[4],"sqlite")?PG_STORE_SQLITE:PG_STORE_JSONL,.create=1};
    pg_file_tools_config files={.environment=pg_local_file_environment(argv[3]),.validate=pg_schema_validate};
    pg_app_plugin plugin={.plugin=&pg_file_tools_plugin,.config=&files,.config_size=sizeof(files)};
    pg_agent_app *app=NULL; CHECK(pg_agent_app_open(&options,&plugin,1,&app)==PG_OK);
    pg_lease *tools=NULL;
    CHECK(pg_service_acquire(pg_agent_app_scope(app),PG_NAME(PG_TOOLS_SERVICE),1,&tools)==PG_OK);
    const pg_tools_service *table=pg_lease_table(tools); CHECK(table && table->count>=2);
    pg_deepseek_config model={.endpoint={argv[1],strlen(argv[1])},.tools=table->tools,.tool_count=table->count,.timeout_ms=5000};
    pg_scope *model_scope=NULL;
    CHECK(pg_plugin_install(pg_agent_app_scope(app),&pg_deepseek_plugin,&model,sizeof(model),&model_scope)==PG_OK);
    pangu_loop_config cfg={.tools=table->tools,.tool_count=table->count,.api_key=S("redacted-integration"),
        .model_config={.id=S("deepseek-chat"),.provider=S("deepseek"),.api=S("openai-completions")},
        .request_options_json=S("{\"sessionId\":\"integration\"}")};
    observer observed={.trace=argc==6}; uint64_t subscription=0;
    CHECK(pg_session_subscribe(pg_agent_app_session(app),event,&observed,&subscription)==PG_SESSION_OK);
    CHECK(prompt(app,"write",&cfg)==PG_SESSION_OK);
    observed.trace=0;
    const pangu_core *messages=pg_session_messages(pg_agent_app_session(app));
    size_t results=0;
    for (size_t i=0;i<pangu_core_message_count(messages);++i) {
        const pangu_message *m=pangu_core_message_at(messages,i);
        if (m->role==PANGU_TOOL_RESULT) { CHECK(!m->is_error && m->tool_name.len==5); ++results; }
    }
    CHECK(results==1 && observed.commits && observed.live);
    for (size_t i=0;i<pangu_core_message_count(messages);++i) {
        const pangu_message *m=pangu_core_message_at(messages,i);
        if (m->role!=PANGU_ASSISTANT) continue;
        CHECK(m->model.len==13 && !memcmp(m->model.data,"deepseek-chat",13));
        CHECK(m->response_model.len==17 && !memcmp(m->response_model.data,"deepseek-concrete",17));
        CHECK(m->response_id.len==20 && !memcmp(m->response_id.data,"integration-response",20));
        const char *native=m->stop_reason==PANGU_TOOL_USE?"tool_calls":"stop";
        CHECK(m->raw_stop_reason.len==strlen(native) && !memcmp(m->raw_stop_reason.data,native,strlen(native)));
    }
    observed.cancel=1;
    CHECK(prompt(app,"cancel",&cfg)==PG_SESSION_CANCELLED && observed.cancel==2);
    messages=pg_session_messages(pg_agent_app_session(app));
    CHECK(pangu_core_message_at(messages,pangu_core_message_count(messages)-1)->stop_reason==PANGU_ABORTED);
    CHECK(prompt(app,"resume",&cfg)==PG_SESSION_OK);
    messages=pg_session_messages(pg_agent_app_session(app));
    size_t count=pangu_core_message_count(messages);
    const pangu_message *last=pangu_core_message_at(messages,count-1);
    CHECK(last->stop_reason==PANGU_STOP && last->content_count && last->content[0].data.len==7);
    CHECK(last->usage.has_reasoning && !last->usage.reasoning);
    cfg.prepare_request=prepare_failure;
    CHECK(prompt(app,"prepare failure",&cfg)==PG_SESSION_CORE_ERROR);
    messages=pg_session_messages(pg_agent_app_session(app));
    CHECK(pangu_core_message_count(messages)==count+2);
    const pangu_message *failure=pangu_core_message_at(messages,count+1);
    CHECK(failure->role==PANGU_ASSISTANT && failure->stop_reason==PANGU_ERROR && failure->error.len==26);
    cfg.prepare_request=NULL;
    for (int stage=0;stage<3;++stage) {
        cfg.transform_context=stage==0?context_failure:NULL;
        cfg.convert_to_model=stage==1?context_failure:NULL;
        cfg.get_api_key=stage==2?key_failure:NULL;
        CHECK(prompt(app,"request hook failure",&cfg)==PG_SESSION_CORE_ERROR);
        messages=pg_session_messages(pg_agent_app_session(app));
        CHECK(pangu_core_message_count(messages)==count+4+(size_t)stage*2);
    }
    cfg.transform_context=NULL; cfg.convert_to_model=NULL; cfg.get_api_key=NULL;
    cfg.request=abort_before_stream;
    CHECK(prompt(app,"abort before stream",&cfg)==PG_SESSION_CANCELLED);
    messages=pg_session_messages(pg_agent_app_session(app));
    CHECK(pangu_core_message_count(messages)==count+10);
    cfg.prepare_request=prepare_cancelled;
    CHECK(prompt(app,"preparation cancelled",&cfg)==PG_SESSION_CANCELLED);
    messages=pg_session_messages(pg_agent_app_session(app));
    CHECK(pangu_core_message_count(messages)==count+12);
    uint64_t sequence=pg_session_sequence(pg_agent_app_session(app));
    CHECK(pg_session_unsubscribe(pg_agent_app_session(app),subscription)==PG_SESSION_OK);
    CHECK(pg_scope_dispose(model_scope)==PG_OK); pg_lease_release(tools); pg_agent_app_close(app);
    options.create=0;
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    messages=pg_session_messages(pg_agent_app_session(app));
    CHECK(pg_session_sequence(pg_agent_app_session(app))==sequence && pangu_core_message_count(messages)==count+12);
    const char *errors[]={"Request preparation failed","Context transform failed","Message conversion failed","API key resolution failed","Operation aborted","Operation aborted"};
    for (size_t i=0;i<6;++i) {
        failure=pangu_core_message_at(messages,count+1+i*2);
        CHECK(failure->role==PANGU_ASSISTANT && failure->stop_reason==(i>=4?PANGU_ABORTED:PANGU_ERROR));
        CHECK(failure->error.len==strlen(errors[i]) && !memcmp(failure->error.data,errors[i],failure->error.len));
    }
    last=pangu_core_message_at(messages,count-1);
    CHECK(last->usage.has_reasoning && !last->usage.reasoning && last->content[0].data.len==7);
    CHECK(!memcmp(last->content[0].data.data,"resumed",7));
    CHECK(last->response_model.len==17 && !memcmp(last->response_model.data,"deepseek-concrete",17));
    CHECK(last->response_id.len==20 && !memcmp(last->response_id.data,"integration-response",20));
    CHECK(last->raw_stop_reason.len==4 && !memcmp(last->raw_stop_reason.data,"stop",4));
    pg_agent_app_close(app);
    file_summary_roundtrip(argv[1],argv[2],&files,options.backend);
#ifdef PANGU_INTEGRATION_OVERFLOW
    recovery_roundtrip(argv[1],argv[2],options.backend,0);
    recovery_roundtrip(argv[1],argv[2],options.backend,1);
#endif
    curl_global_cleanup();
    puts("model/tool/persist/reopen/cancel/reuse passed"); return 0;
}
