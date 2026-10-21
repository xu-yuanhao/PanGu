#ifndef PANGU_SDK2_FIXTURE_H
#define PANGU_SDK2_FIXTURE_H
#include "client-plugin-sdk2-v2.h"
#ifndef SDK2_ENTRY
#define SDK2_ENTRY sdk2_c_entry
#define SDK2_LOAD sdk2_c_load
#define SDK2_DISPOSE sdk2_c_dispose
#define SDK2_DRAIN sdk2_c_drain
#define SDK2_USE sdk2_c_use
#define SDK2_EVAL sdk2_c_evaluate
#endif
typedef struct sdk2_fixture_config sdk2_fixture_config;
typedef struct sdk2_service_state {const pangu_plugin_context *context;const void *table;pangu_sdk_i32 value,calls;} sdk2_service_state;
typedef struct sdk2_test_service {
 pangu_sdk_u32 version,struct_size,target,reserved;
 pangu_sdk_i32 (*evaluate)(void *,const pangu_plugin_lease *,pangu_sdk_i32,pangu_sdk_i32 *);
} sdk2_test_service;
typedef struct {pangu_plugin_request request;sdk2_test_service *table;sdk2_service_state *state;} sdk2_provision;
typedef struct {pangu_plugin_key key;pangu_sdk_u64 expected_count;} sdk2_dependency;
struct sdk2_fixture_config {
 sdk2_provision provisions[16];sdk2_dependency dependencies[16];
 pangu_sdk_u32 provision_count,dependency_count,flags,entries,cleanups,loads,fail_line,pending;
 const pangu_plugin_context *saved_context;pangu_plugin_completion setup_token,cleanup_token;pangu_plugin_lease pending_root;
 pangu_sdk_u32 pending_ordinal,pending_dependency;
 pangu_sdk_u64 (*abi_call)(void (*)(void),const pangu_sdk_u64 *,pangu_sdk_u32,pangu_sdk_u32);
 pangu_sdk_i32 (*expected)(const pangu_plugin_context *,const pangu_plugin_lease *,const pangu_plugin_key *,pangu_sdk_u32);
 void (*entered)(const pangu_plugin_context *,const pangu_plugin_lease *,void *);
};
static inline pangu_sdk_i32 sdk2_abi_provide(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_request * a1,const void * a2,void * a3,pangu_sdk_u64 a4){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->base.provide(a0,a1,a2,a3,a4);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3,(pangu_sdk_u64)a4};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->base.provide,a,5,64);}
static inline pangu_sdk_i32 sdk2_abi_count(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_key * a1,pangu_sdk_u64 a2,pangu_plugin_count * a3){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->base.count(a0,a1,a2,a3);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->base.count,a,4,65);}
static inline pangu_sdk_i32 sdk2_abi_acquire(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_request * a1,pangu_sdk_u64 a2,pangu_plugin_lease * a3,pangu_plugin_report * a4){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->base.acquire(a0,a1,a2,a3,a4);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3,(pangu_sdk_u64)a4};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->base.acquire,a,5,66);}
static inline pangu_sdk_i32 sdk2_abi_release(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_lease * a1,pangu_sdk_u64 a2){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->base.release(a0,a1,a2);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->base.release,a,3,67);}
static inline pangu_sdk_i32 sdk2_abi_defer(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,pangu_sdk_u64 a1,pangu_plugin_completion * a2){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->base.defer(a0,a1,a2);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->base.defer,a,3,68);}
static inline pangu_sdk_i32 sdk2_abi_finish_setup(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_completion * a1,pangu_sdk_i32 a2,pangu_sdk_u64 a3){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->base.finish_setup(a0,a1,a2,a3);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->base.finish_setup,a,4,69);}
static inline pangu_sdk_i32 sdk2_abi_finish_dispose(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_completion * a1,pangu_sdk_i32 a2,pangu_sdk_u64 a3){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->base.finish_dispose(a0,a1,a2,a3);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->base.finish_dispose,a,4,70);}
static inline pangu_sdk_i32 sdk2_abi_acquire_child(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_lease * a1,const pangu_plugin_request * a2,pangu_sdk_u64 a3,pangu_plugin_lease * a4,pangu_plugin_report * a5){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->acquire_child(a0,a1,a2,a3,a4,a5);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2,(pangu_sdk_u64)a3,(pangu_sdk_u64)a4,(pangu_sdk_u64)a5};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->acquire_child,a,6,71);}
static inline pangu_sdk_i32 sdk2_abi_accept(const pangu_plugin_sdk2 *api,const pangu_plugin_context * a0,const pangu_plugin_lease * a1,pangu_sdk_u64 a2){sdk2_fixture_config *cfg=(sdk2_fixture_config *)a0->userdata;if(!cfg->abi_call)return api->accept(a0,a1,a2);pangu_sdk_u64 a[]={(pangu_sdk_u64)a0,(pangu_sdk_u64)a1,(pangu_sdk_u64)a2};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))api->accept,a,3,72);}
static inline pangu_sdk_i32 sdk2_abi_evaluate(const pangu_plugin_context *c,const sdk2_test_service *table,void *state,const pangu_plugin_lease *lease,pangu_sdk_i32 input,pangu_sdk_i32 *out){sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;if(!cfg->abi_call)return table->evaluate(state,lease,input,out);pangu_sdk_u64 a[]={(pangu_sdk_u64)state,(pangu_sdk_u64)lease,(pangu_sdk_u64)input,(pangu_sdk_u64)out};return (pangu_sdk_i32)cfg->abi_call((void (*)(void))table->evaluate,a,4,73);}
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((visibility("hidden"))) pangu_sdk_i32 SDK2_ENTRY(const pangu_plugin_context *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 SDK2_LOAD(const pangu_plugin_context *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 SDK2_DISPOSE(const pangu_plugin_context *,const pangu_plugin_completion *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 SDK2_DRAIN(const pangu_plugin_context *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 SDK2_USE(const pangu_plugin_context *);
__attribute__((visibility("hidden"))) pangu_sdk_i32 SDK2_EVAL(void *,const pangu_plugin_lease *,pangu_sdk_i32,pangu_sdk_i32 *);
#ifdef __cplusplus
}
static_assert(sizeof(sdk2_test_service)==24,"versioned call interface");
#else
_Static_assert(sizeof(sdk2_test_service)==24,"versioned call interface");
#endif
#endif
