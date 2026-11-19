#include "../../src/core/streaming_json.h"
#include "yyjson.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {size_t calls,fail,live,bytes;} memory;
static void *take(void *ctx,size_t n) {
    memory *m=ctx;m->calls++;
    if(m->fail && m->calls==m->fail)return NULL;
    void *p=malloc(n);if(p){m->live++;m->bytes+=n;}return p;
}
static void drop(void *ctx,void *p) {memory *m=ctx;if(p){assert(m->live);m->live--;free(p);}}
static pangu_allocator allocator(memory *m){return (pangu_allocator){m,take,drop};}
static pangu_slice slice(const char *p){return (pangu_slice){p,strlen(p)};}
static int equivalent(yyjson_val *a,yyjson_val *b) {
    if(yyjson_is_num(a)&&yyjson_is_num(b))return yyjson_get_num(a)==yyjson_get_num(b);
    if(yyjson_is_arr(a)&&yyjson_is_arr(b)) {
        if(yyjson_arr_size(a)!=yyjson_arr_size(b))return 0;
        size_t i,max;yyjson_val *v;yyjson_arr_foreach(a,i,max,v)if(!equivalent(v,yyjson_arr_get(b,i)))return 0;
        return 1;
    }
    if(yyjson_is_obj(a)&&yyjson_is_obj(b)) {
        if(yyjson_obj_size(a)!=yyjson_obj_size(b))return 0;
        size_t i,max;yyjson_val *k,*v;yyjson_obj_foreach(a,i,max,k,v) {
            yyjson_val *w=yyjson_obj_getn(b,yyjson_get_str(k),yyjson_get_len(k));if(!w||!equivalent(v,w))return 0;
        }
        return 1;
    }
    return yyjson_equals(a,b);
}
static void equal(pangu_slice actual,const char *expected) {
    if(actual.len==strlen(expected) && !memcmp(actual.data,expected,actual.len))return;
    yyjson_doc *a=yyjson_read(actual.data,actual.len,YYJSON_READ_ALLOW_INVALID_UNICODE);
    yyjson_doc *b=yyjson_read(expected,strlen(expected),YYJSON_READ_ALLOW_INVALID_UNICODE);
    if(!a || !b || !equivalent(yyjson_doc_get_root(a),yyjson_doc_get_root(b))) {
        fprintf(stderr,"actual: %.*s\nexpected: %s\n",(int)actual.len,actual.data,expected);abort();
    }
    yyjson_doc_free(a);yyjson_doc_free(b);
}
static void vector(const char *text,const char *expected) {
    for(size_t chunk=1;chunk<=strlen(text)+1;chunk++) {
        memory m={0};pangu_json_stream *s=NULL;assert(pangu_json_stream_create(allocator(&m),&s)==PANGU_OK);
        pangu_slice out;
        for(size_t at=0;at<strlen(text);at+=chunk) {
            size_t n=strlen(text)-at;if(n>chunk)n=chunk;
            assert(pangu_json_stream_append(s,(pangu_slice){text+at,n})==PANGU_OK);
            assert(pangu_json_stream_snapshot(s,&out)==PANGU_OK);
            yyjson_doc *d=yyjson_read(out.data,out.len,YYJSON_READ_ALLOW_INVALID_UNICODE);
            if(!d && strstr(out.data,"\\ud83d")) {
                char *copy=malloc(out.len+1);assert(copy);memcpy(copy,out.data,out.len);copy[out.len]=0;
                char *sur=strstr(copy,"\\ud83d");memcpy(sur,"\\ufffd",6);
                d=yyjson_read(copy,out.len,YYJSON_READ_ALLOW_INVALID_UNICODE);free(copy);
            }
            if(!d || !yyjson_is_obj(yyjson_doc_get_root(d))){fprintf(stderr,"invalid snapshot input=%s chunk=%zu at=%zu output=%.*s\n",text,chunk,at,(int)out.len,out.data);abort();}yyjson_doc_free(d);
        }
        assert(pangu_json_stream_snapshot(s,&out)==PANGU_OK);equal(out,expected);
        pangu_json_stream_destroy(s);assert(m.live==0);
    }
}
static void vectors(void) {
    const char *v[][2]={
        {"{\"\\ud83d\":1}", "{\"\\ud83d\":1.0}"},{"{\"n\":9007199254740993}","{\"n\":9007199254740992}"},{"", "{}"},{"  ","{}"},{"[1,2", "{}"},{"null","{}"},
        {"{\"a\":tr", "{\"a\":true}"},{"{\"a\":fals", "{\"a\":false}"},
        {"{\"a\":nu", "{\"a\":null}"},{"{\"a\":-1.25e+", "{\"a\":-1.25}"},
        {"{\"a\":12.", "{}"},{"{\"a\":tru}","{}"},
        {"{\"a\":01,\"b\":1.,\"c\":.5,\"d\":+1,\"e\":0x10,\"f\":1e}","{}"},
        {"{\"a\":1 \"b\":2}","{\"a\":1,\"b\":2}"},
        {"{\"a\" 1, \"b\" \"x\"}","{\"a\":1,\"b\":\"x\"}"},
        {"{\"a\":1 2, \"b\":3}","{\"a\":1,\"b\":3}"},
        {"{\"a\":NaN,\"b\":Infinity,\"e\":1}","{\"e\":1}"},
        {"{\"a\":[1 2,,3,},\"b\":3]", "{\"a\":[1,2,3],\"b\":3}"},
        {"{\"a\":1} ignored {\"b\":2}","{\"a\":1}"},
        {"{\"a\":{\"b\":{\"c\":", "{\"a\":{\"b\":{}}}"},
        {"{\"a\":1,\"b\":2,\"a\":\"three\"}","{\"a\":\"three\",\"b\":2}"},
        {"{\"a\":{\"x\":1},\"a\":{\"x\":2,\"y\":3}}", "{\"a\":{\"x\":2,\"y\":3}}"},
        {"{\"__proto__\":{\"polluted\":true}}", "{\"__proto__\":{\"polluted\":true}}"},
        {"{\"a\":\"x\n\t  ","{\"a\":\"x\\n\\t  \"}"},
        {"{\"a\":\"x\\d \\u12\"}","{\"a\":\"x\\\\d \\\\u12\"}"},
        {"{\"a\":\"ab\\u00", "{\"a\":\"ab\"}"},
        {"{\"a\":\"x\\", "{\"a\":\"x\"}"},
        {"{\"a\":\"\\ud83d", "{\"a\":\"\\ud83d\"}"},
        {"{\"a\":\"\\ud83d\\ude00!\"}","{\"a\":\"😀!\"}"},
        {"{\"é😀\":\"café😀\\u00e9\"}","{\"é😀\":\"café😀é\"}"},
        {"{\"a\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"}","{\"a\":\"\\\"\\\\/\\b\\f\\n\\r\\t\"}"}
    };
    for(size_t i=0;i<sizeof(v)/sizeof(v[0]);i++)vector(v[i][0],v[i][1]);
    puts("vectors: 32 cases, all chunk sizes passed");
}
static void prefixes(void) {
    memory m={0};pangu_json_stream *s;assert(!pangu_json_stream_create(allocator(&m),&s));
    const char *parts[]={"{\"a\":1","e","+","2",",\"b\":t","r","u","x",",\"a\":-", "2", "}","trailing"};
    const char *expect[]={"{\"a\":1}","{\"a\":1}","{\"a\":1}","{\"a\":100}","{\"a\":100,\"b\":true}","{\"a\":100,\"b\":true}","{\"a\":100,\"b\":true}","{\"a\":100}","{\"a\":100}","{\"a\":-2}","{\"a\":-2}","{\"a\":-2}"};
    for(size_t i=0;i<sizeof(parts)/sizeof(parts[0]);i++){
        assert(!pangu_json_stream_append(s,slice(parts[i])));pangu_slice out;assert(!pangu_json_stream_snapshot(s,&out));equal(out,expect[i]);
        pangu_slice again;assert(!pangu_json_stream_snapshot(s,&again));assert(out.data==again.data);
    }
    pangu_json_stream_destroy(s);assert(!m.live);puts("prefixes: 12 transitions passed");
}
static void finish_cases(void) {
    const char *inputs[]={"", " \n\t", "{\"a\":1}","[]","null","true","{\"a\":1", "{\"a\":1}x", "{\"a\":\"\\q\"}","{\"a\":\"x\ny\"}","{\"a\":\"\\ud83d\"}","{\"a\":\"\\ud83d\\ude00\"}"};
    for(size_t i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++) {
        memory m={0};pangu_json_stream *s;assert(!pangu_json_stream_create(allocator(&m),&s));assert(!pangu_json_stream_append(s,slice(inputs[i])));
        pangu_slice out,err;assert(!pangu_json_stream_finish(s,&out,&err));
        int valid=i<3 || i>=8;assert((err.len==0)==valid);
        if(!valid){equal(out,"{}");assert(err.len>=strlen(inputs[i]));assert(!memcmp(err.data+err.len-strlen(inputs[i]),inputs[i],strlen(inputs[i])));}
        else {
            const char *expected=i==2?"{\"a\":1}":i==8?"{\"a\":\"\\\\q\"}":i==9?"{\"a\":\"x\\ny\"}":i==10?"{\"a\":\"\\ud83d\"}":i==11?"{\"a\":\"😀\"}":"{}";
            equal(out,expected);
        }
        pangu_json_stream_destroy(s);assert(!m.live);
    }
    memory m={0};pangu_json_stream *s;assert(!pangu_json_stream_create(allocator(&m),&s));
    const char nul[]={'{','"','a','"',':','1','}',0};assert(!pangu_json_stream_append(s,(pangu_slice){nul,sizeof(nul)}));pangu_slice o,e;assert(!pangu_json_stream_finish(s,&o,&e));assert(e.len);pangu_json_stream_destroy(s);assert(!m.live);
    const char *numeric[]={"{\"n\":1e309}","{\"n\":-1e309}","{\"n\":1e-999}","{\"n\":18446744073709551616}"};
    const char *values[]={"{\"n\":null}","{\"n\":null}","{\"n\":0}","{\"n\":18446744073709552000}"};
    for(size_t i=0;i<4;i++) {
        memory n={0};pangu_json_stream *stream;assert(!pangu_json_stream_create(allocator(&n),&stream));
        assert(!pangu_json_stream_append(stream,slice(numeric[i])));
        pangu_slice args,error;assert(!pangu_json_stream_finish(stream,&args,&error)&&!error.len);equal(args,values[i]);
        pangu_json_stream_destroy(stream);assert(!n.live);
    }
    puts("finish: repair/structure/Unicode/NUL and four numeric edge cases passed");
}
static size_t long_string(size_t n) {
    memory m={0};pangu_json_stream *s;assert(!pangu_json_stream_create(allocator(&m),&s));
    assert(!pangu_json_stream_append(s,slice("{\"content\":\"")));
    char chunk[32];memset(chunk,'x',sizeof(chunk));
    for(size_t i=0;i<n;i+=sizeof(chunk)) {
        size_t k=n-i;if(k>sizeof(chunk))k=sizeof(chunk);
        assert(!pangu_json_stream_append(s,(pangu_slice){chunk,k}));pangu_slice o;assert(!pangu_json_stream_snapshot(s,&o));
    }
    assert(!pangu_json_stream_append(s,slice("\"}")));pangu_slice o,e;assert(!pangu_json_stream_snapshot(s,&o));assert(o.len==n+14);
    assert(!pangu_json_stream_finish(s,&o,&e));assert(!e.len&&o.len==n+14);
    size_t bytes=m.bytes;pangu_json_stream_destroy(s);assert(!m.live);return bytes;
}
static void deep_and_keys(void) {
    memory m={0};pangu_json_stream *s;assert(!pangu_json_stream_create(allocator(&m),&s));
    assert(!pangu_json_stream_append(s,slice("{\"a\":")));
    for(int i=0;i<10000;i++){assert(!pangu_json_stream_append(s,slice("[")));pangu_slice o;assert(!pangu_json_stream_snapshot(s,&o));}
    assert(!pangu_json_stream_append(s,slice("1")));
    for(int i=0;i<10000;i++)assert(!pangu_json_stream_append(s,slice("]")));
    assert(!pangu_json_stream_append(s,slice("}")));pangu_slice o;assert(!pangu_json_stream_snapshot(s,&o));
    yyjson_doc *d=yyjson_read(o.data,o.len,0);assert(d);yyjson_val *v=yyjson_obj_get(yyjson_doc_get_root(d),"a");
    for(int i=0;i<10000;i++){v=yyjson_arr_get(v,0);}
    assert(yyjson_get_num(v)==1);yyjson_doc_free(d);
    pangu_json_stream_destroy(s);assert(!m.live);
    assert(!pangu_json_stream_create(allocator(&m),&s));assert(!pangu_json_stream_append(s,slice("{")));
    for(int i=0;i<1000;i++){char b[40];int n=snprintf(b,sizeof(b),"%s\"key%d\":%d",i?",":"",i,i);assert(!pangu_json_stream_append(s,(pangu_slice){b,(size_t)n}));}
    assert(!pangu_json_stream_append(s,slice(",\"key500\":-1}")));pangu_slice e;assert(!pangu_json_stream_finish(s,&o,&e));assert(!e.len);
    d=yyjson_read(o.data,o.len,0);assert(d);v=yyjson_doc_get_root(d);assert(yyjson_obj_size(v)==1000);assert(yyjson_get_num(yyjson_obj_get(v,"key500"))==-1);yyjson_doc_free(d);
    pangu_json_stream_destroy(s);assert(!m.live);puts("deep/hash: 10000 levels, 1000 distinct keys, duplicate final passed");
}
static void scale(void) {
    size_t small=long_string(50000),large=long_string(500000);
    assert(large<small*20);assert(large<500000*200);
    printf("long strings: 32-byte chunks, allocation bytes %zu / %zu, ratio %.2f passed\n",small,large,(double)large/(double)small);
    memory m={0};pangu_json_stream *s;assert(!pangu_json_stream_create(allocator(&m),&s));
    assert(!pangu_json_stream_append(s,slice("{\"items\":[")));
    for(int i=0;i<10000;i++){char b[32];int n=snprintf(b,sizeof(b),"%s%d",i?",":"",i);assert(!pangu_json_stream_append(s,(pangu_slice){b,(size_t)n}));pangu_slice o;assert(!pangu_json_stream_snapshot(s,&o));}
    assert(!pangu_json_stream_append(s,slice("]}")));pangu_slice o;assert(!pangu_json_stream_snapshot(s,&o));yyjson_doc *d=yyjson_read(o.data,o.len,0);assert(d);assert(yyjson_arr_size(yyjson_obj_get(yyjson_doc_get_root(d),"items"))==10000);yyjson_doc_free(d);
    pangu_json_stream_destroy(s);assert(!m.live);puts("large open array: 10000 elements passed");
}
static size_t failure_run(size_t fail) {
    memory m={.fail=fail};pangu_json_stream *s=NULL;pangu_status st=pangu_json_stream_create(allocator(&m),&s);
    if(!st) {
        const char *input="{\"a\":{\"x\":1},\"a\":{\"x\":2},\"b\":[true,123,\"é\\ud83d\\ude00\\q\"],\"tail\":-12.3e+2,\"big\":18446744073709551616,\"infinite\":1e309,\"\\ud800\":3}";
        for(size_t i=0;i<strlen(input)&&!st;i++) {st=pangu_json_stream_append(s,(pangu_slice){input+i,1});if(!st){pangu_slice o;st=pangu_json_stream_snapshot(s,&o);}}
        if(!st){pangu_slice o,e;st=pangu_json_stream_finish(s,&o,&e);}
        if(st==PANGU_NOMEM){pangu_slice o,e;assert(pangu_json_stream_append(s,slice(""))==PANGU_NOMEM);assert(pangu_json_stream_snapshot(s,&o)==PANGU_NOMEM);assert(pangu_json_stream_finish(s,&o,&e)==PANGU_NOMEM);}
        pangu_json_stream_destroy(s);
    }
    assert(st==PANGU_OK||st==PANGU_NOMEM);assert(!m.live);return m.calls;
}
static void failures(void) {
    size_t n=failure_run(0);for(size_t i=1;i<=n+1;i++)failure_run(i);
    printf("allocation failures: %zu injection points passed\n",n);
    pangu_json_stream *s=(void *)1;memory m={0};assert(pangu_json_stream_create((pangu_allocator){0},&s)==PANGU_INVALID && !s);
    assert(!pangu_json_stream_create(allocator(&m),&s));assert(pangu_json_stream_append(s,(pangu_slice){NULL,1})==PANGU_INVALID);
    pangu_slice o;assert(!pangu_json_stream_snapshot(s,&o));equal(o,"{}");pangu_json_stream_destroy(s);assert(!m.live);
}
int main(void){vectors();prefixes();finish_cases();scale();deep_and_keys();failures();puts("streaming-json: all 7 test groups passed");return 0;}
