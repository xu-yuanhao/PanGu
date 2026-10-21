#ifndef PANGU_BOOTSTRAP_H
#define PANGU_BOOTSTRAP_H

/* Shared SysV x86_64 contract. No runtime, platform or provider dependency. */
#define PANGU_PLATFORM_ABI_VERSION 1
#define PANGU_PLATFORM_STRUCT_SIZE 32
#define PANGU_BOOTSTRAP_INVALID -1
#define PANGU_BOOTSTRAP_EXIT_RETURNED -2

#ifndef __ASSEMBLER__
typedef unsigned int pangu_u32;
typedef unsigned long long pangu_u64;
typedef int pangu_i32;
typedef void (*pangu_exit_fn)(void *context, pangu_i32 status);
typedef struct pangu_platform_api {
    pangu_u32 abi_version;
    pangu_u32 struct_size;
    void *context;                 /* borrowed; valid through exit callback */
    pangu_exit_fn exit_process;     /* borrowed callback; must not return */
    pangu_u64 reserved;             /* must be zero */
} pangu_platform_api;
_Static_assert(sizeof(pangu_i32) == 4, "status width");
_Static_assert(sizeof(pangu_platform_api) == PANGU_PLATFORM_STRUCT_SIZE, "platform layout");
_Static_assert(__builtin_offsetof(pangu_platform_api, context) == 8, "context offset");
_Static_assert(__builtin_offsetof(pangu_platform_api, exit_process) == 16, "callback offset");
_Static_assert(__builtin_offsetof(pangu_platform_api, reserved) == 24, "reserved offset");
/* Returns only on invalid input or a misbehaving exit callback. No ownership transfer. */
pangu_i32 pangu_bootstrap_run(const pangu_platform_api *platform, pangu_i32 status);
#endif
#endif
