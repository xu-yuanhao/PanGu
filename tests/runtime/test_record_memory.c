#define _GNU_SOURCE
#include "../../src/runtime/record_memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <json-c/json.h>
_Static_assert(sizeof(pg_memory_row)==32&&offsetof(pg_memory_row,table)==24,"row ABI");
_Static_assert(sizeof(pg_record_memory)==48&&offsetof(pg_record_memory,next_id)==24&&offsetof(pg_record_memory,busy)==40,"memory ABI");
#ifdef PG_MEMORY_FAULTS
static unsigned allocation_failure;
int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
void *__real_calloc(size_t,size_t);
void *__real_reallocarray(void *,size_t,size_t);
int __wrap_json_object_deep_copy(struct json_object *src,struct json_object **dst,json_c_shallow_copy_fn *fn){if(allocation_failure==1){allocation_failure=0;*dst=json_object_new_object();return -1;}return __real_json_object_deep_copy(src,dst,fn);}
void *__wrap_calloc(size_t n,size_t size){if(allocation_failure==2){allocation_failure=0;return 0;}return __real_calloc(n,size);}
void *__wrap_reallocarray(void *p,size_t n,size_t size){if(allocation_failure==3){allocation_failure=0;return 0;}return __real_reallocarray(p,n,size);}
#endif
static unsigned adopted;
static void adopt(void *ctx,uint64_t sequence){pg_record_memory *memory=ctx;assert(sequence==memory->next_sequence-1);++adopted;uint64_t id;assert(pg_record_memory_mint(memory,&id)==PG_MEMORY_INVALID);assert(pg_record_memory_close(memory)==PG_MEMORY_INVALID);struct json_object *record=0;assert(!pg_record_memory_read(memory,PG_TABLE_ENTRY,3,&record));json_object_put(record);}
static int commit_text(pg_record_memory *memory,const char *json,uint64_t *seq){struct json_object *batch=json_tokener_parse(json);assert(batch);pg_record_storage storage=pg_record_memory_storage(memory);int result=storage.commit(storage.context,batch,adopt,memory,seq);json_object_put(batch);return result;}
static void check(pg_record_memory *memory,pg_record_table table,uint64_t id,const char *expected){struct json_object *record=0;assert(!pg_record_memory_read(memory,table,id,&record));struct json_object *want=json_tokener_parse(expected);assert(want&&json_object_equal(record,want));json_object_put(want);json_object_object_add(record,"local",json_object_new_boolean(1));json_object_put(record);assert(!pg_record_memory_read(memory,table,id,&record)&&!json_object_object_get(record,"local"));json_object_put(record);}
static int consumer_adopt(void *ctx,const struct json_object *batch,uint64_t sequence){(void)batch;adopt(ctx,sequence);return 0;}
int main(void){
 pg_record_memory memory={0};assert(!pg_record_memory_open(&memory)&&pg_record_memory_open(&memory)==PG_MEMORY_INVALID);uint64_t id=0,seq=0;assert(!pg_record_memory_mint(&memory,&id)&&id==2);
 assert(!commit_text(&memory,"[{\"type\":\"task\",\"value\":{\"id\":5,\"conversationId\":1,\"kind\":\"test\",\"state\":{\"status\":\"pending\"}}},{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\",\"data\":{\"x\":1}}},{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"submission\",\"value\":{\"id\":7,\"conversationId\":1,\"status\":\"queued\"}},{\"type\":\"task\",\"value\":{\"id\":5,\"conversationId\":1,\"kind\":\"test\",\"state\":{\"status\":\"running\"}}}]",&seq));
 assert(seq==1&&adopted==1&&memory.count==4&&memory.next_id==8);check(&memory,PG_TABLE_CONVERSATION,1,"{\"id\":1}");check(&memory,PG_TABLE_ENTRY,3,"{\"id\":3,\"conversationId\":1,\"kind\":\"test\",\"data\":{\"x\":1}}");check(&memory,PG_TABLE_TASK,5,"{\"id\":5,\"conversationId\":1,\"kind\":\"test\",\"state\":{\"status\":\"running\"}}");
 assert(commit_text(&memory,"[{\"type\":\"entry\",\"value\":{\"id\":4}},{\"type\":\"conversation\",\"value\":{\"id\":3}}]",&seq)==PG_MEMORY_INVALID&&seq==0&&adopted==1&&memory.count==4&&memory.next_sequence==2&&memory.next_id==8);struct json_object *out=0;assert(pg_record_memory_read(&memory,PG_TABLE_ENTRY,4,&out)==PG_MEMORY_MISSING&&!out);
 assert(commit_text(&memory,"[{\"type\":\"entry\",\"value\":{\"id\":3}}]",&seq)==PG_MEMORY_INVALID);assert(commit_text(&memory,"[{\"type\":\"task\",\"value\":{\"id\":1}}]",&seq)==PG_MEMORY_INVALID);
 assert(commit_text(&memory,"[{\"type\":\"entry\",\"value\":{\"id\":4}},{\"type\":\"entry\",\"value\":{\"id\":4}}]",&seq)==PG_MEMORY_INVALID&&memory.count==4);
 assert(!commit_text(&memory,"[{\"type\":\"submission\",\"value\":{\"id\":7,\"conversationId\":1,\"status\":\"done\"}}]",&seq)&&seq==2);check(&memory,PG_TABLE_SUBMISSION,7,"{\"id\":7,\"conversationId\":1,\"status\":\"done\"}");
 assert(!pg_record_memory_mint(&memory,&id)&&id==8);assert(pg_record_memory_read(&memory,PG_TABLE_TASK,3,&out)==PG_MEMORY_MISSING);assert(pg_record_memory_read(&memory,PG_TABLE_ENTRY,0,&out)==PG_MEMORY_INVALID);
 assert(commit_text(&memory,"[{\"type\":\"entry\\u0000wrong\",\"value\":{\"id\":9}}]",&seq)==PG_MEMORY_INVALID);assert(commit_text(&memory,"[{\"type\":\"entry\",\"value\":{\"id\":1.5}}]",&seq)==PG_MEMORY_INVALID);
 struct json_object *bulk=json_object_new_array();assert(bulk);for(unsigned i=31;i>1;--i){if(i==3||i==5||i==7)continue;struct json_object *write_value=json_object_new_object(),*record=json_object_new_object();assert(write_value&&record);assert(!json_object_object_add(record,"id",json_object_new_int((int)i)));assert(!json_object_object_add(record,"conversationId",json_object_new_int(1)));assert(!json_object_object_add(record,"kind",json_object_new_string("test")));assert(!json_object_object_add(write_value,"type",json_object_new_string("entry")));assert(!json_object_object_add(write_value,"value",record));assert(!json_object_array_add(bulk,write_value));}
 pg_record_storage service=pg_record_memory_storage(&memory);
#ifdef PG_MEMORY_FAULTS
 allocation_failure=3;assert(service.commit(service.context,bulk,adopt,&memory,&seq)==PG_MEMORY_ALLOCATION&&memory.count==4&&memory.next_sequence==3&&!memory.busy);check(&memory,PG_TABLE_CONVERSATION,1,"{\"id\":1}");
#endif
 assert(!service.commit(service.context,bulk,adopt,&memory,&seq)&&seq==3&&memory.count==31&&memory.capacity>=31&&memory.next_id==32);json_object_put(bulk);for(size_t i=0;i<memory.count;++i)assert(memory.rows[i].id==i+1);assert(memory.rows[2].sequence==1&&memory.rows[3].sequence==3&&memory.rows[6].sequence==2);
 assert(!pg_record_memory_close(&memory)&&!pg_record_memory_close(&memory));assert(pg_record_memory_mint(&memory,&id)==PG_MEMORY_INVALID&&pg_record_memory_read(&memory,PG_TABLE_ENTRY,3,&out)==PG_MEMORY_INVALID);assert(!pg_record_memory_open(&memory)&&memory.count==0&&memory.next_id==2&&!pg_record_memory_close(&memory));
 assert(!pg_record_memory_open(&memory));pg_record_session session={.storage=pg_record_memory_storage(&memory),.context=&memory,.adopt=consumer_adopt};struct json_object *mixed=json_tokener_parse("[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\"}}]");assert(mixed&&!pg_records_commit(&session,mixed)&&session.sequence==1&&memory.count==2);json_object_put(mixed);assert(!pg_record_memory_close(&memory));
#ifdef PG_MEMORY_FAULTS
 for(unsigned mode=1;mode<=3;++mode){assert(!pg_record_memory_open(&memory));allocation_failure=mode;assert(commit_text(&memory,"[{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\"}}]",&seq)==PG_MEMORY_ALLOCATION&&!memory.rows&&!memory.count&&!memory.busy&&memory.next_id==2&&memory.next_sequence==1);assert(!commit_text(&memory,"[{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\"}}]",&seq)&&seq==1);allocation_failure=1;assert(pg_record_memory_read(&memory,PG_TABLE_ENTRY,3,&out)==PG_MEMORY_ALLOCATION&&!out);assert(!pg_record_memory_close(&memory));}
#endif
 long page=sysconf(_SC_PAGESIZE);char *guard=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED&&!mprotect(guard+page,(size_t)page,PROT_NONE));pg_record_memory *tail=(pg_record_memory*)(guard+page-sizeof(pg_record_memory));memset(tail,0,sizeof(*tail));assert(!pg_record_memory_open(tail)&&!commit_text(tail,"[{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\"}}]",&seq));assert(!pg_record_memory_read(tail,PG_TABLE_ENTRY,3,&out)&&out);assert(!pg_record_memory_close(tail)&&json_object_get_int(json_object_object_get(out,"id"))==3);json_object_put(out);assert(!munmap(guard,(size_t)page*2));
 puts("PASS: real memory table provider, atomic mixed writes, shared ID rules, immutable entries, last task/submission update, detached reads and lifetime");return 0;
}
