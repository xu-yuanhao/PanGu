#ifndef PANGU_SERVICE_ROUTE_H
#define PANGU_SERVICE_ROUTE_H
#include "client-entry-v1.h"
#define PANGU_ROUTE_ABI 1
#define PANGU_ROUTE_API_SIZE 40
#ifndef __ASSEMBLER__
/* Trusted single-thread host candidate. Same immutable successful T13 views and
 * entry records as lifecycle preparation; bind_geometry before use. All storage
 * remains mapped/live. Slots initially zero, never republished in a runtime.
 * Interface header version is the declared service version, not host ABI.
 * No hot replacement: stop/drain old runtime, rebuild with increased epochs.
 */
typedef struct {pangu_u32 version,struct_size,target,reserved;} pangu_service_header;
typedef struct {
 const pangu_service_header *table;void *state;pangu_owner *owner;pangu_u64 epoch;
 pangu_u32 provider,version,table_size,target,published,reserved;
} pangu_service_slot;
typedef struct {
 const pangu_lifecycle_api *host;pangu_lifecycle_runtime *runtime;
 const pangu_static_manifest *manifest;const pangu_prepare_result *plan;
 const pangu_graph_output *graph;const pangu_entry_record *records;
 pangu_service_slot *slots;pangu_u64 slot_count;
} pangu_service_router;
/* mode0 business,1 setup,2 cleanup; scope exact corresponding lifecycle ticket.
 * Name is fixed 32-byte manifest name, not a local catalogue index. Binding is
 * one explicit resolved edge; MANY enumeration remains in the prepared graph.
 */
typedef struct {
 unsigned char service[32];pangu_u32 version,minimum_size,binding,consumer,mode,reserved;
 const void *scope;
} pangu_service_query;
typedef struct {pangu_lifecycle_call_ticket ticket;const pangu_service_header *table;void *state;} pangu_service_lease;
/* Pure routing refusals preserve lease/report. Once lifecycle admission is
 * reached report.work includes routing work, including failed admission. Lease
 * published only on success; release exactly once after the actual call exits.
 * No implicit invocation/retry; never cache table/state beyond live lease.
 */
typedef struct {
 pangu_u32 abi_version,struct_size,target,reserved;
 pangu_i32 (*publish)(const pangu_service_router *,const pangu_entry_context *,pangu_u32,const pangu_service_header *,void *,pangu_u64);
 pangu_i32 (*acquire)(const pangu_service_router *,const pangu_service_query *,pangu_u64,pangu_service_lease *,pangu_lifecycle_report *);
 pangu_i32 (*release)(const pangu_service_router *,const pangu_service_lease *);
} pangu_service_api;
_Static_assert(sizeof(pangu_service_header)==16,"service header");
_Static_assert(sizeof(pangu_service_slot)==56,"service slot");
_Static_assert(sizeof(pangu_service_router)==64,"service router");
_Static_assert(sizeof(pangu_service_query)==64,"service query");
_Static_assert(sizeof(pangu_service_lease)==80,"service lease");
_Static_assert(sizeof(pangu_service_api)==40,"service API");
_Static_assert(__builtin_offsetof(pangu_service_lease,table)==64,"service table");
_Static_assert(__builtin_offsetof(pangu_service_api,publish)==16,"publish offset");
_Static_assert(__builtin_offsetof(pangu_service_api,acquire)==24,"acquire offset");
_Static_assert(__builtin_offsetof(pangu_service_api,release)==32,"release offset");
const pangu_service_api *pangu_service_route_query(pangu_u32,pangu_u32,pangu_u32,pangu_u32);
#endif
#endif
