#include <pangu/core/proxy_http.h>
#include "internal.h"
#include <curl/curl.h>
#include <string.h>
typedef struct { pangu_core *owner; pangu_proxy *proxy; CURL *curl; pangu_status status; } transfer;
static size_t body(char *data,size_t size,size_t count,void *ctx) {
    transfer *t=ctx;
    if (size && count>SIZE_MAX/size) { t->status=PANGU_NOMEM; return 0; }
    long code=0; curl_easy_getinfo(t->curl,CURLINFO_RESPONSE_CODE,&code);
    size_t bytes=size*count;
    if (code<200 || code>=300) return bytes; /* never retain error bodies */
    t->status=pangu_proxy_feed(t->proxy,(pangu_slice){data,bytes});
    return t->status==PANGU_OK?bytes:0;
}
static int progress(void *ctx,curl_off_t a,curl_off_t b,curl_off_t c,curl_off_t d) {
    (void)a; (void)b; (void)c; (void)d;
    return pangu_core_is_cancelled(((transfer *)ctx)->owner);
}
static char *copy(pangu_core *c,pangu_slice value,const char *prefix) {
    size_t n=strlen(prefix);
    if ((value.len && !value.data) || value.len>SIZE_MAX-n-1) return NULL;
    char *result=c->memory.alloc(c->memory.ctx,n+value.len+1);
    if (!result) return NULL;
    memcpy(result,prefix,n); if (value.len) memcpy(result+n,value.data,value.len);
    result[n+value.len]=0; return result;
}
static pangu_status perform(pangu_core *owner,const pangu_proxy_http_options *options,pangu_model_sink sink,pangu_model_event_sink event_sink,void *ctx) {
    if (!owner || !options || (!sink && !event_sink) || options->timeout_ms<0 || !options->url.len ||
        !options->url.data || (options->auth_token.len && !options->auth_token.data) ||
        (options->request_json.len && !options->request_json.data) || options->request_json.len>INT64_MAX) return PANGU_INVALID;
    if (memchr(options->url.data,0,options->url.len) ||
        (options->auth_token.len && (memchr(options->auth_token.data,'\r',options->auth_token.len) ||
        memchr(options->auth_token.data,'\n',options->auth_token.len) || memchr(options->auth_token.data,0,options->auth_token.len)))) return PANGU_INVALID;
    char *url=copy(owner,options->url,"");
    char *authorization=copy(owner,options->auth_token,"Authorization: Bearer ");
    CURL *curl=NULL; struct curl_slist *headers=NULL;
    transfer t={.owner=owner}; pangu_status status=PANGU_NOMEM;
    if (!url || !authorization) goto done;
    status=event_sink?pangu_proxy_create_events(owner,event_sink,ctx,&t.proxy):pangu_proxy_create(owner,sink,ctx,&t.proxy); if (status!=PANGU_OK) goto done;
    status=PANGU_NOMEM;
    curl=curl_easy_init(); if (!curl) goto done; t.curl=curl;
    headers=curl_slist_append(NULL,"Content-Type: application/json"); if (!headers) goto done;
    struct curl_slist *next=curl_slist_append(headers,authorization); if (!next) goto done; headers=next;
#define SET(option,value) do { if (curl_easy_setopt(curl,option,value)!=CURLE_OK) { status=PANGU_INVALID; goto done; } } while (0)
    SET(CURLOPT_URL,url); SET(CURLOPT_PROTOCOLS_STR,"http,https");
    SET(CURLOPT_HTTPHEADER,headers); SET(CURLOPT_POST,1L);
    SET(CURLOPT_POSTFIELDS,options->request_json.data?options->request_json.data:"");
    SET(CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)options->request_json.len);
    SET(CURLOPT_TIMEOUT_MS,options->timeout_ms); SET(CURLOPT_NOSIGNAL,1L);
    SET(CURLOPT_WRITEFUNCTION,body); SET(CURLOPT_WRITEDATA,&t);
    SET(CURLOPT_NOPROGRESS,0L); SET(CURLOPT_XFERINFOFUNCTION,progress); SET(CURLOPT_XFERINFODATA,&t);
#undef SET
    CURLcode result=curl_easy_perform(curl);
    if (result==CURLE_OUT_OF_MEMORY) { status=PANGU_NOMEM; goto done; }
    if (t.status!=PANGU_OK) { status=t.status; goto done; }
    long code=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
    if (result!=CURLE_OK || code<200 || code>=300) {
        const char *message=pangu_core_is_cancelled(owner)?"Request aborted by user":
            result!=CURLE_OK?"Proxy transport failed":"Proxy HTTP request failed";
        pangu_proxy_event error={.kind=PANGU_PROXY_ERROR,.reason=pangu_core_is_cancelled(owner)?PANGU_ABORTED:PANGU_ERROR,
            .error={message,strlen(message)}};
        status=pangu_proxy_push(t.proxy,&error);
        /* A server may close after delivering a terminal event. */
        if (status==PANGU_INVALID) status=pangu_proxy_finish(t.proxy);
    } else status=pangu_proxy_finish(t.proxy);
done:
    curl_slist_free_all(headers); if (curl) curl_easy_cleanup(curl);
    pangu_proxy_destroy(t.proxy);
    if (url) owner->memory.free(owner->memory.ctx,url);
    if (authorization) owner->memory.free(owner->memory.ctx,authorization);
    return status;
}

pangu_status pangu_proxy_http(pangu_core *owner,const pangu_proxy_http_options *options,pangu_model_sink sink,void *ctx) {
    return perform(owner,options,sink,NULL,ctx);
}
pangu_status pangu_proxy_http_events(pangu_core *owner,const pangu_proxy_http_options *options,pangu_model_event_sink sink,void *ctx) {
    return perform(owner,options,NULL,sink,ctx);
}
