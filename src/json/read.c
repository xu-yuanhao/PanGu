#include "src/bootstrap-api/json.h"
typedef struct string_cursor { pangu_text_slice input;pangu_u64 at,end; } string_cursor;
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static int range_ok(const void *p,pangu_u64 n)
{
 return (!n || p) && n<=~(pangu_u64)0-(pangu_u64)p;
}
static pangu_i32 result_ok(pangu_text_slice s,const pangu_json_result *r)
{
 if(!r || !range_ok(s.data,s.length)) return 1;
 if(r->status || r->reserved || !r->tokens || !r->count || r->error_offset!=s.length || r->count>~(pangu_u64)0/48 || !range_ok(r->tokens,r->count*48)) return 10;
 return 0;
}
static int output_ok(pangu_text_slice s,const pangu_json_result *r,const void *out,pangu_u64 bytes)
{
 pangu_u64 p=(pangu_u64)out;
 return range_ok(out,bytes) && !overlap(p,bytes,(pangu_u64)s.data,s.length)
  && !overlap(p,bytes,(pangu_u64)r,sizeof(*r))
  && !overlap(p,bytes,(pangu_u64)r->tokens,r->count*48);
}
static pangu_i32 string_start(pangu_text_slice s,const pangu_json_result *r,pangu_u64 index,string_cursor *c)
{
 if(index>=r->count) return 1;
 const pangu_json_token *t=&r->tokens[index];
 if(t->kind!=PANGU_JSON_STRING) return 9;
 if(t->start>t->end || t->end>s.length || t->end-t->start<2 || t->next!=index+1 || t->children || (t->flags&~3u)) return 10;
 if(s.data[t->start]!='"' || s.data[t->end-1]!='"') return 14;
 c->input.data=s.data;c->input.length=t->end-1;c->at=t->start+1;c->end=t->end-1;return 0;
}
static pangu_i32 hex4(string_cursor *c,pangu_u32 *out)
{
 if(c->end-c->at<4) return 14;
 pangu_u32 value=0;
 for(pangu_u32 i=0;i<4;i++) {
  unsigned char b=c->input.data[c->at++];pangu_u32 n;
  if(b>='0' && b<='9') n=b-'0';
  else if(b>='a' && b<='f') n=b-'a'+10;
  else if(b>='A' && b<='F') n=b-'A'+10;
  else return 14;
  value=(value<<4)|n;
 }
 *out=value;return 0;
}
static pangu_i32 next_scalar(string_cursor *c,pangu_u32 *cp)
{
 if(c->at==c->end) return 3;
 unsigned char b=c->input.data[c->at];
 if(b<0x20 || b=='"') return 14;
 if(b!='\\') return pangu_text_next(c->input,&c->at,cp);
 c->at++;
 if(c->at==c->end) return 14;
 b=c->input.data[c->at++];
 switch(b) {
 case '"':case '\\':case '/':*cp=b;return 0;
 case 'b':*cp=8;return 0;case 'f':*cp=12;return 0;
 case 'n':*cp=10;return 0;case 'r':*cp=13;return 0;case 't':*cp=9;return 0;
 case 'u':break;
 default:return 14;
 }
 pangu_u32 first;pangu_i32 rc=hex4(c,&first);if(rc) return rc;
 if(first>=0xdc00 && first<=0xdfff) return 9;
 if(first>=0xd800 && first<=0xdbff) {
  if(c->end-c->at<6 || c->input.data[c->at]!='\\' || c->input.data[c->at+1]!='u') return 9;
  c->at+=2;pangu_u32 second;rc=hex4(c,&second);if(rc) return rc;
  if(second<0xdc00 || second>0xdfff) return 9;
  *cp=0x10000+(first-0xd800)*1024+second-0xdc00;return 0;
 }
 *cp=first;return 0;
}
static pangu_u32 encode(pangu_u32 cp,unsigned char bytes[4])
{
 if(cp<0x80) { bytes[0]=(unsigned char)cp;return 1; }
 if(cp<0x800) { bytes[0]=(unsigned char)(0xc0|(cp>>6));bytes[1]=(unsigned char)(0x80|(cp&63));return 2; }
 if(cp<0x10000) { bytes[0]=(unsigned char)(0xe0|(cp>>12));bytes[1]=(unsigned char)(0x80|((cp>>6)&63));bytes[2]=(unsigned char)(0x80|(cp&63));return 3; }
 bytes[0]=(unsigned char)(0xf0|(cp>>18));bytes[1]=(unsigned char)(0x80|((cp>>12)&63));bytes[2]=(unsigned char)(0x80|((cp>>6)&63));bytes[3]=(unsigned char)(0x80|(cp&63));return 4;
}
pangu_i32 pangu_json_string_decode(pangu_text_slice s,const pangu_json_result *r,pangu_u64 index,unsigned char *out,pangu_u64 capacity,pangu_u64 *length)
{
 if(!length) return 1;
 pangu_i32 rc=result_ok(s,r);if(rc) return rc;
 if(!output_ok(s,r,length,8) || !output_ok(s,r,out,capacity) || overlap((pangu_u64)out,capacity,(pangu_u64)length,8)) return 1;
 if(index>=r->count) return 1;
 *length=0;string_cursor c;rc=string_start(s,r,index,&c);if(rc) return rc;
 pangu_u64 needed=0;unsigned char bytes[4];
 for(;;) {
  pangu_u32 cp;rc=next_scalar(&c,&cp);if(rc==3) break;if(rc) return rc;
  pangu_u32 n=encode(cp,bytes);if(needed>~(pangu_u64)0-n) return 11;needed+=n;
 }
 *length=needed;if(needed>capacity) return 2;
 rc=string_start(s,r,index,&c);if(rc) { *length=0;return rc; }
 pangu_u64 used=0;
 while(used<needed) {
  pangu_u32 cp;rc=next_scalar(&c,&cp);
  /* The first pass validated this immutable source. */
  if(rc) { *length=0;return rc; }
  pangu_u32 n=encode(cp,bytes);for(pangu_u32 i=0;i<n;i++) out[used++]=bytes[i];
 }
 return 0;
}
static pangu_i32 matches(pangu_text_slice s,const pangu_json_result *r,pangu_u64 index,pangu_text_slice key,int *equal)
{
 string_cursor c;pangu_i32 rc=string_start(s,r,index,&c);if(rc) return rc;
 pangu_u64 at=0;*equal=0;
 for(;;) {
  pangu_u32 cp;rc=next_scalar(&c,&cp);
  if(rc==3) { *equal=at==key.length;return 0; }
  if(rc==9) return 0; /* A lone UTF-16 key cannot equal a valid UTF-8 query. */
  if(rc) return rc;
  unsigned char bytes[4];pangu_u32 n=encode(cp,bytes);
  if(key.length-at<n) return 0;
  for(pangu_u32 i=0;i<n;i++) if(key.data[at++]!=bytes[i]) return 0;
 }
}
pangu_i32 pangu_json_object_find(pangu_text_slice s,const pangu_json_result *r,pangu_u64 object,pangu_text_slice key,pangu_u64 *value)
{
 if(!value || !range_ok(key.data,key.length)) return 1;
 pangu_i32 rc=result_ok(s,r);if(rc) return rc;
 if(!output_ok(s,r,value,8) || overlap((pangu_u64)value,8,(pangu_u64)key.data,key.length)) return 1;
 if(object>=r->count) return 1;
 *value=~(pangu_u64)0;
 pangu_u64 count,error;rc=pangu_text_validate(key,&count,&error);if(rc) return rc;
 const pangu_json_token *o=&r->tokens[object];if(o->kind!=PANGU_JSON_OBJECT) return 9;
 if(o->next<=object || o->next>r->count || o->start>=o->end || o->end>s.length) return 10;
 pangu_u64 at=object+1,found=~(pangu_u64)0,seen=0;
 while(at<o->next) {
  const pangu_json_token *k=&r->tokens[at];
  if(k->kind!=PANGU_JSON_STRING || !(k->flags&PANGU_JSON_KEY) || k->parent!=object || k->next!=at+1 || at+1>=o->next) return 10;
  pangu_u64 v=at+1;const pangu_json_token *t=&r->tokens[v];
  if(t->parent!=object || (t->flags&PANGU_JSON_KEY) || t->next<=v || t->next>o->next || k->start<=o->start || k->end>t->start || t->start>=t->end || t->end>=o->end) return 10;
  int equal;rc=matches(s,r,at,key,&equal);if(rc) return rc;if(equal) found=v;
  seen++;at=t->next;
 }
 if(seen!=o->children) return 10;
 *value=found;return found==~(pangu_u64)0 ? 3 : 0;
}


typedef struct unit_cursor { string_cursor raw;pangu_u32 pending,have; } unit_cursor;
static pangu_i32 next_unit(unit_cursor *c,pangu_u32 *unit)
{
 if(c->have) { *unit=c->pending;c->have=0;return 0; }
 if(c->raw.at==c->raw.end) return 3;
 unsigned char b=c->raw.input.data[c->raw.at];
 if(b<0x20 || b=='"') return 14;
 if(b!='\\') {
  pangu_u32 cp;pangu_i32 rc=pangu_text_next(c->raw.input,&c->raw.at,&cp);if(rc) return rc;
  if(cp<=0xffff) *unit=cp;
  else { cp-=0x10000;*unit=0xd800+(cp>>10);c->pending=0xdc00+(cp&1023);c->have=1; }
  return 0;
 }
 c->raw.at++;if(c->raw.at==c->raw.end) return 14;
 b=c->raw.input.data[c->raw.at++];
 switch(b) {
 case '"':case '\\':case '/':*unit=b;return 0;
 case 'b':*unit=8;return 0;case 'f':*unit=12;return 0;
 case 'n':*unit=10;return 0;case 'r':*unit=13;return 0;case 't':*unit=9;return 0;
 case 'u':return hex4(&c->raw,unit);
 default:return 14;
 }
}
pangu_i32 pangu_json_string_equal(pangu_text_slice a,const pangu_json_result *ra,pangu_u64 ia,
 pangu_text_slice b,const pangu_json_result *rb,pangu_u64 ib,pangu_u32 *equal)
{
 if(!equal) return 1;
 pangu_i32 rc=result_ok(a,ra);if(rc) return rc;rc=result_ok(b,rb);if(rc) return rc;
 if(!output_ok(a,ra,equal,4) || !output_ok(b,rb,equal,4)) return 1;
 unit_cursor x,y;rc=string_start(a,ra,ia,&x.raw);if(rc) return rc;
 rc=string_start(b,rb,ib,&y.raw);if(rc) return rc;
 x.pending=0;x.have=0;y.pending=0;y.have=0;
 for(;;) {
  pangu_u32 u,v;pangu_i32 left=next_unit(&x,&u),right=next_unit(&y,&v);
  if(left && left!=3) return left;
  if(right && right!=3) return right;
  if(left==3 || right==3) { *equal=left==3 && right==3;return 0; }
  if(u!=v) { *equal=0;return 0; }
 }
}
