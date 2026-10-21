#include "src/bootstrap-api/buffer.h"
void pangu_buffer_move(void *,const void *,pangu_u64);
static int live(const pangu_buffer *b)
{
 return b && b->abi_version==1 && b->struct_size==64 && b->allocator && b->epoch &&
  b->epoch==b->allocator->epoch(b->allocator->context) && b->length<=b->capacity &&
  b->capacity<=b->limit && ((b->capacity && b->data) || (!b->capacity && !b->data));
}
pangu_i32 pangu_buffer_init(pangu_buffer *b,const pangu_region_allocator *a,pangu_u64 limit)
{
 if(!b || !a || !limit) return 1;
 if(a->abi_version!=1 || a->struct_size!=40 || a->reserved || !a->allocate || !a->epoch) return 1;
 if(b->allocator || b->data || b->length || b->capacity || b->limit || b->epoch) return 5;
 if(!((b->abi_version==0 && b->struct_size==0) || (b->abi_version==1 && b->struct_size==64))) return 1;
 if(b->revision==~(pangu_u64)0) return 11;
 pangu_u64 epoch=a->epoch(a->context);
 if(!epoch) return 10;
 b->abi_version=1;b->struct_size=64;b->allocator=a;b->data=(void *)0;
 b->length=0;b->capacity=0;b->limit=limit;b->epoch=epoch;b->revision++;
 return 0;
}
pangu_i32 pangu_buffer_append(pangu_buffer *b,const void *source,pangu_u64 size)
{
 if(!live(b)) return 10;
 if(!size) return 0;
 if(!source) return 1;
 if(size>~(pangu_u64)0-b->length || b->revision==~(pangu_u64)0) return 11;
 pangu_u64 needed=b->length+size;
 if(needed>b->limit) return 2;
 if(b->data && (pangu_u64)source>=(pangu_u64)b->data && (pangu_u64)source-(pangu_u64)b->data<b->capacity) {
  pangu_u64 offset=(pangu_u64)source-(pangu_u64)b->data;
  if(offset>b->length || size>b->length-offset) return 1;
 }
 unsigned char *destination=b->data;
 pangu_u64 capacity=b->capacity;
 if(needed>capacity) {
  capacity=capacity ? capacity : (b->limit<16 ? b->limit : 16);
  while(capacity<needed) {
   if(capacity>b->limit/2) { capacity=b->limit; break; }
   capacity*=2;
  }
  void *allocated=(void *)0;
  pangu_i32 status=b->allocator->allocate(b->allocator->context,capacity,1,&allocated);
  if(status) return status;
  destination=allocated;
  /* Provider contract guarantees a fresh valid region on success. */
  pangu_buffer_move(destination,b->data,b->length);
 }
 pangu_buffer_move(destination+b->length,source,size);
 b->data=destination;b->capacity=capacity;b->length=needed;b->revision++;
 return 0;
}
pangu_i32 pangu_buffer_clear(pangu_buffer *b)
{
 if(!live(b)) return 10;
 if(b->revision==~(pangu_u64)0) return 11;
 b->length=0;b->revision++;
 return 0;
}
pangu_i32 pangu_buffer_dispose(pangu_buffer *b)
{
 if(!b) return 1;
 if(b->abi_version!=1 || b->struct_size!=64) return 10;
 if(!b->allocator && !b->data) return 0;
 b->allocator=(void *)0;b->data=(void *)0;b->length=0;b->capacity=0;b->limit=0;b->epoch=0;
 if(b->revision!=~(pangu_u64)0) b->revision++;
 return 0;
}
pangu_i32 pangu_buffer_view(const pangu_buffer *b,pangu_buffer_borrow *out)
{
 if(!out) return 1;
 out->data=(void *)0;out->length=0;out->owner=(void *)0;out->epoch=0;out->revision=0;
 if(!live(b)) return 10;
 out->data=b->data;out->length=b->length;out->owner=b;out->epoch=b->epoch;out->revision=b->revision;
 return 0;
}
pangu_i32 pangu_buffer_view_validate(const pangu_buffer_borrow *view)
{
 if(!view || !view->owner) return 1;
 const pangu_buffer *b=view->owner;
 if(!live(b) || view->epoch!=b->epoch || view->revision!=b->revision || view->data!=b->data || view->length!=b->length) return 10;
 return 0;
}
