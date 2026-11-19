#ifndef PANGU_AGENT_SESSION_H
#define PANGU_AGENT_SESSION_H
#include <pangu/core/state.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Version 1. All APIs are serialized on one caller thread. A path has exactly
 * one writable owner; hosts serialize opens across processes. No hidden worker.
 * Slices are length-delimited UTF-8 (paths cannot contain NUL). Inputs are
 * borrowed through return, appended messages/documents are copied. */
typedef struct pangu_session pangu_session;
typedef enum {
  PG_SESSION_OK,
  PG_SESSION_INVALID,
  PG_SESSION_NOMEM,
  PG_SESSION_IO,
  PG_SESSION_NOT_FOUND,
  PG_SESSION_CORRUPT,
  PG_SESSION_READ_ONLY,
  PG_SESSION_BUSY,
  PG_SESSION_CANCELLED,
  PG_SESSION_CORE_ERROR
} pg_session_status;
typedef enum { PG_STORE_JSONL, PG_STORE_SQLITE } pg_session_backend;
typedef struct {
  pangu_slice path;
  pg_session_backend backend;
  int create; /* exclusive creation; missing open never creates a file */
  int read_only;
} pg_session_options;
typedef struct {
  size_t line;
  int native_code;
} pg_session_diagnostic;
typedef enum {
  PG_SESSION_COMMITTED,
  PG_SESSION_LIVE,
  PG_SESSION_CLOSED,
  PG_SESSION_CONTEXT_CHANGED
} pg_session_event_kind;
typedef struct {
  pg_session_event_kind kind;
  uint64_t sequence; /* committed revision, 0 for empty session */
  size_t first_message, message_count;
  const pangu_event
      *live; /* provisional Core event; callback-duration borrow */
} pg_session_event;
/* Listeners run synchronously in registration order after adoption. They may
 * read committed state, unsubscribe, enqueue, or cancel. Mutation/run/close
 * during dispatch return BUSY. Listener contexts live until unsubscribe return
 * (or until their active callback returns). No callback may destroy session. */
typedef void (*pg_session_listener)(void *, pangu_session *,
                                    const pg_session_event *);
pg_session_status pg_session_open(const pg_session_options *, pangu_session **,
                                  pg_session_diagnostic *);
pg_session_status pg_session_subscribe(pangu_session *, pg_session_listener,
                                       void *, uint64_t *);
pg_session_status pg_session_unsubscribe(pangu_session *, uint64_t);
uint64_t pg_session_sequence(const pangu_session *);
/* Borrowed committed snapshots; invalidated by next successful commit/close.
 * Draft state is never returned. Document JSON is an owned encoding in session.
 */
const pangu_core *pg_session_messages(const pangu_session *);
pangu_slice pg_session_documents(const pangu_session *);
/* Atomic explicit transaction: begin clones committed state, append/document
 * modify draft, commit persists then adopts and notifies, rollback drops draft.
 * Failed commit leaves draft available for retry/rollback; no committed event.
 * Documents are a JSON object, latest-only. Close rolls back pending draft. */
pg_session_status pg_session_begin(pangu_session *);
pg_session_status pg_session_append(pangu_session *, const pangu_message *);
pg_session_status pg_session_set_documents(pangu_session *,
                                           pangu_slice json_object);
pg_session_status pg_session_commit(pangu_session *);
pg_session_status pg_session_rollback(pangu_session *);
/* Core driver: config/contexts/tools live through synchronous return. Its event
 * callback retains its original ctx. Final messages persist individually before
 * the corresponding LIVE message_end is delivered. Persistence failure cancels
 * Core and returns the storage error; the durable prefix remains accessible.
 * Core owns in-flight state separately from the committed session snapshot.
 * Optional compaction policy may repeat an interrupted model request once;
 * previously completed tool effects are not replayed. Accepted queues persist;
 * user message placement and removal from the queue commit together.
 * Caller owns config and credentials; neither is serialized. Actual provider
 * response_model/response_id/raw_stop_reason slices are stored verbatim with
 * messages, independently of requested model/provider. Empty fields are omitted
 * from snapshots; old snapshots without these fields load as empty.
 * diagnostics_json/deferred_json/nested_calls_json are copied encoded JSON
 * bytes, preserving []/{}/null members/zero values without normalization.
 * Encoding uses JSON strings, as details_json does; interpretation/shape
 * validation at model protocol boundaries remains with the producer/provider.
 * end_turn is an optional boolean: explicit false keeps has_end_turn=1;
 * missing legacy fields yield has_end_turn=0 and empty metadata slices.
 * Producers redact sensitive metadata before submission. */
pg_session_status pg_session_run(pangu_session *, const pangu_message *, size_t,
                                 const pangu_loop_config *);
/* Explicit original-input provenance. originals is a count-element array
 * corresponding to prompts before resource expansion; roles must match. Only
 * USER TEXT blocks are concatenated, without separators, for input history.
 * Both arrays are copied through synchronous run return. History is attached
 * atomically to the actual committed USER, never to failed/unplaced inputs.
 * Legacy run/append do not assert raw provenance. Enqueue keeps original text
 * pending until placement; queue acceptance alone is not history. Enqueue
 * accepts NULL original to mark legacy/unknown provenance explicitly. A
 * non-NULL original must be USER; both messages are copied before acceptance
 * publication and may be released when enqueue returns. All calls use the
 * session owner thread; run cancellation keeps only actually committed USERs. */
pg_session_status pg_session_run_inputs(pangu_session *, const pangu_message *,
                                        size_t, const pangu_loop_config *,
                                        const pangu_message *originals);
pg_session_status pg_session_enqueue_input(pangu_session *, pangu_queue_kind,
                                            const pangu_message *expanded,
                                            const pangu_message *original);
typedef pg_session_status (*pg_input_history_sink)(void *, pangu_slice);
/* Active branch/projected context, oldest to newest, excluding synthetic
 * compaction/reset messages and pending queues. No raw text for any projected
 * USER means NOT_FOUND with no sink calls (legacy unavailable, not inferred).
 * Slices borrow only through sink return; a copied snapshot pins callbacks.
 * Synchronous owner thread, no model calls/mutations/cancellation operation.
 * Stop and propagate sink errors; no callback is retained. */
pg_session_status pg_session_input_history(const pangu_session *,
                                           pg_input_history_sink, void *);
/* The default session mailbox accepts USER messages (other roles INVALID).
 * During a run, Core copies the input before durable acceptance. Storage
 * failure removes only the new unselected tail through allocation-free Core
 * truncation; the existing queue and durable snapshot remain unchanged.
 * Acceptance persists before OK/publication. Successful enqueue from listeners
 * is allowed; COMMITTED events
 * remain FIFO even for reentrant acceptance. Explicit draft => BUSY.
 * Queue accessors expose committed acceptance, not a provisional Core drain.
 * prepare_next_turn_update and durable queue placement cannot currently mix:
 * run with both returns INVALID; enqueue during that hook-enabled run is BUSY.
 * This avoids ambiguous identical prepared/queued inputs until R06 integration.
 */
/* Idle-only durable mailbox controls; running/dispatch/draft => BUSY. */
pg_session_status pg_session_clear_queue(pangu_session *, pangu_queue_kind);
pg_session_status pg_session_queue_mode(pangu_session *, pangu_queue_kind,
                                        pangu_queue_mode);
size_t pg_session_queue_count(const pangu_session *, pangu_queue_kind);
const pangu_message *pg_session_queue_at(const pangu_session *,
                                         pangu_queue_kind, size_t);
pg_session_status pg_session_enqueue(pangu_session *, pangu_queue_kind,
                                     const pangu_message *);
void pg_session_cancel(
    pangu_session *); /* same-thread cooperative; drops draft when idle */
pg_session_status
pg_session_close(pangu_session *); /* BUSY in run/dispatch; emits CLOSED once */
void pg_session_destroy(
    pangu_session *); /* ignored while busy; closes then releases */
#ifdef __cplusplus
}
#endif
#endif
