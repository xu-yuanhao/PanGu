#ifndef PANGU_CORE_PROXY_H
#define PANGU_CORE_PROXY_H
#include <pangu/core/loop.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct pangu_proxy pangu_proxy;
typedef enum { PANGU_PROXY_START, PANGU_PROXY_TEXT_START, PANGU_PROXY_TEXT_DELTA,
    PANGU_PROXY_TEXT_END, PANGU_PROXY_THINKING_START, PANGU_PROXY_THINKING_DELTA,
    PANGU_PROXY_THINKING_END, PANGU_PROXY_TOOL_START, PANGU_PROXY_TOOL_DELTA,
    PANGU_PROXY_TOOL_END, PANGU_PROXY_DONE, PANGU_PROXY_ERROR } pangu_proxy_kind;
typedef struct {
    pangu_proxy_kind kind;
    size_t content_index;
    pangu_slice delta,signature,id,name;
    const pangu_content *tool_call; /* authoritative final arguments; optional
        signature/namespace/arguments_error absent (NULL,0) retain prior values,
        a non-NULL empty slice clears them, matching Pi Object.assign */
    pangu_stop_reason reason;
    pangu_usage usage;
    pangu_slice error,provider_thinking_level;
} pangu_proxy_event;
/* Proxy reconstruction adapter. Host owns HTTP/TLS. The byte-feed entry
 * decodes Pi proxy SSE lines and JSON using yyjson. All calls/sinks run on one thread. Inputs are copied before return;
 * sink snapshots are borrowed only during the callback. Sink callbacks may
 * not reenter, destroy, or push to this adapter. A non-OK sink result
 * stops the adapter. owner/context and sink remain alive until destruction.
 * Tool deltas enter a private incremental parser; snapshots carry valid JSON
 * object previews, never raw incomplete text. Preview updates may lag under
 * snapshot credit limits. TOOL_END supplies authoritative final arguments;
 * as in Pi, TOOL_END for a missing/non-tool block is ignored without a sink
 * event. Delta and text/thinking-end type mismatches remain stream errors. */
/* Reconstructed messages receive a UTC millisecond timestamp at creation;
 * that timestamp is preserved through all partial and terminal snapshots. */
pangu_status pangu_proxy_create(pangu_core *owner,pangu_model_sink,void *,pangu_proxy **out);
/* Event-preserving alternative for request_events providers. Same ownership,
 * reconstruction and byte framing as create; exposes start/delta/end metadata. */
pangu_status pangu_proxy_create_events(pangu_core *owner,pangu_model_event_sink,
    void *,pangu_proxy **out);
pangu_status pangu_proxy_push(pangu_proxy *,const pangu_proxy_event *);
/* Feed arbitrary byte fragments. Each newline-delimited `data: ` JSON event
 * follows Pi framing; other lines are ignored. finish processes the last line
 * without requiring a newline. Byte storage grows with the current line only.
 * Stop feeding after a non-OK return; parse errors produce final ERROR. */
pangu_status pangu_proxy_feed(pangu_proxy *,pangu_slice bytes);
/* EOF without a terminal event becomes an error assistant preserving content.
 * Cancellation becomes ABORTED before parsing a buffered final line, retaining
 * only already-published content. Repeated finish after terminal is a no-op. */
pangu_status pangu_proxy_finish(pangu_proxy *);
void pangu_proxy_destroy(pangu_proxy *);
#ifdef __cplusplus
}
#endif
#endif
