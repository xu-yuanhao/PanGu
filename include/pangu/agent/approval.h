#ifndef PANGU_AGENT_APPROVAL_H
#define PANGU_AGENT_APPROVAL_H
#include <pangu/agent/session.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Harness user-approval closed outcomes: only ALLOWED_ONCE grants the exact
 * requesting action. No persisted outcome authorizes a subsequent invocation. */
typedef enum {
  PG_APPROVAL_ALLOWED_ONCE,
  PG_APPROVAL_REJECTED,
  PG_APPROVAL_CANCELLED,
  PG_APPROVAL_UNAVAILABLE
} pg_approval_outcome;
typedef struct {
  pangu_slice tool_name, call_id, reason;
} pg_approval_question;
typedef struct {
  uint64_t id;
  int decided;
  pg_approval_question question; /* asked only; no tool arguments persisted */
  pg_approval_outcome outcome;   /* decided only */
} pg_approval_event;
/* Same run owner thread. Question slices are copied before asked publication;
 * answerer borrows them only through return. It may call reply directly or
 * pump an interactive owner-thread UI that calls reply/cancel. Returning a
 * non-OK status or an out-of-vocabulary result fails closed to UNAVAILABLE.
 * No answerer/context is retained. Missing answerer also fails closed. */
typedef pg_session_status (*pg_approval_answerer)(void *, pangu_session *,
    uint64_t, const pg_approval_question *, pg_approval_outcome *);
/* Only inside an active default-driver run, outside commit dispatch/preparation
 * and explicit drafts. Asked and decided each commit before publication; audit
 * is never included in model transcript, input history, or caller documents.
 * out is written only after the decision commits. Storage/allocation failures
 * return errors: consumers MUST NOT execute on error, even if out held a grant.
 * Cancel wins over an unanswered question; already committed answers are final.
 */
pg_session_status pg_session_approval_request(pangu_session *,
    const pg_approval_question *, pg_approval_answerer, void *,
    pg_approval_outcome *out);
/* First committed answer wins. Only the currently live request can be answered;
 * duplicates/late/reopened IDs return NOT_FOUND. May be called by a COMMITTED
 * listener after asked adoption. No reply callbacks or pointers are retained. */
pg_session_status pg_session_approval_reply(pangu_session *, uint64_t,
                                           pg_approval_outcome);
/* 0 means no live request; pending state is not a grant. Crash-left asked audit
 * is closed as CANCELLED in one durable transaction on writable reopen before
 * returning the session; read-only replay preserves the incomplete audit and
 * has no live/answerable request. No tool is resumed or replayed on reopen. */
uint64_t pg_session_approval_pending(const pangu_session *);
typedef pg_session_status (*pg_approval_event_sink)(void *, const pg_approval_event *);
/* Full session audit, oldest first, independent of branch/compaction/reset.
 * Snapshot copies pin all slices through each callback. Sink error stops and
 * propagates without mutation. Owner thread only; no model or tool dispatch. */
pg_session_status pg_session_approval_events(const pangu_session *,
                                            pg_approval_event_sink, void *);
#ifdef __cplusplus
}
#endif
#endif
