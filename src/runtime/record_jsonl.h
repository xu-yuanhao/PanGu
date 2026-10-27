#ifndef PANGU_RECORD_JSONL_H
#define PANGU_RECORD_JSONL_H
#include "record_memory.h"
typedef struct {
 pg_record_memory memory;
 int directory;
 unsigned opened,busy,poisoned,fsync;
} pg_record_jsonl;
typedef enum {PG_JSONL_OK,PG_JSONL_INVALID,PG_JSONL_IO,PG_JSONL_CORRUPT,PG_JSONL_ALLOCATION} pg_jsonl_status;
/* Zeroed caller-owned control; successful open owns its directory descriptor
   and exclusive writer lease. The path names an existing directory or a new
   leaf directory. PanGu v1 main.jsonl and task/doc sidecars define durable state.
   Recovery truncates torn/unconfirmed tails before admitting writes. Queries
   borrow the embedded memory provider; close is idempotent. A failed append
   poisons writes until reopen. fsync is 0 or 1, matching Pi's sidecar option. */
pg_jsonl_status pg_record_jsonl_open(pg_record_jsonl *,const char *,unsigned fsync);
pg_jsonl_status pg_record_jsonl_close(pg_record_jsonl *);
pg_record_storage pg_record_jsonl_storage(pg_record_jsonl *);
#endif
