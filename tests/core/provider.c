#include <pangu/core/loop.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) abort(); } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
typedef struct { size_t allocations,fail,live; int mode,phase,sent,responses,trace; } host;
static void *allocate(void *ctx,size_t n) {
    host *h=ctx; if (++h->allocations==h->fail) return NULL;
    void *p=malloc(n); if (p) ++h->live; return p;
}
static void release(void *ctx,void *p) { if (p) { --((host *)ctx)->live; free(p); } }
static int same(pangu_slice s,const char *v) { return s.len==strlen(v) && !memcmp(s.data,v,s.len); }
static pangu_status payload_hook(void *ctx,pangu_core *owner,const pangu_model_config *model,pangu_slice input,pangu_payload_sink sink,void *target) {
    host *h=ctx; CHECK(h->phase==0 && same(model->id,"mock") && same(input,"{\"original\":true}"));
    CHECK(pangu_core_is_running(owner)); h->phase=1;
    if (h->trace) puts("payload");
    if (h->mode==1) return PANGU_OK;
    char next[]="{\"changed\":true}";
    pangu_status status=sink(target,h->mode==2?S("null"):h->mode==5?S("broken"):(pangu_slice){next,sizeof(next)-1});
    memset(next,'X',sizeof(next)-1);
    if (status!=PANGU_OK) return status;
    if (h->mode==3) return PANGU_MODEL_ERROR;
    if (h->mode==4) { (void)sink(target,S("false")); return PANGU_OK; }
    return PANGU_OK;
}
static pangu_status response_hook(void *ctx,pangu_core *owner,const pangu_model_config *model,const pangu_provider_response *response) {
    host *h=ctx; CHECK(h->phase==2 && same(model->id,"mock"));
    CHECK(pangu_core_is_running(owner) && response->status==200 && response->header_count==1);
    CHECK(same(response->headers[0].name,"x-test") && same(response->headers[0].value,"present"));
    h->phase=3; ++h->responses;
    if (h->trace) puts("response:200");
    return h->mode==6?PANGU_MODEL_ERROR:PANGU_OK;
}
/* Scripted direct protocol adapter: prepare -> send -> headers -> body. */
static pangu_status provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    (void)request; host *h=ctx; pangu_core *payload=NULL;
    pangu_status status=pangu_core_prepare_payload(owner,S("{\"original\":true}"),&payload);
    if (status!=PANGU_OK) { CHECK(!payload && !h->sent); return status; }
    CHECK(h->phase==(h->mode==7?0:1));
    const pangu_message *message=pangu_core_message_at(payload,0);
    CHECK(message && message->content_count==1);
    pangu_slice body=message->content[0].data;
    CHECK(same(body,h->mode==2?"null":h->mode==1 || h->mode==7?"{\"original\":true}":"{\"changed\":true}"));
    if (h->trace) printf("send:%.*s\n",(int)body.len,body.data);
    ++h->sent; h->phase=2; pangu_core_destroy(payload);
    pangu_response_header header={.name=S("x-test"),.value=S("present")};
    pangu_provider_response response={.status=200,.headers=&header,.header_count=1};
    status=pangu_core_observe_response(owner,&response);
    if (status!=PANGU_OK) return status;
    CHECK(h->phase==(h->mode==7?2:3));
    if (h->trace) puts("body");
    pangu_message result={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP};
    return sink(target,&result,1);
}
static pangu_status run(host *h) {
    pangu_core *owner=NULL; pangu_core_config memory={.allocator={h,allocate,release}};
    CHECK(pangu_core_create(&memory,&owner)==PANGU_OK);
    pangu_core *out=(pangu_core *)1;
    CHECK(pangu_core_prepare_payload(owner,S("{}"),&out)==PANGU_INVALID && !out);
    pangu_provider_response response={0};
    CHECK(pangu_core_observe_response(owner,&response)==PANGU_INVALID);
    pangu_loop_config cfg={.ctx=h,.request=provider,.model_config={.id=S("mock")},
        .on_payload=h->mode==7?NULL:payload_hook,.on_response=h->mode==7?NULL:response_hook};
    pangu_message prompt={.role=PANGU_USER};
    pangu_status status=pangu_core_run(owner,&prompt,1,&cfg);
    pangu_core_destroy(owner); CHECK(!h->live); return status;
}
int main(int argc,char **argv) {
    int trace=argc==2 && !strcmp(argv[1],"--trace-hooks");
    for (int mode=0;mode<(trace?3:8);++mode) {
        host h={.mode=mode,.trace=trace};
        CHECK(run(&h)==((mode>=3 && mode<=6)?PANGU_MODEL_ERROR:PANGU_OK));
        CHECK(h.sent==((mode>=3 && mode<=5)?0:1));
        CHECK(h.responses==((mode>=3 && mode<=5) || mode==7?0:1));
    }
    if (trace) return 0;
    int completed=0;
    for (size_t fail=2;fail<250;++fail) {
        host h={.fail=fail};
        pangu_status status=run(&h); CHECK(status==PANGU_OK || status==PANGU_NOMEM);
        if (status==PANGU_OK) { completed=1; break; }
    }
    CHECK(completed); return 0;
}
