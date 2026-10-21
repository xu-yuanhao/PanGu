#ifndef PANGU_PLUGIN_HOST_H
#define PANGU_PLUGIN_HOST_H
#include "client-route-v1.h"
#include "client-plugin-sdk-v1.h"
#ifndef __ASSEMBLER__
/* Trusted host construction only, absent from plugin SDK. Caller borrows all
 * views, userdata/config and plugin code until complete stop/drain. bind after
 * entry/lifecycle preparation + geometry and before any startup. */
struct pangu_plugin_bank;
typedef struct {pangu_plugin_entry_fn entry;pangu_plugin_dispose_fn dispose;pangu_u32 cleanup_mode,reserved;} pangu_plugin_definition;
typedef struct {
 pangu_plugin_context context;const struct pangu_plugin_bank *bank;
 const pangu_entry_record *record;pangu_u64 epoch;
 pangu_plugin_definition plugin;pangu_u32 entered,reserved;
} pangu_plugin_binding;
typedef struct pangu_plugin_bank {const pangu_service_router *router;const pangu_service_api *route;pangu_plugin_binding *bindings;pangu_u64 count;} pangu_plugin_bank;
typedef struct {
 pangu_u32 abi_version,struct_size,target,reserved;const pangu_plugin_sdk *client;
 pangu_i32 (*bind)(pangu_plugin_bank *,const pangu_service_router *,const pangu_service_api *,pangu_plugin_binding *,pangu_u64,const pangu_plugin_definition *,void *const *,pangu_u64);
 pangu_i32 (*enter)(const pangu_entry_context *,pangu_u64);
} pangu_plugin_host_api;
_Static_assert(sizeof(pangu_plugin_definition)==24,"definition");
_Static_assert(sizeof(pangu_plugin_binding)==88,"binding");
_Static_assert(sizeof(pangu_plugin_bank)==32,"bank");
_Static_assert(sizeof(pangu_plugin_host_api)==40,"host API");
const pangu_plugin_host_api *pangu_plugin_host_query(pangu_u32,pangu_u32,pangu_u32,pangu_u32);
#endif
#endif
