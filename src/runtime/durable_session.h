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
/* Descriptor borrowed by Tx.read_context. Synchronous providers return 0 owned
   read value / positive safe minted ID, 1 missing (reads only), other error.
   A partial read output is consumed on every unsuccessful result. Classifier
   marks StorageRequestError equivalents; cancelled is checked only on read
   errors. Mint errors are never exempt. Descriptor calls are serialized and
   callbacks may not mutate its fields. Initialized Session and services stay
   alive through return. Closing still permits calls for an admitted Tx.
   Initializing descriptors with designated fields leaves private busy state 0. */
typedef struct {
 pg_durable_session *session;
 void *context;
 int (*read)(void *,unsigned,uint64_t,struct json_object **);
 int (*mint)(void *,uint64_t *);
 int (*request_error)(void *,int backend_status);
 int (*cancelled)(void *);
 unsigned busy,previous_busy;
} pg_durable_storage;
#define PG_GUARDED_STORAGE_FAILED 64
#define PG_GUARDED_STORAGE_CLOSED 65
#define PG_GUARDED_STORAGE_INVALID 66
#define PG_GUARDED_STORAGE_REENTRY 67
/* First nonexempt provider error returns its status and latches Session;
   crossing an already latched failure returns FAILED and discards the result.
   Subsequent calls fail before invoking providers; first Session cause wins.
   Healthy success transfers the read value / minted ID to caller. Host drives
   the Session's ordered close after a failure, as for other native fail-stop
   primitives. Existing record read/mint callback signatures are preserved. */
int pg_durable_storage_read(void *,unsigned,uint64_t,struct json_object **);
int pg_durable_storage_mint(void *,uint64_t *);
/* Query descriptor borrows a shared read/mint guard, so all methods use the
   same failure classification and cancellation context. Providers use context
   below; classifiers use guard.context. Pages and records transfer ownership;
   partial outputs are consumed on error, page metadata/entry sequence cleared.
   Input/output buffers must be disjoint and callbacks expire on return. */
typedef struct {
 pg_durable_storage *guard;
 void *context;
 int (*scan)(void *,const pg_memory_query *,pg_memory_page *);
 int (*head)(void *,uint64_t conversation,uint64_t maximum,struct json_object **);
 int (*request)(void *,const pg_record_request_query *,struct json_object **);
 int (*entry)(void *,uint64_t id,uint64_t conversation,struct json_object **,uint64_t *sequence);
} pg_durable_queries;
int pg_durable_storage_scan(void *,const pg_memory_query *,pg_memory_page *);
int pg_durable_storage_head(void *,uint64_t,uint64_t,struct json_object **);
int pg_durable_storage_request(void *,const pg_record_request_query *,struct json_object **);
int pg_durable_storage_entry(void *,uint64_t,uint64_t,struct json_object **,uint64_t *);
#endif
