#include "src/bootstrap-api/text.h"
pangu_i32 pangu_text_next(pangu_text_slice s,pangu_u64 *cursor,pangu_u32 *scalar)
{
 if(!cursor || !scalar || (!s.data && s.length) || *cursor>s.length) return 1;
 pangu_u64 i=*cursor;
 if(i==s.length) return 3;
 unsigned char lead=s.data[i];
 pangu_u32 value,width,minimum;
 if(lead<0x80) { value=lead;width=1;minimum=0; }
 else if(lead>=0xc2 && lead<=0xdf) { value=lead&0x1f;width=2;minimum=0x80; }
 else if(lead>=0xe0 && lead<=0xef) { value=lead&0xf;width=3;minimum=0x800; }
 else if(lead>=0xf0 && lead<=0xf4) { value=lead&7;width=4;minimum=0x10000; }
 else return 12;
 if(s.length-i<width) return 12;
 for(pangu_u32 n=1;n<width;n++) {
  unsigned char b=s.data[i+n];
  if((b&0xc0)!=0x80) return 12;
  value=(value<<6)|(b&0x3f);
 }
 if(value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return 12;
 *cursor=i+width;*scalar=value;return 0;
}
pangu_i32 pangu_text_validate(pangu_text_slice s,pangu_u64 *count,pangu_u64 *error_offset)
{
 if(!count || !error_offset || count==error_offset || (!s.data && s.length)) return 1;
 pangu_u64 cursor=0,n=0; pangu_u32 scalar;
 while(cursor<s.length) {
  pangu_u64 start=cursor;
  pangu_i32 rc=pangu_text_next(s,&cursor,&scalar);
  if(rc) { *count=n;*error_offset=start;return rc; }
  n++;
 }
 *count=n;*error_offset=s.length;return 0;
}
pangu_i32 pangu_text_append(pangu_buffer *buffer,pangu_text_slice s)
{
 pangu_u64 count,error_offset;
 pangu_i32 rc=pangu_text_validate(s,&count,&error_offset);
 if(rc) return rc;
 return pangu_buffer_append(buffer,s.data,s.length);
}
