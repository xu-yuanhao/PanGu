#ifndef PANGU_RECORD_MEMORY_H
#define PANGU_RECORD_MEMORY_H
#include "records.h"
#include "documents.h"
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
   Document create/change/retire/copy share batch atomicity and the global ID index;
   only one live incarnation may occupy an exact logical address. Copy resolves
   a committed conversation snapshot into a base with matching kind/key/history/
   fork; no document command may change its source in the same batch. */
pg_memory_status pg_record_memory_open(pg_record_memory *);
pg_memory_status pg_record_memory_close(pg_record_memory *);
pg_record_storage pg_record_memory_storage(pg_record_memory *);
pg_memory_status pg_record_memory_mint(pg_record_memory *,uint64_t *);
pg_memory_status pg_record_memory_read(pg_record_memory *,pg_record_table,uint64_t,struct json_object **);
/* Status indices: tasks pending/running/waiting/sleeping/completing/terminal;
   submissions queued/placed/done/unanswered. limit>=1. Cursor order 0 models
   legacy cursors without order, using the table default. Page must be disjoint
   from inputs and own no prior items. Success owns items; failure clears page. */
pg_memory_status pg_record_memory_scan(void *,const pg_memory_query *,pg_memory_page *);
/* conversation_id=0 performs a direct global lookup; otherwise fork visibility
   is checked. commit_sequence is mandatory; returned entry is owned. */
pg_memory_status pg_record_memory_entry(pg_record_memory *,uint64_t id,uint64_t conversation_id,
                                       struct json_object **,uint64_t *commit_sequence);
/* max_entry_id=UINT64_MAX is unbounded; zero is a real inclusive cutoff. */
pg_memory_status pg_record_memory_head(void *,uint64_t conversation_id,uint64_t max_entry_id,
                                      struct json_object **);
/* Detached StoredDocument JSON result; at=UINT64_MAX selects current.
   Historical current-only reads are invalid; missing/lifetime absence is MISSING.
   Document rows are private; generic read/scan accept the four ordinary tables.
   This reader may be used in adopt, after the batch has been fully applied. */
pg_memory_status pg_record_memory_document(void *,uint64_t id,uint64_t at,struct json_object **);
/* Logical identity lookup returns an owned DocumentRecord, without reading
   content. Historical membership also works for current-only documents.
   at=UINT64_MAX selects current. Absence returns PG_MEMORY_MISSING. */
pg_memory_status pg_record_memory_find_document(void *,const pg_document_address *,uint64_t at,struct json_object **);
/* Scope-exact ascending lifecycle records; at=UINT64_MAX means current.
   Optional kind and signed-safe after cursor. limit>=1; owned Page snapshot.
   Historical membership works even when content is current-only. */
pg_memory_status pg_record_memory_scan_documents(void *,const pg_document_query *,pg_memory_page *);
/* Borrow a scan/head service descriptor; no resources are allocated. */
void pg_record_memory_queries(pg_record_memory *,pg_record_queries *);
/* Borrow find/read bindings for generic document acquisition. */
void pg_record_memory_documents(pg_record_memory *,pg_document_storage *);
#endif
