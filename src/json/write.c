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
static int disjoint(const pangu_json_writer *w,const void *p,pangu_u64 n)
{
 return range_ok(p,n) && !overlap((pangu_u64)p,n,(pangu_u64)w,560)
  && !overlap((pangu_u64)p,n,(pangu_u64)w->data,w->capacity);
}
static int valid(const pangu_json_writer *w)
{
 if(w->abi_version!=1 || w->struct_size!=560 || !w->max_depth || w->max_depth>64
  || w->depth>w->max_depth || w->root>1 || w->sealed>1 || w->length>w->capacity
  || !range_ok(w->data,w->capacity) || overlap((pangu_u64)w,560,(pangu_u64)w->data,w->capacity)) return 0;
 if((!w->root && (w->length || w->depth)) || (w->sealed && (!w->root || w->depth))) return 0;
 for(pangu_u32 i=0;i<w->depth;i++) {
  const pangu_json_write_frame *f=&w->frames[i];
  if((f->kind!=PANGU_JSON_OBJECT && f->kind!=PANGU_JSON_ARRAY) || f->phase>2
   || (f->kind==PANGU_JSON_ARRAY && f->phase==1) || (i+1<w->depth && f->phase!=2)) return 0;
 }
 return 1;
}
pangu_i32 pangu_json_writer_init(pangu_json_writer *w,unsigned char *out,pangu_u64 capacity,pangu_u32 max_depth)
{
 if(!w || !range_ok(w,560) || !range_ok(out,capacity) || !max_depth || max_depth>64
  || overlap((pangu_u64)w,560,(pangu_u64)out,capacity)) return 1;
 w->abi_version=1;w->struct_size=560;w->data=out;w->capacity=capacity;w->length=0;
 w->depth=0;w->max_depth=max_depth;w->root=0;w->sealed=0;return 0;
}
static void complete(pangu_json_writer *w)
{
 if(w->depth) w->frames[w->depth-1].phase=2;
 else w->root=1;
}
static pangu_i32 number_ok(pangu_text_slice s)
{
 if(!s.length) return 14;
 if(s.length>1048576) return 2;
 pangu_json_parser parser;pangu_json_token token;pangu_json_result result;pangu_json_limits limits;
 unsigned char *raw=(unsigned char *)&parser;
 for(pangu_u32 i=0;i<sizeof(parser);i++) raw[i]=0;
 limits.max_bytes=1048576;limits.max_depth=1;limits.reserved=0;
 pangu_i32 rc=pangu_json_init(&parser,&token,1,limits);if(rc) return rc;
 rc=pangu_json_feed(&parser,s,1);if(rc) return rc==13 ? 14 : rc;
 rc=pangu_json_get_result(&parser,&result);if(rc) return rc;
 if(token.kind!=PANGU_JSON_NUMBER) return 9;
 if(token.start || token.end!=s.length) return 14;
 return 0;
}
pangu_i32 pangu_json_writer_emit(pangu_json_writer *w,pangu_u32 op,pangu_text_slice input)
{
 if(!w) return 1;
 if(!valid(w) || w->sealed) return 10;
 if(op<PANGU_JSON_WRITE_OBJECT || op>PANGU_JSON_WRITE_NULL) return 1;
 int key=op==PANGU_JSON_WRITE_KEY,quoted=key || op==PANGU_JSON_WRITE_STRING;
 int argument=quoted || op==PANGU_JSON_WRITE_NUMBER;
 if(argument) { if(!disjoint(w,input.data,input.length)) return 1; }
 else if(input.data || input.length) return 1;
 int closing=op==PANGU_JSON_WRITE_END_OBJECT || op==PANGU_JSON_WRITE_END_ARRAY;
 pangu_u32 kind=(op==PANGU_JSON_WRITE_OBJECT || op==PANGU_JSON_WRITE_END_OBJECT) ? PANGU_JSON_OBJECT : PANGU_JSON_ARRAY;
 if(closing) {
  if(!w->depth) return 10;
  pangu_json_write_frame *f=&w->frames[w->depth-1];
  if(f->kind!=kind || f->phase==1) return 10;
  if(w->length==w->capacity) return 2;
  w->data[w->length++]=kind==PANGU_JSON_OBJECT ? '}' : ']';w->depth--;return 0;
 }
 pangu_u64 prefix=0;
 if(w->depth) {
  const pangu_json_write_frame *f=&w->frames[w->depth-1];
  if(key) {
   if(f->kind!=PANGU_JSON_OBJECT || f->phase==1) return 10;
   prefix=f->phase==2;
  } else {
   if(f->kind==PANGU_JSON_OBJECT) { if(f->phase!=1) return 10; }
   else prefix=f->phase==2;
  }
 } else if(key || w->root) return 10;
 int opening=op==PANGU_JSON_WRITE_OBJECT || op==PANGU_JSON_WRITE_ARRAY;
 if(opening && w->depth==w->max_depth) return 2;
 const unsigned char *bytes=(void *)0;pangu_u64 n=1;
 if(quoted) {
  pangu_i32 rc=pangu_json_quote_utf8(input,(void *)0,0,&n);if(rc!=2) return rc ? rc : 10;
 } else if(op==PANGU_JSON_WRITE_NUMBER) {
  pangu_i32 rc=number_ok(input);if(rc) return rc;bytes=input.data;n=input.length;
 } else if(op==PANGU_JSON_WRITE_TRUE) { bytes=(const unsigned char *)"true";n=4; }
 else if(op==PANGU_JSON_WRITE_FALSE) { bytes=(const unsigned char *)"false";n=5; }
 else if(op==PANGU_JSON_WRITE_NULL) { bytes=(const unsigned char *)"null";n=4; }
 pangu_u64 extra=prefix+(pangu_u64)key,remaining=w->capacity-w->length;
 if(extra>remaining || n>remaining-extra) return 2;
 unsigned char *dest=w->data+w->length+prefix;
 if(quoted) {
  pangu_u64 actual;pangu_i32 rc=pangu_json_quote_utf8(input,dest,n,&actual);if(rc) return rc;
 } else if(opening) *dest=kind==PANGU_JSON_OBJECT ? '{' : '[';
 else for(pangu_u64 i=0;i<n;i++) dest[i]=bytes[i];
 if(prefix) w->data[w->length]=',';
 if(key) dest[n]=':';
 w->length+=n+extra;
 if(key) w->frames[w->depth-1].phase=1;
 else {
  complete(w);
  if(opening) { pangu_json_write_frame *f=&w->frames[w->depth++];f->kind=kind;f->phase=0; }
 }
 return 0;
}
pangu_i32 pangu_json_writer_finish(pangu_json_writer *w,pangu_text_slice *out)
{
 if(!w || !out) return 1;
 if(!valid(w) || w->sealed) return 10;
 if(!disjoint(w,out,sizeof(*out))) return 1;
 if(!w->root || w->depth) return 13;
 out->data=w->data;out->length=w->length;w->sealed=1;return 0;
}
