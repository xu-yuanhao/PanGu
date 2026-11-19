/* Pi agent-loop.test.ts: completion order vs transcript source order,
 * per-tool sequential override, preflight cancellation and batch terminate. */
#include <pangu/core/state.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) abort(); } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
typedef struct {
    pangu_tool_sink sink; void *target;
    int active, done, id;
} operation;
typedef struct {
    operation ops[3];
    int starts, releases, before, after, requests, pump_calls, active, max_active;
    int mode, terminate_count;
    char trace[128]; size_t n;
    int64_t result_timestamps[3];
} fixture;
enum { PARALLEL, SEQUENTIAL, OVERRIDE, BLOCK_SECOND, CANCEL_PREFLIGHT,
    PUMP_ERROR, START_ERROR, ALL_TERMINATE, MIXED_TERMINATE, CANCEL_ACTIVE,
    MIXED_DEFINITIONS, PARALLEL_DEFINITIONS, QUEUED_PARALLEL, QUEUED_SEQUENTIAL, DUPLICATE_DEFINITIONS };
static void log_event(fixture *f,char c) { CHECK(f->n+1<sizeof(f->trace)); f->trace[f->n++]=c; f->trace[f->n]=0; }
static pangu_status model(void *ctx,pangu_core *c,pangu_model_sink sink,void *target) {
    fixture *f=ctx; ++f->requests;
    pangu_content calls[]={
        {.kind=PANGU_TOOL_CALL,.id=S("a"),.name=S("tool"),.data=S("{}")},
        {.kind=PANGU_TOOL_CALL,.id=S("b"),.name=S("tool"),.data=S("{}")},
        {.kind=PANGU_TOOL_CALL,.id=S("c"),.name=S("tool"),.data=S("{}")}};
    pangu_message m={.role=PANGU_ASSISTANT,.content=calls,.content_count=3,.stop_reason=PANGU_TOOL_USE};
    if(f->mode>=MIXED_DEFINITIONS && f->mode<=QUEUED_SEQUENTIAL)calls[1].name=S("other");
    if (f->requests>1) {
        if((f->mode==QUEUED_PARALLEL || f->mode==QUEUED_SEQUENTIAL)){
            CHECK(!f->active && f->starts==3 && f->releases==3);
            CHECK(!pangu_core_queue_count(c,PANGU_STEERING));
            const pangu_message *last=pangu_core_message_at(c,pangu_core_message_count(c)-1);
            CHECK(last->role==PANGU_USER && last->content[0].data.len==9 && !memcmp(last->content[0].data.data,"interrupt",9));
        }
        m.content=NULL; m.content_count=0; m.stop_reason=PANGU_STOP;
        if (pangu_core_is_cancelled(c)) m.stop_reason=PANGU_ABORTED;
    }
    return sink(target,&m,1);
}
static pangu_status prepare(void *ctx,pangu_slice input,pangu_arguments_sink sink,void *target) {
    (void)ctx; CHECK(input.len==2); return sink(target,S("{}"));
}
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)ctx; CHECK(schema.len==2 && args.len==2); return PANGU_OK;
}
static pangu_before_result before(void *ctx,pangu_core *c,const pangu_message *m,const pangu_content *call) {
    fixture *f=ctx; (void)m;
    CHECK(pangu_core_message_count(c)==3); /* results committed only after batch */
    ++f->before; log_event(f,call->id.data[0]);
    if (f->mode==CANCEL_PREFLIGHT && f->before==2) pangu_core_cancel(c);
    return (pangu_before_result){.block=f->mode==BLOCK_SECOND && f->before==2};
}
static pangu_status start(void *ctx,pangu_core *c,const pangu_content *call,pangu_tool_sink sink,void *target,void **handle) {
    fixture *f=ctx; (void)c;
    int id=call->id.data[0]-'a';
    if (f->mode!=SEQUENTIAL && f->mode!=OVERRIDE && f->mode!=MIXED_DEFINITIONS && f->mode!=QUEUED_SEQUENTIAL) CHECK(f->before==(f->mode==CANCEL_PREFLIGHT?2:3));
    if (f->mode==START_ERROR && id==1) return PANGU_TOOL_ERROR;
    operation *op=&f->ops[id];
    *op=(operation){.sink=sink,.target=target,.active=1,.id=id};
    *handle=op; ++f->starts; ++f->active;
    if (f->active>f->max_active) f->max_active=f->active;
    log_event(f,'A'+id); return PANGU_OK;
}
static void release(void *ctx,void *handle) {
    fixture *f=ctx;
    if (!handle) return;
    operation *op=handle; CHECK(op->active);
    op->active=0; ++f->releases;
    if (!op->done) --f->active;
    op->sink=NULL; op->target=NULL;
}
static pangu_status pump(void *ctx,pangu_core *c) {
    fixture *f=ctx; ++f->pump_calls;
    if((f->mode==QUEUED_PARALLEL || f->mode==QUEUED_SEQUENTIAL) && f->pump_calls==1){
        pangu_content text={.kind=PANGU_TEXT,.data=S("interrupt")};
        pangu_message input={.role=PANGU_USER,.content=&text,.content_count=1};
        CHECK(pangu_core_enqueue(c,PANGU_STEERING,&input)==PANGU_OK);
    }
    if((f->mode==QUEUED_PARALLEL || f->mode==QUEUED_SEQUENTIAL))CHECK(pangu_core_queue_count(c,PANGU_STEERING)==1 && f->requests==1);
    if (f->mode==PUMP_ERROR) return PANGU_TOOL_ERROR;
    if (f->mode==CANCEL_ACTIVE && f->pump_calls==1) pangu_core_cancel(c);
    for (int id=2;id>=0;--id) {
        operation *op=&f->ops[id];
        if (!op->active || op->done) continue;
        pangu_content b={.kind=PANGU_TEXT,.data=S("completed")};
        pangu_message result={.role=PANGU_TOOL_RESULT,.content=&b,.content_count=1,
            .terminate=(f->mode==ALL_TERMINATE || (f->mode==MIXED_TERMINATE && id!=1))};
        if (pangu_core_is_cancelled(c)) result.is_error=1;
        op->done=1; --f->active;
        pangu_status status=op->sink(op->target,&result,1);
        if (status!=PANGU_OK) return status;
        return op->sink(op->target,&result,0); /* late async update ignored */
    }
    CHECK(0); return PANGU_INVALID;
}
static pangu_status after(void *ctx,pangu_core *c,const pangu_content *call,const pangu_message *m,pangu_tool_sink sink,void *target) {
    fixture *f=ctx; (void)call; (void)m; (void)sink; (void)target;
    CHECK(pangu_core_message_count(c)==3); ++f->after; return PANGU_OK;
}
static void event(void *ctx,pangu_core *c,const pangu_event *e) {
    fixture *f=ctx; (void)c;
    if (e->kind==PANGU_TURN_END) {
        CHECK(e->new_messages && e->tool_results);
        CHECK(e->message_count==pangu_core_message_count(e->new_messages));
    }
    if (e->kind==PANGU_TOOL_START) CHECK(pangu_core_pending_tool_count(c)>0);
    if (e->kind==PANGU_TOOL_END) {
        for (size_t i=0;i<pangu_core_pending_tool_count(c);++i)
            CHECK(pangu_core_pending_tool_at(c,i)!=e->tool_call);
        log_event(f,'1'+e->tool_call->id.data[0]-'a');
    }
    if ((e->kind==PANGU_MESSAGE_START || e->kind==PANGU_MESSAGE_END) &&
        e->message->role==PANGU_TOOL_RESULT) {
        size_t index=(size_t)(e->message->tool_call_id.data[0]-'a');CHECK(index<3);
        if (e->kind==PANGU_MESSAGE_START) {
            CHECK(e->message->timestamp_ms>0);
            f->result_timestamps[index]=e->message->timestamp_ms;
        } else CHECK(e->message->timestamp_ms==f->result_timestamps[index]);
    }
    if (e->kind==PANGU_MESSAGE_END && e->message->role==PANGU_TOOL_RESULT)
        log_event(f,'x'+e->message->tool_call_id.data[0]-'a');
    if((f->mode==QUEUED_PARALLEL || f->mode==QUEUED_SEQUENTIAL) && e->kind==PANGU_MESSAGE_START && e->message->role==PANGU_USER &&
       e->message->content_count && e->message->content[0].data.len==9){
        CHECK(f->starts==3 && !f->active && f->pump_calls==3);log_event(f,'q');
    }
}
static pangu_turn_decision finish_context(void *ctx,pangu_core *owner,const pangu_turn_context *turn) {
    fixture *f=ctx; (void)owner;
    CHECK(turn->assistant->role==PANGU_ASSISTANT && turn->new_messages);
    size_t expected=f->requests==1?(f->mode==CANCEL_PREFLIGHT?2:3):0;
    CHECK(pangu_core_message_count(turn->tool_results)==expected);
    for (size_t i=0;i<expected;++i) {
        const pangu_message *m=pangu_core_message_at(turn->tool_results,i);
        CHECK(m->role==PANGU_TOOL_RESULT && m->tool_call_id.data[0]=='a'+(int)i);
        CHECK(m->timestamp_ms==f->result_timestamps[i] && m->timestamp_ms>0);
    }
    return PANGU_TURN_AUTO;
}
typedef struct { size_t calls, fail, live; } memory;
static void *allocate(void *ctx,size_t n) {
    memory *m=ctx; if (++m->calls==m->fail) return NULL;
    void *p=malloc(n); if (p) ++m->live; return p;
}
static void deallocate(void *ctx,void *p) {
    if (p) { --((memory *)ctx)->live; free(p); }
}
typedef struct { operation ops[2]; int starts,ends,released,trace; } duplicate_fixture;
static pangu_status duplicate_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    (void)ctx; (void)owner;
    pangu_content calls[]={{.kind=PANGU_TOOL_CALL,.id=S("duplicate"),.name=S("tool"),.data=S("{}")},
        {.kind=PANGU_TOOL_CALL,.id=S("duplicate"),.name=S("tool"),.data=S("{}")}};
    pangu_message m={.role=PANGU_ASSISTANT,.content=calls,.content_count=2,.stop_reason=PANGU_TOOL_USE};
    return sink(target,&m,1);
}
static pangu_status duplicate_start(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target,void **handle) {
    (void)owner; (void)call; duplicate_fixture *f=ctx; CHECK(f->starts<2);
    operation *op=&f->ops[f->starts++]; *op=(operation){.sink=sink,.target=target,.active=1}; *handle=op; return PANGU_OK;
}
static void duplicate_release(void *ctx,void *handle) {
    duplicate_fixture *f=ctx; operation *op=handle; CHECK(op->active); op->active=0; ++f->released;
}
static pangu_status duplicate_pump(void *ctx,pangu_core *owner) {
    duplicate_fixture *f=ctx;
    CHECK(pangu_core_pending_tool_count(owner)==(f->ends?0u:1u));
    for (size_t i=0;i<2;++i) if (!f->ops[i].done) {
        operation *op=&f->ops[i]; op->done=1;
        pangu_message result={.role=PANGU_TOOL_RESULT}; return op->sink(op->target,&result,1);
    }
    return PANGU_INVALID;
}
static void duplicate_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    duplicate_fixture *f=ctx;
    if (event->kind==PANGU_TOOL_START) {
        CHECK(pangu_core_pending_tool_count(owner)==1);
        CHECK(pangu_core_pending_tool_at(owner,0)->id.len==9 && !pangu_core_pending_tool_at(owner,1));
        if (f->trace) puts("start:1");
    }
    if (event->kind==PANGU_TOOL_END) {
        ++f->ends; CHECK(!pangu_core_pending_tool_count(owner) && !pangu_core_pending_tool_at(owner,0));
        if (f->trace) puts("end:0");
    }
}
static pangu_turn_decision duplicate_finish(void *ctx,pangu_core *owner,const pangu_message *message) {
    (void)ctx; (void)owner; (void)message; return PANGU_TURN_END_RUN;
}
static void duplicate_ids(int trace) {
    pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    duplicate_fixture f={.trace=trace};
    pangu_tool tool={.name=S("tool"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.start=duplicate_start,.release=duplicate_release};
    pangu_loop_config cfg={.ctx=&f,.model=duplicate_model,.tools=&tool,.tool_count=1,.pump=duplicate_pump,.event=duplicate_event,.finish_turn=duplicate_finish};
    pangu_message prompt={.role=PANGU_USER}; CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK);
    CHECK(f.starts==2 && f.ends==2 && f.released==2 && !pangu_core_pending_tool_count(owner));
    pangu_core_destroy(owner);
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--trace-pending")) { duplicate_ids(1); return 0; }
    duplicate_ids(0);
    for (int mode=PARALLEL;mode<=DUPLICATE_DEFINITIONS;++mode) {
      for (size_t fail=0;fail<(mode==PARALLEL?300:1);++fail) {
        fixture f={.mode=mode}; pangu_core *c=NULL;
        memory mem={.fail=fail};
        pangu_core_config memory_config={.allocator={&mem,allocate,deallocate}};
        pangu_status created=pangu_core_create(&memory_config,&c);
        if (created!=PANGU_OK) { CHECK(created==PANGU_NOMEM && !mem.live); continue; }
        pangu_tool t={.name=S("tool"),.schema_json=S("{}"),.ctx=&f,.validate=validate,.prepare_arguments=prepare,.start=start,.release=release,
            .execution_mode=mode==OVERRIDE?PANGU_TOOLS_SEQUENTIAL:PANGU_TOOLS_PARALLEL};
        pangu_tool tools[]={t,t};tools[1].name=S("other");
        if(mode==MIXED_DEFINITIONS || mode==DUPLICATE_DEFINITIONS)tools[1].execution_mode=PANGU_TOOLS_SEQUENTIAL;
        if(mode==DUPLICATE_DEFINITIONS)tools[1].name=S("tool");
        pangu_loop_config cfg={.ctx=&f,.model=model,.tools=tools,.tool_count=mode>=MIXED_DEFINITIONS?2:1,.event=event,
            .before_tool=before,.after_tool=after,.pump=pump,.finish_turn_context=finish_context,
            .tool_execution=(mode==SEQUENTIAL || mode==QUEUED_SEQUENTIAL)?PANGU_TOOLS_SEQUENTIAL:PANGU_TOOLS_PARALLEL};
        pangu_message prompt={.role=PANGU_USER};
        pangu_status s=pangu_core_run(c,&prompt,1,&cfg);
        if (s==PANGU_NOMEM) {
            CHECK(f.starts==f.releases && f.active==0);
            CHECK(!pangu_core_is_running(c));
            pangu_core_destroy(c); CHECK(!mem.live); continue;
        }
        CHECK(s==(mode==PUMP_ERROR?PANGU_TOOL_ERROR:
            (mode==CANCEL_PREFLIGHT || mode==CANCEL_ACTIVE)?PANGU_CANCELLED:PANGU_OK));
        CHECK(f.starts==f.releases && f.active==0);
        if (mode==PARALLEL || mode==PARALLEL_DEFINITIONS || mode==DUPLICATE_DEFINITIONS) { CHECK(!strcmp(f.trace,"abcABC321xyz")); CHECK(f.max_active==3); }
        if (mode==SEQUENTIAL || mode==OVERRIDE || mode==MIXED_DEFINITIONS) {
            CHECK(!strcmp(f.trace,"aA1xbB2ycC3z")); CHECK(f.max_active==1);
        }
        if (mode==BLOCK_SECOND) CHECK(!strcmp(f.trace,"ab2cAC31xyz"));
        if(mode==QUEUED_PARALLEL)CHECK(!strcmp(f.trace,"abcABC321xyzq") && f.max_active==3 && f.requests==2);
        if(mode==QUEUED_SEQUENTIAL)CHECK(!strcmp(f.trace,"aA1xbB2ycC3zq") && f.max_active==1 && f.requests==2);
        if (mode==CANCEL_PREFLIGHT) { CHECK(f.starts==0 && f.before==2); CHECK(!strcmp(f.trace,"ab21xy")); }
        if (mode==ALL_TERMINATE) CHECK(f.requests==1);
        if (mode==MIXED_TERMINATE) CHECK(f.requests==2);
        if (mode!=PUMP_ERROR) {
            size_t results=mode==CANCEL_PREFLIGHT?2:3;
            for (size_t i=0;i<results;++i) {
                const pangu_message *m=pangu_core_message_at(c,3+i);
                CHECK(m->role==PANGU_TOOL_RESULT && m->tool_call_id.data[0]=='a'+(int)i);
            }
        }
        if (!fail) printf("%d:%s\n",mode,f.trace);
        pangu_core_destroy(c); CHECK(!mem.live);
        if (fail) break;
      }
    }
    return 0;
}
