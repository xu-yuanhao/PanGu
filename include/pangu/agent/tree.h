#ifndef PANGU_AGENT_TREE_H
#define PANGU_AGENT_TREE_H
#include <pangu/agent/session.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Append-only session tree, Pi SessionManager branch semantics. IDs are local
 * monotonic integers (0 means before the root). A switch moves the active leaf;
 * the next append creates a child without deleting the abandoned branch.
 * All calls use the session owner thread. Mutation requires idle, writable,
 * no outstanding explicit transaction. Data/views are borrowed through the next
 * successful commit/close. Tree changes persist before CONTEXT_CHANGED events.
 * OMISSION is an append-only recovery context edit: first_kept identifies an
 * earlier assistant to omit with its paired tool results. Original entries and
 * usage remain archived; message is NULL. New readers accept old snapshots;
 * readers predating OMISSION cannot open snapshots containing that entry.
 */
typedef enum {
  PG_ENTRY_MESSAGE,
  PG_ENTRY_RESET,
  PG_ENTRY_COMPACTION,
  PG_ENTRY_OMISSION /* first_kept is the omitted assistant ID; message NULL */
} pg_entry_kind;
typedef struct {
  uint64_t id, parent, first_kept;
  pg_entry_kind kind;
  const pangu_message *message; /* NULL for a plain reset */
} pg_session_entry;
size_t pg_session_entry_count(const pangu_session *);
pg_session_status pg_session_entry_at(const pangu_session *, size_t,
                                      pg_session_entry *);
uint64_t pg_session_leaf(const pangu_session *);
pg_session_status pg_session_switch(pangu_session *, uint64_t entry_id);
/* Reset preserves the replayed system/tool baseline. An optional handoff user
 * message follows it. Previous history remains navigable in the tree. */
pg_session_status pg_session_reset(pangu_session *, pangu_slice handoff);
/* Commit a host/hook-provided summary. first_kept must be a visible user or
 * assistant message on this branch, with an older conversational prefix.
 * Tool results cannot be detached from their preceding calls. usage may be
 * NULL. Summary text is copied/wrapped using Durable's <summary> prefix/suffix.
 */
pg_session_status pg_session_compact_summary(pangu_session *,
                                             uint64_t first_kept,
                                             pangu_slice summary,
                                             const pangu_usage *usage);
/* Synchronous model summarization with source prompt, no executable tools and
 * no transcript/live message side effects. cfg/contexts borrowed through
 * return. Clean STOP text only is accepted. Error, tool-call, length and
 * cancellation do not change the tree. Cancellation is same-thread via
 * pg_session_cancel. The summary model's usage is stored with the compaction
 * entry. Model-generated summaries append Pi <read-files>/<modified-files>
 * suffixes and store details_json={"readFiles":[...],"modifiedFiles":[...]}.
 * Lists use exact call paths, de-duplicated and sorted by UTF-16 code units;
 * modified (write/edit) wins over read, even for failed tool results. Only
 * compacted model-visible messages plus the previous generated details count;
 * kept/inactive/omitted attempts do not contribute new operations. Native
 * producers may put source toolResult.nestedCalls in details_json as
 * {"nestedCalls":{"calls":[{"name":"read","arguments":{"path":"..."}}]}}.
 * Incomplete argumentsBytes records are ignored; no nested executor is implied.
 * Host-supplied compact_summary has no generated details, matching source
 * hooks. All details/XML bytes are session-owned through the next commit/close.
 */
pg_session_status pg_session_compact(pangu_session *, size_t keep_recent_tokens,
                                     const pangu_loop_config *cfg);
typedef struct {
  int enabled;
  size_t context_tokens, reserve_tokens, keep_recent_tokens;
} pg_compaction_policy;
/* Policy is persisted without credentials. Automatic compaction runs before a
 * new session run at context_tokens-reserve_tokens. Zero/default is disabled.
 * After a model attempt, usage overflow on STOP compacts without replaying the
 * successful reply; ERROR overflow (optional classifier) or recoverable LENGTH
 * persistently omits the failed attempt, compacts and continues once. Original
 * model metadata.maxTokens supplies desired output before provider clamping.
 * Without a classifier only numeric usage/LENGTH cases are supported; no error
 * strings are inspected. An unchanged cut never causes uncompressed retry.
 * A second failed attempt is kept, not compacted/retried again; budget resets
 * on accepted new USER or a successful assistant, matching the source.
 * Cancellation stops all later phases and retains the durable prefix, including
 * an already committed omission or compaction. Failed storage adopts nothing
 * from that transaction and prevents the next request. Failed/cancelled summary
 * changes no compaction state; a prior omission remains committed. Estimates
 * use usage-after-compaction rules and UTF-16 / 3.5 via utf8proc. */
pg_compaction_policy pg_session_compaction_policy(const pangu_session *);
pg_session_status
pg_session_set_compaction_policy(pangu_session *, const pg_compaction_policy *);
size_t pg_session_context_tokens(const pangu_session *);
/* Complete archived usage, including inactive branches and summarization.
 * Each immutable message/summary is counted once; context replay adds nothing.
 */
pangu_usage pg_session_usage(const pangu_session *);
#ifdef __cplusplus
}
#endif
#endif
