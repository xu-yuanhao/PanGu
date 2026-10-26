#ifndef PANGU_RECORDS_H
#define PANGU_RECORDS_H
#include <stdint.h>
#include <stddef.h>
struct json_object;
typedef void (*pg_records_adopt)(void *,uint64_t);
typedef struct {
 void *context;
 /* Atomically persist the entire detached StorageWrite JSON array, then call
    adopt exactly once before return. Return 0 and the adopted sequence on
    success. All pointers/callbacks expire on return. Nonzero fails Session. */
 int (*commit)(void *,const struct json_object *,pg_records_adopt,void *,uint64_t *);
} pg_record_storage;
typedef struct {
 pg_record_storage storage;
 uint64_t sequence;
 void *context;
 /* Adoption must use already prepared state; nonzero latches failure without
    escaping through Storage. Publication runs after successful settlement. */
 int (*adopt)(void *,const struct json_object *,uint64_t);
 void (*publish)(void *,const struct json_object *,uint64_t);
 unsigned busy,failed,read_only,adopt_count,adopt_error;
 uint64_t pending_sequence;
 const struct json_object *pending_batch;
} pg_record_session;
typedef enum { PG_RECORDS_OK,PG_RECORDS_INVALID,PG_RECORDS_READ_ONLY,
 PG_RECORDS_STORAGE_FAILED,PG_RECORDS_ADOPTION_FAILED,PG_RECORDS_ALLOCATION } pg_records_status;
/* Caller owns zeroed Session with explicit storage and optional consumer hooks.
   Calls are serialized. Batch is an immutable borrowed StorageWrite JSON array;
   the backend validates table-specific constraints. Input is detached before
   admission. Empty batches are healthy no-ops, including in read-only mode.
   Failure before admission leaves Session healthy. Admitted backend/adoption
   failure latches Session; adopted state is never rolled back. */
pg_records_status pg_records_commit(pg_record_session *,const struct json_object *);
/* Durable transaction submission transition over borrowed typed JSON records.
   Returns a detached owned candidate; terminal records retain their value.
   Placement accepts queued records; done accepts placed inputs; unanswered
   accepts queued/placed. Failure clears output and leaves inputs unchanged. */
pg_records_status pg_records_submission_change(const struct json_object *,const struct json_object *,struct json_object **);
typedef enum { PG_TX_OK,PG_TX_INVALID,PG_TX_READ_AFTER_WRITE,PG_TX_SEALED,
 PG_TX_ALLOCATION,PG_TX_READ_FAILED,PG_TX_COMMIT_FAILED,PG_TX_MISSING,PG_TX_CREATION_FAILED } pg_record_tx_status;
typedef enum {PG_TX_CONVERSATION,PG_TX_ENTRY,PG_TX_TASK,PG_TX_SUBMISSION,
 PG_TX_DOCUMENT_CREATE,PG_TX_DOCUMENT_COPY,PG_TX_DOCUMENT_CHANGE,PG_TX_DOCUMENT_RETIRE} pg_record_tx_write;
typedef struct {
 pg_record_session *session;
 void *read_context;
 /* Committed table read: 0 owned result, 1 missing, other values error. */
 int (*read)(void *,unsigned,uint64_t,struct json_object **);
 struct json_object *writes;
 unsigned busy,sealed,has_table_write,creation_depth;
} pg_record_tx;
/* Caller provides zeroed or discarded Tx and serializes the entire mutation
   line on its Session. Callbacks cannot reenter this Tx. Begin borrows Session
   and read service; discard releases private candidates and is idempotent.
   stage accepts a final StorageWrite of the declared kind and deep-copies it.
   A table-write attempt forbids subsequent public table reads, including if
   staging fails. Document writes do not set the table-write barrier.
   Commit seals before admission, submits through pg_records_commit and consumes
   candidates on every result. Copy/source conflicts fail before Storage admission
   and leave Session healthy. Final owner validation also rejects newly owned
   work on terminal/completing/abort-marked tasks; Task creation is identified
   by a fresh (not committed) ID. Inspect Session for admitted backend errors.
   Typed creation, document tracking and async operation drainage layer on this
   synchronous batch owner; these calls retain no asynchronous operation. */
pg_record_tx_status pg_record_tx_begin(pg_record_tx *,pg_record_session *,void *,int (*)(void *,unsigned,uint64_t,struct json_object **));
pg_record_tx_status pg_record_tx_read(pg_record_tx *,unsigned,uint64_t,struct json_object **);
pg_record_tx_status pg_record_tx_stage(pg_record_tx *,pg_record_tx_write,const struct json_object *);
pg_record_tx_status pg_record_tx_commit(pg_record_tx *);
pg_record_tx_status pg_record_tx_discard(pg_record_tx *);
/* Resolve against the newest staged submission, then committed Storage;
   append the transformed candidate in staging order. Internal committed lookup
   is allowed after the public table-read barrier. */
pg_record_tx_status pg_record_tx_submission_change(pg_record_tx *,uint64_t id,const struct json_object *);
/* Require a staged or committed Conversation, mint an Entry ID through the
   borrowed Storage allocator, detach draft and override id/conversationId and
   byTaskId (0 omits it). head="self" becomes the new ID. Success owns a result
   independent of the staged record. Minted IDs remain consumed on later error.
   mint uses read_context, returns 0 and a positive safe ID on success. */
pg_record_tx_status pg_record_tx_append_entry(pg_record_tx *,uint64_t conversation_id,const struct json_object *draft,
 uint64_t by_task_id,int (*mint)(void *,uint64_t *),struct json_object **);
/* Construct an ownerless (owner_task_id=0) or task-owned Conversation.
   root=1 reserves ID 1 and requires ownerless ownership; otherwise mint uses
   read_context before task lookup. Latest staged task wins over Storage.
   Success returns a detached record independent of the staged candidate.
   This primitive does not perform fork selection or extension creation hooks. */
pg_record_tx_status pg_record_tx_create_conversation(pg_record_tx *,uint64_t owner_task_id,unsigned root,
 int (*mint)(void *,uint64_t *),struct json_object **);
typedef struct {uint64_t conversation_id,at;int64_t after;unsigned has_cursor,reserved;} pg_fork_query;
typedef struct {struct json_object *items;int64_t after;unsigned has_next,reserved;} pg_fork_page;
typedef struct {
 void *context;
 /* Entry lookup is fork-aware; both read callbacks return owned snapshots.
    0 means success. Scan uses ascending IDs and page size 256. */
 int (*entry)(void *,uint64_t conversation,uint64_t id,struct json_object **,uint64_t *sequence);
 int (*scan)(void *,const pg_fork_query *,pg_fork_page *);
 int (*mint)(void *,uint64_t *);
} pg_fork_storage;
/* Ordered {record,source} copy plans, owned and detached. Select asOf documents
   from the visible Entry's owning conversation at its actual commit sequence,
   then current-policy documents from the requested parent. Duplicate target
   logical addresses fail. IDs minted before failure remain consumed. Typed
   DocumentRecord snapshots use only the fields declared by Durable. */
pg_record_tx_status pg_records_prepare_fork(const pg_fork_storage *,uint64_t parent,uint64_t entry,uint64_t child,struct json_object **);
/* Stage a child Conversation parent plus all selected document copies as one
   private candidate batch. On error prior candidates remain unchanged, while
   allocated IDs remain consumed. owner_task_id follows create semantics.
   The fork Storage may use a different context than Tx committed table reads. */
pg_record_tx_status pg_record_tx_fork_conversation(pg_record_tx *,uint64_t parent,uint64_t entry,uint64_t owner_task_id,
 const pg_fork_storage *,struct json_object **);
typedef enum { PG_SCAN_DEFAULT,PG_SCAN_ASCENDING,PG_SCAN_DESCENDING } pg_scan_order;
enum { PG_SCAN_CONVERSATION=1,PG_SCAN_OWNER_CONVERSATION=2,PG_SCAN_OWNER_TASK=4,
 PG_SCAN_MIN_ENTRY=8,PG_SCAN_MAX_ENTRY=16,PG_SCAN_KIND=32,PG_SCAN_STATUS=64,
 PG_SCAN_ABORT_REQUESTED=128,PG_SCAN_BACKGROUND=256,PG_SCAN_HEAD_ONLY=512 };
typedef struct {
 unsigned table,order,has_cursor,cursor_order;
 int64_t after;
 size_t limit;
 uint64_t flags,conversation_id,owner_conversation_id,owner_task_id,min_entry_id,max_entry_id;
 struct {const char *data;size_t size;} kind;
 unsigned status,abort_requested,background;
} pg_memory_query;
typedef struct {struct json_object *items;uint64_t after;unsigned order,has_next;} pg_memory_page;
typedef struct {
 void *context;
 int (*scan)(void *,const pg_memory_query *,pg_memory_page *);
 int (*head)(void *,uint64_t conversation,uint64_t max_entry,struct json_object **);
} pg_record_queries;
/* Committed queries share the public Tx read barrier. Page owns detached items;
   failures clear the output. Latest head is unbounded and fork-aware. */
pg_record_tx_status pg_record_tx_scan(pg_record_tx *,const pg_record_queries *,const pg_memory_query *,pg_memory_page *);
pg_record_tx_status pg_record_tx_latest_head(pg_record_tx *,const pg_record_queries *,uint64_t conversation,struct json_object **);
typedef struct {
 uint64_t owner_task_id,parent,entry;
 unsigned root,reserved;
} pg_conversation_request;
typedef struct {
 void *context;
 /* mint receives Tx.read_context; fork Storage owns its own context. */
 int (*mint)(void *,uint64_t *);
 const pg_fork_storage *fork;
 /* Runs after Conversation and fork plans are staged. May stage or create
    more records on this Tx; the borrowed record is immutable. Return 0 on
    success. Hooks are synchronous; no borrowed pointer survives return. */
 int (*created)(void *,pg_record_tx *,const struct json_object *);
} pg_conversation_host;
/* Unified new/root/fork creation followed by the injected host hook. parent=0
   selects new/root and requires entry=0. Errors return no owned result. A hook
   error leaves its staged candidates for the enclosing callback to discard,
   matching a failed creating callback. Commit/discard are refused while a
   creation callback (including nested creations) is active. */
pg_record_tx_status pg_record_tx_create(pg_record_tx *,const pg_conversation_request *,const pg_conversation_host *,struct json_object **);
/* Session-owned settlement extension. finalize runs once with Tx sealed/busy,
   after existing plans validate, and must prepare any added writes privately.
   Returns 0 on success; nonzero rejects before Storage without failing Session.
   The final batch is validated again before admission. */
pg_record_tx_status pg_record_tx_commit_finalized(pg_record_tx *,int (*finalize)(void *,pg_record_tx *),void *);
/* Internal mutation-line validation used during a settlement finalizer;
   may invoke injected committed reads. 0 valid, nonzero invalid. */
int pg_record_tx_validate_batch(pg_record_tx *);
/* Session settlement routing after final validation, before Storage.
   route runs for a nonempty writable batch with Session/Tx busy; 0 chooses
   Storage, 1 means a prepared memory-only adoption, other values reject.
   A successful memory route consumes candidates without Storage hooks/sequence.
   A route must prepare all fallible state before its first adoption. */
pg_record_tx_status pg_record_tx_commit_routed(pg_record_tx *,
 int (*finalize)(void *,pg_record_tx *),void *,int (*route)(void *,pg_record_tx *));
#endif
