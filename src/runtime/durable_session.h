#ifndef PANGU_DURABLE_SESSION_H
#define PANGU_DURABLE_SESSION_H
#include "hold_timer.h"
typedef struct {
 void *context;
 /* 0 closed, 1 pending (retain resources), negative terminal close error
    after releasing resources. Repeated calls poll a pending close. */
 int (*close)(void *);
} pg_durable_close;
typedef struct {
 pg_record_session *records;
 pg_document_cache *cache;
 pg_document_holds *holds;
 pg_hold_timer *timer;
 pg_durable_close backend;
 pg_document_cache_hooks previous_hooks;
 pg_document_deferred deferred;
 pg_document_merge merge;
 pg_record_tx *active_records;
 pg_document_tx *active_documents;
 unsigned busy,phase,result,reserved;
 int (*release_held)(void *,void *);
} pg_durable_session;
/* Bind initialized, mutually matching primitives on one host thread. They and
   backend services remain borrowed until close is terminal. Holds start empty, timer has no
   ticket, and cache has no previous flush owner. Existing clock/pin hooks are composed and restored on
   cache close. Primitives are exclusively driven through this Session while
   bound; do not sweep/evict while an admitted transaction borrows loaded data.
   Begin admits one caller-owned Tx/Doc scope, borrowing read services. It stays
   on the mutation line until settle/abort. Close seals new admissions at once
   and waits for that admitted transaction; it may still settle while closing.
   Settle uses the native deferred/merge services and syncs the hold timer.
   The queue provider uses idle() before dispatching an admitted timer job.
   close returns 1 pending, 0 closed healthy, -1 closed failed (-2 invalid state). It stops timers/drains queued callbacks, finishes holds,
   closes cache, then polls backend close. Errors preserve the first records
   failure. result holds that final pg_records_status; repeated close is safe.
   Storage/cache/hold initialization remains with their service providers. */
pg_records_status pg_durable_session_open(pg_durable_session *,pg_record_session *,pg_document_cache *,
 pg_document_holds *,pg_hold_timer *,const pg_durable_close *);
pg_record_tx_status pg_durable_session_begin(pg_durable_session *,pg_record_tx *,pg_document_tx *,void *read_context,
 int (*read)(void *,unsigned,uint64_t,struct json_object **));
pg_record_tx_status pg_durable_session_settle(pg_durable_session *,unsigned deferred,uint64_t within_ms,const pg_document_scan *);
pg_record_tx_status pg_durable_session_abort(pg_durable_session *);
int pg_durable_session_idle(const pg_durable_session *);
int pg_durable_session_close(pg_durable_session *);
#endif
