#include <pangu/core/transcript.h>
#include <pangu/core/state.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); abort(); } } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
static int same(pangu_slice a,const char *text) { return a.len==strlen(text) && (!a.len || !memcmp(a.data,text,a.len)); }
static void seed(pangu_core *c) {
    pangu_content b={.kind=PANGU_TEXT,.data=S("base")};
    pangu_system_section sections[]={{.name=S("one"),.value=S("first")},{.name=S("two"),.value=S("second")}};
    pangu_tool_declaration tools[]={{.name=S("a"),.schema_json=S("{}")},{.name=S("b"),.schema_json=S("{}")}};
    pangu_message m={.role=PANGU_SYSTEM,.content=&b,.content_count=1,.sections=sections,.section_count=2,
        .tools_added=tools,.tools_added_count=2,.timestamp_ms=123};
    CHECK(pangu_core_append(c,&m)==PANGU_OK);
    m=(pangu_message){.role=PANGU_USER}; CHECK(pangu_core_append(c,&m)==PANGU_OK);
    b.data=S("patch"); sections[0].removed=1; sections[1].value=S("updated");
    pangu_slice removed=S("a"); tools[0].schema_json=S("{\"type\":\"string\"}");
    m=(pangu_message){.role=PANGU_SYSTEM,.content=&b,.content_count=1,.sections=sections,.section_count=2,
        .tools_added=tools,.tools_added_count=1,.tools_removed=&removed,.tools_removed_count=1,.timestamp_ms=456};
    CHECK(pangu_core_append(c,&m)==PANGU_OK);
    sections[0].removed=0; sections[0].value=S("readded");
    m=(pangu_message){.role=PANGU_SYSTEM,.sections=sections,.section_count=1};
    CHECK(pangu_core_append(c,&m)==PANGU_OK);
}
static void baseline(const pangu_core *c) {
    CHECK(pangu_core_message_count(c)==1);
    const pangu_message *m=pangu_core_message_at(c,0);
    CHECK(m->role==PANGU_SYSTEM && m->timestamp_ms==123);
    CHECK(m->content_count==1 && same(m->content[0].data,"base\n\npatch"));
    CHECK(m->section_count==2 && same(m->sections[0].name,"two") && same(m->sections[0].value,"updated"));
    CHECK(same(m->sections[1].name,"one") && same(m->sections[1].value,"readded"));
    CHECK(m->tools_added_count==2 && same(m->tools_added[0].name,"b") && same(m->tools_added[1].name,"a"));
    CHECK(same(m->tools_added[1].schema_json,"{\"type\":\"string\"}"));
    CHECK(!m->tools_removed_count);
}
typedef struct { int transformed,converted,requests; size_t delta_add,delta_remove; } fixture;
static pangu_status request(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    fixture *f=ctx; ++f->requests;
    CHECK(pangu_core_is_running(owner));
    for (size_t i=0;i<pangu_core_message_count(view);++i) CHECK(pangu_core_message_at(view,i)->role!=PANGU_CUSTOM);
    if (f->transformed) CHECK(same(pangu_core_message_at(view,pangu_core_message_count(view)-1)->content[0].data,"request only"));
    pangu_message m={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&m,1);
}
static pangu_status transform(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *out) {
    fixture *f=ctx; (void)owner; CHECK(pangu_core_message_count(input)>0); ++f->transformed;
    pangu_content b={.kind=PANGU_TEXT,.data=S("request only")};
    pangu_message m={.role=PANGU_USER,.content=&b,.content_count=1};
    return pangu_core_append(out,&m);
}
static pangu_status convert(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *out) {
    fixture *f=ctx; (void)owner; CHECK(f->transformed); ++f->converted;
    return pangu_core_append(out,pangu_core_message_at(input,0));
}
static void event(void *ctx,pangu_core *c,const pangu_event *e) {
    fixture *f=ctx; (void)c;
    if (e->kind==PANGU_MESSAGE_END && e->message->role==PANGU_SYSTEM) {
        CHECK(e->message->timestamp_ms>0);
        f->delta_add+=e->message->tools_added_count; f->delta_remove+=e->message->tools_removed_count;
    }
}
static void requests(void) {
    fixture f={0}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_tool tool={.name=S("tool"),.description=S("original"),.schema_json=S("{ \"type\" : \"object\" }")};
    pangu_loop_config cfg={.ctx=&f,.request=request,.event=event,.tools=&tool,.tool_count=1};
    pangu_content b={.kind=PANGU_TEXT,.data=S("user")};
    pangu_message prompts[]={{.role=PANGU_CUSTOM,.custom_role=S("notification")},{.role=PANGU_USER,.content=&b,.content_count=1}};
    CHECK(pangu_core_run(c,prompts,2,&cfg)==PANGU_OK);
    CHECK(f.delta_add==1 && f.delta_remove==0);
    tool.schema_json=S("{\"type\":\"object\"}");
    CHECK(pangu_core_run(c,&prompts[1],1,&cfg)==PANGU_OK);
    CHECK(f.delta_add==1 && f.delta_remove==0); /* insignificant whitespace */
    tool.description=S("changed");
    CHECK(pangu_core_run(c,&prompts[1],1,&cfg)==PANGU_OK);
    CHECK(f.delta_add==2 && f.delta_remove==1);
    cfg.tool_count=0;
    CHECK(pangu_core_run(c,&prompts[1],1,&cfg)==PANGU_OK);
    CHECK(f.delta_add==2 && f.delta_remove==2);
    cfg.transform_context=transform; cfg.convert_to_model=convert;
    size_t count=pangu_core_message_count(c);
    CHECK(pangu_core_run(c,&prompts[1],1,&cfg)==PANGU_OK);
    CHECK(f.transformed==1 && f.converted==1 && pangu_core_message_count(c)==count+2);
    CHECK(same(pangu_core_message_at(c,count)->content[0].data,"user"));
    CHECK(pangu_core_reset(c)==PANGU_OK);
    CHECK(pangu_core_message_count(c)==1 && !pangu_core_message_at(c,0)->tools_added_count);
    pangu_core_destroy(c);
}
static void schema_comparison(void) {
    fixture f={0}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    const char *schemas[]={
        "{\"maximum\":1.0,\"2\":true,\"1\":false,\"title\":\"\\u0078\",\"x\":0,\"x\":2}",
        "{\"1\":false,\"2\":true,\"maximum\":1,\"title\":\"x\",\"x\":2}",
        "{\"1\":false,\"2\":true,\"title\":\"x\",\"maximum\":1,\"x\":2}"};
    pangu_tool tool={.name=S("numeric")};
    pangu_loop_config cfg={.ctx=&f,.request=request,.event=event,.tools=&tool,.tool_count=1};
    pangu_message prompt={.role=PANGU_USER};
    for (size_t i=0;i<3;++i) {
        tool.schema_json=(pangu_slice){schemas[i],strlen(schemas[i])};
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
        CHECK(f.delta_add==(i==2?2:1) && f.delta_remove==(i==2?1:0));
        printf("schema:%zu:%zu\n",f.delta_add,f.delta_remove);
    }
    tool.has_constrained_sampling=1; tool.constrained_sampling=1;
    CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
    CHECK(f.delta_add==3 && f.delta_remove==2);
    pangu_core *snapshot=NULL; CHECK(pangu_core_system_snapshot(c,&snapshot)==PANGU_OK);
    CHECK(pangu_core_message_at(snapshot,0)->tools_added[0].has_constrained_sampling);
    CHECK(pangu_core_message_at(snapshot,0)->tools_added[0].constrained_sampling);
    pangu_core_destroy(snapshot); pangu_core_destroy(c);
}
typedef struct { size_t calls,fail,live; } memory;
static void *allocate(void *ctx,size_t n) { memory *m=ctx; if (++m->calls==m->fail) return NULL; void *p=malloc(n); if (p) ++m->live; return p; }
static void release(void *ctx,void *p) { if (p) { --((memory *)ctx)->live; free(p); } }
static void reset_failures(void) {
    for (size_t fail=1;fail<20;++fail) {
        memory mem={0}; pangu_core_config cfg={.allocator={&mem,allocate,release}};
        pangu_core *c=NULL; CHECK(pangu_core_create(&cfg,&c)==PANGU_OK); seed(c);
        const pangu_message *first=pangu_core_message_at(c,0); mem.fail=mem.calls+fail;
        pangu_status s=pangu_core_reset(c);
        if (s==PANGU_NOMEM) { CHECK(pangu_core_message_count(c)==4 && pangu_core_message_at(c,0)==first); mem.fail=0; CHECK(pangu_core_reset(c)==PANGU_OK); }
        else CHECK(s==PANGU_OK);
        baseline(c); pangu_core_destroy(c); CHECK(!mem.live);
        if (s==PANGU_OK) break;
    }
}
static pangu_status default_model(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    int *requests=ctx; ++*requests;
    CHECK(pangu_core_set_default_stream(owner,NULL,NULL)==PANGU_BUSY);
    CHECK(pangu_core_message_count(view)==1);
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&reply,1);
}
static void default_stream(void) {
    pangu_core *a=NULL,*b=NULL; int first=0,second=0,explicit=0;
    CHECK(pangu_core_create(NULL,&a)==PANGU_OK && pangu_core_create(NULL,&b)==PANGU_OK);
    pangu_message prompt={.role=PANGU_USER}; pangu_loop_config cfg={0};
    CHECK(pangu_core_run(a,&prompt,1,&cfg)==PANGU_INVALID);
    CHECK(pangu_core_set_default_stream(a,default_model,&first)==PANGU_OK);
    CHECK(pangu_core_set_default_stream(b,default_model,&second)==PANGU_OK);
    CHECK(pangu_core_run(a,&prompt,1,&cfg)==PANGU_OK && first==1 && second==0);
    CHECK(pangu_core_run(b,&prompt,1,&cfg)==PANGU_OK && second==1);
    CHECK(pangu_core_clear(a)==PANGU_OK);
    cfg.request=default_model; cfg.has_request_ctx=1; cfg.request_ctx=&explicit;
    CHECK(pangu_core_run(a,&prompt,1,&cfg)==PANGU_OK && explicit==1 && first==1);
    CHECK(pangu_core_reset(a)==PANGU_OK); cfg=(pangu_loop_config){0};
    CHECK(pangu_core_run(a,&prompt,1,&cfg)==PANGU_OK && first==2);
    CHECK(pangu_core_set_default_stream(a,NULL,NULL)==PANGU_OK);
    CHECK(pangu_core_run(a,&prompt,1,&cfg)==PANGU_INVALID);
    pangu_core_destroy(a); pangu_core_destroy(b);
}
typedef struct { int calls,changed,fail; } preparation;
static pangu_status updated_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    preparation *f=ctx; (void)owner; ++f->calls;
    CHECK(pangu_core_message_count(view)==(size_t)f->calls);
    CHECK(same(pangu_core_message_at(view,0)->content[0].data,"replacement"));
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&reply,1);
}
static pangu_turn_decision updated_finish(void *ctx,pangu_core *owner,const pangu_message *message) {
    (void)owner; (void)message;
    return ((preparation *)ctx)->calls==1?PANGU_TURN_CONTINUE:PANGU_TURN_AUTO;
}
static pangu_status updated_prepare(void *ctx,pangu_core *owner,pangu_request_update_sink sink,void *target) {
    preparation *f=ctx; (void)owner;
    if (f->changed++) return PANGU_OK;
    pangu_core *replacement=NULL; CHECK(pangu_core_create(NULL,&replacement)==PANGU_OK);
    pangu_content b={.kind=PANGU_TEXT,.data=S("replacement")};
    pangu_message m={.role=PANGU_USER,.content=&b,.content_count=1};
    CHECK(pangu_core_append(replacement,&m)==PANGU_OK);
    pangu_request_update update={.fields=PANGU_UPDATE_CONTEXT|PANGU_UPDATE_PROVIDER,.context=replacement,
        .request=updated_provider,.request_ctx=f};
    pangu_status status=sink(target,&update);
    pangu_core_destroy(replacement);
    return f->fail?PANGU_MODEL_ERROR:status;
}
static void updated_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    preparation *f=ctx;
    if (event->kind!=PANGU_AGENT_END) return;
    CHECK(event->new_messages && event->first_message==0);
    CHECK(event->message_count==(size_t)(f->fail?1:3));
    CHECK(event->message_count==pangu_core_message_count(event->new_messages));
    if (f->fail) CHECK(pangu_core_message_at(event->new_messages,0)->role==PANGU_ASSISTANT);
    else CHECK(same(pangu_core_message_at(event->new_messages,0)->content[0].data,"original"));
    if (!f->fail) CHECK(same(pangu_core_message_at(owner,0)->content[0].data,"replacement"));
    CHECK(pangu_core_is_running(owner));
}
static void preparation_updates(void) {
    for (int fail=0;fail<2;++fail) {
        preparation f={.fail=fail}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        int unused=0;
        pangu_loop_config cfg={.ctx=&f,.request=default_model,.has_request_ctx=1,.request_ctx=&unused,
            .prepare_request_update=updated_prepare,.finish_turn=updated_finish,.event=updated_event};
        pangu_content b={.kind=PANGU_TEXT,.data=S("original")};
        pangu_message prompt={.role=PANGU_USER,.content=&b,.content_count=1};
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==(fail?PANGU_MODEL_ERROR:PANGU_OK));
        CHECK(unused==0 && f.calls==(fail?0:2));
        CHECK(same(pangu_core_message_at(c,0)->content[0].data,fail?"original":"replacement"));
        CHECK(!pangu_core_is_running(c)); pangu_core_destroy(c);
    }
}
typedef struct { int completed,prepares,requests;size_t generated; } initial_fixture;
static void initial_event(void *ctx,pangu_core *owner,const pangu_event *e) {
    initial_fixture *f=ctx;(void)owner;
    if(e->kind==PANGU_MESSAGE_END){
        const char *texts[]={"prompt","steering","done"};CHECK(f->completed<3);
        CHECK(e->message->content_count==1 && same(e->message->content[0].data,texts[f->completed++]));
    }
    if(e->kind==PANGU_AGENT_END){
        f->generated=e->message_count;CHECK(f->generated==3);
        CHECK(same(pangu_core_message_at(e->new_messages,0)->content[0].data,"prompt"));
        CHECK(same(pangu_core_message_at(e->new_messages,1)->content[0].data,"steering"));
    }
}
static pangu_status initial_prepare(void *ctx,pangu_core *owner,pangu_request_update_sink sink,void *target) {
    initial_fixture *f=ctx;CHECK(f->completed==2 && pangu_core_message_count(owner)==2);++f->prepares;
    CHECK(same(pangu_core_message_at(owner,1)->content[0].data,"steering"));
    pangu_core *replacement=NULL;CHECK(pangu_core_create(NULL,&replacement)==PANGU_OK);
    pangu_content text={.kind=PANGU_TEXT,.data=S("canonical projection")};
    pangu_message user={.role=PANGU_USER,.content=&text,.content_count=1};CHECK(pangu_core_append(replacement,&user)==PANGU_OK);
    char model[]="replacement";
    pangu_request_update update={.fields=PANGU_UPDATE_CONTEXT|PANGU_UPDATE_MODEL|PANGU_UPDATE_THINKING,
        .context=replacement,.model={.id={model,sizeof(model)-1}},.thinking_level=PANGU_THINKING_HIGH};
    pangu_status status=sink(target,&update);memset(model,'x',sizeof(model)-1);pangu_core_destroy(replacement);return status;
}
static pangu_status initial_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    initial_fixture *f=ctx;++f->requests;
    CHECK(f->prepares==1 && pangu_core_message_count(view)==1);
    CHECK(same(pangu_core_active_model(owner)->id,"replacement") && pangu_core_active_thinking(owner)==PANGU_THINKING_HIGH);
    CHECK(same(pangu_core_message_at(view,0)->content[0].data,"canonical projection"));
    pangu_content text={.kind=PANGU_TEXT,.data=S("done")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_STOP};return sink(target,&reply,1);
}
static void initial_preparation(int trace) {
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);initial_fixture f={0};
    pangu_content text={.kind=PANGU_TEXT,.data=S("steering")};pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
    CHECK(pangu_core_enqueue(owner,PANGU_STEERING,&input)==PANGU_OK);text.data=S("prompt");
    pangu_loop_config config={.ctx=&f,.request=initial_provider,.event=initial_event,.prepare_request_update=initial_prepare};
    CHECK(pangu_core_loop(owner,&input,1,&config)==PANGU_OK && f.requests==1 && f.completed==3);
    CHECK(pangu_core_message_count(owner)==2 && !pangu_core_queue_count(owner,PANGU_STEERING));
    CHECK(pangu_core_message_at(owner,1)->thinking_level==PANGU_THINKING_HIGH);
    if(trace)printf("initial:%d:%d:%d:%zu:2\n",f.prepares,f.requests,f.completed,f.generated);
    pangu_core_destroy(owner);
}
typedef struct { int prepares,requests,included[2]; } late_fixture;
static pangu_status late_prepare(void *ctx,pangu_core *owner,pangu_request_update_sink sink,void *target) {
    (void)sink;(void)target;late_fixture *f=ctx;
    if(++f->prepares==1){
        pangu_content text={.kind=PANGU_TEXT,.data=S("late steering")};
        pangu_message message={.role=PANGU_USER,.content=&text,.content_count=1};
        return pangu_core_enqueue(owner,PANGU_STEERING,&message);
    }
    return PANGU_OK;
}
static pangu_status late_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    late_fixture *f=ctx;CHECK(f->requests<2);int included=0;
    for(size_t i=0;i<pangu_core_message_count(view);++i){
        const pangu_message *m=pangu_core_message_at(view,i);
        if(m->role==PANGU_USER && m->content_count && same(m->content[0].data,"late steering"))included=1;
    }
    CHECK(included==f->requests);
    CHECK(pangu_core_queue_count(owner,PANGU_STEERING)==(f->requests?0u:1u));
    f->included[f->requests++]=included;
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};return sink(target,&reply,1);
}
static void prepare_late_steering(int trace) {
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);late_fixture f={0};
    pangu_content text={.kind=PANGU_TEXT,.data=S("run")};pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
    pangu_loop_config config={.ctx=&f,.request=late_provider,.prepare_request_update=late_prepare};
    CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_OK && f.prepares==2 && f.requests==2);
    CHECK(pangu_core_message_count(owner)==4 && !pangu_core_queue_count(owner,PANGU_STEERING));
    if(trace)puts("{\"included\":[false,true],\"preparations\":2,\"messages\":4,\"queued\":false}");
    pangu_core_destroy(owner);
}
typedef struct { int mode,requests,finishes,effects,prepared; } control_fixture;
static pangu_status control_validate(void *ctx,pangu_slice schema,pangu_slice arguments) {
    (void)ctx;return same(schema,"{}") && same(arguments,"{}")?PANGU_OK:PANGU_INVALID;
}
static pangu_status control_execute(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)owner;(void)call;control_fixture *f=ctx;++f->effects;
    pangu_content text={.kind=PANGU_TEXT,.data=S("done")};
    pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};return sink(target,&result,1);
}
static pangu_before_result control_before(void *ctx,pangu_core *owner,const pangu_message *assistant,const pangu_content *call) {
    (void)owner;(void)assistant;control_fixture *f=ctx;
    int block=(f->mode==4 || f->mode==6) && same(call->id,"a");
    return (pangu_before_result){.block=block,.terminate=block,.reason=S("blocked")};
}
static pangu_status control_after(void *ctx,pangu_core *owner,const pangu_tool_context *input,pangu_patch_sink sink,void *target) {
    (void)owner;(void)input;control_fixture *f=ctx;
    if(f->mode!=5)return PANGU_OK;
    pangu_tool_patch patch={.fields=PANGU_PATCH_TERMINATE,.terminate=1};return sink(target,&patch);
}
static pangu_status control_prepare(void *ctx,pangu_core *owner) {
    (void)owner;++((control_fixture *)ctx)->prepared;return PANGU_OK;
}
static pangu_status control_enqueue(pangu_core *owner,pangu_queue_kind kind,const char *value) {
    pangu_content text={.kind=PANGU_TEXT,.data={value,strlen(value)}};
    pangu_message message={.role=PANGU_USER,.content=&text,.content_count=1};return pangu_core_enqueue(owner,kind,&message);
}
static pangu_turn_decision control_finish(void *ctx,pangu_core *owner,const pangu_turn_context *turn) {
    control_fixture *f=ctx;++f->finishes;
    size_t results=f->requests==1 && (f->mode==1 || (f->mode>=4 && f->mode<=6) || f->mode==9)?(f->mode==6?2u:1u):0;
    CHECK(pangu_core_message_count(turn->tool_results)==results);
    CHECK(pangu_core_message_count(turn->new_messages)==pangu_core_message_count(owner));
    for(size_t i=0;i<results;++i){
        const pangu_message *m=pangu_core_message_at(turn->tool_results,i);
        int blocked=(f->mode==4 || f->mode==6) && i==0;
        CHECK(m->role==PANGU_TOOL_RESULT && same(m->tool_name,"noop") && m->is_error==blocked);
        CHECK(m->content_count==1 && same(m->content[0].data,blocked?"blocked":"done") && !m->terminate);
    }
    if(f->finishes==1){
        if(f->mode==2 || f->mode>=7)CHECK(control_enqueue(owner,PANGU_STEERING,"steering")==PANGU_OK);
        if(f->mode==3 || f->mode>=7)CHECK(control_enqueue(owner,PANGU_FOLLOW_UP,"follow-up")==PANGU_OK);
        if(f->mode==9)return PANGU_TURN_END_RUN;
        if(f->mode<4 || f->mode==7 || f->mode==8)return PANGU_TURN_CONTINUE;
    }
    return PANGU_TURN_AUTO;
}
static pangu_status control_model(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)owner;control_fixture *f=ctx;CHECK(f->requests<2);++f->requests;
    if(f->requests==2 && (f->mode==2 || f->mode==3)){
        const pangu_message *last=pangu_core_message_at(view,pangu_core_message_count(view)-1);
        CHECK(last->role==PANGU_USER && same(last->content[0].data,f->mode==2?"steering":"follow-up"));
    }
    pangu_content content[2]={{.kind=PANGU_TEXT,.data=S("done")}};
    pangu_message message={.role=PANGU_ASSISTANT,.content=content,.content_count=1,.stop_reason=PANGU_STOP};
    if(f->requests==1 && (f->mode==1 || (f->mode>=4 && f->mode<=6) || f->mode==9)){
        content[0]=(pangu_content){.kind=PANGU_TOOL_CALL,.id=S("a"),.name=S("noop"),.data=S("{}")};
        content[1]=(pangu_content){.kind=PANGU_TOOL_CALL,.id=S("b"),.name=S("noop"),.data=S("{}")};
        message.content_count=f->mode==6?2:1;message.stop_reason=PANGU_TOOL_USE;
    }else if(f->mode==7 || f->mode==8){message.stop_reason=f->mode==7?PANGU_ERROR:PANGU_ABORTED;message.error=S("failed");}
    return sink(target,&message,1);
}
static void control_combinations(int trace) {
    for(int mode=0;mode<10;++mode){
        control_fixture f={.mode=mode};pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        pangu_tool tool={.name=S("noop"),.schema_json=S("{}"),.ctx=&f,.validate=control_validate,.execute=control_execute};
        int has_tool=mode==1 || (mode>=4 && mode<=6) || mode==9;
        pangu_loop_config config={.ctx=&f,.request=control_model,.tools=has_tool?&tool:NULL,.tool_count=has_tool,
            .before_tool=control_before,.after_tool_patch=control_after,.finish_turn_context=control_finish,.prepare_next_turn=control_prepare};
        pangu_content text={.kind=PANGU_TEXT,.data=S("run")};pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
        pangu_status status=pangu_core_run(owner,&prompt,1,&config);
        CHECK(status==(mode==7?PANGU_MODEL_ERROR:mode==8?PANGU_CANCELLED:PANGU_OK));
        int continuing=mode<4 || mode==6;
        CHECK(f.requests==1+continuing && f.finishes==f.requests && f.prepared==continuing);
        CHECK(f.effects==(mode==1 || mode==5 || mode==6 || mode==9));
        size_t pending=mode>=7?1:0;
        CHECK(pangu_core_queue_count(owner,PANGU_STEERING)==pending && pangu_core_queue_count(owner,PANGU_FOLLOW_UP)==pending);
        char roles[16]={0},errors[4]={0};size_t count=pangu_core_message_count(owner),results=0;CHECK(count<sizeof(roles));
        for(size_t i=0;i<count;++i){
            const pangu_message *m=pangu_core_message_at(owner,i);roles[i]="suatc"[m->role];
            if(m->role==PANGU_TOOL_RESULT){CHECK(results+1<sizeof(errors));errors[results++]=m->is_error?'1':'0';}
        }
        if(trace)printf("control:%d:%d:%d:%d:%d:%s:%zu:%zu:%s\n",mode,f.requests,f.finishes,f.effects,f.prepared,roles,pending,pending,errors);
        pangu_core_destroy(owner);
    }
}
static pangu_status transcript_only_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    ++*(int *)ctx;CHECK(pangu_core_message_count(view)==2 && !pangu_core_active_tool_count(owner));
    const pangu_message *system=pangu_core_message_at(view,0),*user=pangu_core_message_at(view,1);
    CHECK(system->role==PANGU_SYSTEM && system->timestamp_ms==1 && system->content_count==1 && same(system->content[0].data,"Transcript prompt"));
    CHECK(system->tools_added_count==0 && user->role==PANGU_USER && same(user->content[0].data,"Hello"));
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};return sink(target,&reply,1);
}
static void transcript_only_context(int trace) {
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);int calls=0;
    pangu_content text[]={{.kind=PANGU_TEXT,.data=S("Transcript prompt")},{.kind=PANGU_TEXT,.data=S("Hello")}};
    pangu_message input[]={{.role=PANGU_SYSTEM,.content=&text[0],.content_count=1,.timestamp_ms=1},
        {.role=PANGU_USER,.content=&text[1],.content_count=1}};
    pangu_loop_config config={.ctx=&calls,.request=transcript_only_provider};
    CHECK(pangu_core_loop(owner,input,2,&config)==PANGU_OK && calls==1 && pangu_core_message_count(owner)==3);
    if(trace)puts("{\"contextKeys\":[\"messages\"],\"system\":\"Transcript prompt\",\"systemTimestamp\":1,\"user\":\"Hello\",\"calls\":1}");
    pangu_core_destroy(owner);
}
typedef struct { int mode,phase; size_t projected; } projection_fixture;
static pangu_status prune_projection(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *output) {
    (void)owner;projection_fixture *f=ctx;CHECK(f->mode==1 && f->phase==0 && pangu_core_message_count(input)==5);f->phase=1;
    for(size_t i=3;i<5;++i){pangu_status s=pangu_core_append(output,pangu_core_message_at(input,i));if(s!=PANGU_OK)return s;}
    return PANGU_OK;
}
static pangu_status filter_projection(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *output) {
    (void)owner;projection_fixture *f=ctx;CHECK(f->phase==f->mode);f->phase=2;
    CHECK(pangu_core_message_count(input)==2);
    for(size_t i=0;i<2;++i){const pangu_message *m=pangu_core_message_at(input,i);
        if(m->role==PANGU_CUSTOM)continue;
        pangu_status s=pangu_core_append(output,m);if(s!=PANGU_OK)return s;
    }
    return PANGU_OK;
}
static pangu_status projection_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    projection_fixture *f=ctx;CHECK(f->phase==2);f->phase=3;f->projected=pangu_core_message_count(view);
    CHECK(f->projected==(f->mode?2u:1u));
    CHECK(pangu_core_message_count(owner)==(f->mode?5u:2u));
    const pangu_message *tail=pangu_core_message_at(view,f->projected-1);
    CHECK(tail->role==PANGU_USER && same(tail->content[0].data,f->mode?"new message":"Hello"));
    if(f->mode)CHECK(same(pangu_core_message_at(view,0)->content[0].data,"old response 2"));
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};return sink(target,&reply,1);
}
static void projection_scenarios(int trace) {
    for(int mode=0;mode<2;++mode){
        pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);projection_fixture f={.mode=mode};
        const char *history[]={"old message 1","old response 1","old message 2","old response 2"};
        for(int i=0;i<(mode?4:1);++i){
            pangu_content text={.kind=PANGU_TEXT,.data=mode?(pangu_slice){history[i],strlen(history[i])}:S("This is a notification")};
            pangu_message message={.role=mode?(i%2?PANGU_ASSISTANT:PANGU_USER):PANGU_CUSTOM,
                .custom_role=mode?(pangu_slice){0}:S("notification"),.content=&text,.content_count=1};
            CHECK(pangu_core_append(owner,&message)==PANGU_OK);
        }
        const pangu_message *saved=pangu_core_message_at(owner,0);
        pangu_content text={.kind=PANGU_TEXT,.data=mode?S("new message"):S("Hello")};
        pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
        pangu_loop_config config={.ctx=&f,.request=projection_provider,.transform_context=mode?prune_projection:NULL,.convert_to_model=filter_projection};
        CHECK(pangu_core_loop(owner,&prompt,1,&config)==PANGU_OK && f.phase==3);
        CHECK(pangu_core_message_at(owner,0)==saved && saved->role==(mode?PANGU_USER:PANGU_CUSTOM));
        if(trace)printf("projection:%d:%zu:%zu:3\n",mode,f.projected,pangu_core_message_count(owner));
        pangu_core_destroy(owner);
    }
}
typedef struct { int custom,converted,starts,ends; } tail_fixture;
static pangu_status tail_convert(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *output) {
    (void)owner;tail_fixture *f=ctx;++f->converted;
    CHECK(pangu_core_message_count(input)==1);
    pangu_message m=*pangu_core_message_at(input,0);
    CHECK(m.role==(f->custom?PANGU_CUSTOM:PANGU_USER) && m.timestamp_ms==17);
    m.role=PANGU_USER;m.custom_role=(pangu_slice){0};return pangu_core_append(output,&m);
}
static pangu_status tail_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    tail_fixture *f=ctx;CHECK(f->converted==1 && pangu_core_message_count(view)==1);
    const pangu_message *projected=pangu_core_message_at(view,0),*original=pangu_core_message_at(owner,0);
    CHECK(projected->role==PANGU_USER && projected->timestamp_ms==17 && same(projected->content[0].data,"Hook content"));
    CHECK(original->role==(f->custom?PANGU_CUSTOM:PANGU_USER));
    pangu_content text={.kind=PANGU_TEXT,.data=S("Response")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_STOP};return sink(target,&reply,1);
}
static void tail_event(void *ctx,pangu_core *owner,const pangu_event *e) {
    (void)owner;tail_fixture *f=ctx;
    if(e->kind==PANGU_MESSAGE_START || e->kind==PANGU_MESSAGE_END){
        CHECK(e->message->role==PANGU_ASSISTANT);
        if(e->kind==PANGU_MESSAGE_START)++f->starts;else ++f->ends;
    }
    if(e->kind==PANGU_AGENT_END)CHECK(e->message_count==1 && pangu_core_message_count(e->new_messages)==1);
}
static void continuation_tails(int trace) {
    for(int custom=0;custom<2;++custom){
        tail_fixture f={.custom=custom};pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        pangu_content text={.kind=PANGU_TEXT,.data=S("Hook content")};
        pangu_message input={.role=custom?PANGU_CUSTOM:PANGU_USER,.custom_role=custom?S("custom"):(pangu_slice){0},.content=&text,.content_count=1,.timestamp_ms=17};
        CHECK(pangu_core_append(owner,&input)==PANGU_OK);const pangu_message *saved=pangu_core_message_at(owner,0);
        pangu_loop_config config={.ctx=&f,.request=tail_provider,.convert_to_model=tail_convert,.event=tail_event};
        CHECK(pangu_core_loop_continue(owner,&config)==PANGU_OK && f.starts==1 && f.ends==1 && f.converted==1);
        CHECK(pangu_core_message_count(owner)==2 && pangu_core_message_at(owner,0)==saved);
        if(trace)printf("tail:%d:%d:%d:%d:1:2\n",custom,f.converted,f.starts,f.ends);
        pangu_core_destroy(owner);
    }
}
static void continuation_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    int *ends=ctx; (void)owner;
    if (event->kind==PANGU_AGENT_END) {
        ++*ends; CHECK(event->message_count==1 && event->new_messages);
        CHECK(pangu_core_message_at(event->new_messages,0)->role==PANGU_ASSISTANT);
    }
}
static pangu_status continuation_model(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)ctx; (void)owner; CHECK(pangu_core_message_count(view)==1);
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&reply,1);
}
static void continuation_messages(void) {
    pangu_core *c=NULL; int ends=0; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_message user={.role=PANGU_USER}; CHECK(pangu_core_append(c,&user)==PANGU_OK);
    pangu_loop_config cfg={.ctx=&ends,.request=continuation_model,.event=continuation_event};
    CHECK(pangu_core_run(c,NULL,0,&cfg)==PANGU_OK && ends==1);
    CHECK(pangu_core_message_count(c)==2); pangu_core_destroy(c);
}
typedef struct { int requests,prepared,ends,turns,metadata; memory *allocator; } next_fixture;
static pangu_status next_messages(void *ctx,pangu_core *owner,pangu_request_update_sink sink,void *target) {
    next_fixture *f=ctx; ++f->prepared;
    pangu_core *extra=NULL; CHECK(pangu_core_create(NULL,&extra)==PANGU_OK);
    pangu_content b={.kind=PANGU_TEXT,.data=S("prepared")};
    pangu_message m={.role=PANGU_USER,.content=&b,.content_count=1};
    CHECK(pangu_core_append(extra,&m)==PANGU_OK);
    pangu_request_update update={.fields=PANGU_UPDATE_MESSAGES,.messages=extra};
    char model[]="next-model";
    if(f->metadata){
        update.fields|=PANGU_UPDATE_CONTEXT|PANGU_UPDATE_MODEL|PANGU_UPDATE_THINKING;
        update.context=owner;update.model.id=(pangu_slice){model,sizeof(model)-1};update.thinking_level=PANGU_THINKING_HIGH;
    }
    pangu_status s=sink(target,&update);
    memset(model,'x',sizeof(model)-1);
    pangu_core_destroy(extra);
    b.data=S("queued"); CHECK(pangu_core_enqueue(owner,PANGU_STEERING,&m)==PANGU_OK);
    return s;
}
static pangu_status next_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    next_fixture *f=ctx;
    if (++f->requests==2) {
        CHECK(pangu_core_message_count(view)==4);
        CHECK(same(pangu_core_message_at(view,2)->content[0].data,"prepared"));
        CHECK(same(pangu_core_message_at(view,3)->content[0].data,"queued"));
        if(f->metadata)CHECK(same(pangu_core_active_model(owner)->id,"next-model") && pangu_core_active_thinking(owner)==PANGU_THINKING_HIGH);
    }
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&reply,1);
}
static pangu_turn_decision next_finish(void *ctx,pangu_core *owner,const pangu_message *m) {
    (void)owner; (void)m;
    return ((next_fixture *)ctx)->requests==1?PANGU_TURN_CONTINUE:PANGU_TURN_AUTO;
}
static void next_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    next_fixture *f=ctx; (void)owner;
    if (event->kind==PANGU_AGENT_END) { ++f->ends; CHECK(event->message_count==5); }
}
static void next_preparation(int trace) {
    next_fixture f={.metadata=1}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_loop_config cfg={.ctx=&f,.request=next_provider,.finish_turn=next_finish,
        .prepare_next_turn_update=next_messages,.event=next_event};
    pangu_message prompt={.role=PANGU_USER};
    CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
    CHECK(f.requests==2 && f.prepared==1 && f.ends==1);
    CHECK(pangu_core_message_count(c)==5 && pangu_core_message_at(c,4)->thinking_level==PANGU_THINKING_HIGH); pangu_core_destroy(c);
    if(trace)printf("next:%d:%d:%d:prepared,queued:5\n",f.requests,f.prepared,f.ends);
}
static void next_failure_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    next_fixture *f=ctx; (void)owner;
    if (event->kind==PANGU_TURN_START && ++f->turns==2) f->allocator->fail=f->allocator->calls+1;
}
static void next_preparation_retry(void) {
    for (int discard=0;discard<2;++discard) {
        memory mem={0}; next_fixture f={.allocator=&mem}; pangu_core *c=NULL;
        pangu_core_config allocation={.allocator={&mem,allocate,release}};
        CHECK(pangu_core_create(&allocation,&c)==PANGU_OK);
        pangu_loop_config cfg={.ctx=&f,.request=next_provider,.finish_turn=next_finish,
            .prepare_next_turn_update=next_messages,.event=next_failure_event};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_NOMEM);
        CHECK(pangu_core_message_count(c)==2 && f.requests==1 && f.prepared==1);
        CHECK(pangu_core_prepared_count(c)==1 && same(pangu_core_prepared_at(c,0)->content[0].data,"prepared"));
        CHECK(pangu_core_queue_count(c,PANGU_STEERING)==1 && same(pangu_core_queue_at(c,PANGU_STEERING,0)->content[0].data,"queued"));
        mem.fail=0; cfg.event=NULL;
        if (discard) {
            CHECK(pangu_core_reset(c)==PANGU_OK);
            CHECK(!pangu_core_prepared_count(c) && !pangu_core_queue_count(c,PANGU_STEERING));
        } else {
            CHECK(pangu_core_run(c,NULL,0,&cfg)==PANGU_OK && f.requests==2);
            CHECK(!pangu_core_prepared_count(c) && !pangu_core_queue_count(c,PANGU_STEERING));
            CHECK(pangu_core_message_count(c)==5);
        }
        pangu_core_destroy(c); CHECK(!mem.live);
    }
}
typedef struct { int requests,effects,prepares,fail; } tools_fixture;
static pangu_status updated_validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)ctx; CHECK(same(schema,"{}") && same(args,"{}")); return PANGU_OK;
}
static pangu_status updated_execute(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    tools_fixture *f=ctx; (void)owner; CHECK(same(call->name,"replacement")); ++f->effects;
    pangu_message result={.role=PANGU_TOOL_RESULT}; return sink(target,&result,1);
}
static pangu_status update_tools(void *ctx,pangu_core *owner,pangu_request_update_sink sink,void *target) {
    tools_fixture *f=ctx; (void)owner;
    if (++f->prepares>1) {
        CHECK(pangu_core_active_thinking(owner)==PANGU_THINKING_MAX);
        pangu_request_update off={.fields=PANGU_UPDATE_THINKING,.thinking_level=PANGU_THINKING_OFF};
        return sink(target,&off);
    }
    char name[]="replacement",schema[]="{}";
    pangu_tool tool={.ctx=f,.name={name,sizeof(name)-1},.schema_json={schema,2},
        .validate=updated_validate,.execute=updated_execute};
    char model[]="new-model",metadata[]="{\"capable\":true}";
    pangu_request_update update={.fields=PANGU_UPDATE_TOOLS|PANGU_UPDATE_MODEL|PANGU_UPDATE_THINKING,
        .tools=&tool,.tool_count=1,.model={.id={model,sizeof(model)-1},.provider=S("script"),.api=S("fixture"),
            .metadata_json={metadata,sizeof(metadata)-1}},.thinking_level=PANGU_THINKING_MAX};
    pangu_status status=sink(target,&update);
    memset(name,'?',sizeof(name)-1); memset(schema,'?',2);
    memset(model,'?',sizeof(model)-1); memset(metadata,'?',sizeof(metadata)-1);
    return f->fail?PANGU_MODEL_ERROR:status;
}
static pangu_status tools_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    tools_fixture *f=ctx;
    CHECK(pangu_core_active_tool_count(owner)==1 && !pangu_core_active_tool_at(owner,1));
    const pangu_tool *active=pangu_core_active_tool_at(owner,0);
    CHECK(active && same(active->name,"replacement") && same(active->schema_json,"{}") && active->execute==updated_execute && active->ctx==f);
    const pangu_model_config *model=pangu_core_active_model(owner);
    CHECK(model && same(model->id,"new-model") && same(model->provider,"script") && same(model->api,"fixture"));
    CHECK(same(model->metadata_json,"{\"capable\":true}"));
    CHECK(pangu_core_active_thinking(owner)==(f->requests?PANGU_THINKING_OFF:PANGU_THINKING_MAX));
    pangu_content call={.kind=PANGU_TOOL_CALL,.name=S("replacement"),.id=S("one"),.data=S("{}")};
    pangu_message message={.role=PANGU_ASSISTANT,.content=&call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
    if (++f->requests==2) {
        const pangu_message *last=pangu_core_message_at(view,pangu_core_message_count(view)-1);
        CHECK(pangu_core_message_at(view,1)->role==PANGU_ASSISTANT);
        CHECK(pangu_core_message_at(view,1)->thinking_level==PANGU_THINKING_MAX);
        CHECK(last->role==PANGU_SYSTEM && last->tools_added_count==1);
        CHECK(same(last->tools_added[0].name,"replacement"));
        message.content=NULL; message.content_count=0; message.stop_reason=PANGU_STOP;
    }
    return sink(target,&message,1);
}
static void active_tools_end(void *ctx,pangu_core *owner,const pangu_event *event) {
    tools_fixture *f=ctx;
    if (event->kind!=PANGU_AGENT_END) return;
    size_t count=pangu_core_active_tool_count(owner);
    CHECK(count<=1);
    if (f->fail) CHECK(!count);
    if (f->requests) CHECK(count==1);
    if (count) {
        const pangu_tool *tool=pangu_core_active_tool_at(owner,0);
        CHECK(tool && same(tool->name,"replacement") && same(tool->schema_json,"{}") && tool->ctx==f);
    }
}
static void tool_updates(void) {
    for (int fail=0;fail<2;++fail) {
        tools_fixture f={.fail=fail}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        pangu_loop_config cfg={.ctx=&f,.request=tools_provider,.prepare_request_update=update_tools,.event=active_tools_end};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==(fail?PANGU_MODEL_ERROR:PANGU_OK));
        CHECK(f.requests==(fail?0:2) && f.effects==(fail?0:1));
        if (!fail) CHECK(pangu_core_message_at(c,pangu_core_message_count(c)-1)->thinking_level==PANGU_THINKING_OFF);
        CHECK(!pangu_core_active_model(c) && pangu_core_active_thinking(c)==PANGU_THINKING_OFF);
        CHECK(!cfg.tools && !cfg.tool_count); /* caller configuration untouched */
        CHECK(!pangu_core_active_tool_count(c) && !pangu_core_active_tool_at(c,0));
        CHECK(!pangu_core_is_running(c)); pangu_core_destroy(c);
    }
}
static void tool_update_failures(void) {
    int completed=0;
    for (size_t fail=1;fail<200;++fail) {
        memory mem={0}; pangu_core *c=NULL; tools_fixture f={0};
        pangu_core_config allocation={.allocator={&mem,allocate,release}};
        CHECK(pangu_core_create(&allocation,&c)==PANGU_OK);
        pangu_loop_config cfg={.ctx=&f,.request=tools_provider,.prepare_request_update=update_tools,.event=active_tools_end};
        pangu_message prompt={.role=PANGU_USER}; mem.fail=mem.calls+fail;
        pangu_status status=pangu_core_run(c,&prompt,1,&cfg);
        CHECK(status==PANGU_OK || status==PANGU_NOMEM);
        CHECK(!pangu_core_is_running(c));
        if (status==PANGU_OK) completed=1;
        pangu_core_destroy(c); CHECK(mem.live==0);
        if (completed) break;
    }
    CHECK(completed);
}
static pangu_status low_provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)owner; (void)request; ++*(int *)ctx;
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP}; return sink(target,&reply,1);
}
static void default_exports(int trace) {
    /* The two source streaming wrappers share the native synchronous entry. */
    for(int entry=0;entry<5;++entry)for(int explicit=0;explicit<2;++explicit){
        pangu_core *c=NULL;int fallback=0,override=0;
        CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        CHECK(pangu_core_set_default_stream(c,low_provider,&fallback)==PANGU_OK);
        pangu_message user={.role=PANGU_USER};
        pangu_loop_config config={.ctx=&override};
        if(explicit)config.request=low_provider;
        int continuation=entry==2 || entry==4;
        if(continuation)CHECK(pangu_core_append(c,&user)==PANGU_OK);
        pangu_status status=entry==0?pangu_core_run(c,&user,1,&config):
            continuation?pangu_core_loop_continue(c,&config):pangu_core_loop(c,&user,1,&config);
        CHECK(status==PANGU_OK && fallback==!explicit && override==explicit);
        CHECK(pangu_core_message_count(c)==2);
        const pangu_message *last=pangu_core_message_at(c,1);
        CHECK(last->role==PANGU_ASSISTANT && last->stop_reason==PANGU_STOP);
        if(trace)printf("default:%d:%d:%d:%d:%d\n",entry,explicit,fallback,override,(int)last->role);
        pangu_core_destroy(c);
    }
}
static void low_level_entries(void) {
    pangu_core *c=NULL; int calls=0; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_loop_config cfg={.ctx=&calls,.request=low_provider};
    CHECK(pangu_core_loop_continue(c,&cfg)==PANGU_INVALID);
    CHECK(pangu_core_run(c,NULL,0,&cfg)==PANGU_INVALID);
    CHECK(pangu_core_loop(c,NULL,0,&cfg)==PANGU_OK && calls==1);
    pangu_message user={.role=PANGU_USER};
    CHECK(pangu_core_enqueue(c,PANGU_STEERING,&user)==PANGU_OK);
    CHECK(pangu_core_loop_continue(c,&cfg)==PANGU_INVALID);
    CHECK(pangu_core_clear(c)==PANGU_OK);
    pangu_message system={.role=PANGU_SYSTEM}; CHECK(pangu_core_append(c,&system)==PANGU_OK);
    CHECK(pangu_core_run(c,NULL,0,&cfg)==PANGU_INVALID);
    CHECK(pangu_core_loop_continue(c,&cfg)==PANGU_OK && calls==2);
    CHECK(pangu_core_loop(c,NULL,0,&cfg)==PANGU_OK && calls==3);
    pangu_core_destroy(c);
}
static pangu_status identity_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)view; int mode=*(int *)ctx;
    const pangu_client_identity *i=pangu_core_active_identity(owner);
    CHECK(i && (i->fields&PANGU_IDENTITY_ATTRIBUTION));
    if (!mode) CHECK(i->attribution_enabled && same(i->name,"PanGu") && i->category_count==1 && same(i->categories[0],"cli-agent"));
    if (mode==1) CHECK(!i->attribution_enabled && (i->fields&PANGU_IDENTITY_VERSION) && same(i->version,"host-version"));
    if (mode==2) CHECK(i->attribution_enabled && same(i->name,"Embedded") && !i->category_count && same(i->custom_user_agent,"Host/1"));
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP}; return sink(target,&reply,1);
}
static void identities(void) {
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    CHECK(!pangu_core_active_identity(c) && same(pangu_core_default_identity()->name,"PanGu"));
    int mode=0; pangu_loop_config cfg={.ctx=&mode,.request=identity_provider};
    CHECK(pangu_core_loop(c,NULL,0,&cfg)==PANGU_OK);
    CHECK(!pangu_core_active_identity(c));
    mode=1; cfg.identity=(pangu_client_identity){.fields=PANGU_IDENTITY_ATTRIBUTION|PANGU_IDENTITY_VERSION,.version=S("host-version")};
    CHECK(pangu_core_loop(c,NULL,0,&cfg)==PANGU_OK);
    mode=2; cfg.identity=(pangu_client_identity){.fields=PANGU_IDENTITY_ATTRIBUTION|PANGU_IDENTITY_USER_AGENT,.attribution_enabled=1,.name=S("Embedded"),.user_agent=PANGU_USER_AGENT_CUSTOM,.custom_user_agent=S("Host/1")};
    CHECK(pangu_core_loop(c,NULL,0,&cfg)==PANGU_OK);
    cfg.identity.category_count=1; CHECK(pangu_core_loop(c,NULL,0,&cfg)==PANGU_INVALID);
    CHECK(pangu_core_clear(c)==PANGU_OK); mode=0; cfg.identity=(pangu_client_identity){0};
    CHECK(pangu_core_loop(c,NULL,0,&cfg)==PANGU_OK);
    pangu_core_destroy(c);
}
typedef struct {
    memory mem;
    int mode,phase,resolves,calls,wiped,trace,resolving;
    const char *secret;
    size_t secret_len;
} key_fixture;
static void *key_allocate(void *ctx,size_t n) {
    key_fixture *f=ctx; void *p=allocate(&f->mem,n);
    if (p && f->resolving) { f->secret=p; f->secret_len=n; }
    return p;
}
static void key_release(void *ctx,void *p) {
    key_fixture *f=ctx;
    if (p && p==f->secret) {
        for (size_t i=0;i<f->secret_len;++i) CHECK(((const unsigned char *)p)[i]==0);
        ++f->wiped; f->secret=NULL;
    }
    release(&f->mem,p);
}
static pangu_status key_transform(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *out) {
    key_fixture *f=ctx; CHECK(!pangu_core_active_api_key(owner).len); f->phase=1;
    for (size_t i=0;i<pangu_core_message_count(input);++i) {
        pangu_status status=pangu_core_append(out,pangu_core_message_at(input,i)); if (status!=PANGU_OK) return status;
    }
    return PANGU_OK;
}
static pangu_status key_convert(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *out) {
    key_fixture *f=ctx; CHECK(f->phase==1 && !pangu_core_active_api_key(owner).len);
    pangu_status status=key_transform(ctx,owner,input,out); f->phase=2; return status;
}
static pangu_status key_resolve(void *ctx,pangu_core *owner,pangu_slice provider,pangu_api_key_sink sink,void *target) {
    key_fixture *f=ctx; CHECK(f->phase==2 && same(provider,"mock") && !pangu_core_active_api_key(owner).len); ++f->resolves;
    if (f->trace) printf("resolve:%d\n",f->resolves);
    if (f->mode==2) return PANGU_OK;
    if (f->mode==5) return sink(target,(pangu_slice){NULL,1});
    char key[]={'k',(char)('0'+f->resolves),'!'};
    f->resolving=1;
    pangu_status status=sink(target,f->mode==1?(pangu_slice){0}:(pangu_slice){key,sizeof(key)});
    f->resolving=0;
    memset(key,'X',sizeof(key)); /* Core must own the accepted copy. */
    if (status!=PANGU_OK) return status;
    if (f->mode==3) return PANGU_MODEL_ERROR;
    if (f->mode==4) return sink(target,S("again"));
    return PANGU_OK;
}
static pangu_status key_provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)request; key_fixture *f=ctx; CHECK(f->phase==2); ++f->calls;
    pangu_slice key=pangu_core_active_api_key(owner);
    if (f->mode==0) {
        CHECK(key.len==3 && key.data[0]=='k' && key.data[1]=='0'+f->calls && key.data[2]=='!');
        f->secret=key.data; f->secret_len=key.len;
    } else CHECK(same(key,"fallback"));
    if (f->trace) printf("provider:%s\n",f->mode==0?"resolved":"fallback");
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP}; return sink(target,&reply,1);
}
static pangu_turn_decision key_finish(void *ctx,pangu_core *owner,const pangu_message *reply) {
    (void)reply; key_fixture *f=ctx; CHECK(!pangu_core_active_api_key(owner).len);
    CHECK(f->mode<3 || f->mode>5);
    return f->mode==0 && f->calls==1?PANGU_TURN_CONTINUE:PANGU_TURN_END_RUN;
}
static void key_requests(int trace) {
    for (int mode=0;mode<7;++mode) {
        key_fixture f={.mode=mode,.trace=trace};
        pangu_core_config memory_cfg={.allocator={&f,key_allocate,key_release}};
        pangu_core *owner=NULL; CHECK(pangu_core_create(&memory_cfg,&owner)==PANGU_OK);
        pangu_loop_config cfg={.ctx=&f,.request=key_provider,.transform_context=key_transform,.convert_to_model=key_convert,
            .get_api_key=mode==6?NULL:key_resolve,.api_key=S("fallback"),.model_config={.provider=S("mock")},.finish_turn=key_finish};
        pangu_message prompt={.role=PANGU_USER};
        pangu_status status=pangu_core_run(owner,&prompt,1,&cfg);
        CHECK(status==(mode==3?PANGU_MODEL_ERROR:(mode==4 || mode==5)?PANGU_INVALID:PANGU_OK));
        CHECK(!pangu_core_active_api_key(owner).len);
        if (mode==0) CHECK(f.calls==2 && f.resolves==2 && f.wiped==2);
        else if (mode>=3 && mode<=5) { CHECK(!f.calls); if (mode<5) CHECK(f.wiped==1); }
        else CHECK(f.calls==1);
        pangu_core_destroy(owner); CHECK(!f.mem.live);
        if (trace && mode==2) break;
    }
    if (trace) return;
    int completed=0;
    for (size_t fail=1;fail<400;++fail) {
        key_fixture f={0}; pangu_core_config mc={.allocator={&f,key_allocate,key_release}};
        pangu_core *owner=NULL; CHECK(pangu_core_create(&mc,&owner)==PANGU_OK);
        pangu_loop_config cfg={.ctx=&f,.request=key_provider,.transform_context=key_transform,.convert_to_model=key_convert,
            .get_api_key=key_resolve,.api_key=S("fallback"),.model_config={.provider=S("mock")},.finish_turn=key_finish};
        pangu_message prompt={.role=PANGU_USER}; f.mem.fail=f.mem.calls+fail;
        pangu_status status=pangu_core_run(owner,&prompt,1,&cfg); CHECK(status==PANGU_OK || status==PANGU_NOMEM);
        CHECK(!pangu_core_active_api_key(owner).len);
        pangu_core_destroy(owner); CHECK(!f.mem.live);
        if (status==PANGU_OK) { completed=1; break; }
    }
    CHECK(completed);
}
static pangu_status invalid_config_provider(void *ctx,pangu_core *owner,const pangu_core *input,pangu_model_sink sink,void *target) {
    (void)owner; (void)input; (void)sink; (void)target; ++*(int *)ctx; return PANGU_MODEL_ERROR;
}
static void invalid_model_config(void) {
    for (int field=0;field<4;++field) for (int entry=0;entry<3;++entry) {
        pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        pangu_message user={.role=PANGU_USER};
        CHECK(pangu_core_append(owner,&user)==PANGU_OK);
        int calls=0;
        pangu_loop_config cfg={.ctx=&calls,.request=invalid_config_provider};
        pangu_slice *fields[]={&cfg.model_config.id,&cfg.model_config.provider,&cfg.model_config.api,&cfg.model_config.metadata_json};
        *fields[field]=(pangu_slice){NULL,1};
        pangu_status status=entry==0?pangu_core_run(owner,&user,1,&cfg):
            entry==1?pangu_core_loop(owner,&user,1,&cfg):pangu_core_loop_continue(owner,&cfg);
        CHECK(status==PANGU_INVALID && !calls);
        CHECK(pangu_core_message_count(owner)==1 && !pangu_core_is_running(owner));
        CHECK(!pangu_core_active_model(owner) && !pangu_core_error(owner).len);
        pangu_core_destroy(owner);
    }
}
/* agent.test.ts:212/247: executable tools override pending declarations without
 * losing unrelated sections, timestamps, or creating an extra system message. */
typedef struct { int mode,requests; } pending_fixture;
static pangu_status pending_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    pending_fixture *f=ctx; ++f->requests; (void)owner;
    CHECK(pangu_core_message_count(view)==3);
    const pangu_message *m=pangu_core_message_at(view,1);
    CHECK(m->role==PANGU_SYSTEM && m->timestamp_ms==1 && m->section_count==1);
    CHECK(same(m->sections[0].name,f->mode?"note":"skills"));
    CHECK(same(m->sections[0].value,f->mode?"<note>x</note>":"<skills>x</skills>"));
    CHECK(m->tools_added_count==(size_t)!f->mode && !m->tools_removed_count);
    if(!f->mode)CHECK(same(m->tools_added[0].name,"echo"));
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&reply,1);
}
static void pending_declarations(int trace) {
    for(int mode=0;mode<2;++mode) {
        pangu_core *c=NULL,*snapshot=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        pangu_content base={.kind=PANGU_TEXT,.data=S("You are helpful.")};
        pangu_tool_declaration first={.name=S("first"),.description=S("First"),.schema_json=S("{}")};
        pangu_message initial={.role=PANGU_SYSTEM,.content=&base,.content_count=1,
            .tools_added=mode?&first:NULL,.tools_added_count=(size_t)mode};
        CHECK(pangu_core_append(c,&initial)==PANGU_OK);
        pangu_system_section section={.name=mode?S("note"):S("skills"),.value=mode?S("<note>x</note>"):S("<skills>x</skills>")};
        pangu_tool_declaration false_tool={.name=S("second"),.schema_json=S("{}")};
        pangu_slice removed=S("first");
        pangu_content user={.kind=PANGU_TEXT,.data=S("hi")};
        pangu_message input[]={
            {.role=PANGU_SYSTEM,.sections=&section,.section_count=1,.timestamp_ms=1,
             .tools_added=mode?&false_tool:NULL,.tools_added_count=(size_t)mode,
             .tools_removed=mode?&removed:NULL,.tools_removed_count=(size_t)mode},
            {.role=PANGU_USER,.content=&user,.content_count=1,.timestamp_ms=2}};
        pangu_tool tool={.name=mode?S("first"):S("echo"),.description=mode?S("First"):S("Echo input"),.schema_json=S("{}")};
        pending_fixture f={.mode=mode}; pangu_loop_config cfg={.ctx=&f,.request=pending_provider,.tools=&tool,.tool_count=1};
        CHECK(pangu_core_run(c,input,2,&cfg)==PANGU_OK && f.requests==1);
        CHECK(pangu_core_message_count(c)==4 && pangu_core_system_snapshot(c,&snapshot)==PANGU_OK);
        const pangu_message *system=pangu_core_message_at(snapshot,0),*pending=pangu_core_message_at(c,1);
        CHECK(system->tools_added_count==1 && same(system->tools_added[0].name,mode?"first":"echo"));
        CHECK(input[0].tools_added_count==(size_t)mode && input[0].tools_removed_count==(size_t)mode);
        if(trace)printf("pending:%d:2:%zu:%zu:%.*s:%lld:%.*s\n",mode,pending->tools_added_count,pending->tools_removed_count,
            (int)pending->sections[0].name.len,pending->sections[0].name.data,(long long)pending->timestamp_ms,
            (int)system->tools_added[0].name.len,system->tools_added[0].name.data);
        pangu_core_destroy(snapshot);pangu_core_destroy(c);
    }
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--trace-pending-declarations")) { pending_declarations(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-transcript-context")) { transcript_only_context(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-next")) { next_preparation(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-initial")) { initial_preparation(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-projection")) { projection_scenarios(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-tails")) { continuation_tails(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-controls")) { control_combinations(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-late-steering")) { prepare_late_steering(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-default-exports")) { default_exports(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-keys")) { key_requests(1); return 0; }
    pending_declarations(0); invalid_model_config(); key_requests(0);
    pangu_core *c=NULL,*copy=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK); seed(c);
    CHECK(pangu_core_system_snapshot(c,&copy)==PANGU_OK); baseline(copy);
    const pangu_message *m=pangu_core_message_at(copy,0);
    printf("replay:%.*s;%.*s=%.*s,%.*s=%.*s;%.*s,%.*s\n",
        (int)m->content[0].data.len,m->content[0].data.data,
        (int)m->sections[0].name.len,m->sections[0].name.data,(int)m->sections[0].value.len,m->sections[0].value.data,
        (int)m->sections[1].name.len,m->sections[1].name.data,(int)m->sections[1].value.len,m->sections[1].value.data,
        (int)m->tools_added[0].name.len,m->tools_added[0].name.data,(int)m->tools_added[1].name.len,m->tools_added[1].name.data);
    CHECK(pangu_core_clear(c)==PANGU_OK); baseline(copy);
    pangu_core_destroy(copy); pangu_core_destroy(c);
    transcript_only_context(0); initial_preparation(0); projection_scenarios(0); continuation_tails(0); control_combinations(0); prepare_late_steering(0); default_exports(0); identities(); low_level_entries(); next_preparation_retry(); tool_update_failures(); tool_updates(); next_preparation(0); continuation_messages(); preparation_updates(); default_stream(); reset_failures(); requests(); schema_comparison(); return 0;
}
