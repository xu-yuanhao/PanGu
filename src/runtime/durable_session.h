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
/* Document providers share the guard above. An unsuccessful but nonfatal
   guarded read returns EXEMPT; callers must not relatch it as a backend failure.
   Existing pg_document_storage/scan and historical Entry signatures are kept. */
#define PG_GUARDED_STORAGE_EXEMPT 68
typedef struct {
 pg_durable_storage *guard;
 void *context;
 int (*find)(void *,const pg_document_address *,uint64_t,struct json_object **);
 int (*read)(void *,uint64_t,uint64_t,struct json_object **);
 int (*scan)(void *,const pg_document_query *,pg_memory_page *);
 int (*entry)(void *,uint64_t id,uint64_t conversation,struct json_object **,uint64_t *sequence);
} pg_durable_documents;
int pg_durable_document_find(void *,const pg_document_address *,uint64_t,struct json_object **);
int pg_durable_document_read(void *,uint64_t,uint64_t,struct json_object **);
int pg_durable_document_scan(void *,const pg_document_query *,pg_memory_page *);
/* Frontend takes conversation,id for pg_document_history_reader; the backend
   callback above takes id,conversation like MemoryStorage. */
int pg_durable_document_entry(void *,uint64_t conversation,uint64_t id,struct json_object **,uint64_t *);
typedef int (*pg_session_listener)(void *,const void *publication,void *delivery_context);
typedef struct {pg_session_listener callback;void *context;unsigned kind,reserved;} pg_session_listener_entry;
typedef struct {
 pg_record_session *records;
 pg_session_listener_entry *entries;
 size_t count,capacity;
 void *context;
 void (*report)(void *,int);
 unsigned busy,closed;
} pg_session_listeners;
/* Zeroed caller storage with records and optional report initialized. kind:
   0 internal adoption, 1 durable publication, 2 close. Callback/context identity
   is deduplicated per kind. Mutations during dispatch affect future snapshots;
   a removed callback already in the snapshot still runs. Delivery borrows
   publication/context only until return; callbacks are synchronous and return
   an error code instead of throwing. Internal errors latch adoption failure;
   host/close errors are reported and never stop remaining callbacks. Report
   callbacks must return normally. All descriptor use is serialized.
   close snapshots kind 2 once, clears registrations and seals admission; it
   releases storage, with no resources retained after success. */
int pg_session_listen(pg_session_listeners *,unsigned,pg_session_listener,void *);
void pg_session_unlisten(pg_session_listeners *,unsigned,pg_session_listener,void *);
int pg_session_deliver(pg_session_listeners *,unsigned,const void *,void *);
int pg_session_listeners_close(pg_session_listeners *,void *);
typedef struct {
 const struct json_object *writes,*observed_documents,*stored_documents;
 uint64_t sequence;
 unsigned mode,reserved; /* 0 stored transaction, 1 memory adoption, 2 held flush */
} pg_session_publication_request;
typedef struct {struct json_object *observed,*stored;} pg_session_publications;
/* Prepare detached publication payloads before adoption. Table changes retain
   write order and precede document changes; raw Document StorageWrites are
   excluded. Shared document inputs retain shared publication identity in mode
   0. Mode 1 publishes only memory, mode 2 only stored held changes. Request
   arrays contain finite JSON. Caller keeps outputs immutable through delivery
   and clears them with discard. A failed preparation leaves zero outputs. */
int pg_session_publications_prepare(const pg_session_publication_request *,pg_session_publications *);
void pg_session_publications_discard(pg_session_publications *);
/* Deliver internal observed first, then durable stored, borrowing both values.
   The host supplies the originating delivery context. Callback errors remain
   contained as for deliver; successful return means notification completed. */
int pg_session_publications_deliver(pg_session_listeners *,const pg_session_publications *,void *);
typedef struct {
 const struct json_object *record,*copy_source,*value,*ops;
 uint64_t conversation_id,version;
 unsigned flags,reserved;
} pg_document_publication_request;
enum {PG_NOTICE_RETIRE=1,PG_NOTICE_COPY=2,PG_NOTICE_CREATED=4,PG_NOTICE_PUBLISHES=8};
/* Describe one adopted Document plan. record is its final lifecycle record;
   conversation_id=0 omits the Session-scoped field. Retirement precedes copy,
   and omits version, emits null value/empty ops. Copy emits source only. New
   content emits empty ops; loaded changes preserve exact prepared ops,
   including empty migration ops. No publish flag means successful no notice.
   Inputs are borrowed only during call; output is detached/owned or NULL.
   Failure leaves NULL. Prepare lifecycle stamps before calling; do not retain
   a loaded handle across retirement eviction. */
int pg_document_publication_prepare(const pg_document_publication_request *,struct json_object **);
#endif
