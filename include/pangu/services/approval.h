#ifndef PANGU_SERVICES_APPROVAL_H
#define PANGU_SERVICES_APPROVAL_H
#include <pangu/services/tools.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_APPROVAL_SERVICE "pangu.approval"
/* Separate from Agent's durable enum: adapters must map all four explicitly. */
typedef enum { PG_SERVICE_ALLOWED_ONCE, PG_SERVICE_REJECTED,
  PG_SERVICE_CANCELLED, PG_SERVICE_UNAVAILABLE } pg_service_approval_outcome;
/* UTF-8 byte slices, never required to be NUL terminated. tool_name is
 * nonempty; call_id/reason may be empty. No tool arguments are copied here. */
typedef struct { pangu_slice tool_name, call_id, reason; } pg_service_approval_question;
typedef enum { PG_APPROVAL_ASK, PG_APPROVAL_NEVER } pg_service_approval_policy;
typedef struct pg_approval_provider pg_approval_provider;
typedef pg_status (*pg_service_answer_fn)(void *, pg_approval_provider *, uint64_t,
    pangu_core *, const pg_service_approval_question *, pg_service_approval_outcome *);
typedef struct {
  size_t size;
  pg_status (*request)(void *, pangu_core *, const pg_service_approval_question *,
                      pg_service_approval_outcome *);
} pg_approval_service;
/* All administration/requests/replies are on the run owner thread. Callback
 * borrows readonly slices through return; it may pump UI and reply/cancel by
 * id. It MUST quiesce before returning and must not retain provider/core/q.
 * request holds a host lease externally. revoke BEFORE releasing leases and
 * disposing the scope: it closes in-flight questions UNAVAILABLE and future
 * requests fail closed. Host disposal waits for leases (host.h); never dispose
 * from an active callback holding its own lease. provider is scope-owned and
 * valid only until disposal; reply IDs expire on return, duplicates NOT_FOUND.
 * core cancellation before/after callback wins. Error/invalid answer fails
 * closed even after a callback supplied a grant. No durable audit is stored.
 * Cancellation is cooperative: a UI pump must observe Core cancellation and
 * return promptly; revoke does not interrupt a blocked synchronous answerer.
 * Only provider_reply may use live IDs; these differ from durable Agent IDs.
 * Agent routing uses one scope per agent; child bindings shadow ancestors.
 * config/context is borrowed until scope disposal; no new thread is created.
 */
pg_status pg_approval_provider_register(pg_scope *, pg_service_answer_fn, void *,
                                       pg_approval_provider **);
void pg_approval_provider_revoke(pg_approval_provider *);
pg_status pg_approval_provider_reply(pg_approval_provider *, uint64_t,
                                     pg_service_approval_outcome);
/* Lookup on each request, hold lease through callback. Missing provider =>
 * OK + UNAVAILABLE; out is initialized before any possible failure. */
pg_status pg_approval_ask(pg_scope *, pangu_core *,
    const pg_service_approval_question *, pg_service_approval_outcome *);
/* Runtime bridge commits asked/decided via Agent approval.h, calling
 * pg_approval_ask inside its answerer. Non-OK NEVER authorizes execution.
 * bridge/context live through wrapper disposal. policy NEVER rejects before
 * bridge dispatch. ASK without bridge is unavailable. reason is copied.
 * Wrapper config is copied on successful installation; reason bytes are owned
 * by wrapper, request context remains borrowed. Names are copied by host.
 * out is NULL on any installation failure, with all partial effects rolled back.
 * The installed scope belongs to the host; caller disposes it (not free).
 * Sources and declarations stay pinned by injected source dependency lease.
 * All tools require approval; wraps execute/start and forwards validation and
 * release with original contexts, including successful NULL source operation
 * handles. Returned wrapper operations are opaque: only wrapper release accepts
 * them; it quiesces source operation before freeing wrapper state. A denied
 * start returns NULL and a final error result, without starting/releasing source.
 * Retained async sinks follow Core start/release/pump contract (loop.h).
 * Keep output lease for complete Core run and all operation releases.
 */
typedef pg_status (*pg_approval_request_fn)(void *, pangu_core *,
    const pg_service_approval_question *, pg_service_approval_outcome *);
typedef struct {
  pg_service_approval_policy policy;
  pangu_slice reason;
  pg_approval_request_fn request;
  void *context;
} pg_approval_tools_config;
pg_status pg_approval_tools_install(pg_scope *, pg_name output, pg_name source,
    const pg_approval_tools_config *, pg_scope **);
#ifdef __cplusplus
}
#endif
#endif
