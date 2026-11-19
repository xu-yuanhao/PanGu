#ifndef PANGU_CORE_PROXY_REQUEST_H
#define PANGU_CORE_PROXY_REQUEST_H
#include <pangu/core/proxy.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Serialize supported public message fields to Pi proxy {model,context,options}.
 * model.metadata_json is the full model object; id/provider/api override it.
 * options_json is an object or empty. Only Pi's serializable option allowlist
 * is copied; local authToken/proxyUrl/signal and unknown fields are omitted.
 * Custom messages must be converted before calling. Invalid embedded JSON is
 * INVALID. Nonfinite usage costs serialize as null, like JSON.stringify;
 * in-memory costs are unchanged. On success *out owns one JSON content block,
 * valid until destroyed.
 * Allocator context of owner must outlive output. No owner state is modified. */
pangu_status pangu_proxy_request_json(pangu_core *owner,const pangu_core *request,
    const pangu_model_config *,pangu_slice options_json,pangu_core **out);
/* Provider convenience: use owner's current model/options with the actual
 * transformed request. INVALID when idle. Same output ownership/allowlist. */
pangu_status pangu_proxy_active_request_json(pangu_core *owner,
    const pangu_core *request,pangu_core **out);
#ifdef __cplusplus
}
#endif
#endif
