#ifndef PANGU_DOCUMENTS_H
#define PANGU_DOCUMENTS_H
#include "../contracts/client.h"
#include <stdint.h>
typedef enum { PG_DOC_SESSION,PG_DOC_CONVERSATION,PG_DOC_TASK } pg_document_scope;
typedef struct {
 pg_slice kind,key;
 int64_t owner;
 unsigned scope,has_key;
} pg_document_address;
typedef enum { PG_DOC_OK,PG_DOC_INVALID,PG_DOC_CAPACITY,PG_DOC_ALLOCATION,
 PG_DOC_SEMANTICS,PG_DOC_NEWER_VERSION,PG_DOC_MIGRATION_REQUIRED,PG_DOC_MIGRATION_FAILED,PG_DOC_MISSING,PG_DOC_STORAGE_FAILED,PG_DOC_INITIAL_FAILED } pg_document_status;
/* Slices borrow valid UTF-8, may contain NUL, lengths <=INT_MAX. Session owner
   is zero; conversation/task owners use the signed JS safe-integer range.
   has_key distinguishes an absent family key from an empty family key. */
int pg_document_address_valid(const pg_document_address *);
int pg_document_address_equal(const pg_document_address *,const pg_document_address *);
/* Durable addressId JSON tuple. Output may alias input; size is disjoint from
   all inputs/output. Failure sets size=0 and leaves output untouched. Capacity
   includes a trailing NUL, excluded from size. No retained JSON allocation. */
pg_document_status pg_document_address_encode(const pg_document_address *,char *,size_t,size_t *);
struct json_object;
#define PG_DOCUMENT_DEPTH_LIMIT 256
/* Finite acyclic json-c values, at most 256 nested containers. Shared children
   are allowed. Caller serializes access; objects use ordinary JSON values. */
int pg_document_value_valid(const struct json_object *);
typedef enum { PG_DOC_LATEST=1,PG_DOC_REWINDABLE } pg_document_history;
typedef enum { PG_DOC_CURRENT=1,PG_DOC_INITIAL,PG_DOC_AS_OF } pg_document_fork;
typedef struct {
 pg_slice kind;
 uint64_t version;
 unsigned scope,history,fork,family;
 void *context;
 /* Input is a private borrowed copy; output is an owned reference, even if
    identical to input. Returned values may be retained with another reference. */
 int (*migrate)(void *,struct json_object *,uint64_t,struct json_object **);
} pg_document_definition;
typedef struct {
 pg_document_address address;
 uint64_t id;
 unsigned history,fork;
} pg_document_record;
pg_document_status pg_document_check(const pg_document_definition *,const pg_document_record *,uint64_t stored_version);
/* Input is borrowed, immutable; success yields a detached owned JSON object.
   Output pointer must be disjoint from inputs. Failure leaves *output=NULL. */
pg_document_status pg_document_materialize(const pg_document_definition *,const pg_document_record *,uint64_t,
                                         struct json_object *,struct json_object **output);
/* Decode a typed StoredDocument.record. Slices borrow the JSON record;
   output is disjoint and cleared on failure. IDs are positive safe integers. */
pg_document_status pg_document_record_decode(const struct json_object *,pg_document_record *);
typedef struct {
 unsigned scope,has_kind;
 uint64_t owner;
 pg_slice kind;
 uint64_t at;
 size_t limit;
 int64_t after;
 unsigned has_cursor,reserved;
} pg_document_query;
typedef struct {
 void *context;
 /* 0 owned result, 1 missing, other values storage failure. */
 int (*find)(void *,const pg_document_address *,uint64_t at,struct json_object **);
 int (*read)(void *,uint64_t id,uint64_t at,struct json_object **);
} pg_document_storage;
typedef struct {
 struct json_object *record_owner,*value;
 const pg_document_definition *definition;
 pg_document_record record;
 uint64_t stored_version,value_version,deltas_since_base;
 unsigned busy,active;
} pg_document_loaded;
/* Fresh zeroed or closed caller storage. Owns detached record/value; Definition
   remains borrowed until close. Find/read select current. Missing find returns
   MISSING; missing content after a successful find is STORAGE_FAILED.
   Migration changes value_version while stored_version retains the source.
   Fields are API-managed; consumers borrow them read-only while active. This
   is a loaded handle; Session cache/revision arbitration owns its lifetime. */
pg_document_status pg_document_loaded_open(pg_document_loaded *,const pg_document_storage *,const pg_document_definition *,const pg_document_address *);
pg_document_status pg_document_loaded_close(pg_document_loaded *);
typedef struct {
 void *context;
 uint64_t (*clock)(void *); /* monotonic milliseconds */
 /* Store held changes before version-driven replacement; NULL is no holds.
    The callback keeps an exclusive cache guard; released pins use the
    dedicated touch_flushing entry, while public cache reentry is refused. */
 int (*flush)(void *,const pg_document_loaded *);
 /* Observer/held incarnation pin count lives in its owning service. */
 int (*pinned)(void *,uint64_t id);
} pg_document_cache_hooks;
typedef struct {
 void *table,*head,*idle_head,*idle_tail;
 size_t count;
 pg_document_storage storage;
 void *context;
 uint64_t (*clock)(void *);
 uint64_t retention;
 int (*flush)(void *,const pg_document_loaded *);
 int (*pinned)(void *,uint64_t);
 unsigned busy,closed;
} pg_document_cache;
/* Session mutation-line only. Definitions, providers and hook contexts remain
   borrowed until close. get returns a cache-owned loaded handle, valid until
   reload/evict/sweep/close; consumers retaining data take independent ownership.
   Matching value_version reuses the handle; other versions flush held state
   before dropping/reloading. Acquisition still checks token semantics.
   Hash keys borrow cache-owned record slices (including embedded NUL).
   touch must follow observer/hold pin changes. sweep runs on the mutation line;
   its scheduling belongs to the host. Retention updates are explicit.
   close releases resources; Session stores holds before invoking it. */
pg_document_status pg_document_cache_open(pg_document_cache *,const pg_document_storage *,const pg_document_cache_hooks *,uint64_t retention);
pg_document_status pg_document_cache_get(pg_document_cache *,const pg_document_definition *,const pg_document_address *,pg_document_loaded **);
pg_document_status pg_document_cache_touch(pg_document_cache *,const pg_document_address *);
/* Only inside this cache's version-reload flush callback, after confirmed
   persistence/unpin. Preserves the mutation guard; all public cache reentry
   stays refused. No allocations or eviction. */
pg_document_status pg_document_cache_touch_flushing(pg_document_cache *,const pg_document_address *);
pg_document_status pg_document_cache_evict(pg_document_cache *,const pg_document_address *,uint64_t incarnation);
pg_document_status pg_document_cache_sweep(pg_document_cache *);
pg_document_status pg_document_cache_retention(pg_document_cache *,uint64_t);
pg_document_status pg_document_cache_close(pg_document_cache *);
typedef struct {
 pg_document_cache *cache;
 void *table;
 size_t limit,expected_count;
} pg_document_cache_reservation;
typedef struct {
 pg_document_cache *cache;
 void *node;
 struct json_object *stamp;
 unsigned state,busy;
} pg_document_install;
/* Prepare owns detached record/value and a preallocated lifecycle stamp.
   Input is an unstamped DocumentCreate; definition remains borrowed by cache.
   Plans/reservations are API-managed and disjoint from all inputs. Plan starts
   zeroed; discard releases any uninstalled node.
   After every acquisition/checkpoint, reserve once for all planned installs.
   Between reserve and apply only applies sharing this reservation may mutate
   the cache; keep cache open on the same Session mutation line. Apply invokes
   no hooks and allocates no storage under this precondition. New incarnations
   enter the idle chain at supplied monotonic milliseconds; host pins follow.
   Storage sequence is positive and safe; successful apply consumes the plan. */
pg_document_status pg_document_install_prepare(pg_document_cache *,const pg_document_definition *,
 const struct json_object *record,struct json_object *value,pg_document_install *);
pg_document_status pg_document_cache_reserve(pg_document_cache *,size_t extra,pg_document_cache_reservation *);
pg_document_status pg_document_install_apply(pg_document_install *,pg_document_cache_reservation *,uint64_t sequence,uint64_t now);
pg_document_status pg_document_install_discard(pg_document_install *);
/* Retirement lookup checks cached identity before Storage.find; it never
   materializes or migrates a value. Success returns an owned record snapshot. */
pg_document_status pg_document_cache_find_record(pg_document_cache *,const pg_document_address *,struct json_object **);
/* Conditional eviction using a live reservation. Preserves reserved index
   capacity and adjusts expected_count; missing/different incarnations are OK.
   The address may borrow the evicted node and expires on success. */
pg_document_status pg_document_cache_evict_reserved(pg_document_cache *,const pg_document_address *,uint64_t,pg_document_cache_reservation *);
/* Durable documentCreate record, without storage lifecycle stamps. Definition
   identity/semantics and native safe ID are checked; result owns all slices. */
pg_document_status pg_document_record_create(const pg_document_definition *,const pg_document_address *,uint64_t id,struct json_object **);
typedef struct {
 void *context;
 /* Family seed is a private borrowed copy, NULL for singleton. Return an owned
    object even when identical to seed; callbacks may retain another reference. */
 int (*initial)(void *,struct json_object *seed,struct json_object **);
} pg_document_initializer;
/* Detach family seed before calling initial, then validate/copy its object
   result. Singleton ignores supplied seed. Failure releases partial results;
   output is disjoint, initially unowned and cleared on failure. */
pg_document_status pg_document_initialize(const pg_document_definition *,const struct json_object *seed,
 const pg_document_initializer *,struct json_object **);
#endif
