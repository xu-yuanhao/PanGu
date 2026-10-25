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
static unsigned allocation_failure,array_add_calls;
int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
void *__real_calloc(size_t,size_t);
void *__real_reallocarray(void *,size_t,size_t);
struct json_object *__real_json_object_new_array_ext(int);
int __real_json_object_array_add(struct json_object *,struct json_object *);
int __wrap_json_object_deep_copy(struct json_object *src,struct json_object **dst,json_c_shallow_copy_fn *fn){if(allocation_failure==1){allocation_failure=0;*dst=json_object_new_object();return -1;}return __real_json_object_deep_copy(src,dst,fn);}
void *__wrap_calloc(size_t n,size_t size){if(allocation_failure==2){allocation_failure=0;return 0;}return __real_calloc(n,size);}
void *__wrap_reallocarray(void *p,size_t n,size_t size){if(allocation_failure==3){allocation_failure=0;return 0;}return __real_reallocarray(p,n,size);}
struct json_object *__wrap_json_object_new_array_ext(int size){if(allocation_failure==4){allocation_failure=0;return 0;}return __real_json_object_new_array_ext(size);}
int __wrap_json_object_array_add(struct json_object *array,struct json_object *value){if(allocation_failure==6&&++array_add_calls==2){allocation_failure=0;return -1;}if(allocation_failure==5){allocation_failure=0;return -1;}return __real_json_object_array_add(array,value);}
#endif
static unsigned adopted;
static void adopt(void *ctx,uint64_t sequence){pg_record_memory *memory=ctx;assert(sequence==memory->next_sequence-1);++adopted;uint64_t id;assert(pg_record_memory_mint(memory,&id)==PG_MEMORY_INVALID);assert(pg_record_memory_close(memory)==PG_MEMORY_INVALID);struct json_object *record=0;assert(!pg_record_memory_read(memory,PG_TABLE_ENTRY,3,&record));json_object_put(record);}
static int commit_text(pg_record_memory *memory,const char *json,uint64_t *seq){struct json_object *batch=json_tokener_parse(json);assert(batch);pg_record_storage storage=pg_record_memory_storage(memory);int result=storage.commit(storage.context,batch,adopt,memory,seq);json_object_put(batch);return result;}
static void check(pg_record_memory *memory,pg_record_table table,uint64_t id,const char *expected){struct json_object *record=0;assert(!pg_record_memory_read(memory,table,id,&record));struct json_object *want=json_tokener_parse(expected);assert(want&&json_object_equal(record,want));json_object_put(want);json_object_object_add(record,"local",json_object_new_boolean(1));json_object_put(record);assert(!pg_record_memory_read(memory,table,id,&record)&&!json_object_object_get(record,"local"));json_object_put(record);}
static int consumer_adopt(void *ctx,const struct json_object *batch,uint64_t sequence){(void)batch;adopt(ctx,sequence);pg_memory_query q={.table=PG_TABLE_ENTRY,.limit=1,.flags=PG_SCAN_CONVERSATION,.conversation_id=1};pg_memory_page p={0};assert(!pg_record_memory_scan(ctx,&q,&p));json_object_put(p.items);return 0;}
_Static_assert(sizeof(pg_memory_query)==112&&offsetof(pg_memory_query,flags)==32&&offsetof(pg_memory_query,kind)==80&&offsetof(pg_memory_query,status)==96&&offsetof(pg_memory_query,background)==104,"query ABI");
_Static_assert(sizeof(pg_memory_page)==24&&offsetof(pg_memory_page,has_next)==20,"page ABI");
static void scan_tests(void){
 pg_record_memory memory={0};uint64_t seq;assert(!pg_record_memory_open(&memory));
 assert(!commit_text(&memory,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\",\"head\":3}},{\"type\":\"entry\",\"value\":{\"id\":4,\"conversationId\":1,\"kind\":\"test\",\"head\":4}},{\"type\":\"conversation\",\"value\":{\"id\":10,\"parent\":{\"conversationId\":1,\"at\":3}}},{\"type\":\"entry\",\"value\":{\"id\":11,\"conversationId\":10,\"kind\":\"test\"}}]",&seq));
 pg_memory_query q={.table=PG_TABLE_ENTRY,.limit=1,.flags=PG_SCAN_CONVERSATION,.conversation_id=10};pg_memory_page p={0};
 assert(!pg_record_memory_scan(&memory,&q,&p)&&p.order==PG_SCAN_DESCENDING&&p.has_next&&p.after==11&&json_object_array_length(p.items)==1);json_object_put(p.items);
 q.has_cursor=1;q.after=11;assert(!pg_record_memory_scan(&memory,&q,&p)&&!p.has_next&&json_object_get_int(json_object_object_get(json_object_array_get_idx(p.items,0),"id"))==3);json_object_put(p.items);
 q.order=PG_SCAN_ASCENDING;assert(pg_record_memory_scan(&memory,&q,&p)==PG_MEMORY_INVALID&&!p.items&&!p.has_next);q.has_cursor=0;q.order=0;
 struct json_object *entry=0;assert(!pg_record_memory_entry(&memory,3,10,&entry,&seq)&&seq==1);json_object_put(entry);assert(pg_record_memory_entry(&memory,11,1,&entry,&seq)==PG_MEMORY_MISSING&&!entry&&!seq);assert(!pg_record_memory_head(&memory,10,UINT64_MAX,&entry)&&json_object_get_int(json_object_object_get(entry,"id"))==3);json_object_put(entry);assert(pg_record_memory_head(&memory,10,0,&entry)==PG_MEMORY_MISSING&&!entry);
#ifdef PG_MEMORY_FAULTS
 for(unsigned fault=1;fault<=6;++fault){if(fault==2)continue;allocation_failure=fault;array_add_calls=0;q.limit=fault==6?2:1;assert(pg_record_memory_scan(&memory,&q,&p)==PG_MEMORY_ALLOCATION&&!p.items&&!p.has_next&&memory.count==5&&memory.next_sequence==2);}
 allocation_failure=1;assert(pg_record_memory_entry(&memory,3,10,&entry,&seq)==PG_MEMORY_ALLOCATION&&!entry&&!seq);
#endif
 q.limit=0;assert(pg_record_memory_scan(&memory,&q,&p)==PG_MEMORY_INVALID&&!p.items);q.limit=1;q.has_cursor=2;assert(pg_record_memory_scan(&memory,&q,&p)==PG_MEMORY_INVALID);q.has_cursor=1;q.after=INT64_MIN;assert(pg_record_memory_scan(&memory,&q,&p)==PG_MEMORY_INVALID);q.has_cursor=0;q.conversation_id=999;assert(pg_record_memory_scan(&memory,&q,&p)==PG_MEMORY_INVALID);q.conversation_id=10;
 long page=sysconf(_SC_PAGESIZE);char *guard=mmap(0,(size_t)page*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED&&!mprotect(guard+page,(size_t)page,PROT_NONE)&&!mprotect(guard+page*3,(size_t)page,PROT_NONE));pg_memory_query *last_query=(pg_memory_query*)(guard+page-sizeof(q));pg_memory_page *last_page=(pg_memory_page*)(guard+page*3-sizeof(p));*last_query=q;memset(last_page,0,sizeof(*last_page));assert(!pg_record_memory_scan(&memory,last_query,last_page));assert(!pg_record_memory_close(&memory)&&json_object_array_length(last_page->items)==1);json_object_put(last_page->items);assert(!munmap(guard,(size_t)page*4));
}
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
 scan_tests();
 puts("PASS: real memory table writes, global IDs, fork-aware scans/entry/head, paging/filters, detached ownership, failure rollback and lifetime");return 0;
}
