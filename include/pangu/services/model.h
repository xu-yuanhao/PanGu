#ifndef PANGU_SERVICES_MODEL_H
#define PANGU_SERVICES_MODEL_H
#include <pangu/core/loop.h>
#include <pangu/services/host.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_MODEL_SERVICE "pangu.model"
typedef struct {
  pangu_request_events_fn request_events;
} pg_model_service;
/* Immutable borrowed config must outlive plugin scope.
 * endpoint is full /chat/completions URL; empty selects DeepSeek HTTPS.
 * Keys are read exclusively from Core's per-request active_api_key (get_api_key
 * refreshes it). No key is stored here. Synchronous request_events drives curl
 * on run caller thread; no pump. Hold service lease through Core run return.
 * Plugin installation initializes process-global curl once; application-owned
 * global cleanup must happen only after all curl users have quiesced.
 * timeout_ms=0 selects 120s. Only text/thinking/function tools are supported;
 * unsupported content fails explicitly. HTTP errors expose status only. */
typedef struct {
  pangu_slice endpoint;
  /* Legacy source/struct compatibility only; never read for request declarations.
   * Core active_tool_count/at is authoritative, including an empty set.
   * Supply executable tools to pangu_loop_config or its prepare transaction. */
  const pangu_tool *tools;
  size_t tool_count;
  long timeout_ms;
  /* Optional nonblocking same-thread UI dispatch. Non-OK aborts transfer. */
  pangu_status (*poll)(void *, pangu_core *);
  void *poll_ctx;
} pg_deepseek_config;
extern const pg_plugin pg_deepseek_plugin;
#ifdef __cplusplus
}
#endif
#endif
