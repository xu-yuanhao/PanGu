#ifndef PANGU_INVOCATION_EXT2_H
#define PANGU_INVOCATION_EXT2_H
#include "client-invocation-v1.h"
#ifndef __ASSEMBLER__
typedef struct {pangu_u64 work,bind_fuel;} pangu_invocation_bind_plan;
typedef struct {
 pangu_u32 abi_version,struct_size,target,reserved;const pangu_invocation_api *base;
 pangu_i32 (*prepare_bind)(pangu_invocation_bank *,const pangu_service_router *,const pangu_service_api *,const pangu_lifecycle_child_api *,pangu_invocation_slot *,pangu_u64,pangu_u64,pangu_u64,pangu_invocation_bind_plan *);
 pangu_i32 (*accept)(pangu_invocation_bank *,pangu_u32,const pangu_plugin_lease *,pangu_u64);
} pangu_invocation_api2;
_Static_assert(sizeof(pangu_invocation_api2)==40,"invocation extension");
#endif
#endif
