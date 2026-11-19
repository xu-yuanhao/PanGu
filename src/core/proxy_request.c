/* Pi proxy.ts buildProxyRequestOptions and public message wire representation. */
#include <pangu/core/proxy_request.h>
#include "internal.h"
#include "yyjson.h"
#include <string.h>
typedef struct { pangu_allocator memory; yyjson_alc alc; yyjson_mut_doc *doc; pangu_status status; } writer;
static void *allocate(void *ctx,size_t n) { writer *w=ctx; return w->memory.alloc(w->memory.ctx,n); }
static void release(void *ctx,void *p) { writer *w=ctx; if (p) w->memory.free(w->memory.ctx,p); }
static void *resize(void *ctx,void *p,size_t old,size_t n) {
    void *next=allocate(ctx,n); if (!next) return NULL;
    if (p) { memcpy(next,p,old<n?old:n); release(ctx,p); } return next;
}
static yyjson_mut_val *object(writer *w) { if (w->status!=PANGU_OK) return NULL; yyjson_mut_val *v=yyjson_mut_obj(w->doc); if (!v) w->status=PANGU_NOMEM; return v; }
static yyjson_mut_val *array(writer *w) { if (w->status!=PANGU_OK) return NULL; yyjson_mut_val *v=yyjson_mut_arr(w->doc); if (!v) w->status=PANGU_NOMEM; return v; }
static void put(writer *w,yyjson_mut_val *o,const char *key,yyjson_mut_val *v) {
    if (w->status!=PANGU_OK) return;
    yyjson_mut_val *k=yyjson_mut_str(w->doc,key);
    if (!v || !k || !yyjson_mut_obj_put(o,k,v)) w->status=PANGU_NOMEM;
}
static void string(writer *w,yyjson_mut_val *o,const char *key,pangu_slice s) {
    if (s.len && !s.data) { w->status=PANGU_INVALID; return; }
    put(w,o,key,yyjson_mut_strncpy(w->doc,s.data?s.data:"",s.len));
}
static void present(writer *w,yyjson_mut_val *o,const char *key,pangu_slice s) { if (s.data || s.len) string(w,o,key,s); }
static void number(writer *w,yyjson_mut_val *o,const char *key,uint64_t n) { put(w,o,key,yyjson_mut_uint(w->doc,n)); }
static void item(writer *w,yyjson_mut_val *a,yyjson_mut_val *v) { if (w->status!=PANGU_OK) return; if (!v || !yyjson_mut_arr_append(a,v)) w->status=PANGU_NOMEM; }
static yyjson_mut_val *json(writer *w,pangu_slice s) {
    if (w->status!=PANGU_OK) return NULL;
    if (!s.len || !s.data) { w->status=PANGU_INVALID; return NULL; }
    yyjson_read_err error; yyjson_doc *doc=yyjson_read_opts((char *)s.data,s.len,0,&w->alc,&error);
    if (!doc) { w->status=error.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION?PANGU_NOMEM:PANGU_INVALID; return NULL; }
    yyjson_mut_val *v=yyjson_val_mut_copy(w->doc,yyjson_doc_get_root(doc));
    if (!v) w->status=PANGU_NOMEM;
    yyjson_doc_free(doc); return v;
}
static yyjson_mut_val *metadata(writer *w,pangu_slice s,int list) {
    yyjson_mut_val *value=json(w,s);
    if (w->status==PANGU_OK && !(list?yyjson_mut_is_arr(value):yyjson_mut_is_obj(value)))
        w->status=PANGU_INVALID;
    return value;
}
static yyjson_mut_val *usage(writer *w,pangu_usage u) {
    yyjson_mut_val *v=object(w),*cost=object(w);
    number(w,v,"input",u.input); number(w,v,"output",u.output); number(w,v,"cacheRead",u.cache_read);
    number(w,v,"cacheWrite",u.cache_write); number(w,v,"totalTokens",u.total);
#define COST(key,field) put(w,cost,key,yyjson_mut_real(w->doc,u.field))
    COST("input",input_cost); COST("output",output_cost); COST("cacheRead",cache_read_cost);
    COST("cacheWrite",cache_write_cost); COST("total",total_cost);
#undef COST
    if (u.has_cache_write_1h) number(w,v,"cacheWrite1h",u.cache_write_1h);
    if (u.has_reasoning) number(w,v,"reasoning",u.reasoning);
    if (u.has_total_from_provider) put(w,cost,"totalFromProvider",yyjson_mut_bool(w->doc,u.total_from_provider!=0));
    put(w,v,"cost",cost); return v;
}
static int has_usage(pangu_usage u) {
    return u.input || u.output || u.cache_read || u.cache_write || u.total ||
        u.input_cost || u.output_cost || u.cache_read_cost || u.cache_write_cost || u.total_cost ||
        u.has_cache_write_1h || u.has_reasoning || u.has_total_from_provider;
}
static yyjson_mut_val *content(writer *w,const pangu_content *b) {
    yyjson_mut_val *v=object(w);
    const char *type=b->kind==PANGU_TEXT?"text":b->kind==PANGU_THINKING?"thinking":b->kind==PANGU_IMAGE?"image":"toolCall";
    if (b->kind==PANGU_JSON) return json(w,b->data);
    string(w,v,"type",(pangu_slice){type,strlen(type)});
    if (b->kind==PANGU_TOOL_CALL) {
        string(w,v,"id",b->id); string(w,v,"name",b->name);
        put(w,v,"arguments",json(w,b->data)); present(w,v,"namespace",b->tool_namespace);
        present(w,v,"argumentsError",b->arguments_error); present(w,v,"thoughtSignature",b->signature);
    } else {
        string(w,v,b->kind==PANGU_TEXT?"text":b->kind==PANGU_THINKING?"thinking":"data",b->data);
        if (b->kind==PANGU_IMAGE) string(w,v,"mimeType",b->mime_type);
        else present(w,v,b->kind==PANGU_TEXT?"textSignature":"thinkingSignature",b->signature);
    }
    if (b->kind==PANGU_THINKING && b->has_redacted) put(w,v,"redacted",yyjson_mut_bool(w->doc,b->redacted!=0));
    return v;
}
static yyjson_mut_val *message(writer *w,const pangu_message *m) {
    static const char *roles[]={"system","user","assistant","toolResult"};
    static const char *reasons[]={"pending","stop","length","toolUse","error","aborted","deferred"};
    static const char *levels[]={"off","minimal","low","medium","high","xhigh","max"};
    if ((unsigned)m->role>PANGU_TOOL_RESULT || (unsigned)m->stop_reason>PANGU_DEFERRED ||
        (unsigned)m->thinking_level>PANGU_THINKING_MAX) { w->status=PANGU_INVALID; return NULL; }
    yyjson_mut_val *v=object(w);
    string(w,v,"role",(pangu_slice){roles[m->role],strlen(roles[m->role])});
    put(w,v,"timestamp",yyjson_mut_sint(w->doc,m->timestamp_ms));
    if(m->presence&PANGU_MESSAGE_CONTENT_STRING)string(w,v,"content",m->content_count?m->content[0].data:(pangu_slice){0});
    else {
        yyjson_mut_val *blocks=array(w);
        for (size_t i=0;i<m->content_count;++i) item(w,blocks,content(w,&m->content[i]));
        put(w,v,"content",blocks);
    }
    if (m->role==PANGU_ASSISTANT) {
        string(w,v,"model",m->model); string(w,v,"provider",m->provider); string(w,v,"api",m->api);
        present(w,v,"responseModel",m->response_model); present(w,v,"responseId",m->response_id);
        present(w,v,"rawStopReason",m->raw_stop_reason);
        if (m->diagnostics_json.len) put(w,v,"diagnostics",metadata(w,m->diagnostics_json,1));
        if (m->deferred_json.len) put(w,v,"deferred",metadata(w,m->deferred_json,0));
        if (m->has_end_turn) put(w,v,"endTurn",yyjson_mut_bool(w->doc,m->end_turn!=0));
        string(w,v,"stopReason",(pangu_slice){reasons[m->stop_reason],strlen(reasons[m->stop_reason])});
        if(m->thinking_level!=PANGU_THINKING_OFF || (m->presence&PANGU_MESSAGE_HAS_THINKING_LEVEL))
            string(w,v,"thinkingLevel",(pangu_slice){levels[m->thinking_level],strlen(levels[m->thinking_level])});
        present(w,v,"providerThinkingLevel",m->provider_thinking_level); present(w,v,"errorMessage",m->error);
        put(w,v,"usage",usage(w,m->usage));
    }
    if (m->role==PANGU_TOOL_RESULT) {
        string(w,v,"toolCallId",m->tool_call_id); string(w,v,"toolName",m->tool_name);
        put(w,v,"isError",yyjson_mut_bool(w->doc,m->is_error!=0));
        if (m->details_json.len) put(w,v,"details",json(w,m->details_json));
        if (m->nested_calls_json.len) put(w,v,"nestedCalls",metadata(w,m->nested_calls_json,0));
        if((m->presence&PANGU_MESSAGE_HAS_USAGE) || has_usage(m->usage))put(w,v,"usage",usage(w,m->usage));
    }
    if (m->has_duration) number(w,v,"durationMs",m->duration_ms);
    if (m->role==PANGU_SYSTEM) {
        if (m->section_count || (m->presence&PANGU_MESSAGE_HAS_SECTIONS)) {
            yyjson_mut_val *sections=object(w);
            for (size_t i=0;i<m->section_count;++i) {
                const pangu_system_section *s=&m->sections[i];
                yyjson_mut_val *key=yyjson_mut_strncpy(w->doc,s->name.data?s->name.data:"",s->name.len);
                yyjson_mut_val *value=s->removed?yyjson_mut_null(w->doc):yyjson_mut_strncpy(w->doc,s->value.data?s->value.data:"",s->value.len);
                if (!key || !value || !yyjson_mut_obj_put(sections,key,value)) w->status=PANGU_NOMEM;
            }
            put(w,v,"sections",sections);
        }
        if (m->tools_added_count || (m->presence&PANGU_MESSAGE_HAS_TOOLS_ADDED)) {
            yyjson_mut_val *tools=array(w);
            for (size_t i=0;i<m->tools_added_count;++i) {
                const pangu_tool_declaration *t=&m->tools_added[i]; yyjson_mut_val *tool=object(w);
                string(w,tool,"name",t->name); string(w,tool,"description",t->description);
                put(w,tool,"parameters",json(w,t->schema_json));
                if (t->has_constrained_sampling) put(w,tool,"constrainedSampling",yyjson_mut_bool(w->doc,t->constrained_sampling!=0));
                item(w,tools,tool);
            }
            put(w,v,"toolsAdded",tools);
        }
        if (m->tools_removed_count || (m->presence&PANGU_MESSAGE_HAS_TOOLS_REMOVED)) {
            yyjson_mut_val *tools=array(w);
            for (size_t i=0;i<m->tools_removed_count;++i) { yyjson_mut_val *tool=object(w); string(w,tool,"name",m->tools_removed[i]); item(w,tools,tool); }
            put(w,v,"toolsRemoved",tools);
        }
    }
    return v;
}
pangu_status pangu_proxy_request_json(pangu_core *owner,const pangu_core *request,
    const pangu_model_config *model,pangu_slice options,pangu_core **out) {
    if (!out) return PANGU_INVALID;
    *out=NULL; if (!owner || !request || !model) return PANGU_INVALID;
    writer w={.memory=owner->memory}; w.alc=(yyjson_alc){allocate,resize,release,&w};
    w.doc=yyjson_mut_doc_new(&w.alc); if (!w.doc) return PANGU_NOMEM;
    yyjson_mut_val *root=object(&w),*context=object(&w),*messages=array(&w);
    yyjson_mut_val *m=model->metadata_json.len?json(&w,model->metadata_json):object(&w);
    if (!yyjson_mut_is_obj(m)) { if (w.status==PANGU_OK) w.status=PANGU_INVALID; goto done; }
    string(&w,m,"id",model->id); string(&w,m,"provider",model->provider); string(&w,m,"api",model->api);
    put(&w,root,"model",m);
    for (size_t i=0;i<request->count && w.status==PANGU_OK;++i) item(&w,messages,message(&w,request->messages[i]));
    put(&w,context,"messages",messages); put(&w,root,"context",context);
    yyjson_mut_val *input=options.len?json(&w,options):object(&w),*filtered=object(&w);
    if (!yyjson_mut_is_obj(input)) { if (w.status==PANGU_OK) w.status=PANGU_INVALID; goto done; }
    static const char *keys[]={"temperature","samplingParams","maxTokens","reasoning","cacheRetention","sessionId","headers","metadata","transport","thinkingBudgets","maxRetryDelayMs"};
    for (size_t i=0;i<sizeof(keys)/sizeof(keys[0]);++i) {
        yyjson_mut_val *value=yyjson_mut_obj_get(input,keys[i]); if (value) put(&w,filtered,keys[i],yyjson_mut_val_mut_copy(w.doc,value));
    }
    put(&w,root,"options",filtered); yyjson_mut_doc_set_root(w.doc,root);
    if (w.status==PANGU_OK) {
        size_t len=0; yyjson_write_err error;
        /* JSON.stringify maps nonfinite numeric usage costs to null. */
        char *encoded=yyjson_mut_write_opts(w.doc,YYJSON_WRITE_INF_AND_NAN_AS_NULL,&w.alc,&len,&error);
        if (!encoded) w.status=error.code==YYJSON_WRITE_ERROR_MEMORY_ALLOCATION?PANGU_NOMEM:PANGU_INVALID;
        else {
            pangu_core_config config={.allocator=owner->memory};
            w.status=pangu_core_create(&config,out);
            if (w.status==PANGU_OK) {
                pangu_content b={.kind=PANGU_JSON,.data={encoded,len}};
                pangu_message result={.role=PANGU_CUSTOM,.content=&b,.content_count=1};
                w.status=pangu_core_append(*out,&result);
                if (w.status!=PANGU_OK) { pangu_core_destroy(*out); *out=NULL; }
            }
            release(&w,encoded);
        }
    }
done:
    yyjson_mut_doc_free(w.doc); return w.status;
}

pangu_status pangu_proxy_active_request_json(pangu_core *owner,const pangu_core *request,pangu_core **out) {
    if (!out) return PANGU_INVALID;
    *out=NULL;
    const pangu_model_config *model=pangu_core_active_model(owner);
    if (!model) return PANGU_INVALID;
    return pangu_proxy_request_json(owner,request,model,pangu_core_active_options(owner),out);
}
