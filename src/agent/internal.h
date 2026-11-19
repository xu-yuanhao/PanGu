#ifndef PG_AGENT_INTERNAL_H
#define PG_AGENT_INTERNAL_H
#include <pangu/agent/recovery.h>
#include <pangu/agent/approval.h>
#include <pangu/agent/session.h>
#include <pangu/agent/tree.h>
#include <sqlite3.h>
#include <yyjson.h>
typedef struct {
  uint64_t id;
  pg_session_listener fn;
  void *ctx;
} pg_listener;
typedef struct {
  uint64_t parent, first_kept;
  pg_entry_kind kind;
  pangu_core *value, *input;
} pg_tree_node;
typedef struct {
  pg_tree_node *nodes;
  size_t count;
  uint64_t leaf;
} pg_tree;
typedef struct pg_notification {
  pg_session_event event;
  struct pg_notification *next;
} pg_notification;
struct pangu_session {
  char *path, *documents, *draft_documents, *log;
  char *approval_events, *draft_approval_events;
  uint64_t approval_active;
  int approval_settling, approval_answered;
  pg_approval_outcome approval_result;
  size_t log_len;
  pangu_core *committed, *draft, *running, *compaction_core;
  pangu_core *accepted, *draft_queues, *queue_override;
  pangu_core *raw_queues, *draft_raw_queues, *raw_queue_override;
  pangu_core *input_originals, *input_expected;
  const pangu_message *placed_input;
  int placed_queue;
  size_t input_cursor;
  pg_notification *notifications, *notification_tail;
  int draining, prepared_hook;
  int operation, cancel_requested, recovery_attempted;
  const pg_context_classifier *classifier;
  void *classifier_ctx;
  uint64_t seq, next_listener;
  pg_tree tree, draft_tree;
  pg_compaction_policy policy, draft_policy;
  int context_changed;
  pg_listener *listeners;
  size_t listener_count;
  int readonly, closed, dispatch, running_flag;
  pg_session_backend backend;
  sqlite3 *db;
  pg_session_status run_error;
};
pg_session_status pg_approval_decode(yyjson_val *, char **);
pg_session_status pg_approval_recover(pangu_session *);
pg_session_status pg_input_copy(const pangu_message *, pangu_core **);
pg_session_status pg_input_attach(pangu_session *, const pangu_message *);
pg_session_status pg_raw_queues_restore(yyjson_val *, const pangu_core *, pangu_core **);
pg_session_status pg_session_omit_attempt(pangu_session *, uint64_t);
pg_session_status pg_session_compact_internal(pangu_session *, size_t,
                                              const pangu_loop_config *);
pg_session_status pg_session_compact_summary_internal(pangu_session *, uint64_t,
                                                      pangu_slice,
                                                      const pangu_usage *,
                                                      pangu_slice details);
pg_session_status pg_compaction_file_metadata(const pg_tree *,
                                              const pangu_core *,
                                              const uint64_t *, size_t, char **,
                                              size_t *, char **, size_t *);
pg_session_status pg_queues_place(pangu_session *, const pangu_message *);
const pangu_core *pg_queue_source(const pangu_session *);
pg_session_status pg_queues_clone(const pangu_core *, pangu_core **);
yyjson_mut_val *pg_queues_encode(yyjson_mut_doc *, const pangu_core *);
pg_session_status pg_queues_decode(yyjson_val *, pangu_core **);
void pg_tree_clear(pg_tree *);
pg_session_status pg_tree_clone(const pg_tree *, pg_tree *);
pg_session_status pg_tree_append(pg_tree *, pg_entry_kind,
                                 const pangu_message *, uint64_t);
yyjson_mut_val *pg_tree_encode(yyjson_mut_doc *, const pg_tree *);
pg_session_status pg_tree_decode(yyjson_val *, const pangu_core *, pg_tree *);
pg_session_status pg_tree_project(const pg_tree *, pangu_core **, uint64_t **);
pg_session_status pg_session_append_draft(pangu_session *,
                                          const pangu_message *);
pg_session_status pg_session_commit_internal(pangu_session *);
pg_session_status pg_session_begin_internal(pangu_session *);
void pg_session_rollback_internal(pangu_session *);
yyjson_mut_val *pg_encode_message(yyjson_mut_doc *, const pangu_message *);
pg_session_status pg_decode_message(yyjson_val *, pangu_core *);
pg_session_status pg_store_open(pangu_session *, int, pg_session_diagnostic *);
pg_session_status pg_store_write(pangu_session *, const char *, size_t);
#endif
