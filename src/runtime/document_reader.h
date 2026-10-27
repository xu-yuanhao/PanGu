#ifndef PANGU_DOCUMENT_READER_H
#define PANGU_DOCUMENT_READER_H
#include "durable_session.h"
/* Other return values use pg_document_status. Pending admits no read: enqueue
   or retry on the Session line. Existing ABI enum values remain unchanged. */
#define PG_DOCUMENT_READ_PENDING 11
#define PG_DOCUMENT_ENTRY_NOT_VISIBLE 12
#define PG_DOCUMENT_SESSION_FAILED 13
#define PG_DOCUMENT_SESSION_CLOSED 14
#define PG_DOCUMENT_OBSERVE_CANCELLED 15
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
typedef struct {
 const pg_document_definition *definition;
 const pg_document_address *address;
 pg_session_listeners *listeners;
 const pg_document_observer_host *host;
 void *context;
 int (*cancelled)(void *);
} pg_document_observe_request;
/* Acquires and attaches on the Session mutation line. PENDING requests retry
   after admitted work; unlike snapshot it never attaches during an active Tx.
   Uses the same guarded cache/provider path as current snapshot. Cancellation
   is checked before/after acquisition and after attachment; a late cancellation
   detaches/releases the tentative observer before returning CANCELLED. Optional
   cancellation callback only reads caller state and cannot call Session work.
   Request and address expire on return; definition can stay cached. Observer
   starts zeroed/released; success follows observer_open ownership/lifetime. */
int pg_durable_observe(pg_durable_session *,const pg_document_observe_request *,pg_document_observer *);
typedef struct {
 const pg_document_definition *definition;
 const pg_document_address *address;
 pg_session_listeners *listeners;
 const pg_document_watch_pins *pins;
 void *context;
 int (*cancelled)(void *);
} pg_document_watch_request;
/* Acquire a Watch with the same admission/cancellation as observe. Pin service
   is copied; its context is borrowed until termination. Report uses Session
   listeners. Source starts zeroed/released; drive source.watch outside adopt. */
int pg_durable_watch(pg_durable_session *,const pg_document_watch_request *,pg_document_watch_source *);
/* Combine request cancellation checks with this provider throughout acquisition,
   then install its lifetime subscription. An abort during installation returns
   CANCELLED with source/binding cleaned, as a late acquisition cancellation.
   Provider rejection releases the tentative source and partial subscription.
   Successful source and cancellation binding have their ordinary lifetimes. */
int pg_durable_watch_cancellable(pg_durable_session *,const pg_document_watch_request *,pg_document_watch_source *,pg_document_watch_cancellation *,const pg_document_watch_cancellation_provider *);
/* Mutation-line maintenance on an open healthy Session: save holds (including
   configured durable notifications), synchronize timer, then drop loaded cache
   nodes. Providers, definitions and observer subscriptions remain usable.
   PENDING during admitted work; a flush error preserves nodes and first cause. */
int pg_durable_session_unload_documents(pg_durable_session *);
#endif
