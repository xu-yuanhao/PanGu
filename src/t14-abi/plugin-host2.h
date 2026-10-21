#ifndef PANGU_PLUGIN_HOST2_H
#define PANGU_PLUGIN_HOST2_H
#include "plugin-host.h"
#include "plugin-sdk2.h"
#include "invocation2.h"
#ifndef __ASSEMBLER__
typedef struct {pangu_plugin_bank base;pangu_invocation_bank calls;const pangu_invocation_api2 *calls_api;} pangu_plugin_bank2;
typedef struct {
 pangu_u32 abi_version,struct_size,target,reserved;const pangu_plugin_host_api *base;const pangu_plugin_sdk2 *client;
 pangu_i32 (*bind)(pangu_plugin_bank2 *,const pangu_service_router *,const pangu_service_api *,const pangu_invocation_api2 *,const pangu_lifecycle_child_api *,pangu_plugin_binding *,pangu_u64,const pangu_plugin_definition *,void *const *,pangu_invocation_slot *,pangu_u64,pangu_u64,pangu_u64);
 pangu_i32 (*enter)(const pangu_entry_context *,pangu_u64);
} pangu_plugin_host_api2;
_Static_assert(sizeof(pangu_plugin_bank2)==96,"bank2");
_Static_assert(sizeof(pangu_plugin_host_api2)==48,"host2");
#endif
#endif
