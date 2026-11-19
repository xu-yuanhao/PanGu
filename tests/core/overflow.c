#include <pangu/core/overflow.h>
#include "yyjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"overflow:%d: %s\n",__LINE__,#x);abort();}}while(0)
static pangu_slice string(yyjson_val *object,const char *key) {
    yyjson_val *v=yyjson_obj_get(object,key);return (pangu_slice){yyjson_get_str(v),yyjson_get_len(v)};
}
static uint64_t number(yyjson_val *object,const char *key) {return yyjson_get_uint(yyjson_obj_get(object,key));}
typedef struct {size_t calls,fail,live;} memory;
static void *allocate(void *ctx,size_t n){memory *m=ctx;if(++m->calls==m->fail)return NULL;void *p=malloc(n);if(p)++m->live;return p;}
static void release(void *ctx,void *p){if(p){--((memory *)ctx)->live;free(p);}}
static void failures(void) {
    int completed=0;
    for(size_t fail=1;fail<1000 && !completed;++fail){
        memory m={.fail=fail};pangu_allocator a={&m,allocate,release};pangu_overflow_classifier *c=NULL;
        pangu_status s=pangu_overflow_create(&a,&c);
        CHECK(s==PANGU_OK || s==PANGU_NOMEM);
        if(s==PANGU_OK){
            pangu_message message={.role=PANGU_ASSISTANT,.stop_reason=PANGU_ERROR,.error={"too many tokens",15}};
            int out=42;s=pangu_message_is_context_overflow(c,&message,0,&out);
            CHECK(s==PANGU_OK || s==PANGU_NOMEM);
            if(s==PANGU_NOMEM)CHECK(out==42);else {CHECK(out==1);completed=1;}
        }else CHECK(!c);
        pangu_overflow_destroy(c);CHECK(!m.live);
    }
    CHECK(completed);
}
int main(int argc,char **argv) {
    CHECK(argc==2 || argc==3);
    yyjson_read_err error;yyjson_doc *doc=yyjson_read_file(argv[1],0,NULL,&error);CHECK(doc);
    pangu_overflow_classifier *c=NULL;CHECK(pangu_overflow_create(NULL,&c)==PANGU_OK);
    yyjson_val *entry;size_t i,max;
    static const char *reasons[]={"pending","stop","length","toolUse","error","aborted","deferred"};
    yyjson_arr_foreach(yyjson_doc_get_root(doc),i,max,entry){
        pangu_slice reason=string(entry,"reason");pangu_stop_reason stop=PANGU_ERROR;
        if(reason.len)for(size_t k=0;k<7;++k)if(reason.len==strlen(reasons[k])&&!memcmp(reason.data,reasons[k],reason.len))stop=(pangu_stop_reason)k;
        pangu_message m={.role=PANGU_ASSISTANT,.stop_reason=stop,.provider=string(entry,"provider"),.error=string(entry,"error"),
            .usage={.input=number(entry,"input"),.cache_read=number(entry,"cache"),.output=number(entry,"output")}};
        int out=42;CHECK(pangu_message_is_context_overflow(c,&m,number(entry,"capacity"),&out)==PANGU_OK);
        if(out!=yyjson_get_bool(yyjson_obj_get(entry,"expected"))){fprintf(stderr,"vector %zu mismatch\n",i);abort();}
        if(argc==3)printf("%zu:%d:%d\n",i,out,pangu_message_is_recoverable_length(&m,number(entry,"desired")));
    }
    pangu_message invalid={.role=PANGU_ASSISTANT,.stop_reason=PANGU_ERROR,.error={NULL,1}};
    int out=42;CHECK(pangu_message_is_context_overflow(c,&invalid,0,&out)==PANGU_INVALID && out==42);
    const char bad[]={(char)0xff};invalid.error=(pangu_slice){bad,1};
    CHECK(pangu_message_is_context_overflow(c,&invalid,0,&out)==PANGU_INVALID && out==42);
    invalid.error=(pangu_slice){0};invalid.role=PANGU_USER;
    CHECK(pangu_message_is_context_overflow(c,&invalid,0,&out)==PANGU_INVALID && out==42);
    CHECK(!pangu_message_is_recoverable_length(&invalid,1));
    pangu_overflow_destroy(c);yyjson_doc_free(doc);failures();return 0;
}
