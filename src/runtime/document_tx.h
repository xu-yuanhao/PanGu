#ifndef PANGU_DOCUMENT_TX_H
#define PANGU_DOCUMENT_TX_H
#include "records.h"
#include "delta.h"
typedef struct {
 pg_document_loaded *loaded;
 pg_document_change *change;
 pg_document_prepared prepared;
 pg_document_adoption adoption;
 struct json_object *write;
 size_t write_index;
 void *context;
 /* Runs after batch validation, only for nonempty same-version edits.
    Borrowed value/ops are read-only. Return 0 delta, 1 base, other error. */
 int (*checkpoint)(void *,const struct json_object *,const struct json_object *,uint64_t deltas_since_base);
 unsigned kind,reserved;
} pg_document_tx_entry;
/* Loaded handles and edits are borrowed, disjoint, unique per incarnation and
   alive for the whole synchronous mutation line. Entry internal fields start
   zeroed; reserved is private to managed Doc lifecycle. Seal and prepare all
   edits, validate all generated plans, then invoke
   checkpoint predicates and prepare adoption before Storage admission.
   Storage confirmation applies all loaded values before the Session's existing
   adopt hook; its existing publish hook follows normal settlement. Hooks and
   context are restored on return. No-op edits cause no Storage commit.
   After a valid admission every result discards prepared state and seals edits;
   argument/precondition refusal leaves caller state unchanged. Managed creation
   stores a base and installs after confirmation; managed retirement evicts the
   matching incarnation. Deferred holds are coordinated by the Session host.
   Callers must finish through this entry once loaded edits were acquired. */
pg_record_tx_status pg_document_tx_commit(pg_record_tx *,pg_document_tx_entry *,size_t);
typedef struct {
 pg_record_tx *records;
 pg_document_cache *cache;
 pg_document_tx_entry *entries;
 size_t count,capacity;
 unsigned busy,sealed;
} pg_document_tx;
/* Bind one Doc scope to an open record Tx and cache on its mutation line.
   Scope fields are API-managed. get
   acquires committed incarnations or staged fork-copy drafts; missing addresses
   return PG_DOC_MISSING.
   Repeated addresses reuse the first edit and its definition/checkpoint,
   matching transaction-local draft identity. Returned edits are owned by this
   scope and remain stable across entry-array growth, until finish/abort.
   Callers must not prepare/discard them separately. Cache sweep/eviction/close
   must not run while the mutation line holds these borrowed loaded handles.
   finish seals edits, commits through uniform settlement and frees them;
   abort discards the whole record Tx and frees edits. Neither leaves handles
   valid after return. doc adds initializer/mint based creation on missing. */
pg_document_status pg_document_tx_open(pg_document_tx *,pg_record_tx *,pg_document_cache *);
pg_document_status pg_document_tx_get(pg_document_tx *,const pg_document_definition *,const pg_document_address *,void *,
 int (*checkpoint)(void *,const struct json_object *,const struct json_object *,uint64_t),pg_document_change **);
typedef struct {
 const pg_document_definition *definition;
 const pg_document_address *address;
 const struct json_object *seed;
 const pg_document_initializer *initializer;
 int (*mint)(void *,uint64_t *); /* record Tx read_context */
 void *context;
 int (*checkpoint)(void *,const struct json_object *,const struct json_object *,uint64_t);
} pg_document_request;
/* Acquire an existing or transaction-local draft; on missing address require
   its owner, initialize a detached value, then mint and stage a private new
   incarnation. Initialization/mint only run on first missing acquisition.
   All callbacks run with scope/record Tx busy. Failure returns no edit and
   leaves prior candidates intact; minted IDs remain consumed. Staged fork
   copies materialize their selected source/version without invoking
   initializer or mint; settlement replaces the copy with the edited base and
   preserves source-write conflict validation. New incarnations install during
   Storage confirmation; admitted backend/adoption errors follow
   the Session fail-stop contract. */
pg_document_status pg_document_tx_doc(pg_document_tx *,const pg_document_request *,pg_document_change **);
/* Idempotently retire the latest incarnation. Prior edits still settle their
   final content; a following doc call creates a fresh incarnation. Retirement
   without an edit only resolves record identity/scope, without initialization,
   migration or Storage.read. Missing addresses have no write. */
pg_document_status pg_document_tx_retire(pg_document_tx *,const pg_document_definition *,const pg_document_address *);
typedef struct {
 void *context;
 /* 0 owned ascending current-lifecycle page, other values error.
    Output starts zeroed; any partial items remain owned by the caller on error. */
 int (*scan)(void *,const pg_document_query *,pg_memory_page *);
} pg_document_scan;
/* Finish with terminal-Task lifecycle discovery. Newly terminal tasks retire
   acquired/new documents and page through committed task documents without
   loading their values. Committed tasks transitioning to terminal require scan; an absent scan
   rejects before Storage admission. New tasks need no committed scan.
   All edits are sealed before Storage reads/scan; callbacks cannot reenter.
   Scanner and context are borrowed only for this synchronous call. */
pg_record_tx_status pg_document_tx_settle(pg_document_tx *,const pg_document_scan *);
typedef struct {
 void *context;
 /* Prepare detached hold queues/pins without changing authoritative state.
    Entries include unchanged reads; only entries with write!=NULL are held.
    within_ms=UINT64_MAX denotes an infinite window. A partial token returned
    on error is discarded. Return a pg_document_status. */
 int (*prepare)(void *,const pg_document_tx_entry *,size_t,uint64_t within_ms,void **token);
 /* Allocation-free install of prepared queues/pins after value adoption.
    Nonzero is an internal adoption failure and fails the Session. */
 int (*apply)(void *,void *token);
 /* Always after an attempted prepare; release token, preserving applied state. */
 void (*discard)(void *,void *token);
 /* Optional memory-only observer after successful queue/value adoption.
    Entries are borrowed through return; only write!=NULL entries changed.
    Retain any JSON independently. */
 void (*observe)(void *,const pg_document_tx_entry *,size_t);
} pg_document_deferred;
/* Deferred when every write is an existing latest/initial conversation Doc at
   its stored version; otherwise submit the whole batch normally. Checkpoint
   selection precedes this decision. Memory adoption leaves Storage sequence,
   stored versions/delta counts and durable publish hooks untouched.
   Host owns queue/pin/deadline lifetime and drains/merges held changes before
   later durable operations. prepare/apply/discard are required and cannot
   reenter the Session; scanner has settle's contract. */
pg_record_tx_status pg_document_tx_deferred(pg_document_tx *,const pg_document_scan *,
 const pg_document_deferred *,uint64_t within_ms);
/* finish delegates to settle with no committed-document scanner. */
pg_record_tx_status pg_document_tx_finish(pg_document_tx *);
pg_record_tx_status pg_document_tx_abort(pg_document_tx *);
#endif
