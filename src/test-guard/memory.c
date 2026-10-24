#include "src/bootstrap-api/memory.h"
extern pangu_u64 guard_syscall(pangu_u64,pangu_u64,pangu_u64,pangu_u64,pangu_u64,pangu_u64,pangu_u64);
pangu_i32 pangu_memory_map(void *context,pangu_u64 length,pangu_mapping *out)
{
 (void)context;
 if(!out || !length || length>4096) return 1;
 if(out->address || out->length) return 5;
 pangu_u64 base=guard_syscall(9,0,12288,3,34,~(pangu_u64)0,0);
 if(base>=~(pangu_u64)4094) return 8;
 if(guard_syscall(10,base,4096,0,0,0,0) || guard_syscall(10,base+8192,4096,0,0,0,0)) {
  guard_syscall(11,base,12288,0,0,0,0);return 8;
 }
 out->address=(void *)(base+4096);out->length=4096;return 0;
}
pangu_i32 pangu_memory_unmap(void *context,pangu_mapping *mapping)
{
 (void)context;
 if(!mapping || !mapping->address || mapping->length!=4096 || ((pangu_u64)mapping->address&4095)) return 1;
 if(guard_syscall(11,(pangu_u64)mapping->address-4096,12288,0,0,0,0)) return 8;
 mapping->address=(void *)0;mapping->length=0;return 0;
}
