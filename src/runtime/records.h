#ifndef PANGU_RECORDS_H
#define PANGU_RECORDS_H
#include <stdint.h>
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
 PG_TX_ALLOCATION,PG_TX_READ_FAILED,PG_TX_COMMIT_FAILED,PG_TX_MISSING } pg_record_tx_status;
typedef enum {PG_TX_CONVERSATION,PG_TX_ENTRY,PG_TX_TASK,PG_TX_SUBMISSION,
 PG_TX_DOCUMENT_CREATE,PG_TX_DOCUMENT_COPY,PG_TX_DOCUMENT_CHANGE,PG_TX_DOCUMENT_RETIRE} pg_record_tx_write;
typedef struct {
 pg_record_session *session;
 void *read_context;
 /* Committed table read: 0 owned result, 1 missing, other values error. */
 int (*read)(void *,unsigned,uint64_t,struct json_object **);
 struct json_object *writes;
 unsigned busy,sealed,has_table_write,reserved;
} pg_record_tx;
/* Caller provides zeroed or discarded Tx and serializes the entire mutation
   line on its Session. Callbacks cannot reenter this Tx. Begin borrows Session
   and read service; discard releases private candidates and is idempotent.
   stage accepts a final StorageWrite of the declared kind and deep-copies it.
   A table-write attempt forbids subsequent public table reads, including if
   staging fails. Document writes do not set the table-write barrier.
   Commit seals before admission, submits through pg_records_commit and consumes
   candidates on every result. Inspect Session for the underlying commit error.
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
#endif
