#ifndef PANGU_PLUGIN_SDK2_H
#define PANGU_PLUGIN_SDK2_H
#include "client-plugin-sdk-v1.h"
#define PANGU_PLUGIN_SDK2_SIZE 88
#ifndef __ASSEMBLER__
#ifdef __cplusplus
extern "C" {
#define PANGU_SDK2_ASSERT static_assert
#else
#define PANGU_SDK2_ASSERT _Static_assert
#endif
/* Same canonical 32-byte context, header abi2; base remains an actual first
 * member. SDK1 abi1/72 and raw-ticket lease representation remain unchanged.
 * SDK2 leases are managed opaque capabilities, never mix the two encodings.
 */
typedef struct {
 pangu_plugin_sdk base;
 pangu_sdk_i32 (*acquire_child)(const pangu_plugin_context *,const pangu_plugin_lease *,const pangu_plugin_request *,pangu_sdk_u64,pangu_plugin_lease *,pangu_plugin_report *);
 pangu_sdk_i32 (*accept)(const pangu_plugin_context *,const pangu_plugin_lease *,pangu_sdk_u64);
} pangu_plugin_sdk2;
/* count is a declaration view, not authority. Root acquire derives the caller's
 * own lifecycle scope. acquire_child derives authority only from a live incoming
 * lease whose provider is this instance. Service method must accept the incoming
 * handle before effects, and pass it explicitly to every nested acquire.
 * accept verifies an already-admitted operation, not new-work permission; child
 * acquisition still refuses cancelled/completed root scopes. Return2 on release
 * with live descendants; close after actual operation exit, never just callback
 * return. Native memory/code remains trusted, no implicit current-parent/TLS.
 */
static inline const pangu_plugin_sdk2 *pangu_plugin_sdk2_view(const pangu_plugin_context *c) {
 if(!c || c->abi_version!=2 || c->struct_size!=32 || !c->host || c->host->abi_version!=2 || c->host->struct_size!=88 || c->host->target!=1 || c->host->reserved) return (const pangu_plugin_sdk2 *)0;
 return (const pangu_plugin_sdk2 *)(const void *)c->host;
}
PANGU_SDK2_ASSERT(sizeof(pangu_plugin_sdk2)==88,"SDK2");
PANGU_SDK2_ASSERT(__builtin_offsetof(pangu_plugin_sdk2,base)==0,"base");
PANGU_SDK2_ASSERT(__builtin_offsetof(pangu_plugin_sdk2,acquire_child)==72,"child");
PANGU_SDK2_ASSERT(__builtin_offsetof(pangu_plugin_sdk2,accept)==80,"accept");
#ifdef __cplusplus
}
#endif
#undef PANGU_SDK2_ASSERT
#endif
#endif
