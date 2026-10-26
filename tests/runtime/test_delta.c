#define _GNU_SOURCE
#include "../../src/runtime/delta.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <json-c/json.h>
#ifdef PG_DELTA_FAULTS
static unsigned tracking,calls,fail_at,path_keys;
static int fault(void){return tracking&&++calls==fail_at;}
const char *__real_json_object_to_json_string_ext(struct json_object *,int);
const char *__wrap_json_object_to_json_string_ext(struct json_object *o,int flags){++path_keys;return fault()?0:__real_json_object_to_json_string_ext(o,flags);}
int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
struct json_object *__real_json_object_new_array_ext(int);
struct json_object *__real_json_object_new_string_len(const char *,int);
int __real_json_object_array_add(struct json_object *,struct json_object *);
int __real_json_object_array_put_idx(struct json_object *,size_t,struct json_object *);
int __real_json_object_object_add(struct json_object *,const char *,struct json_object *);
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
int __wrap_json_object_deep_copy(struct json_object *src,struct json_object **out,json_c_shallow_copy_fn *fn){if(fault()){*out=json_object_new_object();return -1;}return __real_json_object_deep_copy(src,out,fn);}
struct json_object *__wrap_json_object_new_array_ext(int n){return fault()?0:__real_json_object_new_array_ext(n);}
struct json_object *__wrap_json_object_new_string_len(const char *s,int n){return fault()?0:__real_json_object_new_string_len(s,n);}
int __wrap_json_object_array_add(struct json_object *a,struct json_object *v){return fault()?-1:__real_json_object_array_add(a,v);}
int __wrap_json_object_array_put_idx(struct json_object *a,size_t i,struct json_object *v){return fault()?-1:__real_json_object_array_put_idx(a,i,v);}
int __wrap_json_object_object_add(struct json_object *o,const char *key,struct json_object *v){return fault()?-1:__real_json_object_object_add(o,key,v);}
void *__wrap_malloc(size_t n){return fault()?0:__real_malloc(n);}
void *__wrap_calloc(size_t n,size_t size){return fault()?0:__real_calloc(n,size);}
struct json_object *__real_json_object_new_object(void);
struct json_object *__real_json_object_new_int64(int64_t);
struct json_object *__wrap_json_object_new_object(void){return fault()?0:__real_json_object_new_object();}
struct json_object *__wrap_json_object_new_int64(int64_t n){return fault()?0:__real_json_object_new_int64(n);}
static void failures(void){
 struct json_object *value=json_tokener_parse("{\"text\":\"abc\",\"xs\":[1,2,3]}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"def\"],[\"a\",[\"text\"],\"more\"],[\"t\",[\"text\"],1],[\"s\",[\"new\"],{\"x\":2}],[\"s\",[\"xs\",3],4],[\"p\",[\"xs\"],1,1,[7,8]],[\"m\",[\"xs\"],[4,3,2,1,0]]]");
 struct json_object *saved_value=0,*saved_ops=0,*batches=json_object_new_array(),*out=0;assert(value&&ops&&batches&&!json_object_deep_copy(value,&saved_value,0)&&!json_object_deep_copy(ops,&saved_ops,0)&&!json_object_array_add(batches,json_object_get(ops)));
 for(unsigned mode=0;mode<2;++mode){tracking=1;calls=0;fail_at=0;assert((mode?pg_document_delta_batches(value,batches,&out):pg_document_delta_apply(value,ops,&out))==PG_DOC_OK);tracking=0;unsigned count=calls;json_object_put(out);
 for(unsigned i=1;i<=count;++i){tracking=1;calls=0;fail_at=i;pg_document_status status=mode?pg_document_delta_batches(value,batches,&out):pg_document_delta_apply(value,ops,&out);tracking=0;assert(status!=PG_DOC_OK&&!out&&json_object_equal(value,saved_value)&&json_object_equal(ops,saved_ops));}
 }
 fail_at=0;json_object_put(batches);json_object_put(saved_ops);json_object_put(saved_value);json_object_put(ops);json_object_put(value);
}
#endif
static void run(const char *input,const char *operations,const char *answer){struct json_object *value=json_tokener_parse(input),*ops=json_tokener_parse(operations),*out=0;struct json_object *saved_value=0,*saved_ops=0;assert((!value||!json_object_deep_copy(value,&saved_value,0))&&!json_object_deep_copy(ops,&saved_ops,0));pg_document_status result=pg_document_delta_apply(value,ops,&out);if(answer){struct json_object *expected=json_tokener_parse(answer);assert(result==PG_DOC_OK&&json_object_equal(out,expected));json_object_put(expected);}else assert(result!=PG_DOC_OK&&!out);assert(json_object_equal(value,saved_value)&&json_object_equal(ops,saved_ops));json_object_put(value);json_object_put(ops);json_object_put(out);json_object_put(saved_value);json_object_put(saved_ops);}
static void revisions(void){
 _Static_assert(sizeof(pg_document_revision)==32,"revision ABI");
 _Static_assert(sizeof(pg_document_revision_view)==40,"view ABI");
 _Static_assert(sizeof(pg_document_snapshot)==24,"snapshot ABI");
 struct json_object *base=json_tokener_parse("{\"text\":\"a\"}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"b\"]]"),*newbase=json_tokener_parse("{\"text\":\"new\"}");
 pg_document_revision rev[]={{2,1,PG_REVISION_BASE,0,base},{4,1,PG_REVISION_DELTA,0,ops},{7,2,PG_REVISION_BASE,0,newbase},{9,2,PG_REVISION_DELTA,0,ops}};
 pg_document_revision_view view={2,12,0,0,rev,4};pg_document_snapshot out;
 for(uint64_t at=0;at<=13;++at){assert(!pg_document_revision_read(&view,at,&out));if(at<2||at>=12){assert(!out.value&&!out.version&&!out.deltas_since_base);continue;}const char *expected=at<4?"a":at<7?"ab":at<9?"new":"newb";assert(out.version==(at<7?1u:2u)&&out.deltas_since_base==((at>=4&&at<7)||at>=9));assert(!strcmp(json_object_get_string(json_object_object_get(out.value,"text")),expected));json_object_put(out.value);}
 assert(!pg_document_revision_read(&view,UINT64_MAX,&out)&&!out.version);view.current_only=1;assert(pg_document_revision_read(&view,0,&out)==PG_DOC_SEMANTICS&&!out.value);view.retired_at=0;assert(!pg_document_revision_read(&view,UINT64_MAX,&out)&&out.version==2);json_object_put(out.value);view.current_only=0;
 rev[3].version=3;assert(pg_document_revision_read(&view,9,&out)==PG_DOC_INVALID&&!out.value&&!out.version);assert(!pg_document_revision_read(&view,8,&out)&&out.version==2);json_object_put(out.value);rev[3].version=2;
 rev[1].sequence=2;assert(pg_document_revision_read(&view,UINT64_MAX,&out)==PG_DOC_INVALID&&!out.value);rev[1].sequence=4;
 view.count=1;rev[0].kind=PG_REVISION_DELTA;assert(pg_document_revision_read(&view,2,&out)==PG_DOC_INVALID&&!out.value);rev[0].kind=PG_REVISION_BASE;view.count=4;
#ifdef PG_DELTA_FAULTS
 tracking=1;calls=0;fail_at=0;assert(!pg_document_revision_read(&view,UINT64_MAX,&out));tracking=0;unsigned count=calls;json_object_put(out.value);
 for(unsigned i=1;i<=count;++i){tracking=1;calls=0;fail_at=i;pg_document_status status=pg_document_revision_read(&view,UINT64_MAX,&out);tracking=0;assert(status!=PG_DOC_OK&&!out.value&&!out.version&&!out.deltas_since_base);}
 fail_at=0;
#endif
 assert(!strcmp(json_object_get_string(json_object_object_get(base,"text")),"a"));json_object_put(base);json_object_put(ops);json_object_put(newbase);
}
static void changes(void){
 _Static_assert(sizeof(pg_document_change)==32&&sizeof(pg_document_prepared)==16,"change ABI");pg_document_change change={0};struct json_object *base=json_tokener_parse("{\"text\":\"a\"}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"b\"],[\"s\",[\"x\"],1]]"),*snapshot=0;assert(base&&ops&&!pg_document_change_open(&change,base));assert(pg_document_change_open(&change,base)==PG_DOC_INVALID);json_object_object_add(base,"text",json_object_new_string("outside"));assert(!pg_document_change_append(&change,ops));struct json_object *first=json_object_array_get_idx(ops,0);assert(!json_object_array_put_idx(first,2,json_object_new_string("changed")));assert(!pg_document_change_snapshot(&change,&snapshot)&&!strcmp(json_object_get_string(json_object_object_get(snapshot,"text")),"ab"));json_object_object_add(snapshot,"text",json_object_new_string("snapshot"));json_object_put(snapshot);pg_document_prepared prepared={0},extra={0};assert(!pg_document_change_prepare(&change,&prepared)&&!strcmp(json_object_get_string(json_object_object_get(prepared.value,"text")),"ab")&&json_object_array_length(prepared.ops)==2);assert(pg_document_change_snapshot(&change,&snapshot)==PG_DOC_INVALID&&!snapshot&&pg_document_change_append(&change,ops)==PG_DOC_INVALID&&pg_document_change_prepare(&change,&extra)==PG_DOC_INVALID&&!extra.value&&!extra.ops);assert(!pg_document_change_discard(&change)&&!pg_document_change_discard(&change)&&json_object_object_get(prepared.value,"x"));json_object_put(prepared.value);json_object_put(prepared.ops);
 assert(!pg_document_change_open(&change,base));struct json_object *bad=json_tokener_parse("[[\"s\",[\"missing\",\"x\"],1]]");assert(bad&&!pg_document_change_append(&change,bad)&&pg_document_change_prepare(&change,&prepared)==PG_DOC_INVALID&&!prepared.value&&!prepared.ops&&change.sealed&&!change.active&&!change.base&&!change.ops);json_object_put(bad);
#ifdef PG_DELTA_FAULTS
 for(unsigned operation=0;operation<3;++operation){if(operation){assert(!pg_document_change_open(&change,base));assert(!pg_document_change_append(&change,ops));}tracking=1;calls=fail_at=0;pg_document_status status=operation==0?pg_document_change_open(&change,base):operation==1?pg_document_change_append(&change,ops):pg_document_change_prepare(&change,&prepared);tracking=0;assert(!status);unsigned count=calls;if(operation==2){json_object_put(prepared.value);json_object_put(prepared.ops);}assert(!pg_document_change_discard(&change));
 for(unsigned i=1;i<=count;++i){if(operation){assert(!pg_document_change_open(&change,base));assert(!pg_document_change_append(&change,ops));}tracking=1;calls=0;fail_at=i;status=operation==0?pg_document_change_open(&change,base):operation==1?pg_document_change_append(&change,ops):pg_document_change_prepare(&change,&prepared);tracking=0;assert(status!=PG_DOC_OK&&!change.busy);if(operation==0)assert(!change.active&&!change.base&&!change.ops);if(operation==1)assert(change.active&&json_object_array_length(change.ops)==2&&json_object_equal(change.ops,ops));if(operation==2)assert(change.sealed&&!change.active&&!change.base&&!change.ops&&!prepared.value&&!prepared.ops);assert(!pg_document_change_discard(&change));}fail_at=0;
 }
#endif
 json_object_put(base);json_object_put(ops);
}
static void plans(void){
 pg_document_prepared prepared={json_tokener_parse("{\"text\":\"x\"}"),json_tokener_parse("[]")};struct json_object *write=0;assert(prepared.value&&prepared.ops&&!pg_document_plan_change(2,1,1,&prepared,0,&write)&&!write&&!pg_document_plan_change(2,1,1,&prepared,1,&write)&&!write);assert(!pg_document_plan_change(2,1,2,&prepared,0,&write)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(write,"content"),"kind")),"base"));json_object_object_add(prepared.value,"text",json_object_new_string("outside"));assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(json_object_object_get(write,"content"),"value"),"text")),"x"));json_object_put(write);assert(pg_document_plan_change(2,2,1,&prepared,0,&write)==PG_DOC_NEWER_VERSION&&!write);assert(pg_document_plan_change(0,1,1,&prepared,0,&write)==PG_DOC_INVALID&&!write);json_object_put(prepared.ops);prepared.ops=json_tokener_parse("[[\"s\",[\"text\"],\"outside\"]]");assert(prepared.ops);
 for(unsigned checkpoint=0;checkpoint<2;++checkpoint){assert(!pg_document_plan_change(2,1,1,&prepared,checkpoint,&write));struct json_object *content=json_object_object_get(write,"content");assert(!strcmp(json_object_get_string(json_object_object_get(content,"kind")),checkpoint?"base":"delta")&&json_object_object_get(content,checkpoint?"value":"ops"));json_object_put(write);
#ifdef PG_DELTA_FAULTS
 tracking=1;calls=fail_at=0;assert(!pg_document_plan_change(2,1,1,&prepared,checkpoint,&write));tracking=0;unsigned count=calls;json_object_put(write);for(unsigned i=1;i<=count;++i){tracking=1;calls=0;fail_at=i;pg_document_status status=pg_document_plan_change(2,1,1,&prepared,checkpoint,&write);tracking=0;assert(status==PG_DOC_ALLOCATION&&!write);}fail_at=0;
#endif
 }
 json_object_put(prepared.value);json_object_put(prepared.ops);
}
static void adoptions(void){
 _Static_assert(sizeof(pg_document_adoption)==80&&offsetof(pg_document_adoption,busy)==72,"adoption ABI");struct json_object *base=json_tokener_parse("{\"x\":1}"),*next=json_tokener_parse("{\"x\":2}"),*ops=json_tokener_parse("[[\"s\",[\"x\"],2]]"),*empty=json_object_new_array();assert(base&&next&&ops&&empty);pg_document_loaded target={.value=json_object_get(base),.record={.id=2},.stored_version=1,.value_version=1,.deltas_since_base=3,.active=1};pg_document_prepared prepared={next,ops};pg_document_adoption a={0},b={0};assert(!pg_document_adoption_prepare(&a,&target,&prepared,PG_ADOPTION_DELTA)&&target.value==base);assert(pg_document_adoption_prepare(&a,&target,&prepared,PG_ADOPTION_DELTA)==PG_DOC_INVALID);assert(!pg_document_adoption_prepare(&b,&target,&prepared,PG_ADOPTION_DELTA));json_object_object_add(next,"x",json_object_new_int(9));
#ifdef PG_DELTA_FAULTS
 tracking=1;calls=0;fail_at=1;
#endif
 assert(!pg_document_adoption_apply(&a));
#ifdef PG_DELTA_FAULTS
 tracking=0;assert(calls==0);fail_at=0;
#endif
 assert(target.stored_version==1&&target.deltas_since_base==4&&json_object_get_int(json_object_object_get(target.value,"x"))==2);assert(pg_document_adoption_apply(&a)==PG_DOC_INVALID&&pg_document_adoption_apply(&b)==PG_DOC_SEMANTICS&&json_object_get_int(json_object_object_get(target.value,"x"))==2);assert(!pg_document_adoption_discard(&a)&&!pg_document_adoption_discard(&b)&&!pg_document_adoption_discard(&b));
 target.value_version=2;prepared.ops=empty;struct json_object *old=target.value;assert(!pg_document_adoption_prepare(&a,&target,&prepared,PG_ADOPTION_BASE)&&!pg_document_adoption_apply(&a)&&target.value==old&&target.stored_version==2&&!target.deltas_since_base&&!pg_document_adoption_discard(&a));assert(!pg_document_adoption_prepare(&a,&target,&prepared,PG_ADOPTION_NO_WRITE)&&!pg_document_adoption_apply(&a)&&target.value==old&&!pg_document_adoption_discard(&a));assert(pg_document_adoption_prepare(&a,&target,&prepared,PG_ADOPTION_DELTA)==PG_DOC_INVALID&&!a.expected&&!a.candidate&&!target.busy);prepared.ops=ops;
#ifdef PG_DELTA_FAULTS
 tracking=1;calls=fail_at=0;assert(!pg_document_adoption_prepare(&a,&target,&prepared,PG_ADOPTION_BASE));tracking=0;unsigned count=calls;assert(!pg_document_adoption_discard(&a));for(unsigned i=1;i<=count;++i){tracking=1;calls=0;fail_at=i;int status=pg_document_adoption_prepare(&a,&target,&prepared,PG_ADOPTION_BASE);tracking=0;assert(status==PG_DOC_ALLOCATION&&!a.expected&&!a.candidate&&!a.busy&&target.value==old&&!target.busy&&target.stored_version==2&&!target.deltas_since_base);assert(!pg_document_adoption_discard(&a));}fail_at=0;
#endif
 json_object_put(target.value);json_object_put(base);json_object_put(next);json_object_put(ops);json_object_put(empty);
}
static void pending_ownership(void){
 _Static_assert(sizeof(pg_pending_ops)==32&&offsetof(pg_pending_ops,state)==16&&offsetof(pg_pending_ops,active)==24&&sizeof(pg_pending_take)==16,"PendingOps ABI");
 pg_pending_ops q={0};pg_pending_take take={0};struct json_object *a=json_tokener_parse("[[\"a\",[\"x\"],\"A\"],[\"s\",[\"y\"],1]]"),*b=json_tokener_parse("[[\"a\",[\"x\"],\"中\\u0000😀\"],[\"s\",[\"y\"],2]]");assert(a&&b&&!pg_pending_ops_open(&q));
 assert(!pg_pending_ops_push(&q,a)&&pg_pending_ops_size(&q)==2);
 json_object_array_put_idx(json_object_array_get_idx(a,0),2,json_object_new_string("caller"));
 assert(!pg_pending_ops_push(&q,b)&&pg_pending_ops_size(&q)==2);
#ifdef PG_DELTA_FAULTS
 tracking=1;calls=0;fail_at=1;
#endif
 assert(!pg_pending_ops_take(&q,&take)&&!take.replace&&take.ops&&!pg_pending_ops_size(&q));
#ifdef PG_DELTA_FAULTS
 tracking=0;assert(!calls);fail_at=0;
#endif
 struct json_object *expected=json_tokener_parse("[[\"a\",[\"x\"],\"A中\\u0000😀\"],[\"s\",[\"y\"],2]]");assert(expected&&json_object_equal(take.ops,expected));
 assert(!pg_pending_ops_push(&q,b));json_object_put(a);json_object_put(b);assert(json_object_equal(take.ops,expected));json_object_put(take.ops);json_object_put(expected);
 q.busy=1;take=(pg_pending_take){0};assert(pg_pending_ops_take(&q,&take)==PG_DOC_INVALID&&!take.ops&&pg_pending_ops_close(&q)==PG_DOC_INVALID&&!pg_pending_ops_size(&q));q.busy=0;
 assert(!pg_pending_ops_close(&q)&&!pg_pending_ops_close(&q)&&!pg_pending_ops_open(&q));
#ifdef PG_DELTA_FAULTS
 tracking=1;calls=0;fail_at=1;assert(pg_pending_ops_take(&q,&take)==PG_DOC_ALLOCATION&&!take.ops&&!q.busy&&q.active&&!q.state);tracking=0;fail_at=0;
#endif
 assert(!pg_pending_ops_take(&q,&take)&&take.ops&&!json_object_array_length(take.ops));json_object_put(take.ops);assert(!pg_pending_ops_close(&q));
 long page=sysconf(_SC_PAGESIZE);char *memory=mmap(0,(size_t)page*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED&&!mprotect(memory+page,(size_t)page,PROT_NONE)&&!mprotect(memory+3*page,(size_t)page,PROT_NONE));pg_pending_ops *tail=(pg_pending_ops*)(memory+page-sizeof(*tail));pg_pending_take *result=(pg_pending_take*)(memory+3*page-sizeof(*result));memset(tail,0,sizeof(*tail));memset(result,0,sizeof(*result));a=json_tokener_parse("[[\"a\",[\"tail\"],\"x\"]]");assert(a&&!pg_pending_ops_open(tail)&&!pg_pending_ops_push(tail,a)&&!pg_pending_ops_take(tail,result)&&result->ops&&!pg_pending_ops_close(tail));json_object_put(a);json_object_put(result->ops);assert(!munmap(memory,(size_t)page*4));
}
static void pending_overflow(void){
 pg_pending_ops q={0};pg_pending_take take={0};struct json_object *large=json_object_new_array_ext(10001),*small=json_tokener_parse("[[\"a\",[\"x\"],\"a\"]]"),*root=json_tokener_parse("[[\"r\",{\"x\":\"r\"}],[\"a\",[\"x\"],\"t\"]]");assert(large&&small&&root&&!pg_pending_ops_open(&q));
 for(unsigned i=0;i<10001;++i){char text[96];int n=snprintf(text,sizeof(text),"[\"s\",[\"k%u\"],%u]",i,i);assert(n>0&&(size_t)n<sizeof(text));struct json_object *op=json_tokener_parse(text);assert(op&&!json_object_array_add(large,op));}
 assert(!pg_pending_ops_push(&q,large)&&pg_pending_ops_size(&q)==10001&&!pg_pending_ops_push(&q,small)&&pg_pending_ops_size(&q)==1);
#ifdef PG_DELTA_FAULTS
 tracking=1;calls=0;fail_at=1;
#endif
 assert(!pg_pending_ops_push(&q,small));
#ifdef PG_DELTA_FAULTS
 tracking=0;assert(!calls);fail_at=0;
#endif
 assert(!pg_pending_ops_take(&q,&take)&&take.replace&&!take.ops&&!pg_pending_ops_size(&q)&&!pg_pending_ops_push(&q,large)&&!pg_pending_ops_push(&q,root)&&pg_pending_ops_size(&q)==2&&!pg_pending_ops_take(&q,&take)&&!take.replace&&json_object_equal(take.ops,root));
 json_object_put(take.ops);json_object_put(large);json_object_put(small);json_object_put(root);assert(!pg_pending_ops_close(&q));
}
#ifdef PG_DELTA_FAULTS
static void pending_key_cost(void){
 pg_pending_ops q={0};struct json_object *a=json_tokener_parse("[[\"a\",[\"x\"],\"a\"],[\"s\",[\"y\"],1]]"),*b=json_tokener_parse("[[\"a\",[\"x\"],\"b\"]]"),*structural=json_tokener_parse("[[\"d\",[\"gone\"]],[\"p\",[\"xs\"],0,0,[1]]]");assert(a&&b&&structural&&!pg_pending_ops_open(&q));
 path_keys=0;assert(!pg_pending_ops_push(&q,a)&&!path_keys&&!pg_pending_ops_push(&q,b)&&path_keys==3&&!pg_pending_ops_push(&q,b)&&path_keys==4&&!pg_pending_ops_push(&q,structural)&&path_keys==4);
 json_object_put(a);json_object_put(b);json_object_put(structural);assert(!pg_pending_ops_close(&q));
}
static void pending_failures(void){
 struct json_object *a=json_tokener_parse("[[\"a\",[\"x\"],\"a\"],[\"s\",[\"n\"],1]]"),*b=json_tokener_parse("[[\"a\",[\"x\"],\"b\"],[\"s\",[\"n\"],2]]"),*c=json_tokener_parse("[[\"a\",[\"x\"],\"c\"],[\"d\",[\"gone\"]],[\"s\",[\"xs\",1.0],\"one\"],[\"a\",[\"xs\",1],\"!\"]]");assert(a&&b&&c);
 unsigned totals[3]={0};for(unsigned mode=0;mode<3;++mode)for(unsigned attempt=0;attempt<=totals[mode];++attempt){
  pg_pending_ops q={0};assert(!pg_pending_ops_open(&q));if(mode)assert(!pg_pending_ops_push(&q,a));if(mode==2)assert(!pg_pending_ops_push(&q,b));
  struct json_object *ops=0,*map=0;assert(!q.ops||!json_object_deep_copy(q.ops,&ops,0));assert(!q.leaves||!json_object_deep_copy(q.leaves,&map,0));unsigned state=q.state;void *identity=q.ops;
  tracking=1;calls=0;fail_at=attempt;int status=pg_pending_ops_push(&q,mode==0?a:mode==1?b:c);tracking=0;
  if(!attempt){assert(!status);totals[mode]=calls;}else assert(status!=PG_DOC_OK&&!q.busy&&q.active&&q.state==state&&q.ops==identity&&json_object_equal(q.ops,ops)&&json_object_equal(q.leaves,map));
  fail_at=0;json_object_put(ops);json_object_put(map);assert(!pg_pending_ops_close(&q));
 }
 json_object_put(a);json_object_put(b);json_object_put(c);
}
#endif
static void held_store_ownership(void){
 _Static_assert(sizeof(pg_document_held_content)==16,"held content ABI");pg_pending_ops q={0};pg_document_held_content held={0};
 struct json_object *a=json_tokener_parse("[[\"a\",[\"text\"],\"a\"]]"),*b=json_tokener_parse("[[\"a\",[\"text\"],\"b\"]]"),*value=json_tokener_parse("{\"text\":\"final\"}");assert(a&&b&&value&&!pg_pending_ops_open(&q)&&!pg_pending_ops_push(&q,a));
 assert(pg_document_store_held(&q,b,value,0,0,&held)==PG_DOC_INVALID&&!held.content&&!held.ops&&pg_pending_ops_size(&q)==1);
 assert(pg_document_store_held(&q,b,value,1,2,&held)==PG_DOC_INVALID&&!held.content&&!held.ops&&pg_pending_ops_size(&q)==1);
 assert(!pg_document_store_held(&q,b,value,2,1,&held)&&!pg_pending_ops_size(&q)&&!strcmp(json_object_get_string(json_object_object_get(held.content,"kind")),"base")&&json_object_get_int(json_object_object_get(held.content,"version"))==2);
 struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"ab\"]]");assert(ops&&json_object_equal(held.ops,ops));json_object_object_add(value,"text",json_object_new_string("caller"));json_object_array_put_idx(json_object_array_get_idx(b,0),2,json_object_new_string("caller"));assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(held.content,"value"),"text")),"final")&&json_object_equal(held.ops,ops));
 json_object_put(held.content);json_object_put(held.ops);json_object_put(ops);json_object_put(a);json_object_put(b);json_object_put(value);assert(!pg_pending_ops_close(&q));
}
#ifdef PG_DELTA_FAULTS
static void held_store_failures(void){
 struct json_object *a=json_tokener_parse("[[\"a\",[\"text\"],\"a\"]]"),*b=json_tokener_parse("[[\"a\",[\"text\"],\"b\"]]"),*value=json_tokener_parse("{\"text\":\"final\"}");assert(a&&b&&value);
 unsigned totals[3]={0};for(unsigned mode=0;mode<3;++mode)for(unsigned attempt=0;attempt<=totals[mode];++attempt){
  pg_pending_ops q={0};assert(!pg_pending_ops_open(&q));if(mode<2)assert(!pg_pending_ops_push(&q,a)&&!pg_pending_ops_push(&q,b));else q.state=3;
  struct json_object *ops=0,*map=0;assert(!q.ops||!json_object_deep_copy(q.ops,&ops,0));assert(!q.leaves||!json_object_deep_copy(q.leaves,&map,0));void *identity=q.ops;unsigned state=q.state;pg_document_held_content held={0};
  tracking=1;calls=0;fail_at=attempt;int status=pg_document_store_held(&q,b,value,2,mode==1,&held);tracking=0;
  if(!attempt){assert(!status&&!pg_pending_ops_size(&q));totals[mode]=calls;if(mode==2){struct json_object *op=json_object_array_get_idx(held.ops,0);assert(!strcmp(json_object_get_string(json_object_array_get_idx(op,0)),"r")&&json_object_equal(json_object_array_get_idx(op,1),value)&&!strcmp(json_object_get_string(json_object_object_get(held.content,"kind")),"base"));}}
  else assert(status!=PG_DOC_OK&&!held.content&&!held.ops&&!q.busy&&q.active&&q.state==state&&q.ops==identity&&json_object_equal(q.ops,ops)&&json_object_equal(q.leaves,map));
  fail_at=0;json_object_put(held.content);json_object_put(held.ops);json_object_put(ops);json_object_put(map);assert(!pg_pending_ops_close(&q));
 }
 json_object_put(a);json_object_put(b);json_object_put(value);
}
#endif
int main(void){
 held_store_ownership();
#ifdef PG_DELTA_FAULTS
 held_store_failures();
#endif
 pending_ownership();
 pending_overflow();
#ifdef PG_DELTA_FAULTS
 pending_key_cost();
 pending_failures();
#endif
 adoptions();
 plans();
 changes();
 revisions();
 run("{\"text\":\"\"}","[[\"a\",[\"text\"],\"中\"],[\"a\",[\"text\"],\"😀\"],[\"t\",[\"text\"],1]]","{\"text\":\"😀\"}");
 run("{}","[[\"s\",[\"text\"],\"abc\"],[\"a\",[\"text\"],\"def\"],[\"t\",[\"text\"],2]]","{\"text\":\"cdef\"}");
 run("{\"xs\":[0,1,2,3]}","[[\"p\",[\"xs\"],1,2,[8,9]],[\"m\",[\"xs\"],[3,2,1,0]]]","{\"xs\":[3,9,8,0]}");
 run("[1,2]","[[\"s\",[2],3],[\"d\",[0]]]","[2,3]");
 run("null","[[\"r\",{\"x\":[1]}],[\"s\",[\"x\",0],2]]","{\"x\":[2]}");
 run("{}","[[\"s\",[\"__proto__\",\"x\"],1]]",0);
 run("[1]","[[\"s\",[3],9]]",0);
 run("{\"text\":\"中😀tail\"}","[[\"t\",[\"text\"],3]]","{\"text\":\"tail\"}");
 run("{\"text\":\"中😀tail\"}","[[\"t\",[\"text\"],2]]",0);
 run("[1,2]","[[\"m\",[],[0,0]]]",0);
 run("{}","[[\"s\",[\"ok\"],1],[\"a\",[\"missing\"],\"x\"]]",0);
 struct json_object *base=json_tokener_parse("{\"x\":[]}"),*batches=json_tokener_parse("[[[\"p\",[\"x\"],0,0,[1]]],[],[[\"p\",[\"x\"],1,0,[2]]]]"),*out=0,*expected=json_tokener_parse("{\"x\":[1,2]}");assert(!pg_document_delta_batches(base,batches,&out)&&json_object_equal(out,expected));json_object_put(base);json_object_put(batches);json_object_put(out);json_object_put(expected);
#ifdef PG_DELTA_FAULTS
 failures();
#endif
 puts("PASS: decoded document delta verbs, safe paths, dense arrays, UTF-16 boundaries and atomic immutable inputs");return 0;}
