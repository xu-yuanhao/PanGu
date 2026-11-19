#ifndef PANGU_TUI_PLUGIN_H
#define PANGU_TUI_PLUGIN_H
#include <pangu/agent/plugin.h>
#include <pangu/agent/approval.h>
#include <pangu/tui/view.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_TUI_SERVICE "pangu.tui"
#define PG_TUI_ABI 1u
/* Install after pangu.session; dependency lease pins session through UI
 * cleanup. loop is copied, its borrowed contexts/tools/slices must outlive
 * scope. The UI owns terminal/chat and subscribes to session LIVE events. All
 * table calls run on host/run caller thread. poll must ALSO be invoked from
 * provider progress on that same thread during network waits; it
 * cancels/enqueues while running. stop joins input and restores terminal;
 * dispose unsubscribes then destroys. No table calls or retained pointers after
 * lease release/scope disposal. */
/* Optional execution owner supplied by app_present. All calls are synchronous
 * on the presentation/run owner thread, including reentrant provider polls.
 * Message/loop inputs are borrowed through return; enqueue must copy/commit
 * accepted input before returning. The driver owns resource expansion, request
 * snapshots, persistence and cancellation semantics; UI passes raw input once.
 * run/enqueue must either both be set or both be NULL. Missing slots preserve
 * standalone session behavior; missing reload reports unavailable. compact
 * must track its request lifetime just like run. Calls may poll this UI while
 * run/compact is in flight; reload is only invoked while UI is idle.
 * size counts valid bytes (at least through enqueue). Table is copied at
 * install; callback code and actions_ctx must live through scope disposal.
 * Return errors must not expose credentials in diagnostics.
 */
typedef struct {
  size_t size;
  pg_session_status (*run)(void *, const pangu_message *, size_t,
                            const pangu_loop_config *);
  pg_session_status (*enqueue)(void *, pangu_queue_kind, const pangu_message *);
  pg_status (*reload_resources)(void *);
  pg_session_status (*compact)(void *, size_t, const pangu_loop_config *);
  /* Optional size-checked tail. Startup-only raw committed input source using
   * actions_ctx. See chat.h: ordered original USER text, never expanded prompt
   * bodies. UNSUPPORTED leaves current in-memory history and starts normally;
   * other failures abort startup before terminal entry. Missing callback does
   * not infer raw provenance from session.messages/current resource settings.
   * The driver must preserve provenance across save/open/backend changes. */
  pg_tui_history_source input_history;
} pg_tui_actions;

typedef struct {
  pangu_loop_config loop;
  pg_tui_complete_fn complete;
  void *complete_ctx;
  /* Optional tail. A config ending immediately before actions remains valid.
   * A new config includes both fields; NULL actions uses standalone session.
   * App may provide its own driver to retain app lifecycle/lease ownership. */
  const pg_tui_actions *actions;
  void *actions_ctx;
} pg_tui_config;
typedef struct {
  uint32_t abi;
  size_t size;
  pangu_tui_status (*start)(void *);
  pangu_tui_status (*run)(void *);
  void (*poll)(void *);
  void (*stop)(void *);
  pangu_tui_chat *(*chat)(void *); /* borrowed through service lease */
} pg_tui_service;
/* Pi getUserMessageText: concatenate TEXT blocks of committed USER messages
 * in snapshot order, ignoring all other roles/content. The HOST must certify
 * these messages contain original input, e.g. a direct-session-only store;
 * a generic app/model-visible snapshot is NOT evidence of raw provenance.
 * Owner thread only, no concurrent snapshot mutation. Data is borrowed through
 * return; sink receives temporary concatenated bytes and must copy if needed.
 * Stops on sink/allocation error, never invokes a model or changes session.
 */
pangu_tui_status pg_tui_history_user_messages(const pangu_core *,
                                              pg_tui_history_sink, void *);
/* Optional synchronous approval answerer for a composed tool consumer.
 * ctx is a pg_scope* whose pangu.tui service belongs to the requesting session.
 * Caller must route only that session and keep scope alive through return.
 * Acquires/releases its own presentation lease, pumps owner-thread poll until
 * the live question is durably replied/cancelled. Absent/stopped UI returns
 * UNAVAILABLE (no grant); quit cancels. No callback/context is retained. Current
 * runtime request observes its own first committed reply, so out is never used
 * to synthesize a grant here. Service-owner registration/lifecycle is separate.
 */
pg_session_status pg_tui_approval_answer(void *, pangu_session *, uint64_t,
                                       const pg_approval_question *,
                                       pg_approval_outcome *);
/* Default presentation also registers pangu.approval ABI1 in its own scope.
 * The synchronous owner-thread answerer borrows question/core only through
 * return, displays committed toolName/callId/reason, and never retains grants.
 * stop revokes the provider before terminal/session cleanup; callers must let
 * an active action/answer return (quiesce) before disposing the scope.
 * Deployment opts in before present with pg_agent_app_set_approval(ASK/NEVER);
 * installing presentation alone does not change app policy. No UI input or
 * approval audit is submitted as USER text. */
const pg_plugin *pg_tui_plugin(void);
#ifdef __cplusplus
}
#endif
#endif
