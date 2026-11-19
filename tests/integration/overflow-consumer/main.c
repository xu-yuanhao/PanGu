#include <pangu/core/overflow.h>
#include <pangu/core/loop.h>
typedef struct {pangu_overflow_classifier *classifier;int classified;} host;
static pangu_status request(void *ctx,pangu_core *owner,const pangu_core *input,pangu_model_sink sink,void *target) {
    (void)ctx;(void)owner;(void)input;
    const char error[]="custom-provider API error (413): request too large";
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_ERROR,.error={error,sizeof(error)-1}};
    return sink(target,&reply,1);
}
static void event(void *ctx,pangu_core *owner,const pangu_event *e) {
    (void)owner;host *h=ctx;
    if(e->kind==PANGU_TURN_END){
        int overflow=0;
        if(pangu_message_is_context_overflow(h->classifier,e->message,100,&overflow)==PANGU_OK && overflow)++h->classified;
    }
}
int main(void) {
    host h={0};pangu_core *owner=NULL;
    if(pangu_overflow_create(NULL,&h.classifier)!=PANGU_OK)return 1;
    if(pangu_core_create(NULL,&owner)!=PANGU_OK){pangu_overflow_destroy(h.classifier);return 2;}
    pangu_loop_config cfg={.ctx=&h,.request=request,.event=event};
    pangu_message prompt={.role=PANGU_USER};
    pangu_status status=pangu_core_run(owner,&prompt,1,&cfg);
    int passed=status==PANGU_MODEL_ERROR && h.classified==1 && pangu_core_message_count(owner)==2;
    pangu_core_destroy(owner);pangu_overflow_destroy(h.classifier);return !passed;
}
