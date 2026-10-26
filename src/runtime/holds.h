#ifndef PANGU_HOLDS_H
#define PANGU_HOLDS_H
#include "document_tx.h"
typedef struct {
 pg_document_loaded *loaded;
 pg_pending_ops pending;
 uint64_t deadline;
 unsigned base,reserved;
} pg_document_hold;
typedef struct {
 pg_document_cache *cache;
 pg_document_hold *items;
 size_t count;
 void *context;
 uint64_t (*clock)(void *);
 void (*observe)(void *,const pg_document_tx_entry *,size_t);
 unsigned busy,active;
} pg_document_holds;
/* Session mutation-line owner in zeroed/closed storage. Cache, loaded handles and callbacks are borrowed
   through close; wire pinned into the cache's composite pin callback before
   admitting deferred edits. The host must settle holds before reload/eviction.
   Fields/items are read-only to consumers. Prepared tokens own private queue
   snapshots and lock this owner until discard, including after apply.
   Binding writes an independent deferred descriptor, borrowed through use.
   Deadlines saturate to UINT64_MAX (infinite). The timer host queries earliest
   and flushes all holds on expiry; this API does not schedule callbacks.
   release is called only after confirmed persistence of that incarnation.
   close requires an empty registry. Teardown after a failed Session may use
   discard_all to release pins/queues before closing the cache. */
pg_document_status pg_document_holds_open(pg_document_holds *,pg_document_cache *,void *,
 uint64_t (*clock)(void *),void (*observe)(void *,const pg_document_tx_entry *,size_t));
pg_document_status pg_document_holds_bind(pg_document_holds *,pg_document_deferred *);
pg_document_status pg_document_holds_merge_bind(pg_document_holds *,pg_document_merge *);
int pg_document_holds_pinned(void *,uint64_t id);
uint64_t pg_document_holds_deadline(const pg_document_holds *);
pg_document_status pg_document_holds_release(pg_document_holds *,const pg_document_loaded *);
pg_document_status pg_document_holds_discard_all(pg_document_holds *);
pg_document_status pg_document_holds_close(pg_document_holds *);
/* Persist all held incarnations in one Storage batch. Preparation snapshots
   queues without draining them; pre-admission failures preserve them. Adoption
   changes only durable replay counters, preserving loaded values/identities.
   Success releases queues/pins before the existing durable publish hook;
   memory observers are silent. Existing Session hooks/context are restored on
   every result. Backend/adoption failure follows Session fail-stop and keeps
   the holds for failed-Session teardown. Serialized with all Doc transactions. */
pg_records_status pg_document_holds_flush(pg_document_holds *,pg_record_session *);
/* Flush only one incarnation; missing is a healthy no-op. May run from the
   cache's version-reload flush callback; releases just that pin before reload.
   Other holds and their deadlines remain. Positive safe incarnation ID. */
pg_records_status pg_document_holds_flush_one(pg_document_holds *,pg_record_session *,uint64_t id);
/* Host has stopped admissions. Flush a healthy Session, skip writes after
   failure, then release queues and close this owner on every result. Cache and
   Storage remain owned by the host and close after return. Preserve first
   Session failure. Repeated finish returns the same Session result. */
pg_records_status pg_document_holds_finish(pg_document_holds *,pg_record_session *);
typedef struct {
 void *context;
 int (*open)(void *,pg_document_holds *,pg_record_session *,void **);
 int (*append)(void *,void *,const pg_document_loaded *,const struct json_object *ops);
 int (*prepare)(void *,void *,const struct json_object *writes);
 int (*adopt)(void *,void *,uint64_t sequence);
 void (*publish)(void *,void *,const struct json_object *writes,uint64_t sequence);
 void (*discard)(void *,void *);
} pg_hold_notifications;
/* Capture folded operations during normal flush preparation. open/append/
   prepare return pg_document_status and may return partial owned tokens;
   adopt is allocation-free. publish follows confirmed release of pins, and
   discard runs once after any attempted open. Existing hooks are preserved.
   id=0 flushes all; otherwise selected-incarnation/cache-reload rules apply.
   Services and inputs remain borrowed only through this synchronous call. */
pg_records_status pg_document_holds_flush_observed(pg_document_holds *,pg_record_session *,
 uint64_t id,const pg_hold_notifications *);
#endif
