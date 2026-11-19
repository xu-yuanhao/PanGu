#include <pangu/core/proxy.h>
#include <stdlib.h>
#include <stdio.h>
#include "yyjson.h"
#include <string.h>
#include <time.h>
#define CHECK(x) do { if (!(x)) abort(); } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
typedef struct { int finals,updates,mode; pangu_core *result; int64_t timestamp; } fixture;
static pangu_status capture(void *ctx,const pangu_message *m,int final) {
    fixture *f=ctx;
    if (!f->timestamp) f->timestamp=m->timestamp_ms;
    CHECK(f->timestamp>0 && m->timestamp_ms==f->timestamp);
    if (final) { ++f->finals; return pangu_core_append(f->result,m); }
    ++f->updates;
    for (size_t i=0;i<m->content_count;++i) if (m->content[i].kind==PANGU_TOOL_CALL) {
        pangu_slice args=m->content[i].data;
        yyjson_doc *parsed=yyjson_read(args.data,args.len,0);
        CHECK(parsed && yyjson_is_obj(yyjson_doc_get_root(parsed)));
        yyjson_doc_free(parsed);
    }
    return PANGU_OK;
}
static int64_t utc_ms(void) {
    struct timespec value;CHECK(timespec_get(&value,TIME_UTC)==TIME_UTC);
    return (int64_t)value.tv_sec*INT64_C(1000)+value.tv_nsec/1000000;
}
static void timestamp_metadata(int trace) {
    fixture f={0};pangu_core *owner=NULL;pangu_proxy *proxy=NULL;
    CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
    int64_t before=utc_ms();
    CHECK(pangu_proxy_create(owner,capture,&f,&proxy)==PANGU_OK);
    CHECK(pangu_proxy_feed(proxy,S("data: {\"type\":\"start\"}\n"
        "data: {\"type\":\"text_start\",\"contentIndex\":0}\n"
        "data: {\"type\":\"text_delta\",\"contentIndex\":0,\"delta\":\"kept\"}\n"
        "data: {\"type\":\"done\",\"reason\":\"stop\",\"usage\":{}}\n"))==PANGU_OK);
    int64_t after=utc_ms();
    CHECK(f.timestamp>=before && f.timestamp<=after && f.updates==3 && f.finals==1);
    if(trace)puts("{\"withinRequest\":true,\"stable\":true,\"snapshots\":4}");
    pangu_proxy_destroy(proxy);pangu_core_destroy(f.result);pangu_core_destroy(owner);
}
static pangu_status feed(pangu_proxy *p,int mode) {
    pangu_proxy_event events[]={
        {.kind=PANGU_PROXY_START},
        {.kind=PANGU_PROXY_TEXT_START},
        {.kind=PANGU_PROXY_TEXT_DELTA,.delta=S("hello ")},
        {.kind=PANGU_PROXY_TEXT_DELTA,.delta=S("世界")},
        {.kind=PANGU_PROXY_TEXT_END,.signature=S("text-sig")},
        {.kind=PANGU_PROXY_THINKING_START,.content_index=1},
        {.kind=PANGU_PROXY_THINKING_DELTA,.content_index=1,.delta=S("reason")},
        {.kind=PANGU_PROXY_THINKING_END,.content_index=1,.signature=S("reason-sig")},
        {.kind=PANGU_PROXY_TOOL_START,.content_index=2,.id=S("one"),.name=S("lookup")},
        {.kind=PANGU_PROXY_TOOL_DELTA,.content_index=2,.delta=S("{\"value\":")},
        {.kind=PANGU_PROXY_TOOL_DELTA,.content_index=2,.delta=S("1}")}};
    for (size_t i=0;i<sizeof(events)/sizeof(events[0]);++i) {
        pangu_status s=pangu_proxy_push(p,&events[i]); if (s!=PANGU_OK) return s;
    }
    if (mode==1) return pangu_proxy_finish(p);
    if (mode==2) {
        pangu_proxy_event wrong={.kind=PANGU_PROXY_TEXT_DELTA,.content_index=1,.delta=S("bad")};
        return pangu_proxy_push(p,&wrong);
    }
    pangu_content call={.kind=PANGU_TOOL_CALL,.id=S("one"),.name=S("lookup"),.data=S("{\"value\":1}"),.signature=S("tool-sig"),.tool_namespace=S("dynamic_tools")};
    pangu_proxy_event end={.kind=PANGU_PROXY_TOOL_END,.content_index=2,.tool_call=&call};
    pangu_status s=pangu_proxy_push(p,&end); if (s!=PANGU_OK) return s;
    pangu_proxy_event done={.kind=PANGU_PROXY_DONE,.reason=PANGU_TOOL_USE,.usage={.input=10,.output=3,.total=13},.provider_thinking_level=S("high")};
    return pangu_proxy_push(p,&done);
}
static pangu_status provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    fixture *f=ctx; (void)view;
    pangu_proxy *p=NULL; pangu_status s=pangu_proxy_create(owner,sink,target,&p);
    if (s==PANGU_OK) s=feed(p,f->mode);
    if (s==PANGU_OK) s=pangu_proxy_finish(p);
    pangu_proxy_destroy(p); return s;
}
static pangu_turn_decision finish(void *ctx,pangu_core *owner,const pangu_message *m) {
    (void)ctx; (void)owner; (void)m; return PANGU_TURN_END_RUN;
}
static const char wire[]=
    ": heartbeat\n\ndata: {\"type\":\"start\"}\r\n"
    "data: {\"type\":\"text_start\",\"contentIndex\":0}\n"
    "data: {\"type\":\"text_delta\",\"contentIndex\":0,\"delta\":\"世界\"}\n"
    "data: {\"type\":\"toolcall_start\",\"contentIndex\":1,\"id\":\"one\",\"toolName\":\"lookup\"}\n"
    "data: {\"type\":\"toolcall_end\",\"contentIndex\":1,\"toolCall\":{\"id\":\"one\",\"name\":\"lookup\",\"namespace\":\"dynamic_tools\",\"arguments\":{\"value\":1}}}\n"
    "data: {\"type\":\"done\",\"reason\":\"toolUse\",\"usage\":{\"input\":10,\"output\":3,\"totalTokens\":13,\"cacheWrite1h\":0,\"reasoning\":0,\"cost\":{\"total\":0.25,\"totalFromProvider\":false}},\"providerThinkingLevel\":\"high\"}";
static void wire_fragments(void) {
    for (size_t split=0;split<sizeof(wire);++split) {
        pangu_core *owner=NULL; fixture f={0}; pangu_proxy *p=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
        CHECK(pangu_proxy_create(owner,capture,&f,&p)==PANGU_OK);
        CHECK(pangu_proxy_feed(p,(pangu_slice){wire,split})==PANGU_OK);
        for (size_t i=split;i<sizeof(wire)-1;++i) CHECK(pangu_proxy_feed(p,(pangu_slice){wire+i,1})==PANGU_OK);
        CHECK(pangu_proxy_finish(p)==PANGU_OK && f.finals==1);
        const pangu_message *m=pangu_core_message_at(f.result,0);
        CHECK(m->stop_reason==PANGU_TOOL_USE && m->content_count==2 && m->content[0].data.len==6);
        CHECK(m->content[1].tool_namespace.len==13 && m->content[1].data.len==11);
        CHECK(m->usage.total==13 && m->usage.total_cost==0.25 && m->provider_thinking_level.len==4);
        CHECK(m->usage.has_cache_write_1h && !m->usage.cache_write_1h && m->usage.has_reasoning && !m->usage.reasoning);
        CHECK(m->usage.has_total_from_provider && !m->usage.total_from_provider);
        pangu_proxy_destroy(p); pangu_core_destroy(f.result); pangu_core_destroy(owner);
    }
    const char *invalid="data: {invalid}";
    pangu_core *owner=NULL; fixture f={0}; pangu_proxy *p=NULL;
    CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
    CHECK(pangu_proxy_create(owner,capture,&f,&p)==PANGU_OK);
    CHECK(pangu_proxy_feed(p,(pangu_slice){invalid,strlen(invalid)})==PANGU_OK);
    CHECK(pangu_proxy_finish(p)==PANGU_OK && f.finals==1);
    CHECK(pangu_core_message_at(f.result,0)->stop_reason==PANGU_ERROR);
    pangu_proxy_destroy(p); pangu_core_destroy(f.result); pangu_core_destroy(owner);
}
typedef struct { size_t calls,fail,live; } memory;
static void *allocate(void *ctx,size_t n) {
    memory *m=ctx; if (++m->calls==m->fail) return NULL;
    void *p=malloc(n); if (p) ++m->live; return p;
}
static void release(void *ctx,void *p) { if (p) { --((memory *)ctx)->live; free(p); } }
static void allocation_failures(void) {
    int success=0;
    for (size_t fail=1;fail<700;++fail) {
        memory mem={0}; pangu_core_config cfg={.allocator={&mem,allocate,release}};
        pangu_core *owner=NULL; fixture f={0};
        CHECK(pangu_core_create(&cfg,&owner)==PANGU_OK && pangu_core_create(&cfg,&f.result)==PANGU_OK);
        mem.fail=mem.calls+fail; pangu_proxy *proxy=NULL;
        pangu_status status=pangu_proxy_create(owner,capture,&f,&proxy);
        if (status==PANGU_OK) status=pangu_proxy_feed(proxy,(pangu_slice){wire,sizeof(wire)-1});
        if (status==PANGU_OK) status=pangu_proxy_finish(proxy);
        CHECK(status==PANGU_OK || status==PANGU_NOMEM);
        pangu_proxy_destroy(proxy); pangu_core_destroy(f.result); pangu_core_destroy(owner);
        CHECK(mem.live==0);
        if (status==PANGU_OK) { success=1; break; }
    }
    CHECK(success);
}
static yyjson_mut_doc *trace_doc;
static yyjson_mut_val *trace_events;
typedef struct { int mode; size_t updates; int starts,ends; } event_fixture;
static pangu_status event_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_event_sink sink,void *target) {
    event_fixture *f=ctx; (void)view;
    pangu_proxy *p=NULL; pangu_status status=pangu_proxy_create_events(owner,sink,target,&p);
    if (status==PANGU_OK) {
        if (f->mode==3) {
            for (size_t i=0;i<sizeof(wire)-1 && status==PANGU_OK;++i)
                status=pangu_proxy_feed(p,(pangu_slice){wire+i,1});
        } else status=feed(p,f->mode);
    }
    if (status==PANGU_OK) status=pangu_proxy_finish(p);
    pangu_proxy_destroy(p); return status;
}
static void event_observer(void *ctx,pangu_core *owner,const pangu_event *e) {
    event_fixture *f=ctx; (void)owner;
    if (e->kind==PANGU_MESSAGE_START && e->message->role==PANGU_ASSISTANT) ++f->starts;
    if (e->kind==PANGU_MESSAGE_END && e->message->role==PANGU_ASSISTANT) ++f->ends;
    if (e->kind!=PANGU_MESSAGE_UPDATE) { CHECK(!e->assistant_event); return; }
    const pangu_assistant_event *a=e->assistant_event;
    CHECK(a && a->content_index<e->message->content_count);
    if (f->mode==3) {
        const pangu_assistant_kind expected[]={PANGU_ASSISTANT_TEXT_START,PANGU_ASSISTANT_TEXT_DELTA,PANGU_ASSISTANT_TOOL_START,PANGU_ASSISTANT_TOOL_END};
        CHECK(f->updates<4 && a->kind==expected[f->updates]);
    } else {
        const pangu_assistant_kind expected[]={PANGU_ASSISTANT_TEXT_START,PANGU_ASSISTANT_TEXT_DELTA,PANGU_ASSISTANT_TEXT_DELTA,PANGU_ASSISTANT_TEXT_END,PANGU_ASSISTANT_THINKING_START,PANGU_ASSISTANT_THINKING_DELTA,PANGU_ASSISTANT_THINKING_END,PANGU_ASSISTANT_TOOL_START,PANGU_ASSISTANT_TOOL_DELTA,PANGU_ASSISTANT_TOOL_DELTA,PANGU_ASSISTANT_TOOL_END};
        CHECK(f->updates<11 && a->kind==expected[f->updates]);
    }
    const pangu_content *b=&e->message->content[a->content_index];
    if (a->kind==PANGU_ASSISTANT_TEXT_DELTA || a->kind==PANGU_ASSISTANT_THINKING_DELTA || a->kind==PANGU_ASSISTANT_TOOL_DELTA) CHECK(a->delta.len);
    if (a->kind==PANGU_ASSISTANT_TEXT_END || a->kind==PANGU_ASSISTANT_THINKING_END)
        CHECK(a->content.len==b->data.len && !memcmp(a->content.data,b->data.data,b->data.len));
    if (a->kind==PANGU_ASSISTANT_TOOL_END) CHECK(a->tool_call && a->tool_call->data.len==11 && a->tool_call->tool_namespace.len==13);
    if (trace_doc) {
        yyjson_mut_val *item=yyjson_mut_obj(trace_doc); CHECK(item);
        CHECK(yyjson_mut_obj_add_uint(trace_doc,item,"kind",a->kind));
        CHECK(yyjson_mut_obj_add_uint(trace_doc,item,"index",a->content_index));
        CHECK(yyjson_mut_obj_add_strncpy(trace_doc,item,"delta",a->delta.data?a->delta.data:"",a->delta.len));
        CHECK(yyjson_mut_obj_add_strncpy(trace_doc,item,"content",a->content.data?a->content.data:"",a->content.len));
        if (a->tool_call) CHECK(yyjson_mut_obj_add_strncpy(trace_doc,item,"arguments",a->tool_call->data.data,a->tool_call->data.len));
        CHECK(yyjson_mut_arr_append(trace_events,item));
    }
    ++f->updates;
    if (f->mode==4 && f->updates==2) pangu_core_cancel(owner);
}
static void event_pipeline(void) {
    for (int mode=0;mode<(trace_doc?1:5);++mode) {
        pangu_core *c=NULL; CHECK(pangu_core_create(NULL,&c)==PANGU_OK);
        event_fixture f={.mode=mode};
        pangu_loop_config cfg={.ctx=&f,.request_events=event_provider,.event=event_observer,.finish_turn=finish};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(c,&prompt,1,&cfg)==(mode==4?PANGU_CANCELLED:(mode==1 || mode==2)?PANGU_MODEL_ERROR:PANGU_OK));
        CHECK(f.starts==1 && f.ends==1 && f.updates==(mode==4?2u:mode==3?4u:(mode==1 || mode==2)?10u:11u));
        const pangu_message *m=pangu_core_message_at(c,1);
        CHECK(m && m->content_count==(mode==4?1u:mode==3?2u:3u));
        pangu_core_destroy(c);
    }
}
static pangu_status unexpected_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)ctx; (void)owner; (void)view; (void)sink; (void)target; return PANGU_INVALID;
}
static pangu_status prepare_event_provider(void *ctx,pangu_core *owner,pangu_request_update_sink sink,void *target) {
    (void)owner;
    pangu_request_update update={.fields=PANGU_UPDATE_PROVIDER,.request_events=event_provider,.request_ctx=ctx};
    return sink(target,&update);
}
static void event_provider_lifecycle(void) {
    int success=0;
    for (size_t fail=1;fail<2000;++fail) {
        memory mem={0}; pangu_core_config config={.allocator={&mem,allocate,release}};
        pangu_core *owner=NULL; CHECK(pangu_core_create(&config,&owner)==PANGU_OK);
        event_fixture f={0};
        pangu_loop_config cfg={.ctx=&f,.request=unexpected_provider,.prepare_request_update=prepare_event_provider,.event=event_observer,.finish_turn=finish};
        pangu_message prompt={.role=PANGU_USER}; mem.fail=mem.calls+fail;
        pangu_status status=pangu_core_run(owner,&prompt,1,&cfg);
        CHECK(status==PANGU_OK || status==PANGU_NOMEM);
        if (status==PANGU_OK) CHECK(f.starts==1 && f.ends==1 && f.updates==11);
        pangu_core_destroy(owner); CHECK(!mem.live);
        if (status==PANGU_OK) { success=1; break; }
    }
    CHECK(success);
}
static pangu_status terminal_event_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_event_sink sink,void *target) {
    (void)owner; (void)view; int mode=*(int *)ctx;
    pangu_content b={.kind=PANGU_TEXT,.data={"kept",4}};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&b,.content_count=1};
    pangu_assistant_event ignored={.kind=PANGU_ASSISTANT_TEXT_DELTA,.delta={"ignored",7}};
    CHECK(sink(target,&m,&ignored)==PANGU_OK); /* no start: Pi ignores update */
    pangu_assistant_event terminal={.kind=mode<2?PANGU_ASSISTANT_DONE:PANGU_ASSISTANT_ERROR,
        .reason=mode==0?PANGU_STOP:mode==1?PANGU_DEFERRED:mode==2?PANGU_ERROR:PANGU_ABORTED};
    m.error=mode>=2?(pangu_slice){"provider diagnostic",19}:(pangu_slice){0};
    return sink(target,&m,&terminal);
}
static void terminal_events(void) {
    for (int mode=0;mode<4;++mode) {
        pangu_core *owner=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        event_fixture f={0}; pangu_loop_config cfg={.ctx=&f,.request_events=terminal_event_provider,
            .has_request_ctx=1,.request_ctx=&mode,.event=event_observer};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==(mode<2?PANGU_OK:mode==2?PANGU_MODEL_ERROR:PANGU_CANCELLED));
        CHECK(f.starts==1 && f.ends==1 && !f.updates);
        const pangu_message *m=pangu_core_message_at(owner,1);
        CHECK(m && m->content_count==1 && m->content[0].data.len==4);
        if (mode>=2) CHECK(m->error.len==19);
        if (mode==1) CHECK(m->stop_reason==PANGU_DEFERRED);
        pangu_core_destroy(owner);
    }
}
static pangu_status save_preview(void *ctx,const pangu_message *message,int final) {
    (void)final;return pangu_core_append(ctx,message);
}
static void pending_literal_preview(void) {
    pangu_core *owner=NULL,*copies=NULL;pangu_proxy *proxy=NULL;
    CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&copies)==PANGU_OK);
    CHECK(pangu_proxy_create(owner,save_preview,copies,&proxy)==PANGU_OK);
    pangu_proxy_event event={.kind=PANGU_PROXY_TOOL_START,.id=S("one"),.name=S("lookup")};
    CHECK(pangu_proxy_push(proxy,&event)==PANGU_OK);
    event=(pangu_proxy_event){.kind=PANGU_PROXY_TOOL_DELTA,.delta=S("{\"a\":tru")};
    CHECK(pangu_proxy_push(proxy,&event)==PANGU_OK);
    event.delta=S(",\"b\"");CHECK(pangu_proxy_push(proxy,&event)==PANGU_OK);
    const pangu_message *preview=pangu_core_message_at(copies,pangu_core_message_count(copies)-1);
    CHECK(preview->content_count==1 && preview->content[0].data.len==10 &&
          !memcmp(preview->content[0].data.data,"{\"a\":true}",10));
    pangu_content final={.kind=PANGU_TOOL_CALL,.id=S("one"),.name=S("lookup"),
        .data=S("{}"),.arguments_error=S("provider supplied invalid arguments")};
    event=(pangu_proxy_event){.kind=PANGU_PROXY_TOOL_END,.tool_call=&final};
    CHECK(pangu_proxy_push(proxy,&event)==PANGU_OK);
    event=(pangu_proxy_event){.kind=PANGU_PROXY_DONE,.reason=PANGU_TOOL_USE};
    CHECK(pangu_proxy_push(proxy,&event)==PANGU_OK);
    const pangu_message *result=pangu_core_message_at(copies,pangu_core_message_count(copies)-1);
    CHECK(result->content[0].data.len==2 && result->content[0].arguments_error.len==35);
    /* Previously copied previews survive later authoritative replacement. */
    CHECK(!memcmp(preview->content[0].data.data,"{\"a\":true}",10));
    pangu_proxy_destroy(proxy);pangu_core_destroy(copies);pangu_core_destroy(owner);
}
static void ignored_tool_end(int trace) {
    const char wire[]= "data: \xef\xbb\xbf{\"type\":\"start\"}\xc2\xa0\xe2\x80\xa8\n"
        "data: \xef\xbb\xbf\xc2\xa0\n"
        "data: {\"type\":\"text_start\",\"contentIndex\":0}\n"
        "data: {\"type\":\"text_delta\",\"contentIndex\":0,\"delta\":\"kept\"}\n"
        "data: {\"type\":\"toolcall_end\",\"contentIndex\":0,\"toolCall\":{\"type\":\"toolCall\",\"id\":\"late\",\"name\":\"unused\",\"arguments\":{}}}\n"
        "data: {\"type\":\"toolcall_end\",\"contentIndex\":4,\"toolCall\":{\"type\":\"toolCall\",\"id\":\"late\",\"name\":\"unused\",\"arguments\":{}}}\n"
        "data: {\"type\":\"toolcall_end\",\"contentIndex\":-1}\n"
        "data: {\"type\":\"toolcall_end\",\"contentIndex\":null}\n"
        "data: {\"type\":\"toolcall_end\",\"contentIndex\":0}\n"
        "data: {\"type\":\"toolcall_end\"}\n"
        "data: {\"type\":\"done\",\"reason\":\"stop\",\"usage\":{}}\n";
    fixture f={0};pangu_core *owner=NULL;pangu_proxy *proxy=NULL;
    CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
    CHECK(pangu_proxy_create(owner,capture,&f,&proxy)==PANGU_OK);
    CHECK(pangu_proxy_feed(proxy,(pangu_slice){wire,sizeof(wire)-1})==PANGU_OK);
    CHECK(pangu_proxy_finish(proxy)==PANGU_OK);
    const pangu_message *result=pangu_core_message_at(f.result,0);
    CHECK(f.finals==1 && f.updates==3 && result->stop_reason==PANGU_STOP && result->content_count==1);
    CHECK(result->content[0].data.len==4 && !memcmp(result->content[0].data.data,"kept",4));
    if(trace)printf("{\"stopReason\":\"stop\",\"text\":\"kept\",\"updates\":%d}\n",f.updates);
    pangu_proxy_destroy(proxy);pangu_core_destroy(f.result);pangu_core_destroy(owner);
}
static void eof_partial(int trace) {
    const char wire[]="data: {\"type\":\"text_start\",\"contentIndex\":0}\n"
        "data: {\"type\":\"text_delta\",\"contentIndex\":0,\"delta\":\"kept\"}";
    fixture f={0};pangu_core *owner=NULL;pangu_proxy *proxy=NULL;
    CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
    CHECK(pangu_proxy_create(owner,capture,&f,&proxy)==PANGU_OK);
    CHECK(pangu_proxy_feed(proxy,(pangu_slice){wire,sizeof(wire)-1})==PANGU_OK);
    CHECK(pangu_proxy_finish(proxy)==PANGU_OK && pangu_proxy_finish(proxy)==PANGU_OK);
    const pangu_message *m=pangu_core_message_at(f.result,0);
    const char expected[]="Connection closed by proxy server before the response completed";
    CHECK(f.finals==1 && m->stop_reason==PANGU_ERROR && m->content_count==1);
    CHECK(m->content[0].data.len==4 && !memcmp(m->content[0].data.data,"kept",4));
    CHECK(m->error.len==sizeof(expected)-1 && !memcmp(m->error.data,expected,sizeof(expected)-1));
    if(trace)printf("{\"stopReason\":\"error\",\"text\":\"kept\",\"error\":\"%s\",\"finals\":%d}\n",expected,f.finals);
    pangu_proxy_destroy(proxy);pangu_core_destroy(f.result);pangu_core_destroy(owner);
}
static pangu_status cancelled_eof_provider(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)view;fixture *f=ctx;pangu_proxy *proxy=NULL;
    CHECK(pangu_proxy_create(owner,capture,f,&proxy)==PANGU_OK);
    CHECK(pangu_proxy_feed(proxy,S("data: {\"type\":\"text_start\",\"contentIndex\":0}\n"
        "data: {\"type\":\"text_delta\",\"contentIndex\":0,\"delta\":\"kept\"}\n"))==PANGU_OK);
    const char *tails[]={"data: {invalid}","data: {\"type\":\"text_delta\",\"contentIndex\":0,\"delta\":\"discarded\"}",
        "data: {\"type\":\"done\",\"reason\":\"stop\",\"usage\":{}}"};
    CHECK(pangu_proxy_feed(proxy,(pangu_slice){tails[f->mode],strlen(tails[f->mode])})==PANGU_OK);
    pangu_core_cancel(owner);
    CHECK(pangu_proxy_finish(proxy)==PANGU_OK && pangu_proxy_finish(proxy)==PANGU_OK);
    pangu_status status=sink(target,pangu_core_message_at(f->result,0),1);
    pangu_proxy_destroy(proxy);return status;
}
static void cancelled_eof(int trace) {
    for(int mode=0;mode<3;++mode){
        fixture f={.mode=mode};pangu_core *owner=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
        pangu_loop_config config={.ctx=&f,.request=cancelled_eof_provider};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(owner,&prompt,1,&config)==PANGU_CANCELLED);
        const pangu_message *m=pangu_core_message_at(f.result,0);
        const char expected[]="Request aborted by user";
        CHECK(f.finals==1 && f.updates==2 && m->stop_reason==PANGU_ABORTED && m->content_count==1);
        CHECK(m->content[0].data.len==4 && !memcmp(m->content[0].data.data,"kept",4));
        CHECK(m->error.len==sizeof(expected)-1 && !memcmp(m->error.data,expected,sizeof(expected)-1));
        if(trace)printf("{\"stopReason\":\"aborted\",\"text\":\"kept\",\"error\":\"%s\",\"finals\":%d}\n",expected,f.finals);
        pangu_core_destroy(f.result);pangu_core_destroy(owner);
    }
}
static void repeated_tool_end(int trace) {
    for(int clear=0;clear<2;++clear){
        fixture f={0};pangu_core *owner=NULL;pangu_proxy *proxy=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
        CHECK(pangu_proxy_create(owner,capture,&f,&proxy)==PANGU_OK);
        CHECK(pangu_proxy_feed(proxy,S("data: {\"type\":\"toolcall_start\",\"contentIndex\":0,\"id\":\"one\",\"toolName\":\"lookup\"}\n"
            "data: {\"type\":\"toolcall_end\",\"contentIndex\":0,\"toolCall\":{\"type\":\"toolCall\",\"id\":\"one\",\"name\":\"lookup\",\"arguments\":{},\"namespace\":\"ns\",\"thoughtSignature\":\"sig\",\"argumentsError\":\"bad\"}}\n"
            "data: {\"type\":\"toolcall_end\",\"contentIndex\":0,\"toolCall\":{\"type\":\"toolCall\",\"id\":\"one\",\"name\":\"lookup\",\"arguments\":{\"value\":2}}}\n"))==PANGU_OK);
        if(clear)CHECK(pangu_proxy_feed(proxy,S("data: {\"type\":\"toolcall_end\",\"contentIndex\":0,\"toolCall\":{\"type\":\"toolCall\",\"id\":\"one\",\"name\":\"lookup\",\"arguments\":{\"value\":2},\"namespace\":\"\",\"thoughtSignature\":\"\",\"argumentsError\":\"\"}}\n"))==PANGU_OK);
        CHECK(pangu_proxy_feed(proxy,S("data: {\"type\":\"done\",\"reason\":\"toolUse\",\"usage\":{}}\n"))==PANGU_OK);
        const pangu_message *m=pangu_core_message_at(f.result,0);const pangu_content *b=&m->content[0];
        CHECK(m->stop_reason==PANGU_TOOL_USE && f.finals==1 && f.updates==3+clear);
        CHECK(b->data.len==11 && !memcmp(b->data.data,"{\"value\":2}",11));
        CHECK(b->tool_namespace.len==(clear?0u:2u) && b->signature.len==(clear?0u:3u) && b->arguments_error.len==(clear?0u:3u));
        if(trace)printf("{\"namespace\":\"%.*s\",\"signature\":\"%.*s\",\"argumentsError\":\"%.*s\",\"updates\":%d}\n",
            (int)b->tool_namespace.len,b->tool_namespace.data?b->tool_namespace.data:"",
            (int)b->signature.len,b->signature.data?b->signature.data:"",
            (int)b->arguments_error.len,b->arguments_error.data?b->arguments_error.data:"",f.updates);
        pangu_proxy_destroy(proxy);pangu_core_destroy(f.result);pangu_core_destroy(owner);
    }
}
static void mismatch_errors(int trace) {
    const char *types[]={"text_delta","text_end","thinking_delta","thinking_end","toolcall_delta"};
    const char *expected[]={"Received text_delta for non-text content","Received text_end for non-text content",
        "Received thinking_delta for non-thinking content","Received thinking_end for non-thinking content",
        "Received toolcall_delta for non-toolCall content"};
    for(size_t i=0;i<5;++i){
        fixture f={0};pangu_core *owner=NULL;pangu_proxy *proxy=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
        CHECK(pangu_proxy_create(owner,capture,&f,&proxy)==PANGU_OK);
        char wire[512];int n=snprintf(wire,sizeof(wire),
            "data: {\"type\":\"%s\",\"contentIndex\":0}\n"
            "data: {\"type\":\"%s\",\"contentIndex\":0,\"delta\":\"bad\"}\n",
            i<2?"thinking_start":"text_start",types[i]);
        CHECK(n>0 && (size_t)n<sizeof(wire));
        CHECK(pangu_proxy_feed(proxy,(pangu_slice){wire,(size_t)n})==PANGU_OK);
        const pangu_message *m=pangu_core_message_at(f.result,0);
        CHECK(f.finals==1 && m->stop_reason==PANGU_ERROR && m->content_count==1);
        CHECK(m->error.len==strlen(expected[i]) && !memcmp(m->error.data,expected[i],m->error.len));
        if(trace)printf("%.*s\n",(int)m->error.len,m->error.data);
        pangu_proxy_destroy(proxy);pangu_core_destroy(f.result);pangu_core_destroy(owner);
    }
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--trace-repeated-tool-end")){repeated_tool_end(1);return 0;}
    if(argc==2 && !strcmp(argv[1],"--trace-timestamp")){timestamp_metadata(1);return 0;}
    if(argc==2 && !strcmp(argv[1],"--trace-cancelled-eof")){cancelled_eof(1);return 0;}
    if(argc==2 && !strcmp(argv[1],"--trace-mismatch")){mismatch_errors(1);return 0;}
    if(argc==2 && !strcmp(argv[1],"--trace-eof")){eof_partial(1);return 0;}
    if(argc==2 && !strcmp(argv[1],"--trace-ignored-tool-end")){ignored_tool_end(1);return 0;}
    if (argc==2 && !strcmp(argv[1],"--trace-events")) {
        trace_doc=yyjson_mut_doc_new(NULL); CHECK(trace_doc);
        trace_events=yyjson_mut_arr(trace_doc); CHECK(trace_events);
        yyjson_mut_doc_set_root(trace_doc,trace_events); event_pipeline();
        char *json=yyjson_mut_write(trace_doc,0,NULL); CHECK(json);
        puts(json); free(json); yyjson_mut_doc_free(trace_doc); return 0;
    }
    repeated_tool_end(0); timestamp_metadata(0); cancelled_eof(0); mismatch_errors(0); eof_partial(0); ignored_tool_end(0); pending_literal_preview(); terminal_events(); event_provider_lifecycle(); event_pipeline(); wire_fragments();
    allocation_failures();
    for (int mode=0;mode<3;++mode) {
        pangu_core *owner=NULL; fixture f={.mode=mode};
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&f.result)==PANGU_OK);
        pangu_proxy *p=NULL; CHECK(pangu_proxy_create(owner,capture,&f,&p)==PANGU_OK);
        CHECK(feed(p,mode)==PANGU_OK); CHECK(pangu_proxy_finish(p)==PANGU_OK);
        CHECK(f.finals==1 && f.updates>=11);
        const pangu_message *m=pangu_core_message_at(f.result,0);
        CHECK(m->content_count==3 && m->content[0].data.len==12);
        CHECK(!memcmp(m->content[0].data.data,"hello 世界",12));
        CHECK(m->content[1].signature.len==10);
        CHECK(m->stop_reason==(mode?PANGU_ERROR:PANGU_TOOL_USE));
        if (!mode) CHECK(m->usage.total==13 && m->content[2].signature.len==8 &&
            m->content[2].tool_namespace.len==13 && m->provider_thinking_level.len==4);
        else CHECK(m->error.len>0);
        pangu_proxy_destroy(p); pangu_core_destroy(f.result);
        pangu_loop_config cfg={.ctx=&f,.request=provider,.finish_turn=finish};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==(mode?PANGU_MODEL_ERROR:PANGU_OK));
        m=pangu_core_message_at(owner,1);
        CHECK(m->role==PANGU_ASSISTANT && m->content_count==3 && m->content[0].data.len==12);
        if (!mode) CHECK(m->content[2].tool_namespace.len==13 && m->provider_thinking_level.len==4);
        pangu_core_destroy(owner);
    }
    return 0;
}
