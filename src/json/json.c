#include "src/bootstrap-api/json.h"
enum { IDLE,STRING,ESCAPE,UNICODE,NUMBER,LITERAL };
enum { KEY_OR_END,KEY,COLON,VALUE,OBJECT_COMMA,ARRAY_VALUE_OR_END,ARRAY_VALUE,ARRAY_COMMA };
static int valid(const pangu_json_parser *p)
{
 return p && p->abi_version==1 && p->struct_size==1144;
}
static pangu_i32 fail(pangu_json_parser *p,pangu_i32 status,pangu_u64 offset)
{
 p->status=status;p->error_offset=offset;p->sealed=2;return status;
}
static int space(unsigned char b) { return b==' ' || b=='\t' || b=='\n' || b=='\r'; }
static int digit(unsigned char b) { return b>='0' && b<='9'; }
static int hex(unsigned char b) { return digit(b) || (b>='a' && b<='f') || (b>='A' && b<='F'); }
static void complete(pangu_json_parser *p)
{
 if(!p->depth) p->root_phase=1;
 else {
  pangu_json_frame *f=&p->frames[p->depth-1];
  f->phase=p->tokens[f->token].kind==PANGU_JSON_OBJECT ? OBJECT_COMMA : ARRAY_COMMA;
 }
}
static pangu_i32 emit(pangu_json_parser *p,pangu_u32 kind,pangu_u32 key)
{
 if(p->count==p->capacity) return fail(p,2,p->offset);
 pangu_u64 index=p->count++;pangu_json_token *t=&p->tokens[index];
 t->kind=kind;t->flags=key ? PANGU_JSON_KEY : 0;t->start=p->offset;t->end=0;
 t->parent=p->depth ? p->frames[p->depth-1].token : ~(pangu_u64)0;
 t->next=index+1;t->children=0;
 if(p->depth && !key) p->tokens[t->parent].children++;
 p->active_index=index;p->key=key;return 0;
}
static void finish_primitive(pangu_json_parser *p,pangu_u64 end)
{
 pangu_json_token *t=&p->tokens[p->active_index];t->end=end;t->next=p->count;
 p->mode=IDLE;
 if(p->key) p->frames[p->depth-1].phase=COLON;
 else complete(p);
}
static void close_container(pangu_json_parser *p)
{
 pangu_json_token *t=&p->tokens[p->frames[p->depth-1].token];
 t->end=p->offset+1;t->next=p->count;p->depth--;complete(p);
}
pangu_i32 pangu_json_init(pangu_json_parser *p,pangu_json_token *tokens,pangu_u64 capacity,pangu_json_limits limits)
{
 if(!p || (capacity && !tokens) || !limits.max_bytes || !limits.max_depth || limits.max_depth>64 || limits.reserved) return 1;
 if(!((p->abi_version==0 && p->struct_size==0) || valid(p))) return 1;
 if(!p->abi_version && !p->struct_size) {
  const unsigned char *bytes=(const unsigned char *)p;
  for(pangu_u32 i=0;i<sizeof(*p);i++) if(bytes[i]) return 1;
 }
 p->abi_version=1;p->struct_size=1144;p->tokens=tokens;p->capacity=capacity;
 p->count=0;p->offset=0;p->max_bytes=limits.max_bytes;p->error_offset=0;p->active_index=0;
 p->depth=0;p->max_depth=limits.max_depth;p->root_phase=0;p->mode=IDLE;p->status=13;p->sealed=0;
 p->number_state=0;p->literal_position=0;p->unicode_left=0;p->utf8_have=0;p->utf8_need=0;p->key=0;p->reserved=0;
 return 0;
}
static pangu_i32 string_byte(pangu_json_parser *p,unsigned char b)
{
 if(p->utf8_have) {
  pangu_u64 start=p->offset-p->utf8_have;
  if((b&0xc0)!=0x80) return fail(p,12,start);
  p->utf8[p->utf8_have++]=b;
  if(p->utf8_have==p->utf8_need) {
   pangu_text_slice s;s.data=p->utf8;s.length=p->utf8_have;pangu_u64 cursor=0;pangu_u32 scalar;
   if(pangu_text_next(s,&cursor,&scalar)) return fail(p,12,start);
   p->utf8_have=0;
  }
 } else if(b=='"') finish_primitive(p,p->offset+1);
 else if(b=='\\') { p->mode=ESCAPE;p->tokens[p->active_index].flags|=PANGU_JSON_ESCAPED; }
 else if(b<0x20) return fail(p,14,p->offset);
 else if(b>=0x80) {
  if(b>=0xc2 && b<=0xdf) p->utf8_need=2;
  else if(b>=0xe0 && b<=0xef) p->utf8_need=3;
  else if(b>=0xf0 && b<=0xf4) p->utf8_need=4;
  else return fail(p,12,p->offset);
  p->utf8[0]=b;p->utf8_have=1;
 }
 return 0;
}
/* Return 3 to reprocess the delimiter structurally, otherwise consume once. */
static pangu_i32 number_byte(pangu_json_parser *p,unsigned char b)
{
 pangu_u32 state=p->number_state;
 if(state==0 || state==3 || state==6) {
  if(!digit(b)) return fail(p,14,p->offset);
  p->number_state=state==0 ? (b=='0' ? 1 : 2) : (state==3 ? 4 : 7);return 0;
 }
 if(state==5) {
  if(b=='+' || b=='-') p->number_state=6;
  else if(digit(b)) p->number_state=7;
  else return fail(p,14,p->offset);
  return 0;
 }
 if(digit(b)) {
  if(state==1) return fail(p,14,p->offset);
  return 0;
 }
 if(b=='.' && (state==1 || state==2)) { p->number_state=3;return 0; }
 if((b=='e' || b=='E') && (state==1 || state==2 || state==4)) { p->number_state=5;return 0; }
 finish_primitive(p,p->offset);return 3;
}
static pangu_i32 structural_byte(pangu_json_parser *p,unsigned char b)
{
 if(space(b)) return 0;
 pangu_u32 key=0;
 if(p->depth) {
  pangu_json_frame *f=&p->frames[p->depth-1];
  pangu_u32 phase=f->phase;
  if(phase==KEY_OR_END || phase==KEY) {
   if(b=='}' && phase==KEY_OR_END) { close_container(p);return 0; }
   if(b!='"') return fail(p,14,p->offset);
   key=1;
  } else if(phase==COLON) {
   if(b!=':') return fail(p,14,p->offset);
   f->phase=VALUE;return 0;
  } else if(phase==OBJECT_COMMA || phase==ARRAY_COMMA) {
   if(b==',') { f->phase=phase==OBJECT_COMMA ? KEY : ARRAY_VALUE;return 0; }
   if((phase==OBJECT_COMMA && b=='}') || (phase==ARRAY_COMMA && b==']')) { close_container(p);return 0; }
   return fail(p,14,p->offset);
  } else if(phase==ARRAY_VALUE_OR_END && b==']') { close_container(p);return 0; }
 } else if(p->root_phase) return fail(p,14,p->offset);
 pangu_u32 kind;
 if(b=='"') kind=PANGU_JSON_STRING;
 else if(b=='{' && !key) kind=PANGU_JSON_OBJECT;
 else if(b=='[' && !key) kind=PANGU_JSON_ARRAY;
 else if((digit(b) || b=='-') && !key) kind=PANGU_JSON_NUMBER;
 else if(b=='t' && !key) kind=PANGU_JSON_TRUE;
 else if(b=='f' && !key) kind=PANGU_JSON_FALSE;
 else if(b=='n' && !key) kind=PANGU_JSON_NULL;
 else return fail(p,14,p->offset);
 if((kind==PANGU_JSON_OBJECT || kind==PANGU_JSON_ARRAY) && p->depth==p->max_depth) return fail(p,2,p->offset);
 pangu_i32 rc=emit(p,kind,key);if(rc) return rc;
 if(kind==PANGU_JSON_STRING) p->mode=STRING;
 else if(kind==PANGU_JSON_NUMBER) { p->mode=NUMBER;p->number_state=b=='-' ? 0 : (b=='0' ? 1 : 2); }
 else if(kind==PANGU_JSON_OBJECT || kind==PANGU_JSON_ARRAY) {
  pangu_json_frame *f=&p->frames[p->depth++];f->token=p->active_index;f->phase=kind==PANGU_JSON_OBJECT ? KEY_OR_END : ARRAY_VALUE_OR_END;f->reserved=0;
 } else { p->mode=LITERAL;p->literal_position=1; }
 return 0;
}
pangu_i32 pangu_json_feed(pangu_json_parser *p,pangu_text_slice s,pangu_u32 final)
{
 if(!valid(p) || (!s.data && s.length) || final>1) return 1;
 if(p->sealed) return 10;
 pangu_u64 i=0;
 while(i<s.length) {
  if(p->offset==p->max_bytes) return fail(p,2,p->offset);
  unsigned char b=s.data[i];pangu_i32 rc;
  if(p->mode==STRING) rc=string_byte(p,b);
  else if(p->mode==ESCAPE) {
   if(b=='u') { p->mode=UNICODE;p->unicode_left=4;rc=0; }
   else if(b=='"' || b=='\\' || b=='/' || b=='b' || b=='f' || b=='n' || b=='r' || b=='t') { p->mode=STRING;rc=0; }
   else rc=fail(p,14,p->offset);
  } else if(p->mode==UNICODE) {
   if(!hex(b)) rc=fail(p,14,p->offset);
   else { if(!--p->unicode_left) p->mode=STRING;rc=0; }
  } else if(p->mode==NUMBER) {
   rc=number_byte(p,b);if(rc==3) continue;
  } else if(p->mode==LITERAL) {
   pangu_u32 kind=p->tokens[p->active_index].kind;
   const char *word=kind==PANGU_JSON_TRUE ? "true" : (kind==PANGU_JSON_FALSE ? "false" : "null");
   if(b!=(unsigned char)word[p->literal_position]) rc=fail(p,14,p->offset);
   else { p->literal_position++;if(!word[p->literal_position]) finish_primitive(p,p->offset+1);rc=0; }
  } else rc=structural_byte(p,b);
  if(rc) return rc;
  p->offset++;i++;
 }
 if(!final) return 13;
 if(p->mode==NUMBER && (p->number_state==1 || p->number_state==2 || p->number_state==4 || p->number_state==7)) finish_primitive(p,p->offset);
 if(p->utf8_have) return fail(p,12,p->offset-p->utf8_have);
 if(p->mode!=IDLE || p->depth || !p->root_phase) return fail(p,13,p->offset);
 p->sealed=1;p->status=0;p->error_offset=p->offset;return 0;
}
pangu_i32 pangu_json_get_result(const pangu_json_parser *p,pangu_json_result *out)
{
 if(!valid(p) || !out) return 1;
 out->tokens=p->sealed==1 ? p->tokens : (void *)0;
 out->count=p->sealed==1 ? p->count : 0;out->error_offset=p->error_offset;out->status=p->status;out->reserved=0;
 return p->status;
}
