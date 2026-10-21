#ifndef PANGU_LIFECYCLE_CHILD_H
#define PANGU_LIFECYCLE_CHILD_H
#include "lifecycle.h"
#ifndef __ASSEMBLER__
/* Query existing factory with abi2,size32; return pointer is explicitly cast
 * back to this extension type. abi1,size152 and its frozen table are unchanged.
 * TRUSTED host manager only: manager proves parent/root ancestry from immutable
 * issued-capability records. Kernel rechecks live tickets, receiver, root scope,
 * dependency direction and both Active child owners. Never expose to plugins.
 */
typedef struct {
 pangu_u32 abi_version,struct_size,target,reserved;
 const pangu_lifecycle_api *base;
 pangu_i32 (*enter_child)(pangu_lifecycle_runtime *,pangu_u32,const pangu_lifecycle_call_ticket *,const pangu_lifecycle_call_ticket *,pangu_u32,const void *,pangu_u64,pangu_lifecycle_call_ticket *,pangu_lifecycle_report *);
} pangu_lifecycle_child_api;
_Static_assert(sizeof(pangu_lifecycle_child_api)==32,"child extension");
_Static_assert(__builtin_offsetof(pangu_lifecycle_child_api,enter_child)==24,"child method");
#endif
#endif
