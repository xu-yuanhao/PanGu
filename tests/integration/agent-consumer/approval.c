#include <pangu/agent/approval.h>
#include <pangu/agent/app.h>
#include <pangu/services/approval.h>
#include <pangu/core/loop.h>
#include <stdio.h>
#include <string.h>
#define S(x) ((pangu_slice){x,sizeof(x)-1})
#define CHECK(x) do{if(!(x)){fprintf(stderr,"installed approval check %d\n",__LINE__);goto fail;}}while(0)
typedef struct {pangu_session *session;int step,effects;pg_approval_outcome answer;uint64_t provider_id;const pangu_tool *selection;} fixture;
static pg_session_status answer(void *ctx,pangu_session *session,uint64_t id,
    const pg_approval_question *question,pg_approval_outcome *out){
    fixture *f=ctx;
    if(!id || pg_session_approval_pending(session)!=id || question->tool_name.len!=5)return PG_SESSION_INVALID;
    *out=f->answer;return PG_SESSION_OK;
}
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args){
    (void)ctx;(void)schema;return args.len==2&&!memcmp(args.data,"{}",2)?PANGU_OK:PANGU_INVALID;
}
static pangu_status execute(void *ctx,pangu_core *core,const pangu_content *call,pangu_tool_sink sink,void *target){
    (void)core;fixture *f=ctx;
    pg_approval_question q={.tool_name=call->name,.call_id=call->id,.reason=S("installed fixture action")};
    pg_approval_outcome outcome=PG_APPROVAL_ALLOWED_ONCE;
    pg_session_status z=pg_session_approval_request(f->session,&q,
        f->answer==PG_APPROVAL_UNAVAILABLE?NULL:answer,f,&outcome);
    if(z!=PG_SESSION_OK)return PANGU_TOOL_ERROR;
    int allowed=outcome==PG_APPROVAL_ALLOWED_ONCE;
    if(allowed)++f->effects;
    pangu_content text={.kind=PANGU_TEXT,.data=allowed?S("executed"):S("not executed")};
    pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1,.is_error=!allowed};
    return sink(target,&result,1);
}
static pangu_status model(void *ctx,pangu_core *core,pangu_model_sink sink,void *target){
    fixture *f=ctx;pangu_content content={.kind=PANGU_TEXT,.data=S("finished")};
    pangu_message message={.role=PANGU_ASSISTANT,.content=&content,.content_count=1,.stop_reason=PANGU_STOP};
    if(!f->step++){
        content=(pangu_content){.kind=PANGU_TOOL_CALL,.name=S("guard"),.id=S("guard-one"),.data=S("{}")};
        message.stop_reason=PANGU_TOOL_USE;
    }else{
        const pangu_message *last=NULL;
        /* A freshly reconciled tool declaration may follow the result. */
        for(size_t i=pangu_core_message_count(core);i>0;--i){
            const pangu_message *candidate=pangu_core_message_at(core,i-1);
            if(candidate->role==PANGU_TOOL_RESULT){last=candidate;break;}
        }
        if(!last || last->role!=PANGU_TOOL_RESULT || last->is_error!=(f->answer!=PG_APPROVAL_ALLOWED_ONCE))return PANGU_MODEL_ERROR;
    }
    return sink(target,&message,1);
}
typedef struct {size_t asked,decided;pg_approval_outcome last;} audit;
static pg_session_status collect(void *ctx,const pg_approval_event *event){
    audit *a=ctx;if(event->decided){++a->decided;a->last=event->outcome;}else ++a->asked;return PG_SESSION_OK;
}
static pg_status service_answer(void *ctx,pg_approval_provider *provider,uint64_t id,
    pangu_core *core,const pg_service_approval_question *q,pg_service_approval_outcome *out){
    (void)core;fixture *f=ctx;
    if(!pg_session_approval_pending(f->session) || !id || q->tool_name.len!=5)return PG_INVALID;
    f->provider_id=id;
    switch(f->answer){
    case PG_APPROVAL_ALLOWED_ONCE:*out=PG_SERVICE_ALLOWED_ONCE;break;
    case PG_APPROVAL_REJECTED:*out=PG_SERVICE_REJECTED;break;
    case PG_APPROVAL_CANCELLED:*out=PG_SERVICE_CANCELLED;break;
    case PG_APPROVAL_UNAVAILABLE:*out=PG_SERVICE_UNAVAILABLE;break;
    }
    return pg_approval_provider_reply(provider,id,*out);
}
/* The tool has no approval logic: the installed app/service composition gates
 * its actual effect and supplies the durable asked/decided audit. */
static pangu_status app_effect(void *ctx,pangu_core *core,const pangu_content *call,pangu_tool_sink sink,void *target){
    (void)core;(void)call;fixture *f=ctx;++f->effects;
    pangu_content text={.kind=PANGU_TEXT,.data=S("executed")};
    pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};
    return sink(target,&result,1);
}
static pangu_status select_tools(void *ctx,pangu_core *core,pangu_request_update_sink sink,void *target){
    (void)core;fixture *f=ctx;
    pangu_tool selected=*f->selection;
    char name[]="guard",schema[]="{}";
    selected.name=(pangu_slice){name,5};selected.schema_json=(pangu_slice){schema,2};
    pangu_request_update update={.fields=PANGU_UPDATE_TOOLS,.tools=&selected,.tool_count=1};
    pangu_status status=sink(target,&update);
    memset(name,'x',5);memset(schema,'x',2);
    return status;
}
static int app_flow(const char *path,pg_session_backend backend,int registered){
    pg_agent_app *app=NULL;remove(path);
    pg_session_options options={.path={path,strlen(path)},.backend=backend,.create=1};
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    fixture f={.session=pg_agent_app_session(app)};
    pg_approval_provider *provider=NULL;
    CHECK(pg_approval_provider_register(pg_agent_app_scope(app),service_answer,&f,&provider)==PG_OK);
    CHECK(pg_agent_app_set_approval(app,PG_APP_APPROVAL_ASK)==PG_OK);
    pangu_tool tool={.name=S("guard"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.execute=app_effect};
    pangu_loop_config cfg={.ctx=&f,.model=model,.tools=&tool,.tool_count=1};
    pg_tools_service source={.tools=&tool,.count=1};
    if(registered==1){
        CHECK(pg_service_register(pg_agent_app_scope(app),PG_NAME(PG_TOOLS_SERVICE),1,&source,NULL)==PG_OK);
        cfg.tools=NULL;cfg.tool_count=0;
    }else if(registered==2){
        f.selection=&tool;cfg.tool_count=0;
        cfg.prepare_request_update=select_tools;
    }
    pangu_content text={.kind=PANGU_TEXT,.data=S("installed app approval")};
    pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
    const pg_approval_outcome answers[]={PG_APPROVAL_ALLOWED_ONCE,PG_APPROVAL_REJECTED,PG_APPROVAL_CANCELLED,PG_APPROVAL_UNAVAILABLE};
    for(size_t i=0;i<4;++i){
        f.step=0;f.answer=answers[i];
        pg_session_status status=pg_agent_app_run(app,&input,1,&cfg);
        if(status!=PG_SESSION_OK || f.effects!=1)
            fprintf(stderr,"app approval source=%d answer=%zu status=%d effects=%d\n",registered,i,(int)status,f.effects);
        CHECK(status==PG_SESSION_OK && f.effects==1);
        CHECK(pg_approval_provider_reply(provider,f.provider_id,PG_SERVICE_ALLOWED_ONCE)==PG_NOT_FOUND);
    }
    CHECK(pg_agent_app_set_approval(app,PG_APP_APPROVAL_NEVER)==PG_OK);
    f.step=0;f.answer=PG_APPROVAL_REJECTED;
    CHECK(pg_agent_app_run(app,&input,1,&cfg)==PG_SESSION_OK && f.effects==1);
    pg_approval_provider_revoke(provider);
    pg_agent_app_close(app);app=NULL;options.create=0;
    CHECK(pg_agent_app_open(&options,NULL,0,&app)==PG_OK);
    f.session=pg_agent_app_session(app);
    if(registered==1)CHECK(pg_service_register(pg_agent_app_scope(app),PG_NAME(PG_TOOLS_SERVICE),1,&source,NULL)==PG_OK);
    audit a={0};CHECK(pg_session_approval_events(f.session,collect,&a)==PG_SESSION_OK);
    CHECK(a.asked==5 && a.decided==5 && a.last==PG_APPROVAL_REJECTED);
    CHECK(!pg_session_approval_pending(f.session));
    CHECK(pg_agent_app_set_approval(app,PG_APP_APPROVAL_ASK)==PG_OK);
    f.step=0;f.answer=PG_APPROVAL_UNAVAILABLE;
    CHECK(pg_agent_app_run(app,&input,1,&cfg)==PG_SESSION_OK && f.effects==1);
    a=(audit){0};CHECK(pg_session_approval_events(f.session,collect,&a)==PG_SESSION_OK);
    CHECK(a.asked==6 && a.decided==6 && a.last==PG_APPROVAL_UNAVAILABLE);
    pg_agent_app_close(app);remove(path);return 0;
fail:
    pg_agent_app_close(app);return 1;
}
int main(int argc,char **argv){
    pangu_session *session=NULL;CHECK(argc==3);remove(argv[1]);
    pg_session_options options={.path={argv[1],strlen(argv[1])},.create=1,
        .backend=!strcmp(argv[2],"sqlite")?PG_STORE_SQLITE:PG_STORE_JSONL};
    CHECK(pg_session_open(&options,&session,NULL)==PG_SESSION_OK);
    fixture f={.session=session};
    pangu_tool tool={.name=S("guard"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.execute=execute};
    pangu_loop_config cfg={.ctx=&f,.model=model,.tools=&tool,.tool_count=1};
    pangu_content text={.kind=PANGU_TEXT,.data=S("run guarded action")};
    pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
    const pg_approval_outcome answers[]={PG_APPROVAL_ALLOWED_ONCE,PG_APPROVAL_REJECTED,
        PG_APPROVAL_CANCELLED,PG_APPROVAL_UNAVAILABLE};
    for(size_t i=0;i<sizeof(answers)/sizeof(answers[0]);++i){
        f.step=0;f.answer=answers[i];
        CHECK(pg_session_run(session,&input,1,&cfg)==PG_SESSION_OK);
        CHECK(f.effects==1 && !pg_session_approval_pending(session));
    }
    audit a={0};CHECK(pg_session_approval_events(session,collect,&a)==PG_SESSION_OK);
    CHECK(a.asked==4 && a.decided==4 && a.last==PG_APPROVAL_UNAVAILABLE);
    uint64_t sequence=pg_session_sequence(session);pg_session_destroy(session);session=NULL;options.create=0;
    CHECK(pg_session_open(&options,&session,NULL)==PG_SESSION_OK && pg_session_sequence(session)==sequence);
    a=(audit){0};CHECK(pg_session_approval_events(session,collect,&a)==PG_SESSION_OK && a.asked==4 && a.decided==4);
    CHECK(!pg_session_approval_pending(session));
    CHECK(pg_session_approval_reply(session,1,PG_APPROVAL_ALLOWED_ONCE)==PG_SESSION_NOT_FOUND);
    pg_session_destroy(session);session=NULL;remove(argv[1]);
    if(app_flow(argv[1],options.backend,0))return 1;
    if(app_flow(argv[1],options.backend,1))return 1;
    return app_flow(argv[1],options.backend,2);
fail:
    pg_session_destroy(session);return 1;
}
