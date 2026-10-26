#include "../../src/runtime/delta.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json-c/json.h>
#ifdef PG_DELTA_FAULTS
static unsigned tracking,calls,fail_at;
static int fault(void){return tracking&&++calls==fail_at;}
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
int main(void){
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
