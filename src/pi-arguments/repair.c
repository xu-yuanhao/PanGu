#include "src/bootstrap-api/pi-arguments.h"
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static int hex(unsigned char c)
{
 return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}
static int valid_escape(unsigned char c)
{
 return c=='"' || c=='\\' || c=='/' || c=='b' || c=='f' || c=='n' || c=='r' || c=='t';
}
/* First pass measures, second pass emits. The caller's immutable input makes
 * branch decisions identical, with no failure after output mutation starts. */
static pangu_i32 transform(pangu_text_slice input,unsigned char *out,pangu_u64 *length)
{
 pangu_u64 count=0;pangu_u32 in_string=0;
 for(pangu_u64 i=0;i<input.length;i++) {
  unsigned char c=input.data[i],bytes[6];pangu_u64 n=1;bytes[0]=c;
  if(!in_string) { if(c=='"') in_string=1; }
  else if(c=='"') in_string=0;
  else if(c=='\\') {
   if(i+1<input.length && input.data[i+1]=='u' && input.length-i>=6 && hex(input.data[i+2]) && hex(input.data[i+3]) && hex(input.data[i+4]) && hex(input.data[i+5])) {
    n=6;for(pangu_u64 j=1;j<6;j++) bytes[j]=input.data[i+j];i+=5;
   } else if(i+1<input.length && valid_escape(input.data[i+1])) { n=2;bytes[1]=input.data[++i]; }
   else { n=2;bytes[1]='\\'; }
  } else if(c<=31) {
   n=2;bytes[0]='\\';
   if(c==8) bytes[1]='b';
   else if(c==12) bytes[1]='f';
   else if(c==10) bytes[1]='n';
   else if(c==13) bytes[1]='r';
   else if(c==9) bytes[1]='t';
   else { const unsigned char *digits=(const unsigned char *)"0123456789abcdef";n=6;bytes[1]='u';bytes[2]='0';bytes[3]='0';bytes[4]=digits[c>>4];bytes[5]=digits[c&15]; }
  }
  if(n>~(pangu_u64)0-count) return 2;
  if(out) for(pangu_u64 j=0;j<n;j++) out[count+j]=bytes[j];
  count+=n;
 }
 *length=count;return 0;
}
pangu_i32 pangu_pi_repair_utf8(pangu_text_slice input,unsigned char *out,pangu_u64 capacity,pangu_u64 max_bytes,pangu_u64 *written)
{
 if(!written || (!input.data && input.length) || (!out && capacity) || input.length>~(pangu_u64)0-(pangu_u64)input.data || capacity>~(pangu_u64)0-(pangu_u64)out || sizeof(*written)>~(pangu_u64)0-(pangu_u64)written) return 1;
 if(overlap((pangu_u64)written,8,(pangu_u64)input.data,input.length) || overlap((pangu_u64)written,8,(pangu_u64)out,capacity) || overlap((pangu_u64)out,capacity,(pangu_u64)input.data,input.length)) return 1;
 if(input.length>max_bytes) return 2;
 pangu_u64 scalars,error_offset,need;pangu_i32 rc=pangu_text_validate(input,&scalars,&error_offset);if(rc) return rc;
 rc=transform(input,(void *)0,&need);if(rc) return rc;
 if(out && capacity<need) return 2;
 if(out) { pangu_u64 emitted;rc=transform(input,out,&emitted);if(rc) return rc; }
 *written=need;return 0;
}
