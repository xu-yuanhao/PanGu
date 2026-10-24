#include "src/bootstrap-api/json.h"
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static int range_ok(const void *p,pangu_u64 n)
{
 return (!n || p) && n<=~(pangu_u64)0-(pangu_u64)p;
}
static unsigned char short_escape(unsigned char b)
{
 switch(b) {
 case 8:return 'b';case 9:return 't';case 10:return 'n';case 12:return 'f';case 13:return 'r';
 default:return 0;
 }
}
pangu_i32 pangu_json_quote_utf8(pangu_text_slice s,unsigned char *out,pangu_u64 capacity,pangu_u64 *length)
{
 if(!length || !range_ok(s.data,s.length) || !range_ok(out,capacity)
  || overlap((pangu_u64)out,capacity,(pangu_u64)s.data,s.length)
  || overlap((pangu_u64)length,8,(pangu_u64)s.data,s.length)
  || overlap((pangu_u64)length,8,(pangu_u64)out,capacity)) return 1;
 *length=0;
 pangu_u64 scalars,error;pangu_i32 rc=pangu_text_validate(s,&scalars,&error);if(rc) return rc;
 pangu_u64 needed=2;
 for(pangu_u64 i=0;i<s.length;i++) {
  unsigned char b=s.data[i];pangu_u64 width=1;
  if(b=='"' || b=='\\') width=2;
  else if(b<0x20) width=short_escape(b) ? 2 : 6;
  if(needed>~(pangu_u64)0-width) return 11;
  needed+=width;
 }
 *length=needed;if(needed>capacity) return 2;
 static const unsigned char hex[]="0123456789abcdef";
 pangu_u64 at=0;out[at++]='"';
 for(pangu_u64 i=0;i<s.length;i++) {
  unsigned char b=s.data[i];
  if(b=='"' || b=='\\') { out[at++]='\\';out[at++]=b; }
  else if(b<0x20) {
   unsigned char escape=short_escape(b);out[at++]='\\';
   if(escape) out[at++]=escape;
   else { out[at++]='u';out[at++]='0';out[at++]='0';out[at++]=hex[b>>4];out[at++]=hex[b&15]; }
  } else out[at++]=b;
 }
 out[at]='"';return 0;
}
