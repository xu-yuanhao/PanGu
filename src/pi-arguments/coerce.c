#include "src/bootstrap-api/pi-arguments.h"
#define NONE (~(pangu_u64)0)
typedef struct normalize {
 const pangu_schema_view *s,*v;const pangu_pi_tree_schema *schema;
 pangu_schema_limits limits;pangu_schema_report *report;
 pangu_u64 work,where_s,where_v,length,scratch_capacity,nodes;
 unsigned char *output,*scratch;pangu_u32 emitting,pass;
} normalize;
typedef struct tree_frame {
 pangu_u64 schema,value,at,end,properties,items;
 pangu_u32 kind,first,wrapped,depth;
} tree_frame;
static int range_ok(const void *p,pangu_u64 n) { return (!n || p) && n<=NONE-(pangu_u64)p; }
static int overlaps(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static pangu_i32 bad(normalize *c,pangu_i32 status,pangu_u32 reason)
{
 if(!c->report->status) { c->report->status=status;c->report->reason=reason;c->report->schema_token=c->where_s;c->report->value_token=c->where_v; }
 return status;
}
static pangu_i32 charge(normalize *c,pangu_u64 n)
{
 if(c->emitting) return 0;
 if(n>c->limits.max_work-c->work) return bad(c,2,PANGU_SCHEMA_LIMIT);
 c->work+=n;return 0;
}
static const pangu_json_token *token(normalize *c,const pangu_schema_view *d,pangu_u64 i)
{
 if(i>=d->result->count) { bad(c,10,PANGU_SCHEMA_DEFINITION);return (void *)0; }
 const pangu_json_token *t=&d->result->tokens[i];
 if(t->kind<1 || t->kind>7 || t->start>=t->end || t->end>d->input.length || t->next<=i || t->next>d->result->count) { bad(c,10,PANGU_SCHEMA_DEFINITION);return (void *)0; }
 return t;
}
static pangu_i32 pair(normalize *c,const pangu_schema_view *d,pangu_u64 object,pangu_u64 key,pangu_u64 *value,pangu_u64 *next)
{
 const pangu_json_token *o=token(c,d,object),*k=token(c,d,key);if(!o || !k) return c->report->status;
 if(o->kind!=PANGU_JSON_OBJECT || key+1>=o->next || k->kind!=PANGU_JSON_STRING || !(k->flags&PANGU_JSON_KEY) || k->parent!=object || k->next!=key+1) return bad(c,10,PANGU_SCHEMA_DEFINITION);
 const pangu_json_token *v=token(c,d,key+1);if(!v) return c->report->status;
 if(v->parent!=object || v->next>o->next) return bad(c,10,PANGU_SCHEMA_DEFINITION);
 *value=key+1;*next=v->next;return charge(c,1);
}
static pangu_i32 equal(normalize *c,const pangu_schema_view *a,pangu_u64 ia,const pangu_schema_view *b,pangu_u64 ib,pangu_u32 *same)
{
 const pangu_json_token *x=token(c,a,ia),*y=token(c,b,ib);if(!x || !y) return c->report->status;
 if(charge(c,1) || charge(c,x->end-x->start) || charge(c,y->end-y->start)) return c->report->status;
 pangu_i32 rc=pangu_json_string_equal(a->input,a->result,ia,b->input,b->result,ib,same);return rc ? bad(c,rc,PANGU_SCHEMA_DEFINITION) : 0;
}
static pangu_i32 find(normalize *c,const pangu_schema_view *d,pangu_u64 object,const pangu_schema_view *kd,pangu_u64 key,pangu_u64 *out)
{
 const pangu_json_token *o=token(c,d,object);if(!o) return c->report->status;
 pangu_u64 found=NONE,at=object+1;
 while(at<o->next) { pangu_u64 value,next;pangu_u32 same;if(pair(c,d,object,at,&value,&next) || equal(c,d,at,kd,key,&same)) return c->report->status;if(same) found=value;at=next; }
 *out=found;return 0;
}
static pangu_i32 field(normalize *c,pangu_u64 node,const char *name,pangu_u64 n,pangu_u64 *out)
{
 const pangu_json_token *s=token(c,c->s,node);if(!s) return c->report->status;*out=NONE;
 if(s->kind!=PANGU_JSON_OBJECT) return 0;
 if(charge(c,1) || charge(c,s->end-s->start)) return c->report->status;
 pangu_text_slice key={(const unsigned char *)name,n};pangu_i32 rc=pangu_json_object_find(c->s->input,c->s->result,node,key,out);
 return rc==3 ? 0 : rc ? bad(c,rc,PANGU_SCHEMA_DEFINITION) : 0;
}
static pangu_i32 view_ok(const pangu_schema_view *v)
{
 if(!v || !range_ok(v,32) || !range_ok(v->input.data,v->input.length) || !v->result || !range_ok(v->result,32)) return 1;
 const pangu_json_result *r=v->result;
 if(r->status || r->reserved || !r->tokens || !r->count || r->count>NONE/48 || !range_ok(r->tokens,r->count*48) || r->error_offset!=v->input.length) return 10;
 return v->root<r->count ? 0 : 1;
}
static pangu_i32 append(normalize *c,const unsigned char *p,pangu_u64 n)
{
 if(n>NONE-c->length || charge(c,n)) return bad(c,2,PANGU_SCHEMA_LIMIT);
 if(c->emitting) for(pangu_u64 i=0;i<n;i++) c->output[c->length+i]=p[i];
 c->length+=n;return 0;
}
static pangu_i32 raw(normalize *c,pangu_u64 value)
{
 const pangu_json_token *t=token(c,c->v,value);if(!t) return c->report->status;
 return append(c,c->v->input.data+t->start,t->end-t->start);
}
static pangu_u32 typename(normalize *c,pangu_u64 i)
{
 unsigned char b[8];pangu_u64 n=0;
 if(pangu_json_string_decode(c->s->input,c->s->result,i,b,8,&n)) return 0;
 static const char *const names[7]={"number","integer","string","boolean","null","array","object"};
 static const pangu_u32 lengths[7]={6,7,6,7,4,5,6};
 for(pangu_u32 k=0;k<7;k++) { if(n!=lengths[k]) continue;pangu_u32 same=1;for(pangu_u64 j=0;j<n;j++) if(b[j]!=(unsigned char)names[k][j]) same=0;if(same) return k+1; }
 return 0;
}
static pangu_i32 matches(normalize *c,pangu_u64 value,pangu_u32 target,pangu_u32 *yes)
{
 const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report->status;*yes=0;
 if(target==1) *yes=v->kind==PANGU_JSON_NUMBER;
 else if(target==2 && v->kind==PANGU_JSON_NUMBER) {
  pangu_u64 bits=0;pangu_i32 rc=pangu_json_number_bits(c->v->input,c->v->result,value,&bits);if(rc) return bad(c,rc,PANGU_SCHEMA_TYPE);
  pangu_u32 e=(pangu_u32)((bits>>52)&2047);pangu_u64 m=bits&0xfffffffffffffULL;
  *yes=e!=2047 && ((bits<<1)==0 || e>=1075 || (e>=1023 && !(m&(((pangu_u64)1<<(1075-e))-1))));
 } else if(target==3) *yes=v->kind==PANGU_JSON_STRING;
 else if(target==4) *yes=v->kind==PANGU_JSON_TRUE || v->kind==PANGU_JSON_FALSE;
 else if(target==5) *yes=v->kind==PANGU_JSON_NULL;
 else if(target==6) *yes=v->kind==PANGU_JSON_ARRAY;
 else if(target==7) *yes=v->kind==PANGU_JSON_OBJECT;
 return 0;
}
static pangu_i32 scalar(normalize *c,pangu_u64 value,pangu_u32 target,pangu_pi_scalar_result *out)
{
 const pangu_json_token *t=token(c,c->v,value);if(!t) return c->report->status;
 if(charge(c,t->end-t->start)) return c->report->status;
 pangu_schema_view v;v.input.data=c->v->input.data;v.input.length=c->v->input.length;v.result=c->v->result;v.root=value;
 pangu_i32 rc=pangu_pi_scalar_convert(&v,target,c->pass,c->scratch,c->scratch_capacity,t->end-t->start,out);
 return rc ? bad(c,rc,PANGU_SCHEMA_TYPE) : 0;
}
static pangu_i32 primitive(normalize *c,pangu_u64 value,const pangu_pi_scalar_result *r)
{
 if(!r->kind) return raw(c,value);
 if(r->kind==PANGU_JSON_NULL) return append(c,(const unsigned char *)"null",4);
 if(r->kind==PANGU_JSON_TRUE) return append(c,(const unsigned char *)"true",4);
 if(r->kind==PANGU_JSON_FALSE) return append(c,(const unsigned char *)"false",5);
 if(r->kind==PANGU_JSON_STRING) {
  /* Scalar replacements are ASCII words, number text, or empty; max25 bytes. */
  unsigned char b[64];pangu_u64 n=0;pangu_i32 rc=pangu_json_quote_utf8(r->text,b,64,&n);
  return rc ? bad(c,rc,PANGU_SCHEMA_TYPE) : append(c,b,n);
 }
 if(r->kind==PANGU_JSON_NUMBER) {
  if(r->number_bits==0x8000000000000000ULL) return append(c,(const unsigned char *)"-0",2);
  unsigned char b[25];pangu_u64 n=0;pangu_i32 rc=pangu_json_number_text(r->number_bits,PANGU_JSON_NUMBER_STRING,b,25,&n);
  return rc ? bad(c,rc,PANGU_SCHEMA_TYPE) : append(c,b,n);
 }
 return bad(c,9,PANGU_SCHEMA_UNSUPPORTED);
}
static pangu_i32 literal_equal(normalize *c,pangu_u64 value,const pangu_pi_scalar_result *r,pangu_u64 literal,pangu_u32 *yes)
{
 const pangu_json_token *t=token(c,c->s,literal),*v=token(c,c->v,value);if(!t || !v) return c->report->status;
 if(charge(c,t->end-t->start) || charge(c,1)) return c->report->status;
 pangu_u32 kind=r->kind ? r->kind : v->kind;*yes=0;if(kind!=t->kind) return 0;
 if(kind==PANGU_JSON_NULL || kind==PANGU_JSON_TRUE || kind==PANGU_JSON_FALSE) { *yes=1;return 0; }
 if(kind==PANGU_JSON_NUMBER) {
  pangu_u64 a=r->number_bits,b=0;pangu_i32 rc=pangu_json_number_bits(c->s->input,c->s->result,literal,&b);if(rc) return bad(c,rc,PANGU_SCHEMA_TYPE);
  if(!r->kind) { rc=pangu_json_number_bits(c->v->input,c->v->result,value,&a);if(rc) return bad(c,rc,PANGU_SCHEMA_TYPE); }
  *yes=a==b || ((a<<1)==0 && (b<<1)==0);return 0;
 }
 if(kind==PANGU_JSON_STRING) {
  if(!r->kind) return equal(c,c->v,value,c->s,literal,yes);
  unsigned char b[25];pangu_u64 n=0;pangu_i32 rc=pangu_json_string_decode(c->s->input,c->s->result,literal,b,25,&n);
  if(rc==2 || rc==9) return 0;if(rc) return bad(c,rc,PANGU_SCHEMA_TYPE);
  if(n!=r->text.length) return 0;*yes=1;for(pangu_u64 i=0;i<n;i++) if(b[i]!=r->text.data[i]) *yes=0;return 0;
 }
 return bad(c,9,PANGU_SCHEMA_UNSUPPORTED);
}
static pangu_i32 literal_candidate(normalize *c,pangu_u64 value,pangu_u64 literal,pangu_pi_scalar_result *r,pangu_u32 *yes)
{
 const pangu_json_token *t=token(c,c->s,literal);if(!t) return c->report->status;
 pangu_u32 target=t->kind==PANGU_JSON_NUMBER ? 1 : t->kind==PANGU_JSON_STRING ? 3 : t->kind==PANGU_JSON_TRUE || t->kind==PANGU_JSON_FALSE ? 4 : 0;
 if(!target) return bad(c,9,PANGU_SCHEMA_UNSUPPORTED);
 if(scalar(c,value,target,r)) return c->report->status;return literal_equal(c,value,r,literal,yes);
}
static pangu_i32 constrained(normalize *c,pangu_u64 schema,pangu_u64 value,pangu_u32 kind)
{
 pangu_u64 constraint;if(field(c,schema,kind==8 ? "const" : "enum",kind==8 ? 5 : 4,&constraint)) return c->report->status;
 if(constraint==NONE) return bad(c,16,PANGU_SCHEMA_DEFINITION);
 pangu_pi_scalar_result r;r.kind=0;r.reserved=0;r.number_bits=0;r.text.data=(void *)0;r.text.length=0;pangu_u32 yes=0;
 if(kind==8) { if(literal_candidate(c,value,constraint,&r,&yes)) return c->report->status;return yes ? primitive(c,value,&r) : raw(c,value); }
 const pangu_json_token *a=token(c,c->s,constraint);if(!a) return c->report->status;
 for(pangu_u64 i=constraint+1;i<a->next;) { const pangu_json_token *t=token(c,c->s,i);if(!t || literal_equal(c,value,&r,i,&yes)) return c->report->status;if(yes) return raw(c,value);i=t->next; }
 for(pangu_u64 i=constraint+1;i<a->next;) { const pangu_json_token *t=token(c,c->s,i);if(!t || literal_candidate(c,value,i,&r,&yes)) return c->report->status;if(yes) return primitive(c,value,&r);i=t->next; }
 return raw(c,value);
}
static int meta(pangu_u32 ch)
{
 return ch=='\\' || ch=='.' || ch=='^' || ch=='$' || ch=='*' || ch=='+' || ch=='?' || ch=='(' || ch==')' || ch=='[' || ch==']' || ch=='{' || ch=='}' || ch=='|';
}
static pangu_u32 hex(unsigned char ch)
{
 return ch>='0' && ch<='9' ? ch-'0' : ch>='a' && ch<='f' ? ch-'a'+10 : ch-'A'+10;
}
static pangu_i32 safe_key(normalize *c,pangu_u64 key)
{
 const pangu_json_token *t=token(c,c->s,key);if(!t) return c->report->status;
 if(charge(c,t->end-t->start)) return c->report->status;
 for(pangu_u64 i=t->start+1;i+1<t->end;i++) {
  pangu_u32 ch=c->s->input.data[i];
  if(ch=='\\') { ch=c->s->input.data[++i];if(ch=='u') { ch=0;for(pangu_u32 j=0;j<4;j++) ch=(ch<<4)|hex(c->s->input.data[++i]); } }
  if(meta(ch)) return bad(c,9,PANGU_SCHEMA_UNSUPPORTED);
 }
 return 0;
}
static pangu_i32 metadata(normalize *c)
{
 if(c->pass!=1) return 0;
 const pangu_json_token *root=token(c,c->s,c->s->root);if(!root) return c->report->status;
 for(pangu_u64 i=c->s->root;i<root->next;i++) {
  if(charge(c,1)) return c->report->status;pangu_u32 kind=c->schema->kinds[i];if(!kind) continue;
  c->where_s=i;const pangu_json_token *t=token(c,c->s,i);if(!t) return c->report->status;
  if(kind>9) return bad(c,9,PANGU_SCHEMA_UNSUPPORTED);
  if(t->kind!=PANGU_JSON_OBJECT) return bad(c,16,PANGU_SCHEMA_DEFINITION);
  if(kind==8 || kind==9) {
   pangu_u64 constraint;if(field(c,i,kind==8 ? "const" : "enum",kind==8 ? 5 : 4,&constraint)) return c->report->status;
   if(constraint==NONE) return bad(c,16,PANGU_SCHEMA_DEFINITION);
   const pangu_json_token *a=token(c,c->s,constraint);if(!a) return c->report->status;
   pangu_u64 at=kind==8 ? constraint : constraint+1,end=kind==8 ? a->next : a->next;
   for(;at<end;) { const pangu_json_token *x=token(c,c->s,at);if(!x) return c->report->status;
    if(x->kind!=PANGU_JSON_NUMBER && x->kind!=PANGU_JSON_STRING && x->kind!=PANGU_JSON_TRUE && x->kind!=PANGU_JSON_FALSE) return bad(c,9,PANGU_SCHEMA_UNSUPPORTED);at=x->next;
   }
  }
  if(kind==7) {
   pangu_u64 props;if(field(c,i,"properties",10,&props)) return c->report->status;
   if(props==NONE) return bad(c,16,PANGU_SCHEMA_DEFINITION);
   const pangu_json_token *p=token(c,c->s,props);if(!p) return c->report->status;
   for(pangu_u64 at=props+1;at<p->next;) { pangu_u64 v,next;if(pair(c,c->s,props,at,&v,&next) || safe_key(c,at)) return c->report->status;at=next; }
  }
 }
 return 0;
}
static pangu_i32 enter(normalize *c,pangu_u64 schema,pangu_u64 value,pangu_u32 depth,tree_frame *f)
{
 c->where_s=schema;c->where_v=value;
 if(depth>c->limits.max_depth || charge(c,1)) return bad(c,2,PANGU_SCHEMA_LIMIT);
 if(!c->emitting && ++c->nodes>c->limits.max_nodes) return bad(c,2,PANGU_SCHEMA_LIMIT);
 const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report->status;
 f->schema=schema;f->value=value;f->at=0;f->end=0;f->properties=NONE;f->items=NONE;f->kind=0;f->first=1;f->wrapped=0;f->depth=depth;
 if(schema==NONE) return raw(c,value);
 pangu_u32 kind=0;pangu_pi_scalar_result r;r.kind=0;r.reserved=0;r.number_bits=0;r.text.data=(void *)0;r.text.length=0;
 if(c->pass==1) {
  kind=c->schema->kinds[schema];
  if(kind==8 || kind==9) return constrained(c,schema,value,kind);
  if(kind>=1 && kind<=5) { if(scalar(c,value,kind,&r)) return c->report->status;return primitive(c,value,&r); }
 } else {
  pangu_u64 types;if(field(c,schema,"type",4,&types)) return c->report->status;
  if(types!=NONE) {
   const pangu_json_token *t=token(c,c->s,types);if(!t) return c->report->status;
   if(t->kind==PANGU_JSON_STRING) kind=typename(c,types);
   else if(t->kind==PANGU_JSON_ARRAY) {
    pangu_u32 matched=0,count=0;for(pangu_u64 i=types+1;i<t->next;) { const pangu_json_token *x=token(c,c->s,i);if(!x) return c->report->status;pangu_u32 yes=0;if(matches(c,value,typename(c,i),&yes)) return c->report->status;matched|=yes;count++;i=x->next; }
    if(count<=1 || !matched) for(pangu_u64 i=types+1;i<t->next;) {
     const pangu_json_token *x=token(c,c->s,i);if(!x) return c->report->status;pangu_u32 k=typename(c,i);
     if(k<=5 && k) { if(scalar(c,value,k,&r)) return c->report->status;if(r.kind) return primitive(c,value,&r); }i=x->next;
    }
    for(pangu_u64 i=types+1;i<t->next;) { const pangu_json_token *x=token(c,c->s,i);if(!x) return c->report->status;pangu_u32 k=typename(c,i);if((k==6 && v->kind==PANGU_JSON_ARRAY)||(k==7 && v->kind==PANGU_JSON_OBJECT)) { kind=k;break; }i=x->next; }
   }
  }
  if(kind>=1 && kind<=5) { if(scalar(c,value,kind,&r)) return c->report->status;return primitive(c,value,&r); }
 }
 if(kind==7 && v->kind==PANGU_JSON_OBJECT) {
  if(field(c,schema,"properties",10,&f->properties)) return c->report->status;
  f->kind=PANGU_JSON_OBJECT;f->at=value+1;f->end=v->next;return append(c,(const unsigned char *)"{",1);
 }
 if(kind==6 && (v->kind==PANGU_JSON_ARRAY || c->pass==1)) {
  if(field(c,schema,"items",5,&f->items)) return c->report->status;
  if(f->items==NONE) return bad(c,9,PANGU_SCHEMA_UNSUPPORTED);
  f->kind=PANGU_JSON_ARRAY;f->wrapped=v->kind!=PANGU_JSON_ARRAY;f->at=f->wrapped ? value : value+1;f->end=f->wrapped ? value+1 : v->next;
  return append(c,(const unsigned char *)"[",1);
 }
 return raw(c,value);
}
static pangu_i32 walk(normalize *c)
{
 tree_frame stack[64];pangu_u32 depth=1;
 if(enter(c,c->s->root,c->v->root,1,&stack[0])) return c->report->status;
 while(depth) {
  tree_frame *f=&stack[depth-1];c->where_s=f->schema;c->where_v=f->value;
  if(f->at==f->end) { if(f->kind && append(c,(const unsigned char *)(f->kind==PANGU_JSON_OBJECT ? "}" : "]"),1)) return c->report->status;depth--;continue; }
  pangu_u64 value,child=NONE;
  if(f->kind==PANGU_JSON_OBJECT) {
   pangu_u64 key=f->at,next,last;if(pair(c,c->v,f->value,key,&value,&next) || find(c,c->v,f->value,c->v,key,&last)) return c->report->status;f->at=next;if(value!=last) continue;
   if(f->properties!=NONE && find(c,c->s,f->properties,c->v,key,&child)) return c->report->status;
   if(!f->first && append(c,(const unsigned char *)",",1)) return c->report->status;f->first=0;
   const pangu_json_token *k=token(c,c->v,key);if(!k || append(c,c->v->input.data+k->start,k->end-k->start) || append(c,(const unsigned char *)":",1)) return c->report->status;
  } else {
   value=f->at;child=f->items;const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report->status;f->at=f->wrapped ? f->end : v->next;
   if(!f->first && append(c,(const unsigned char *)",",1)) return c->report->status;f->first=0;
  }
  if(depth==64 || depth==c->limits.max_depth) return bad(c,2,PANGU_SCHEMA_LIMIT);
  if(enter(c,child,value,depth+1,&stack[depth])) return c->report->status;depth++;
 }
 return 0;
}
pangu_i32 pangu_pi_tree_convert(const pangu_pi_tree_schema *s,const pangu_schema_view *v,pangu_u32 pass,pangu_schema_limits limits,unsigned char *scratch,pangu_u64 scratch_capacity,unsigned char *output,pangu_u64 capacity,pangu_u64 *written,pangu_schema_report *report)
{
 if(!s || !range_ok(s,48) || (pass!=1 && pass!=2) || !limits.max_work || !limits.max_nodes || !limits.max_depth || limits.max_depth>64 || !written || !report || !range_ok(scratch,scratch_capacity) || !range_ok(output,capacity) || !range_ok(written,8) || !range_ok(report,32) || s->kind_count>NONE/4 || !range_ok(s->kinds,s->kind_count*4)) return 1;
 pangu_i32 rc=view_ok(&s->json);if(rc) return rc;rc=view_ok(v);if(rc) return rc;
 if((pass==1 || s->kind_count) && s->kind_count!=s->json.result->count) return 1;
 if((s->json.result->tokens[s->json.root].flags&PANGU_JSON_KEY) || (v->result->tokens[v->root].flags&PANGU_JSON_KEY)) return 1;
 pangu_u64 read[9],rn[9],write[4],wn[4];
 read[0]=(pangu_u64)s;rn[0]=48;read[1]=(pangu_u64)v;rn[1]=32;read[2]=(pangu_u64)s->json.result;rn[2]=32;read[3]=(pangu_u64)v->result;rn[3]=32;
 read[4]=(pangu_u64)s->json.input.data;rn[4]=s->json.input.length;read[5]=(pangu_u64)v->input.data;rn[5]=v->input.length;read[6]=(pangu_u64)s->json.result->tokens;rn[6]=s->json.result->count*48;read[7]=(pangu_u64)v->result->tokens;rn[7]=v->result->count*48;read[8]=(pangu_u64)s->kinds;rn[8]=s->kind_count*4;
 write[0]=(pangu_u64)scratch;wn[0]=scratch_capacity;write[1]=(pangu_u64)output;wn[1]=capacity;write[2]=(pangu_u64)written;wn[2]=8;write[3]=(pangu_u64)report;wn[3]=32;
 for(pangu_u32 i=0;i<4;i++) { for(pangu_u32 j=0;j<9;j++) if(overlaps(write[i],wn[i],read[j],rn[j])) return 1;for(pangu_u32 j=i+1;j<4;j++) if(overlaps(write[i],wn[i],write[j],wn[j])) return 1; }
 report->status=0;report->reason=0;report->schema_token=s->json.root;report->value_token=v->root;report->work=0;
 normalize c;c.s=&s->json;c.v=v;c.schema=s;c.limits=limits;c.report=report;c.work=0;c.where_s=s->json.root;c.where_v=v->root;c.length=0;c.nodes=0;c.output=output;c.scratch=scratch;c.scratch_capacity=scratch_capacity;c.emitting=0;c.pass=pass;
 pangu_schema_report checked;checked.status=0;checked.reason=0;checked.schema_token=NONE;checked.value_token=NONE;checked.work=0;
 rc=pangu_schema_check(&s->json,&limits,&checked);c.work=checked.work;
 if(rc) { report->status=rc;report->reason=checked.reason;report->schema_token=checked.schema_token;report->value_token=checked.value_token;report->work=c.work;return rc; }
 rc=metadata(&c);if(!rc) rc=walk(&c);report->work=c.work;if(rc) return rc;
 pangu_u64 need=c.length;if(output && need>capacity) return bad(&c,2,PANGU_SCHEMA_LIMIT);
 if(output) { c.emitting=1;c.length=0;rc=walk(&c);if(rc) return rc; }
 *written=need;return 0;
}
