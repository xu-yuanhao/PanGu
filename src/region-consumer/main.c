#include "src/bootstrap-api/text.h"
static pangu_buffer buffer;
static pangu_buffer_borrow view;
static unsigned char expected[64];
/* The same object is linked unchanged into both allocator compositions. */
int pangu_region_consumer(const pangu_region_allocator *allocator)
{
 if(pangu_buffer_init(&buffer,allocator,64)) return 1;
 pangu_text_slice s;s.data=(const unsigned char *)"A\0\xc2\xa2";s.length=4;
 if(pangu_text_append(&buffer,s) || buffer.length!=4) return 2;
 for(pangu_u32 round=0;round<4;round++) {
  if(pangu_buffer_view(&buffer,&view)) return 3;
  s.data=view.data;s.length=view.length;
  if(pangu_text_append(&buffer,s) || pangu_buffer_view_validate(&view)!=10) return 4;
 }
 if(buffer.length!=64 || pangu_buffer_view(&buffer,&view)) return 5;
 for(pangu_u32 i=0;i<64;i++) expected[i]=buffer.data[i];
 pangu_u64 revision=buffer.revision,capacity=buffer.capacity;
 s.data=(const unsigned char *)"x";s.length=1;
 if(pangu_text_append(&buffer,s)!=2 || buffer.revision!=revision || buffer.capacity!=capacity || pangu_buffer_view_validate(&view)) return 6;
 for(pangu_u32 i=0;i<64;i++) if(buffer.data[i]!=expected[i]) return 7;
 s.data=buffer.data;s.length=buffer.length;pangu_u64 count,error;
 if(pangu_text_validate(s,&count,&error) || count!=48 || error!=64) return 8;
 for(pangu_u32 i=0;i<10000;i++) {
  if(pangu_buffer_clear(&buffer)) return 9;
  s.data=(const unsigned char *)"A\0\xc2\xa2";s.length=4;
  if(pangu_text_append(&buffer,s) || buffer.capacity!=capacity) return 10;
 }
 if(pangu_buffer_dispose(&buffer) || pangu_buffer_view_validate(&view)!=10) return 11;
 return 0;
}
