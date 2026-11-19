#ifndef PANGU_CORE_PROXY_HTTP_H
#define PANGU_CORE_PROXY_HTTP_H
#include <pangu/core/proxy.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    pangu_slice url; /* complete endpoint, including /api/stream */
    pangu_slice auth_token;
    pangu_slice request_json; /* host-serialized {model,context,options} */
    long timeout_ms; /* zero = no total timeout; negative invalid */
} pangu_proxy_http_options;
/* Optional PanGu::proxy_http target (libcurl). Synchronous, callbacks on caller
 * thread. Host must initialize libcurl globally before concurrent use and keep
 * it initialized until all calls settle. Options/slices borrowed until return.
 * TLS verification uses libcurl defaults. Redirects disabled. HTTP and network
 * failures become a final error assistant; infrastructure/OOM returns status.
 * No token, response body or URL is included in failure messages. Cancellation
 * is cooperative through owner state; callbacks may cancel on this thread.
 * Request serialization stays with the host; it must exclude local authToken,
 * proxyUrl and cancellation fields from provider options, as Pi does. */
pangu_status pangu_proxy_http(pangu_core *,const pangu_proxy_http_options *,pangu_model_sink,void *);
/* Same transport with complete assistant event metadata. */
pangu_status pangu_proxy_http_events(pangu_core *,const pangu_proxy_http_options *,
    pangu_model_event_sink,void *);
#ifdef __cplusplus
}
#endif
#endif
