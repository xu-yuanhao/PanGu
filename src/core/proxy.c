/* Native typed processProxyEvent adapter; Pi d5b3de22, MIT (Pi-LICENSE). */
#include <pangu/core/proxy.h>
#include "internal.h"
#include <string.h>
#include "yyjson.h"
#include "streaming_json.h"
struct pangu_proxy {
    pangu_core *owner,*snapshot;
    pangu_model_sink sink;
    pangu_model_event_sink event_sink;
    void *ctx;
    char *line;
    size_t line_len,line_capacity;
    pangu_json_stream **arguments;
    size_t arguments_capacity;
    int terminal;
    pangu_status failure;
};
static pangu_slice text(const char *s) { return (pangu_slice){s,strlen(s)}; }
static pangu_status publish(pangu_proxy *p,const pangu_message *message,int final,const pangu_proxy_event *event) {
    pangu_core *next=NULL; pangu_core_config config={.allocator=p->owner->memory};
    pangu_status s=pangu_core_create(&config,&next);
    if (s==PANGU_OK) s=pangu_core_append(next,message);
    if (s!=PANGU_OK) { pangu_core_destroy(next); return p->failure=s; }
    pangu_core *old=p->snapshot; p->snapshot=next;
    pangu_core_destroy(old);
    if (final) p->terminal=1;
    const pangu_message *saved=pangu_core_message_at(next,0);
    if (p->event_sink) {
        pangu_assistant_event e={.kind=final?(saved->stop_reason==PANGU_ERROR || saved->stop_reason==PANGU_ABORTED?
            PANGU_ASSISTANT_ERROR:PANGU_ASSISTANT_DONE):PANGU_ASSISTANT_START,.reason=saved->stop_reason};
        if (event && !final) {
            /* Both enums follow the reference wire event order. */
            e.kind=(pangu_assistant_kind)event->kind;
            e.content_index=event->content_index; e.delta=event->delta;
            if (e.content_index<saved->content_count) {
                const pangu_content *b=&saved->content[e.content_index];
                if (e.kind==PANGU_ASSISTANT_TEXT_END || e.kind==PANGU_ASSISTANT_THINKING_END) e.content=b->data;
                if (e.kind==PANGU_ASSISTANT_TOOL_END) e.tool_call=b;
            }
        }
        s=p->event_sink(p->ctx,saved,&e);
    } else s=p->sink(p->ctx,saved,final);
    if (s!=PANGU_OK) p->failure=s;
    return s;
}
static pangu_status create(pangu_core *owner,pangu_model_sink sink,pangu_model_event_sink event_sink,void *ctx,pangu_proxy **out) {
    if (!out) return PANGU_INVALID;
    *out=NULL; if (!owner || (!sink && !event_sink)) return PANGU_INVALID;
    pangu_proxy *p=owner->memory.alloc(owner->memory.ctx,sizeof(*p));
    if (!p) return PANGU_NOMEM;
    *p=(pangu_proxy){.owner=owner,.sink=sink,.event_sink=event_sink,.ctx=ctx};
    pangu_core_config config={.allocator=owner->memory};
    pangu_status s=pangu_core_create(&config,&p->snapshot);
    const pangu_model_config *model=pangu_core_active_model(owner);
    pangu_message m={.role=PANGU_ASSISTANT,.stop_reason=PANGU_PENDING,
        .timestamp_ms=pangu_core_wallclock_ms()};
    if (model) { m.model=model->id; m.provider=model->provider; m.api=model->api; }
    if (s==PANGU_OK) s=pangu_core_append(p->snapshot,&m);
    if (s!=PANGU_OK) { pangu_proxy_destroy(p); return s; }
    *out=p; return PANGU_OK;
}
pangu_status pangu_proxy_create(pangu_core *owner,pangu_model_sink sink,void *ctx,pangu_proxy **out) {
    return create(owner,sink,NULL,ctx,out);
}
pangu_status pangu_proxy_create_events(pangu_core *owner,pangu_model_event_sink sink,void *ctx,pangu_proxy **out) {
    return create(owner,NULL,sink,ctx,out);
}
void pangu_proxy_destroy(pangu_proxy *p) {
    if (!p) return;
    pangu_allocator memory=p->owner->memory;
    pangu_core_destroy(p->snapshot);
    if (p->line) memory.free(memory.ctx,p->line);
    for (size_t i=0;i<p->arguments_capacity;++i) pangu_json_stream_destroy(p->arguments[i]);
    if (p->arguments) memory.free(memory.ctx,p->arguments);
    memory.free(memory.ctx,p);
}
static pangu_status fail(pangu_proxy *p,const char *error) {
    pangu_message m=*pangu_core_message_at(p->snapshot,0);
    m.stop_reason=pangu_core_is_cancelled(p->owner)?PANGU_ABORTED:PANGU_ERROR;
    m.error=text(error);
    return publish(p,&m,1,NULL);
}
static pangu_status argument_slot(pangu_proxy *p,size_t index) {
    if (index<p->arguments_capacity) return PANGU_OK;
    if (index==SIZE_MAX || index+1>SIZE_MAX/sizeof(*p->arguments)) return PANGU_NOMEM;
    size_t capacity=index+1;
    pangu_json_stream **items=p->owner->memory.alloc(p->owner->memory.ctx,capacity*sizeof(*items));
    if (!items) return PANGU_NOMEM;
    memset(items,0,capacity*sizeof(*items));
    if (p->arguments_capacity) memcpy(items,p->arguments,p->arguments_capacity*sizeof(*items));
    if (p->arguments) p->owner->memory.free(p->owner->memory.ctx,p->arguments);
    p->arguments=items; p->arguments_capacity=capacity; return PANGU_OK;
}
static const char *mismatch_error(pangu_proxy_kind kind) {
    switch(kind){
    case PANGU_PROXY_TEXT_DELTA:return "Received text_delta for non-text content";
    case PANGU_PROXY_TEXT_END:return "Received text_end for non-text content";
    case PANGU_PROXY_THINKING_DELTA:return "Received thinking_delta for non-thinking content";
    case PANGU_PROXY_THINKING_END:return "Received thinking_end for non-thinking content";
    case PANGU_PROXY_TOOL_DELTA:return "Received toolcall_delta for non-toolCall content";
    default:return "Invalid proxy content index";
    }
}
pangu_status pangu_proxy_push(pangu_proxy *p,const pangu_proxy_event *e) {
    if (!p || !e) return PANGU_INVALID;
    if (p->failure!=PANGU_OK) return p->failure;
    if (p->terminal) return PANGU_INVALID;
    if (pangu_core_is_cancelled(p->owner)) return fail(p,"Request aborted by user");
    pangu_message m=*pangu_core_message_at(p->snapshot,0);
    if (e->kind==PANGU_PROXY_START) return publish(p,&m,0,e);
    if (e->kind==PANGU_PROXY_DONE || e->kind==PANGU_PROXY_ERROR) {
        int error=e->kind==PANGU_PROXY_ERROR;
        if ((error && e->reason!=PANGU_ERROR && e->reason!=PANGU_ABORTED) ||
            (!error && e->reason!=PANGU_STOP && e->reason!=PANGU_LENGTH && e->reason!=PANGU_TOOL_USE))
            return fail(p,"Invalid proxy terminal reason");
        m.stop_reason=e->reason; m.error=e->error; m.usage=e->usage;
        m.provider_thinking_level=e->provider_thinking_level;
        return publish(p,&m,1,e);
    }
    /* Pi toolcall_end is a no-op when its original tool block is absent or
     * has been replaced; delta and text/thinking-end mismatches still fail. */
    if (e->kind==PANGU_PROXY_TOOL_END &&
        (e->content_index>=m.content_count || m.content[e->content_index].kind!=PANGU_TOOL_CALL))
        return PANGU_OK;
    int start=e->kind==PANGU_PROXY_TEXT_START || e->kind==PANGU_PROXY_THINKING_START || e->kind==PANGU_PROXY_TOOL_START;
    if ((unsigned)e->kind>PANGU_PROXY_ERROR || e->content_index>m.content_count ||
        (!start && e->content_index==m.content_count)) return fail(p,mismatch_error(e->kind));
    size_t count=m.content_count+(start && e->content_index==m.content_count);
    if (count>SIZE_MAX/sizeof(pangu_content)) return p->failure=PANGU_NOMEM;
    pangu_allocator a=p->owner->memory;
    pangu_content *blocks=a.alloc(a.ctx,count*sizeof(*blocks));
    if (!blocks) return p->failure=PANGU_NOMEM;
    if (m.content_count) memcpy(blocks,m.content,m.content_count*sizeof(*blocks));
    pangu_content *b=&blocks[e->content_index]; char *joined=NULL;
    pangu_status status=PANGU_OK;
    if (start) {
        pangu_content_kind kind=e->kind==PANGU_PROXY_TEXT_START?PANGU_TEXT:
            e->kind==PANGU_PROXY_THINKING_START?PANGU_THINKING:PANGU_TOOL_CALL;
        *b=(pangu_content){.kind=kind};
        status=argument_slot(p,e->content_index);
        if (status!=PANGU_OK) goto done;
        pangu_json_stream_destroy(p->arguments[e->content_index]); p->arguments[e->content_index]=NULL;
        if (kind==PANGU_TOOL_CALL) {
            b->id=e->id; b->name=e->name; b->data=text("{}");
            status=pangu_json_stream_create(a,&p->arguments[e->content_index]);
            if (status!=PANGU_OK) goto done;
        }
    } else {
        pangu_content_kind expected=(e->kind==PANGU_PROXY_TEXT_DELTA || e->kind==PANGU_PROXY_TEXT_END)?PANGU_TEXT:
            (e->kind==PANGU_PROXY_THINKING_DELTA || e->kind==PANGU_PROXY_THINKING_END)?PANGU_THINKING:PANGU_TOOL_CALL;
        if (b->kind!=expected) { status=fail(p,mismatch_error(e->kind)); goto done; }
        if (e->kind==PANGU_PROXY_TOOL_END) {
            if (!e->tool_call || e->tool_call->kind!=PANGU_TOOL_CALL) { status=fail(p,"Invalid final proxy tool call"); goto done; }
            /* Pi uses Object.assign: an omitted optional property retains
             * earlier metadata, while an explicit empty string clears it. */
            pangu_content final=*e->tool_call;
            if (!final.signature.data && !final.signature.len) final.signature=b->signature;
            if (!final.tool_namespace.data && !final.tool_namespace.len) final.tool_namespace=b->tool_namespace;
            if (!final.arguments_error.data && !final.arguments_error.len) final.arguments_error=b->arguments_error;
            *b=final;
        } else if (e->kind==PANGU_PROXY_TOOL_DELTA) {
            if (e->content_index>=p->arguments_capacity || !p->arguments[e->content_index]) {
                status=fail(p,mismatch_error(e->kind)); goto done;
            }
            status=pangu_json_stream_append(p->arguments[e->content_index],e->delta);
            if (status==PANGU_OK) status=pangu_json_stream_snapshot(p->arguments[e->content_index],&b->data);
            if (status!=PANGU_OK) goto done;
        } else if (e->kind==PANGU_PROXY_TEXT_END || e->kind==PANGU_PROXY_THINKING_END) b->signature=e->signature;
        else {
            if ((e->delta.len && !e->delta.data) || e->delta.len>SIZE_MAX-b->data.len) { status=PANGU_INVALID; goto done; }
            size_t len=b->data.len+e->delta.len;
            joined=len?a.alloc(a.ctx,len):NULL;
            if (len && !joined) { status=PANGU_NOMEM; goto done; }
            if (b->data.len) memcpy(joined,b->data.data,b->data.len);
            if (e->delta.len) memcpy(joined+b->data.len,e->delta.data,e->delta.len);
            b->data=(pangu_slice){joined,len};
        }
    }
    m.content=blocks; m.content_count=count;
    status=publish(p,&m,0,e);
    if (status==PANGU_OK && e->kind==PANGU_PROXY_TOOL_END && e->content_index<p->arguments_capacity) {
        pangu_json_stream_destroy(p->arguments[e->content_index]); p->arguments[e->content_index]=NULL;
    }
done:
    if (joined) a.free(a.ctx,joined);
    a.free(a.ctx,blocks);
    if (status!=PANGU_OK) p->failure=status;
    return status;
}
static void *json_alloc(void *ctx,size_t n) { pangu_proxy *p=ctx; return p->owner->memory.alloc(p->owner->memory.ctx,n); }
static void json_free(void *ctx,void *value) { pangu_proxy *p=ctx; if (value) p->owner->memory.free(p->owner->memory.ctx,value); }
static void *json_realloc(void *ctx,void *value,size_t old,size_t n) {
    void *next=json_alloc(ctx,n); if (!next) return NULL;
    if (value) { memcpy(next,value,old<n?old:n); json_free(ctx,value); } return next;
}
static pangu_slice string_value(yyjson_val *value) {
    return (pangu_slice){yyjson_get_str(value),yyjson_get_len(value)};
}
static int matches(pangu_slice value,const char *expected) {
    return value.len==strlen(expected) && value.data && !memcmp(value.data,expected,value.len);
}
static pangu_status process_line(pangu_proxy *p) {
    if (p->terminal || p->line_len<6 || memcmp(p->line,"data: ",6)) return PANGU_OK;
    pangu_slice trimmed=pangu_json_trim((pangu_slice){p->line+6,p->line_len-6});
    size_t length=trimmed.len; char *data=(char *)trimmed.data;
    if (!length) return PANGU_OK;
    yyjson_alc alc={json_alloc,json_realloc,json_free,p}; yyjson_read_err error;
    yyjson_doc *doc=yyjson_read_opts(data,length,0,&alc,&error);
    if (!doc) {
        if (error.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION) return p->failure=PANGU_NOMEM;
        return fail(p,"Invalid proxy JSON event");
    }
    yyjson_val *root=yyjson_doc_get_root(doc);
    pangu_slice type=string_value(yyjson_obj_get(root,"type"));
    static const char *types[]={"start","text_start","text_delta","text_end","thinking_start",
        "thinking_delta","thinking_end","toolcall_start","toolcall_delta","toolcall_end","done","error"};
    size_t kind=0; while (kind<sizeof(types)/sizeof(types[0]) && !matches(type,types[kind])) ++kind;
    pangu_status status=PANGU_OK; char *arguments=NULL;
    if (kind==sizeof(types)/sizeof(types[0])) goto done; /* Pi ignores unknown events */
    pangu_proxy_event event={.kind=(pangu_proxy_kind)kind};
    if (kind>0 && kind<10) {
        yyjson_val *index=yyjson_obj_get(root,"contentIndex");
        if (!yyjson_is_uint(index) || yyjson_get_uint(index)>SIZE_MAX) {
            if (kind==PANGU_PROXY_TOOL_END) goto done;
            status=fail(p,mismatch_error((pangu_proxy_kind)kind)); goto done;
        }
        event.content_index=(size_t)yyjson_get_uint(index);
    }
    if (kind==PANGU_PROXY_TOOL_END) {
        const pangu_message *snapshot=pangu_core_message_at(p->snapshot,0);
        if (event.content_index>=snapshot->content_count ||
            snapshot->content[event.content_index].kind!=PANGU_TOOL_CALL) goto done;
    }
    event.delta=string_value(yyjson_obj_get(root,"delta"));
    event.signature=string_value(yyjson_obj_get(root,"contentSignature"));
    event.id=string_value(yyjson_obj_get(root,"id")); event.name=string_value(yyjson_obj_get(root,"toolName"));
    pangu_content call={.kind=PANGU_TOOL_CALL};
    if (kind==PANGU_PROXY_TOOL_END) {
        yyjson_val *tool=yyjson_obj_get(root,"toolCall");
        yyjson_val *args=yyjson_obj_get(tool,"arguments");
        if (!yyjson_is_obj(tool) || !args) { status=fail(p,"Invalid proxy final tool call"); goto done; }
        size_t bytes=0; yyjson_write_err write_error;
        arguments=yyjson_val_write_opts(args,0,&alc,&bytes,&write_error);
        if (!arguments) { status=p->failure=PANGU_NOMEM; goto done; }
        call.id=string_value(yyjson_obj_get(tool,"id")); call.name=string_value(yyjson_obj_get(tool,"name"));
        call.tool_namespace=string_value(yyjson_obj_get(tool,"namespace"));
        call.arguments_error=string_value(yyjson_obj_get(tool,"argumentsError"));
        call.signature=string_value(yyjson_obj_get(tool,"thoughtSignature"));
        call.data=(pangu_slice){arguments,bytes}; event.tool_call=&call;
    }
    if (kind==PANGU_PROXY_DONE || kind==PANGU_PROXY_ERROR) {
        pangu_slice reason=string_value(yyjson_obj_get(root,"reason"));
        if (matches(reason,"stop")) event.reason=PANGU_STOP;
        else if (matches(reason,"length")) event.reason=PANGU_LENGTH;
        else if (matches(reason,"toolUse")) event.reason=PANGU_TOOL_USE;
        else if (matches(reason,"error")) event.reason=PANGU_ERROR;
        else if (matches(reason,"aborted")) event.reason=PANGU_ABORTED;
        else { status=fail(p,"Invalid proxy terminal reason"); goto done; }
        event.error=string_value(yyjson_obj_get(root,"errorMessage"));
        event.provider_thinking_level=string_value(yyjson_obj_get(root,"providerThinkingLevel"));
        yyjson_val *usage=yyjson_obj_get(root,"usage"),*cost=yyjson_obj_get(usage,"cost");
#define TOK(field,key) event.usage.field=yyjson_get_uint(yyjson_obj_get(usage,key))
        TOK(input,"input"); TOK(output,"output"); TOK(cache_read,"cacheRead"); TOK(cache_write,"cacheWrite"); TOK(total,"totalTokens");
#undef TOK
#define COST(field,key) event.usage.field=yyjson_get_num(yyjson_obj_get(cost,key))
        COST(input_cost,"input"); COST(output_cost,"output"); COST(cache_read_cost,"cacheRead"); COST(cache_write_cost,"cacheWrite"); COST(total_cost,"total");
#undef COST
        yyjson_val *cache_1h=yyjson_obj_get(usage,"cacheWrite1h"),*reasoning=yyjson_obj_get(usage,"reasoning"),*billed=yyjson_obj_get(cost,"totalFromProvider");
        event.usage.has_cache_write_1h=yyjson_is_uint(cache_1h); event.usage.cache_write_1h=yyjson_get_uint(cache_1h);
        event.usage.has_reasoning=yyjson_is_uint(reasoning); event.usage.reasoning=yyjson_get_uint(reasoning);
        event.usage.has_total_from_provider=yyjson_is_bool(billed); event.usage.total_from_provider=yyjson_get_bool(billed);
    }
    status=pangu_proxy_push(p,&event);
done:
    if (arguments) json_free(p,arguments);
    yyjson_doc_free(doc); return status;
}
pangu_status pangu_proxy_feed(pangu_proxy *p,pangu_slice bytes) {
    if (!p || (bytes.len && !bytes.data)) return PANGU_INVALID;
    if (p->failure!=PANGU_OK) return p->failure;
    if (p->terminal) return PANGU_OK;
    for (size_t i=0;i<bytes.len;++i) {
        if (bytes.data[i]=='\n') {
            pangu_status s=process_line(p); p->line_len=0;
            if (s!=PANGU_OK || p->terminal) return s;
            continue;
        }
        if (p->line_len==p->line_capacity) {
            size_t capacity=p->line_capacity?p->line_capacity*2:256;
            if (capacity<p->line_capacity) return p->failure=PANGU_NOMEM;
            char *line=json_alloc(p,capacity); if (!line) return p->failure=PANGU_NOMEM;
            if (p->line_len) memcpy(line,p->line,p->line_len);
            json_free(p,p->line); p->line=line; p->line_capacity=capacity;
        }
        p->line[p->line_len++]=bytes.data[i];
    }
    return PANGU_OK;
}
pangu_status pangu_proxy_finish(pangu_proxy *p) {
    if (!p) return PANGU_INVALID;
    if (p->failure!=PANGU_OK) return p->failure;
    if (p->terminal) return PANGU_OK;
    /* streamProxy checks abort after reader EOF, before decoding the tail. */
    if (pangu_core_is_cancelled(p->owner)) return fail(p,"Request aborted by user");
    if (p->line_len) {
        pangu_status s=process_line(p); p->line_len=0;
        if (s!=PANGU_OK || p->terminal) return s;
    }
    return fail(p,pangu_core_is_cancelled(p->owner)?"Request aborted by user":
        "Connection closed by proxy server before the response completed");
}
