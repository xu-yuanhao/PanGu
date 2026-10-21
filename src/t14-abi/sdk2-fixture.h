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
 pangu_sdk_i32 (*expected)(const pangu_plugin_context *,const pangu_plugin_lease *,const pangu_plugin_key *,pangu_sdk_u32);
 void (*entered)(const pangu_plugin_context *,const pangu_plugin_lease *,void *);
};
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
