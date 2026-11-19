#include <pangu/core/core.h>
#include <pangu/core/loop.h>
#include <pangu/core/state.h>
#include <pangu/core/transcript.h>
#include <pangu/core/proxy.h>
#include <pangu/core/proxy_request.h>
#include <string.h>
static pangu_status provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)ctx;
    pangu_model_config model={0}; pangu_core *json=NULL;
    pangu_status status=pangu_proxy_request_json(owner,request,&model,(pangu_slice){0},&json);
    if (status!=PANGU_OK) return status;
    pangu_core_destroy(json);
    pangu_content text={.kind=PANGU_TEXT,.data={"installed",9}};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_STOP};
    return sink(target,&reply,1);
}
static pangu_status failed_provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)ctx;(void)request;(void)sink;(void)target;
    char detail[]="installed provider failure";
    pangu_status status=pangu_core_report_error(owner,(pangu_slice){detail,sizeof(detail)-1});
    memset(detail,'x',sizeof(detail)-1);
    return status==PANGU_OK?PANGU_MODEL_ERROR:status;
}
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)ctx;(void)schema;return args.len==2 && !memcmp(args.data,"{}",2)?PANGU_OK:PANGU_INVALID;
}
static pangu_status failed_tool(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)ctx;(void)call;(void)sink;(void)target;
    char text[]="installed tool failure";
    pangu_status status=pangu_core_report_error(owner,(pangu_slice){text,sizeof(text)-1});
    memset(text,'x',sizeof(text)-1);return status==PANGU_OK?PANGU_TOOL_ERROR:status;
}
typedef struct { int requests,effects; } persistent_host;
static pangu_status persistent_tool(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)owner;(void)call;++((persistent_host *)ctx)->effects;
    pangu_content text={.kind=PANGU_TEXT,.data={"stored tool",11}};
    pangu_message reply={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};return sink(target,&reply,1);
}
static pangu_status persistent_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)view;persistent_host *host=ctx;++host->requests;
    const pangu_model_config *model=pangu_core_active_model(owner);
    if(!model || model->id.len!=7 || memcmp(model->id.data,"model-A",7) ||
       pangu_core_active_thinking(owner)!=PANGU_THINKING_HIGH || pangu_core_active_tool_count(owner)!=1)return PANGU_MODEL_ERROR;
    pangu_content call={.kind=PANGU_TOOL_CALL,.id={"stored",6},.name={"stored",6},.data={"{}",2}};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=host->requests%2?&call:NULL,
        .content_count=host->requests%2?1:0,.stop_reason=host->requests%2?PANGU_TOOL_USE:PANGU_STOP};return sink(target,&reply,1);
}
static int installed_state(void) {
    pangu_core *owner=NULL;if(pangu_core_create(NULL,&owner)!=PANGU_OK)return 0;
    persistent_host host={0};char id[]="model-A";
    pangu_model_config model={.id={id,7},.provider={"local",5},.api={"fixture",7}};
    pangu_tool tool={.ctx=&host,.name={"stored",6},.schema_json={"{}",2},.validate=validate,.execute=persistent_tool};
    int passed=pangu_core_set_model(owner,&model)==PANGU_OK && pangu_core_set_thinking(owner,PANGU_THINKING_HIGH)==PANGU_OK &&
        pangu_core_set_tools(owner,&tool,1)==PANGU_OK;
    memset(id,'x',7);tool.execute=NULL;tool.name=(pangu_slice){"lost",4};
    pangu_loop_config config={.ctx=&host,.request=persistent_provider};pangu_message prompt={.role=PANGU_USER};
    for(int run=0;passed && run<2;++run){
        passed=pangu_core_run(owner,&prompt,1,&config)==PANGU_OK && host.effects==run+1 && host.requests==2*(run+1);
        passed=passed && !pangu_core_active_model(owner) && pangu_core_model(owner)->id.len==7 &&
            !memcmp(pangu_core_model(owner)->id.data,"model-A",7) && pangu_core_tool_count(owner)==1;
        if(passed)passed=pangu_core_reset(owner)==PANGU_OK;
    }
    pangu_core_destroy(owner);return passed;
}
int main(void) {
    pangu_core *core=NULL;
    if (pangu_core_create(NULL,&core)!=PANGU_OK) return 1;
    pangu_loop_config config={.request=provider};
    pangu_message prompt={.role=PANGU_USER};
    pangu_status status=pangu_core_run(core,&prompt,1,&config);
    const pangu_message *reply=pangu_core_message_at(core,1);
    int passed=status==PANGU_OK && reply && reply->content_count==1 &&
        reply->content[0].data.len==9 && !memcmp(reply->content[0].data.data,"installed",9) &&
        pangu_core_wait_idle(core)==PANGU_OK;
    if(passed) {
        passed=pangu_core_reset(core)==PANGU_OK;
        config.request=failed_provider;
        status=pangu_core_run(core,&prompt,1,&config);
        pangu_slice detail=pangu_core_error(core);
        passed=passed && status==PANGU_MODEL_ERROR && detail.len==26 &&
            !memcmp(detail.data,"installed provider failure",26);
    }
    if(passed){
        passed=pangu_core_reset(core)==PANGU_OK;
        pangu_tool tool={.name={"fail",4},.schema_json={"{}",2},.validate=validate,.execute=failed_tool};
        pangu_loop_config tools={.tools=&tool,.tool_count=1};pangu_message assistant={.role=PANGU_ASSISTANT};
        pangu_content call={.kind=PANGU_TOOL_CALL,.id={"one",3},.name={"fail",4},.data={"{}",2}};
        pangu_tool_outcome out={0};status=pangu_core_call_tool(core,&assistant,&call,&tools,NULL,NULL,&out);
        const pangu_message *result=pangu_core_message_at(out.result,0);
        passed=passed && status==PANGU_OK && result && result->is_error && result->content_count==1 &&
            result->content[0].data.len==22 && !memcmp(result->content[0].data.data,"installed tool failure",22) &&
            !pangu_core_message_count(core) && !pangu_core_error(core).len;
        pangu_core_destroy(out.result);
    }
    pangu_core_destroy(core); return !(passed && installed_state());
}
