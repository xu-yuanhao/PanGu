#ifndef PANGU_ABI_GUARD_TEST_H
#define PANGU_ABI_GUARD_TEST_H
#include "src/t14-abi/client-plugin-host2-v2.h"
typedef void (*pangu_abi_fn)(void);
typedef struct {pangu_abi_fn fn;pangu_u64 args[13],result,mask,sp,previous;} pangu_abi_frame;
_Static_assert(sizeof(pangu_abi_frame)==144,"guard frame");
__attribute__((visibility("hidden"))) pangu_u64 pangu_abi_checked(pangu_abi_fn,const pangu_u64 *,pangu_u32,pangu_u32);
void pangu_abi_guard_frame(pangu_abi_frame *);
void pangu_abi_misaligned_frame(pangu_abi_frame *);
pangu_abi_fn pangu_abi_factory_target(pangu_u32);
pangu_abi_fn pangu_abi_stub_target(pangu_u32);
void pangu_abi_init(void);
void pangu_abi_report(void);
const pangu_lifecycle_api *pangu_abi_life(void);
const pangu_lifecycle_child_api *pangu_abi_child(void);
const pangu_entry_api *pangu_abi_entry(void);
const pangu_service_api *pangu_abi_route(void);
const pangu_invocation_api2 *pangu_abi_inv(void);
const pangu_plugin_host_api2 *pangu_abi_host(void);
const pangu_plugin_host_api2 *pangu_abi_host_original(void);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_c_entry(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_c_load(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_c_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *completion);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_c_drain(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_c_use(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_entry(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_load(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *completion);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_drain(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_use(const pangu_plugin_context *c);
void pangu_abi_counts(pangu_u32,pangu_u64 *);
#endif
