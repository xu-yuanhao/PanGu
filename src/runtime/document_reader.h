#ifndef PANGU_DOCUMENT_READER_H
#define PANGU_DOCUMENT_READER_H
#include "durable_session.h"
/* Other return values use pg_document_status. Pending admits no read: enqueue
   or retry on the Session line. Existing ABI enum values remain unchanged. */
#define PG_DOCUMENT_READ_PENDING 11
#define PG_DOCUMENT_ENTRY_NOT_VISIBLE 12
#define PG_DOCUMENT_SESSION_FAILED 13
#define PG_DOCUMENT_SESSION_CLOSED 14
typedef struct {
 pg_document_storage documents;
 void *context;
 /* Fork-aware visible Entry, owned on success/partial failure, plus its actual
    commit sequence. 0 result, 1 invisible, other values storage failure. */
 int (*entry)(void *,uint64_t conversation,uint64_t id,struct json_object **,uint64_t *);
} pg_document_history_reader;
/* Return a detached owned value; output is initially unowned and disjoint from
   inputs. Current matching cached values are readable during an admitted Tx
   and include deferred memory adoption; a cache miss/version change waits for
   the line. Historical reads always wait for the line, resolve Entry visibility
   and use its original conversation/commit sequence. They never load cache,
   drain holds, or write migrations. Missing documents return PG_DOC_MISSING;
   an invisible Entry has its own error. Addresses/history providers and
   historical definitions expire on return. Current definitions can be retained
   by cache; keep them alive until eviction/close, as for cache_get.
   Unexpected Storage failures latch the first Session cause; argument,
   definition/migration and caller-copy failures leave Session healthy. */
int pg_durable_snapshot(pg_durable_session *,const pg_document_definition *,const pg_document_address *,struct json_object **);
int pg_durable_snapshot_as_of(pg_durable_session *,const pg_document_history_reader *,const pg_document_definition *,
 const pg_document_address *,uint64_t entry,struct json_object **);
#endif
