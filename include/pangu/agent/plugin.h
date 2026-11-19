#ifndef PANGU_AGENT_PLUGIN_H
#define PANGU_AGENT_PLUGIN_H
#include <pangu/agent/session.h>
#include <pangu/agent/approval.h>
#include <pangu/agent/tree.h>
#include <pangu/services/host.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_SESSION_SERVICE "pangu.session"
#define PG_SESSION_ABI 1u
/* Context is a borrowed pangu_session, held alive by the service lease. Release
 * lease after run returns, before disposing plugin scope. The function table
 * has process lifetime. Session close is scope-owned; consumers only cancel. */
typedef struct {
  uint32_t abi;
  size_t size;
  pg_session_status (*run)(pangu_session *, const pangu_message *, size_t,
                           const pangu_loop_config *);
  pg_session_status (*enqueue)(pangu_session *, pangu_queue_kind,
                               const pangu_message *);
  void (*cancel)(pangu_session *);
  pg_session_status (*subscribe)(pangu_session *, pg_session_listener, void *,
                                 uint64_t *);
  pg_session_status (*unsubscribe)(pangu_session *, uint64_t);
  const pangu_core *(*messages)(const pangu_session *);
  pangu_slice (*documents)(const pangu_session *);
  /* Appended capability slots; consumers check size before using them. */
  size_t (*entry_count)(const pangu_session *);
  pg_session_status (*entry_at)(const pangu_session *, size_t,
                                pg_session_entry *);
  uint64_t (*leaf)(const pangu_session *);
  pg_session_status (*switch_to)(pangu_session *, uint64_t);
  pg_session_status (*reset)(pangu_session *, pangu_slice);
  pg_session_status (*compact)(pangu_session *, size_t,
                               const pangu_loop_config *);
  pangu_usage (*usage)(const pangu_session *);
  /* Optional original-input provenance tail; check size before each slot.
   * Old drivers keep existing run/enqueue behavior and history unavailable. */
  pg_session_status (*run_inputs)(pangu_session *, const pangu_message *, size_t,
                                  const pangu_loop_config *, const pangu_message *);
  pg_session_status (*enqueue_input)(pangu_session *, pangu_queue_kind,
                                      const pangu_message *, const pangu_message *);
  pg_session_status (*input_history)(const pangu_session *, pg_input_history_sink, void *);
  /* Optional approval lifecycle tail. Check size and pointer before use.
   * Missing capability never grants permission. See approval.h for ownership. */
  pg_session_status (*approval_request)(pangu_session *, const pg_approval_question *,
      pg_approval_answerer, void *, pg_approval_outcome *);
  pg_session_status (*approval_reply)(pangu_session *, uint64_t, pg_approval_outcome);
  uint64_t (*approval_pending)(const pangu_session *);
  pg_session_status (*approval_events)(const pangu_session *, pg_approval_event_sink, void *);
} pg_session_service;
/* Config is pg_session_options, copied/consumed synchronously by installation.
 * This default plugin owns Core driver + selected storage and the session. */
const pg_plugin *pg_agent_core_plugin(void);
#ifdef __cplusplus
}
#endif
#endif
