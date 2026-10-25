#define _GNU_SOURCE
#include "../../src/runtime/documents.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <json-c/json.h>
#include <math.h>
_Static_assert(sizeof(pg_document_address)==48,"address size");
_Static_assert(offsetof(pg_document_address,owner)==32,"owner");
_Static_assert(offsetof(pg_document_address,scope)==40,"scope");
#ifdef PG_DOC_FAULTS
#include <json-c/json.h>
static unsigned calls,fail_at;
static int fault(void){return ++calls==fail_at;}
struct json_object *__real_json_object_new_array_ext(int);
struct json_object *__real_json_object_new_string_len(const char *,int);
struct json_object *__real_json_object_new_string(const char *);
struct json_object *__real_json_object_new_int64(int64_t);
int __real_json_object_array_add(struct json_object *,struct json_object *);
const char *__real_json_object_to_json_string_length(struct json_object *,int,size_t *);
struct json_object *__wrap_json_object_new_array_ext(int n){return fault()?0:__real_json_object_new_array_ext(n);}
struct json_object *__wrap_json_object_new_string_len(const char *s,int n){return fault()?0:__real_json_object_new_string_len(s,n);}
struct json_object *__wrap_json_object_new_string(const char *s){return fault()?0:__real_json_object_new_string(s);}
struct json_object *__wrap_json_object_new_int64(int64_t n){return fault()?0:__real_json_object_new_int64(n);}
int __wrap_json_object_array_add(struct json_object *o,struct json_object *v){return fault()?-1:__real_json_object_array_add(o,v);}
const char *__wrap_json_object_to_json_string_length(struct json_object *o,int flags,size_t *n){return fault()?0:__real_json_object_to_json_string_length(o,flags,n);}
int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
int __wrap_json_object_deep_copy(struct json_object *src,struct json_object **dst,json_c_shallow_copy_fn *fn){return fault()?-1:__real_json_object_deep_copy(src,dst,fn);}
static void allocation_failures(void){
 for(unsigned scope=0;scope<3;++scope){pg_document_address a={.kind={"kind",4},.scope=scope,.owner=scope?17:0,.has_key=scope!=0,.key={scope?"key":0,scope?3:0}};char out[256];size_t size;
 calls=0;fail_at=0;assert(!pg_document_address_encode(&a,out,sizeof(out),&size));unsigned count=calls;
 for(unsigned i=1;i<=count;++i){calls=0;fail_at=i;memset(out,0xa7,sizeof(out));assert(pg_document_address_encode(&a,out,sizeof(out),&size)==PG_DOC_ALLOCATION&&!size);for(size_t j=0;j<sizeof(out);++j)assert((unsigned char)out[j]==0xa7);}
 fail_at=0;
 }
}
#endif
_Static_assert(sizeof(pg_document_definition)==56,"definition");
_Static_assert(offsetof(pg_document_definition,migrate)==48,"migration");
_Static_assert(sizeof(pg_document_record)==64,"record");
static struct json_object *retained;
static int migrate(void *ctx,struct json_object *value,uint64_t from,struct json_object **out){
 assert(from==1);json_object_object_add(value,"count",json_object_new_int(2));
 *out=json_object_get(value);retained=json_object_get(value);return ctx?1:0;
}
static void materialization(void){
 pg_document_definition d={.kind={"counter",7},.version=2,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_LATEST,.fork=PG_DOC_CURRENT};
 pg_document_record r={.address={.kind={"counter",7},.scope=PG_DOC_CONVERSATION,.owner=1},.id=1,.history=PG_DOC_LATEST,.fork=PG_DOC_CURRENT};
 struct json_object *input=json_tokener_parse("{\"count\":1}"),*out=0;assert(input);
 assert(pg_document_check(&d,&r,1)==PG_DOC_MIGRATION_REQUIRED);assert(pg_document_check(&d,&r,3)==PG_DOC_NEWER_VERSION);
 r.history=PG_DOC_REWINDABLE;assert(pg_document_check(&d,&r,2)==PG_DOC_SEMANTICS);r.history=PG_DOC_LATEST;
 r.fork=PG_DOC_INITIAL;assert(pg_document_check(&d,&r,2)==PG_DOC_SEMANTICS);r.fork=PG_DOC_CURRENT;
 r.address.scope=PG_DOC_TASK;assert(pg_document_check(&d,&r,2)==PG_DOC_SEMANTICS);r.address.scope=PG_DOC_CONVERSATION;
 assert(!pg_document_materialize(&d,&r,2,input,&out)&&out&&out!=input);json_object_object_add(out,"count",json_object_new_int(9));assert(json_object_get_int(json_object_object_get(input,"count"))==1);json_object_put(out);
 d.migrate=migrate;assert(!pg_document_materialize(&d,&r,1,input,&out));assert(json_object_get_int(json_object_object_get(out,"count"))==2);assert(json_object_get_int(json_object_object_get(input,"count"))==1);json_object_object_add(retained,"count",json_object_new_int(8));assert(json_object_get_int(json_object_object_get(out,"count"))==2);json_object_put(retained);json_object_put(out);
 #ifdef PG_DOC_FAULTS
 for(unsigned i=1;i<=2;++i){calls=0;fail_at=i;retained=0;assert(pg_document_materialize(&d,&r,1,input,&out)==PG_DOC_ALLOCATION&&!out);json_object_put(retained);}
 fail_at=0;
#endif
 d.context=&d;assert(pg_document_materialize(&d,&r,1,input,&out)==PG_DOC_MIGRATION_FAILED&&!out);json_object_put(retained);
 assert(pg_document_materialize(&d,&r,2,0,&out)==PG_DOC_INVALID&&!out);d.history=0;assert(pg_document_check(&d,&r,2)==PG_DOC_INVALID);d.history=PG_DOC_LATEST;d.fork=PG_DOC_AS_OF;assert(pg_document_check(&d,&r,2)==PG_DOC_INVALID);d.fork=PG_DOC_CURRENT;d.version=0;assert(pg_document_check(&d,&r,1)==PG_DOC_INVALID);d.version=9007199254740992ULL;assert(pg_document_check(&d,&r,1)==PG_DOC_INVALID);assert(pg_document_check(0,&r,1)==PG_DOC_INVALID);json_object_put(input);
}
_Static_assert(sizeof(struct json_object_iterator)==sizeof(void *),"json-c iterator ABI");
static int nonfinite_migration(void *ctx,struct json_object *value,uint64_t from,struct json_object **out){
 (void)ctx;(void)value;(void)from;*out=json_object_new_object();assert(*out);json_object_object_add(*out,"invalid",json_object_new_double(NAN));return 0;
}
static void value_admission(void){
 assert(pg_document_value_valid(0));
 struct json_object *root=json_object_new_object(),*child=json_object_new_object();assert(root&&child);
 assert(!json_object_object_add(root,"child",json_object_get(child)));assert(!json_object_object_add(child,"parent",json_object_get(root)));
 assert(!pg_document_value_valid(root));
 pg_document_definition definition={.version=1,.scope=PG_DOC_SESSION};pg_document_record record={.address={.scope=PG_DOC_SESSION}};struct json_object *out=0;
 assert(pg_document_materialize(&definition,&record,1,root,&out)==PG_DOC_INVALID&&!out);
 json_object_object_del(child,"parent");assert(pg_document_value_valid(root));
 assert(!json_object_object_add(root,"shared",json_object_get(child)));assert(pg_document_value_valid(root));
 for(unsigned i=0;i<3;++i){double number=i==0?NAN:i==1?INFINITY:-INFINITY;assert(!json_object_object_add(child,"number",json_object_new_double(number)));assert(!pg_document_value_valid(root));}
 json_object_object_del(child,"number");assert(pg_document_value_valid(root));
 definition.version=2;definition.migrate=nonfinite_migration;assert(pg_document_materialize(&definition,&record,1,root,&out)==PG_DOC_MIGRATION_FAILED&&!out);
 json_object_put(root);json_object_put(child);
 root=json_object_new_array();struct json_object *tail=root;
 for(unsigned i=1;i<PG_DOCUMENT_DEPTH_LIMIT;++i){child=json_object_new_array();assert(child&&!json_object_array_add(tail,child));tail=child;}
 assert(pg_document_value_valid(root));child=json_object_new_array();assert(child&&!json_object_array_add(tail,child));assert(!pg_document_value_valid(root));json_object_put(root);
 root=json_tokener_parse("[null,true,false,17,1.25,\"value\",{},[]]");assert(root&&pg_document_value_valid(root));json_object_put(root);
}
static void encode(pg_document_address a,const char *expected){char out[512],before[512];size_t size=999;memset(out,0xa7,sizeof(out));memcpy(before,out,sizeof(out));size_t n=strlen(expected);assert(pg_document_address_encode(&a,out,n,&size)==PG_DOC_CAPACITY&&!size&&!memcmp(out,before,sizeof(out)));assert(!pg_document_address_encode(&a,out,n+1,&size)&&size==n&&!strcmp(out,expected));}
int main(void){
#ifdef PG_DOC_FAULTS
 allocation_failures();
#endif
 materialization();
 value_admission();
 pg_document_address a={.kind={"counter",7}};assert(pg_document_address_valid(&a)&&pg_document_address_equal(&a,&a));encode(a,"[\"counter\",\"session\",null,null]");
 pg_document_address b=a;b.has_key=1;assert(!pg_document_address_equal(&a,&b));encode(b,"[\"counter\",\"session\",null,\"\"]");
 b.scope=PG_DOC_CONVERSATION;b.owner=42;b.key=(pg_slice){"user/one",8};encode(b,"[\"counter\",\"conversation\",42,\"user/one\"]");
 b.scope=PG_DOC_TASK;b.owner=-9007199254740991LL;b.key=(pg_slice){"a\0b\n",4};encode(b,"[\"counter\",\"task\",-9007199254740991,\"a\\u0000b\\n\"]");b.owner=9007199254740991LL;assert(pg_document_address_valid(&b));b.owner++;assert(!pg_document_address_valid(&b));b.owner=INT64_MIN;assert(!pg_document_address_valid(&b));
 b=a;b.scope=3;assert(!pg_document_address_valid(&b));b=a;b.owner=1;assert(!pg_document_address_valid(&b));b=a;b.kind.size=0;assert(pg_document_address_valid(&b));encode(b,"[\"\",\"session\",null,null]");b=a;b.kind.data=0;assert(!pg_document_address_valid(&b));b=a;b.kind.size=(size_t)INT_MAX+1;assert(!pg_document_address_valid(&b));b=a;b.has_key=2;assert(!pg_document_address_valid(&b));b=a;b.key.size=1;assert(!pg_document_address_valid(&b));b.has_key=1;assert(!pg_document_address_valid(&b));assert(!pg_document_address_valid(0)&&!pg_document_address_equal(&a,0));
 char out[512];memset(out,'x',sizeof(out));size_t size;assert(pg_document_address_encode(&b,out,sizeof(out),&size)==PG_DOC_INVALID&&!size&&out[0]=='x');assert(pg_document_address_encode(&a,out,sizeof(out),0)==PG_DOC_INVALID);
 char alias[512]="counter";a.kind=(pg_slice){alias,7};assert(!pg_document_address_encode(&a,alias,sizeof(alias),&size)&&!strcmp(alias,"[\"counter\",\"session\",null,null]"));
 long page=sysconf(_SC_PAGESIZE);char *p=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED&&!mprotect(p+page,(size_t)page,PROT_NONE));memcpy(p+page-7,"counter",7);a.kind=(pg_slice){p+page-7,7};encode(a,"[\"counter\",\"session\",null,null]");assert(!munmap(p,(size_t)page*2));
 puts("PASS: Durable document address identity, scope/owner/family, JSON bytes, capacity/alias, scope/version/migration and guard boundaries");return 0;}
