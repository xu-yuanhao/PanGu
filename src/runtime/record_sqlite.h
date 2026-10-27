#ifndef PANGU_RECORD_SQLITE_H
#define PANGU_RECORD_SQLITE_H
#include "record_memory.h"
typedef struct {
 pg_record_memory memory;
 void *database;
 unsigned busy,closed,read_only,poisoned;
} pg_record_sqlite;
typedef enum {PG_SQLITE_OK,PG_SQLITE_INVALID,PG_SQLITE_IO,PG_SQLITE_CORRUPT,PG_SQLITE_ALLOCATION} pg_sqlite_status;
/* Caller owns zeroed storage. open owns its SQLite connection; successful open
   exposes recovered memory through existing read/query/document services.
   One serialized owner per database; commits reject a stale recovered sequence.
   read_only is 0 or 1 and never creates a missing database. PanGu schema v1
   stores validated StorageWrite batches in its own database.
   Recovery cost is proportional to committed history. close is idempotent;
   pending/reentrant operations reject close, and failed open releases resources. */
pg_sqlite_status pg_record_sqlite_open(pg_record_sqlite *,const char *path,unsigned read_only);
pg_sqlite_status pg_record_sqlite_close(pg_record_sqlite *);
pg_record_storage pg_record_sqlite_storage(pg_record_sqlite *);
#endif
