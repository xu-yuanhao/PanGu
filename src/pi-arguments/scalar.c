#include "src/bootstrap-api/pi-arguments.h"
#define MAG 0x7fffffffffffffffULL
#define INF 0x7ff0000000000000ULL
static int range_ok(const void *p,pangu_u64 n) { return (!n || p) && n<=~(pangu_u64)0-(pangu_u64)p; }
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static void original(pangu_pi_scalar_result *r)
{
 r->kind=0;r->reserved=0;r->number_bits=0;r->text.data=(void *)0;r->text.length=0;
}
static void number(pangu_pi_scalar_result *r,pangu_u64 bits) { r->kind=PANGU_JSON_NUMBER;r->number_bits=bits; }
static void string(pangu_pi_scalar_result *r,const unsigned char *p,pangu_u64 n) { r->kind=PANGU_JSON_STRING;r->text.data=p;r->text.length=n; }
static int word(pangu_text_slice s,const char *w,pangu_u64 n,pangu_u32 fold)
{
 if(s.length!=n) return 0;
 for(pangu_u64 i=0;i<n;i++) { unsigned char c=s.data[i];if(fold && c>='A' && c<='Z') c=(unsigned char)(c+32);if(c!=(unsigned char)w[i]) return 0; }
 return 1;
}
static int whitespace(pangu_u32 c)
{
 return (c>=9 && c<=13) || c==32 || c==0xa0 || c==0x1680 || (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000 || c==0xfeff;
}
static int integer(pangu_u64 bits)
{
 pangu_u64 m=bits&MAG;if(!m) return 1;if(m>=INF) return 0;
 pangu_i32 e=(pangu_i32)(m>>52)-1023;if(e<0) return 0;if(e>=52) return 1;
 return (m&(((pangu_u64)1<<(52-e))-1))==0;
}
static pangu_u64 trunc_bits(pangu_u64 bits)
{
 pangu_i32 e=(pangu_i32)((bits&MAG)>>52)-1023;
 if(e<0) return bits&~MAG;
 if(e>=52) return bits;
 return bits&~(((pangu_u64)1<<(52-e))-1);
}
/* TypeBox TryBigInt's only additional numeric syntax that can succeed after
 * Number(string) failed to be finite: exact safe decimal integer suffix n. */
static int bigint_suffix(pangu_text_slice s,pangu_u64 *bits)
{
 if(s.length<2 || s.data[s.length-1]!='n') return 0;
 pangu_u64 i=0,negative=0,magnitude=0,end=s.length-1;
 if(s.data[i]=='-') { negative=1;i++; }
 if(i==end || s.data[i]<'0' || s.data[i]>'9' || (s.data[i]=='0' && i+1!=end)) return 0;
 for(;i<end;i++) {
  unsigned char c=s.data[i];if(c<'0' || c>'9') return 0;
  pangu_u64 d=(pangu_u64)(c-'0');if(magnitude>(9007199254740991ULL-d)/10) return 0;magnitude=magnitude*10+d;
 }
 if(!magnitude) { *bits=0;return 1; }
 pangu_u32 length=0;for(pangu_u64 n=magnitude;n;n>>=1) length++;
 *bits=(negative<<63)|((pangu_u64)(length+1022)<<52)|((magnitude<<(53-length))&0xfffffffffffffULL);return 1;
}
static pangu_i32 format_number(pangu_u64 bits,unsigned char *workspace,pangu_u64 capacity,pangu_pi_scalar_result *r)
{
 pangu_u64 n;pangu_i32 rc=pangu_json_number_text(bits,PANGU_JSON_NUMBER_STRING,workspace,capacity,&n);if(rc) return rc;
 string(r,workspace,n);return 0;
}
static pangu_i32 convert(const pangu_schema_view *v,const pangu_json_token *t,pangu_u32 target,pangu_u32 mode,unsigned char *workspace,pangu_u64 capacity,pangu_u64 max_bytes,pangu_pi_scalar_result *r)
{
 pangu_u32 typed=mode!=PANGU_PI_PLAIN_RULE;original(r);
 if(t->kind==PANGU_JSON_ARRAY || t->kind==PANGU_JSON_OBJECT) return 0;
 if(target==PANGU_PI_SCALAR_STRING) {
  if(t->kind==PANGU_JSON_STRING) return 0;
  if(t->kind==PANGU_JSON_NULL) { string(r,(const unsigned char *)(typed?"null":""),typed?4:0);return 0; }
  if(t->kind==PANGU_JSON_TRUE || t->kind==PANGU_JSON_FALSE) { string(r,(const unsigned char *)(t->kind==PANGU_JSON_TRUE?"true":"false"),t->kind==PANGU_JSON_TRUE?4:5);return 0; }
  pangu_u64 bits;pangu_i32 rc=pangu_json_number_bits(v->input,v->result,v->root,&bits);if(rc) return rc;
  if(mode==PANGU_PI_TYPEBOX_RULE && (bits&MAG)>=INF) return 0;
  return format_number(bits,workspace,capacity,r);
 }
 if((target==PANGU_PI_SCALAR_NUMBER && t->kind==PANGU_JSON_NUMBER) || (target==PANGU_PI_SCALAR_INTEGER && !typed && t->kind==PANGU_JSON_NUMBER) || (target==PANGU_PI_SCALAR_BOOLEAN && (t->kind==PANGU_JSON_TRUE || t->kind==PANGU_JSON_FALSE)) || (target==PANGU_PI_SCALAR_NULL && t->kind==PANGU_JSON_NULL)) return 0;
 if(t->kind==PANGU_JSON_NULL) {
  if(target==PANGU_PI_SCALAR_NUMBER || target==PANGU_PI_SCALAR_INTEGER) number(r,0);
  else if(target==PANGU_PI_SCALAR_BOOLEAN) r->kind=PANGU_JSON_FALSE;
  return 0;
 }
 if(t->kind==PANGU_JSON_TRUE || t->kind==PANGU_JSON_FALSE) {
  if(target==PANGU_PI_SCALAR_NUMBER || target==PANGU_PI_SCALAR_INTEGER) number(r,t->kind==PANGU_JSON_TRUE?0x3ff0000000000000ULL:0);
  else if(target==PANGU_PI_SCALAR_NULL && t->kind==PANGU_JSON_FALSE) r->kind=PANGU_JSON_NULL;
  return 0;
 }
 if(t->kind==PANGU_JSON_NUMBER) {
  pangu_u64 bits;pangu_i32 rc=pangu_json_number_bits(v->input,v->result,v->root,&bits);if(rc) return rc;
  if(target==PANGU_PI_SCALAR_INTEGER) { if((bits&MAG)<INF) { pangu_u64 n=trunc_bits(bits);if(n!=bits) number(r,n); } }
  else if(target==PANGU_PI_SCALAR_BOOLEAN) { if(!(bits&MAG)) r->kind=PANGU_JSON_FALSE;else if(bits==0x3ff0000000000000ULL) r->kind=PANGU_JSON_TRUE; }
  else if(target==PANGU_PI_SCALAR_NULL && !(bits&MAG)) r->kind=PANGU_JSON_NULL;
  return 0;
 }
 pangu_u64 n;pangu_i32 rc=pangu_json_string_decode(v->input,v->result,v->root,(void *)0,0,&n);
 if(rc==9) return 0; /* Unpaired UTF16 cannot match any numeric/word predicate. */
 if(rc!=0 && rc!=2) return rc;
 if((target==PANGU_PI_SCALAR_BOOLEAN && n>5) || (target==PANGU_PI_SCALAR_NULL && n>9)) return 0;
 if(n>capacity) return 2;
 rc=pangu_json_string_decode(v->input,v->result,v->root,workspace,n,&n);if(rc) return rc;
 pangu_text_slice text={workspace,n};
 if(target==PANGU_PI_SCALAR_BOOLEAN) {
  if(word(text,"true",4,typed) || (typed && word(text,"1",1,0))) r->kind=PANGU_JSON_TRUE;
  else if(word(text,"false",5,typed) || (typed && word(text,"0",1,0))) r->kind=PANGU_JSON_FALSE;
  return 0;
 }
 if(target==PANGU_PI_SCALAR_NULL) {
  if(!n || (typed && (word(text,"null",4,1) || word(text,"undefined",9,1) || word(text,"0",1,0)))) r->kind=PANGU_JSON_NULL;
  return 0;
 }
 if(!typed) {
  pangu_u64 at=0;pangu_u32 nonwhite=0;
  while(at<n) { pangu_u32 scalar;rc=pangu_text_next(text,&at,&scalar);if(rc) return rc;if(!whitespace(scalar)) nonwhite=1; }
  if(!nonwhite) return 0;
 }
 pangu_u64 bits;unsigned char *canonical=workspace ? workspace+n : (void *)0;
 rc=pangu_pi_number_string(text,canonical,capacity-n,max_bytes,&bits);if(rc) return rc;
 if((bits&MAG)>=INF) {
  if(!typed) return 0;
  if(word(text,"true",4,1)) bits=0x3ff0000000000000ULL;
  else if(word(text,"false",5,1)) bits=0;
  else if(!bigint_suffix(text,&bits)) return 0;
 }
 if(target==PANGU_PI_SCALAR_INTEGER) { if(typed) bits=trunc_bits(bits);else if(!integer(bits)) return 0; }
 number(r,bits);return 0;
}
pangu_i32 pangu_pi_scalar_convert(const pangu_schema_view *v,pangu_u32 target,pangu_u32 mode,unsigned char *workspace,pangu_u64 capacity,pangu_u64 max_bytes,pangu_pi_scalar_result *out)
{
 if(!v || !range_ok(v,32) || !out || !range_ok(out,32) || !range_ok(workspace,capacity) || target<1 || target>5 || mode<1 || mode>3 || !range_ok(v->input.data,v->input.length) || !v->result || !range_ok(v->result,32)) return 1;
 const pangu_json_result *result=v->result;
 if(result->status || result->reserved || !result->tokens || !result->count || result->count>~(pangu_u64)0/48 || !range_ok(result->tokens,result->count*48) || result->error_offset!=v->input.length) return 10;
 if(v->root>=result->count) return 1;
 const pangu_json_token *t=&result->tokens[v->root];
 if(t->kind<1 || t->kind>7 || t->start>=t->end || t->end>v->input.length || t->next<=v->root || t->next>result->count) return 10;
 if(t->flags&PANGU_JSON_KEY) return 1;
 pangu_u64 read[4],rn[4];read[0]=(pangu_u64)v;rn[0]=32;read[1]=(pangu_u64)result;rn[1]=32;read[2]=(pangu_u64)result->tokens;rn[2]=result->count*48;read[3]=(pangu_u64)v->input.data;rn[3]=v->input.length;
 for(pangu_u32 i=0;i<4;i++) if(overlap((pangu_u64)out,32,read[i],rn[i]) || overlap((pangu_u64)workspace,capacity,read[i],rn[i])) return 1;
 if(overlap((pangu_u64)out,32,(pangu_u64)workspace,capacity)) return 1;
 if(t->end-t->start>max_bytes) return 2;
 pangu_pi_scalar_result r;pangu_i32 rc=convert(v,t,target,mode,workspace,capacity,max_bytes,&r);if(rc) return rc;
 out->kind=r.kind;out->reserved=0;out->number_bits=r.number_bits;out->text.data=r.text.data;out->text.length=r.text.length;return 0;
}
