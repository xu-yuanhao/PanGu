#ifndef PANGU_PLUGIN_SDK_H
#define PANGU_PLUGIN_SDK_H
#define PANGU_PLUGIN_SDK_ABI 1
#define PANGU_PLUGIN_SDK_SIZE 72
#define PANGU_PLUGIN_CONTEXT_SIZE 32
#ifndef __ASSEMBLER__
#ifdef __cplusplus
#define PANGU_SDK_ASSERT static_assert
extern "C" {
#else
#define PANGU_SDK_ASSERT _Static_assert
#endif
typedef unsigned int pangu_sdk_u32;
typedef unsigned long long pangu_sdk_u64;
typedef int pangu_sdk_i32;
struct pangu_plugin_sdk;
/* Borrowed canonical context, immutable through the instance lifetime. opaque
 * belongs to host. Native SDK narrows API exposure, not a memory sandbox.
 */
typedef struct {pangu_sdk_u32 abi_version,struct_size;const struct pangu_plugin_sdk *host;const void *opaque;void *userdata;} pangu_plugin_context;
typedef struct {unsigned char name[32];pangu_sdk_u32 version,reserved;} pangu_plugin_key;
typedef struct {pangu_plugin_key key;pangu_sdk_u32 ordinal,minimum_size;} pangu_plugin_request;
/* Never interpret opaque bytes; copy only. A lease holds both owners until
 * release after the actual operation exits. table/state valid only meanwhile.
 */
typedef struct {pangu_sdk_u64 opaque[8];const void *table;void *state;} pangu_plugin_lease;
typedef struct {pangu_sdk_u64 opaque[4];} pangu_plugin_completion;
typedef struct {pangu_sdk_u64 count,work;} pangu_plugin_count;
typedef struct {pangu_sdk_u64 work;pangu_sdk_u32 state,pending,reason,reserved;} pangu_plugin_report;
typedef pangu_sdk_i32 (*pangu_plugin_entry_fn)(const pangu_plugin_context *);
/* NULL completion for sync cleanup; async receives a token valid for callback
 * duration only: copy before returning5, finish_dispose after actual exit.
 */
typedef pangu_sdk_i32 (*pangu_plugin_dispose_fn)(const pangu_plugin_context *,const pangu_plugin_completion *);
typedef struct pangu_plugin_sdk {
 pangu_sdk_u32 abi_version,struct_size,target,reserved;
 pangu_sdk_i32 (*provide)(const pangu_plugin_context *,const pangu_plugin_request *,const void *,void *,pangu_sdk_u64);
 pangu_sdk_i32 (*count)(const pangu_plugin_context *,const pangu_plugin_key *,pangu_sdk_u64,pangu_plugin_count *);
 pangu_sdk_i32 (*acquire)(const pangu_plugin_context *,const pangu_plugin_request *,pangu_sdk_u64,pangu_plugin_lease *,pangu_plugin_report *);
 pangu_sdk_i32 (*release)(const pangu_plugin_context *,const pangu_plugin_lease *,pangu_sdk_u64);
 pangu_sdk_i32 (*defer)(const pangu_plugin_context *,pangu_sdk_u64,pangu_plugin_completion *);
 pangu_sdk_i32 (*finish_setup)(const pangu_plugin_context *,const pangu_plugin_completion *,pangu_sdk_i32,pangu_sdk_u64);
 pangu_sdk_i32 (*finish_dispose)(const pangu_plugin_context *,const pangu_plugin_completion *,pangu_sdk_i32,pangu_sdk_u64);
} pangu_plugin_sdk;
/* count returns all resolved edges for a declared dependency (optional absent
 * gives zero); acquire uses explicit ordinal, never implicitly chooses MANY.
 * Pure refusals preserve outputs. Admission report sums SDK+router+host fuel.
 * provide ordinal selects own matching provision; minimum_size must be16.
 * Defer then return5; finish_setup requires copied completion token even inline.
 * C++ callbacks must not let exceptions escape the C ABI.
 */
PANGU_SDK_ASSERT(sizeof(pangu_sdk_u32)==4,"u32");
PANGU_SDK_ASSERT(sizeof(pangu_sdk_u64)==8,"u64");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_context)==32,"context");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_key)==40,"key");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_request)==48,"request");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_lease)==80,"lease");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_completion)==32,"completion");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_count)==16,"count");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_report)==24,"report");
PANGU_SDK_ASSERT(sizeof(pangu_plugin_sdk)==72,"SDK");
PANGU_SDK_ASSERT(__builtin_offsetof(pangu_plugin_context,host)==8,"context host");
PANGU_SDK_ASSERT(__builtin_offsetof(pangu_plugin_context,userdata)==24,"context userdata");
PANGU_SDK_ASSERT(__builtin_offsetof(pangu_plugin_lease,table)==64,"lease table");
PANGU_SDK_ASSERT(__builtin_offsetof(pangu_plugin_sdk,provide)==16,"provide");
PANGU_SDK_ASSERT(__builtin_offsetof(pangu_plugin_sdk,finish_dispose)==64,"finish dispose");
#ifdef __cplusplus
}
#endif
#undef PANGU_SDK_ASSERT
#endif
#endif
