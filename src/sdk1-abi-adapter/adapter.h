#ifndef PANGU_SDK1_ADAPTER_H
#define PANGU_SDK1_ADAPTER_H
#include "src/t14-guard/guard.h"
#include "src/t14-abi/sdk1-fixture.h"
void pangu_sdk1_guard_report(void);
pangu_i32 pangu_sdk1_guard_eval(pangu_interop_method,void *,pangu_i32,pangu_i32 *);
pangu_i32 pangu_sdk1_guard_cpp_eval(void *,pangu_i32,pangu_i32 *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_c_entry(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_c_load(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_c_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *token);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_c_use(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_entry(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_load(const pangu_plugin_context *c);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *token);
__attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_use(const pangu_plugin_context *c);
#endif
