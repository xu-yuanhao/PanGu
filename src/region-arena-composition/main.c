#include "src/bootstrap-api/arena.h"
extern int pangu_region_consumer(const pangu_region_allocator *);
static pangu_arena arena;
static pangu_region_allocator allocator;
static pangu_memory_api tracked;
static const pangu_memory_api *original;
static pangu_u64 maps,unmaps,owned_bytes,owned_address;
/* Test-only ownership accounting; forwards to the actual platform provider. */
static pangu_i32 tracked_map(void *context,pangu_u64 length,pangu_mapping *out)
{
 if(context!=&tracked || owned_bytes) return 88;
 pangu_i32 rc=original->map(original->context,length,out);
 if(!rc) { maps++;owned_bytes=out->length;owned_address=(pangu_u64)out->address; }
 return rc;
}
static pangu_i32 tracked_unmap(void *context,pangu_mapping *mapping)
{
 if(context!=&tracked || !owned_bytes || (pangu_u64)mapping->address!=owned_address || mapping->length!=owned_bytes) return 89;
 pangu_i32 rc=original->unmap(original->context,mapping);
 if(!rc) { unmaps++;owned_bytes=0;owned_address=0; }
 return rc;
}
int region_arena_test(const pangu_memory_api *memory)
{
 original=memory;tracked.abi_version=1;tracked.struct_size=40;tracked.context=&tracked;
 tracked.map=tracked_map;tracked.unmap=tracked_unmap;tracked.reserved=0;
 if(pangu_arena_init(&arena,&tracked,4096) || pangu_arena_allocator(&arena,&allocator)) return 80;
 if(maps!=1 || unmaps || owned_bytes!=4096) return 82;
 int rc=pangu_region_consumer(&allocator);
 if(pangu_arena_dispose(&arena)) return 81;
 if(maps!=1 || unmaps!=1 || owned_bytes || owned_address || arena.memory || arena.region.address || arena.region.length) return 83;
 return rc;
}
