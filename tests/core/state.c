#include <pangu/core/state.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); abort(); } } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
typedef struct { int requests, prepares, mode, added; char seen[16]; uint64_t first, second, third; int a,b,d,end; } fixture;
enum { ONE, ALL, HOOK_QUEUE, FINISH_END, ERROR_EXIT, RESUME, FOLLOW_RESUME };
static pangu_status enqueue(pangu_core *c,pangu_queue_kind kind,const char *text) {
    pangu_content b={.kind=PANGU_TEXT,.data={text,strlen(text)}};
    pangu_message m={.role=PANGU_USER,.content=&b,.content_count=1};
    return pangu_core_enqueue(c,kind,&m);
}
static pangu_status model(void *ctx,pangu_core *c,pangu_model_sink sink,void *target) {
    fixture *f=ctx;
    const pangu_message *tail=pangu_core_message_at(c,pangu_core_message_count(c)-1);
    CHECK(tail->role==PANGU_USER);
    CHECK(f->requests<15);
    f->seen[f->requests++]=tail->content[0].data.data[0];
    pangu_content b={.kind=PANGU_TEXT,.data=S("answer")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1,.stop_reason=f->mode==ERROR_EXIT?PANGU_ERROR:PANGU_STOP,.error=f->mode==ERROR_EXIT?S("script error"):(pangu_slice){0}};
    pangu_status s=sink(target,&m,0); if (s!=PANGU_OK) return s;
    CHECK(pangu_core_streaming_message(c)!=NULL);
    return sink(target,&m,1);
}
static pangu_status next(void *ctx,pangu_core *c) {
    fixture *f=ctx; ++f->prepares;
    if (f->mode==HOOK_QUEUE && !f->added++) CHECK(enqueue(c,PANGU_STEERING,"3")==PANGU_OK);
    return PANGU_OK;
}
static pangu_turn_decision finish(void *ctx,pangu_core *c,const pangu_message *m) {
    fixture *f=ctx; (void)c; (void)m;
    if (f->mode==FINISH_END || f->mode==RESUME || f->mode==FOLLOW_RESUME) return PANGU_TURN_END_RUN;
    return PANGU_TURN_AUTO;
}
static void third(void *ctx,pangu_core *c,const pangu_event *event) {
    fixture *f=ctx; CHECK(pangu_core_wait_idle(c)==PANGU_BUSY); ++f->d;
    if (event->kind==PANGU_AGENT_END) ++f->end;
}
static void second(void *ctx,pangu_core *c,const pangu_event *event) {
    (void)c; (void)event; ++((fixture *)ctx)->b;
}
static void first(void *ctx,pangu_core *c,const pangu_event *event) {
    fixture *f=ctx; ++f->a;
    if (event->kind==PANGU_AGENT_START) {
        CHECK(pangu_core_unsubscribe(c,f->second)==PANGU_OK);
        CHECK(pangu_core_subscribe(c,third,ctx,&f->third)==PANGU_OK);
        CHECK(pangu_core_unsubscribe(c,f->first)==PANGU_OK);
    }
}
static void subscriptions(void) {
    fixture f={0}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    CHECK(pangu_core_subscribe(c,first,&f,&f.first)==PANGU_OK);
    uint64_t duplicate; CHECK(pangu_core_subscribe(c,first,&f,&duplicate)==PANGU_OK && duplicate==f.first);
    CHECK(pangu_core_subscribe(c,second,&f,&f.second)==PANGU_OK);
    pangu_loop_config cfg={.ctx=&f,.model=model};
    pangu_content b={.kind=PANGU_TEXT,.data=S("p")};
    pangu_message prompt={.role=PANGU_USER,.content=&b,.content_count=1};
    CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
    CHECK(f.a==1 && f.b==0 && f.d==8 && f.end==1);
    CHECK(pangu_core_wait_idle(c)==PANGU_OK);
    CHECK(pangu_core_reset(c)==PANGU_OK);
    CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
    CHECK(f.end==2);
    CHECK(pangu_core_unsubscribe(c,f.third)==PANGU_OK);
    pangu_core_destroy(c);
}
typedef struct { size_t calls, fail, live; } memory;
static void *allocate(void *ctx,size_t n) {
    memory *m=ctx; if (++m->calls==m->fail) return NULL;
    void *p=malloc(n); if (p) ++m->live; return p;
}
static void release(void *ctx,void *p) { if (p) { --((memory *)ctx)->live; free(p); } }
static void allocation_failures(void) {
    int success=0;
    for (size_t fail=1;fail<100;++fail) {
        memory mem={0}; fixture f={0}; pangu_core *c=NULL;
        pangu_core_config memory_config={.allocator={&mem,allocate,release}};
        CHECK(pangu_core_create(&memory_config,&c)==PANGU_OK);
        CHECK(enqueue(c,PANGU_STEERING,"1")==PANGU_OK);
        CHECK(enqueue(c,PANGU_STEERING,"2")==PANGU_OK);
        CHECK(enqueue(c,PANGU_FOLLOW_UP,"f")==PANGU_OK);
        mem.fail=mem.calls+fail;
        pangu_loop_config cfg={.ctx=&f,.model=model,.finish_turn=finish,.prepare_next_turn=next};
        pangu_content b={.kind=PANGU_TEXT,.data=S("p")};
        pangu_message prompt={.role=PANGU_USER,.content=&b,.content_count=1};
        pangu_status s=pangu_core_run(c,&prompt,1,&cfg);
        CHECK(s==PANGU_OK || s==PANGU_NOMEM);
        size_t retained=pangu_core_queue_count(c,PANGU_STEERING)+pangu_core_queue_count(c,PANGU_FOLLOW_UP);
        for (size_t i=0;i<pangu_core_message_count(c);++i) {
            const pangu_message *m=pangu_core_message_at(c,i);
            if (m->role==PANGU_USER && m->content[0].data.data[0]!='p') ++retained;
        }
        CHECK(retained==3); /* failure must not silently drop selected input */
        CHECK(pangu_core_wait_idle(c)==PANGU_OK);
        if (s==PANGU_OK) success=1;
        if (!success) {
            memset(&f,0,sizeof(f)); mem.fail=0;
            CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
            CHECK(!pangu_core_queue_count(c,PANGU_STEERING) && !pangu_core_queue_count(c,PANGU_FOLLOW_UP));
        }
        pangu_core_destroy(c); CHECK(!mem.live);
        if (success) break;
    }
    CHECK(success);
}
static int tracing;
static void preview(pangu_core *c,const char *expected) {
    size_t count=pangu_core_peek_queued_count(c); CHECK(count==strlen(expected));
    for (size_t i=0;i<count;++i) {
        const pangu_message *m=pangu_core_peek_queued_at(c,i);
        CHECK(m && m->content_count && m->content[0].data.data[0]==expected[i]);
    }
    CHECK(!pangu_core_peek_queued_at(c,count));
    if (tracing) printf("queue:%s\n",expected);
}
static void queue_preview(void) {
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_queue_mode mode=PANGU_QUEUE_ALL;
    CHECK(pangu_core_get_queue_mode(c,PANGU_STEERING,&mode)==PANGU_OK && mode==PANGU_QUEUE_ONE);
    CHECK(pangu_core_get_queue_mode(c,(pangu_queue_kind)2,&mode)==PANGU_INVALID && mode==PANGU_QUEUE_ONE);
    CHECK(!pangu_core_peek_queued_count(NULL) && !pangu_core_peek_queued_at(NULL,0));
    preview(c,"");
    CHECK(enqueue(c,PANGU_STEERING,"1")==PANGU_OK && enqueue(c,PANGU_STEERING,"2")==PANGU_OK);
    CHECK(enqueue(c,PANGU_FOLLOW_UP,"f")==PANGU_OK && enqueue(c,PANGU_FOLLOW_UP,"g")==PANGU_OK);
    preview(c,"1"); preview(c,"1");
    CHECK(pangu_core_queue_count(c,PANGU_STEERING)==2);
    CHECK(pangu_core_queue_mode(c,PANGU_STEERING,PANGU_QUEUE_ALL)==PANGU_OK);
    CHECK(pangu_core_get_queue_mode(c,PANGU_STEERING,&mode)==PANGU_OK && mode==PANGU_QUEUE_ALL);
    preview(c,"12");
    CHECK(pangu_core_queue_mode(c,PANGU_FOLLOW_UP,PANGU_QUEUE_ALL)==PANGU_OK);
    CHECK(pangu_core_clear_queue(c,PANGU_STEERING)==PANGU_OK); preview(c,"fg");
    CHECK(pangu_core_queue_mode(c,PANGU_FOLLOW_UP,PANGU_QUEUE_ONE)==PANGU_OK); preview(c,"f");
    CHECK(pangu_core_reset(c)==PANGU_OK); preview(c,"");
    CHECK(pangu_core_get_queue_mode(c,PANGU_STEERING,&mode)==PANGU_OK && mode==PANGU_QUEUE_ALL);
    CHECK(enqueue(c,PANGU_STEERING,"1")==PANGU_OK && enqueue(c,PANGU_STEERING,"1")==PANGU_OK);
    CHECK(enqueue(c,PANGU_FOLLOW_UP,"f")==PANGU_OK);preview(c,"11");
    CHECK(pangu_core_clear_queue(c,PANGU_FOLLOW_UP)==PANGU_OK);preview(c,"11");
    CHECK(pangu_core_queue_mode(c,PANGU_STEERING,PANGU_QUEUE_ONE)==PANGU_OK);preview(c,"1");
    CHECK(pangu_core_queue_mode(c,PANGU_STEERING,PANGU_QUEUE_ALL)==PANGU_OK);preview(c,"11");
    CHECK(pangu_core_clear_queue(c,PANGU_STEERING)==PANGU_OK);preview(c,"");
    CHECK(enqueue(c,PANGU_FOLLOW_UP,"f")==PANGU_OK && enqueue(c,PANGU_FOLLOW_UP,"g")==PANGU_OK);
    CHECK(pangu_core_queue_mode(c,PANGU_FOLLOW_UP,PANGU_QUEUE_ALL)==PANGU_OK);preview(c,"fg");
    CHECK(pangu_core_reset(c)==PANGU_OK);preview(c,"");
    CHECK(pangu_core_get_queue_mode(c,PANGU_FOLLOW_UP,&mode)==PANGU_OK && mode==PANGU_QUEUE_ALL);
    pangu_core_destroy(c);
}
typedef struct { int mode,phase; } observation;
static void observe_state(observation *f,pangu_core *c,const char *phase,int streaming,int has_error,int running) {
    CHECK((pangu_core_streaming_message(c)!=NULL)==streaming);
    CHECK((pangu_core_error(c).len!=0)==has_error);
    CHECK(pangu_core_is_running(c)==running);
    if (has_error) CHECK(pangu_core_error(c).len==10 && !memcmp(pangu_core_error(c).data,"diagnostic",10));
    if (tracing) printf("%d:%s:%d:%d:%d\n",f->mode,phase,has_error,streaming,running);
    ++f->phase;
}
static void observed_event(void *ctx,pangu_core *c,const pangu_event *e) {
    observation *f=ctx;
    if (e->kind==PANGU_MESSAGE_START && e->message->role==PANGU_ASSISTANT) observe_state(f,c,"start",1,0,1);
    if (e->kind==PANGU_MESSAGE_UPDATE) observe_state(f,c,"update",1,0,1);
    if (e->kind==PANGU_MESSAGE_END && e->message->role==PANGU_ASSISTANT) observe_state(f,c,"end",0,0,1);
    if (e->kind==PANGU_TURN_END) observe_state(f,c,"turn",0,f->mode<3,1);
    if (e->kind==PANGU_AGENT_END) observe_state(f,c,"agent",0,f->mode<3,1);
}
static pangu_turn_decision observed_finish(void *ctx,pangu_core *c,const pangu_message *m) {
    (void)m; observe_state(ctx,c,"finish",0,0,1); return PANGU_TURN_AUTO;
}
static pangu_status observed_model(void *ctx,pangu_core *c,pangu_model_sink sink,void *target) {
    (void)c; observation *f=ctx;
    pangu_content b={.kind=PANGU_TEXT,.data=S("answer")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1,
        .stop_reason=f->mode==1?PANGU_ERROR:f->mode==2?PANGU_ABORTED:PANGU_STOP,
        .error=f->mode<3?S("diagnostic"):(pangu_slice){0}};
    pangu_status status=sink(target,&m,0);
    if (status==PANGU_OK) status=sink(target,&m,0);
    return status==PANGU_OK?sink(target,&m,1):status;
}
static void state_observations(void) {
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    observation f={0}; pangu_loop_config cfg={.ctx=&f,.model=observed_model,.event=observed_event,.finish_turn=observed_finish};
    pangu_message prompt={.role=PANGU_USER};
    for (f.mode=0;f.mode<4;++f.mode) {
        f.phase=0;
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==(f.mode==1?PANGU_MODEL_ERROR:f.mode==2?PANGU_CANCELLED:PANGU_OK));
        observe_state(&f,c,"idle",0,f.mode<3,0); CHECK(f.phase==7);
    }
    CHECK(pangu_core_reset(c)==PANGU_OK && !pangu_core_error(c).len);
    pangu_core_destroy(c);
}
typedef struct { memory *mem; int calls; } truncate_fixture;
static pangu_status truncate_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    truncate_fixture *f=ctx; ++f->calls;
    CHECK(pangu_core_queue_count(owner,PANGU_FOLLOW_UP)==2);
    const pangu_message *first=pangu_core_queue_at(owner,PANGU_FOLLOW_UP,0);
    CHECK(enqueue(owner,PANGU_FOLLOW_UP,"new")==PANGU_OK);
    size_t allocations=f->mem->calls; f->mem->fail=allocations+1;
    CHECK(pangu_core_truncate_queue(owner,PANGU_FOLLOW_UP,2)==PANGU_OK);
    CHECK(f->mem->calls==allocations && pangu_core_queue_count(owner,PANGU_FOLLOW_UP)==2);
    CHECK(pangu_core_queue_at(owner,PANGU_FOLLOW_UP,0)==first);
    CHECK(pangu_core_truncate_queue(owner,PANGU_FOLLOW_UP,3)==PANGU_INVALID);
    f->mem->fail=0;
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP}; return sink(target,&reply,1);
}
static pangu_turn_decision truncate_finish(void *ctx,pangu_core *owner,const pangu_message *m) {
    (void)ctx; (void)owner; (void)m; return PANGU_TURN_END_RUN;
}
static void truncate_queues(void) {
    memory mem={0}; pangu_core_config config={.allocator={&mem,allocate,release}};
    pangu_core *owner=NULL; CHECK(pangu_core_create(&config,&owner)==PANGU_OK);
    CHECK(enqueue(owner,PANGU_FOLLOW_UP,"a")==PANGU_OK && enqueue(owner,PANGU_FOLLOW_UP,"b")==PANGU_OK);
    CHECK(enqueue(owner,PANGU_STEERING,"s")==PANGU_OK);
    CHECK(pangu_core_queue_mode(owner,PANGU_FOLLOW_UP,PANGU_QUEUE_ALL)==PANGU_OK);
    truncate_fixture f={.mem=&mem}; pangu_loop_config cfg={.ctx=&f,.model=truncate_model,.finish_turn=truncate_finish};
    pangu_message prompt={.role=PANGU_USER}; CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK && f.calls==1);
    CHECK(pangu_core_queue_count(owner,PANGU_FOLLOW_UP)==2);
    pangu_queue_mode mode=PANGU_QUEUE_ONE; CHECK(pangu_core_get_queue_mode(owner,PANGU_FOLLOW_UP,&mode)==PANGU_OK && mode==PANGU_QUEUE_ALL);
    CHECK(enqueue(owner,PANGU_STEERING,"keep")==PANGU_OK);
    const pangu_message *other=pangu_core_queue_at(owner,PANGU_STEERING,0);
    size_t calls=mem.calls; mem.fail=calls+1;
    CHECK(pangu_core_truncate_queue(owner,PANGU_FOLLOW_UP,1)==PANGU_OK && mem.calls==calls);
    CHECK(pangu_core_truncate_queue(owner,(pangu_queue_kind)2,0)==PANGU_INVALID);
    CHECK(pangu_core_truncate_queue(NULL,PANGU_STEERING,0)==PANGU_INVALID);
    CHECK(pangu_core_truncate_queue(owner,PANGU_FOLLOW_UP,SIZE_MAX)==PANGU_INVALID);
    CHECK(pangu_core_truncate_queue(owner,PANGU_FOLLOW_UP,0)==PANGU_OK && mem.calls==calls);
    CHECK(!pangu_core_queue_count(owner,PANGU_FOLLOW_UP));
    CHECK(pangu_core_queue_count(owner,PANGU_STEERING)==1 && pangu_core_queue_at(owner,PANGU_STEERING,0)==other);
    pangu_core_destroy(owner); CHECK(!mem.live);
    int completed=0,retained=0;
    for (size_t fail=1;fail<100;++fail) {
        memory m={0}; pangu_core_config mc={.allocator={&m,allocate,release}};
        CHECK(pangu_core_create(&mc,&owner)==PANGU_OK);
        CHECK(enqueue(owner,PANGU_STEERING,"1")==PANGU_OK && enqueue(owner,PANGU_STEERING,"2")==PANGU_OK);
        CHECK(pangu_core_queue_mode(owner,PANGU_STEERING,PANGU_QUEUE_ALL)==PANGU_OK);
        pangu_content text={.kind=PANGU_TEXT,.data=S("p")};
        pangu_message seed={.role=PANGU_USER,.content=&text,.content_count=1}; CHECK(pangu_core_append(owner,&seed)==PANGU_OK);
        fixture run={.mode=FINISH_END}; pangu_loop_config loop={.ctx=&run,.model=model,.finish_turn=finish};
        m.fail=m.calls+fail; pangu_status status=pangu_core_run(owner,NULL,0,&loop);
        CHECK(status==PANGU_OK || status==PANGU_NOMEM);
        if (status==PANGU_NOMEM) {
            size_t count=pangu_core_queue_count(owner,PANGU_STEERING);
            if (count) {
                ++retained; const pangu_message *prefix=pangu_core_queue_at(owner,PANGU_STEERING,0);
                size_t before=m.calls; m.fail=before+1;
                CHECK(pangu_core_truncate_queue(owner,PANGU_STEERING,count-1)==PANGU_OK && m.calls==before);
                CHECK(pangu_core_queue_count(owner,PANGU_STEERING)==count-1);
                if (count>1) CHECK(pangu_core_queue_at(owner,PANGU_STEERING,0)==prefix);
                CHECK(pangu_core_truncate_queue(owner,PANGU_STEERING,0)==PANGU_OK && m.calls==before);
            }
        }
        pangu_core_destroy(owner); CHECK(!m.live);
        if (status==PANGU_OK) { completed=1; break; }
    }
    CHECK(completed && retained);
}
typedef struct { int calls,queued; } cancel_fixture;
static void cancel_queue_event(void *ctx,pangu_core *owner,const pangu_event *e) {
    cancel_fixture *f=ctx;
    if(e->kind!=PANGU_MESSAGE_START || e->message->role!=PANGU_ASSISTANT || f->queued)return;
    f->queued=1;
    CHECK(enqueue(owner,PANGU_STEERING,"s1")==PANGU_OK && enqueue(owner,PANGU_STEERING,"s2")==PANGU_OK);
    CHECK(enqueue(owner,PANGU_FOLLOW_UP,"f1")==PANGU_OK && enqueue(owner,PANGU_FOLLOW_UP,"f2")==PANGU_OK);
    pangu_core_cancel(owner);
}
static pangu_status cancel_queue_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    cancel_fixture *f=ctx;const char *inputs[]={"p","s1","s2","f1","f2"};
    CHECK(f->calls<5);const pangu_message *last=pangu_core_message_at(owner,pangu_core_message_count(owner)-1);
    CHECK(last->role==PANGU_USER && last->content[0].data.len==strlen(inputs[f->calls]) &&
        !memcmp(last->content[0].data.data,inputs[f->calls],last->content[0].data.len));
    int first=!f->calls++;
    pangu_content text={.kind=PANGU_TEXT,.data=first?S("partial"):S("answer")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_STOP};
    if(first){
        m.stop_reason=PANGU_PENDING;
        pangu_status status=sink(target,&m,0);if(status!=PANGU_OK)return status;
        CHECK(pangu_core_is_cancelled(owner));m.stop_reason=PANGU_ABORTED;m.error=S("Operation aborted");
    }
    return sink(target,&m,1);
}
static pangu_turn_decision cancel_queue_finish(void *ctx,pangu_core *owner,const pangu_message *m) {
    (void)ctx;(void)owner;(void)m;return PANGU_TURN_END_RUN;
}
static void cancelled_queue_resume(int trace) {
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);cancel_fixture f={0};
    pangu_loop_config config={.ctx=&f,.model=cancel_queue_model,.event=cancel_queue_event,.finish_turn=cancel_queue_finish};
    pangu_content text={.kind=PANGU_TEXT,.data=S("p")};pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
    const size_t steering[]={2,1,0,0,0},follow[]={2,2,2,1,0};
    for(int step=0;step<5;++step){
        CHECK(pangu_core_run(owner,step?NULL:&prompt,step?0:1,&config)==(step?PANGU_OK:PANGU_CANCELLED));
        CHECK(pangu_core_queue_count(owner,PANGU_STEERING)==steering[step] && pangu_core_queue_count(owner,PANGU_FOLLOW_UP)==follow[step]);
        CHECK(pangu_core_message_count(owner)==(size_t)(step+1)*2 && pangu_core_wait_idle(owner)==PANGU_OK && !pangu_core_streaming_message(owner));
        const pangu_message *last=pangu_core_message_at(owner,(size_t)(step+1)*2-1);
        CHECK(last->stop_reason==(step?PANGU_STOP:PANGU_ABORTED) && !!pangu_core_error(owner).len==!step);
        const pangu_message *queued=pangu_core_peek_queued_at(owner,0);
        pangu_slice next=queued?queued->content[0].data:(pangu_slice){0};
        CHECK(last->content[0].data.len==(step?6u:7u));
        if(trace)printf("resume:%d:%.*s:%zu:%d:%d\n",step,(int)next.len,next.data?next.data:"",pangu_core_message_count(owner),(int)last->stop_reason,!!pangu_core_error(owner).len);
    }
    CHECK(pangu_core_run(owner,NULL,0,&config)==PANGU_INVALID && f.calls==5);
    pangu_core_destroy(owner);
}
/* agent.test.ts:473/538. Retained sinks are exercised only before release,
 * per the native operation lifetime; never call a freed sink after run return. */
typedef struct {
    pangu_tool_sink sink[2]; void *target[2];
    int parallel, starts, releases, pumps, events, updates, ends, requests;
} late_fixture;
static pangu_status late_validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)ctx; CHECK(schema.len==2 && args.len==2); return PANGU_OK;
}
static pangu_status late_model(void *ctx,pangu_core *c,pangu_model_sink sink,void *target) {
    late_fixture *f=ctx; (void)c; CHECK(++f->requests==1);
    pangu_content calls[]={
        {.kind=PANGU_TOOL_CALL,.id=S("a"),.name=S("tool"),.data=S("{}")},
        {.kind=PANGU_TOOL_CALL,.id=S("b"),.name=S("tool"),.data=S("{}")}};
    pangu_message m={.role=PANGU_ASSISTANT,.content=calls,.content_count=f->parallel?2:1,.stop_reason=PANGU_TOOL_USE};
    return sink(target,&m,1);
}
static pangu_status late_start(void *ctx,pangu_core *c,const pangu_content *call,pangu_tool_sink sink,void *target,void **op) {
    late_fixture *f=ctx; (void)c; int i=call->id.data[0]-'a'; CHECK(i>=0 && i<2);
    f->sink[i]=sink; f->target[i]=target; ++f->starts; *op=&f->target[i]; return PANGU_OK;
}
static void late_release(void *ctx,void *op) {
    late_fixture *f=ctx; int i=op==&f->target[0]?0:1;
    CHECK(op==&f->target[i] && f->sink[i]);
    f->sink[i]=NULL; f->target[i]=NULL; ++f->releases;
}
static void late_event(void *ctx,pangu_core *c,const pangu_event *e) {
    late_fixture *f=ctx; ++f->events;
    if(e->kind==PANGU_TOOL_UPDATE) {
        ++f->updates; CHECK(!f->parallel && e->message->content[0].data.len==7);
    }
    if(e->kind==PANGU_TOOL_END) {
        ++f->ends;
        CHECK(e->message->content[0].data.len==4 && !memcmp(e->message->content[0].data.data,"done",4));
        CHECK(pangu_core_pending_tool_count(c)==(size_t)(f->parallel && f->ends==1));
    }
}
static pangu_status late_pump(void *ctx,pangu_core *c) {
    late_fixture *f=ctx; ++f->pumps;
    pangu_content text={.kind=PANGU_TEXT,.data=S("done")};
    pangu_message m={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1,.terminate=1};
    if(f->pumps==1) {
        CHECK(f->starts==(f->parallel?2:1));
        if(!f->parallel) { text.data=S("running"); CHECK(f->sink[0](f->target[0],&m,0)==PANGU_OK); text.data=S("done"); }
        CHECK(f->sink[0](f->target[0],&m,1)==PANGU_OK);
        if(f->parallel) return PANGU_OK; /* let Core emit end(a), keeping b active */
    } else CHECK(f->parallel && f->pumps==2 && f->ends==1);
    int before=f->events;
    text.data=S("late"); m.is_error=1; m.terminate=0;
    CHECK(f->sink[0] && f->sink[0](f->target[0],&m,0)==PANGU_OK);
    CHECK(f->events==before && pangu_core_is_running(c));
    /* Ignore even malformed stale payloads, before validation/copying. */
    CHECK(f->sink[0](f->target[0],NULL,0)==PANGU_OK && f->events==before);
    if(f->parallel) { text.data=S("done"); m.is_error=0; m.terminate=1; CHECK(f->sink[1](f->target[1],&m,1)==PANGU_OK); }
    return PANGU_OK;
}
static void late_tool_updates(int trace) {
    for(int parallel=0;parallel<2;++parallel) {
        late_fixture f={.parallel=parallel}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        pangu_tool tool={.name=S("tool"),.schema_json=S("{}"),.ctx=&f,.validate=late_validate,.start=late_start,.release=late_release};
        pangu_loop_config cfg={.ctx=&f,.model=late_model,.tools=&tool,.tool_count=1,.pump=late_pump,.event=late_event};
        pangu_message prompt={.role=PANGU_USER}; CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
        CHECK(f.updates==!parallel && f.ends==parallel+1 && f.releases==parallel+1 && f.pumps==parallel+1);
        CHECK(!f.sink[0] && !f.sink[1] && !pangu_core_pending_tool_count(c));
        for(size_t i=pangu_core_message_count(c)-(size_t)(parallel+1);i<pangu_core_message_count(c);++i) {
            const pangu_message *m=pangu_core_message_at(c,i);
            CHECK(m->role==PANGU_TOOL_RESULT && !m->is_error && m->content[0].data.len==4);
            CHECK(!memcmp(m->content[0].data.data,"done",4));
        }
        CHECK(pangu_core_wait_idle(c)==PANGU_OK);
        if(trace)printf("late:%d:%d:%d:1\n",parallel,f.updates,f.ends);
        pangu_core_destroy(c);
    }
}
/* Native settlement means returning from each callback, in registration order. */
typedef struct { int stage, rounds, cancelled; pangu_loop_config *cfg; } barrier_fixture;
static void barrier_run_event(void *ctx,pangu_core *c,const pangu_event *e) {
    barrier_fixture *f=ctx;
    if(e->kind==PANGU_AGENT_END) { CHECK(f->stage==0); f->stage=1; }
    CHECK(pangu_core_wait_idle(c)==PANGU_BUSY);
}
static void barrier_first(void *ctx,pangu_core *c,const pangu_event *e) {
    barrier_fixture *f=ctx;
    CHECK(pangu_core_is_running(c) && pangu_core_wait_idle(c)==PANGU_BUSY);
    if(e->kind!=PANGU_AGENT_END)return;
    CHECK(f->stage==1); f->stage=2;
    size_t count=pangu_core_message_count(c); const pangu_message *tail=pangu_core_message_at(c,count-1);
    pangu_message prompt={.role=PANGU_USER};
    CHECK(pangu_core_reset(c)==PANGU_BUSY && pangu_core_clear(c)==PANGU_BUSY);
    CHECK(pangu_core_run(c,&prompt,1,f->cfg)==PANGU_BUSY);
    CHECK(pangu_core_run(c,NULL,0,f->cfg)==PANGU_BUSY);
    CHECK(pangu_core_append(c,&prompt)==PANGU_BUSY);
    CHECK(pangu_core_message_count(c)==count && pangu_core_message_at(c,count-1)==tail);
    CHECK(!pangu_core_is_cancelled(c)); pangu_core_cancel(c);
    CHECK(pangu_core_is_cancelled(c)); ++f->cancelled;
}
static void barrier_second(void *ctx,pangu_core *c,const pangu_event *e) {
    barrier_fixture *f=ctx;
    if(e->kind==PANGU_AGENT_END) {
        CHECK(f->stage==2 && pangu_core_is_cancelled(c));
        CHECK(pangu_core_wait_idle(c)==PANGU_BUSY); f->stage=3; ++f->rounds;
    }
}
static pangu_status barrier_model(void *ctx,pangu_core *c,pangu_model_sink sink,void *target) {
    (void)ctx; CHECK(!pangu_core_is_cancelled(c));
    pangu_message m={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP}; return sink(target,&m,1);
}
static void settlement_barrier(int trace) {
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    barrier_fixture f={0}; pangu_loop_config cfg={.ctx=&f,.model=barrier_model,.event=barrier_run_event}; f.cfg=&cfg;
    uint64_t a,b; CHECK(pangu_core_subscribe(c,barrier_first,&f,&a)==PANGU_OK);
    CHECK(pangu_core_subscribe(c,barrier_second,&f,&b)==PANGU_OK);
    for(int i=0;i<2;++i) {
        f.stage=0; pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
        CHECK(f.stage==3 && f.rounds==i+1 && f.cancelled==i+1 && pangu_core_wait_idle(c)==PANGU_OK);
        if(trace)printf("barrier:%d:%d:%d:%d:%d:%d\n",i,f.stage,f.rounds,f.cancelled,pangu_core_is_running(c),pangu_core_wait_idle(c)==PANGU_OK);
        CHECK(pangu_core_reset(c)==PANGU_OK);
    }
    CHECK(pangu_core_unsubscribe(c,a)==PANGU_OK && pangu_core_unsubscribe(c,b)==PANGU_OK);
    pangu_core_destroy(c);
}
static void default_mutators_reset(void) {
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    CHECK(!pangu_core_message_count(c) && !pangu_core_message_at(c,0));
    CHECK(!pangu_core_is_running(c) && !pangu_core_is_cancelled(c));
    CHECK(!pangu_core_streaming_message(c) && !pangu_core_pending_tool_count(c) && !pangu_core_pending_tool_at(c,0));
    CHECK(!pangu_core_error(c).len && !pangu_core_prepared_count(c) && !pangu_core_prepared_at(c,0));
    CHECK(pangu_core_wait_idle(c)==PANGU_OK);
    for(int k=PANGU_STEERING;k<=PANGU_FOLLOW_UP;++k) {
        pangu_queue_mode mode=PANGU_QUEUE_ALL;
        CHECK(pangu_core_get_queue_mode(c,(pangu_queue_kind)k,&mode)==PANGU_OK && mode==PANGU_QUEUE_ONE);
        CHECK(pangu_core_queue_mode(c,(pangu_queue_kind)k,PANGU_QUEUE_ALL)==PANGU_OK);
    }
    char bytes[]="base"; pangu_content text={.kind=PANGU_TEXT,.data={bytes,4}};
    pangu_tool_declaration tool={.name=S("echo"),.schema_json=S("{}")};
    pangu_message system={.role=PANGU_SYSTEM,.content=&text,.content_count=1,.tools_added=&tool,.tools_added_count=1};
    CHECK(pangu_core_append(c,&system)==PANGU_OK); memset(bytes,'x',4); tool.name=S("lost");
    CHECK(!memcmp(pangu_core_message_at(c,0)->content[0].data.data,"base",4));
    CHECK(!memcmp(pangu_core_message_at(c,0)->tools_added[0].name.data,"echo",4));
    fixture f={.mode=ERROR_EXIT}; pangu_loop_config cfg={.ctx=&f,.model=model};
    /* Preserve the existing baseline declarations while generating an error. */
    pangu_tool executable={.name=S("echo"),.schema_json=S("{}")}; cfg.tools=&executable; cfg.tool_count=1;
    pangu_content body={.kind=PANGU_TEXT,.data=S("p")}; pangu_message prompt={.role=PANGU_USER,.content=&body,.content_count=1};
    CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_MODEL_ERROR && pangu_core_error(c).len);
    CHECK(enqueue(c,PANGU_STEERING,"s")==PANGU_OK && enqueue(c,PANGU_FOLLOW_UP,"f")==PANGU_OK);
    CHECK(pangu_core_reset(c)==PANGU_OK);
    CHECK(pangu_core_message_count(c)==1 && !pangu_core_error(c).len && !pangu_core_streaming_message(c));
    CHECK(!pangu_core_pending_tool_count(c) && !pangu_core_peek_queued_count(c));
    const pangu_message *base=pangu_core_message_at(c,0);
    CHECK(base->role==PANGU_SYSTEM && base->tools_added_count==1 && !memcmp(base->tools_added[0].name.data,"echo",4));
    for(int k=0;k<2;++k) { pangu_queue_mode mode=PANGU_QUEUE_ONE; CHECK(pangu_core_get_queue_mode(c,(pangu_queue_kind)k,&mode)==PANGU_OK && mode==PANGU_QUEUE_ALL); CHECK(!pangu_core_queue_count(c,(pangu_queue_kind)k)); }
    CHECK(pangu_core_clear(c)==PANGU_OK && !pangu_core_message_count(c));
    pangu_core_destroy(c);
}
/* Stateful continuation startup (agent.test.ts:786,824,945,961,1007,
 * 1043,1071,1109). Low-level conversion/projection stays in transcript.c. */
enum { CONT_NORMAL, CONT_REQUEST_QUEUE, CONT_NEXT_QUEUE, CONT_REQUEST_CANCEL,
    CONT_ERROR, CONT_ABORT, CONT_END };
typedef struct {
    const char *expected; size_t offset;
    char inputs[512]; size_t length;
    int requests, request_prepares, next_prepares, events, ends, mode;
    size_t initial_count;
} continuation_fixture;
static pangu_status continuation_prepare(void *ctx,pangu_core *c) {
    continuation_fixture *f=ctx; ++f->request_prepares;
    CHECK(pangu_core_is_running(c) && !pangu_core_is_cancelled(c));
    CHECK(f->request_prepares==f->requests+1);
    CHECK(f->next_prepares==f->requests);
    if(f->mode==CONT_REQUEST_QUEUE && f->request_prepares==1)
        CHECK(enqueue(c,PANGU_STEERING,"s2")==PANGU_OK);
    return f->mode==CONT_REQUEST_CANCEL?PANGU_CANCELLED:PANGU_OK;
}
static pangu_status continuation_next(void *ctx,pangu_core *c) {
    continuation_fixture *f=ctx; ++f->next_prepares;
    CHECK(pangu_core_is_running(c) && !pangu_core_is_cancelled(c));
    CHECK(f->next_prepares==f->requests);
    if(f->mode==CONT_NEXT_QUEUE && f->next_prepares==1)
        CHECK(enqueue(c,PANGU_STEERING,"s2")==PANGU_OK);
    return PANGU_OK;
}
static pangu_status continuation_provider(void *ctx,pangu_core *c,pangu_model_sink sink,void *target) {
    continuation_fixture *f=ctx;
    CHECK(f->requests<6 && f->request_prepares==f->requests+1);
    char input[128]={0}; size_t n=0;
    for(size_t i=0;i<pangu_core_message_count(c);++i) {
        const pangu_message *m=pangu_core_message_at(c,i);
        if(m->role!=PANGU_USER)continue;
        CHECK(m->content_count==1 && n+1+m->content[0].data.len<sizeof(input));
        if(n)input[n++]=',';
        memcpy(input+n,m->content[0].data.data,m->content[0].data.len); n+=m->content[0].data.len;
    }
    const char *expected=f->expected+f->offset;
    size_t size=strcspn(expected,"/"); CHECK(n==size && !memcmp(input,expected,n));
    f->offset+=size+(expected[size]=='/');
    CHECK(f->length+n+2<sizeof(f->inputs));
    if(f->requests)f->inputs[f->length++]='/';
    memcpy(f->inputs+f->length,input,n); f->length+=n; ++f->requests;
    pangu_content text={.kind=PANGU_TEXT,.data=S("done")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,
        .stop_reason=f->mode==CONT_ERROR?PANGU_ERROR:f->mode==CONT_ABORT?PANGU_ABORTED:PANGU_STOP,
        .error=f->mode==CONT_ERROR?S("error"):f->mode==CONT_ABORT?S("aborted"):(pangu_slice){0}};
    return sink(target,&reply,1);
}
static void continuation_events(void *ctx,pangu_core *c,const pangu_event *e) {
    continuation_fixture *f=ctx; ++f->events;
    CHECK(pangu_core_wait_idle(c)==PANGU_BUSY);
    if(e->kind==PANGU_AGENT_END)++f->ends;
    if(e->kind==PANGU_MESSAGE_START && e->message->role==PANGU_USER) {
        /* Existing history must never be emitted again. */
        CHECK(e->message->content[0].data.len==2);
    }
    if(e->kind==PANGU_MESSAGE_END && e->message->role==PANGU_ASSISTANT &&
       f->mode>=CONT_ERROR) CHECK(enqueue(c,PANGU_STEERING,"s2")==PANGU_OK);
}
static pangu_turn_decision continuation_finish(void *ctx,pangu_core *c,const pangu_message *m) {
    continuation_fixture *f=ctx; (void)m;
    CHECK(!pangu_core_is_cancelled(c));
    return f->mode==CONT_END?PANGU_TURN_END_RUN:
        f->mode==CONT_ERROR || f->mode==CONT_ABORT?PANGU_TURN_CONTINUE:PANGU_TURN_AUTO;
}
/* tail: 0 assistant, 1 user, 2 toolResult, 3 empty, 4 system-only,
 * 5 custom after user, 6 system after user. These last two are legal source
 * tails: Agent rejects assistant-without-queue and all-system, not all
 * non-user/toolResult roles. */
static void continuation_case(const char *name,int tail,int queues,int steering_all,int follow_all,
    int mode,const char *expected,pangu_status status,size_t left_s,size_t left_f,int trace) {
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_content text={.kind=PANGU_TEXT,.data=S("u")};
    pangu_message user={.role=PANGU_USER,.content=&text,.content_count=1,.timestamp_ms=1};
    if(tail!=3 && tail!=4)CHECK(pangu_core_append(c,&user)==PANGU_OK);
    if(tail==0 || tail==2) {
        pangu_content call={.kind=PANGU_TOOL_CALL,.id=S("call-1"),.name=S("noop"),.data=S("{}")};
        pangu_message assistant={.role=PANGU_ASSISTANT,.stop_reason=tail==2?PANGU_TOOL_USE:PANGU_STOP,
            .content=tail==2?&call:NULL,.content_count=tail==2?1:0,.timestamp_ms=2};
        CHECK(pangu_core_append(c,&assistant)==PANGU_OK);
        if(tail==2) { pangu_message result={.role=PANGU_TOOL_RESULT,.tool_call_id=S("call-1"),.tool_name=S("noop"),.timestamp_ms=3}; CHECK(pangu_core_append(c,&result)==PANGU_OK); }
    }
    if(tail==4 || tail==5 || tail==6) {
        pangu_message extra={.role=tail==5?PANGU_CUSTOM:PANGU_SYSTEM,.custom_role=tail==5?S("note"):(pangu_slice){0}};
        CHECK(pangu_core_append(c,&extra)==PANGU_OK);
    }
    CHECK(pangu_core_queue_mode(c,PANGU_STEERING,steering_all?PANGU_QUEUE_ALL:PANGU_QUEUE_ONE)==PANGU_OK);
    CHECK(pangu_core_queue_mode(c,PANGU_FOLLOW_UP,follow_all?PANGU_QUEUE_ALL:PANGU_QUEUE_ONE)==PANGU_OK);
    if(queues&1) { CHECK(enqueue(c,PANGU_STEERING,"s1")==PANGU_OK); CHECK(enqueue(c,PANGU_STEERING,"s2")==PANGU_OK); }
    if(queues&2) { CHECK(enqueue(c,PANGU_FOLLOW_UP,"f1")==PANGU_OK); CHECK(enqueue(c,PANGU_FOLLOW_UP,"f2")==PANGU_OK); }
    const pangu_message *original[3]; size_t count=pangu_core_message_count(c); CHECK(count<=3);
    for(size_t i=0;i<count;++i)original[i]=pangu_core_message_at(c,i);
    const pangu_message *queued_s=pangu_core_queue_at(c,PANGU_STEERING,0),*queued_f=pangu_core_queue_at(c,PANGU_FOLLOW_UP,0);
    continuation_fixture f={.expected=expected,.mode=mode,.initial_count=count};
    pangu_loop_config cfg={.ctx=&f,.model=continuation_provider,.prepare_request=continuation_prepare,
        .prepare_next_turn=continuation_next,.event=continuation_events,.finish_turn=continuation_finish};
    CHECK(pangu_core_run(c,NULL,0,&cfg)==status);
    CHECK(!expected[f.offset] && !strcmp(f.inputs,expected));
    CHECK(pangu_core_queue_count(c,PANGU_STEERING)==left_s && pangu_core_queue_count(c,PANGU_FOLLOW_UP)==left_f);
    for(size_t i=0;i<count;++i)CHECK(pangu_core_message_at(c,i)==original[i]);
    CHECK(pangu_core_wait_idle(c)==PANGU_OK && !pangu_core_streaming_message(c) && !pangu_core_pending_tool_count(c));
    if(status==PANGU_INVALID) {
        CHECK(!f.events && !f.request_prepares && !f.next_prepares && pangu_core_message_count(c)==count);
        CHECK(pangu_core_queue_at(c,PANGU_STEERING,0)==queued_s && pangu_core_queue_at(c,PANGU_FOLLOW_UP,0)==queued_f);
    } else {
        CHECK(f.ends==1 && f.request_prepares==f.requests+(mode==CONT_REQUEST_CANCEL));
        CHECK(f.next_prepares==(f.requests?f.requests-1:0));
        CHECK(!!pangu_core_error(c).len==(status!=PANGU_OK));
    }
    if(trace)printf("continuation:%s:%s:%d:%s:%zu:%zu:%d,%d\n",name,
        status==PANGU_OK?"ok":status==PANGU_INVALID?"invalid":status==PANGU_MODEL_ERROR?"error":"aborted",
        f.requests,f.length?f.inputs:"-",left_s,left_f,f.request_prepares,f.next_prepares);
    pangu_core_destroy(c);
}
static void continuation_startup(int trace) {
    /* Explicit request histories, not computed from the implementation's drain
     * algorithm. Index: queue kind (steering/follow/both), s mode, f mode. */
    static const char *assistant[3][2][2]={
        {{"u,s1/u,s1,s2","u,s1/u,s1,s2"},{"u,s1,s2","u,s1,s2"}},
        {{"u,f1/u,f1,f2","u,f1,f2"},{"u,f1/u,f1,f2","u,f1,f2"}},
        {{"u,s1/u,s1,s2/u,s1,s2,f1/u,s1,s2,f1,f2","u,s1/u,s1,s2/u,s1,s2,f1,f2"},
         {"u,s1,s2/u,s1,s2,f1/u,s1,s2,f1,f2","u,s1,s2/u,s1,s2,f1,f2"}}};
    static const char *nonassistant_follow[2]={"u/u,f1/u,f1,f2","u/u,f1,f2"};
    const char *tails[]={"assistant","user","tool"},*kinds[]={"steering","follow","both"},*modes[]={"one","all"};
    for(int tail=0;tail<3;++tail)for(int q=0;q<3;++q)for(int sm=0;sm<2;++sm)for(int fm=0;fm<2;++fm) {
        char name[80]; snprintf(name,sizeof(name),"%s-%s-%s-%s",tails[tail],kinds[q],modes[sm],modes[fm]);
        const char *expected=tail && q==1?nonassistant_follow[fm]:assistant[q][sm][fm];
        continuation_case(name,tail,q+1,sm,fm,CONT_NORMAL,expected,PANGU_OK,0,0,trace);
    }
    continuation_case("assistant-empty",0,0,0,0,CONT_NORMAL,"",PANGU_INVALID,0,0,trace);
    continuation_case("empty-queued",3,3,0,0,CONT_NORMAL,"",PANGU_INVALID,2,2,trace);
    continuation_case("system-queued",4,3,0,0,CONT_NORMAL,"",PANGU_INVALID,2,2,trace);
    continuation_case("user-empty",1,0,0,0,CONT_NORMAL,"u",PANGU_OK,0,0,trace);
    continuation_case("tool-empty",2,0,0,0,CONT_NORMAL,"u",PANGU_OK,0,0,trace);
    continuation_case("custom-both",5,3,0,0,CONT_NORMAL,assistant[2][0][0],PANGU_OK,0,0,trace);
    continuation_case("system-tail",6,0,0,0,CONT_NORMAL,"u",PANGU_OK,0,0,trace);
    continuation_case("request-queue",1,0,0,0,CONT_REQUEST_QUEUE,"u/u,s2",PANGU_OK,0,0,trace);
    /* pending f1 prevents a second steering poll after next-turn preparation. */
    continuation_case("next-queue",0,2,0,0,CONT_NEXT_QUEUE,"u,f1/u,f1,f2/u,f1,f2,s2",PANGU_OK,0,0,trace);
    continuation_case("request-cancel",0,3,0,0,CONT_REQUEST_CANCEL,"",PANGU_CANCELLED,1,2,trace);
    continuation_case("error-continue",1,2,0,0,CONT_ERROR,"u",PANGU_MODEL_ERROR,1,2,trace);
    continuation_case("aborted-continue",1,2,0,0,CONT_ABORT,"u",PANGU_CANCELLED,1,2,trace);
    continuation_case("finish-end",1,2,0,0,CONT_END,"u",PANGU_OK,1,2,trace);
}
/* agent.test.ts:676/710/750: reject mutations while a model snapshot is live,
 * before assistant commit, rather than only at agent_end settlement. */
typedef struct { pangu_loop_config *config; int checked; } busy_fixture;
static pangu_status busy_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    busy_fixture *f=ctx; (void)owner;
    pangu_content text={.kind=PANGU_TEXT,.data=S("")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1};
    pangu_status status=sink(target,&reply,0); if(status!=PANGU_OK)return status;
    CHECK(f->checked==1);text.data=S("Done");reply.stop_reason=PANGU_STOP;
    return sink(target,&reply,1);
}
static void busy_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    busy_fixture *f=ctx;
    if(event->kind!=PANGU_MESSAGE_START || event->message->role!=PANGU_ASSISTANT)return;
    CHECK(!f->checked++ && pangu_core_is_running(owner));
    CHECK(pangu_core_streaming_message(owner) && pangu_core_message_count(owner)==1);
    const pangu_message *original=pangu_core_message_at(owner,0);
    pangu_message input={.role=PANGU_USER};
    CHECK(pangu_core_reset(owner)==PANGU_BUSY);
    CHECK(pangu_core_run(owner,&input,1,f->config)==PANGU_BUSY);
    CHECK(pangu_core_run(owner,NULL,0,f->config)==PANGU_BUSY);
    CHECK(pangu_core_message_count(owner)==1 && pangu_core_message_at(owner,0)==original);
    CHECK(pangu_core_is_running(owner) && pangu_core_wait_idle(owner)==PANGU_BUSY);
}
static void streaming_busy(int trace) {
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    busy_fixture f={0};pangu_loop_config config={.ctx=&f,.model=busy_model,.event=busy_event};f.config=&config;
    pangu_content text={.kind=PANGU_TEXT,.data=S("Hello")};
    pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
    CHECK(pangu_core_run(owner,&input,1,&config)==PANGU_OK && f.checked==1);
    CHECK(!pangu_core_is_running(owner) && pangu_core_wait_idle(owner)==PANGU_OK);
    CHECK(pangu_core_message_count(owner)==2 && pangu_core_message_at(owner,0)->role==PANGU_USER);
    CHECK(pangu_core_message_at(owner,1)->role==PANGU_ASSISTANT);
    if(trace)printf("busy:%d:%zu:%d\n",f.checked,pangu_core_message_count(owner),pangu_core_is_running(owner));
    pangu_core_destroy(owner);
}
/* agent.test.ts:860/901: legacy next-turn and contextual finish hooks receive
 * the active run, after real tool results have committed. */
typedef struct { int mode,requests,next,finishes,signals; char roles[8]; } hook_fixture;
static void hook_context(hook_fixture *f,pangu_core *owner) {
    CHECK(pangu_core_is_running(owner) && !pangu_core_is_cancelled(owner));
    CHECK(pangu_core_wait_idle(owner)==PANGU_BUSY);++f->signals;
    CHECK(pangu_core_message_count(owner)==4);
    for(size_t i=0;i<4;++i)f->roles[i]=(char)('0'+pangu_core_message_at(owner,i)->role);
    CHECK(!strcmp(f->roles,"0123"));
}
static pangu_status hook_provider(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    hook_fixture *f=ctx;(void)owner;++f->requests;
    pangu_content content=f->requests==1?(pangu_content){.kind=PANGU_TOOL_CALL,.id=S("tool-1"),.name=S("noop"),.data=S("{}")}
        :(pangu_content){.kind=PANGU_TEXT,.data=S("done")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&content,.content_count=1,.stop_reason=f->requests==1?PANGU_TOOL_USE:PANGU_STOP};
    return sink(target,&reply,1);
}
static pangu_status hook_execute(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)ctx;(void)owner;CHECK(call->id.len==6);
    pangu_content text={.kind=PANGU_TEXT,.data=S("tool complete")};
    pangu_message reply={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};return sink(target,&reply,1);
}
static pangu_status hook_next(void *ctx,pangu_core *owner) {
    hook_fixture *f=ctx;++f->next;hook_context(f,owner);return PANGU_OK;
}
static pangu_turn_decision hook_finish(void *ctx,pangu_core *owner,const pangu_turn_context *turn) {
    hook_fixture *f=ctx;++f->finishes;hook_context(f,owner);
    CHECK(turn->assistant->stop_reason==PANGU_TOOL_USE && pangu_core_message_count(turn->tool_results)==1);
    CHECK(pangu_core_message_at(turn->tool_results,0)->role==PANGU_TOOL_RESULT);
    return PANGU_TURN_END_RUN;
}
static void legacy_hooks(int trace) {
    for(int mode=0;mode<2;++mode) {
        pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);hook_fixture f={.mode=mode};
        pangu_tool tool={.name=S("noop"),.schema_json=S("{}"),.validate=late_validate,.execute=hook_execute};
        pangu_loop_config config={.ctx=&f,.model=hook_provider,.tools=&tool,.tool_count=1,
            .prepare_next_turn=mode?NULL:hook_next,.finish_turn_context=mode?hook_finish:NULL};
        pangu_content text={.kind=PANGU_TEXT,.data=S("start")};pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
        CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_OK);
        CHECK(f.requests==2-mode && f.next==!mode && f.finishes==mode && f.signals==1);
        if(trace)printf("hooks:%d:%d:%d:%d:%d:%s\n",mode,f.requests,f.next,f.finishes,f.signals,f.roles);
        pangu_core_destroy(owner);
    }
}
static int state_equal(pangu_slice s,const char *text) {
    return s.len==strlen(text) && (!s.len || !memcmp(s.data,text,s.len));
}
typedef struct { const char *model,*tool; pangu_thinking_level thinking;size_t tools;int requests; } override_fixture;
static pangu_status override_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    override_fixture *f=ctx;(void)view;++f->requests;
    CHECK(state_equal(pangu_core_active_model(owner)->id,f->model));
    CHECK(pangu_core_active_thinking(owner)==f->thinking && pangu_core_active_tool_count(owner)==f->tools);
    if(f->tools)CHECK(state_equal(pangu_core_active_tool_at(owner,0)->name,f->tool));
    pangu_message reply={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};return sink(target,&reply,1);
}
static void instance_overrides(void) {
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    pangu_model_config model={.id=S("stored"),.api=S("fixture"),.provider=S("local")};
    pangu_tool tool={.name=S("stored"),.schema_json=S("{}")};
    CHECK(pangu_core_set_model(owner,&model)==PANGU_OK && pangu_core_set_thinking(owner,PANGU_THINKING_HIGH)==PANGU_OK && pangu_core_set_tools(owner,&tool,1)==PANGU_OK);
    override_fixture f={0};pangu_message prompt={.role=PANGU_USER};
    for(unsigned mask=0;mask<8;++mask){
        f.model=mask&PANGU_RUN_OVERRIDE_MODEL?"":"stored";
        f.thinking=mask&PANGU_RUN_OVERRIDE_THINKING?PANGU_THINKING_OFF:PANGU_THINKING_HIGH;
        f.tools=mask&PANGU_RUN_OVERRIDE_TOOLS?0:1;f.tool="stored";
        pangu_loop_config config={.ctx=&f,.request=override_provider,.override_fields=mask};
        CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_OK);
        CHECK(state_equal(pangu_core_model(owner)->id,"stored") && pangu_core_thinking(owner)==PANGU_THINKING_HIGH && pangu_core_tool_count(owner)==1);
        CHECK(pangu_core_message_at(owner,pangu_core_message_count(owner)-1)->thinking_level==f.thinking);
    }
    f.model="request";f.thinking=PANGU_THINKING_LOW;f.tools=1;f.tool="override";
    pangu_tool alternate={.name=S("override"),.schema_json=S("{}")};
    pangu_loop_config config={.ctx=&f,.request=override_provider,.model_config={.id=S("request")},.thinking_level=PANGU_THINKING_LOW,.tools=&alternate,.tool_count=1};
    CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_OK);
    f.model="";f.thinking=PANGU_THINKING_OFF;f.tools=0;
    config=(pangu_loop_config){.ctx=&f,.request=override_provider};
    CHECK(pangu_core_loop(owner,&prompt,1,&config)==PANGU_OK);
    size_t count=pangu_core_message_count(owner);config.override_fields=8;
    CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_INVALID && pangu_core_message_count(owner)==count && f.requests==10);
    CHECK(state_equal(pangu_core_model(owner)->id,"stored") && pangu_core_thinking(owner)==PANGU_THINKING_HIGH && pangu_core_tool_count(owner)==1);
    pangu_core_destroy(owner);
}
typedef struct { int requests,effects,events;size_t tool_count;const char *tool_name; } persistent_fixture;
static pangu_status persistent_validate(void *ctx,pangu_slice schema,pangu_slice args) {
    CHECK(ctx && state_equal(schema,"{}") && state_equal(args,"{}")); return PANGU_OK;
}
static pangu_status persistent_execute(void *ctx,pangu_core *c,const pangu_content *call,pangu_tool_sink sink,void *target) {
    persistent_fixture *f=ctx; (void)c; CHECK(state_equal(call->name,f->tool_name)); ++f->effects;
    pangu_message result={.role=PANGU_TOOL_RESULT}; return sink(target,&result,1);
}
static void persistent_event(void *ctx,pangu_core *c,const pangu_event *e) {
    persistent_fixture *f=ctx; ++f->events; (void)e;
    CHECK(pangu_core_set_model(c,pangu_core_model(c))==PANGU_BUSY);
    CHECK(pangu_core_set_thinking(c,PANGU_THINKING_OFF)==PANGU_BUSY);
    CHECK(pangu_core_set_tools(c,NULL,0)==PANGU_BUSY);
    CHECK(pangu_core_replace_messages(c,NULL,0)==PANGU_BUSY);
}
static pangu_status persistent_request(void *ctx,pangu_core *c,const pangu_core *view,pangu_model_sink sink,void *target) {
    persistent_fixture *f=ctx; (void)view;
    CHECK(state_equal(pangu_core_active_model(c)->id,"owned"));
    CHECK(pangu_core_active_thinking(c)==PANGU_THINKING_HIGH);
    CHECK(pangu_core_active_tool_count(c)==f->tool_count);
    if(f->tool_count==2)CHECK(pangu_core_tool_at(c,1)==pangu_core_tool_at(c,0)+1);
    pangu_content call={.kind=PANGU_TOOL_CALL,.name={f->tool_name,strlen(f->tool_name)},.id=S("one"),.data=S("{}")};
    pangu_message m={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    if(!(f->requests++%2)) { m.content=&call; m.content_count=1; m.stop_reason=PANGU_TOOL_USE; }
    return sink(target,&m,1);
}
static void persistent_state(int trace) {
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    CHECK(state_equal(pangu_core_model(c)->id,"unknown"));
    CHECK(state_equal(pangu_core_model(c)->provider,"unknown") && state_equal(pangu_core_model(c)->api,"unknown"));
    CHECK(state_equal(pangu_core_model(c)->metadata_json,"{\"name\":\"unknown\",\"baseUrl\":\"\",\"reasoning\":false,\"input\":[],\"cost\":{\"input\":0,\"output\":0,\"cacheRead\":0,\"cacheWrite\":0},\"contextWindow\":0,\"maxTokens\":0}"));
    CHECK(pangu_core_thinking(c)==PANGU_THINKING_OFF && !pangu_core_tool_count(c));
    CHECK(!pangu_core_model(NULL) && !pangu_core_tool_at(c,0));
    CHECK(pangu_core_set_model(c,NULL)==PANGU_INVALID);
    pangu_model_config invalid_model={.id={NULL,1}};
    CHECK(pangu_core_set_model(c,&invalid_model)==PANGU_INVALID);
    CHECK(pangu_core_set_thinking(c,(pangu_thinking_level)-1)==PANGU_INVALID);
    CHECK(pangu_core_set_tools(c,NULL,1)==PANGU_INVALID);
    pangu_tool invalid_tool={.schema_json={NULL,1}};
    CHECK(pangu_core_set_tools(c,&invalid_tool,1)==PANGU_INVALID);
    CHECK(pangu_core_set_tools(c,&invalid_tool,SIZE_MAX)==PANGU_INVALID);
    CHECK(pangu_core_replace_messages(c,NULL,1)==PANGU_INVALID);
    persistent_fixture f={.tool_count=2,.tool_name="echo"}; uint64_t id;
    CHECK(pangu_core_subscribe(c,persistent_event,&f,&id)==PANGU_OK);
    char name[]="owned",metadata[]="{}",toolname[]="echo",schema[]="{}",description[]="desc";
    pangu_model_config config={.id={name,5},.provider=S("script"),.api=S("fixture"),.metadata_json={metadata,2}};
    CHECK(pangu_core_set_model(c,&config)==PANGU_OK);
    CHECK(pangu_core_set_thinking(c,PANGU_THINKING_HIGH)==PANGU_OK);
    pangu_tool tools[2]={{.name={toolname,4},.description={description,4},.schema_json={schema,2},.ctx=&f,
        .validate=persistent_validate,.execute=persistent_execute,.has_constrained_sampling=1,.constrained_sampling=1},
        {.name=S("second"),.schema_json=S("{}"),.ctx=&f}};
    CHECK(pangu_core_set_tools(c,tools,2)==PANGU_OK);
    memset(name,'x',5); memset(metadata,'x',2); memset(toolname,'x',4); memset(schema,'x',2); memset(description,'x',4);
    memset(tools,0,sizeof(tools));
    CHECK(state_equal(pangu_core_model(c)->id,"owned") && state_equal(pangu_core_model(c)->metadata_json,"{}"));
    CHECK(state_equal(pangu_core_tool_at(c,0)->description,"desc"));
    CHECK(pangu_core_tool_at(c,0)->ctx==&f && pangu_core_tool_at(c,0)->constrained_sampling==1);
    CHECK(pangu_core_set_model(c,pangu_core_model(c))==PANGU_OK);
    CHECK(pangu_core_set_tools(c,pangu_core_tool_at(c,0),2)==PANGU_OK);
    pangu_message seed={.role=PANGU_CUSTOM,.custom_role=S("seed")};
    CHECK(pangu_core_replace_messages(c,&seed,1)==PANGU_OK);
    CHECK(pangu_core_replace_messages(c,NULL,0)==PANGU_OK);
    CHECK(!f.events);
    pangu_message prompt={.role=PANGU_USER};
    pangu_loop_config cfg={.ctx=&f,.request=persistent_request};
    for(int round=0;round<2;++round) {
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
        CHECK(f.effects==round+1 && f.requests==2*(round+1));
        CHECK(!pangu_core_active_model(c) && !pangu_core_active_tool_count(c));
        CHECK(pangu_core_thinking(c)==PANGU_THINKING_HIGH);
        CHECK(pangu_core_reset(c)==PANGU_OK);
        CHECK(state_equal(pangu_core_model(c)->id,"owned") && pangu_core_tool_count(c)==2);
        if(trace)printf("persistent:%d:%.*s:%d:%zu:%d:%d\n",round,(int)pangu_core_model(c)->id.len,pangu_core_model(c)->id.data,(int)pangu_core_thinking(c),pangu_core_tool_count(c),f.requests,f.effects);
    }
    pangu_tool switched=*pangu_core_tool_at(c,0);switched.name=S("switched");
    CHECK(pangu_core_set_tools(c,&switched,1)==PANGU_OK);f.tool_count=1;f.tool_name="switched";
    CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK && f.effects==3 && f.requests==6);
    CHECK(pangu_core_reset(c)==PANGU_OK && pangu_core_tool_count(c)==1);
    if(trace)printf("persistent:2:%.*s:%d:%zu:%d:%d\n",(int)pangu_core_model(c)->id.len,pangu_core_model(c)->id.data,(int)pangu_core_thinking(c),pangu_core_tool_count(c),f.requests,f.effects);
    CHECK(pangu_core_clear(c)==PANGU_OK && pangu_core_tool_count(c)==1);
    CHECK(pangu_core_set_tools(c,NULL,0)==PANGU_OK && !pangu_core_tool_count(c));
    CHECK(pangu_core_set_thinking(c,PANGU_THINKING_OFF)==PANGU_OK);
    CHECK(pangu_core_unsubscribe(c,id)==PANGU_OK); pangu_core_destroy(c);
}
static void persistent_failures(void) {
    /* Every allocation in aliased model/tools replacement must be atomic. */
    for(int kind=0;kind<2;++kind) for(size_t fail=1;fail<=2;++fail) {
        memory mem={0}; pangu_core_config allocator={.allocator={&mem,allocate,release}};
        pangu_core *c=NULL; CHECK(pangu_core_create(&allocator,&c)==PANGU_OK);
        pangu_model_config m={.id=S("before"),.metadata_json=S("{}")};
        pangu_tool tool={.name=S("echo"),.schema_json=S("{}"),.ctx=&mem};
        CHECK(pangu_core_set_model(c,&m)==PANGU_OK && pangu_core_set_tools(c,&tool,1)==PANGU_OK);
        const char *old=pangu_core_model(c)->id.data; const pangu_tool *oldtool=pangu_core_tool_at(c,0);
        mem.fail=mem.calls+fail;
        pangu_status s=kind?pangu_core_set_tools(c,oldtool,1):pangu_core_set_model(c,pangu_core_model(c));
        CHECK(s==(fail==1?PANGU_NOMEM:PANGU_OK));
        if(s==PANGU_NOMEM)CHECK(old==pangu_core_model(c)->id.data && oldtool==pangu_core_tool_at(c,0));
        CHECK(state_equal(pangu_core_model(c)->id,"before") && pangu_core_tool_at(c,0)->ctx==&mem);
        pangu_core_destroy(c); CHECK(!mem.live);
    }
    /* Error diagnostic initially borrows a transcript message. Replace both
     * with that message itself and with an empty transcript, at every OOM. */
    for(int empty=0;empty<2;++empty) {
        int completed=0;
        for(size_t fail=1;fail<20 && !completed;++fail) {
            memory mem={0}; pangu_core_config allocator={.allocator={&mem,allocate,release}};
            pangu_core *c=NULL; CHECK(pangu_core_create(&allocator,&c)==PANGU_OK);
            fixture f={.mode=ERROR_EXIT}; pangu_loop_config cfg={.ctx=&f,.model=model};
            pangu_content body={.kind=PANGU_TEXT,.data=S("p")};
            pangu_message prompt={.role=PANGU_USER,.content=&body,.content_count=1};
            CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_MODEL_ERROR);
            CHECK(enqueue(c,PANGU_STEERING,"s")==PANGU_OK);
            const pangu_message *old=pangu_core_message_at(c,1);
            const char *old_error=pangu_core_error(c).data;
            mem.fail=mem.calls+fail;
            pangu_status s=pangu_core_replace_messages(c,empty?NULL:old,empty?0:1);
            CHECK(s==PANGU_NOMEM || s==PANGU_OK);
            CHECK(state_equal(pangu_core_error(c),"script error") && pangu_core_queue_count(c,PANGU_STEERING)==1);
            if(s==PANGU_NOMEM)CHECK(pangu_core_message_at(c,1)==old && pangu_core_error(c).data==old_error);
            else {
                completed=1; CHECK(pangu_core_message_count(c)==(empty?0:1));
                mem.fail=0;
                CHECK(pangu_core_replace_messages(c,NULL,0)==PANGU_OK);
                CHECK(state_equal(pangu_core_error(c),"script error"));
                CHECK(pangu_core_clear_queue(c,PANGU_STEERING)==PANGU_OK);
                f.mode=ONE;
                CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK && !pangu_core_error(c).len);
                CHECK(pangu_core_clear(c)==PANGU_OK && !pangu_core_error(c).len);
            }
            pangu_core_destroy(c); CHECK(!mem.live);
        }
        CHECK(completed);
    }
    pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_message initial[2]={{.role=PANGU_USER},{.role=PANGU_CUSTOM,.custom_role=S("note")}};
    CHECK(pangu_core_replace_messages(c,initial,2)==PANGU_OK);
    pangu_message aliases[2]={*pangu_core_message_at(c,1),*pangu_core_message_at(c,0)};
    CHECK(pangu_core_replace_messages(c,aliases,2)==PANGU_OK);
    CHECK(state_equal(pangu_core_message_at(c,0)->custom_role,"note"));
    pangu_message invalid[2]={*pangu_core_message_at(c,0),{.role=(pangu_role)-1}};
    const pangu_message *old=pangu_core_message_at(c,0);
    CHECK(pangu_core_replace_messages(c,invalid,2)==PANGU_INVALID && pangu_core_message_at(c,0)==old);
    pangu_core_destroy(c);
}
int main(int argc,char **argv) {
    instance_overrides();
    if(argc==2 && !strcmp(argv[1],"--trace-persistent-state")) { persistent_state(1); return 0; }
    if(argc==2 && !strcmp(argv[1],"--persistent-state")) { persistent_state(0); persistent_failures(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-settlement")) { settlement_barrier(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-legacy-hooks")) { legacy_hooks(1); return 0; }
    legacy_hooks(0);
    if (argc==2 && !strcmp(argv[1],"--trace-streaming-busy")) { streaming_busy(1); return 0; }
    streaming_busy(0);
    if (argc==2 && !strcmp(argv[1],"--trace-continuation")) { continuation_startup(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-late-updates")) { late_tool_updates(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-cancel-queues")) { cancelled_queue_resume(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-state")) { tracing=1; queue_preview(); state_observations(); return 0; }
    persistent_state(0); persistent_failures();
    continuation_startup(0); late_tool_updates(0); settlement_barrier(0); default_mutators_reset();
    cancelled_queue_resume(0); truncate_queues(); queue_preview(); state_observations();
    subscriptions();
    allocation_failures();
    for (int mode=ONE;mode<=FOLLOW_RESUME;++mode) {
        fixture f={.mode=mode}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        CHECK(enqueue(c,PANGU_STEERING,"1")==PANGU_OK);
        CHECK(enqueue(c,PANGU_STEERING,"2")==PANGU_OK);
        CHECK(enqueue(c,PANGU_FOLLOW_UP,"f")==PANGU_OK);
        CHECK(enqueue(c,PANGU_FOLLOW_UP,"g")==PANGU_OK);
        if (mode==ALL) {
            CHECK(pangu_core_queue_mode(c,PANGU_STEERING,PANGU_QUEUE_ALL)==PANGU_OK);
            CHECK(pangu_core_queue_mode(c,PANGU_FOLLOW_UP,PANGU_QUEUE_ALL)==PANGU_OK);
        }
        pangu_loop_config cfg={.ctx=&f,.model=model,.finish_turn=finish,.prepare_next_turn=next};
        pangu_content b={.kind=PANGU_TEXT,.data=S("p")};
        pangu_message prompt={.role=PANGU_USER,.content=&b,.content_count=1};
        if (mode==RESUME || mode==FOLLOW_RESUME) {
            pangu_message assistant={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
            CHECK(pangu_core_append(c,&assistant)==PANGU_OK);
            if (mode==FOLLOW_RESUME) CHECK(pangu_core_clear_queue(c,PANGU_STEERING)==PANGU_OK);
        }
        pangu_status s=pangu_core_run(c,mode>=RESUME?NULL:&prompt,mode>=RESUME?0:1,&cfg);
        CHECK(s==(mode==ERROR_EXIT?PANGU_MODEL_ERROR:PANGU_OK));
        if (mode==ONE) CHECK(!strcmp(f.seen,"12fg"));
        if (mode==ALL) CHECK(!strcmp(f.seen,"2g"));
        if (mode==HOOK_QUEUE) CHECK(!strcmp(f.seen,"123fg"));
        if (mode==FINISH_END || mode==ERROR_EXIT || mode==RESUME) {
            CHECK(!strcmp(f.seen,"1"));
            CHECK(pangu_core_queue_count(c,PANGU_STEERING)==1 && pangu_core_queue_count(c,PANGU_FOLLOW_UP)==2);
        }
        if (mode==FOLLOW_RESUME) { CHECK(!strcmp(f.seen,"f")); CHECK(pangu_core_queue_count(c,PANGU_FOLLOW_UP)==1); }
        if (mode==ERROR_EXIT) CHECK(pangu_core_error(c).len==12);
        printf("%d:%s\n",mode,f.seen);
        CHECK(!pangu_core_streaming_message(c));
        CHECK(pangu_core_reset(c)==PANGU_OK);
        CHECK(!pangu_core_queue_count(c,PANGU_STEERING) && !pangu_core_queue_count(c,PANGU_FOLLOW_UP));
        CHECK(!pangu_core_error(c).len);
        pangu_core_destroy(c);
    }
    return 0;
}
