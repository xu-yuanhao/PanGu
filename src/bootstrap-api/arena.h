#ifndef PANGU_ARENA_H
#define PANGU_ARENA_H
#include "src/bootstrap-api/memory.h"
#include "src/bootstrap-api/region.h"
#ifndef __ASSEMBLER__
/* Zero-init before first use; retain storage while borrowed allocator tables exist.
 * Never alias output pointers with arena state. Single-thread, non-reentrant.
 * All allocations borrow this scope: no per-block free. Reset invalidates all.
 */
typedef struct pangu_arena {
 pangu_u32 abi_version,struct_size;
 const pangu_memory_api *memory;
 pangu_mapping region;
 pangu_u64 used,capacity,epoch;
} pangu_arena;
_Static_assert(sizeof(pangu_arena)==56,"arena layout");
_Static_assert(sizeof(pangu_region_allocator)==40,"allocator layout");
_Static_assert(__builtin_offsetof(pangu_arena,epoch)==48,"epoch offset");
/* Status 0 OK, 1 invalid, 2 capacity exhausted, 5 busy, 10 inactive, 11 overflow.
 * Backend map/unmap errors propagate; dispose failure preserves active ownership.
 * Epoch 0 means inactive; nonzero epoch never wraps or resets on re-init.
 */
pangu_i32 pangu_arena_init(pangu_arena *,const pangu_memory_api *,pangu_u64);
pangu_i32 pangu_arena_reset(pangu_arena *);
pangu_i32 pangu_arena_dispose(pangu_arena *);
pangu_i32 pangu_arena_allocator(pangu_arena *,pangu_region_allocator *);
#endif
#endif
