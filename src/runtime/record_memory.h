#ifndef PANGU_RECORD_MEMORY_H
#define PANGU_RECORD_MEMORY_H
#include "records.h"
#include <stddef.h>
typedef enum { PG_TABLE_CONVERSATION,PG_TABLE_ENTRY,PG_TABLE_TASK,PG_TABLE_SUBMISSION,PG_TABLE_DOCUMENT } pg_record_table;
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
   be replaced; task/submission replacement keeps the same global record type.
   Document create/change/retire share batch atomicity and the global ID index;
   only one live incarnation may occupy an exact logical address. */
pg_memory_status pg_record_memory_open(pg_record_memory *);
pg_memory_status pg_record_memory_close(pg_record_memory *);
pg_record_storage pg_record_memory_storage(pg_record_memory *);
pg_memory_status pg_record_memory_mint(pg_record_memory *,uint64_t *);
pg_memory_status pg_record_memory_read(pg_record_memory *,pg_record_table,uint64_t,struct json_object **);
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
/* Status indices: tasks pending/running/waiting/sleeping/completing/terminal;
   submissions queued/placed/done/unanswered. limit>=1. Cursor order 0 models
   legacy cursors without order, using the table default. Page must be disjoint
   from inputs and own no prior items. Success owns items; failure clears page. */
pg_memory_status pg_record_memory_scan(pg_record_memory *,const pg_memory_query *,pg_memory_page *);
/* conversation_id=0 performs a direct global lookup; otherwise fork visibility
   is checked. commit_sequence is mandatory; returned entry is owned. */
pg_memory_status pg_record_memory_entry(pg_record_memory *,uint64_t id,uint64_t conversation_id,
                                       struct json_object **,uint64_t *commit_sequence);
/* max_entry_id=UINT64_MAX is unbounded; zero is a real inclusive cutoff. */
pg_memory_status pg_record_memory_head(pg_record_memory *,uint64_t conversation_id,uint64_t max_entry_id,
                                      struct json_object **);
/* Detached StoredDocument JSON result; at=UINT64_MAX selects current.
   Historical current-only reads are invalid; missing/lifetime absence is MISSING.
   Document rows are private; generic read/scan accept the four ordinary tables.
   This reader may be used in adopt, after the batch has been fully applied. */
pg_memory_status pg_record_memory_document(pg_record_memory *,uint64_t id,uint64_t at,struct json_object **);
#endif
