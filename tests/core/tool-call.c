#include <pangu/core/state.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); abort(); } } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
enum { KEEP, CONTENT, STRUCTURED, BOTH, DETAILS, FLAGS, EMPTY, BLOCKED,
    INVALID, PREP_FAIL, UNKNOWN, EXEC_FAIL, AFTER_FAIL, CANCELLED, NESTED, NORMALIZE, ARGUMENT_ERROR };
typedef struct { int mode,prepared,before,executed,after,updates,events,depth,requests; uint64_t ns; int64_t result_timestamp; pangu_loop_config *config; } fixture;
static int same(pangu_slice a,const char *b) { return a.len==strlen(b) && (!a.len || !memcmp(a.data,b,a.len)); }
static uint64_t clock_ns(void *ctx) { return ((fixture *)ctx)->ns; }
static pangu_status prepare(void *ctx,pangu_slice raw,pangu_arguments_sink sink,void *target) {
    fixture *f=ctx; CHECK(same(raw,"{\"old\":true}")); ++f->prepared;
    if (f->mode==PREP_FAIL) return PANGU_TOOL_ERROR;
    char value[]="{\"new\":true}";
    pangu_status s=sink(target,(pangu_slice){value,sizeof(value)-1});
    memset(value,'x',sizeof(value)-1); /* prepared bytes must already be owned */
    return s;
}
static pangu_status validate(void *ctx,pangu_slice schema,pangu_slice args) {
    fixture *f=ctx; CHECK(schema.len==2 && same(args,"{\"new\":true}"));
    return f->mode==INVALID?PANGU_INVALID:PANGU_OK;
}
static pangu_status normalize(void *ctx,pangu_slice schema,pangu_slice args,pangu_arguments_sink sink,void *target) {
    (void)ctx; CHECK(schema.len==2 && same(args,"{\"new\":true}"));
    char normalized[]="{\"new\":true,\"validated\":true}";
    pangu_status status=sink(target,(pangu_slice){normalized,sizeof(normalized)-1});
    memset(normalized,'?',sizeof(normalized)-1); return status;
}
static pangu_status unused_validator(void *ctx,pangu_slice schema,pangu_slice args,pangu_arguments_sink sink,void *target) {
    (void)ctx;(void)schema;(void)args;(void)sink;(void)target;CHECK(0);return PANGU_INVALID;
}
static pangu_status normalize_call(void *ctx,pangu_core *owner,const pangu_content *call,pangu_slice schema,pangu_slice args,pangu_arguments_sink sink,void *target) {
    CHECK(pangu_core_is_running(owner) && same(call->name,"tool") && same(call->data,"{\"old\":true}"));
    return normalize(ctx,schema,args,sink,target);
}
static pangu_before_result before(void *ctx,pangu_core *c,const pangu_tool_context *input) {
    fixture *f=ctx; ++f->before; f->ns+=UINT64_C(100000000); /* preflight excluded */
    CHECK(input->assistant->role==PANGU_ASSISTANT && same(input->call->data,"{\"old\":true}") && same(input->args_json,f->mode==NORMALIZE?"{\"new\":true,\"validated\":true}":"{\"new\":true}"));
    if (f->mode==CANCELLED) pangu_core_cancel(c);
    return (pangu_before_result){.block=f->mode==BLOCKED,.reason=S("nope"),.terminate=1};
}
static pangu_status update(void *ctx,const pangu_message *result,int final) {
    fixture *f=ctx; CHECK(!final && result->role==PANGU_TOOL_RESULT); ++f->updates; return PANGU_OK;
}
static pangu_status execute(void *ctx,pangu_core *c,const pangu_content *call,pangu_tool_sink sink,void *target) {
    fixture *f=ctx; ++f->executed; CHECK(same(call->data,f->mode==NORMALIZE?"{\"new\":true,\"validated\":true}":"{\"new\":true}"));
    if (f->mode==NESTED && !f->depth) {
        f->depth=1;
        pangu_message assistant={.role=PANGU_ASSISTANT};
        pangu_content nested={.kind=PANGU_TOOL_CALL,.name=S("tool"),.id=S("nested"),.data=S("{\"old\":true}")};
        pangu_tool_outcome result={0};
        pangu_status s=pangu_core_call_tool(c,&assistant,&nested,f->config,NULL,NULL,&result);
        if (s!=PANGU_OK) return s;
        CHECK(result.has_duration && pangu_core_message_count(result.result)==1);
        pangu_core_destroy(result.result); f->depth=0;
    }
    f->ns+=UINT64_C(26500000);
    if (f->mode==EXEC_FAIL) return PANGU_TOOL_ERROR;
    pangu_content b={.kind=PANGU_TEXT,.data=S("original")};
    pangu_message m={.role=PANGU_TOOL_RESULT,.content=&b,.content_count=1,.details_json=S("{\"note\":\"original\"}"),
        .structured_json=S("{\"value\":\"original\"}"),.usage={.total=7},.is_error=1,.terminate=1,.timestamp_ms=123};
    pangu_status s=sink(target,&m,0); if (s!=PANGU_OK) return s;
    s=sink(target,&m,1);
    if (s!=PANGU_OK) return s;
    return sink(target,&m,0); /* late update is ignored after result settlement */
}
static pangu_status after(void *ctx,pangu_core *c,const pangu_tool_context *input,pangu_patch_sink sink,void *target) {
    fixture *f=ctx; (void)c; ++f->after; f->ns+=UINT64_C(500000000); /* excluded from duration */
    CHECK(same(input->args_json,f->mode==NORMALIZE?"{\"new\":true,\"validated\":true}":"{\"new\":true}") && input->result);
    if (f->mode==AFTER_FAIL) return PANGU_TOOL_ERROR;
    pangu_content b={.kind=PANGU_TEXT,.data=S("redacted")};
    pangu_tool_patch p={.content=&b,.content_count=1,.structured_json=S("{\"value\":\"replaced\"}"),
        .details_json=S("{\"note\":\"kept\"}"),.usage={.total=12}};
    if (f->mode==CONTENT) p.fields=PANGU_PATCH_CONTENT;
    if (f->mode==STRUCTURED) p.fields=PANGU_PATCH_STRUCTURED;
    if (f->mode==BOTH) p.fields=PANGU_PATCH_CONTENT|PANGU_PATCH_STRUCTURED;
    if (f->mode==DETAILS) p.fields=PANGU_PATCH_DETAILS|PANGU_PATCH_USAGE;
    if (f->mode==FLAGS) p.fields=PANGU_PATCH_ERROR|PANGU_PATCH_TERMINATE;
    if (f->mode==EMPTY) { p.fields=PANGU_PATCH_CONTENT; p.content=NULL; p.content_count=0; }
    if (!p.fields) return PANGU_OK;
    pangu_status s=sink(target,&p);
    /* input result is valid until after callback returns, even after patch. */
    CHECK(input->result->role==PANGU_TOOL_RESULT);
    return s;
}
static void event(void *ctx,pangu_core *c,const pangu_event *e) { (void)c; (void)e; ++((fixture *)ctx)->events; }
static void scenario(int mode,pangu_allocator *memory,int trace) {
    fixture f={.mode=mode}; pangu_core *c=NULL;
    pangu_core_config memory_config={.allocator=memory?*memory:(pangu_allocator){0}};
    pangu_status created=pangu_core_create(&memory_config,&c);
    if (created!=PANGU_OK) { CHECK(created==PANGU_NOMEM); return; }
    uint64_t subscription=0; CHECK(pangu_core_subscribe(c,event,&f,&subscription)==PANGU_OK);
    pangu_tool t={.ctx=&f,.name=S("tool"),.schema_json=S("{}"),.prepare_arguments=prepare,.validate=validate,.execute=execute};
    if (mode==NORMALIZE) { t.validate_arguments=unused_validator; t.validate_call=normalize_call; }
    pangu_loop_config cfg={.ctx=&f,.tools=&t,.tool_count=1,.event=event,.before_tool_context=before,.after_tool_patch=after,.monotonic_ns=clock_ns}; f.config=&cfg;
    pangu_message assistant={.role=PANGU_ASSISTANT,.stop_reason=PANGU_LENGTH}; /* standalone ignores truncation policy */
    pangu_content call={.kind=PANGU_TOOL_CALL,.name=mode==UNKNOWN?S("missing"):S("tool"),.id=S("one"),.data=S("{\"old\":true}")};
    if (mode==ARGUMENT_ERROR) call.arguments_error=S("truncated JSON");
    pangu_tool_outcome outcome={0};
    pangu_status s=pangu_core_call_tool(c,&assistant,&call,&cfg,update,&f,&outcome);
    CHECK(s==PANGU_OK);
    const pangu_message *result=pangu_core_message_at(outcome.result,0);
    CHECK(result && pangu_core_message_count(outcome.result)==1 && pangu_core_message_count(c)==0 && f.events==0);
    CHECK(!pangu_core_is_running(c));
    int ran=mode!=BLOCKED && mode!=INVALID && mode!=PREP_FAIL && mode!=UNKNOWN && mode!=CANCELLED && mode!=ARGUMENT_ERROR;
    CHECK(outcome.has_duration==ran && f.executed==ran+(mode==NESTED) && f.after==f.executed);
    if (ran && mode!=NESTED) CHECK(outcome.duration_ms==27);
    if (mode==CONTENT || mode==EMPTY) CHECK(!result->structured_json.len);
    if (mode==STRUCTURED || mode==BOTH) CHECK(same(result->structured_json,"{\"value\":\"replaced\"}"));
    if (mode==KEEP || mode==DETAILS || mode==FLAGS) CHECK(same(result->structured_json,"{\"value\":\"original\"}"));
    if (mode==FLAGS) CHECK(!result->is_error && !result->terminate);
    if (mode==DETAILS) CHECK(result->usage.total==12 && same(result->details_json,"{\"note\":\"kept\"}"));
    if (mode==CONTENT || mode==BOTH) CHECK(same(result->content[0].data,"redacted"));
    if (mode==EMPTY) CHECK(!result->content_count);
    if (mode==BLOCKED) CHECK(result->is_error && result->terminate && same(result->content[0].data,"nope") && !f.after);
    if (!ran) CHECK(result->is_error && !f.after);
    if (mode==ARGUMENT_ERROR) {
        CHECK(f.prepared==1 && !f.before && !f.executed);
        CHECK(same(result->content[0].data,"Invalid arguments for tool \"tool\": truncated JSON"));
    }
    if (mode>=CONTENT && mode<=DETAILS) printf("patch:%d:%.*s\n",mode,(int)result->structured_json.len,
        result->structured_json.data?result->structured_json.data:"");
    if (trace==2)printf("%.*s\n",(int)result->content[0].data.len,result->content[0].data.data);
    if (trace==1) {
        pangu_slice text=result->content_count?result->content[0].data:(pangu_slice){0};
        pangu_slice structured=result->structured_json.len?result->structured_json:S("null");
        pangu_slice details=result->details_json.len?result->details_json:S("{}");
        printf("outcome:{\"mode\":%d,\"text\":\"%.*s\",\"contentCount\":%zu,\"structured\":%.*s,\"details\":%.*s,\"usage\":%llu,\"error\":%d,\"terminate\":%d,\"timed\":%d,\"prepared\":%d,\"before\":%d,\"executed\":%d,\"after\":%d,\"updates\":%d}\n",
            mode,(int)text.len,text.data?text.data:"",result->content_count,(int)structured.len,structured.data,
            (int)details.len,details.data,(unsigned long long)result->usage.total,!!result->is_error,!!result->terminate,
            !!outcome.has_duration,f.prepared,f.before,f.executed,f.after,f.updates);
    }
    CHECK(same(call.data,"{\"old\":true}"));
    pangu_core_destroy(outcome.result); pangu_core_destroy(c);
}
static pangu_status model(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    fixture *f=ctx; (void)owner;
    pangu_content call={.kind=PANGU_TOOL_CALL,.name=S("tool"),.id=S("one"),.data=S("{\"old\":true}")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
    if (++f->requests==2) {
        const pangu_message *result=pangu_core_message_at(view,pangu_core_message_count(view)-1);
        CHECK(result->role==PANGU_TOOL_RESULT && !result->structured_json.len && !result->terminate);
        CHECK(result->has_duration && result->duration_ms==27 && !result->is_error);
        CHECK(result->timestamp_ms>123 && result->timestamp_ms==f->result_timestamp);
        m.content=NULL; m.content_count=0; m.stop_reason=PANGU_STOP;
    }
    return sink(target,&m,1);
}
static void rich_event(void *ctx,pangu_core *c,const pangu_event *e) {
    fixture *f=ctx; (void)c;
    if (e->kind==PANGU_TOOL_END) {
        CHECK(e->has_duration && e->duration_ms==27);
        CHECK(same(e->message->structured_json,"{\"value\":\"original\"}")); ++f->events;
    }
    if (e->kind==PANGU_MESSAGE_START && e->message->role==PANGU_TOOL_RESULT) {
        CHECK(e->message->timestamp_ms>123);f->result_timestamp=e->message->timestamp_ms;
    }
    if (e->kind==PANGU_MESSAGE_END && e->message->role==PANGU_TOOL_RESULT) {
        CHECK(!e->message->structured_json.len && e->message->timestamp_ms==f->result_timestamp);
    }
}
static void loop_result(int trace) {
    fixture f={.mode=FLAGS}; pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
    pangu_tool t={.ctx=&f,.name=S("tool"),.schema_json=S("{}"),.prepare_arguments=prepare,.validate=validate,.execute=execute};
    pangu_loop_config cfg={.ctx=&f,.request=model,.tools=&t,.tool_count=1,.event=rich_event,
        .before_tool_context=before,.after_tool_patch=after,.monotonic_ns=clock_ns};
    pangu_message prompt={.role=PANGU_USER};
    CHECK(pangu_core_run(c,&prompt,1,&cfg)==PANGU_OK);
    CHECK(f.requests==2 && f.executed==1 && f.events==1);
    if(trace==1)puts("{\"fresh\":true,\"stable\":true}");
    if(trace==2)puts("{\"eventDuration\":27,\"resultDuration\":27,\"effects\":1}");
    if(trace==3){
        const pangu_message *raw=pangu_core_message_at(c,2);
        CHECK(raw && raw->role==PANGU_ASSISTANT && same(raw->content[0].data,"{\"old\":true}"));
        printf("{\"prepared\":%d,\"before\":%d,\"executed\":%d,\"after\":%d,\"requests\":%d,\"rawUnchanged\":true}\n",f.prepared,f.before,f.executed,f.after,f.requests);
    }
    pangu_core_destroy(c);
}
typedef struct { size_t calls,fail,live; } memory;
static void *allocate(void *ctx,size_t n) {
    memory *m=ctx; if (++m->calls==m->fail) return NULL;
    void *p=malloc(n); if (p) ++m->live; return p;
}
static void release(void *ctx,void *p) { if (p) { --((memory *)ctx)->live; free(p); } }
static void allocation_failures(void) {
    for (int nested=0;nested<3;++nested) {
        int completed=0;
        for (size_t fail=1;fail<100;++fail) {
            memory mem={0}; fixture f={.mode=nested==2?ARGUMENT_ERROR:nested?NESTED:CONTENT}; pangu_core *c=NULL;
            pangu_core_config memory_config={.allocator={&mem,allocate,release}};
            CHECK(pangu_core_create(&memory_config,&c)==PANGU_OK);
            pangu_tool t={.ctx=&f,.name=S("tool"),.schema_json=S("{}"),.prepare_arguments=prepare,.validate=validate,.execute=execute};
            pangu_loop_config cfg={.ctx=&f,.tools=&t,.tool_count=1,.before_tool_context=before,.after_tool_patch=after,.monotonic_ns=clock_ns}; f.config=&cfg;
            pangu_message assistant={.role=PANGU_ASSISTANT};
            pangu_content call={.kind=PANGU_TOOL_CALL,.name=S("tool"),.id=S("oom"),.data=S("{\"old\":true}")};
            if(nested==2)call.arguments_error=S("truncated JSON");
            pangu_tool_outcome outcome={0}; mem.fail=mem.calls+fail;
            pangu_status status=pangu_core_call_tool(c,&assistant,&call,&cfg,NULL,NULL,&outcome);
            if (status==PANGU_NOMEM) CHECK(!outcome.result && !outcome.has_duration && !outcome.duration_ms);
            else { CHECK(status==PANGU_OK && outcome.result); pangu_core_destroy(outcome.result); completed=1; }
            CHECK(!pangu_core_is_running(c) && !pangu_core_message_count(c));
            pangu_core_destroy(c); CHECK(!mem.live);
            if (completed) break;
        }
        CHECK(completed);
    }
}
typedef struct { int mode,validations,executed,after,requests,trace; } mutable_fixture;
static pangu_status mutable_prepare(void *ctx,pangu_slice args,pangu_arguments_sink sink,void *target) {
    (void)ctx; return sink(target,args);
}
static pangu_status mutable_validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)schema; mutable_fixture *f=ctx; ++f->validations;
    CHECK(same(args,"{\"value\":\"hello\"}")); return PANGU_OK;
}
static pangu_status mutable_before(void *ctx,pangu_core *owner,const pangu_tool_context *input,pangu_before_update_sink sink,void *target) {
    mutable_fixture *f=ctx;
    CHECK(same(input->call->data,"{\"value\":\"hello\"}") && same(input->args_json,"{\"value\":\"hello\"}"));
    if (f->trace) puts("raw:{\"value\":\"hello\"}");
    if (f->mode==6) return PANGU_OK;
    char next[]="{\"value\":123}",reason[]="blocked";
    pangu_before_update update={.has_arguments=1,.arguments_json={next,sizeof(next)-1},
        .result={.block=f->mode==1,.terminate=f->mode==1,.reason={reason,sizeof(reason)-1}}};
    if (f->mode==4) update.arguments_json=S("{broken}");
    if (f->mode==5) update.arguments_json=S("[]");
    pangu_status status=sink(target,&update);
    /* The old prepared args must stay valid until the before callback returns. */
    CHECK(same(input->args_json,"{\"value\":\"hello\"}"));
    memset(next,'?',sizeof(next)-1); memset(reason,'?',sizeof(reason)-1);
    if (status!=PANGU_OK) return status;
    if (f->mode==2) return PANGU_TOOL_ERROR;
    if (f->mode==3) { (void)sink(target,&update); return PANGU_OK; }
    if (f->mode==7) pangu_core_cancel(owner);
    return PANGU_OK;
}
static pangu_status mutable_execute(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)owner; mutable_fixture *f=ctx; ++f->executed;
    CHECK(f->validations==1 && same(call->data,f->mode==6?"{\"value\":\"hello\"}":"{\"value\":123}"));
    if (f->trace) puts("execute:{\"value\":123}");
    pangu_content b={.kind=PANGU_TEXT,.data=S("result")};
    pangu_message m={.role=PANGU_TOOL_RESULT,.content=&b,.content_count=1}; return sink(target,&m,1);
}
static pangu_status mutable_after(void *ctx,pangu_core *owner,const pangu_tool_context *input,pangu_patch_sink sink,void *target) {
    (void)owner; (void)sink; (void)target; mutable_fixture *f=ctx; ++f->after;
    CHECK(same(input->call->data,"{\"value\":\"hello\"}") && same(input->args_json,f->mode==6?"{\"value\":\"hello\"}":"{\"value\":123}"));
    if (f->trace) puts("after:{\"value\":123}");
    return PANGU_OK;
}
static pangu_status mutable_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    (void)owner; mutable_fixture *f=ctx;
    pangu_content b={.kind=PANGU_TOOL_CALL,.id=S("mutable"),.name=S("tool"),.data=S("{\"value\":\"hello\"}")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1,.stop_reason=PANGU_TOOL_USE};
    if (f->requests++) { m.content=NULL; m.content_count=0; m.stop_reason=PANGU_STOP; }
    return sink(target,&m,1);
}
static void mutable_args(int trace) {
    for (int mode=0;mode<(trace?1:8);++mode) {
        for (int driver=0;driver<(mode || trace?1:2);++driver) {
            mutable_fixture f={.mode=mode,.trace=trace}; pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
            pangu_tool tool={.name=S("tool"),.schema_json=S("{}"),.ctx=&f,.prepare_arguments=mutable_prepare,.validate=mutable_validate,.execute=mutable_execute};
            pangu_loop_config cfg={.ctx=&f,.tools=&tool,.tool_count=1,.before_tool_update=mutable_before,.after_tool_patch=mutable_after,.model=mutable_model};
            if (trace || driver) {
                pangu_message prompt={.role=PANGU_USER}; CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK);
                const pangu_message *raw=pangu_core_message_at(owner,2); CHECK(raw && same(raw->content[0].data,"{\"value\":\"hello\"}"));
            } else {
                pangu_message assistant={.role=PANGU_ASSISTANT};
                pangu_content call={.kind=PANGU_TOOL_CALL,.id=S("mutable"),.name=S("tool"),.data=S("{\"value\":\"hello\"}")};
                pangu_tool_outcome outcome={0}; CHECK(pangu_core_call_tool(owner,&assistant,&call,&cfg,NULL,NULL,&outcome)==PANGU_OK);
                const pangu_message *result=pangu_core_message_at(outcome.result,0); CHECK(result);
                CHECK(result->is_error==(mode!=0 && mode!=6));
                if (mode==1) CHECK(same(result->content[0].data,"blocked") && result->terminate);
                pangu_core_destroy(outcome.result);
            }
            CHECK(f.validations==1 && f.executed==(mode==0 || mode==6) && f.after==f.executed);
            pangu_core_destroy(owner);
        }
    }
    if (trace) return;
    for (int blocked=0;blocked<2;++blocked) {
        int completed=0;
        for (size_t fail=1;fail<120;++fail) {
            memory mem={0}; mutable_fixture f={.mode=blocked}; pangu_core *owner=NULL;
            pangu_core_config mc={.allocator={&mem,allocate,release}}; CHECK(pangu_core_create(&mc,&owner)==PANGU_OK);
            pangu_tool tool={.name=S("tool"),.schema_json=S("{}"),.ctx=&f,.prepare_arguments=mutable_prepare,.validate=mutable_validate,.execute=mutable_execute};
            pangu_loop_config cfg={.ctx=&f,.tools=&tool,.tool_count=1,.before_tool_update=mutable_before,.after_tool_patch=mutable_after};
            pangu_message assistant={.role=PANGU_ASSISTANT};
            pangu_content call={.kind=PANGU_TOOL_CALL,.name=S("tool"),.data=S("{\"value\":\"hello\"}")};
            pangu_tool_outcome outcome={0}; mem.fail=mem.calls+fail;
            pangu_status status=pangu_core_call_tool(owner,&assistant,&call,&cfg,NULL,NULL,&outcome);
            CHECK(status==PANGU_OK || status==PANGU_NOMEM);
            pangu_core_destroy(outcome.result); pangu_core_destroy(owner); CHECK(!mem.live);
            if (status==PANGU_OK) { completed=1; break; }
        }
        CHECK(completed);
    }
}
typedef struct { pangu_core *owner;int stage,index; } detail_tool;
typedef struct { detail_tool tools[2];int requests; } detail_fixture;
static const char *detail_fallback(int stage) {
    return stage<2?"Invalid tool arguments":stage==2?"Tool preflight hook failed":stage==4?"Tool result hook failed":"Tool execution failed";
}
static pangu_status detail_fail(detail_tool *tool) {
    if(!tool->index){
        char text[]="tool a exploded";
        pangu_status status=pangu_core_report_error(tool->owner,(pangu_slice){text,sizeof(text)-1});
        memset(text,'x',sizeof(text)-1);if(status!=PANGU_OK)return status;
    }
    return PANGU_TOOL_ERROR;
}
static pangu_status detail_prepare(void *ctx,pangu_slice raw,pangu_arguments_sink sink,void *target) {
    (void)raw;(void)sink;(void)target;return detail_fail(ctx);
}
static pangu_status detail_validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)schema;(void)args;detail_tool *tool=ctx;return tool->stage==1?detail_fail(tool):PANGU_OK;
}
static pangu_status detail_before(void *ctx,pangu_core *owner,const pangu_tool_context *input,pangu_before_update_sink sink,void *target) {
    (void)owner;(void)sink;(void)target;detail_fixture *f=ctx;return detail_fail(&f->tools[input->call->id.data[0]-'a']);
}
static pangu_status detail_execute(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)owner;(void)call;detail_tool *tool=ctx;
    if(tool->stage==3)return detail_fail(tool);
    pangu_content text={.kind=PANGU_TEXT,.data=S("ok")};pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};
    return sink(target,&result,1);
}
static pangu_status detail_after(void *ctx,pangu_core *owner,const pangu_tool_context *input,pangu_patch_sink sink,void *target) {
    (void)owner;(void)sink;(void)target;detail_fixture *f=ctx;return detail_fail(&f->tools[input->call->id.data[0]-'a']);
}
static pangu_status detail_start(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target,void **op) {
    (void)owner;(void)call;(void)sink;(void)target;*op=NULL;return detail_fail(ctx);
}
static void detail_release(void *ctx,void *op) { (void)ctx;(void)op;CHECK(0); }
static pangu_status detail_pump(void *ctx,pangu_core *owner) { (void)ctx;(void)owner;CHECK(0);return PANGU_INVALID; }
static void detail_check(const pangu_message *result,int stage,int index) {
    CHECK(result && result->role==PANGU_TOOL_RESULT && result->is_error && result->content_count==1);
    CHECK(same(result->content[0].data,index?detail_fallback(stage):"tool a exploded"));
}
static pangu_status detail_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    detail_fixture *f=ctx;++f->requests;
    pangu_content calls[]={{.kind=PANGU_TOOL_CALL,.id=S("a"),.name=S("a"),.data=S("{}")},
        {.kind=PANGU_TOOL_CALL,.id=S("b"),.name=S("b"),.data=S("{}")}};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=f->requests==1?calls:NULL,.content_count=f->requests==1?2:0,.stop_reason=f->requests==1?PANGU_TOOL_USE:PANGU_STOP};
    if(f->requests==2){
        size_t count=pangu_core_message_count(owner);detail_check(pangu_core_message_at(owner,count-2),f->tools[0].stage,0);
        detail_check(pangu_core_message_at(owner,count-1),f->tools[0].stage,1);
    }
    return sink(target,&reply,1);
}
static void detail_config(detail_fixture *f,pangu_tool tools[2],pangu_loop_config *config,int stage) {
    for(int i=0;i<2;++i)tools[i]=(pangu_tool){.ctx=&f->tools[i],.name=i?S("b"):S("a"),.schema_json=S("{}"),
        .validate=detail_validate,.prepare_arguments=stage==0?detail_prepare:NULL,
        .execute=stage==5?NULL:detail_execute,.start=stage==5?detail_start:NULL,.release=stage==5?detail_release:NULL};
    *config=(pangu_loop_config){.ctx=f,.model=detail_model,.tools=tools,.tool_count=2,
        .before_tool_update=stage==2?detail_before:NULL,.after_tool_patch=stage==4?detail_after:NULL,.pump=detail_pump};
}
static pangu_status detail_nested_validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)ctx;(void)schema;(void)args;return PANGU_OK;
}
static pangu_status detail_nested_execute(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)call;(void)sink;(void)target;
    pangu_status status=pangu_core_report_error(owner,S("outer exploded"));if(status!=PANGU_OK)return status;
    pangu_message assistant={.role=PANGU_ASSISTANT};pangu_content inner={.kind=PANGU_TOOL_CALL,.id=S("a"),.name=S("a"),.data=S("{}")};
    pangu_tool_outcome out={0};status=pangu_core_call_tool(owner,&assistant,&inner,ctx,NULL,NULL,&out);
    if(status!=PANGU_OK)return status;
    detail_check(pangu_core_message_at(out.result,0),3,0);pangu_core_destroy(out.result);
    return PANGU_TOOL_ERROR;
}
static void tool_diagnostics(int trace) {
    for(int stage=0;stage<6;++stage)for(int loop=0;loop<2;++loop){
        pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        detail_fixture f={.tools={{owner,stage,0},{owner,stage,1}}};
        pangu_tool tools[2];pangu_loop_config config;detail_config(&f,tools,&config,stage);
        if(loop){pangu_message prompt={.role=PANGU_USER};CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_OK && f.requests==2);}
        else for(int i=0;i<2;++i){
            pangu_message assistant={.role=PANGU_ASSISTANT};pangu_content call={.kind=PANGU_TOOL_CALL,.id=i?S("b"):S("a"),.name=i?S("b"):S("a"),.data=S("{}")};
            pangu_tool_outcome out={0};CHECK(pangu_core_call_tool(owner,&assistant,&call,&config,NULL,NULL,&out)==PANGU_OK);
            detail_check(pangu_core_message_at(out.result,0),stage,i);pangu_core_destroy(out.result);
        }
        CHECK(!pangu_core_error(owner).len && !pangu_core_is_running(owner));
        if(trace)printf("tool-diagnostic:%d:%d:tool a exploded|%s\n",stage,loop,detail_fallback(stage));
        pangu_core_destroy(owner);
    }
    for(int stage=0;stage<6;++stage)for(int loop=0;loop<2;++loop)for(size_t fail=1;fail<250;++fail){
        memory mem={0};pangu_core_config allocation={.allocator={&mem,allocate,release}};
        pangu_core *owner=NULL;CHECK(pangu_core_create(&allocation,&owner)==PANGU_OK);
        detail_fixture f={.tools={{owner,stage,0},{owner,stage,1}}};pangu_tool tools[2];pangu_loop_config config;detail_config(&f,tools,&config,stage);
        mem.fail=mem.calls+fail;pangu_status status;
        if(loop){pangu_message prompt={.role=PANGU_USER};status=pangu_core_run(owner,&prompt,1,&config);}
        else{
            pangu_message assistant={.role=PANGU_ASSISTANT};pangu_content call={.kind=PANGU_TOOL_CALL,.id=S("a"),.name=S("a"),.data=S("{}")};
            pangu_tool_outcome out={0};status=pangu_core_call_tool(owner,&assistant,&call,&config,NULL,NULL,&out);pangu_core_destroy(out.result);
        }
        CHECK(status==PANGU_OK || status==PANGU_NOMEM);pangu_core_destroy(owner);CHECK(!mem.live);
        if(status==PANGU_OK)break;CHECK(fail<249);
    }
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    detail_fixture f={.tools={{owner,3,0},{owner,3,1}}};pangu_tool inner_tools[2];pangu_loop_config inner_config;detail_config(&f,inner_tools,&inner_config,3);
    pangu_tool outer={.ctx=&inner_config,.name=S("outer"),.schema_json=S("{}"),.validate=detail_nested_validate,.execute=detail_nested_execute};
    pangu_loop_config outer_config={.tools=&outer,.tool_count=1};pangu_message assistant={.role=PANGU_ASSISTANT};
    pangu_content call={.kind=PANGU_TOOL_CALL,.id=S("outer"),.name=S("outer"),.data=S("{}")};pangu_tool_outcome out={0};
    CHECK(pangu_core_call_tool(owner,&assistant,&call,&outer_config,NULL,NULL,&out)==PANGU_OK);
    CHECK(same(pangu_core_message_at(out.result,0)->content[0].data,"outer exploded"));
    pangu_core_destroy(out.result);pangu_core_destroy(owner);
}
typedef struct { int requests,before,inner,after; } nested_pending_fixture;
static pangu_status pending_nested_inner(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)call;nested_pending_fixture *f=ctx;f->inner=(int)pangu_core_pending_tool_count(owner);CHECK(f->inner==1);
    pangu_content text={.kind=PANGU_TEXT,.data=S("inner")};pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};return sink(target,&result,1);
}
static pangu_status pending_nested_outer(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    nested_pending_fixture *f=ctx;f->before=(int)pangu_core_pending_tool_count(owner);CHECK(f->before==1);
    pangu_tool inner={.ctx=f,.name=S("inner"),.schema_json=S("{}"),.validate=detail_nested_validate,.execute=pending_nested_inner};
    pangu_loop_config config={.tools=&inner,.tool_count=1};pangu_message assistant={.role=PANGU_ASSISTANT};
    pangu_content nested=*call;nested.name=S("inner"); /* same ID is intentional */
    pangu_tool_outcome out={0};pangu_status status=pangu_core_call_tool(owner,&assistant,&nested,&config,NULL,NULL,&out);
    if(status!=PANGU_OK)return status;
    CHECK(same(pangu_core_message_at(out.result,0)->content[0].data,"inner"));pangu_core_destroy(out.result);
    f->after=(int)pangu_core_pending_tool_count(owner);CHECK(f->after==1);
    pangu_content text={.kind=PANGU_TEXT,.data=S("outer")};pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};return sink(target,&result,1);
}
static pangu_status pending_nested_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    nested_pending_fixture *f=ctx;(void)owner;++f->requests;
    pangu_content call={.kind=PANGU_TOOL_CALL,.name=S("outer"),.id=S("shared"),.data=S("{}")};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=f->requests==1?&call:NULL,.content_count=f->requests==1?1:0,.stop_reason=f->requests==1?PANGU_TOOL_USE:PANGU_STOP};return sink(target,&reply,1);
}
static void nested_pending(int trace) {
    pangu_core *owner=NULL;CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);nested_pending_fixture f={0};
    pangu_tool outer={.ctx=&f,.name=S("outer"),.schema_json=S("{}"),.validate=detail_nested_validate,.execute=pending_nested_outer};
    pangu_loop_config config={.ctx=&f,.model=pending_nested_model,.tools=&outer,.tool_count=1};pangu_message prompt={.role=PANGU_USER};
    CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_OK && f.requests==2);
    size_t results=0;for(size_t i=0;i<pangu_core_message_count(owner);++i)if(pangu_core_message_at(owner,i)->role==PANGU_TOOL_RESULT)++results;
    CHECK(results==1 && !pangu_core_pending_tool_count(owner));
    if(trace)printf("nested-pending:%d:%d:%d\n",f.before,f.inner,f.after);
    pangu_core_destroy(owner);
}
typedef struct { memory mem; int prepared,executed,requests; } sticky_fixture;
static pangu_status sticky_arguments(void *ctx,pangu_slice raw,pangu_arguments_sink sink,void *target) {
    (void)raw; sticky_fixture *f=ctx; ++f->prepared;
    f->mem.fail=f->mem.calls+1;
    CHECK(sink(target,S("{}"))==PANGU_NOMEM);
    size_t calls=f->mem.calls;
    CHECK(sink(target,S("{}"))==PANGU_NOMEM);
    CHECK(sink(target,(pangu_slice){NULL,1})==PANGU_NOMEM);
    CHECK(f->mem.calls==calls);
    return PANGU_OK; /* Ignoring a sink failure must not erase it. */
}
static pangu_status sticky_validate(void *ctx,pangu_slice schema,pangu_slice args,pangu_arguments_sink sink,void *target) {
    (void)schema; return sticky_arguments(ctx,args,sink,target);
}
static pangu_status sticky_validate_call(void *ctx,pangu_core *owner,const pangu_content *call,pangu_slice schema,pangu_slice args,pangu_arguments_sink sink,void *target) {
    CHECK(pangu_core_is_running(owner) && same(call->id,"oom") && same(call->name,"tool"));
    return sticky_validate(ctx,schema,args,sink,target);
}
static pangu_status sticky_execute(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)owner; (void)call; (void)sink; (void)target;
    ++((sticky_fixture *)ctx)->executed; return PANGU_TOOL_ERROR;
}
static pangu_status sticky_model(void *ctx,pangu_core *owner,pangu_model_sink sink,void *target) {
    (void)owner; ++((sticky_fixture *)ctx)->requests;
    pangu_content call={.kind=PANGU_TOOL_CALL,.name=S("tool"),.id=S("oom"),.data=S("{}")};
    pangu_message message={.role=PANGU_ASSISTANT,.content=&call,.content_count=1,.stop_reason=PANGU_TOOL_USE};
    return sink(target,&message,1);
}
static pangu_status sticky_plain_validate(void *ctx,pangu_slice schema,pangu_slice args) {
    (void)ctx; (void)schema; (void)args; return PANGU_OK;
}
static pangu_status sticky_result(void *ctx,pangu_core *owner,const pangu_content *call,pangu_tool_sink sink,void *target) {
    (void)owner; (void)call; ++((sticky_fixture *)ctx)->executed;
    pangu_content text={.kind=PANGU_TEXT,.data=S("completed")};
    pangu_message result={.role=PANGU_TOOL_RESULT,.content=&text,.content_count=1};
    return sink(target,&result,1);
}
static pangu_status sticky_after(void *ctx,pangu_core *owner,const pangu_content *call,const pangu_message *result,pangu_tool_sink sink,void *target) {
    (void)owner; (void)call; sticky_fixture *f=ctx;
    f->mem.fail=f->mem.calls+1;
    CHECK(sink(target,result,1)==PANGU_NOMEM);
    size_t calls=f->mem.calls;
    CHECK(sink(target,NULL,0)==PANGU_NOMEM);
    CHECK(sink(target,result,1)==PANGU_NOMEM);
    CHECK(f->mem.calls==calls && same(result->content[0].data,"completed"));
    return PANGU_OK;
}
static pangu_status sticky_patch(void *ctx,pangu_core *owner,const pangu_tool_context *input,pangu_patch_sink sink,void *target) {
    (void)owner; sticky_fixture *f=ctx;
    pangu_tool_patch patch={.fields=PANGU_PATCH_USAGE,.usage={.total=3}};
    f->mem.fail=f->mem.calls+1;
    CHECK(sink(target,&patch)==PANGU_NOMEM);
    size_t calls=f->mem.calls;
    CHECK(sink(target,NULL)==PANGU_NOMEM);
    CHECK(sink(target,&patch)==PANGU_NOMEM);
    CHECK(f->mem.calls==calls && same(input->result->content[0].data,"completed"));
    return PANGU_OK;
}
static void sticky_failures(void) {
    for (int stage=0;stage<5;++stage) for (int loop=0;loop<2;++loop) {
        sticky_fixture f={0}; pangu_core *owner=NULL;
        pangu_core_config alloc={.allocator={&f.mem,allocate,release}};
        CHECK(pangu_core_create(&alloc,&owner)==PANGU_OK);
        pangu_tool tool={.ctx=&f,.name=S("tool"),.schema_json=S("{}"),.execute=sticky_execute};
        if (stage==4) {tool.validate_call=sticky_validate_call;tool.validate_arguments=unused_validator;tool.validate=sticky_plain_validate;}
        else if (stage==1) tool.validate_arguments=sticky_validate;
        else if (!stage) { tool.prepare_arguments=sticky_arguments; tool.validate_arguments=sticky_validate; }
        else { tool.validate=sticky_plain_validate; tool.execute=sticky_result; }
        pangu_loop_config config={.ctx=&f,.model=sticky_model,.tools=&tool,.tool_count=1};
        if (stage==2) config.after_tool=sticky_after;
        if (stage==3) config.after_tool_patch=sticky_patch;
        if (loop) {
            pangu_message prompt={.role=PANGU_USER};
            CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_NOMEM);
            CHECK(pangu_core_message_at(owner,pangu_core_message_count(owner)-1)->role==PANGU_ASSISTANT);
        } else {
            pangu_message assistant={.role=PANGU_ASSISTANT};
            pangu_content call={.kind=PANGU_TOOL_CALL,.name=S("tool"),.id=S("oom"),.data=S("{}")};
            pangu_tool_outcome outcome={0};
            CHECK(pangu_core_call_tool(owner,&assistant,&call,&config,NULL,NULL,&outcome)==PANGU_NOMEM);
            CHECK(!outcome.result && !pangu_core_message_count(owner));
        }
        CHECK(f.prepared==(stage<2 || stage==4) && f.executed==(stage>=2 && stage<4) && !pangu_core_pending_tool_count(owner));
        CHECK(!pangu_core_is_running(owner));
        pangu_core_destroy(owner); CHECK(!f.mem.live);
    }
}
int main(int argc,char **argv) { if (argc==2 && !strcmp(argv[1],"--trace-nested-pending")) { nested_pending(1); return 0; } nested_pending(0); if (argc==2 && !strcmp(argv[1],"--trace-diagnostics")) { tool_diagnostics(1); return 0; } tool_diagnostics(0); if (argc==2 && !strcmp(argv[1],"--trace-argument-error")) { scenario(ARGUMENT_ERROR,NULL,2); return 0; } if (argc==2 && !strcmp(argv[1],"--trace-preparation")) { loop_result(3); return 0; } if (argc==2 && !strcmp(argv[1],"--trace-duration")) { loop_result(2); return 0; } if (argc==2 && !strcmp(argv[1],"--trace-timestamp")) { loop_result(1); return 0; } if (argc==2 && !strcmp(argv[1],"--trace-outcomes")) { for (int mode=KEEP;mode<=CANCELLED;++mode) if (mode!=INVALID) scenario(mode,NULL,1); return 0; } if (argc==2 && !strcmp(argv[1],"--trace-before")) { mutable_args(1); return 0; } sticky_failures(); mutable_args(0); for (int mode=KEEP;mode<=ARGUMENT_ERROR;++mode) scenario(mode,NULL,0); loop_result(0); allocation_failures(); return 0; }
