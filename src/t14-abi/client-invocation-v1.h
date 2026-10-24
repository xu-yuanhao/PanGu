#ifndef PANGU_INVOCATION_HOST_H
#define PANGU_INVOCATION_HOST_H
#include "client-lifecycle-child-v2.h"
#include "client-route-v1.h"
#include "client-plugin-sdk-v1.h"
#ifndef __ASSEMBLER__
/* Trusted host-control construction; not imported by plugins. Source metadata
 * immutable/live until all issued leases close. Zero bank initially; rebind only
 * with no active slots and increasing bank epoch. No dynamic allocation.
 * Opaque public lease layout reused; representation differs from SDK1 raw ticket.
 */
typedef struct {
 pangu_u64 generation;pangu_u32 active,children;
 pangu_u64 parent,parent_generation,root,root_generation;
 pangu_service_lease lease;pangu_u32 kind,reserved;unsigned char scope[32];
} pangu_invocation_slot;
typedef struct {
 pangu_u32 abi_version,struct_size;pangu_u64 epoch;
 const pangu_service_router *router;const pangu_service_api *route;
 const pangu_lifecycle_child_api *child;pangu_invocation_slot *slots;pangu_u64 count;
} pangu_invocation_bank;
typedef struct {
 pangu_u32 abi_version,struct_size,target,reserved;
 pangu_i32 (*bind)(pangu_invocation_bank *,const pangu_service_router *,const pangu_service_api *,const pangu_lifecycle_child_api *,pangu_invocation_slot *,pangu_u64,pangu_u64,pangu_u64);
 pangu_i32 (*open_root)(pangu_invocation_bank *,const pangu_service_query *,pangu_u64,pangu_plugin_lease *,pangu_lifecycle_report *);
 pangu_i32 (*open_child)(pangu_invocation_bank *,const pangu_plugin_lease *,pangu_u32,const pangu_service_query *,pangu_u64,pangu_plugin_lease *,pangu_lifecycle_report *);
 pangu_i32 (*close)(pangu_invocation_bank *,pangu_u32,const pangu_plugin_lease *,pangu_u64);
} pangu_invocation_api;
_Static_assert(sizeof(pangu_invocation_slot)==168,"invocation slot");
_Static_assert(sizeof(pangu_invocation_bank)==56,"invocation bank");
_Static_assert(sizeof(pangu_invocation_api)==48,"invocation API");
const pangu_invocation_api *pangu_invocation_query(pangu_u32,pangu_u32,pangu_u32,pangu_u32);
#endif
#endif
