#include "src/bootstrap-api/buffer.h"
#include "src/bootstrap-api/arena.h"
static pangu_arena arena;
static pangu_region_allocator allocator;
static pangu_buffer buffer;
static pangu_region_allocator proxy;
static int fail_allocation;
static unsigned char payload[1024];
/* Test-only fault adapter over the real registered allocator. */
static pangu_i32 proxy_allocate(void *context,pangu_u64 size,pangu_u64 align,void **out)
{
 const pangu_region_allocator *a=context;
 if(fail_allocation) { *out=(void *)0;return 2; }
 return a->allocate(a->context,size,align,out);
}
static pangu_u64 proxy_epoch(void *context)
{
 const pangu_region_allocator *a=context;
 return a->epoch(a->context);
}
int buffer_test(const pangu_memory_api *memory)
{
 pangu_buffer_borrow view;
 if(pangu_arena_init(&arena,memory,4096) || pangu_arena_allocator(&arena,&allocator)) return 1;
 proxy.abi_version=1;proxy.struct_size=40;proxy.context=&allocator;
 proxy.allocate=proxy_allocate;proxy.epoch=proxy_epoch;proxy.reserved=0;
 if(pangu_buffer_init(&buffer,&proxy,64)) return 2;
 if(pangu_buffer_append(&buffer,(void *)0,0)) return 3;
 if(pangu_buffer_append(&buffer,"abc",3) || buffer.length!=3) return 4;
 if(pangu_buffer_view(&buffer,&view) || pangu_buffer_view_validate(&view)) return 5;
 if(pangu_buffer_append(&buffer,view.data,3) || buffer.length!=6 || buffer.data[3]!='a') return 6;
 if(pangu_buffer_view_validate(&view)!=10) return 7;
 if(pangu_buffer_append(&buffer,"\0x",2) || buffer.data[6]!=0 || buffer.data[7]!='x') return 8;
 pangu_u64 used=arena.used, revision=buffer.revision;
 if(pangu_buffer_append(&buffer,"x",~(pangu_u64)0)!=11 || buffer.revision!=revision || arena.used!=used) return 9;
 if(pangu_buffer_append(&buffer,"x",65)!=2 || buffer.length!=8 || arena.used!=used) return 10;
 if(pangu_buffer_append(&buffer,(void *)0,1)!=1) return 11;
 if(pangu_buffer_append(&buffer,buffer.data+8,1)!=1) return 12;
 if(pangu_buffer_append(&buffer,buffer.data,8) || buffer.length!=16) return 13;
 if(pangu_buffer_view(&buffer,&view)) return 14;
 if(pangu_buffer_append(&buffer,buffer.data,16) || buffer.length!=32 || pangu_buffer_view_validate(&view)!=10) return 15;
 if(pangu_buffer_append(&buffer,buffer.data,32) || buffer.length!=64) return 16;
 for(pangu_u32 i=0;i<8;i++) if(buffer.data[i*8]!='a' || buffer.data[i*8+6]!=0 || buffer.data[i*8+7]!='x') return 17;
 used=arena.used;
 if(pangu_buffer_clear(&buffer) || buffer.length || buffer.capacity<64 || arena.used!=used) return 19;
 if(pangu_buffer_append(&buffer,"z",1) || buffer.data[0]!='z') return 20;
 if(pangu_buffer_dispose(&buffer) || arena.used!=used || pangu_buffer_dispose(&buffer)) return 21;
 if(pangu_buffer_init(&buffer,&proxy,1024) || pangu_buffer_append(&buffer,"q",1)) return 22;
 if(pangu_buffer_view(&buffer,&view)) return 23;
 revision=buffer.revision;used=arena.used;
 pangu_u64 before_capacity=buffer.capacity;
 const unsigned char *before_data=buffer.data;
 if(before_capacity>=1024) return 35;
 fail_allocation=1;
 if(pangu_buffer_append(&buffer,payload,before_capacity)!=2 || buffer.length!=1 || buffer.data!=before_data || buffer.capacity!=before_capacity || buffer.data[0]!='q' || buffer.revision!=revision || arena.used!=used || pangu_buffer_view_validate(&view)) return 24;
 fail_allocation=0;
 if(pangu_arena_reset(&arena) || pangu_buffer_view_validate(&view)!=10 || pangu_buffer_append(&buffer,"x",1)!=10) return 25;
 if(pangu_buffer_dispose(&buffer) || pangu_buffer_init(&buffer,&proxy,1) || pangu_buffer_append(&buffer,"k",1)) return 26;
 if(buffer.capacity!=1 || buffer.length!=1 || pangu_buffer_append(&buffer,"x",1)!=2) return 27;
 if(pangu_buffer_view(&buffer,&view) || pangu_arena_dispose(&arena)) return 31;
 if(pangu_buffer_view_validate(&view)!=10 || pangu_buffer_append(&buffer,"x",1)!=10) return 32;
 if(pangu_buffer_dispose(&buffer)) return 33;
 if(pangu_arena_init(&arena,memory,128) || pangu_buffer_init(&buffer,&proxy,1) || pangu_buffer_append(&buffer,"k",1)) return 34;
 buffer.revision=~(pangu_u64)0;
 if(pangu_buffer_clear(&buffer)!=11 || pangu_buffer_append(&buffer,"x",1)!=11) return 28;
 if(pangu_buffer_dispose(&buffer) || pangu_buffer_init(&buffer,&proxy,1)!=11) return 29;
 if(pangu_arena_dispose(&arena)) return 30;
 return 0;
}
