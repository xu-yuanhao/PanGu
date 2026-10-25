#ifndef PANGU_RECORD_MEMORY_H
#define PANGU_RECORD_MEMORY_H
#include "records.h"
#include <stddef.h>
typedef enum { PG_TABLE_CONVERSATION,PG_TABLE_ENTRY,PG_TABLE_TASK,PG_TABLE_SUBMISSION } pg_record_table;
typedef struct {uint64_t id;struct json_object *value;uint64_t sequence;unsigned table,reserved;} pg_memory_row;
typedef struct {
 pg_memory_row *rows;
 size_t count,capacity;
 uint64_t next_id,next_sequence;
 unsigned busy,closed;
} pg_record_memory;
typedef enum { PG_MEMORY_OK,PG_MEMORY_MISSING,PG_MEMORY_INVALID,PG_MEMORY_ALLOCATION } pg_memory_status;
/* Fresh zeroed or completely closed caller storage. Reads return detached owned
   values; rows are private. Serialized use, no retained Storage callbacks.
   IDs are positive safe integers; integral JSON floats share the same namespace.
   Table writes use Pi StorageWrite JSON objects; conversation/entry IDs cannot
   be replaced; task/submission replacement keeps the same global record type. */
pg_memory_status pg_record_memory_open(pg_record_memory *);
pg_memory_status pg_record_memory_close(pg_record_memory *);
pg_record_storage pg_record_memory_storage(pg_record_memory *);
pg_memory_status pg_record_memory_mint(pg_record_memory *,uint64_t *);
pg_memory_status pg_record_memory_read(pg_record_memory *,pg_record_table,uint64_t,struct json_object **);
#endif
