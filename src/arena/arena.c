#include "src/bootstrap-api/arena.h"
static int active(const pangu_arena *a)
{
 return a && a->abi_version==1 && a->struct_size==56 && a->memory && a->region.address;
}
static pangu_u64 current_epoch(void *context)
{
 pangu_arena *a=context;
 return active(a) ? a->epoch : 0;
}
static pangu_i32 allocate(void *context,pangu_u64 size,pangu_u64 alignment,void **out)
{
 pangu_arena *a=context;
 if(!out) return 1;
 *out=(void *)0;
 if(!active(a)) return 10;
 if(!size || !alignment || alignment>4096 || (alignment & (alignment-1))) return 1;
 pangu_u64 base=(pangu_u64)a->region.address;
 if(a->used>a->capacity || base>~(pangu_u64)0-a->used) return 11;
 pangu_u64 next=base+a->used;
 if(next>~(pangu_u64)0-(alignment-1)) return 11;
 pangu_u64 aligned=(next+alignment-1)&~(alignment-1);
 pangu_u64 offset=aligned-base;
 if(offset>a->capacity || size>a->capacity-offset) return 2;
 *out=(void *)aligned;
 a->used=offset+size;
 return 0;
}
pangu_i32 pangu_arena_init(pangu_arena *a,const pangu_memory_api *memory,pangu_u64 capacity)
{
 if(!a || !memory || !capacity) return 1;
 if(memory->abi_version!=1 || memory->struct_size!=40 || memory->reserved || !memory->map || !memory->unmap) return 1;
 if(a->memory || a->region.address || a->region.length || a->used || a->capacity) return 5;
 if(!((a->abi_version==0 && a->struct_size==0) || (a->abi_version==1 && a->struct_size==56))) return 1;
 if(a->epoch==~(pangu_u64)0) return 11;
 pangu_mapping mapping;
 mapping.address=(void *)0;mapping.length=0;
 pangu_i32 status=memory->map(memory->context,capacity,&mapping);
 if(status) return status;
 /* A valid provider guarantees a non-NULL owned range of at least capacity. */
 a->abi_version=1;a->struct_size=56;a->memory=memory;
 a->region.address=mapping.address;a->region.length=mapping.length;
 a->used=0;a->capacity=capacity;a->epoch++;
 return 0;
}
pangu_i32 pangu_arena_reset(pangu_arena *a)
{
 if(!active(a)) return 10;
 if(a->epoch==~(pangu_u64)0) return 11;
 a->used=0;a->epoch++;
 return 0;
}
pangu_i32 pangu_arena_dispose(pangu_arena *a)
{
 if(!active(a)) return 10;
 pangu_i32 status=a->memory->unmap(a->memory->context,&a->region);
 if(status) return status;
 a->memory=(void *)0;a->used=0;a->capacity=0;
 return 0;
}
pangu_i32 pangu_arena_allocator(pangu_arena *a,pangu_region_allocator *out)
{
 if(!out) return 1;
 if(!active(a)) return 10;
 out->abi_version=1;out->struct_size=40;out->context=a;
 out->allocate=allocate;out->epoch=current_epoch;out->reserved=0;
 return 0;
}
