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
#endif
