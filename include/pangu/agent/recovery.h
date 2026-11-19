#ifndef PANGU_AGENT_RECOVERY_H
#define PANGU_AGENT_RECOVERY_H
#include <pangu/agent/session.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_CONTEXT_CLASSIFIER_SERVICE "pangu.context_classifier"
#define PG_CONTEXT_CLASSIFIER_ABI 1
/* Optional classifier capability. Implementations own provider error parsing;
 * Agent never infers overflow from error strings. Inputs are borrowed slices
 * and immutable messages for the duration of the synchronous callback, on the
 * session run thread. Overflow writes 0/1 only on OK; INVALID/NOMEM stop
 * recovery and are never treated as a non-match. Length returns 0/1. capacity
 * is the configured context window; desired_output is the original model
 * metadata.maxTokens before clamping. Both functions are required for ABI1. No
 * callbacks/slices may be retained. App holds an acquired service lease through
 * run return. */
typedef struct {
  size_t size;
  pangu_status (*is_context_overflow)(void *ctx, const pangu_message *,
                                      uint64_t capacity, int *out);
  int (*is_recoverable_length)(void *ctx, const pangu_message *,
                               uint64_t desired_output);
} pg_context_classifier;
/* Explicit per-session classifier takes priority over an app service. Table
 * and ctx are caller-owned and must live until replaced or session destroyed.
 * NULL restores numeric-only defaults. Owner-thread, idle/no transaction only;
 * never persisted. Getter returns borrowed table and optional borrowed ctx.
 * Service registration uses PG_CONTEXT_CLASSIFIER_SERVICE/ABI above. */
pg_session_status
pg_session_set_context_classifier(pangu_session *,
                                  const pg_context_classifier *, void *ctx);
const pg_context_classifier *
pg_session_context_classifier(const pangu_session *, void **ctx);
#ifdef __cplusplus
}
#endif
#endif
