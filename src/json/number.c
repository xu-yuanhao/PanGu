#include "src/bootstrap-api/json.h"
/* 160*32 bits cover 800 retained decimal digits, a denominator up to
 * 10^1123, and every binary64 midpoint comparison (at most 4755 bits).
 * A binary64 rounding midpoint is an integer times 2^e, -1075<=e<=970.
 * Its terminating decimal has at most 768 significant digits; hence retaining
 * 800 input digits plus a sticky tail cannot jump across an unseen midpoint.
 */
#define LIMBS 160
#define DIGITS 800
#define NUMBER_BYTES 1048576
#define EXP_LIMIT 2097152
typedef struct big { pangu_u32 used,word[LIMBS]; } big;
static void small(big *a,pangu_u32 n) { a->used=1;a->word[0]=n; }
static void copy(big *a,const big *b)
{
 a->used=b->used;for(pangu_u32 i=0;i<b->used;i++) a->word[i]=b->word[i];
}
static int mul(big *a,pangu_u64 m)
{
 pangu_u64 carry=0;
 for(pangu_u32 i=0;i<a->used;i++) {
  pangu_u64 low=(pangu_u64)a->word[i]*(pangu_u32)m+(pangu_u32)carry;
  carry=(pangu_u64)a->word[i]*(m>>32)+(carry>>32)+(low>>32);
  a->word[i]=(pangu_u32)low;
 }
 while(carry) {
  if(a->used==LIMBS) return 11;
  a->word[a->used++]=(pangu_u32)carry;carry>>=32;
 }
 return 0;
}
static int add_digit(big *a,pangu_u32 n)
{
 if(mul(a,10)) return 11;
 pangu_u64 carry=n;
 for(pangu_u32 i=0;i<a->used && carry;i++) {
  carry+=(pangu_u64)a->word[i];a->word[i]=(pangu_u32)carry;carry>>=32;
 }
 if(carry) { if(a->used==LIMBS) return 11;a->word[a->used++]=(pangu_u32)carry; }
 return 0;
}
static int shift(big *a,pangu_u32 bits)
{
 pangu_u32 whole=bits/32,part=bits%32,old=a->used;
 pangu_u32 top=part ? a->word[old-1]>>(32-part) : 0;
 if(old+whole+(top!=0)>LIMBS) return 11;
 for(pangu_u32 i=old;i>0;i--) {
  pangu_u32 n=i-1,value=a->word[n]<<part;
  if(part && n) value|=a->word[n-1]>>(32-part);
  a->word[n+whole]=value;
 }
 for(pangu_u32 i=0;i<whole;i++) a->word[i]=0;
 a->used=old+whole;
 if(top) a->word[a->used++]=top;
 return 0;
}
static int compare(const big *a,const big *b)
{
 if(a->used!=b->used) return a->used<b->used ? -1 : 1;
 for(pangu_u32 i=a->used;i>0;i--) if(a->word[i-1]!=b->word[i-1]) return a->word[i-1]<b->word[i-1] ? -1 : 1;
 return 0;
}
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static int range_ok(const void *p,pangu_u64 n)
{
 return (!n || p) && n<=~(pangu_u64)0-(pangu_u64)p;
}
typedef struct decimal_input { big numerator;pangu_u32 seen,kept,sticky; } decimal_input;
static int digit(decimal_input *d,unsigned char b)
{
 pangu_u32 n=b-'0';
 if(!d->seen && !n) return 0;
 d->seen++;
 if(d->kept<DIGITS) { d->kept++;return add_digit(&d->numerator,n); }
 if(n) d->sticky=1;
 return 0;
}
static int is_digit(unsigned char b) { return b>='0' && b<='9'; }
/* Compare N/D against the midpoint following a nonnegative finite bit pattern.
 * Infinities are represented only by the binary-search sentinel, never here.
 */
static int midpoint(const big *n,const big *d,pangu_u64 bits,big *a,big *b,int *comparison)
{
 pangu_u32 biased=(pangu_u32)(bits>>52);pangu_u64 m=bits&0xfffffffffffffULL;
 pangu_i32 e=-1074;
 if(biased) { m|=0x10000000000000ULL;e=(pangu_i32)biased-1075; }
 m=m*2+1;e--;
 copy(a,n);copy(b,d);
 if(mul(b,m)) return 11;
 if(e<0) { if(shift(a,(pangu_u32)-e)) return 11; }
 else if(shift(b,(pangu_u32)e)) return 11;
 *comparison=compare(a,b);return 0;
}
pangu_i32 pangu_json_number_bits(pangu_text_slice s,const pangu_json_result *r,pangu_u64 index,pangu_u64 *out)
{
 if(!out || !r || !range_ok(s.data,s.length) || !range_ok(out,8)) return 1;
 if(r->status || r->reserved || !r->tokens || !r->count || r->count>~(pangu_u64)0/48 || r->error_offset!=s.length || !range_ok(r->tokens,r->count*48)) return 10;
 if(overlap((pangu_u64)out,8,(pangu_u64)s.data,s.length) || overlap((pangu_u64)out,8,(pangu_u64)r,32) || overlap((pangu_u64)out,8,(pangu_u64)r->tokens,r->count*48) || index>=r->count) return 1;
 const pangu_json_token *t=&r->tokens[index];
 if(t->kind!=PANGU_JSON_NUMBER) return 9;
 if(t->flags || t->children || t->next!=index+1 || t->start>=t->end || t->end>s.length) return 10;
 pangu_u64 length=t->end-t->start;if(length>NUMBER_BYTES) return 2;
 const unsigned char *p=s.data+t->start;pangu_u64 at=0,sign=0;
 if(p[at]=='-') { sign=0x8000000000000000ULL;if(++at==length) return 14; }
 decimal_input v;small(&v.numerator,0);v.seen=0;v.kept=0;v.sticky=0;
 if(p[at]=='0') at++;
 else {
  if(p[at]<'1' || p[at]>'9') return 14;
  do { if(digit(&v,p[at++])) return 11; } while(at<length && is_digit(p[at]));
 }
 pangu_u32 fraction=0;
 if(at<length && p[at]=='.') {
  at++;if(at==length || !is_digit(p[at])) return 14;
  do { fraction++;if(digit(&v,p[at++])) return 11; } while(at<length && is_digit(p[at]));
 }
 pangu_i32 exponent=0,negative=0;
 if(at<length && (p[at]=='e' || p[at]=='E')) {
  at++;if(at<length && (p[at]=='+' || p[at]=='-')) { negative=p[at]=='-';at++; }
  if(at==length || !is_digit(p[at])) return 14;
  do {
   pangu_i32 x=p[at++]-'0';
   exponent=exponent>(EXP_LIMIT-x)/10 ? EXP_LIMIT : exponent*10+x;
  } while(at<length && is_digit(p[at]));
 }
 if(at!=length) return 14;
 if(negative) exponent=-exponent;
 if(!v.seen) { *out=sign;return 0; }
 exponent-=(pangu_i32)fraction;
 pangu_i32 order=exponent+(pangu_i32)v.seen-1;
 if(order>308) { *out=sign|0x7ff0000000000000ULL;return 0; }
 if(order< -324) { *out=sign;return 0; }
 exponent+=(pangu_i32)(v.seen-v.kept);
 big denominator,a,b;small(&denominator,1);
 if(exponent<0) { for(pangu_i32 i=0;i< -exponent;i++) if(mul(&denominator,10)) return 11; }
 else for(pangu_i32 i=0;i<exponent;i++) if(mul(&v.numerator,10)) return 11;
 pangu_u64 low=0,high=0x7ff0000000000000ULL;
 while(low<high) {
  pangu_u64 middle=low+(high-low)/2;int cmp;
  if(midpoint(&v.numerator,&denominator,middle,&a,&b,&cmp)) return 11;
  if(cmp<=0) high=middle;else low=middle+1;
 }
 if(low<0x7ff0000000000000ULL) {
  int cmp;if(midpoint(&v.numerator,&denominator,low,&a,&b,&cmp)) return 11;
  if(!cmp && (v.sticky || (low&1))) low++;
 }
 *out=sign|low;return 0;
}

static void small64(big *a,pangu_u64 value)
{
 small(a,(pangu_u32)value);
 if(value>>32) { a->word[1]=(pangu_u32)(value>>32);a->used=2; }
}
static pangu_u32 div10(big *a)
{
 pangu_u64 carry=0;
 for(pangu_u32 i=a->used;i>0;i--) {
  pangu_u64 value=(carry<<32)|a->word[i-1];
  a->word[i-1]=(pangu_u32)(value/10);carry=value%10;
 }
 while(a->used>1 && !a->word[a->used-1]) a->used--;
 return (pangu_u32)carry;
}
/* Test exact decimal candidates against both binary64 midpoints. Equality
 * belongs to the target only if its significand is even. No float operation
 * or second numeric parser is needed. bits is positive, finite and nonzero. */
static int roundtrips(pangu_u64 bits,pangu_u64 coefficient,pangu_i32 scale,int *matches)
{
 big n,d,a,b;small64(&n,coefficient);small(&d,1);
 if(scale<0) { for(pangu_i32 i=0;i< -scale;i++) if(mul(&d,10)) return 11; }
 else for(pangu_i32 i=0;i<scale;i++) if(mul(&n,10)) return 11;
 int comparison;
 if(midpoint(&n,&d,bits-1,&a,&b,&comparison)) return 11;
 *matches=0;if(comparison<0 || (!comparison && (bits&1))) return 0;
 if(midpoint(&n,&d,bits,&a,&b,&comparison)) return 11;
 if(comparison>0 || (!comparison && (bits&1))) return 0;
 *matches=1;return 0;
}
static pangu_u32 decimal_u64(pangu_u64 value,unsigned char *out)
{
 unsigned char reverse[20];pangu_u32 count=0;
 do { reverse[count++]=(unsigned char)('0'+value%10);value/=10; } while(value);
 for(pangu_u32 i=0;i<count;i++) out[i]=reverse[count-i-1];
 return count;
}
static int finite_text(pangu_u64 bits,unsigned char *out,pangu_u32 *length)
{
 pangu_u64 magnitude=bits&0x7fffffffffffffffULL;
 pangu_u32 biased=(pangu_u32)(magnitude>>52);pangu_u64 mantissa=magnitude&0xfffffffffffffULL;
 pangu_i32 scale=-1074;
 if(biased) { mantissa|=0x10000000000000ULL;scale=(pangu_i32)biased-1075; }
 big exact;small64(&exact,mantissa);
 if(scale<0) { for(pangu_i32 i=0;i< -scale;i++) if(mul(&exact,5)) return 11; }
 else { if(shift(&exact,(pangu_u32)scale)) return 11;scale=0; }
 unsigned char digits[800];pangu_u32 at=800;
 do {
  if(!at) return 11;
  digits[--at]=(unsigned char)('0'+div10(&exact));
 } while(exact.used!=1 || exact.word[0]);
 pangu_u32 count=800-at;pangu_i32 order=(pangu_i32)count+scale-1;
 pangu_u64 prefix=0,chosen=0;pangu_i32 selected_scale=0;
 for(pangu_u32 n=1;n<=17 && n<=count;n++) {
  prefix=prefix*10+(digits[at+n-1]-'0');pangu_i32 candidate_scale=order-(pangu_i32)n+1;
  int down,up;
  if(roundtrips(magnitude,prefix,candidate_scale,&down) || roundtrips(magnitude,prefix+1,candidate_scale,&up)) return 11;
  if(!down && !up) continue;
  int prefer_up=0;
  if(n<count) {
   unsigned char next=digits[at+n];
   if(next>'5') prefer_up=1;
   else if(next=='5') {
    int tail=0;for(pangu_u32 i=n+1;i<count;i++) if(digits[at+i]!='0') { tail=1;break; }
    prefer_up=tail || (prefix&1);
   }
  }
  chosen=prefix+(!down || (up && prefer_up));selected_scale=candidate_scale;break;
 }
 if(!chosen) return 11;
 while(chosen%10==0) { chosen/=10;selected_scale++; }
 unsigned char coefficient[20];pangu_u32 k=decimal_u64(chosen,coefficient);
 pangu_i32 point=(pangu_i32)k+selected_scale;pangu_u32 used=0;
#define APPEND(byte) do { if(used==32) return 11;out[used++]=(unsigned char)(byte); } while(0)
 if(bits>>63) APPEND('-');
 if(point>0 && point<=21) {
  if(point>=(pangu_i32)k) {
   for(pangu_u32 i=0;i<k;i++) APPEND(coefficient[i]);
   for(pangu_i32 i=(pangu_i32)k;i<point;i++) APPEND('0');
  } else {
   for(pangu_u32 i=0;i<k;i++) { if(i==(pangu_u32)point) APPEND('.');APPEND(coefficient[i]); }
  }
 } else if(point<=0 && point>= -5) {
  APPEND('0');APPEND('.');for(pangu_i32 i=0;i< -point;i++) APPEND('0');
  for(pangu_u32 i=0;i<k;i++) APPEND(coefficient[i]);
 } else {
  APPEND(coefficient[0]);if(k>1) APPEND('.');
  for(pangu_u32 i=1;i<k;i++) APPEND(coefficient[i]);
  APPEND('e');pangu_i32 exponent=point-1;APPEND(exponent<0 ? '-' : '+');
  unsigned char exponent_digits[20];pangu_u32 n=decimal_u64((pangu_u64)(exponent<0 ? -exponent : exponent),exponent_digits);
  for(pangu_u32 i=0;i<n;i++) APPEND(exponent_digits[i]);
 }
#undef APPEND
 *length=used;return 0;
}
pangu_i32 pangu_json_number_text(pangu_u64 bits,pangu_u32 mode,unsigned char *out,pangu_u64 capacity,pangu_u64 *length)
{
 if(!length || (mode!=PANGU_JSON_NUMBER_JSON && mode!=PANGU_JSON_NUMBER_STRING)
  || !range_ok(length,8) || !range_ok(out,capacity)
  || overlap((pangu_u64)out,capacity,(pangu_u64)length,8)) return 1;
 *length=0;unsigned char text[32];pangu_u32 n=0;pangu_u64 magnitude=bits&0x7fffffffffffffffULL;
 if(!magnitude) { text[0]='0';n=1; }
 else if(magnitude>=0x7ff0000000000000ULL) {
  const unsigned char *literal;
  if(mode==PANGU_JSON_NUMBER_JSON) { literal=(const unsigned char *)"null";n=4; }
  else if(magnitude>0x7ff0000000000000ULL) { literal=(const unsigned char *)"NaN";n=3; }
  else if(bits>>63) { literal=(const unsigned char *)"-Infinity";n=9; }
  else { literal=(const unsigned char *)"Infinity";n=8; }
  for(pangu_u32 i=0;i<n;i++) text[i]=literal[i];
 } else { pangu_i32 rc=finite_text(bits,text,&n);if(rc) return rc; }
 *length=n;if(n>capacity) return 2;
 for(pangu_u32 i=0;i<n;i++) out[i]=text[i];
 return 0;
}
