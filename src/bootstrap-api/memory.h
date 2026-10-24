#ifndef PANGU_MEMORY_H
#define PANGU_MEMORY_H
#include "src/bootstrap-api/bootstrap.h"
#define PANGU_MEMORY_ABI_VERSION 1
#define PANGU_MEMORY_STRUCT_SIZE 40
#ifndef __ASSEMBLER__
/* Exact owned range, borrowed to unmap. Not a security handle. Do not forge/copy ownership. */
typedef struct pangu_mapping { void *address; pangu_u64 length; } pangu_mapping;
typedef pangu_i32 (*pangu_map_fn)(void *, pangu_u64, pangu_mapping *);
typedef pangu_i32 (*pangu_unmap_fn)(void *, pangu_mapping *);
typedef struct pangu_memory_api {
 pangu_u32 abi_version, struct_size;
 void *context;
 pangu_map_fn map;
 pangu_unmap_fn unmap;
 pangu_u64 reserved;
} pangu_memory_api;
_Static_assert(sizeof(pangu_mapping)==16,"mapping size");
_Static_assert(__builtin_offsetof(pangu_mapping,length)==8,"mapping length");
_Static_assert(sizeof(pangu_memory_api)==40,"memory API size");
_Static_assert(__builtin_offsetof(pangu_memory_api,map)==16,"map offset");
_Static_assert(__builtin_offsetof(pangu_memory_api,unmap)==24,"unmap offset");
/* Provider symbols used only by composition. Consumers call the injected table.
 * 0=OK, 1=invalid, 2=no memory, 5=output occupied, 8=OS error.
 * map requires zeroed output; overflow is invalid. unmap clears only on success.
 * Caller must provide the original range from this provider, with valid storage.
 */
pangu_i32 pangu_memory_map(void *,pangu_u64,pangu_mapping *);
pangu_i32 pangu_memory_unmap(void *,pangu_mapping *);
#endif
#endif
