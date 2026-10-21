#ifndef PANGU_REGION_H
#define PANGU_REGION_H
#include "src/bootstrap-api/bootstrap.h"
#ifndef __ASSEMBLER__
/* Generic scoped allocator contract; no arena or platform state dependency. */
typedef pangu_i32 (*pangu_region_alloc_fn)(void *,pangu_u64,pangu_u64,void **);
typedef pangu_u64 (*pangu_region_epoch_fn)(void *);
typedef struct pangu_region_allocator {
 pangu_u32 abi_version,struct_size;
 void *context;
 pangu_region_alloc_fn allocate;
 pangu_region_epoch_fn epoch;
 pangu_u64 reserved;
} pangu_region_allocator;
_Static_assert(sizeof(pangu_region_allocator)==40,"region allocator layout");
#endif
#endif
