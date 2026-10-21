#include "src/bootstrap-api/pi-arguments.h"
#define NONE (~(pangu_u64)0)
typedef struct normalize {
 const pangu_schema_view *s,*v;pangu_schema_limits limits;pangu_schema_report *report;
 pangu_u64 *drops,capacity,count,work,where_s,where_v,length;unsigned char *output;pangu_u32 emitting;
} normalize;
typedef struct plan_frame { pangu_u64 schema,value,at,end,properties,required,items;pangu_u32 kind,depth; } plan_frame;
typedef struct emit_frame { pangu_u64 node,at,end;pangu_u32 kind,first; } emit_frame;
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
static pangu_i32 required(normalize *c,pangu_u64 array,pangu_u64 key,pangu_u32 *yes)
{
 *yes=0;if(array==NONE) return 0;const pangu_json_token *a=token(c,c->s,array);if(!a) return c->report->status;
 for(pangu_u64 i=array+1;i<a->next;) { const pangu_json_token *t=token(c,c->s,i);if(!t || equal(c,c->s,i,c->s,key,yes)) return c->report->status;if(*yes) return 0;i=t->next; }
 return 0;
}
static pangu_i32 null_rejected(normalize *c,pangu_u64 schema,pangu_u64 value,pangu_u32 *rejected)
{
 const pangu_json_token *t=token(c,c->s,schema);if(!t) return c->report->status;
 /* Pi getSubSchemaValidator catches WeakMap.set on primitive Schema keys. */
 if(t->kind==PANGU_JSON_TRUE || t->kind==PANGU_JSON_FALSE) { *rejected=0;return charge(c,1); }
 if(c->work==c->limits.max_work) return bad(c,2,PANGU_SCHEMA_LIMIT);
 pangu_schema_view s,v;s.input=c->s->input;s.result=c->s->result;s.root=schema;v.input=c->v->input;v.result=c->v->result;v.root=value;
 pangu_schema_limits limits=c->limits;limits.max_work-=c->work;pangu_schema_report r;r.status=0;r.reason=0;r.schema_token=NONE;r.value_token=NONE;r.work=0;
 pangu_i32 rc=pangu_schema_validate(&s,&v,&limits,&r);c->work+=r.work;
 if(rc!=0 && rc!=15) return bad(c,rc,r.reason);
 *rejected=rc==15;return 0;
}
static pangu_i32 enter_plan(normalize *c,pangu_u64 schema,pangu_u64 value,pangu_u32 depth,plan_frame *f)
{
 c->where_s=schema;c->where_v=value;
 if(depth>c->limits.max_depth || charge(c,1)) return bad(c,2,PANGU_SCHEMA_LIMIT);
 const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report->status;
 f->schema=schema;f->value=value;f->kind=v->kind;f->depth=depth;f->at=0;f->end=0;f->properties=NONE;f->required=NONE;f->items=NONE;
 if(v->kind==PANGU_JSON_OBJECT) {
  if(field(c,schema,"properties",10,&f->properties) || field(c,schema,"required",8,&f->required)) return c->report->status;
  if(f->properties!=NONE) { const pangu_json_token *p=token(c,c->s,f->properties);if(!p) return c->report->status;f->at=f->properties+1;f->end=p->next; }
 } else if(v->kind==PANGU_JSON_ARRAY) {
  if(field(c,schema,"items",5,&f->items)) return c->report->status;
  if(f->items!=NONE) { f->at=value+1;f->end=v->next; }
 }
 return 0;
}
static pangu_i32 plan(normalize *c)
{
 plan_frame stack[64];pangu_u32 depth=1;
 if(enter_plan(c,c->s->root,c->v->root,1,&stack[0])) return c->report->status;
 while(depth) {
  plan_frame *f=&stack[depth-1];c->where_s=f->schema;c->where_v=f->value;
  if(f->at==f->end) { depth--;continue; }
  pangu_u64 child,value;
  if(f->kind==PANGU_JSON_OBJECT) {
   pangu_u64 key=f->at,next,last;
   if(pair(c,c->s,f->properties,key,&child,&next) || find(c,c->s,f->properties,c->s,key,&last)) return c->report->status;
   f->at=next;if(last!=child) continue;
   if(find(c,c->v,f->value,c->s,key,&value)) return c->report->status;
   if(value==NONE) continue;
   const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report->status;
   if(v->kind==PANGU_JSON_NULL) {
    pangu_u32 yes;if(required(c,f->required,key,&yes)) return c->report->status;
    if(!yes) {
     if(null_rejected(c,child,value,&yes)) return c->report->status;
     if(yes) { if(c->count==c->capacity) return bad(c,2,PANGU_SCHEMA_LIMIT);c->drops[c->count++]=value; }
    }
    continue;
   }
  } else {
   value=f->at;child=f->items;const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report->status;f->at=v->next;
  }
  if(depth==64 || depth==c->limits.max_depth) return bad(c,2,PANGU_SCHEMA_LIMIT);
  if(enter_plan(c,child,value,depth+1,&stack[depth])) return c->report->status;
  depth++;
 }
 return 0;
}
static pangu_i32 affected(normalize *c,pangu_u64 node,pangu_u64 end,pangu_u32 *yes)
{
 *yes=0;
 for(pangu_u64 i=0;i<c->count;i++) { if(charge(c,1)) return c->report->status;if(c->drops[i]>=node && c->drops[i]<end) { *yes=1;break; } }
 return 0;
}
static pangu_i32 append(normalize *c,const unsigned char *bytes,pangu_u64 n)
{
 if(n>NONE-c->length || charge(c,n)) return bad(c,2,PANGU_SCHEMA_LIMIT);
 if(c->emitting) for(pangu_u64 i=0;i<n;i++) c->output[c->length+i]=bytes[i];
 c->length+=n;return 0;
}
static pangu_i32 open_emit(normalize *c,pangu_u64 node,emit_frame *f)
{
 c->where_v=node;const pangu_json_token *t=token(c,c->v,node);if(!t) return c->report->status;
 f->node=node;f->at=0;f->end=0;f->kind=t->kind;f->first=1;pangu_u32 yes;
 if(charge(c,1) || affected(c,node,t->next,&yes)) return c->report->status;
 if(!yes || (t->kind!=PANGU_JSON_OBJECT && t->kind!=PANGU_JSON_ARRAY)) return append(c,c->v->input.data+t->start,t->end-t->start);
 f->at=node+1;f->end=t->next;return append(c,(const unsigned char *)(t->kind==PANGU_JSON_OBJECT ? "{" : "["),1);
}
static pangu_i32 emit(normalize *c)
{
 emit_frame stack[64];pangu_u32 depth=1;
 if(open_emit(c,c->v->root,&stack[0])) return c->report->status;
 while(depth) {
  emit_frame *f=&stack[depth-1];c->where_v=f->node;
  if(f->at==f->end) { if(f->end && append(c,(const unsigned char *)(f->kind==PANGU_JSON_OBJECT ? "}" : "]"),1)) return c->report->status;depth--;continue; }
  pangu_u64 value;
  if(f->kind==PANGU_JSON_OBJECT) {
   pangu_u64 key=f->at,next,last;pangu_u32 drop=0;
   if(pair(c,c->v,f->node,key,&value,&next) || find(c,c->v,f->node,c->v,key,&last)) return c->report->status;
   f->at=next;
   for(pangu_u64 i=0;i<c->count;i++) { if(charge(c,1)) return c->report->status;if(c->drops[i]==last) { drop=1;break; } }
   if(drop) continue;
   if(!f->first && append(c,(const unsigned char *)",",1)) return c->report->status;
   f->first=0;const pangu_json_token *k=token(c,c->v,key);if(!k || append(c,c->v->input.data+k->start,k->end-k->start) || append(c,(const unsigned char *)":",1)) return c->report->status;
  } else {
   value=f->at;const pangu_json_token *v=token(c,c->v,value);if(!v) return c->report->status;f->at=v->next;
   if(!f->first && append(c,(const unsigned char *)",",1)) return c->report->status;
   f->first=0;
  }
  if(depth==64 || depth==c->limits.max_depth) return bad(c,2,PANGU_SCHEMA_LIMIT);
  if(open_emit(c,value,&stack[depth])) return c->report->status;
  depth++;
 }
 return 0;
}
static pangu_i32 view_ok(const pangu_schema_view *v)
{
 if(!v || !range_ok(v,32) || !range_ok(v->input.data,v->input.length) || !v->result || !range_ok(v->result,32)) return 1;
 const pangu_json_result *r=v->result;
 if(r->status || r->reserved || !r->tokens || !r->count || r->count>NONE/48 || !range_ok(r->tokens,r->count*48) || r->error_offset!=v->input.length) return 10;
 return v->root<r->count ? 0 : 1;
}
pangu_i32 pangu_pi_optional_nulls(const pangu_schema_view *s,const pangu_schema_view *v,pangu_schema_limits limits,pangu_u64 *drops,pangu_u64 drop_capacity,unsigned char *output,pangu_u64 capacity,pangu_u64 *written,pangu_schema_report *report)
{
 if(!limits.max_work || !limits.max_nodes || !limits.max_depth || limits.max_depth>64 || drop_capacity>NONE/8 || !written || !report || !range_ok(drops,drop_capacity*8) || !range_ok(output,capacity) || !range_ok(written,8) || !range_ok(report,32)) return 1;
 pangu_i32 rc=view_ok(s);if(rc) return rc;rc=view_ok(v);if(rc) return rc;
 pangu_u64 read[8],rn[8],write[4],wn[4];
 read[0]=(pangu_u64)s;rn[0]=32;read[1]=(pangu_u64)v;rn[1]=32;read[2]=(pangu_u64)s->result;rn[2]=32;read[3]=(pangu_u64)v->result;rn[3]=32;
 read[4]=(pangu_u64)s->input.data;rn[4]=s->input.length;read[5]=(pangu_u64)v->input.data;rn[5]=v->input.length;read[6]=(pangu_u64)s->result->tokens;rn[6]=s->result->count*48;read[7]=(pangu_u64)v->result->tokens;rn[7]=v->result->count*48;
 write[0]=(pangu_u64)drops;wn[0]=drop_capacity*8;write[1]=(pangu_u64)output;wn[1]=capacity;write[2]=(pangu_u64)written;wn[2]=8;write[3]=(pangu_u64)report;wn[3]=32;
 for(pangu_u32 i=0;i<4;i++) { for(pangu_u32 j=0;j<8;j++) if(overlaps(write[i],wn[i],read[j],rn[j])) return 1;for(pangu_u32 j=i+1;j<4;j++) if(overlaps(write[i],wn[i],write[j],wn[j])) return 1; }
 report->status=0;report->reason=0;report->schema_token=s->root;report->value_token=v->root;report->work=0;
 normalize c;c.s=s;c.v=v;c.limits=limits;c.report=report;c.drops=drops;c.capacity=drop_capacity;c.count=0;c.work=0;c.where_s=s->root;c.where_v=v->root;c.length=0;c.output=output;c.emitting=0;
 pangu_schema_report checked;checked.status=0;checked.reason=0;checked.schema_token=NONE;checked.value_token=NONE;checked.work=0;rc=pangu_schema_check(s,&limits,&checked);c.work=checked.work;
 if(rc) { report->status=rc;report->reason=checked.reason;report->schema_token=checked.schema_token;report->value_token=checked.value_token;report->work=c.work;return rc; }
 rc=plan(&c);if(!rc) rc=emit(&c);report->work=c.work;if(rc) return rc;
 pangu_u64 need=c.length;
 if(output && need>capacity) return bad(&c,2,PANGU_SCHEMA_LIMIT);
 if(output) { c.emitting=1;c.length=0;rc=emit(&c);if(rc) return rc; }
 *written=need;return 0;
}
