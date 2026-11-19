#include <pangu/agent/plugin.h>
#include <pangu/agent/recovery.h>
#ifdef PANGU_AGENT_OVERFLOW
#include <pangu/core/overflow.h>
static void overflow_cleanup(void *ctx) { pangu_overflow_destroy(ctx); }
static pangu_status overflow_classify(void *ctx, const pangu_message *message,
                                      uint64_t capacity, int *out) {
  return pangu_message_is_context_overflow(ctx, message, capacity, out);
}
static int overflow_length(void *ctx, const pangu_message *message,
                           uint64_t desired) {
  (void)ctx;
  return pangu_message_is_recoverable_length(message, desired);
}
static const pg_context_classifier classifier = {
    sizeof(pg_context_classifier), overflow_classify, overflow_length};
#endif
static void cleanup(void *s) { pg_session_destroy(s); }
static const pg_session_service table = {.abi = PG_SESSION_ABI,
                                         .size = sizeof(pg_session_service),
                                         .run = pg_session_run,
                                         .enqueue = pg_session_enqueue,
                                         .cancel = pg_session_cancel,
                                         .subscribe = pg_session_subscribe,
                                         .unsubscribe = pg_session_unsubscribe,
                                         .messages = pg_session_messages,
                                         .documents = pg_session_documents,
                                         .entry_count = pg_session_entry_count,
                                         .entry_at = pg_session_entry_at,
                                         .leaf = pg_session_leaf,
                                         .switch_to = pg_session_switch,
                                         .reset = pg_session_reset,
                                         .compact = pg_session_compact,
                                         .usage = pg_session_usage,
                                         .run_inputs = pg_session_run_inputs,
                                         .enqueue_input = pg_session_enqueue_input,
                                         .input_history = pg_session_input_history,
                                         .approval_request = pg_session_approval_request,
                                         .approval_reply = pg_session_approval_reply,
                                         .approval_pending = pg_session_approval_pending,
                                         .approval_events = pg_session_approval_events};
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  if (!config || size != sizeof(pg_session_options))
    return PG_INVALID;
#ifdef PANGU_AGENT_OVERFLOW
  /* Default capability is installed by the execution plugin, not the host.
   * A composition-provided classifier remains authoritative. */
  pg_lease *existing = NULL;
  pg_status found =
      pg_service_acquire(scope, PG_NAME(PG_CONTEXT_CLASSIFIER_SERVICE),
                         PG_CONTEXT_CLASSIFIER_ABI, &existing);
  if (found == PG_OK)
    pg_lease_release(existing);
  else if (found == PG_NOT_FOUND) {
    pangu_overflow_classifier *owned = NULL;
    pangu_status created = pangu_overflow_create(NULL, &owned);
    if (created != PANGU_OK)
      return created == PANGU_NOMEM ? PG_NOMEM : PG_FAILED;
    pg_status registered = pg_scope_defer(scope, overflow_cleanup, owned);
    if (registered != PG_OK) {
      pangu_overflow_destroy(owned);
      return registered;
    }
    registered =
        pg_service_register(scope, PG_NAME(PG_CONTEXT_CLASSIFIER_SERVICE),
                            PG_CONTEXT_CLASSIFIER_ABI, &classifier, owned);
    if (registered != PG_OK)
      return registered;
  } else
    return found;
#endif
  pangu_session *s = NULL;
  pg_session_status z = pg_session_open(config, &s, NULL);
  if (z != PG_SESSION_OK)
    return z == PG_SESSION_NOMEM ? PG_NOMEM : PG_FAILED;
  pg_status rc = pg_scope_defer(scope, cleanup, s);
  if (rc != PG_OK) {
    pg_session_destroy(s);
    return rc;
  }
  return pg_service_register(scope, PG_NAME(PG_SESSION_SERVICE), PG_SESSION_ABI,
                             &table, s);
}
const pg_plugin *pg_agent_core_plugin(void) {
  static const pg_plugin plugin = {
      PANGU_SERVICES_ABI,
      sizeof(pg_plugin),
      {"pangu.agent.core", sizeof("pangu.agent.core") - 1},
      NULL,
      0,
      apply};
  return &plugin;
}
