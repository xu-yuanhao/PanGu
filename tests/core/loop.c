#include <pangu/core/loop.h>
#include <pangu/core/state.h>
#include <pangu/core/proxy_request.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed at %d: %s\n",__LINE__,#x); abort(); } } while (0)
#define S(x) (pangu_slice){x, sizeof(x)-1}
typedef struct {
    int requests, effects, validation, before, after, finish;
    int mode;
    pangu_event_kind events[256]; size_t n;
} fixture;
enum { NORMAL, UNKNOWN, INVALID_ARGS, BLOCKED, TRUNCATED, CANCEL_BEFORE,
    MODEL_FAIL, MODEL_ABORT, TERMINATE, FINISH_CONTINUE, NO_FINAL, PARTIAL_FAIL, PARTIAL_ABORT, TERMINAL_ABORT };
static pangu_status model(void *ctx, pangu_core *c, pangu_model_sink sink, void *target) {
    fixture *f=ctx; ++f->requests;
    CHECK(pangu_core_is_running(c));
    CHECK(pangu_core_reset(c)==PANGU_BUSY);
    CHECK(pangu_core_append(c,NULL)==PANGU_BUSY);
    pangu_content b={.kind=PANGU_TEXT,.data=S("thinking...")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1,.stop_reason=PANGU_PENDING,
        .model=S("mock"),.provider=S("mock"),.api=S("mock"),
        .usage={.has_cache_write_1h=1,.has_reasoning=1,.has_total_from_provider=1}};
    if (f->mode==MODEL_FAIL) return PANGU_MODEL_ERROR;
    if (f->mode==MODEL_ABORT) { pangu_core_cancel(c); return PANGU_CANCELLED; }
    pangu_status update=sink(target,&m,0);
    if (update!=PANGU_OK) return update;
    if (f->mode==NO_FINAL) return PANGU_OK;
    if (f->mode==PARTIAL_FAIL) return PANGU_MODEL_ERROR;
    if (f->mode==PARTIAL_ABORT || f->mode==TERMINAL_ABORT) {
        pangu_core_cancel(c);
        if (f->mode==PARTIAL_ABORT) return PANGU_CANCELLED;
        m.stop_reason=PANGU_ABORTED; m.error=S("provider aborted");
        m.provider_thinking_level=S("native-high"); m.usage.total=42;
        return sink(target,&m,1);
    }
    if (f->requests==1 && f->mode!=FINISH_CONTINUE) {
        b=(pangu_content){.kind=PANGU_TOOL_CALL,.data=S("{}"),.id=S("call-1"),.name=S("echo")};
        if (f->mode==UNKNOWN) b.name=S("missing");
        m.stop_reason=f->mode==TRUNCATED?PANGU_LENGTH:PANGU_TOOL_USE;
    } else {
        if (f->mode!=FINISH_CONTINUE) {
            const pangu_message *result=pangu_core_message_at(c,pangu_core_message_count(c)-1);
            CHECK(result->role==PANGU_TOOL_RESULT);
            CHECK(result->tool_call_id.len==6);
            if(f->mode==NORMAL || f->mode==TERMINATE)
                CHECK(result->nested_calls_json.len==28 &&
                      !memcmp(result->nested_calls_json.data,"{\"calls\":[],\"complete\":true}",28));
            CHECK(result->is_error==(f->mode!=NORMAL && f->mode!=TERMINATE));
        }
        b.data=S("done"); m.stop_reason=PANGU_STOP;
    }
    return sink(target,&m,1);
}
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args) {
    fixture *f=ctx; ++f->validation;
    CHECK(schema.len==2 && args.len==2);
    return f->mode==INVALID_ARGS?PANGU_INVALID:PANGU_OK;
}
static pangu_status execute(void *ctx,pangu_core *c,const pangu_content *call,pangu_tool_sink sink,void *target) {
    fixture *f=ctx; (void)c; CHECK(call->id.len==6); ++f->effects;
    pangu_content b={.kind=PANGU_TEXT,.data=S("ok")};
    char nested[]="{\"calls\":[],\"complete\":true}";
    pangu_message m={.role=PANGU_TOOL_RESULT,.content=&b,.content_count=1,.terminate=f->mode==TERMINATE,
        .nested_calls_json={nested,sizeof(nested)-1}};
    pangu_status update=sink(target,&m,0);
    if (update!=PANGU_OK) return update;
    pangu_status status=sink(target,&m,1);
    memset(nested,'!',sizeof(nested));
    return status;
}
static pangu_before_result before(void *ctx,pangu_core *c,const pangu_message *assistant,const pangu_content *call) {
    fixture *f=ctx; ++f->before; CHECK(assistant->role==PANGU_ASSISTANT && call);
    if (f->mode==CANCEL_BEFORE) pangu_core_cancel(c);
    return (pangu_before_result){.block=f->mode==BLOCKED,.reason=S("Denied")};
}
static pangu_status after(void *ctx,pangu_core *c,const pangu_content *call,const pangu_message *result,pangu_tool_sink sink,void *target) {
    fixture *f=ctx; (void)c; (void)call; (void)sink; (void)target;
    ++f->after; CHECK(result->content_count==1); return PANGU_OK;
}
static pangu_turn_decision finish(void *ctx,pangu_core *c,const pangu_message *m) {
    fixture *f=ctx; (void)c; CHECK(m->role==PANGU_ASSISTANT); ++f->finish;
    if (f->mode==FINISH_CONTINUE && f->requests==1) return PANGU_TURN_CONTINUE;
    return PANGU_TURN_AUTO;
}
static pangu_turn_decision finish_context(void *ctx,pangu_core *owner,const pangu_turn_context *turn) {
    CHECK(turn->assistant && turn->new_messages && turn->tool_results);
    size_t count=pangu_core_message_count(turn->tool_results);
    if (turn->assistant->stop_reason==PANGU_ERROR || turn->assistant->stop_reason==PANGU_ABORTED) CHECK(count==0);
    else {
        int requested=0;
        for (size_t i=0;i<turn->assistant->content_count;++i) requested+=turn->assistant->content[i].kind==PANGU_TOOL_CALL;
        CHECK(count==(size_t)requested);
    }
    return finish(ctx,owner,turn->assistant);
}
static void event(void *ctx,pangu_core *c,const pangu_event *e) {
    fixture *f=ctx; CHECK(f->n<256); f->events[f->n++]=e->kind;
    CHECK(pangu_core_is_running(c));
    if (e->kind==PANGU_AGENT_END) CHECK(e->message_count==pangu_core_message_count(c));
}
typedef struct { size_t calls, fail, live, large; } memory;
static void *allocate(void *ctx,size_t n) {
    memory *m=ctx;
    if (n>=4096) ++m->large;
    if (++m->calls==m->fail) return NULL;
    void *p=malloc(n); if (p) ++m->live; return p;
}
static void release(void *ctx,void *p) {
    if (p) { --((memory *)ctx)->live; free(p); }
}
static void allocation_failures(void) {
    int reached_success=0;
    for (size_t fail=1;fail<250;++fail) {
        memory mem={.fail=fail}; fixture f={0}; pangu_core *c=NULL;
        pangu_core_config memory_config={.allocator={&mem,allocate,release}};
        pangu_status s=pangu_core_create(&memory_config,&c);
        if (s==PANGU_OK) {
            pangu_tool t={.name=S("echo"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.execute=execute};
            pangu_loop_config cfg={.ctx=&f,.model=model,.event=event,.tools=&t,.tool_count=1,
                .before_tool=before,.after_tool=after,.finish_turn_context=finish_context,.tool_execution=PANGU_TOOLS_SEQUENTIAL};
            pangu_content b={.kind=PANGU_TEXT,.data=S("hello")};
            pangu_message prompt={.role=PANGU_USER,.content=&b,.content_count=1};
            s=pangu_core_run(c,&prompt,1,&cfg);
            CHECK(s==PANGU_OK || s==PANGU_NOMEM);
            CHECK(!pangu_core_is_running(c));
            CHECK(f.events[f.n-1]==PANGU_AGENT_END);
            if (s==PANGU_OK) reached_success=1;
            mem.fail=0;
            CHECK(pangu_core_reset(c)==PANGU_OK);
        } else CHECK(s==PANGU_NOMEM);
        pangu_core_destroy(c); CHECK(!mem.live);
        if (reached_success) break;
    }
    CHECK(reached_success);
}
typedef struct { char *input; size_t len; } long_fixture;
static pangu_status long_model(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)owner; long_fixture *f=ctx;
    memset(f->input,'X',f->len);
    const pangu_message *user=pangu_core_message_at(request,0);
    CHECK(user && user->role==PANGU_USER && user->content_count==3);
    for (size_t i=0;i<2;++i) {
        pangu_slice data=user->content[i].data;
        CHECK(data.len==f->len && data.data[0]=='A' && data.data[f->len-1]=='Z');
    }
    CHECK(user->content[2].kind==PANGU_JSON);
    pangu_content blocks[]={{.kind=PANGU_TEXT,.data=user->content[0].data},
        {.kind=PANGU_THINKING,.has_redacted=1,.redacted=1,.signature=S("opaque")}};
    char revision[]="concrete-model",identity[]="response-id",reason[]="native-stop";
    char diagnostics[]="[]",deferred[]="{\"provider\":\"fixture\",\"modelId\":\"concrete-model\",\"api\":\"mock\",\"id\":\"pending-id\",\"data\":null}";
    pangu_message reply={.role=PANGU_ASSISTANT,.content=blocks,.content_count=2,.stop_reason=PANGU_DEFERRED,
        .response_model={revision,sizeof(revision)-1},.response_id={identity,sizeof(identity)-1},
        .raw_stop_reason={reason,sizeof(reason)-1},.diagnostics_json={diagnostics,sizeof(diagnostics)-1},
        .deferred_json={deferred,sizeof(deferred)-1},.has_end_turn=1};
    pangu_status status=sink(target,&reply,0);
    if (status==PANGU_OK) status=sink(target,&reply,0);
    if (status==PANGU_OK) status=sink(target,&reply,1);
    memset(revision,'!',sizeof(revision)); memset(identity,'!',sizeof(identity)); memset(reason,'!',sizeof(reason));
    memset(diagnostics,'!',sizeof(diagnostics));memset(deferred,'!',sizeof(deferred));
    return status;
}
static void long_messages(void) {
    long_fixture f={.len=1024*1024}; f.input=malloc(f.len); CHECK(f.input);
    memset(f.input,'A',f.len); f.input[f.len-1]='Z';
    pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    pangu_content blocks[]={{.kind=PANGU_TEXT,.data={f.input,f.len}},
        {.kind=PANGU_IMAGE,.data={f.input,f.len},.mime_type=S("image/png")},
        {.kind=PANGU_JSON,.data=S("{\"type\":\"metadata\",\"value\":true}")}};
    pangu_message prompt={.role=PANGU_USER,.content=blocks,.content_count=3};
    pangu_loop_config cfg={.ctx=&f,.request=long_model};
    CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK);
    free(f.input);
    CHECK(pangu_core_message_count(owner)==2);
    const pangu_message *reply=pangu_core_message_at(owner,1);
    CHECK(reply->content_count==2 && reply->content[0].data.len==f.len);
    CHECK(reply->content[0].data.data[0]=='A' && reply->content[0].data.data[f.len-1]=='Z');
    CHECK(reply->content[1].has_redacted && reply->content[1].redacted && reply->content[1].signature.len==6);
    CHECK(reply->response_model.len==14 && !memcmp(reply->response_model.data,"concrete-model",14));
    CHECK(reply->response_id.len==11 && !memcmp(reply->response_id.data,"response-id",11));
    CHECK(reply->raw_stop_reason.len==11 && !memcmp(reply->raw_stop_reason.data,"native-stop",11));
    CHECK(reply->stop_reason==PANGU_DEFERRED && reply->has_end_turn && !reply->end_turn);
    CHECK(reply->diagnostics_json.len==2 && !memcmp(reply->diagnostics_json.data,"[]",2));
    CHECK(reply->deferred_json.len>10 && !memcmp(reply->deferred_json.data,"{\"provider\"",11));
    CHECK(pangu_core_reset(owner)==PANGU_OK); pangu_core_destroy(owner);
}
typedef struct { memory mem; char *bytes; size_t size; } stream_memory;
static pangu_status pooled_model(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)request; stream_memory *f=ctx;
    pangu_content text={.kind=PANGU_TEXT,.data={f->bytes,0}};
    pangu_message message={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_PENDING};
    pangu_status status=sink(target,&message,0);
    for(size_t i=1;status==PANGU_OK && i<=128;++i){
        char mark=(char)('a'+i%26);f->bytes[0]=mark;text.data.len=i*(f->size/128);
        if(i==128)message.stop_reason=PANGU_STOP;
        status=sink(target,&message,i==128);
        f->bytes[0]='!'; /* Provider buffers cannot alias the owned snapshot. */
        if(status==PANGU_OK && i<128){
            const pangu_message *saved=pangu_core_streaming_message(owner);
            CHECK(saved && saved->content[0].data.len==text.data.len && saved->content[0].data.data[0]==mark);
        }
    }
    return status;
}
static void stream_allocation_regression(void) {
    stream_memory f={.size=1024*1024};f.bytes=malloc(f.size);CHECK(f.bytes);memset(f.bytes,'x',f.size);
    pangu_core_config alloc={.allocator={&f.mem,allocate,release}};pangu_core *owner=NULL;
    CHECK(pangu_core_create(&alloc,&owner)==PANGU_OK);
    pangu_loop_config config={.ctx=&f,.request=pooled_model};pangu_message prompt={.role=PANGU_USER};
    CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_OK);
    const pangu_message *reply=pangu_core_message_at(owner,1);
    CHECK(reply && reply->content[0].data.len==f.size && reply->content[0].data.data[0]=='a'+128%26);
    CHECK(f.mem.large<40); /* 128 cumulative snapshots must not allocate 128 large blocks. */
    pangu_core_destroy(owner);CHECK(!f.mem.live);free(f.bytes);
}
typedef struct { int mode,events[16],n,trace; } failure_fixture;
static pangu_status failed_provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)request; failure_fixture *f=ctx;
    if (f->mode==2 || f->mode==3 || f->mode==5) {
        pangu_content text={.kind=PANGU_TEXT,.data=S("retained partial")};
        pangu_message m={.role=PANGU_ASSISTANT,.model=S("actual-model"),.provider=S("actual-provider"),.api=S("actual-api"),
            .content=&text,.content_count=1,.usage={.total=42},.stop_reason=PANGU_PENDING};
        pangu_status status=sink(target,&m,0); if (status!=PANGU_OK) return status;
    }
    if (f->mode==1 || f->mode==3) pangu_core_cancel(owner);
    return f->mode%2 || f->mode>=4?PANGU_CANCELLED:PANGU_MODEL_ERROR;
}
static void failure_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    (void)owner; failure_fixture *f=ctx; CHECK(f->n<16); f->events[f->n++]=event->kind;
    if (event->kind!=PANGU_MESSAGE_END || event->message->role!=PANGU_ASSISTANT) return;
    const pangu_message *m=event->message;
    CHECK(m->stop_reason==((f->mode%2 || f->mode>=4)?PANGU_ABORTED:PANGU_ERROR));
    const char *diagnostic=f->mode%2 || f->mode>=4?"Operation aborted":"Model stream failed";
    CHECK(m->error.len==strlen(diagnostic) && !memcmp(m->error.data,diagnostic,m->error.len));
    if (f->mode<2 || f->mode==4) {
        CHECK(m->model.len==10 && !memcmp(m->model.data,"configured",10));
        CHECK(m->provider.len==8 && !memcmp(m->provider.data,"provider",8));
        CHECK(m->api.len==3 && !memcmp(m->api.data,"api",3));
    } else {
        CHECK(m->model.len==12 && !memcmp(m->model.data,"actual-model",12));
        CHECK(m->provider.len==15 && !memcmp(m->provider.data,"actual-provider",15));
        CHECK(m->api.len==10 && !memcmp(m->api.data,"actual-api",10));
        CHECK(m->usage.total==42 && m->content[0].data.len==16);
    }
}
static void failed_identities(int trace) {
    for (int mode=0;mode<6;++mode) {
        pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        failure_fixture f={.mode=mode,.trace=trace};
        pangu_loop_config cfg={.ctx=&f,.request=failed_provider,.event=failure_event,
            .model_config={.id=S("configured"),.provider=S("provider"),.api=S("api")},.thinking_level=PANGU_THINKING_HIGH};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==((mode%2 || mode>=4)?PANGU_CANCELLED:PANGU_MODEL_ERROR));
        CHECK(pangu_core_is_cancelled(owner)==(mode==1 || mode==3));
        const pangu_message *m=pangu_core_message_at(owner,1);
        CHECK(m && m->thinking_level==PANGU_THINKING_HIGH);
        CHECK(f.events[f.n-2]==PANGU_TURN_END && f.events[f.n-1]==PANGU_AGENT_END);
        if (trace) printf("failure:%d:%.*s:%.*s:%.*s:%d\n",mode,(int)m->model.len,m->model.data,
            (int)m->provider.len,m->provider.data,(int)m->api.len,m->api.data,(int)m->stop_reason);
        pangu_core_destroy(owner);
    }
}
typedef struct { int next,cancel,independent,requests,ends,turns,errors,n; int events[32]; const char *error; } prep_fixture;
static pangu_status prep_fail(void *ctx,pangu_core *owner) {
    prep_fixture *f=ctx; if (f->cancel && !f->independent) pangu_core_cancel(owner);
    return f->independent?PANGU_CANCELLED:PANGU_MODEL_ERROR;
}
static pangu_status prep_update_fail(void *ctx,pangu_core *owner,pangu_request_update_sink sink,void *target) {
    (void)sink; (void)target; return prep_fail(ctx,owner);
}
static pangu_status prep_provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)owner; (void)request; ++((prep_fixture *)ctx)->requests;
    pangu_message m={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&m,1);
}
static pangu_turn_decision prep_continue(void *ctx,pangu_core *owner,const pangu_message *m) {
    (void)ctx; (void)owner; (void)m; return PANGU_TURN_CONTINUE;
}
static void prep_event(void *ctx,pangu_core *owner,const pangu_event *e) {
    prep_fixture *f=ctx; CHECK(f->n<32); f->events[f->n++]=e->kind;
    CHECK(pangu_core_is_running(owner));
    if (e->kind==PANGU_MESSAGE_END && e->message->stop_reason== (f->cancel?PANGU_ABORTED:PANGU_ERROR)) {
        const pangu_message *m=e->message; ++f->errors;
        CHECK(m->timestamp_ms>0);
        CHECK(m->role==PANGU_ASSISTANT && m->model.len==10 && !memcmp(m->model.data,"configured",10));
        CHECK(m->content_count==1 && m->content[0].kind==PANGU_TEXT && !m->content[0].data.len);
        const char *error=f->cancel?"Operation aborted":f->error?f->error:"Request preparation failed";
        CHECK(m->error.len==strlen(error) && !memcmp(m->error.data,error,m->error.len));
    }
    if (e->kind==PANGU_TURN_END) {
        ++f->turns;
        CHECK(e->tool_results && !pangu_core_message_count(e->tool_results));
        CHECK(e->message_count==pangu_core_message_count(owner));
    }
    if (e->kind==PANGU_AGENT_END) {
        ++f->ends;
        if (f->errors) CHECK(e->message_count==1 && pangu_core_message_at(e->new_messages,0)->role==PANGU_ASSISTANT);
    }
}
static void preparation_failures(int trace) {
    for (int mode=0;mode<32;++mode) {
        prep_fixture f={.next=!!(mode&2),.cancel=!!((mode&1) || (mode&16)),.independent=!!(mode&16)};
        pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        pangu_loop_config cfg={.ctx=&f,.request=prep_provider,.event=prep_event,.finish_turn=prep_continue,
            .model_config={.id=S("configured"),.provider=S("provider"),.api=S("api")}};
        if (f.next) {
            if (mode&4) cfg.prepare_next_turn_update=prep_update_fail;
            else cfg.prepare_next_turn=prep_fail;
        } else {
            if (mode&4) cfg.prepare_request_update=prep_update_fail;
            else cfg.prepare_request=prep_fail;
        }
        pangu_message prompt={.role=PANGU_USER};
        pangu_status status=mode&8?pangu_core_loop(owner,&prompt,1,&cfg):pangu_core_run(owner,&prompt,1,&cfg);
        CHECK(status==((f.independent || (f.cancel && !(mode&8)))?PANGU_CANCELLED:PANGU_MODEL_ERROR));
        CHECK(pangu_core_is_cancelled(owner)==(f.cancel && !f.independent));
        CHECK(f.requests==f.next && f.ends==1 && f.errors==!(mode&8));
        CHECK(f.turns==f.next+!(mode&8));
        CHECK(pangu_core_message_count(owner)==(size_t)(1+f.next+!(mode&8)));
        if (!(mode&8)) {
            CHECK(f.events[f.n-4]==PANGU_MESSAGE_START && f.events[f.n-3]==PANGU_MESSAGE_END);
            CHECK(f.events[f.n-2]==PANGU_TURN_END && f.events[f.n-1]==PANGU_AGENT_END);
        }
        if (trace && mode<4) {
            printf("prep:%d:",mode);
            for (int j=0;j<f.n;++j) printf("%s%d",j?",":"",f.events[j]);
            printf(":%zu:%d\n",pangu_core_message_count(owner),f.cancel?PANGU_ABORTED:PANGU_ERROR);
        }
        pangu_core_destroy(owner);
    }
    int completed=0;
    for (size_t fail=1;fail<180 && !completed;++fail) {
        memory mem={.fail=fail}; pangu_core *owner=NULL; prep_fixture f={0};
        pangu_core_config alloc={.allocator={&mem,allocate,release}};
        pangu_status status=pangu_core_create(&alloc,&owner);
        if (status==PANGU_OK) {
            pangu_loop_config cfg={.ctx=&f,.request=prep_provider,.prepare_request=prep_fail,.event=prep_event};
            /* Identity assertions are valid on every successfully constructed error. */
            cfg.model_config.id=S("configured");
            pangu_message prompt={.role=PANGU_USER};
            status=pangu_core_run(owner,&prompt,1,&cfg);
            CHECK(status==PANGU_NOMEM || status==PANGU_MODEL_ERROR);
            CHECK(f.ends==1 && !pangu_core_is_running(owner));
            CHECK(pangu_core_message_count(owner)<=(size_t)(status==PANGU_NOMEM?1:2));
            completed=status==PANGU_MODEL_ERROR;
        } else CHECK(status==PANGU_NOMEM);
        pangu_core_destroy(owner); CHECK(!mem.live);
    }
    CHECK(completed);
}
static pangu_status failed_transform(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *output) {
    CHECK(pangu_core_message_count(input)==1);
    pangu_message temporary={.role=PANGU_CUSTOM,.custom_role=S("discarded")};
    pangu_status status=pangu_core_append(output,&temporary);
    return status==PANGU_OK?prep_fail(ctx,owner):status;
}
static pangu_status failed_key(void *ctx,pangu_core *owner,pangu_slice provider,pangu_api_key_sink sink,void *target) {
    (void)provider;
    pangu_status status=sink(target,S("temporary"));
    return status==PANGU_OK?prep_fail(ctx,owner):status;
}
static void request_hook_failures(int trace) {
    const char *errors[]={"Context transform failed","Message conversion failed","API key resolution failed"};
    for (int stage=0;stage<3;++stage) for (int mode=0;mode<8;++mode) {
        prep_fixture f={.cancel=!!((mode&1) || (mode&4)),.independent=!!(mode&4),.error=errors[stage]};
        pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        pangu_loop_config cfg={.ctx=&f,.request=prep_provider,.event=prep_event,.finish_turn=prep_continue,
            .model_config={.id=S("configured"),.provider=S("provider"),.api=S("api")}};
        if (stage==0) cfg.transform_context=failed_transform;
        if (stage==1) cfg.convert_to_model=failed_transform;
        if (stage==2) cfg.get_api_key=failed_key;
        pangu_message prompt={.role=PANGU_USER};
        pangu_status status=mode&2?pangu_core_loop(owner,&prompt,1,&cfg):pangu_core_run(owner,&prompt,1,&cfg);
        CHECK(status==((f.independent || (f.cancel && !(mode&2)))?PANGU_CANCELLED:PANGU_MODEL_ERROR));
        CHECK(pangu_core_is_cancelled(owner)==(f.cancel && !f.independent));
        CHECK(!f.requests && f.ends==1 && f.errors==!(mode&2) && f.turns==!(mode&2));
        CHECK(pangu_core_message_count(owner)==(size_t)(1+!(mode&2)));
        CHECK(!pangu_core_active_api_key(owner).len);
        if (trace && mode<2) {
            printf("hook:%d:%d:",stage,mode);
            for (int j=0;j<f.n;++j) printf("%s%d",j?",":"",f.events[j]);
            printf(":%zu:%d\n",pangu_core_message_count(owner),f.cancel?PANGU_ABORTED:PANGU_ERROR);
        }
        pangu_core_destroy(owner);
    }
    for (int stage=0;stage<3;++stage) {
        int completed=0;
        for (size_t fail=1;fail<180 && !completed;++fail) {
            memory mem={.fail=fail}; pangu_core *owner=NULL;
            prep_fixture f={.error=errors[stage]};
            pangu_core_config alloc={.allocator={&mem,allocate,release}};
            pangu_status status=pangu_core_create(&alloc,&owner);
            if (status==PANGU_OK) {
                pangu_loop_config cfg={.ctx=&f,.request=prep_provider,.event=prep_event,
                    .model_config={.id=S("configured")}};
                if (stage==0) cfg.transform_context=failed_transform;
                if (stage==1) cfg.convert_to_model=failed_transform;
                if (stage==2) cfg.get_api_key=failed_key;
                pangu_message prompt={.role=PANGU_USER};
                status=pangu_core_run(owner,&prompt,1,&cfg);
                CHECK(status==PANGU_NOMEM || status==PANGU_MODEL_ERROR);
                CHECK(!f.requests && f.ends==1 && !pangu_core_active_api_key(owner).len);
                CHECK(pangu_core_message_count(owner)<=(size_t)(status==PANGU_NOMEM?1:2));
                completed=status==PANGU_MODEL_ERROR;
            } else CHECK(status==PANGU_NOMEM);
            pangu_core_destroy(owner); CHECK(!mem.live);
        }
        CHECK(completed);
    }

}
typedef struct { int stage,cancel,requests; } diagnostic_fixture;
static pangu_status diagnostic_fail(void *ctx,pangu_core *owner) {
    diagnostic_fixture *f=ctx;char bytes[]="provider exploded";
    CHECK(pangu_core_report_error(owner,(pangu_slice){NULL,1})==PANGU_INVALID);
    pangu_status status=pangu_core_report_error(owner,S("superseded"));
    if(status!=PANGU_OK)return status;
    status=pangu_core_report_error(owner,(pangu_slice){bytes,sizeof(bytes)-1});
    memset(bytes,'x',sizeof(bytes)-1);
    if(status!=PANGU_OK)return status;
    if(f->cancel)pangu_core_cancel(owner);
    return PANGU_MODEL_ERROR;
}
static pangu_status diagnostic_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    diagnostic_fixture *f=ctx;++f->requests;
    if(f->stage==0)return diagnostic_fail(ctx,owner);
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};return sink(target,&reply,1);
}
static pangu_status diagnostic_transform(void *ctx,pangu_core *owner,const pangu_core *input,pangu_core *output) {
    (void)input;(void)output;return diagnostic_fail(ctx,owner);
}
static pangu_status diagnostic_key(void *ctx,pangu_core *owner,pangu_slice provider,pangu_arguments_sink sink,void *target) {
    (void)provider;(void)sink;(void)target;return diagnostic_fail(ctx,owner);
}
static pangu_turn_decision diagnostic_continue(void *ctx,pangu_core *owner,const pangu_message *reply) {
    (void)ctx;(void)owner;(void)reply;return PANGU_TURN_CONTINUE;
}
static void diagnostic_config(pangu_loop_config *cfg,diagnostic_fixture *f) {
    *cfg=(pangu_loop_config){.ctx=f,.model=diagnostic_model};
    if(f->stage==1)cfg->prepare_request=diagnostic_fail;
    if(f->stage==2){cfg->prepare_next_turn=diagnostic_fail;cfg->finish_turn=diagnostic_continue;}
    if(f->stage==3)cfg->transform_context=diagnostic_transform;
    if(f->stage==4)cfg->convert_to_model=diagnostic_transform;
    if(f->stage==5)cfg->get_api_key=diagnostic_key;
    /* transform/convert are used by the modern provider path only. */
}
static pangu_status diagnostic_request(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)view;return diagnostic_model(ctx,owner,sink,target);
}
static pangu_status diagnostic_plain(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    (void)ctx;(void)owner;(void)sink;(void)target;return PANGU_MODEL_ERROR;
}
static void failure_diagnostics(int trace) {
    for(int stage=0;stage<6;++stage)for(int cancel=0;cancel<2;++cancel) {
        pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        CHECK(pangu_core_report_error(owner,S("idle"))==PANGU_INVALID);
        diagnostic_fixture f={.stage=stage,.cancel=cancel};pangu_loop_config cfg;diagnostic_config(&cfg,&f);cfg.request=diagnostic_request;
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==(cancel?PANGU_CANCELLED:PANGU_MODEL_ERROR));
        const pangu_message *reply=pangu_core_message_at(owner,pangu_core_message_count(owner)-1);
        CHECK(reply->error.len==17 && !memcmp(reply->error.data,"provider exploded",17));
        CHECK(reply->stop_reason==(cancel?PANGU_ABORTED:PANGU_ERROR));
        CHECK(reply->content_count==1 && !reply->content[0].data.len);
        CHECK(pangu_core_error(owner).len==17 && !memcmp(pangu_core_error(owner).data,"provider exploded",17));
        if(trace)printf("diagnostic:%d:%d:%d:%.*s\n",stage,cancel,f.requests,(int)reply->error.len,reply->error.data);
        /* New run never inherits the previous diagnostic. */
        CHECK(pangu_core_reset(owner)==PANGU_OK && !pangu_core_error(owner).len);
        cfg=(pangu_loop_config){.model=diagnostic_plain};
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_MODEL_ERROR);
        CHECK(pangu_core_error(owner).len==strlen("Model stream failed") &&
            !memcmp(pangu_core_error(owner).data,"Model stream failed",strlen("Model stream failed")));
        pangu_core_destroy(owner);
    }
    for(int stage=0;stage<6;++stage)for(size_t fail=1;fail<200;++fail) {
        memory mem={0};pangu_core_config allocation={.allocator={&mem,allocate,release}};
        pangu_core *owner=NULL;CHECK(pangu_core_create(&allocation,&owner)==PANGU_OK);
        diagnostic_fixture f={.stage=stage};pangu_loop_config cfg;diagnostic_config(&cfg,&f);cfg.request=diagnostic_request;
        mem.fail=mem.calls+fail;pangu_message prompt={.role=PANGU_USER};
        pangu_status status=pangu_core_run(owner,&prompt,1,&cfg);CHECK(status==PANGU_NOMEM || status==PANGU_MODEL_ERROR);
        pangu_core_destroy(owner);CHECK(!mem.live);if(status==PANGU_MODEL_ERROR)break;
        CHECK(fail<199);
    }
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--trace-diagnostics")) { failure_diagnostics(1); return 0; }
    failure_diagnostics(0);
    if (argc==2 && !strcmp(argv[1],"--trace-failure-identity")) { failed_identities(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-preparation")) { preparation_failures(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-request-hooks")) { request_hook_failures(1); return 0; }
    stream_allocation_regression(); request_hook_failures(0); preparation_failures(0); failed_identities(0); long_messages();
    (void)argv;
    allocation_failures();
    for (int mode=NORMAL;mode<=TERMINAL_ABORT;++mode) {
        if (argc==3 && !strcmp(argv[1],"--case") && mode!=atoi(argv[2])) continue;
        fixture f={.mode=mode}; pangu_core *c=NULL;
        CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        pangu_tool t={.name=S("echo"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.execute=execute};
        pangu_loop_config cfg={.ctx=&f,.model=model,.event=event,.tools=&t,.tool_count=1,
            .model_config={.id=S("mock"),.api=S("mock"),.provider=S("mock")},.before_tool=before,.after_tool=after,.finish_turn_context=finish_context,.tool_execution=PANGU_TOOLS_SEQUENTIAL};
        CHECK(pangu_core_run(c,NULL,0,&cfg)==PANGU_INVALID);
        pangu_content b={.kind=PANGU_TEXT,.data=S("hello")};
        pangu_message prompt={.role=PANGU_USER,.content=&b,.content_count=1};
        pangu_status s=pangu_core_run(c,&prompt,1,&cfg);
        CHECK(s==((mode==MODEL_ABORT || mode==CANCEL_BEFORE || mode==PARTIAL_ABORT || mode==TERMINAL_ABORT)?PANGU_CANCELLED:
            (mode==MODEL_FAIL || mode==NO_FINAL || mode==PARTIAL_FAIL)?PANGU_MODEL_ERROR:PANGU_OK));
        CHECK(!pangu_core_is_running(c));
        if (mode==PARTIAL_FAIL || mode==PARTIAL_ABORT || mode==TERMINAL_ABORT || mode==NO_FINAL) {
            const pangu_message *saved=pangu_core_message_at(c,pangu_core_message_count(c)-1);
            CHECK(saved->content_count==1 && saved->content[0].data.len==11);
            CHECK(!memcmp(saved->content[0].data.data,"thinking...",11));
            if (mode==TERMINAL_ABORT) {
                CHECK(saved->usage.total==42 && saved->provider_thinking_level.len==11);
                CHECK(saved->error.len==16 && !memcmp(saved->error.data,"provider aborted",16));
            }
        }
        CHECK(f.events[0]==PANGU_AGENT_START && f.events[1]==PANGU_TURN_START);
        CHECK(f.events[f.n-1]==PANGU_AGENT_END && f.events[f.n-2]==PANGU_TURN_END);
        CHECK(f.effects==(mode==NORMAL || mode==TERMINATE));
        CHECK(f.after==f.effects);
        CHECK(f.requests==((mode==MODEL_FAIL || mode==MODEL_ABORT || mode==NO_FINAL || mode==PARTIAL_FAIL || mode==PARTIAL_ABORT || mode==TERMINAL_ABORT || mode==TERMINATE)?1:2));
        if (mode==NORMAL) {
            pangu_event_kind expected[]={PANGU_AGENT_START,PANGU_TURN_START,PANGU_MESSAGE_START,PANGU_MESSAGE_END,
                PANGU_MESSAGE_START,PANGU_MESSAGE_END,PANGU_MESSAGE_START,
                PANGU_MESSAGE_END,PANGU_TOOL_START,PANGU_TOOL_UPDATE,PANGU_TOOL_END,
                PANGU_MESSAGE_START,PANGU_MESSAGE_END,PANGU_TURN_END,PANGU_TURN_START,
                PANGU_MESSAGE_START,PANGU_MESSAGE_END,PANGU_TURN_END,PANGU_AGENT_END};
            CHECK(f.n==sizeof(expected)/sizeof(expected[0]));
            CHECK(memcmp(f.events,expected,sizeof(expected))==0);
            if (argc>1 && !strcmp(argv[1],"--trace-normal")) {
                static const char *names[]={"agent_start","agent_end","turn_start","turn_end",
                    "message_start","message_update","message_end","tool_execution_start","tool_execution_update","tool_execution_end"};
                putchar('[');
                for (size_t i=0;i<f.n;++i) printf("%s\"%s\"",i?",":"",names[f.events[i]]);
                puts("]");
            }
        }
        if (mode==NORMAL && argc>1 && !strcmp(argv[1],"--messages-normal")) {
            pangu_core *encoded=NULL; pangu_model_config metadata={0};
            CHECK(pangu_proxy_request_json(c,c,&metadata,(pangu_slice){0},&encoded)==PANGU_OK);
            pangu_slice json=pangu_core_message_at(encoded,0)->content[0].data;
            fwrite(json.data,1,json.len,stdout); putchar('\n'); pangu_core_destroy(encoded);
        }
        if (argc==3 && !strcmp(argv[1],"--case")) {
            static const char *names[]={"agent_start","agent_end","turn_start","turn_end","message_start","message_update","message_end","tool_execution_start","tool_execution_update","tool_execution_end"};
            printf("{\"status\":%d,\"effects\":%d,\"requests\":%d,\"finished\":%d,\"events\":[",s,f.effects,f.requests,f.finish);
            for (size_t i=0;i<f.n;++i) printf("%s\"%s\"",i?",":"",names[f.events[i]]);
            fputs("],\"request\":",stdout);
            pangu_core *encoded=NULL; pangu_model_config metadata={0};
            CHECK(pangu_proxy_request_json(c,c,&metadata,(pangu_slice){0},&encoded)==PANGU_OK);
            pangu_slice json=pangu_core_message_at(encoded,0)->content[0].data;
            fwrite(json.data,1,json.len,stdout); puts("}"); pangu_core_destroy(encoded);
        }
        CHECK(pangu_core_reset(c)==PANGU_OK);
        pangu_core_destroy(c);
    }
    return 0;
}
