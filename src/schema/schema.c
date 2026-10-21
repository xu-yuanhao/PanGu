#include "src/bootstrap-api/schema.h"
#define NONE (~(pangu_u64)0)
enum { T_NULL=1,T_BOOL=2,T_NUMBER=4,T_INTEGER=8,T_STRING=16,T_ARRAY=32,T_OBJECT=64 };
typedef struct context {
 const pangu_schema_view *s,*v;const pangu_schema_limits *limits;
 pangu_schema_report report;pangu_u64 nodes,where_s,where_v;
} context;
typedef struct meta {
 pangu_u64 properties,required,items,enumeration,constant;
 pangu_u32 form,types,additional;
} meta;
typedef struct frame {
 pangu_u64 schema,value,cursor,end,properties,items;
 pangu_u32 kind,depth;
} frame;
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static int range_ok(const void *p,pangu_u64 n)
{
 return (!n || p) && n<=~(pangu_u64)0-(pangu_u64)p;
}
static pangu_i32 fail(context *c,pangu_i32 status,pangu_u32 reason,pangu_u64 schema,pangu_u64 value)
{
 if(!c->report.status) { c->report.status=status;c->report.reason=reason;c->report.schema_token=schema;c->report.value_token=value; }
 return c->report.status;
}
static pangu_i32 charge(context *c,pangu_u64 amount)
{
 if(amount>c->limits->max_work-c->report.work) return fail(c,2,PANGU_SCHEMA_LIMIT,c->where_s,c->where_v);
 c->report.work+=amount;return 0;
}
static const pangu_json_token *token(context *c,const pangu_schema_view *d,pangu_u64 i)
{
 if(i>=d->result->count) { fail(c,10,PANGU_SCHEMA_DEFINITION,c->where_s,c->where_v);return (void *)0; }
 const pangu_json_token *t=&d->result->tokens[i];
 if(t->kind<PANGU_JSON_NULL || t->kind>PANGU_JSON_OBJECT || t->start>=t->end || t->end>d->input.length || t->next<=i || t->next>d->result->count) {
  fail(c,10,PANGU_SCHEMA_DEFINITION,c->where_s,c->where_v);return (void *)0;
 }
 return t;
}
static pangu_i32 pair(context *c,const pangu_schema_view *d,pangu_u64 object,pangu_u64 at,pangu_u64 *value,pangu_u64 *next)
{
 const pangu_json_token *o=token(c,d,object),*k=token(c,d,at);if(!o || !k) return c->report.status;
 if(o->kind!=PANGU_JSON_OBJECT || at+1>=o->next || k->kind!=PANGU_JSON_STRING || !(k->flags&PANGU_JSON_KEY) || k->parent!=object || k->next!=at+1) return fail(c,10,PANGU_SCHEMA_DEFINITION,c->where_s,c->where_v);
 const pangu_json_token *v=token(c,d,at+1);if(!v) return c->report.status;
 if(v->parent!=object || v->next>o->next || (v->flags&PANGU_JSON_KEY)) return fail(c,10,PANGU_SCHEMA_DEFINITION,c->where_s,c->where_v);
 if(charge(c,1)) return c->report.status;
 *value=at+1;*next=v->next;return 0;
}
static pangu_i32 strings_equal(context *c,const pangu_schema_view *a,pangu_u64 ia,const pangu_schema_view *b,pangu_u64 ib,pangu_u32 *equal)
{
 const pangu_json_token *x=token(c,a,ia),*y=token(c,b,ib);if(!x || !y) return c->report.status;
 if(charge(c,1) || charge(c,x->end-x->start) || charge(c,y->end-y->start)) return c->report.status;
 pangu_i32 rc=pangu_json_string_equal(a->input,a->result,ia,b->input,b->result,ib,equal);
 return rc ? fail(c,rc,PANGU_SCHEMA_DEFINITION,c->where_s,c->where_v) : 0;
}
static pangu_i32 find(context *c,const pangu_schema_view *d,pangu_u64 object,const pangu_schema_view *keydoc,pangu_u64 key,pangu_u64 *out)
{
 const pangu_json_token *o=token(c,d,object);if(!o) return c->report.status;
 if(o->kind!=PANGU_JSON_OBJECT) return fail(c,10,PANGU_SCHEMA_DEFINITION,c->where_s,c->where_v);
 pangu_u64 at=object+1,found=NONE;
 while(at<o->next) {
  pangu_u64 v,next;pangu_u32 equal;
  if(pair(c,d,object,at,&v,&next) || strings_equal(c,d,at,keydoc,key,&equal)) return c->report.status;
  if(equal) found=v;
  at=next;
 }
 *out=found;return 0;
}
static int bytes_equal(const unsigned char *p,pangu_u64 n,const char *word,pangu_u64 length)
{
 if(n!=length) return 0;
 for(pangu_u64 i=0;i<n;i++) if(p[i]!=(unsigned char)word[i]) return 0;
 return 1;
}
#define WORD(p,n,s) bytes_equal(p,n,s,sizeof(s)-1)
/* Non-ASCII/non-representable or long names cannot equal an ASCII keyword. */
static pangu_i32 word(context *c,pangu_u64 i,unsigned char *bytes,pangu_u64 *length,pangu_u32 *complete)
{
 const pangu_json_token *t=token(c,c->s,i);if(!t) return c->report.status;
 if(t->kind!=PANGU_JSON_STRING) return fail(c,16,PANGU_SCHEMA_DEFINITION,i,c->where_v);
 if(charge(c,1) || charge(c,t->end-t->start)) return c->report.status;
 pangu_i32 rc=pangu_json_string_decode(c->s->input,c->s->result,i,bytes,32,length);
 *complete=0;if(rc==2 || rc==9) return 0;
 if(rc) return fail(c,rc,PANGU_SCHEMA_DEFINITION,i,c->where_v);
 *complete=1;return 0;
}
static pangu_i32 sensitive(context *c,pangu_u64 key)
{
 unsigned char b[32];pangu_u64 n;pangu_u32 complete;if(word(c,key,b,&n,&complete)) return c->report.status;
 if(!complete) return 0;
 if(WORD(b,n,"constructor") || WORD(b,n,"__defineGetter__") || WORD(b,n,"__defineSetter__") || WORD(b,n,"hasOwnProperty") || WORD(b,n,"__lookupGetter__") || WORD(b,n,"__lookupSetter__") || WORD(b,n,"isPrototypeOf") || WORD(b,n,"propertyIsEnumerable") || WORD(b,n,"toString") || WORD(b,n,"valueOf") || WORD(b,n,"__proto__") || WORD(b,n,"toLocaleString")) return fail(c,9,PANGU_SCHEMA_UNSUPPORTED,key,c->where_v);
 return 0;
}
static pangu_i32 type_name(context *c,pangu_u64 i,pangu_u32 *mask)
{
 unsigned char b[32];pangu_u64 n;pangu_u32 complete;if(word(c,i,b,&n,&complete)) return c->report.status;
 if(complete) {
  if(WORD(b,n,"null")) *mask|=T_NULL;
  else if(WORD(b,n,"boolean")) *mask|=T_BOOL;
  else if(WORD(b,n,"number")) *mask|=T_NUMBER;
  else if(WORD(b,n,"integer")) *mask|=T_INTEGER;
  else if(WORD(b,n,"string")) *mask|=T_STRING;
  else if(WORD(b,n,"array")) *mask|=T_ARRAY;
  else if(WORD(b,n,"object")) *mask|=T_OBJECT;
  else return fail(c,16,PANGU_SCHEMA_DEFINITION,i,c->where_v);
  return 0;
 }
 return fail(c,16,PANGU_SCHEMA_DEFINITION,i,c->where_v);
}
static pangu_i32 number_bits(context *c,const pangu_schema_view *d,pangu_u64 i,pangu_u64 *bits)
{
 const pangu_json_token *t=token(c,d,i);if(!t) return c->report.status;
 if(charge(c,t->end-t->start)) return c->report.status;
 pangu_i32 rc=pangu_json_number_bits(d->input,d->result,i,bits);
 return rc ? fail(c,rc,PANGU_SCHEMA_DEFINITION,c->where_s,c->where_v) : 0;
}
static pangu_i32 scalar_definition(context *c,pangu_u64 i)
{
 const pangu_json_token *t=token(c,c->s,i);if(!t) return c->report.status;
 if(t->kind==PANGU_JSON_OBJECT || t->kind==PANGU_JSON_ARRAY) return fail(c,9,PANGU_SCHEMA_UNSUPPORTED,i,c->where_v);
 if(t->kind==PANGU_JSON_NUMBER) {
  pangu_u64 bits;if(number_bits(c,c->s,i,&bits)) return c->report.status;
  if((bits&0x7fffffffffffffffULL)>=0x7ff0000000000000ULL) return fail(c,9,PANGU_SCHEMA_UNSUPPORTED,i,c->where_v);
 }
 return 0;
}
static pangu_i32 read_meta(context *c,pangu_u64 node,meta *m)
{
 m->properties=NONE;m->required=NONE;m->items=NONE;m->enumeration=NONE;m->constant=NONE;m->types=0;m->additional=1;
 const pangu_json_token *s=token(c,c->s,node);if(!s) return c->report.status;m->form=s->kind;
 if(s->kind==PANGU_JSON_TRUE || s->kind==PANGU_JSON_FALSE) return 0;
 if(s->kind!=PANGU_JSON_OBJECT) return fail(c,16,PANGU_SCHEMA_DEFINITION,node,c->where_v);
 pangu_u64 at=node+1,type=NONE,extra=NONE;
 while(at<s->next) {
  pangu_u64 value,next,n;unsigned char b[32];pangu_u32 complete;
  if(pair(c,c->s,node,at,&value,&next) || word(c,at,b,&n,&complete)) return c->report.status;
  if(!complete) return fail(c,9,PANGU_SCHEMA_UNSUPPORTED,at,c->where_v);
  if(WORD(b,n,"type")) type=value;
  else if(WORD(b,n,"properties")) m->properties=value;
  else if(WORD(b,n,"required")) m->required=value;
  else if(WORD(b,n,"items")) m->items=value;
  else if(WORD(b,n,"additionalProperties")) extra=value;
  else if(WORD(b,n,"enum")) m->enumeration=value;
  else if(WORD(b,n,"const")) m->constant=value;
  else if(!(WORD(b,n,"title") || WORD(b,n,"description") || WORD(b,n,"$comment"))) return fail(c,9,PANGU_SCHEMA_UNSUPPORTED,at,c->where_v);
  at=next;
 }
 if(type!=NONE) {
  const pangu_json_token *t=token(c,c->s,type);if(!t) return c->report.status;
  if(t->kind==PANGU_JSON_STRING) { if(type_name(c,type,&m->types)) return c->report.status; }
  else if(t->kind==PANGU_JSON_ARRAY && t->children) {
   for(pangu_u64 i=type+1;i<t->next;) { const pangu_json_token *v=token(c,c->s,i);if(!v) return c->report.status;if(type_name(c,i,&m->types)) return c->report.status;i=v->next; }
  } else return fail(c,16,PANGU_SCHEMA_DEFINITION,type,c->where_v);
 }
 if(m->properties!=NONE) {
  const pangu_json_token *p=token(c,c->s,m->properties);if(!p) return c->report.status;
  if(p->kind!=PANGU_JSON_OBJECT) return fail(c,16,PANGU_SCHEMA_DEFINITION,m->properties,c->where_v);
  for(pangu_u64 i=m->properties+1;i<p->next;) { pangu_u64 v,next;if(pair(c,c->s,m->properties,i,&v,&next) || sensitive(c,i)) return c->report.status;i=next; }
 }
 if(m->required!=NONE) {
  const pangu_json_token *p=token(c,c->s,m->required);if(!p) return c->report.status;
  if(p->kind!=PANGU_JSON_ARRAY) return fail(c,16,PANGU_SCHEMA_DEFINITION,m->required,c->where_v);
  for(pangu_u64 i=m->required+1;i<p->next;) { const pangu_json_token *v=token(c,c->s,i);if(!v) return c->report.status;if(sensitive(c,i)) return c->report.status;i=v->next; }
 }
 if(m->items!=NONE) {
  const pangu_json_token *t=token(c,c->s,m->items);if(!t) return c->report.status;
  if(t->kind==PANGU_JSON_ARRAY) return fail(c,9,PANGU_SCHEMA_UNSUPPORTED,m->items,c->where_v);
  if(t->kind!=PANGU_JSON_OBJECT && t->kind!=PANGU_JSON_TRUE && t->kind!=PANGU_JSON_FALSE) return fail(c,16,PANGU_SCHEMA_DEFINITION,m->items,c->where_v);
 }
 if(extra!=NONE) {
  const pangu_json_token *t=token(c,c->s,extra);if(!t) return c->report.status;
  if(t->kind==PANGU_JSON_OBJECT) return fail(c,9,PANGU_SCHEMA_UNSUPPORTED,extra,c->where_v);
  if(t->kind!=PANGU_JSON_TRUE && t->kind!=PANGU_JSON_FALSE) return fail(c,16,PANGU_SCHEMA_DEFINITION,extra,c->where_v);
  m->additional=t->kind==PANGU_JSON_TRUE;
 }
 if(m->enumeration!=NONE) {
  const pangu_json_token *t=token(c,c->s,m->enumeration);if(!t) return c->report.status;
  if(t->kind!=PANGU_JSON_ARRAY) return fail(c,16,PANGU_SCHEMA_DEFINITION,m->enumeration,c->where_v);
  for(pangu_u64 i=m->enumeration+1;i<t->next;) { const pangu_json_token *v=token(c,c->s,i);if(!v) return c->report.status;if(charge(c,1) || scalar_definition(c,i)) return c->report.status;i=v->next; }
 }
 if(m->constant!=NONE && scalar_definition(c,m->constant)) return c->report.status;
 return 0;
}
static pangu_i32 check_frame(context *c,pangu_u64 node,pangu_u32 depth,frame *f)
{
 c->where_s=node;c->where_v=NONE;
 if(depth>c->limits->max_depth || c->nodes==c->limits->max_nodes) return fail(c,2,PANGU_SCHEMA_LIMIT,node,NONE);
 c->nodes++;if(charge(c,1)) return c->report.status;
 meta m;if(read_meta(c,node,&m)) return c->report.status;
 f->schema=node;f->value=NONE;f->properties=m.properties;f->items=m.items;f->depth=depth;f->kind=0;f->cursor=0;f->end=0;
 if(m.properties!=NONE) { const pangu_json_token *p=token(c,c->s,m.properties);if(!p) return c->report.status;f->cursor=m.properties+1;f->end=p->next; }
 return 0;
}
static pangu_i32 check_all(context *c)
{
 frame stack[64];pangu_u32 depth=1;
 if(check_frame(c,c->s->root,1,&stack[0])) return c->report.status;
 while(depth) {
  frame *f=&stack[depth-1];pangu_u64 child=NONE;c->where_s=f->schema;c->where_v=NONE;
  if(f->cursor<f->end) {
   pangu_u64 key=f->cursor,next,found;
   if(pair(c,c->s,f->properties,key,&child,&next) || find(c,c->s,f->properties,c->s,key,&found)) return c->report.status;
   f->cursor=next;if(found!=child) continue;
  } else if(f->items!=NONE) { child=f->items;f->items=NONE; }
  else { depth--;continue; }
  if(depth==64 || depth==c->limits->max_depth) return fail(c,2,PANGU_SCHEMA_LIMIT,child,NONE);
  if(check_frame(c,child,depth+1,&stack[depth])) return c->report.status;
  depth++;
 }
 return 0;
}
static pangu_i32 scalar_equal(context *c,pangu_u64 schema,pangu_u64 value,pangu_u32 *equal)
{
 const pangu_json_token *s=token(c,c->s,schema),*v=token(c,c->v,value);if(!s || !v) return c->report.status;
 *equal=0;if(charge(c,1)) return c->report.status;
 if(s->kind!=v->kind) return 0;
 if(s->kind==PANGU_JSON_STRING) return strings_equal(c,c->s,schema,c->v,value,equal);
 if(s->kind==PANGU_JSON_NUMBER) {
  pangu_u64 a,b;if(number_bits(c,c->s,schema,&a) || number_bits(c,c->v,value,&b)) return c->report.status;
  *equal=a==b || (!(a&0x7fffffffffffffffULL) && !(b&0x7fffffffffffffffULL));return 0;
 }
 *equal=1;return 0;
}
static pangu_i32 type_matches(context *c,pangu_u32 mask,pangu_u64 value,pangu_u32 *matches)
{
 const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report.status;
 *matches=1;if(!mask) return 0;
 pangu_u32 bit=v->kind==PANGU_JSON_NULL ? T_NULL : v->kind==PANGU_JSON_TRUE || v->kind==PANGU_JSON_FALSE ? T_BOOL : v->kind==PANGU_JSON_STRING ? T_STRING : v->kind==PANGU_JSON_ARRAY ? T_ARRAY : v->kind==PANGU_JSON_OBJECT ? T_OBJECT : 0;
 if(v->kind!=PANGU_JSON_NUMBER) { *matches=(mask&bit)!=0;return 0; }
 *matches=0;if(!(mask&(T_NUMBER|T_INTEGER))) return 0;
 pangu_u64 bits;if(number_bits(c,c->v,value,&bits)) return c->report.status;
 pangu_u64 magnitude=bits&0x7fffffffffffffffULL;
 if(magnitude>=0x7ff0000000000000ULL) return 0;
 if(mask&T_NUMBER) { *matches=1;return 0; }
 if(!magnitude) { *matches=1;return 0; }
 pangu_i32 exponent=(pangu_i32)(magnitude>>52)-1023;
 if(exponent<0) return 0;
 if(exponent>=52) { *matches=1;return 0; }
 *matches=(magnitude&(((pangu_u64)1<<(52-exponent))-1))==0;return 0;
}
static pangu_i32 value_frame(context *c,pangu_u64 schema,pangu_u64 value,pangu_u32 depth,frame *f)
{
 c->where_s=schema;c->where_v=value;
 if(depth>c->limits->max_depth || charge(c,1)) return fail(c,2,PANGU_SCHEMA_LIMIT,schema,value);
 meta m;if(read_meta(c,schema,&m)) return c->report.status;
 const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report.status;
 f->schema=schema;f->value=value;f->properties=m.properties;f->items=m.items;f->kind=v->kind;f->depth=depth;f->cursor=0;f->end=0;
 if(m.form==PANGU_JSON_FALSE) return fail(c,15,PANGU_SCHEMA_FALSE,schema,value);
 if(m.form==PANGU_JSON_TRUE) return 0;
 pangu_u32 matches;if(type_matches(c,m.types,value,&matches)) return c->report.status;
 if(!matches) return fail(c,15,PANGU_SCHEMA_TYPE,schema,value);
 if(m.constant!=NONE) { if(scalar_equal(c,m.constant,value,&matches)) return c->report.status;if(!matches) return fail(c,15,PANGU_SCHEMA_ENUM,m.constant,value); }
 if(m.enumeration!=NONE) {
  const pangu_json_token *e=token(c,c->s,m.enumeration);if(!e) return c->report.status;matches=0;
  for(pangu_u64 i=m.enumeration+1;i<e->next;) {
   const pangu_json_token *x=token(c,c->s,i);if(!x) return c->report.status;
   if(scalar_equal(c,i,value,&matches)) return c->report.status;
   if(matches) break;
   i=x->next;
  }
  if(!matches) return fail(c,15,PANGU_SCHEMA_ENUM,m.enumeration,value);
 }
 if(v->kind==PANGU_JSON_OBJECT) {
  if(m.required!=NONE) {
   const pangu_json_token *r=token(c,c->s,m.required);if(!r) return c->report.status;
   for(pangu_u64 i=m.required+1;i<r->next;) {
    const pangu_json_token *x=token(c,c->s,i);if(!x) return c->report.status;pangu_u64 found;
    if(find(c,c->v,value,c->s,i,&found)) return c->report.status;
    if(found==NONE) return fail(c,15,PANGU_SCHEMA_REQUIRED,i,value);
    i=x->next;
   }
  }
  if(!m.additional) {
   for(pangu_u64 i=value+1;i<v->next;) {
    pangu_u64 child,next,found=NONE;if(pair(c,c->v,value,i,&child,&next)) return c->report.status;
    if(m.properties!=NONE && find(c,c->s,m.properties,c->v,i,&found)) return c->report.status;
    if(found==NONE) return fail(c,15,PANGU_SCHEMA_ADDITIONAL,schema,i);
    i=next;
   }
  }
  if(m.properties!=NONE) { const pangu_json_token *p=token(c,c->s,m.properties);if(!p) return c->report.status;f->cursor=m.properties+1;f->end=p->next; }
 } else if(v->kind==PANGU_JSON_ARRAY && m.items!=NONE) { f->cursor=value+1;f->end=v->next; }
 return 0;
}
static pangu_i32 validate_all(context *c)
{
 frame stack[64];pangu_u32 depth=1;if(value_frame(c,c->s->root,c->v->root,1,&stack[0])) return c->report.status;
 while(depth) {
  frame *f=&stack[depth-1];c->where_s=f->schema;c->where_v=f->value;
  if(f->cursor==f->end) { depth--;continue; }
  pangu_u64 schema=NONE,value=NONE;
  if(f->kind==PANGU_JSON_OBJECT) {
   pangu_u64 key=f->cursor,next,found;
   if(pair(c,c->s,f->properties,key,&schema,&next) || find(c,c->s,f->properties,c->s,key,&found)) return c->report.status;
   f->cursor=next;if(found!=schema) continue;
   if(find(c,c->v,f->value,c->s,key,&value)) return c->report.status;
   if(value==NONE) continue;
  } else {
   value=f->cursor;schema=f->items;const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report.status;
   if(v->next>f->end) return fail(c,10,PANGU_SCHEMA_DEFINITION,schema,value);
   f->cursor=v->next;
  }
  if(depth==64 || depth==c->limits->max_depth) return fail(c,2,PANGU_SCHEMA_LIMIT,schema,value);
  if(value_frame(c,schema,value,depth+1,&stack[depth])) return c->report.status;
  depth++;
 }
 return 0;
}
static pangu_i32 view_ok(const pangu_schema_view *d)
{
 if(!d || !range_ok(d->input.data,d->input.length) || !d->result) return 1;
 const pangu_json_result *r=d->result;
 if(r->status || r->reserved || !r->tokens || !r->count || r->error_offset!=d->input.length || r->count>~(pangu_u64)0/48 || !range_ok(r->tokens,r->count*48)) return 10;
 return d->root<r->count ? 0 : 1;
}
static int output_ok(const pangu_schema_view *d,pangu_schema_report *out)
{
 pangu_u64 p=(pangu_u64)out;
 return !overlap(p,32,(pangu_u64)d,sizeof(*d)) && !overlap(p,32,(pangu_u64)d->input.data,d->input.length)
  && !overlap(p,32,(pangu_u64)d->result,32) && !overlap(p,32,(pangu_u64)d->result->tokens,d->result->count*48);
}
static pangu_i32 run(const pangu_schema_view *schema,const pangu_schema_view *value,const pangu_schema_limits *limits,pangu_schema_report *out)
{
 if(!limits || !out || !range_ok(out,32) || !limits->max_work || !limits->max_nodes || !limits->max_depth || limits->max_depth>64) return 1;
 pangu_i32 rc=view_ok(schema);if(rc) return rc;if(value) { rc=view_ok(value);if(rc) return rc; }
 if(!output_ok(schema,out) || (value && !output_ok(value,out)) || overlap((pangu_u64)out,32,(pangu_u64)limits,sizeof(*limits))) return 1;
 context c;c.s=schema;c.v=value;c.limits=limits;c.nodes=0;c.where_s=schema->root;c.where_v=NONE;
 c.report.status=0;c.report.reason=0;c.report.schema_token=schema->root;c.report.value_token=value ? value->root : NONE;c.report.work=0;
 rc=check_all(&c);if(!rc && value) rc=validate_all(&c);
 out->status=rc;out->reason=c.report.reason;out->schema_token=c.report.schema_token;out->value_token=c.report.value_token;out->work=c.report.work;return rc;
}
pangu_i32 pangu_schema_check(const pangu_schema_view *schema,const pangu_schema_limits *limits,pangu_schema_report *out)
{
 return run(schema,(void *)0,limits,out);
}
pangu_i32 pangu_schema_validate(const pangu_schema_view *schema,const pangu_schema_view *value,const pangu_schema_limits *limits,pangu_schema_report *out)
{
 if(!value) return 1;
 return run(schema,value,limits,out);
}
