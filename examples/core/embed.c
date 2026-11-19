#include <pangu/core/state.h>
#include <stdio.h>
#include <string.h>
#define S(x) (pangu_slice){x, sizeof(x)-1}
typedef struct { unsigned requests, effects, queued; } host;
/* This fixed script demonstrates injection, not a network/model provider. */
static pangu_status model(void *ctx,pangu_core *owner,const pangu_core *core,pangu_model_sink sink,void *target) {
    (void)owner;
    host *h=ctx;
    pangu_content b={.kind=PANGU_TEXT,.data=S("The local tool completed.")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1,.stop_reason=PANGU_STOP};
    if (++h->requests==1) {
        b=(pangu_content){.kind=PANGU_TOOL_CALL,.id=S("demo-1"),.name=S("local"),.data=S("{}")};
        m.stop_reason=PANGU_TOOL_USE;
    } else {
        pangu_role tail=pangu_core_message_at(core,pangu_core_message_count(core)-1)->role;
        if (tail!=PANGU_TOOL_RESULT && tail!=PANGU_USER) return PANGU_MODEL_ERROR;
    }
    return sink(target,&m,1);
}
/* Fixed fixture validator accepts only the script's exact empty object.
 * A production host supplies its mature JSON Schema implementation here. */
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)ctx; (void)schema;
    return args.len==2 && !memcmp(args.data,"{}",2)?PANGU_OK:PANGU_INVALID;
}
static pangu_status execute(void *ctx,pangu_core *core,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)core; (void)call; ++((host *)ctx)->effects;
    pangu_content b={.kind=PANGU_TEXT,.data=S("local tool result")};
    pangu_message m={.role=PANGU_TOOL_RESULT,.content=&b,.content_count=1};
    return sink(target,&m,1);
}
static void event(void *ctx,pangu_core *core,const pangu_event *e) {
    host *h=ctx;
    if (e->kind==PANGU_TOOL_END && !h->queued) {
        pangu_content b={.kind=PANGU_TEXT,.data=S("Explain the result")};
        pangu_message follow={.role=PANGU_USER,.content=&b,.content_count=1};
        if (pangu_core_enqueue(core,PANGU_FOLLOW_UP,&follow)==PANGU_OK) {
            h->queued=1;
            const pangu_message *next=pangu_core_peek_queued_at(core,0);
            if (next && next->content_count) {
                fputs("Queued: ",stdout);
                fwrite(next->content[0].data.data,1,next->content[0].data.len,stdout); putchar('\n');
            }
        }
    }
    if (e->kind==PANGU_MESSAGE_END && e->message->content_count && e->message->content[0].kind==PANGU_TEXT) {
        pangu_slice s=e->message->content[0].data;
        fwrite(s.data,1,s.len,stdout); putchar('\n');
    }
}
int main(void) {
    pangu_core *core=NULL; host h={0};
    if (pangu_core_create(NULL,&core)!=PANGU_OK) return 1;
    pangu_tool tool={.name=S("local"),.schema_json=S("{\"type\":\"object\"}"),.ctx=&h,.validate=validate,.execute=execute};
    uint64_t subscription=0;
    if (pangu_core_subscribe(core,event,&h,&subscription)!=PANGU_OK) { pangu_core_destroy(core); return 1; }
    pangu_loop_config config={.ctx=&h,.request=model,.tools=&tool,.tool_count=1};
    pangu_content text={.kind=PANGU_TEXT,.data=S("Run the local tool")};
    pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
    pangu_status s=pangu_core_run(core,&prompt,1,&config);
    int success=s==PANGU_OK && h.requests==3 && h.effects==1 && h.queued && pangu_core_message_count(core)==7;
    pangu_core_unsubscribe(core,subscription);
    pangu_core_destroy(core);
    return !success;
}
