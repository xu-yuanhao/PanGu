#include "src/bootstrap-api/pi-arguments.h"
#define NAN_BITS 0x7ff8000000000000ULL
#define INF_BITS 0x7ff0000000000000ULL
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static int whitespace(pangu_u32 c)
{
 return (c>=9 && c<=13) || c==32 || c==0xa0 || c==0x1680 || (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000 || c==0xfeff;
}
static int digit(unsigned char c) { return c>='0' && c<='9'; }
static pangu_i32 radix_digit(unsigned char c)
{
 if(digit(c)) return c-'0';
 if(c>='a' && c<='f') return c-'a'+10;
 if(c>='A' && c<='F') return c-'A'+10;
 return -1;
}
static pangu_u64 radix(pangu_text_slice input,pangu_u64 start,pangu_u64 end,pangu_u32 width)
{
 if(start==end) return NAN_BITS;
 pangu_u32 total=0,retained=0,sticky=0;pangu_u64 top=0;
 for(pangu_u64 i=start;i<end;i++) {
  pangu_i32 d=radix_digit(input.data[i]);if(d<0 || (pangu_u32)d>=((pangu_u32)1<<width)) return NAN_BITS;
  for(pangu_u32 j=width;j>0;j--) {
   pangu_u32 bit=((pangu_u32)d>>(j-1))&1;
   if(!total && !bit) continue;
   if(total<1025) total++;
   if(retained<54) { top=(top<<1)|bit;retained++; }
   else sticky|=bit;
  }
 }
 if(!total) return 0;
 if(total>1024) return INF_BITS;
 pangu_u64 significand;
 if(total<=53) significand=top<<(53-total);
 else {
  significand=top>>1;
  if((top&1) && (sticky || (significand&1))) significand++;
  if(significand==((pangu_u64)1<<53)) { significand>>=1;total++; }
 }
 if(total>1024) return INF_BITS;
 return ((pangu_u64)(total+1022)<<52)|(significand&0xfffffffffffffULL);
}
static pangu_i32 decimal(pangu_text_slice input,pangu_u64 start,pangu_u64 end,unsigned char *scratch,pangu_u64 capacity,pangu_u64 *bits)
{
 pangu_u64 p=start;pangu_u32 negative=0;
 if(p<end && (input.data[p]=='+' || input.data[p]=='-')) { negative=input.data[p]=='-';p++; }
 pangu_u64 integer=p;while(p<end && digit(input.data[p])) p++;
 pangu_u64 integer_end=p,fraction=p,fraction_end=p;pangu_u32 dot=0;
 if(p<end && input.data[p]=='.') { dot=1;p++;fraction=p;while(p<end && digit(input.data[p])) p++;fraction_end=p; }
 if(integer==integer_end && fraction==fraction_end) { *bits=NAN_BITS;return 0; }
 pangu_u64 exponent=p;
 if(p<end && (input.data[p]=='e' || input.data[p]=='E')) {
  p++;if(p<end && (input.data[p]=='+' || input.data[p]=='-')) p++;
  pangu_u64 first=p;while(p<end && digit(input.data[p])) p++;
  if(first==p) { *bits=NAN_BITS;return 0; }
 }
 if(p!=end) { *bits=NAN_BITS;return 0; }
 while(integer<integer_end && input.data[integer]=='0') integer++;
 pangu_u64 integers=integer_end-integer;if(!integers) integers=1;
 pangu_u64 fractions=fraction_end-fraction;if(dot && !fractions) fractions=1;
 /* Every term is <= source length; bound before addition without overflowing. */
 pangu_u64 need=negative;
 if(integers>1048576-need) return 2;
 need+=integers;
 if(dot) { if(need==1048576 || fractions>1048575-need) return 2;need+=1+fractions; }
 if(end-exponent>1048576-need) return 2;
 need+=end-exponent;
 if(need>capacity) return 2;
 pangu_u64 n=0;if(negative) scratch[n++]='-';
 if(integer==integer_end) scratch[n++]='0';else for(pangu_u64 i=integer;i<integer_end;i++) scratch[n++]=input.data[i];
 if(dot) { scratch[n++]='.';if(fraction==fraction_end) scratch[n++]='0';else for(pangu_u64 i=fraction;i<fraction_end;i++) scratch[n++]=input.data[i]; }
 for(pangu_u64 i=exponent;i<end;i++) scratch[n++]=input.data[i];
 pangu_json_parser parser;volatile unsigned char *zero=(volatile unsigned char *)&parser;
 for(pangu_u32 i=0;i<sizeof(parser);i++) zero[i]=0;
 pangu_json_token token;pangu_json_result result;pangu_json_limits limits={1048576,1,0};pangu_text_slice number={scratch,n};
 pangu_i32 rc=pangu_json_init(&parser,&token,1,limits);if(rc) return rc;
 rc=pangu_json_feed(&parser,number,1);if(rc) return rc;
 rc=pangu_json_get_result(&parser,&result);if(rc) return rc;
 return pangu_json_number_bits(number,&result,0,bits);
}
pangu_i32 pangu_pi_number_string(pangu_text_slice input,unsigned char *scratch,pangu_u64 capacity,pangu_u64 max_bytes,pangu_u64 *bits)
{
 if(!bits || (!input.data && input.length) || (!scratch && capacity) || input.length>~(pangu_u64)0-(pangu_u64)input.data || capacity>~(pangu_u64)0-(pangu_u64)scratch || 8>~(pangu_u64)0-(pangu_u64)bits) return 1;
 if(overlap((pangu_u64)bits,8,(pangu_u64)input.data,input.length) || overlap((pangu_u64)bits,8,(pangu_u64)scratch,capacity) || overlap((pangu_u64)scratch,capacity,(pangu_u64)input.data,input.length)) return 1;
 if(input.length>max_bytes) return 2;
 pangu_u64 at=0,start=input.length,end=0;
 while(at<input.length) { pangu_u64 before=at;pangu_u32 scalar;pangu_i32 rc=pangu_text_next(input,&at,&scalar);if(rc) return rc;if(!whitespace(scalar)) { if(start==input.length) start=before;end=at; } }
 if(start==input.length) { *bits=0;return 0; }
 pangu_u64 p=start,sign=0;if(input.data[p]=='+' || input.data[p]=='-') { sign=input.data[p]=='-' ? ((pangu_u64)1<<63) : 0;p++; }
 static const unsigned char infinity[]="Infinity";
 if(end-p==8) { pangu_u32 same=1;for(pangu_u32 i=0;i<8;i++) if(input.data[p+i]!=infinity[i]) same=0;if(same) { *bits=sign|INF_BITS;return 0; } }
 if(end-start>=2 && input.data[start]=='0') {
  unsigned char c=input.data[start+1];pangu_u32 width=c=='x' || c=='X' ? 4 : c=='o' || c=='O' ? 3 : c=='b' || c=='B' ? 1 : 0;
  if(width) { *bits=radix(input,start+2,end,width);return 0; }
 }
 pangu_u64 value;pangu_i32 rc=decimal(input,start,end,scratch,capacity,&value);if(!rc) *bits=value;return rc;
}
