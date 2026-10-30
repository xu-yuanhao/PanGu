#ifndef PANGU_CLIENT_H
#define PANGU_CLIENT_H
#include <stddef.h>
#include <stdint.h>
/* Internal source contract v0: borrowed event bytes expire at next poll. */
typedef struct { const char *data; size_t size; } pg_slice;
typedef enum { PG_RUN_STARTED, PG_TEXT_DELTA, PG_TOOL_STARTED, PG_TOOL_FINISHED, PG_RUN_FINISHED, PG_TOOL_OUTPUT_DELTA, PG_USAGE_CHANGED, PG_COMPACTION_STARTED, PG_COMPACTION_FINISHED, PG_RETRY_STARTED, PG_RETRY_FINISHED, PG_DEFERRED_POLL, PG_TASK_FAILED, PG_MESSAGE_STARTED, PG_MESSAGE_FINISHED, PG_ENTRY_APPENDED, PG_SUBMISSION_CHANGED, PG_TURN_STARTED, PG_TURN_FINISHED, PG_INBOX_UPDATED } pg_event_kind;
typedef enum { PG_SUCCESS, PG_ERROR, PG_CANCELLED } pg_outcome;
typedef struct {
    pg_event_kind kind;
    pg_outcome outcome;
    /* COMPACTION_STARTED/FINISHED text contains JSON taskId/reason/blocking;
       a failed finish includes error. USAGE_CHANGED text is a JSON ledger.
       RETRY_STARTED: {attempt,at,errorMessage}; RETRY_FINISHED: {attempt};
       DEFERRED_POLL: {pollAt}; TASK_FAILED: {taskId,kind,message}.
       TURN_STARTED/FINISHED identify the committed Generation task.
       SUBMISSION_CHANGED contains the committed PanGu Submission record;
       INBOX_UPDATED contains the full ascending array of queued records.
       MESSAGE_STARTED contains the first model message; MESSAGE_FINISHED and
       ENTRY_APPENDED contain the committed Entry. Text deltas remain raw UTF-8.
       Payloads are copied into the queue and borrowed until next poll.
       TOOL_STARTED text is the canonical JSON arguments; TOOL_FINISHED text
       is output. Slices borrow the event until next poll. */
    pg_slice text, call_id, tool_name;
} pg_event;
typedef struct {
    void *context;
    int (*submit)(void *, pg_slice); /* 0 accepted; nonzero leaves state unchanged */
    void (*cancel)(void *);
    int (*poll)(void *, pg_event *); /* 1 event, 0 no event; bounded work */
} pg_client;
struct json_object;
/* Foreground conversation binding. select is serialized and requires an idle
   client: 0 selected, 2 missing, 3 busy, 4 invalid, 5 preparation failed,
   6 storage error, 7 closed. Failure retains the old binding/observations.
   Success invalidates borrowed events; history returns owned flat display rows.
   Switching does not close the shared scheduler or abort other conversations.
   current returns 0 after close. Context stays borrowed through client lifetime. */
typedef struct {void *context;int (*select)(void *,uint64_t);uint64_t (*current)(void *);
 int (*history)(void *,struct json_object **);void (*release)(struct json_object *);
 /* parent/at both zero creates an independent conversation; both nonzero forks.
    agent is an optional borrowed change; output owns the committed record.
    The foreground selection stays unchanged. Same status family as select. */
 int (*create)(void *,uint64_t parent,uint64_t at,const struct json_object *agent,struct json_object **);} pg_conversation_client_ops;
/* Zero owner IDs omit filters. order/cursor_order: 0 default, 1 ascending,
   2 descending. Round-trip the returned cursor's after/order without changing
   its scan order. A NULL query uses an ascending page of 64 records. */
typedef struct {
 uint64_t owner_conversation,owner_task,after;size_t limit;
 unsigned order,has_cursor,cursor_order,reserved;
} pg_conversation_query;
/* Serialized reads return detached records with an optional display name.
   list owns {items,cursor}, cursor is null or {after,order}; read owns a record.
   rename appends metadata without changing model context or selection and
   accepts explicit-length UTF-8; an empty sanitized name clears the title.
   Statuses follow the foreground selection family. Context is borrowed. */
typedef struct {
 void *context;
 int (*list)(void *,const pg_conversation_query *,struct json_object **);
 int (*read)(void *,uint64_t,struct json_object **);
 int (*rename)(void *,uint64_t,pg_slice);
 void (*release)(struct json_object *);
} pg_conversation_catalog_ops;
/* Fork-aware Entry range, inclusive bounds; zero bounds are omitted. Cursor
   uses the same after/order round-trip as the catalog. Default newest first. */
typedef struct {uint64_t min_entry,max_entry,after;size_t limit;unsigned order,has_cursor,cursor_order,reserved;} pg_conversation_entry_query;
typedef struct {
 void *context;
 int (*entries)(void *,uint64_t,const pg_conversation_entry_query *,struct json_object **);
 void (*release)(struct json_object *);
} pg_conversation_history_ops;
/* Reset the selected conversation through durable Submission admission.
   NULL handoff starts an empty context; a present slice adds that user message,
   including an empty message. Owns no borrowed text after return. id receives
   the durable Submission ID; inspect it through the existing inputs service. */
typedef struct {void *context;int (*reset)(void *,const pg_slice *handoff,uint64_t *id);} pg_conversation_actions_ops;
/* start copies instructions; reserve_tokens is 2..1000000. Serialized binding
   remains borrowed through poll/release and the owning runtime lifetime. */
typedef struct {
 uint64_t conversation,keep_recent_tokens,reserve_tokens;
 const char *instructions;
} pg_compaction_request;
typedef struct {
 void *context;
 int (*start)(void *,const pg_compaction_request *,uint64_t *);
 /* Pump one scheduler step; 0 owns a compaction Task snapshot, 1 missing. */
 int (*poll)(void *,uint64_t,struct json_object **);
 void (*release)(struct json_object *);
} pg_compaction_ops;

/* Serialized, non-pumping Submission operations. submit borrows a typed
   input/write draft and returns its stable deduplicated ID. status returns an
   owned record; list returns an owned ascending conversation-scoped array,
   limit 1..128, after=0 starts and later pages use the last ID. release once.
   Status: 0 success, 2 missing, 3 busy, 4 invalid, 5 allocation, 6 storage,
   7 closed. withdraw result: 0 withdrawn, 1 placed, 2 settled, 3 missing. */
typedef struct {
    void *context;
    int (*submit)(void *,const struct json_object *draft,uint64_t *id);
    int (*status)(void *,uint64_t id,struct json_object **);
    int (*withdraw)(void *,uint64_t id,unsigned *result);
    int (*list)(void *,uint64_t after,size_t limit,struct json_object **);
    void (*release)(struct json_object *);
} pg_inputs_ops;
/* Optional serialized resource service. reload: 0 adopted, 3 busy, 5 failed,
   7 closed. snapshot returns detached owned catalogs/diagnostics and revision;
   settings values and credentials are omitted. release once. expand processes
   foreground template/skill text; buffers are caller-owned and disjoint. */
typedef struct {
    void *context;
    int (*reload)(void *);
    int (*snapshot)(void *,struct json_object **);
    void (*release)(struct json_object *);
    int (*expand)(void *,pg_slice,char *,size_t,size_t *);
} pg_resource_ops;
#endif
