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
/* Seal registration and snapshot close listeners once, before draining admitted
   work. Preserve commit observers until final listeners_close. May run inside
   an observer; restores its dispatch guard. Allocation rejection is retryable
   and invokes no callback. closed: 0 open, 2 draining, 1 fully released. */
int pg_session_listeners_begin_close(pg_session_listeners *,void *);
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
/* Confirm prepared stored publications without allocating. Existing seq
   scalars are replaced; zero createdAt/retiredAt placeholders in Document
   records become the confirmed sequence. Positive lifecycle stamps are kept.
   Both channels are validated before any mutation. Memory-only publications,
   missing seq/stamps, invalid JSON shapes or unsafe sequence reject unchanged.
   Call at most once on each prepared pair, before delivery. Outputs belong
   to prepare; no concurrent
   readers may borrow them during confirmation. */
int pg_session_publications_confirm(pg_session_publications *,uint64_t sequence);
typedef struct {
 pg_session_listeners *listeners;
 pg_document_holds *holds;
 void *delivery_context;
} pg_session_notification_host;
/* Bind the default stored-transaction notification service. Host/listeners and
   optional native holds stay borrowed through settlement. Task ownership is
   resolved from the Tx's latest candidate/cache. Notification snapshots own
   their lifecycle/value/ops before retirement; held queues remain unchanged
   during preparation. Delivery uses host.delivery_context. */
int pg_session_notifications_bind(pg_session_notification_host *,pg_document_notifications *);
/* Stored settlement on the actual Session mutation line with a prepared
   notification host. Merges held changes, restores ordinary hooks, syncs the
   hold timer and releases the admitted Tx exactly as settle(deferred=0).
   The notification descriptor is borrowed through return and mandatory. */
pg_record_tx_status pg_durable_session_settle_notified(pg_durable_session *,
 const pg_document_scan *,const pg_document_notifications *);
/* Prepare both memory-only and persistent-fallback publications before
   routing; confirmation only selects the already owned payload. Default
   stored binding remains appropriate for stored-only settlement. */
int pg_session_deferred_notifications_bind(pg_session_notification_host *,pg_document_notifications *);
pg_record_tx_status pg_durable_session_defer_notified(pg_durable_session *,uint64_t within_ms,
 const pg_document_scan *,const pg_document_notifications *);
/* Capture the exact folded ops already prepared by holds flush, then deliver
   only a durable publication after pins are released. No duplicate memory
   observation; host.holds must identify the flushed owner. */
int pg_session_hold_notifications_bind(pg_session_notification_host *,pg_hold_notifications *);
typedef struct {
 pg_session_listeners *listeners;
 void *head,*tail;
 size_t prepared_count;
 unsigned busy,closed;
} pg_session_dispatch;
/* Caller initializes a zeroed queue with borrowed listeners. Prepare retains
   an immutable publication and allocates one token before adoption. Queue and
   listener lifetimes include every prepared/queued token. enqueue transfers
   its token without allocating/calling subscribers; discard releases only an
   unqueued token. Both reject a token belonging to another queue or one already
   enqueued. Calls are serialized on the host thread.
   drain is the explicit native microtask boundary, outside Storage.adopt:
   FIFO host notifications use the subscriptions present when each runs. A
   snapshot allocation failure leaves that event queued for retry. Reentrant
   drain/close return 1. close returns 1 until prepared and queued work drains,
   then seals admission and returns 0. Keep listeners open through close. */
int pg_session_dispatch_prepare(pg_session_dispatch *,const struct json_object *,void *,void **);
int pg_session_dispatch_enqueue(pg_session_dispatch *,void *);
int pg_session_dispatch_discard(pg_session_dispatch *,void *);
int pg_session_dispatch_drain(pg_session_dispatch *);
int pg_session_dispatch_close(pg_session_dispatch *);
/* Synchronous stored observers: capture the first callback error without
   failing Session inside Storage.adopt. Delivery infrastructure failure is
   returned separately. Caller resolves observer-vs-Storage failure precedence
   after backend settlement; remaining observers still run. */
int pg_session_deliver_observed(pg_session_listeners *,const void *,void *,int *first_error);
typedef struct {pg_session_notification_host base;pg_session_dispatch *dispatch;} pg_session_adoption_host;
/* New adoption semantics: queue host dispatch before synchronous observation.
   The queue and delivery context remain borrowed until drained. Cleanup drains
   the explicit microtask queue outside Storage; providers may drain it earlier
   after their synchronous adopt callback returns. Observer failures are latched
   after settlement; an existing Storage failure wins and observer error reports.
   All token/payload allocation precedes adoption. */
int pg_session_adoption_notifications_bind(pg_session_adoption_host *,pg_document_notifications *);
int pg_session_adoption_deferred_bind(pg_session_adoption_host *,pg_document_notifications *);
int pg_session_adoption_hold_bind(pg_session_adoption_host *,pg_hold_notifications *);
/* Seal Session admission, contain close listeners, then finish admitted work
   and held saves. Final close drains the host queue and releases listeners.
   Same return convention as Session close; retain host through all retries.
   Close listeners are synchronous and cannot invoke Session work. */
int pg_durable_session_close_notified(pg_durable_session *,pg_session_adoption_host *);
typedef struct {pg_record_storage backend;pg_session_dispatch *dispatch;} pg_session_dispatch_storage;
/* Synchronous native backend adapter: drain adopted host notifications outside
   the backend's commit/adopt step, before its result reaches failure policy.
   Return the backend's exact status. A queue snapshot allocation failure leaves
   work for the notification cleanup's retry, without inventing Storage errors. */
int pg_session_dispatch_storage_commit(void *,const struct json_object *,pg_records_adopt,void *,uint64_t *);
/* Owned address-following state; fields are API-managed and JSON is immutable.
   Open copies a checked loaded document and retains no pointer into its cache.
   Retirement drops the state value; its returned event owns the last value.
   Outputs are initially empty/disjoint and released before reuse. */
typedef struct {struct json_object *record,*value;uint64_t incarnation,version;unsigned active,busy;} pg_document_observation;
typedef struct {const pg_document_record *record;const struct json_object *value,*ops;uint64_t version;} pg_document_observation_input;
typedef struct {struct json_object *value,*ops;unsigned retired,replaced;} pg_document_observation_change;
pg_document_status pg_document_observation_open(pg_document_observation *,const pg_document_loaded *);
/* One already-validated document notice. NULL value denotes retirement; ops
   are needed for same-incarnation/version updates. MISSING means no delivery.
   New incarnation/version produces a root replacement. Allocation failure
   preserves the previous state and clears output. State/event share immutable
   owned values, independent of the input notice and retired loaded handle. */
pg_document_status pg_document_observation_advance(pg_document_observation *,const pg_document_observation_input *,pg_document_observation_change *);
void pg_document_observation_change_release(pg_document_observation_change *);
pg_document_status pg_document_observation_close(pg_document_observation *);
typedef struct {
 void *context;
 int (*advance)(void *,const pg_document_observation_change *,void *delivery_context);
 int (*end)(void *,int failure);
 int (*pin)(void *,uint64_t incarnation);
 void (*unpin)(void *,uint64_t incarnation);
} pg_document_observer_host;
typedef struct {
 pg_document_observation state;
 pg_session_listeners *listeners;
 pg_document_observer_host host;
 uint64_t pinned;
 unsigned attached,busy;
} pg_document_observer;
/* Mutation-line attachment to an already acquired checked document. Copies the
   host; listeners/context stay borrowed through release. Pin services serialize
   cache ownership without user callbacks. Successful pin is undone once.
   Synchronous advance errors report, detach and end with that error; end errors
   report. Task owner terminal ends after all its document changes. Detach may
   run inside advance or another observer, stops the remainder of the snapshot,
   and keeps caller storage/value alive. Release only outside registry dispatch
   and callbacks; a rejected release retains all resources. */
pg_document_status pg_document_observer_open(pg_document_observer *,pg_session_listeners *,const pg_document_loaded *,const pg_document_observer_host *);
void pg_document_observer_detach(pg_document_observer *);
pg_document_status pg_document_observer_release(pg_document_observer *);
/* Allocation-free pin queries over active observer subscriptions. No separate
   ownership index is retained. Compatible pinned callback for cache hooks;
   a composition combines this result with holds/other existing pin owners.
   Registration and pin state are installed before the host's pin callback and
   removed before unpin, so a callback can immediately update cache idleness. */
size_t pg_session_observers_pin_count(const pg_session_listeners *,uint64_t incarnation);
int pg_session_observers_pinned(void *listeners,uint64_t incarnation);
#endif
