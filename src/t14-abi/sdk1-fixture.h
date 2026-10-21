#ifndef PANGU_SDK_FIXTURE_H
#define PANGU_SDK_FIXTURE_H
#include "client-plugin-sdk-v1.h"
#include "interop-contract-v1.h"
#include "interop-guard.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
__attribute__((visibility("hidden"))) pangu_interop_i32 pangu_route_cpp_invoke(const void *,void *,pangu_interop_u32,pangu_interop_i32,pangu_interop_i32 *) noexcept;
#else
__attribute__((visibility("hidden"))) pangu_interop_i32 pangu_route_cpp_invoke(const void *,void *,pangu_interop_u32,pangu_interop_i32,pangu_interop_i32 *) ;
#endif
#ifdef __cplusplus
}
#endif
#ifndef PANGU_SDK_FIXTURE_ENTRY
#define PANGU_SDK_FIXTURE_ENTRY pangu_sdk_fixture_entry
#define PANGU_SDK_FIXTURE_DISPOSE pangu_sdk_fixture_dispose
#define PANGU_SDK_FIXTURE_LOAD pangu_sdk_fixture_load
#define PANGU_SDK_FIXTURE_USE pangu_sdk_fixture_use
#endif
typedef struct {pangu_plugin_request request;const void *table;void *state;} sdk_fixture_provision;
typedef struct {pangu_plugin_key key;pangu_sdk_u64 expected_count;} sdk_fixture_dependency;
typedef struct {
 sdk_fixture_provision provisions[16];sdk_fixture_dependency dependencies[16];
 pangu_sdk_u32 provision_count,dependency_count,flags,entries,cleanups,loads,fail_line,reserved;
 pangu_sdk_i32 (*budget_unchanged)(const pangu_plugin_context *,pangu_sdk_u32);
 pangu_sdk_i32 (*expected)(const pangu_plugin_context *,const pangu_plugin_lease *,const pangu_plugin_key *,pangu_sdk_u32,pangu_sdk_u32);
 pangu_interop_guard_fn abi_call;
 const pangu_plugin_context *saved_context;pangu_plugin_completion setup_token,cleanup_token;
} sdk_fixture_config;
static inline pangu_sdk_i32 sdk1_abi_provide(const pangu_plugin_sdk *api,const pangu_plugin_context * a0,const pangu_plugin_request * a1,const void * a2,void * a3,pangu_sdk_u64 a4){sdk_fixture_config *cfg=(sdk_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->provide(a0,a1,a2,a3,a4);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3,(pangu_sdk_u64)a4};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))api->provide,a,5,96);}
static inline pangu_sdk_i32 sdk1_abi_count(const pangu_plugin_sdk *api,const pangu_plugin_context * a0,const pangu_plugin_key * a1,pangu_sdk_u64 a2,pangu_plugin_count * a3){sdk_fixture_config *cfg=(sdk_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->count(a0,a1,a2,a3);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))api->count,a,4,97);}
static inline pangu_sdk_i32 sdk1_abi_acquire(const pangu_plugin_sdk *api,const pangu_plugin_context * a0,const pangu_plugin_request * a1,pangu_sdk_u64 a2,pangu_plugin_lease * a3,pangu_plugin_report * a4){sdk_fixture_config *cfg=(sdk_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->acquire(a0,a1,a2,a3,a4);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3,(pangu_sdk_u64)a4};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))api->acquire,a,5,98);}
static inline pangu_sdk_i32 sdk1_abi_release(const pangu_plugin_sdk *api,const pangu_plugin_context * a0,const pangu_plugin_lease * a1,pangu_sdk_u64 a2){sdk_fixture_config *cfg=(sdk_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->release(a0,a1,a2);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))api->release,a,3,99);}
static inline pangu_sdk_i32 sdk1_abi_defer(const pangu_plugin_sdk *api,const pangu_plugin_context * a0,pangu_sdk_u64 a1,pangu_plugin_completion * a2){sdk_fixture_config *cfg=(sdk_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->defer(a0,a1,a2);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))api->defer,a,3,100);}
static inline pangu_sdk_i32 sdk1_abi_finish_setup(const pangu_plugin_sdk *api,const pangu_plugin_context * a0,const pangu_plugin_completion * a1,pangu_sdk_i32 a2,pangu_sdk_u64 a3){sdk_fixture_config *cfg=(sdk_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->finish_setup(a0,a1,a2,a3);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))api->finish_setup,a,4,101);}
static inline pangu_sdk_i32 sdk1_abi_finish_dispose(const pangu_plugin_sdk *api,const pangu_plugin_context * a0,const pangu_plugin_completion * a1,pangu_sdk_i32 a2,pangu_sdk_u64 a3){sdk_fixture_config *cfg=(sdk_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->finish_dispose(a0,a1,a2,a3);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))api->finish_dispose,a,4,102);}
static inline pangu_sdk_i32 sdk1_abi_invoke(const pangu_plugin_context *c,const void *table,void *state,pangu_sdk_u32 version,pangu_sdk_i32 value,pangu_sdk_i32 *out){sdk_fixture_config *cfg=(sdk_fixture_config *)c->userdata;if(!cfg->abi_call)return pangu_route_cpp_invoke(table,state,version,value,out);pangu_sdk_u64 a[]={(pangu_sdk_u64)table,(pangu_sdk_u64)state,version,(pangu_sdk_u64)value,(pangu_sdk_u64)out};return (pangu_sdk_i32)cfg->abi_call((void(*)(void))pangu_route_cpp_invoke,a,5,112);}
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((visibility("hidden"))) pangu_sdk_i32 PANGU_SDK_FIXTURE_ENTRY(const pangu_plugin_context *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 PANGU_SDK_FIXTURE_DISPOSE(const pangu_plugin_context *,const pangu_plugin_completion *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 PANGU_SDK_FIXTURE_LOAD(const pangu_plugin_context *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 PANGU_SDK_FIXTURE_USE(const pangu_plugin_context *);
#ifdef __cplusplus
}
#endif
#endif
