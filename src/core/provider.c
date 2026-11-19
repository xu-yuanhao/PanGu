/* Native JSON/borrowed-header adaptation of Pi ProviderRequestOptions. */
#include "internal.h"
#include "yyjson.h"
#include <string.h>
typedef struct { pangu_core *owner,*result; int submitted; pangu_status failure; } payload;
static void *allocate(void *ctx,size_t n) { pangu_core *c=ctx; return c->memory.alloc(c->memory.ctx,n); }
static void release(void *ctx,void *p) { pangu_core *c=ctx; if (p) c->memory.free(c->memory.ctx,p); }
static void *resize(void *ctx,void *p,size_t old,size_t n) {
    void *next=allocate(ctx,n); if (!next) return NULL;
    if (p) { memcpy(next,p,old<n?old:n); release(ctx,p); } return next;
}
pangu_status pangu_core_validate_json(pangu_core *c,pangu_slice bytes,int require_object) {
    if (!bytes.len || !bytes.data) return PANGU_INVALID;
    yyjson_alc alc={allocate,resize,release,c}; yyjson_read_err error;
    yyjson_doc *doc=yyjson_read_opts((char *)bytes.data,bytes.len,0,&alc,&error);
    if (!doc) return error.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION?PANGU_NOMEM:PANGU_INVALID;
    int valid=!require_object || yyjson_is_obj(yyjson_doc_get_root(doc));
    yyjson_doc_free(doc); return valid?PANGU_OK:PANGU_INVALID;
}
static pangu_status hold(pangu_core *owner,pangu_slice bytes,pangu_core **out) {
    pangu_core_config config={.allocator=owner->memory};
    pangu_status status=pangu_core_create(&config,out);
    if (status!=PANGU_OK) return status;
    pangu_content block={.kind=PANGU_JSON,.data=bytes};
    pangu_message message={.role=PANGU_CUSTOM,.content=&block,.content_count=1};
    status=pangu_core_append(*out,&message);
    if (status!=PANGU_OK) { pangu_core_destroy(*out); *out=NULL; }
    return status;
}
static pangu_status replace(void *ctx,pangu_slice bytes) {
    payload *p=ctx;
    if (p->failure!=PANGU_OK) return p->failure;
    if (p->submitted) return p->failure=PANGU_INVALID;
    p->submitted=1;
    pangu_status status=pangu_core_validate_json(p->owner,bytes,0);
    if (status==PANGU_OK) status=hold(p->owner,bytes,&p->result);
    return p->failure=status;
}
pangu_status pangu_core_prepare_payload(pangu_core *owner,pangu_slice input,pangu_core **out) {
    if (!out) return PANGU_INVALID;
    *out=NULL;
    if (!owner || !owner->active_config) return PANGU_INVALID;
    pangu_status status=pangu_core_validate_json(owner,input,0);
    if (status!=PANGU_OK) return status;
    const pangu_loop_config *cfg=owner->active_config;
    payload p={.owner=owner};
    if (cfg->on_payload) status=cfg->on_payload(cfg->ctx,owner,&cfg->model_config,input,replace,&p);
    if (p.failure!=PANGU_OK) status=p.failure;
    if (status==PANGU_OK && !p.submitted) status=hold(owner,input,&p.result);
    if (status==PANGU_OK) *out=p.result;
    else pangu_core_destroy(p.result);
    return status;
}
pangu_status pangu_core_observe_response(pangu_core *owner,const pangu_provider_response *response) {
    if (!owner || !owner->active_config || !response || (response->header_count && !response->headers)) return PANGU_INVALID;
    for (size_t i=0;i<response->header_count;++i) {
        const pangu_response_header *h=&response->headers[i];
        if ((h->name.len && !h->name.data) || (h->value.len && !h->value.data)) return PANGU_INVALID;
    }
    const pangu_loop_config *cfg=owner->active_config;
    return cfg->on_response?cfg->on_response(cfg->ctx,owner,&cfg->model_config,response):PANGU_OK;
}
