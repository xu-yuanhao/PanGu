#include "src/bootstrap-api/arena.h"
static void empty(pangu_arena *a)
{
 a->abi_version=0;a->struct_size=0;a->memory=(void *)0;
 a->region.address=(void *)0;a->region.length=0;a->used=0;a->capacity=0;a->epoch=0;
}
int arena_test(const pangu_memory_api *memory)
{
 pangu_arena a; pangu_region_allocator allocator; void *p=(void *)0,*q=(void *)0;
 empty(&a);
 if(pangu_arena_init((void *)0,memory,128)!=1) return 1;
 if(pangu_arena_init(&a,memory,0)!=1 || a.memory) return 2;
 if(pangu_arena_init(&a,memory,(pangu_u64)1<<63)!=2 || a.memory || a.region.address || a.epoch) return 24;
 if(pangu_arena_init(&a,memory,128)!=0 || a.capacity!=128 || a.region.length<128) return 3;
 if(pangu_arena_init(&a,memory,128)!=5) return 4;
 if(pangu_arena_allocator(&a,&allocator)!=0 || allocator.epoch(allocator.context)!=1) return 5;
 if(allocator.allocate(allocator.context,8,8,&p)!=0 || (pangu_u64)p%8) return 6;
 if(allocator.allocate(allocator.context,8,16,&q)!=0 || (pangu_u64)q%16 || p==q) return 7;
 ((volatile unsigned char *)p)[0]=17;((volatile unsigned char *)q)[7]=29;
 pangu_u64 used=a.used;
 if(allocator.allocate(allocator.context,200,8,&q)!=2 || q || a.used!=used) return 8;
 if(allocator.allocate(allocator.context,~(pangu_u64)0,8,&q)!=2 || q || a.used!=used) return 9;
 if(allocator.allocate(allocator.context,0,8,&q)!=1 || q) return 10;
 if(allocator.allocate(allocator.context,1,3,&q)!=1 || q) return 11;
 if(allocator.allocate(allocator.context,1,0,&q)!=1 || q) return 12;
 if(allocator.allocate(allocator.context,1,8192,&q)!=1 || q) return 13;
 if(((volatile unsigned char *)p)[0]!=17) return 14;
 if(pangu_arena_reset(&a)!=0 || a.used || allocator.epoch(allocator.context)!=2) return 15;
 if(allocator.allocate(allocator.context,128,1,&q)!=0 || q!=p) return 16;

 for(pangu_u32 round=0;round<32;round++) {
  void *blocks[16];pangu_u64 lengths[16];pangu_u32 count=0;
  if(pangu_arena_reset(&a)!=0) return 25;
  pangu_u64 last=(pangu_u64)a.region.address;
  for(pangu_u32 i=0;i<16;i++) {
   pangu_u64 amount=(i*7+round*3)%17+1, align=(pangu_u64)1<<(i%5), before=a.used;
   pangu_i32 status=allocator.allocate(allocator.context,amount,align,&q);
   if(status==2) { if(q || a.used!=before) return 26; continue; }
   if(status || !q || (pangu_u64)q%align || (pangu_u64)q<last ||
       (pangu_u64)q<(pangu_u64)a.region.address ||
       (pangu_u64)q+amount>(pangu_u64)a.region.address+a.capacity) return 27;
   last=(pangu_u64)q+amount;blocks[count]=q;lengths[count]=amount;count++;
   ((volatile unsigned char *)q)[0]=(unsigned char)count;
   ((volatile unsigned char *)q)[amount-1]=(unsigned char)count;
   for(pangu_u32 j=0;j<count;j++) {
    volatile unsigned char *v=blocks[j];
    if(v[0]!=(unsigned char)(j+1) || v[lengths[j]-1]!=(unsigned char)(j+1)) return 28;
   }
  }
 }
 pangu_u64 saved_epoch=a.epoch,saved_used=a.used;
 a.epoch=~(pangu_u64)0;
 if(pangu_arena_reset(&a)!=11 || a.used!=saved_used) return 29;
 a.epoch=saved_epoch;
 pangu_u64 epoch=allocator.epoch(allocator.context);
 if(pangu_arena_dispose(&a)!=0 || a.region.address || a.region.length || a.memory) return 17;
 if(allocator.epoch(allocator.context)!=0) return 18;
 if(allocator.allocate(allocator.context,1,1,&q)!=10 || q) return 19;
 if(pangu_arena_dispose(&a)!=10) return 20;
 if(pangu_arena_init(&a,memory,64)!=0 || allocator.epoch(allocator.context)<=epoch) return 21;
 if(pangu_arena_dispose(&a)!=0) return 22;
 a.epoch=~(pangu_u64)0;
 if(pangu_arena_init(&a,memory,64)!=11 || a.memory) return 23;
 return 0;
}
