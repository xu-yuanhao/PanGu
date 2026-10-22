#ifndef PANGU_LIFECYCLE_CANDIDATE_H
#define PANGU_LIFECYCLE_CANDIDATE_H
#include "src/bootstrap-api/bootstrap.h"
#define PANGU_LIFECYCLE_ABI 1
#define PANGU_LIFECYCLE_API_SIZE 152
#define PANGU_LIFECYCLE_TARGET 1
#ifndef __ASSEMBLER__
#include "src/bootstrap-api/host-plan.h"
/* Candidate host-control ABI for C17 / SysV Linux x86_64. Not a plugin security
 * boundary. Caller storage and borrowed layouts remain valid and immutable as
 * specified; runtime/owner initially zero. Every exported work driver takes fuel.
 * Geometry must be bound before start. Exact ABI/size/target/flags handshake.
 */
typedef pangu_i32 (*pangu_release_fn)(void *);
struct pangu_owner;
typedef struct pangu_cleanup_ticket {const struct pangu_owner *owner;pangu_u64 epoch,serial;} pangu_cleanup_ticket;
typedef pangu_i32 (*pangu_async_release_fn)(void *,const pangu_cleanup_ticket *);
typedef struct pangu_owned_resource {pangu_release_fn release;void *context;pangu_async_release_fn begin;} pangu_owned_resource;
typedef struct pangu_owned_operation {pangu_u32 generation,active;} pangu_owned_operation;
typedef struct pangu_owner {
 pangu_u32 abi_version,struct_size,state,busy;
 pangu_owned_resource *resources;pangu_u64 capacity,count;
 pangu_owned_operation *operations;pangu_u64 operation_capacity,inflight;
 pangu_u64 epoch;pangu_i32 load_error,cleanup_error;
 pangu_u64 cleanup_serial;pangu_u32 cleanup_waiting,cleanup_completed;pangu_i32 cleanup_status;pangu_u32 cleanup_callback;
} pangu_owner;
typedef struct pangu_operation_ticket {const pangu_owner *owner;pangu_u64 epoch;pangu_u32 index,generation;} pangu_operation_ticket;
_Static_assert(sizeof(pangu_owner)==104,"owner frame");
_Static_assert(sizeof(pangu_operation_ticket)==24,"operation ticket");
enum { PANGU_OWNER_LOADING=1,PANGU_OWNER_ACTIVE,PANGU_OWNER_QUIESCING,PANGU_OWNER_DISPOSED };
/* WIP single-thread owned synchronous/asynchronous disposer core. Caller spans
 * valid/aligned/disjoint. Owner starts wholly zero; reuse only after Disposed
 * with increasing epoch. Async begin returns5 pending, other status completed.
 * It copies its ticket before returning; explicit finish may happen inline or
 * later but cannot advance cleanup itself. Return5 after inline finish too.
 * Ticket storage/lifetime valid through callbacks; no cross-thread completion.
 * Late/duplicate/wrong-epoch tickets rejected; caller drives stop again.
 * Completion means callback finished, not proof external side effects undone.
 */
typedef struct {pangu_u64 work;pangu_u32 state,pending,reason,reserved;} pangu_lifecycle_report;
_Static_assert(sizeof(pangu_lifecycle_report)==24,"stop report");
/* reason0 terminal,1 fuel exhausted,2 awaiting operation/callback/completion.
 * Work bounds host transitions/resource callbacks, not callback wall-clock time.
 * Budget0 publishes exhausted report without mutating owner. */
typedef pangu_i32 (*pangu_lifecycle_setup_fn)(pangu_owner *,void *);
typedef struct {pangu_owner *owner;pangu_lifecycle_setup_fn setup;void *context;} pangu_lifecycle_item;
typedef struct {pangu_u32 consumer,provider;} pangu_lifecycle_edge;
typedef struct {const pangu_lifecycle_edge *edges;pangu_u64 edge_count;pangu_u32 *disposed;pangu_u64 capacity;} pangu_lifecycle_graph;
typedef struct {pangu_u64 begin,end;} pangu_lifecycle_span;
struct pangu_lifecycle_runtime;
typedef struct {const struct pangu_lifecycle_runtime *runtime;pangu_operation_ticket operation;} pangu_lifecycle_setup_ticket;
/* WIP single-thread, prevalidated borrowed activation list. Runtime wholly zero
 * initially and not reinitialized while any ticket/callback can refer to it.
 * Defer inside current setup before returning5; copy ticket, finish later/inline.
 * Finish only records completion; pump advances activation or rollback.
 * All borrowed arrays/frames valid/disjoint/immutable through complete stop.
 */
typedef struct pangu_lifecycle_runtime {
 pangu_lifecycle_item *items;pangu_u64 count,started;pangu_u32 busy,stop_requested;pangu_i32 load_error,cleanup_error;
 pangu_operation_ticket setup_operation;pangu_u32 setup_waiting,setup_completed,callback_open,phase;
 pangu_i32 setup_status;pangu_u32 reserved;
 const pangu_lifecycle_graph *stop_graph;pangu_u64 launched;
 pangu_u64 stop_cursor,stop_edge_cursor;pangu_u32 stop_scan_active,stop_pass_active;
 const pangu_lifecycle_span *protected_spans;pangu_u64 protected_count;
 pangu_u64 validate_cursor,validate_peer,entry_cursor;pangu_u32 validate_stage,validate_owner_checked,entry_ready,start_reserved;
 pangu_u32 stop_rescan,completion_reserved;
} pangu_lifecycle_runtime;
/* Bind complete pangu_lifecycle_runtime metadata before start; spans is caller-owned immutable
 * after success, disposable on failure. Count <=u32 permits <=32 lookup probes.
 * Caller mappings live/aligned; this validates geometry, not pointer provenance.
 */
/* Phase5 validating,6 rejected before any setup. Initialization of geometry is
 * separately budgeted but not resumable. These APIs bound lifecycle validation,
 * slot search, setup invocation and ensuing rollback under one remaining fuel.
 */
/* Internal host completion hook, to hide behind the final facade. */
/* Trusted prepared POD views, not an authenticity proof. All source/destination
 * spans valid/aligned/disjoint; plan lifetime covers this call. Source items are
 * indexed by manifest instance, output items by activation order. No callback.
 * Complete validation and budget check before publication, caller-owned storage.
 * Static entry -> owner-aware setup ABI adapter is intentionally not inferred.
 */

/* After preparing items against these same immutable successful views, map
 * resolved bindings into activation positions for optional graph stop mode.
 * No callbacks; final edges/count unchanged on failure. Duplicates allowed
 * when separate services bind the same instance pair. All spans disjoint. */

typedef struct {pangu_lifecycle_item *items;pangu_u64 item_capacity;pangu_lifecycle_edge *edges;pangu_u64 edge_capacity;} pangu_lifecycle_storage;
typedef struct {pangu_u64 item_count,edge_count,work;} pangu_lifecycle_prepared;
/* Unified transaction: all outputs/counts committed only after both semantic
 * stages, complete destination geometry and aggregate budget succeed. Source
 * owner frames/pools required only for eligible instances; Pending items ignored.
 * Scratch-free, no callbacks. All caller mappings valid and source immutable.
 */
/* Host liveness gate indexed by resolved binding, not a full service vtable or
 * native sandbox. Calls retain both owners. Caller must invoke the provider only
 * after enter succeeds and finish when that operation actually exits.
 * Metadata/token storage valid/aligned/disjoint and borrowed graph immutable.
 */
typedef struct {const pangu_lifecycle_runtime *runtime;pangu_u32 binding,reserved;pangu_operation_ticket consumer,provider;} pangu_lifecycle_call_ticket;
_Static_assert(sizeof(pangu_lifecycle_call_ticket)==64,"call ticket");

/* Atomic retry-style admission, not a persistent operation. Report reason1 /
 * pending1 means insufficient fuel; reason0/pending0 includes quota refusal.
 * No owner/ticket mutation until both slots and the commit fuel are available.
 * Source scope, output ticket and output report must be disjoint.
 */
typedef struct pangu_lifecycle_api {
 pangu_u32 abi_version,struct_size,target,reserved;
pangu_i32 (*owner_init)(pangu_owner *,pangu_owned_resource *,pangu_u64,pangu_owned_operation *,pangu_u64,pangu_u64,pangu_u64);
pangu_i32 (*register_resource)(pangu_lifecycle_runtime *,pangu_u32,pangu_release_fn,void *);
pangu_i32 (*register_async)(pangu_lifecycle_runtime *,pangu_u32,pangu_async_release_fn,void *);
pangu_i32 (*prepare)(const pangu_prepare_result *,const pangu_graph_output *,const pangu_lifecycle_item *,pangu_u64,const pangu_lifecycle_storage *,pangu_u64,pangu_lifecycle_prepared *);
pangu_i32 (*bind_geometry)(pangu_lifecycle_runtime *,pangu_lifecycle_span *,pangu_u64,pangu_u64);
pangu_i32 (*start)(pangu_lifecycle_runtime *,pangu_u64,pangu_lifecycle_report *);
pangu_i32 (*pump)(pangu_lifecycle_runtime *,pangu_u64,pangu_lifecycle_report *);
pangu_i32 (*stop)(pangu_lifecycle_runtime *,pangu_u64,pangu_lifecycle_report *);
pangu_i32 (*setup_scope)(pangu_lifecycle_runtime *,pangu_lifecycle_setup_ticket *);
pangu_i32 (*setup_defer)(pangu_lifecycle_runtime *,pangu_lifecycle_setup_ticket *);
pangu_i32 (*setup_finish)(pangu_lifecycle_runtime *,const pangu_lifecycle_setup_ticket *,pangu_i32);
pangu_i32 (*cleanup_scope)(pangu_lifecycle_runtime *,pangu_u32,pangu_cleanup_ticket *);
pangu_i32 (*disposer_finish)(pangu_lifecycle_runtime *,pangu_u32,const pangu_cleanup_ticket *,pangu_i32);
pangu_i32 (*call)(pangu_lifecycle_runtime *,pangu_u32,pangu_u64,pangu_lifecycle_call_ticket *,pangu_lifecycle_report *);
pangu_i32 (*setup_call)(pangu_lifecycle_runtime *,pangu_u32,const pangu_lifecycle_setup_ticket *,pangu_u64,pangu_lifecycle_call_ticket *,pangu_lifecycle_report *);
pangu_i32 (*cleanup_call)(pangu_lifecycle_runtime *,pangu_u32,const pangu_cleanup_ticket *,pangu_u64,pangu_lifecycle_call_ticket *,pangu_lifecycle_report *);
pangu_i32 (*call_finish)(pangu_lifecycle_runtime *,const pangu_lifecycle_call_ticket *);
} pangu_lifecycle_api;
_Static_assert(sizeof(pangu_lifecycle_api)==PANGU_LIFECYCLE_API_SIZE,"lifecycle API table");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,owner_init)==16,"first method");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,call_finish)==144,"last method");
_Static_assert(sizeof(pangu_owner)==104,"pangu_owner size");
_Static_assert(sizeof(pangu_owned_resource)==24,"pangu_owned_resource size");
_Static_assert(sizeof(pangu_owned_operation)==8,"pangu_owned_operation size");
_Static_assert(sizeof(pangu_operation_ticket)==24,"pangu_operation_ticket size");
_Static_assert(sizeof(pangu_cleanup_ticket)==24,"pangu_cleanup_ticket size");
_Static_assert(sizeof(pangu_lifecycle_runtime)==192,"pangu_lifecycle_runtime size");
_Static_assert(sizeof(pangu_lifecycle_item)==24,"pangu_lifecycle_item size");
_Static_assert(sizeof(pangu_lifecycle_edge)==8,"pangu_lifecycle_edge size");
_Static_assert(sizeof(pangu_lifecycle_graph)==32,"pangu_lifecycle_graph size");
_Static_assert(sizeof(pangu_lifecycle_span)==16,"pangu_lifecycle_span size");
_Static_assert(sizeof(pangu_lifecycle_setup_ticket)==32,"pangu_lifecycle_setup_ticket size");
_Static_assert(sizeof(pangu_lifecycle_call_ticket)==64,"pangu_lifecycle_call_ticket size");
_Static_assert(sizeof(pangu_lifecycle_storage)==32,"pangu_lifecycle_storage size");
_Static_assert(sizeof(pangu_lifecycle_prepared)==24,"pangu_lifecycle_prepared size");
_Static_assert(sizeof(pangu_lifecycle_report)==24,"pangu_lifecycle_report size");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,owner_init)==16,"owner_init offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,register_resource)==24,"register_resource offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,register_async)==32,"register_async offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,prepare)==40,"prepare offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,bind_geometry)==48,"bind_geometry offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,start)==56,"start offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,pump)==64,"pump offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,stop)==72,"stop offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,setup_scope)==80,"setup_scope offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,setup_defer)==88,"setup_defer offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,setup_finish)==96,"setup_finish offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,cleanup_scope)==104,"cleanup_scope offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,disposer_finish)==112,"disposer_finish offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,call)==120,"call offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,setup_call)==128,"setup_call offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,cleanup_call)==136,"cleanup_call offset");
_Static_assert(__builtin_offsetof(pangu_lifecycle_api,call_finish)==144,"call_finish offset");
const pangu_lifecycle_api *pangu_lifecycle_query(pangu_u32,pangu_u32,pangu_u32,pangu_u32);
#endif
#endif
