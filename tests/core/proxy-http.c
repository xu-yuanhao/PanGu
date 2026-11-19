#include <pangu/core/proxy_http.h>
#include <pangu/core/proxy_request.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if (!(x)) abort(); } while (0)
static pangu_status capture(void *ctx,const pangu_message *m,int final) {
    if (final) return pangu_core_append(ctx,m);
    return PANGU_OK;
}
typedef struct { pangu_proxy_http_options options; int cancelled,resolved; } host;
static pangu_status request(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_event_sink sink,void *target) {
    pangu_core *encoded=NULL;
    pangu_status status=pangu_proxy_active_request_json(owner,view,&encoded);
    if (status!=PANGU_OK) return status;
    pangu_proxy_http_options options=((host *)ctx)->options;
    options.request_json=pangu_core_message_at(encoded,0)->content[0].data;
    options.auth_token=pangu_core_active_api_key(owner);
    status=pangu_proxy_http_events(owner,&options,sink,target);
    pangu_core_destroy(encoded); return status;
}
static pangu_status resolve_key(void *ctx,pangu_core *owner,pangu_slice provider,pangu_api_key_sink sink,void *target) {
    (void)provider; CHECK(!pangu_core_active_api_key(owner).len); ++((host *)ctx)->resolved;
    return sink(target,(pangu_slice){"redacted-test",13});
}
static void cancel_event(void *ctx,pangu_core *owner,const pangu_event *event) {
    host *h=ctx;
    if (event->kind==PANGU_MESSAGE_UPDATE && event->message && event->message->content_count &&
        event->message->content[0].data.len && !h->cancelled) {
        CHECK(event->assistant_event && event->assistant_event->kind==PANGU_ASSISTANT_TEXT_DELTA);
        CHECK(event->assistant_event->delta.len==6);
        h->cancelled=1; pangu_core_cancel(owner);
    }
}
int main(int argc,char **argv) {
    CHECK(argc==3 && curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK);
    pangu_core *owner=NULL,*result=NULL;
    CHECK(pangu_core_create(NULL,&owner)==PANGU_OK && pangu_core_create(NULL,&result)==PANGU_OK);
    pangu_core *encoded=NULL;
    pangu_model_config model={0};
    const char *settings="{\"temperature\":0.5,\"sessionId\":\"test-session\",\"authToken\":\"private-local\",\"proxyUrl\":\"local-only\",\"signal\":{},\"unknown\":true}";
    if (!strcmp(argv[2],"nonfinite")) {
        pangu_message previous={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP,
            .usage={.input_cost=INFINITY,.output_cost=-INFINITY,.total_cost=NAN}};
        CHECK(pangu_core_append(owner,&previous)==PANGU_OK);
    }
    CHECK(pangu_proxy_request_json(owner,owner,&model,(pangu_slice){settings,strlen(settings)},&encoded)==PANGU_OK);
    pangu_slice serialized=pangu_core_message_at(encoded,0)->content[0].data;
    const char *json="{\"model\":{},\"context\":{\"messages\":[]},\"options\":{}}";
    pangu_proxy_http_options options={.url={argv[1],strlen(argv[1])},.auth_token={"redacted-test",13},
        .request_json=serialized,.timeout_ms=3000};
    (void)json;
    if (!strcmp(argv[2],"cancel")) {
        host h={.options=options};
        pangu_loop_config cfg={.ctx=&h,.request_events=request,.event=cancel_event,.request_options_json={settings,strlen(settings)},.get_api_key=resolve_key};
        pangu_message prompt={.role=PANGU_USER};
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_CANCELLED && h.cancelled);
        CHECK(!pangu_core_is_running(owner));
        const pangu_message *aborted=pangu_core_message_at(owner,pangu_core_message_count(owner)-1);
        CHECK(aborted->stop_reason==PANGU_ABORTED && aborted->content_count==1);
        CHECK(aborted->content[0].data.len==6 && !memcmp(aborted->content[0].data.data,"世界",6));
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK);
        CHECK(pangu_core_message_at(owner,pangu_core_message_count(owner)-1)->stop_reason==PANGU_STOP && h.resolved==2);
        CHECK(!pangu_core_active_api_key(owner).len);
        pangu_core_destroy(encoded); pangu_core_destroy(result); pangu_core_destroy(owner); curl_global_cleanup(); return 0;
    }
    CHECK(pangu_proxy_http(owner,&options,capture,result)==PANGU_OK);
    CHECK(pangu_core_message_count(result)==1);
    const pangu_message *m=pangu_core_message_at(result,0);
    int success=!strcmp(argv[2],"ok") || !strcmp(argv[2],"nonfinite") || !strcmp(argv[2],"ignored-tool-end");
    CHECK(m->stop_reason==(success?PANGU_STOP:PANGU_ERROR));
    if (success) CHECK(m->content_count==1 && m->content[0].data.len==6);
    pangu_core_destroy(encoded); pangu_core_destroy(result); pangu_core_destroy(owner); curl_global_cleanup(); return 0;
}
