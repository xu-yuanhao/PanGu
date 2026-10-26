#define _GNU_SOURCE
#include "../../src/runtime/record_memory.h"
#include "../../src/runtime/delta.h"
#include "../../src/runtime/document_tx.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <json-c/json.h>
#include <json-c/linkhash.h>
_Static_assert(sizeof(pg_memory_row)==32&&offsetof(pg_memory_row,table)==24,"row ABI");
_Static_assert(sizeof(pg_record_memory)==48&&offsetof(pg_record_memory,next_id)==24&&offsetof(pg_record_memory,busy)==40,"memory ABI");
#ifdef PG_MEMORY_FAULTS
static unsigned allocation_failure,array_add_calls,doc_tracking,doc_calls,doc_fail;
static int doc_fault(void){return doc_tracking&&++doc_calls==doc_fail;}
struct lh_table *__real_lh_table_new(int,lh_entry_free_fn *,lh_hash_fn *,lh_equal_fn *);
struct lh_table *__real_lh_kptr_table_new(int,lh_entry_free_fn *);
struct lh_table *__wrap_lh_kptr_table_new(int size,lh_entry_free_fn *f){return doc_fault()?0:__real_lh_kptr_table_new(size,f);}
int __real_lh_table_insert(struct lh_table *,const void *,const void *);
int __real_lh_table_resize(struct lh_table *,int);
int __wrap_lh_table_resize(struct lh_table *t,int size){return doc_fault()?-1:__real_lh_table_resize(t,size);}
struct lh_table *__wrap_lh_table_new(int size,lh_entry_free_fn *f,lh_hash_fn *h,lh_equal_fn *e){return doc_fault()?0:__real_lh_table_new(size,f,h,e);}
int __wrap_lh_table_insert(struct lh_table *t,const void *k,const void *v){return doc_fault()?-1:__real_lh_table_insert(t,k,v);}

int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
void *__real_calloc(size_t,size_t);
void *__real_reallocarray(void *,size_t,size_t);
struct json_object *__real_json_object_new_array_ext(int);
int __real_json_object_array_add(struct json_object *,struct json_object *);
int __real_json_object_array_put_idx(struct json_object *,size_t,struct json_object *);
int __wrap_json_object_array_put_idx(struct json_object *o,size_t index,struct json_object *v){return doc_fault()?-1:__real_json_object_array_put_idx(o,index,v);}
int __wrap_json_object_deep_copy(struct json_object *src,struct json_object **dst,json_c_shallow_copy_fn *fn){if(doc_fault()){*dst=json_object_new_object();return -1;}if(allocation_failure==1){allocation_failure=0;*dst=json_object_new_object();return -1;}return __real_json_object_deep_copy(src,dst,fn);}
void *__wrap_calloc(size_t n,size_t size){if(doc_fault())return 0;if(allocation_failure==2){allocation_failure=0;return 0;}return __real_calloc(n,size);}
void *__wrap_reallocarray(void *p,size_t n,size_t size){if(doc_fault())return 0;if(allocation_failure==3){allocation_failure=0;return 0;}return __real_reallocarray(p,n,size);}
struct json_object *__wrap_json_object_new_array_ext(int size){if(doc_fault())return 0;if(allocation_failure==4){allocation_failure=0;return 0;}return __real_json_object_new_array_ext(size);}
int __wrap_json_object_array_add(struct json_object *array,struct json_object *value){if(doc_fault())return -1;if(allocation_failure==6&&++array_add_calls==2){allocation_failure=0;return -1;}if(allocation_failure==5){allocation_failure=0;return -1;}return __real_json_object_array_add(array,value);}
struct json_object *__real_json_object_new_object(void);
struct json_object *__real_json_object_new_int64(int64_t);
struct json_object *__real_json_object_new_string(const char *);
int __real_json_object_object_add(struct json_object *,const char *,struct json_object *);
struct json_object *__wrap_json_object_new_object(void){return doc_fault()?0:__real_json_object_new_object();}
struct json_object *__wrap_json_object_new_int64(int64_t n){return doc_fault()?0:__real_json_object_new_int64(n);}
struct json_object *__wrap_json_object_new_string(const char *s){return doc_fault()?0:__real_json_object_new_string(s);}
int __wrap_json_object_object_add(struct json_object *o,const char *key,struct json_object *v){return doc_fault()?-1:__real_json_object_object_add(o,key,v);}
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
static void doc_adopt(void *context,uint64_t seq){(void)context;assert(seq);}
static int doc_commit(pg_record_memory *memory,const char *text){struct json_object *batch=json_tokener_parse(text);assert(batch);uint64_t seq=0;pg_record_storage service=pg_record_memory_storage(memory);int result=service.commit(memory,batch,doc_adopt,0,&seq);json_object_put(batch);return result;}
static void document_writes(void){
 _Static_assert(sizeof(pg_document_query)==64&&offsetof(pg_document_query,at)==32&&offsetof(pg_document_query,has_cursor)==56,"document query ABI");
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));
 assert(!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"test\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":1}}}]"));
 pg_document_address address={.kind={"test",4},.scope=PG_DOC_SESSION};struct json_object *found=0;assert(!pg_record_memory_find_document(&m,&address,UINT64_MAX,&found)&&json_object_get_int(json_object_object_get(found,"id"))==2);json_object_object_add(found,"local",json_object_new_boolean(1));json_object_put(found);assert(!pg_record_memory_find_document(&m,&address,1,&found)&&!json_object_object_get(found,"local"));json_object_put(found);assert(pg_record_memory_find_document(&m,&address,0,&found)==PG_MEMORY_MISSING&&!found);address.has_key=1;assert(pg_record_memory_find_document(&m,&address,UINT64_MAX,&found)==PG_MEMORY_MISSING&&!found);address.has_key=0;
 assert(m.count==1&&m.rows[0].table==4&&m.next_id==3);struct json_object *state=m.rows[0].value,*record=json_object_object_get(state,"record"),*revisions=json_object_object_get(state,"revisions");assert(json_object_get_int(json_object_object_get(record,"createdAt"))==1&&json_object_array_length(revisions)==1);
 assert(!doc_commit(&m,"[{\"type\":\"document.change\",\"id\":2,\"content\":{\"kind\":\"delta\",\"version\":1,\"ops\":[[\"s\",[\"x\"],2]]}}]"));
 assert(json_object_array_length(json_object_object_get(m.rows[0].value,"revisions"))==2);
 assert(doc_commit(&m,"[{\"type\":\"document.change\",\"id\":2,\"content\":{\"kind\":\"delta\",\"version\":2,\"ops\":[]}}]")==PG_MEMORY_INVALID&&m.next_sequence==3);
 assert(doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":3,\"kind\":\"test\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{}}}]")==PG_MEMORY_INVALID&&m.count==1);
 assert(!doc_commit(&m,"[{\"type\":\"document.retire\",\"id\":2},{\"type\":\"document.create\",\"record\":{\"id\":3,\"kind\":\"test\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{}}}]"));
 assert(m.count==2&&json_object_array_length(json_object_object_get(m.rows[0].value,"revisions"))==0&&json_object_get_int(json_object_object_get(json_object_object_get(m.rows[0].value,"record"),"retiredAt"))==3);
 assert(doc_commit(&m,"[{\"type\":\"document.retire\",\"id\":2}]")==PG_MEMORY_INVALID);
 assert(!pg_record_memory_find_document(&m,&address,2,&found)&&json_object_get_int(json_object_object_get(found,"id"))==2);json_object_put(found);assert(!pg_record_memory_find_document(&m,&address,3,&found)&&json_object_get_int(json_object_object_get(found,"id"))==3);json_object_put(found);
 pg_document_query query={.scope=PG_DOC_SESSION,.at=UINT64_MAX,.limit=1};pg_memory_page page={0};assert(!pg_record_memory_scan_documents(&m,&query,&page)&&json_object_array_length(page.items)==1&&!page.has_next&&json_object_get_int(json_object_object_get(json_object_array_get_idx(page.items,0),"id"))==3);json_object_put(page.items);query.at=2;assert(!pg_record_memory_scan_documents(&m,&query,&page)&&json_object_get_int(json_object_object_get(json_object_array_get_idx(page.items,0),"id"))==2);json_object_put(page.items);query.limit=0;assert(pg_record_memory_scan_documents(&m,&query,&page)==PG_MEMORY_INVALID&&!page.items&&!page.has_next);query.limit=1;
 assert(!pg_record_memory_close(&m));
#ifdef PG_MEMORY_FAULTS
 const char *source="[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"test\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":1}}}]";
 struct json_object *batch=json_tokener_parse(source);assert(batch);pg_record_storage service=pg_record_memory_storage(&m);uint64_t seq;
 assert(!pg_record_memory_open(&m));doc_tracking=1;doc_calls=doc_fail=0;assert(!service.commit(&m,batch,doc_adopt,0,&seq));doc_tracking=0;unsigned count=doc_calls;assert(!pg_record_memory_close(&m));
 for(unsigned i=1;i<=count;++i){assert(!pg_record_memory_open(&m));doc_tracking=1;doc_calls=0;doc_fail=i;int status=service.commit(&m,batch,doc_adopt,0,&seq);doc_tracking=0;assert(status==PG_MEMORY_ALLOCATION&&!seq&&!m.count&&m.next_sequence==1&&m.next_id==2&&!m.busy);assert(!pg_record_memory_close(&m));}
 doc_fail=0;json_object_put(batch);
 const char *updates[]={
 "[{\"type\":\"document.change\",\"id\":2,\"content\":{\"kind\":\"delta\",\"version\":1,\"ops\":[[\"s\",[\"x\"],2]]}},{\"type\":\"task\",\"value\":{\"id\":8}}]",
 "[{\"type\":\"document.retire\",\"id\":2},{\"type\":\"document.create\",\"record\":{\"id\":3,\"kind\":\"test\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{}}}]"};
 for(unsigned mode=0;mode<2;++mode){batch=json_tokener_parse(updates[mode]);assert(batch);assert(!pg_record_memory_open(&m)&&!doc_commit(&m,source));doc_tracking=1;doc_calls=doc_fail=0;assert(!service.commit(&m,batch,doc_adopt,0,&seq));doc_tracking=0;count=doc_calls;assert(!pg_record_memory_close(&m));
 for(unsigned i=1;i<=count;++i){assert(!pg_record_memory_open(&m)&&!doc_commit(&m,source));struct json_object *before=0;assert(!json_object_deep_copy(m.rows[0].value,&before,0));doc_tracking=1;doc_calls=0;doc_fail=i;int status=service.commit(&m,batch,doc_adopt,0,&seq);doc_tracking=0;assert(status==PG_MEMORY_ALLOCATION&&!seq&&m.count==1&&m.next_sequence==2&&m.next_id==3&&!m.busy&&json_object_equal(before,m.rows[0].value));json_object_put(before);assert(!pg_record_memory_close(&m));}doc_fail=0;json_object_put(batch);}

#endif
}
static void document_copies(void){
 const char *source="[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"copy\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":1}}}]";
 const char *copy="[{\"type\":\"document.copy\",\"record\":{\"id\":3,\"kind\":\"copy\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":4},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"source\":{\"id\":2,\"at\":\"current\"}}]";
 pg_record_memory m={0};uint64_t seq;struct json_object *batch=json_tokener_parse(copy),*saved=0,*snapshot=0;assert(batch&&!json_object_deep_copy(batch,&saved,0));pg_record_storage service=pg_record_memory_storage(&m);assert(!pg_record_memory_open(&m)&&!doc_commit(&m,source));assert(!service.commit(&m,batch,doc_adopt,0,&seq)&&seq==2&&json_object_equal(batch,saved));assert(!pg_record_memory_document(&m,3,UINT64_MAX,&snapshot));assert(json_object_get_int(json_object_object_get(json_object_object_get(snapshot,"value"),"x"))==1);json_object_put(snapshot);
 assert(!doc_commit(&m,"[{\"type\":\"document.change\",\"id\":2,\"content\":{\"kind\":\"base\",\"version\":2,\"value\":{\"x\":9}}}]"));assert(!pg_record_memory_document(&m,3,UINT64_MAX,&snapshot)&&json_object_get_int(json_object_object_get(snapshot,"version"))==1);json_object_put(snapshot);assert(!pg_record_memory_close(&m));
#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_memory_open(&m)&&!doc_commit(&m,source));doc_tracking=1;doc_calls=doc_fail=0;assert(!service.commit(&m,batch,doc_adopt,0,&seq));doc_tracking=0;unsigned count=doc_calls;assert(!pg_record_memory_close(&m));
 for(unsigned i=1;i<=count;++i){assert(!pg_record_memory_open(&m)&&!doc_commit(&m,source));struct json_object *before=0;assert(!json_object_deep_copy(m.rows[0].value,&before,0));doc_tracking=1;doc_calls=0;doc_fail=i;int status=service.commit(&m,batch,doc_adopt,0,&seq);doc_tracking=0;assert(status==PG_MEMORY_ALLOCATION&&!seq&&m.count==1&&m.next_sequence==2&&m.next_id==3&&!m.busy&&json_object_equal(before,m.rows[0].value)&&json_object_equal(batch,saved));json_object_put(before);assert(!pg_record_memory_close(&m));}doc_fail=0;
#endif
 json_object_put(saved);json_object_put(batch);
}
static int tx_read(void *context,unsigned table,uint64_t id,struct json_object **out){return pg_record_memory_read(context,(pg_record_table)table,id,out);}
static int tx_mint(void *context,uint64_t *id){return pg_record_memory_mint(context,id);}
static void typed_entries(void){
 pg_record_memory memory={0};assert(!pg_record_memory_open(&memory));pg_record_session session={.storage=pg_record_memory_storage(&memory)};pg_record_tx tx={0};struct json_object *conversation=json_tokener_parse("{\"type\":\"conversation\",\"value\":{\"id\":1}}"),*draft=json_tokener_parse("{\"kind\":\"test\",\"head\":\"self\",\"byTaskId\":999,\"id\":999,\"conversationId\":999,\"data\":{\"x\":1}}"),*entry=0;assert(conversation&&draft);
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read));assert(pg_record_tx_append_entry(&tx,1,draft,0,tx_mint,&entry)==PG_TX_INVALID&&!entry&&memory.next_id==2&&!json_object_array_length(tx.writes));assert(!pg_record_tx_stage(&tx,PG_TX_CONVERSATION,conversation));assert(!pg_record_tx_append_entry(&tx,1,draft,0,tx_mint,&entry)&&json_object_get_int(json_object_object_get(entry,"id"))==2&&json_object_get_int(json_object_object_get(entry,"head"))==2&&!json_object_object_get(entry,"byTaskId"));json_object_object_add(entry,"local",json_object_new_boolean(1));json_object_put(entry);assert(!pg_record_tx_commit(&tx)&&memory.count==2);assert(!pg_record_memory_read(&memory,PG_TABLE_ENTRY,2,&entry)&&!json_object_object_get(entry,"local"));json_object_put(entry);
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_append_entry(&tx,1,draft,7,tx_mint,&entry)&&json_object_get_int(json_object_object_get(entry,"byTaskId"))==7);json_object_put(entry);assert(!pg_record_tx_discard(&tx)&&memory.next_id==4&&memory.count==2);assert(json_object_get_int(json_object_object_get(draft,"id"))==999);
#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read));doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_record_tx_append_entry(&tx,1,draft,7,tx_mint,&entry));doc_tracking=0;unsigned count=doc_calls;json_object_put(entry);assert(!pg_record_tx_discard(&tx));
 for(unsigned i=1;i<=count;++i){assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_record_tx_append_entry(&tx,1,draft,7,tx_mint,&entry);doc_tracking=0;assert(status!=PG_TX_OK&&!entry&&!tx.busy&&tx.has_table_write&&!json_object_array_length(tx.writes)&&memory.count==2);assert(!pg_record_tx_discard(&tx));}doc_fail=0;
#endif
 json_object_put(conversation);json_object_put(draft);assert(!pg_record_memory_close(&memory));
}
static void typed_conversations(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};struct json_object *out=0,*task=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}");assert(task);assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_create_conversation(&tx,0,1,0,&out)&&json_object_get_int(json_object_object_get(out,"id"))==1&&m.next_id==2);json_object_put(out);assert(!pg_record_tx_stage(&tx,PG_TX_TASK,task)&&!pg_record_tx_create_conversation(&tx,20,0,tx_mint,&out)&&json_object_get_int(json_object_object_get(out,"id"))==2&&json_object_get_int(json_object_object_get(json_object_object_get(out,"owner"),"taskId"))==20);json_object_object_add(out,"local",json_object_new_boolean(1));json_object_put(out);assert(!pg_record_tx_commit(&tx)&&m.count==3&&!pg_record_memory_read(&m,PG_TABLE_CONVERSATION,2,&out)&&!json_object_object_get(out,"local"));json_object_put(out);
 json_object_object_add(json_object_object_get(task,"value"),"conversationId",json_object_new_int(2));assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_TASK,task)&&!pg_record_tx_create_conversation(&tx,20,0,tx_mint,&out)&&json_object_get_int(json_object_object_get(json_object_object_get(out,"owner"),"conversationId"))==2);json_object_put(out);assert(!pg_record_tx_discard(&tx));
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_create_conversation(&tx,20,0,tx_mint,&out)&&json_object_get_int(json_object_object_get(out,"id"))==22);json_object_put(out);assert(!pg_record_tx_discard(&tx));uint64_t before=m.next_id;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&pg_record_tx_create_conversation(&tx,999,0,tx_mint,&out)==PG_TX_INVALID&&!out&&m.next_id==before+1&&!json_object_array_length(tx.writes)&&!pg_record_tx_discard(&tx));
#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_record_tx_create_conversation(&tx,20,0,tx_mint,&out));doc_tracking=0;unsigned count=doc_calls;json_object_put(out);assert(!pg_record_tx_discard(&tx));for(unsigned i=1;i<=count;++i){assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_record_tx_create_conversation(&tx,20,0,tx_mint,&out);doc_tracking=0;assert(status!=PG_TX_OK&&!out&&!tx.busy&&tx.has_table_write&&!json_object_array_length(tx.writes)&&m.count==3);assert(!pg_record_tx_discard(&tx));}doc_fail=0;
#endif
 json_object_put(task);assert(!pg_record_memory_close(&m));
}
static int fork_entry(void *context,uint64_t conversation,uint64_t id,struct json_object **out,uint64_t *seq){return pg_record_memory_entry(context,id,conversation,out,seq);}
static int fork_scan(void *context,const pg_fork_query *query,pg_fork_page *out){pg_document_query q={.scope=PG_DOC_CONVERSATION,.owner=query->conversation_id,.at=query->at,.limit=256,.after=query->after,.has_cursor=query->has_cursor};pg_memory_page page={0};int status=pg_record_memory_scan_documents(context,&q,&page);*out=(pg_fork_page){.items=page.items,.after=(int64_t)page.after,.has_next=page.has_next};return status;}
static int fork_context_read(void *context,unsigned table,uint64_t id,struct json_object **out){pg_record_memory **memory=context;return pg_record_memory_read(*memory,(pg_record_table)table,id,out);}
static void fork_plans(void){
 _Static_assert(sizeof(pg_fork_query)==32&&sizeof(pg_fork_page)==24&&sizeof(pg_fork_storage)==32,"fork ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m));
 const char *initial="[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"conversation\",\"value\":{\"id\":2,\"parent\":{\"conversationId\":1,\"at\":20}}},{\"type\":\"entry\",\"value\":{\"id\":20,\"conversationId\":1,\"kind\":\"test\"}},{\"type\":\"document.create\",\"record\":{\"id\":10,\"kind\":\"past\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"v\":1}}},{\"type\":\"document.create\",\"record\":{\"id\":30,\"kind\":\"now\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":2},\"history\":\"latest\",\"fork\":\"current\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"v\":3}}}]";
 assert(!doc_commit(&m,initial)&&!doc_commit(&m,"[{\"type\":\"document.change\",\"id\":10,\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"v\":2}}}]"));pg_fork_storage service={&m,fork_entry,fork_scan,tx_mint};struct json_object *plans=0;assert(!pg_records_prepare_fork(&service,2,20,3,&plans)&&json_object_array_length(plans)==2);struct json_object *past=json_object_array_get_idx(plans,0),*now=json_object_array_get_idx(plans,1);assert(json_object_get_int(json_object_object_get(json_object_object_get(past,"source"),"at"))==1&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(now,"source"),"at")),"current"));assert(!json_object_object_get(json_object_object_get(past,"record"),"createdAt"));
 struct json_object *batch=json_tokener_parse("[{\"type\":\"conversation\",\"value\":{\"id\":3,\"parent\":{\"conversationId\":2,\"at\":20}}}]");for(size_t i=0;i<2;++i){struct json_object *plan=json_object_array_get_idx(plans,i),*write=json_object_new_object();json_object_object_add(write,"type",json_object_new_string("document.copy"));json_object_object_add(write,"record",json_object_get(json_object_object_get(plan,"record")));json_object_object_add(write,"source",json_object_get(json_object_object_get(plan,"source")));assert(!json_object_array_add(batch,write));}pg_record_storage storage=pg_record_memory_storage(&m);uint64_t sequence;assert(!storage.commit(&m,batch,doc_adopt,0,&sequence));json_object_put(batch);struct json_object *snapshot=0;assert(!pg_record_memory_document(&m,31,UINT64_MAX,&snapshot)&&json_object_get_int(json_object_object_get(json_object_object_get(snapshot,"value"),"v"))==1);json_object_put(snapshot);json_object_put(plans);
 assert(!doc_commit(&m,"[{\"type\":\"task\",\"value\":{\"id\":7,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}]"));pg_record_memory *read_memory=&m;
 pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&read_memory,fork_context_read));struct json_object *child=0;uint64_t child_id=m.next_id;assert(!pg_record_tx_fork_conversation(&tx,2,20,7,&service,&child)&&json_object_get_uint64(json_object_object_get(child,"id"))==child_id&&json_object_get_int(json_object_object_get(json_object_object_get(child,"parent"),"conversationId"))==2&&m.count==9);json_object_object_add(json_object_object_get(child,"parent"),"at",json_object_new_int(999));json_object_put(child);assert(!pg_record_tx_commit(&tx)&&m.count==12);assert(!pg_record_memory_read(&m,PG_TABLE_CONVERSATION,child_id,&child)&&json_object_get_int(json_object_object_get(json_object_object_get(child,"parent"),"at"))==20);json_object_put(child);assert(!pg_record_memory_document(&m,child_id+1,UINT64_MAX,&snapshot)&&json_object_get_int(json_object_object_get(json_object_object_get(snapshot,"value"),"v"))==1);json_object_put(snapshot);
 struct json_object *prior=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":8,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}");assert(prior&&!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_TASK,prior));assert(pg_record_tx_fork_conversation(&tx,2,999,0,&service,&child)==PG_TX_READ_FAILED&&!child&&json_object_array_length(tx.writes)==1&&!tx.busy&&!pg_record_tx_discard(&tx));
 struct json_object *change=json_tokener_parse("{\"type\":\"document.change\",\"id\":10,\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{}}}");assert(change&&!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CHANGE,change));assert(pg_record_tx_fork_conversation(&tx,2,20,0,&service,&child)==PG_TX_INVALID&&!child&&json_object_array_length(tx.writes)==1&&!session.failed&&!pg_record_tx_discard(&tx));assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_fork_conversation(&tx,2,20,0,&service,&child));json_object_put(child);assert(!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CHANGE,change));sequence=m.next_sequence;assert(pg_record_tx_commit(&tx)==PG_TX_INVALID&&!session.failed&&m.next_sequence==sequence&&!tx.writes);json_object_put(change);
#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_TASK,prior));doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_record_tx_fork_conversation(&tx,2,20,0,&service,&child));doc_tracking=0;unsigned tx_count=doc_calls;json_object_put(child);assert(!pg_record_tx_discard(&tx));for(unsigned i=1;i<=tx_count;++i){assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_TASK,prior));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_record_tx_fork_conversation(&tx,2,20,0,&service,&child);doc_tracking=0;assert(status!=PG_TX_OK&&!child&&!tx.busy&&json_object_array_length(tx.writes)==1&&m.count==12&&!session.failed);assert(!pg_record_tx_discard(&tx));}doc_fail=0;
#endif
 struct json_object *fresh=json_tokener_parse("{\"type\":\"document.create\",\"record\":{\"id\":9,\"kind\":\"new-current\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":2},\"history\":\"latest\",\"fork\":\"current\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{}}}");assert(fresh);for(unsigned order=0;order<2;++order){assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));if(!order){assert(!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CREATE,fresh)&&pg_record_tx_fork_conversation(&tx,2,20,0,&service,&child)==PG_TX_INVALID&&!child&&json_object_array_length(tx.writes)==1);}else{assert(!pg_record_tx_fork_conversation(&tx,2,20,0,&service,&child));json_object_put(child);assert(!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CREATE,fresh)&&pg_record_tx_commit(&tx)==PG_TX_INVALID&&!session.failed);}assert(!pg_record_tx_discard(&tx));}json_object_put(fresh);
 json_object_put(prior);
 assert(pg_records_prepare_fork(&service,2,999,4,&plans)==PG_TX_READ_FAILED&&!plans);
#ifdef PG_MEMORY_FAULTS
 doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_records_prepare_fork(&service,2,20,4,&plans));doc_tracking=0;unsigned count=doc_calls;json_object_put(plans);for(unsigned i=1;i<=count;++i){doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_records_prepare_fork(&service,2,20,4,&plans);doc_tracking=0;assert(status!=PG_TX_OK&&!plans&&!m.busy); }doc_fail=0;
#endif
 assert(!pg_record_memory_close(&m));
}
static int tx_reentrant(void *context,unsigned table,uint64_t id,struct json_object **out){(void)table;(void)id;pg_record_tx *tx=context;struct json_object *value=0;assert(pg_record_tx_discard(tx)==PG_TX_INVALID&&pg_record_tx_commit(tx)==PG_TX_INVALID&&pg_record_tx_read(tx,0,1,&value)==PG_TX_INVALID&&!value&&pg_record_tx_begin(tx,tx->session,context,tx_reentrant)==PG_TX_INVALID);*out=json_object_new_object();assert(*out);return 2;}
static void transaction_batches(void){
 _Static_assert(sizeof(pg_record_tx)==48&&offsetof(pg_record_tx,busy)==32&&offsetof(pg_record_tx,has_table_write)==40,"Tx ABI");
 pg_record_memory memory={0};assert(!pg_record_memory_open(&memory));pg_record_session session={.storage=pg_record_memory_storage(&memory)};pg_record_tx tx={0};struct json_object *out=0,*conversation=json_tokener_parse("{\"type\":\"conversation\",\"value\":{\"id\":1}}"),*entry=json_tokener_parse("{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\"}}");assert(conversation&&entry);
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read));assert(pg_record_tx_begin(&tx,&session,&memory,tx_read)==PG_TX_INVALID);assert(pg_record_tx_read(&tx,PG_TABLE_ENTRY,3,&out)==PG_TX_MISSING&&!out);assert(!pg_record_tx_stage(&tx,PG_TX_CONVERSATION,conversation));assert(pg_record_tx_read(&tx,PG_TABLE_ENTRY,3,&out)==PG_TX_READ_AFTER_WRITE&&!out);assert(!pg_record_tx_stage(&tx,PG_TX_ENTRY,entry)&&!memory.count);json_object_object_add(json_object_object_get(entry,"value"),"local",json_object_new_boolean(1));assert(!pg_record_tx_commit(&tx)&&session.sequence==1&&memory.count==2&&!tx.writes);assert(pg_record_tx_commit(&tx)==PG_TX_SEALED&&pg_record_tx_read(&tx,0,1,&out)==PG_TX_SEALED&&!out);assert(!pg_record_memory_read(&memory,PG_TABLE_ENTRY,3,&out)&&!json_object_object_get(out,"local"));json_object_put(out);
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_read(&tx,PG_TABLE_ENTRY,3,&out));json_object_put(out);assert(pg_record_tx_stage(&tx,PG_TX_ENTRY,conversation)==PG_TX_INVALID&&tx.has_table_write&&json_object_array_length(tx.writes)==0);assert(pg_record_tx_read(&tx,0,1,&out)==PG_TX_READ_AFTER_WRITE&&!out);assert(!pg_record_tx_discard(&tx)&&!pg_record_tx_discard(&tx)&&!tx.writes&&session.sequence==1);
 struct json_object *document=json_tokener_parse("{\"type\":\"document.create\",\"record\":{\"id\":4,\"kind\":\"test\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{}}}");assert(document);assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CREATE,document)&&!tx.has_table_write&&!pg_record_tx_read(&tx,PG_TABLE_ENTRY,3,&out));json_object_put(out);assert(!pg_record_tx_commit(&tx)&&memory.count==3&&session.sequence==2);json_object_put(document);
#ifdef PG_MEMORY_FAULTS
 allocation_failure=4;assert(pg_record_tx_begin(&tx,&session,&memory,tx_read)==PG_TX_ALLOCATION&&!tx.writes&&!tx.busy);assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read));allocation_failure=1;assert(pg_record_tx_stage(&tx,PG_TX_ENTRY,entry)==PG_TX_ALLOCATION&&!tx.busy&&tx.has_table_write&&!json_object_array_length(tx.writes));assert(!pg_record_tx_discard(&tx));
#endif
 assert(!pg_record_tx_begin(&tx,&session,&tx,tx_reentrant)&&pg_record_tx_read(&tx,0,1,&out)==PG_TX_READ_FAILED&&!out&&!tx.busy&&!pg_record_tx_discard(&tx));
 struct json_object *submission=json_tokener_parse("{\"type\":\"submission\",\"value\":{\"id\":6,\"conversationId\":1,\"type\":\"input\",\"status\":\"queued\"}}"),*placement=json_tokener_parse("{\"status\":\"placed\",\"entry\":3}"),*settlement=json_tokener_parse("{\"status\":\"done\",\"answer\":{\"ok\":true}}");assert(submission&&placement&&settlement);assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_SUBMISSION,submission)&&!pg_record_tx_submission_change(&tx,6,placement)&&!pg_record_tx_submission_change(&tx,6,settlement)&&!pg_record_tx_commit(&tx));assert(!pg_record_memory_read(&memory,PG_TABLE_SUBMISSION,6,&out)&&!strcmp(json_object_get_string(json_object_object_get(out,"status")),"done")&&json_object_object_get(out,"answer"));json_object_put(out);assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_submission_change(&tx,6,placement)&&!pg_record_tx_commit(&tx));assert(!pg_record_memory_read(&memory,PG_TABLE_SUBMISSION,6,&out)&&!strcmp(json_object_get_string(json_object_object_get(out,"status")),"done"));json_object_put(out);json_object_put(submission);json_object_put(placement);json_object_put(settlement);
 session.read_only=1;assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_ENTRY,entry)&&pg_record_tx_commit(&tx)==PG_TX_COMMIT_FAILED&&!session.failed&&!tx.writes&&memory.count==4);session.read_only=0;
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_ENTRY,entry));assert(pg_record_tx_commit(&tx)==PG_TX_COMMIT_FAILED&&session.failed&&!tx.writes&&!tx.busy&&memory.count==4&&memory.next_sequence==5);assert(pg_record_tx_begin(&tx,&session,&memory,tx_read)==PG_TX_INVALID);assert(!pg_record_tx_discard(&tx));json_object_put(conversation);json_object_put(entry);assert(!pg_record_memory_close(&memory));
}
#ifdef PG_MEMORY_FAULTS
static void transaction_faults(void){
 pg_record_memory memory={0};assert(!pg_record_memory_open(&memory));pg_record_session session={.storage=pg_record_memory_storage(&memory)};pg_record_tx tx={0};struct json_object *submission=json_tokener_parse("{\"type\":\"submission\",\"value\":{\"id\":6,\"conversationId\":1,\"type\":\"input\",\"status\":\"queued\"}}"),*placement=json_tokener_parse("{\"status\":\"placed\",\"entry\":3}");assert(submission&&placement);
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_SUBMISSION,submission));doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_record_tx_submission_change(&tx,6,placement));doc_tracking=0;unsigned count=doc_calls;assert(!pg_record_tx_discard(&tx));
 for(unsigned i=1;i<=count;++i){assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_SUBMISSION,submission));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_record_tx_submission_change(&tx,6,placement);doc_tracking=0;assert(status==PG_TX_ALLOCATION&&!tx.busy&&json_object_array_length(tx.writes)==1&&!memory.count&&memory.next_sequence==1);assert(!pg_record_tx_discard(&tx));}doc_fail=0;
 assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read));doc_tracking=1;doc_calls=0;assert(!pg_record_tx_stage(&tx,PG_TX_SUBMISSION,submission));doc_tracking=0;count=doc_calls;assert(!pg_record_tx_discard(&tx));for(unsigned i=1;i<=count;++i){assert(!pg_record_tx_begin(&tx,&session,&memory,tx_read));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_record_tx_stage(&tx,PG_TX_SUBMISSION,submission);doc_tracking=0;assert(status==PG_TX_ALLOCATION&&!tx.busy&&tx.has_table_write&&!json_object_array_length(tx.writes));assert(!pg_record_tx_discard(&tx));}doc_fail=0;
 json_object_put(submission);json_object_put(placement);assert(!pg_record_memory_close(&memory));
}
#endif
static int bad_tx_scan(void *context,const pg_memory_query *query,pg_memory_page *out){(void)query;pg_record_tx *tx=context;assert(pg_record_tx_discard(tx)==PG_TX_INVALID&&pg_record_tx_commit(tx)==PG_TX_INVALID);out->items=json_object_new_array();assert(out->items);out->after=33;out->has_next=1;return 2;}
static int bad_tx_head(void *context,uint64_t conversation,uint64_t max,struct json_object **out){(void)conversation;assert(max==UINT64_MAX&&pg_record_tx_discard(context)==PG_TX_INVALID);*out=json_object_new_object();assert(*out);return 3;}
static void transaction_queries(void){
 _Static_assert(sizeof(pg_record_queries)==24,"query service ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m));assert(!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test\",\"head\":3}},{\"type\":\"conversation\",\"value\":{\"id\":10,\"parent\":{\"conversationId\":1,\"at\":3}}},{\"type\":\"entry\",\"value\":{\"id\":11,\"conversationId\":10,\"kind\":\"test\"}},{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}},{\"type\":\"submission\",\"value\":{\"id\":30,\"conversationId\":1,\"status\":\"queued\"}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_record_queries service;assert(!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":40}}]"));pg_record_memory_queries(&m,&service);assert(service.context==&m&&service.scan&&service.head&&!pg_record_tx_begin(&tx,&session,&m,tx_read));
 pg_memory_query query={.table=PG_TABLE_ENTRY,.flags=PG_SCAN_CONVERSATION,.conversation_id=10,.limit=1};pg_memory_page page={0};struct json_object *head=0;assert(!pg_record_tx_scan(&tx,&service,&query,&page)&&page.has_next&&page.after==11);json_object_put(page.items);query.has_cursor=1;query.after=11;assert(!pg_record_tx_scan(&tx,&service,&query,&page)&&!page.has_next&&json_object_get_int(json_object_object_get(json_object_array_get_idx(page.items,0),"id"))==3);json_object_put(page.items);assert(!pg_record_tx_latest_head(&tx,&service,10,&head)&&json_object_get_int(json_object_object_get(head,"id"))==3);json_object_put(head);assert(pg_record_tx_latest_head(&tx,&service,999,&head)==PG_TX_READ_FAILED&&!head);assert(pg_record_tx_latest_head(&tx,&service,40,&head)==PG_TX_MISSING&&!head);
 for(unsigned table=0;table<4;++table){query=(pg_memory_query){.table=table,.limit=100};if(table==PG_TABLE_ENTRY){query.flags=PG_SCAN_CONVERSATION;query.conversation_id=1;}assert(!pg_record_tx_scan(&tx,&service,&query,&page)&&json_object_array_length(page.items)>0);json_object_put(page.items);}
 pg_record_queries broken={&tx,bad_tx_scan,bad_tx_head};assert(pg_record_tx_scan(&tx,&broken,&query,&page)==PG_TX_READ_FAILED&&!page.items&&!page.has_next&&!page.after&&!page.order&&!tx.busy);assert(pg_record_tx_latest_head(&tx,&broken,1,&head)==PG_TX_READ_FAILED&&!head&&!tx.busy);
#ifdef PG_MEMORY_FAULTS
 for(unsigned kind=0;kind<2;++kind){doc_tracking=1;doc_calls=doc_fail=0;assert(!(kind?pg_record_tx_latest_head(&tx,&service,10,&head):pg_record_tx_scan(&tx,&service,&query,&page)));doc_tracking=0;unsigned count=doc_calls;if(kind)json_object_put(head);else json_object_put(page.items);for(unsigned i=1;i<=count;++i){doc_tracking=1;doc_calls=0;doc_fail=i;int status=kind?pg_record_tx_latest_head(&tx,&service,10,&head):pg_record_tx_scan(&tx,&service,&query,&page);doc_tracking=0;assert(status==PG_TX_READ_FAILED&&!tx.busy);if(kind)assert(!head);else assert(!page.items&&!page.has_next&&!page.after&&!page.order);}doc_fail=0;}
#endif
 struct json_object *write=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{\"status\":\"waiting\"}}}");assert(write&&!pg_record_tx_stage(&tx,PG_TX_TASK,write));assert(pg_record_tx_scan(&tx,&service,&query,&page)==PG_TX_READ_AFTER_WRITE&&!page.items&&!page.has_next&&pg_record_tx_latest_head(&tx,&service,10,&head)==PG_TX_READ_AFTER_WRITE&&!head);assert(!pg_record_tx_commit(&tx));assert(pg_record_tx_scan(&tx,&service,&query,&page)==PG_TX_SEALED&&pg_record_tx_latest_head(&tx,&service,10,&head)==PG_TX_SEALED);json_object_put(write);assert(!pg_record_memory_close(&m));
}
typedef struct {pg_record_memory *memory;pg_conversation_host *host;unsigned calls,nest,fail_depth;} creation_fixture;
static int created_hook(void *context,pg_record_tx *tx,const struct json_object *record){creation_fixture *f=context;++f->calls;assert(tx->creation_depth&&!tx->busy&&pg_record_tx_commit(tx)==PG_TX_INVALID&&pg_record_tx_discard(tx)==PG_TX_INVALID);struct json_object *out=0;assert(pg_record_tx_read(tx,PG_TABLE_CONVERSATION,1,&out)==PG_TX_READ_AFTER_WRITE&&!out);uint64_t id=json_object_get_uint64(json_object_object_get(record,"id"));struct json_object *draft=json_tokener_parse("{\"kind\":\"created\",\"head\":\"self\"}");assert(draft);int status=pg_record_tx_append_entry(tx,id,draft,0,tx_mint,&out);json_object_put(draft);if(status)return 1;json_object_put(out);if(f->nest&&tx->creation_depth==1){pg_conversation_request request={0};status=pg_record_tx_create(tx,&request,f->host,&out);if(status)return 1;json_object_put(out);}return f->fail_depth==tx->creation_depth;}
static void conversation_hooks(void){
 _Static_assert(sizeof(pg_conversation_request)==32&&sizeof(pg_conversation_host)==32&&offsetof(pg_record_tx,creation_depth)==44,"creation ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_fork_storage fork={&m,fork_entry,fork_scan,tx_mint};creation_fixture fixture={.memory=&m,.nest=1};pg_conversation_host host={&fixture,tx_mint,&fork,created_hook};fixture.host=&host;pg_conversation_request request={.root=1};struct json_object *out=0;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_create(&tx,&request,&host,&out)&&!tx.creation_depth&&fixture.calls==2&&!m.count&&json_object_array_length(tx.writes)==4);json_object_put(out);assert(!pg_record_tx_commit(&tx)&&m.count==4);fixture.nest=0;request=(pg_conversation_request){0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_create(&tx,&request,&host,&out)&&fixture.calls==3);json_object_put(out);assert(!pg_record_tx_commit(&tx)&&m.count==6);
 request=(pg_conversation_request){.parent=1,.entry=2};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_create(&tx,&request,&host,&out)&&fixture.calls==4&&json_object_get_int(json_object_object_get(json_object_object_get(out,"parent"),"at"))==2);json_object_put(out);assert(!pg_record_tx_commit(&tx)&&m.count==8);
 fixture.fail_depth=1;request=(pg_conversation_request){0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&pg_record_tx_create(&tx,&request,&host,&out)==PG_TX_CREATION_FAILED&&!out&&!tx.creation_depth&&!tx.busy&&!session.failed&&m.count==8&&json_object_array_length(tx.writes)==2&&!pg_record_tx_discard(&tx));fixture.fail_depth=2;fixture.nest=1;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&pg_record_tx_create(&tx,&request,&host,&out)==PG_TX_CREATION_FAILED&&!out&&!tx.creation_depth&&!tx.busy&&!pg_record_tx_discard(&tx));fixture.fail_depth=0;fixture.nest=0;
#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_record_tx_create(&tx,&request,&host,&out));doc_tracking=0;unsigned count=doc_calls;json_object_put(out);assert(!pg_record_tx_discard(&tx));for(unsigned i=1;i<=count;++i){assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_record_tx_create(&tx,&request,&host,&out);doc_tracking=0;assert(status!=PG_TX_OK&&!out&&!tx.creation_depth&&!tx.busy&&m.count==8&&!session.failed&&!pg_record_tx_discard(&tx));}doc_fail=0;
#endif
 host.created=0;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_create(&tx,&request,&host,&out)&&json_object_array_length(tx.writes)==1);json_object_put(out);assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&m));
}
static void final_owners(void){
 const char *statuses[]={"pending","running","waiting","sleeping","completing","terminal"};
 for(unsigned kind=0;kind<2;++kind)for(unsigned order=0;order<2;++order)for(unsigned state=0;state<6;++state)for(unsigned abort=0;abort<2;++abort){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));struct json_object *change=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{}}}"),*value=json_object_object_get(change,"value"),*work=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":21,\"conversationId\":1,\"owner\":20,\"state\":{\"status\":\"pending\"}}}"),*record=0;assert(change&&work);json_object_object_add(json_object_object_get(value,"state"),"status",json_object_new_string(statuses[state]));json_object_object_add(value,"abortRequested",json_object_new_boolean((int)abort));if(!order)assert(!pg_record_tx_stage(&tx,PG_TX_TASK,change));if(kind)assert(!pg_record_tx_stage(&tx,PG_TX_TASK,work));else{assert(!pg_record_tx_create_conversation(&tx,20,0,tx_mint,&record));json_object_put(record);}if(order)assert(!pg_record_tx_stage(&tx,PG_TX_TASK,change));int allowed=state<4&&!abort;assert(pg_record_tx_commit(&tx)==(allowed?PG_TX_OK:PG_TX_INVALID)&&!session.failed&&!tx.writes&&!tx.busy&&m.count==(allowed?3u:2u)&&m.next_sequence==(allowed?3u:2u));json_object_put(change);json_object_put(work);assert(!pg_record_memory_close(&m));
 }
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{\"status\":\"terminal\"}}},{\"type\":\"task\",\"value\":{\"id\":21,\"conversationId\":1,\"owner\":20,\"state\":{\"status\":\"pending\"}}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};struct json_object *replace=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":21,\"conversationId\":1,\"owner\":20,\"state\":{\"status\":\"waiting\"}}}");assert(replace&&!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_TASK,replace)&&!pg_record_tx_commit(&tx));json_object_put(replace);assert(!pg_record_memory_close(&m));
 assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));session=(pg_record_session){.storage=pg_record_memory_storage(&m)};struct json_object *new_owner=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}"),*child=0;assert(new_owner&&!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_TASK,new_owner)&&!pg_record_tx_create_conversation(&tx,20,0,tx_mint,&child));json_object_put(child);json_object_object_add(json_object_object_get(json_object_object_get(new_owner,"value"),"state"),"status",json_object_new_string("terminal"));assert(!pg_record_tx_stage(&tx,PG_TX_TASK,new_owner)&&pg_record_tx_commit(&tx)==PG_TX_INVALID&&!session.failed&&m.count==1&&m.next_sequence==2);json_object_put(new_owner);assert(!pg_record_memory_close(&m));

#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"task\",\"value\":{\"id\":20,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}]"));session=(pg_record_session){.storage=pg_record_memory_storage(&m)};struct json_object *record=0;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_create_conversation(&tx,20,0,tx_mint,&record));json_object_put(record);allocation_failure=1;assert(pg_record_tx_commit(&tx)==PG_TX_INVALID&&!session.failed&&!tx.writes&&m.count==2&&m.next_sequence==2);assert(!pg_record_memory_close(&m));
#endif
}
static int migrate_document(void *context,struct json_object *state,uint64_t version,struct json_object **out){(void)context;assert(version==1);if(json_object_object_add(state,"text",json_object_new_string("migrated")))return 1;*out=json_object_get(state);return 0;}
static void document_edit_commit(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"edit\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"a\"}}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};struct json_object *old=0;assert(!pg_record_memory_document(&m,2,UINT64_MAX,&old));
 for(unsigned step=0;step<3;++step){struct json_object *snapshot=0,*migrated=0,*ops=json_tokener_parse(step==1?"[]":step==0?"[[\"a\",[\"text\"],\"b\"]]":"[[\"a\",[\"text\"],\"!\"]]");assert(ops&&!pg_record_memory_document(&m,2,UINT64_MAX,&snapshot));uint64_t stored=(uint64_t)json_object_get_int64(json_object_object_get(snapshot,"version")),version=step?2:1;struct json_object *base=json_object_object_get(snapshot,"value");if(step==1){pg_document_definition definition={.kind={"edit",4},.version=2,.scope=PG_DOC_SESSION,.migrate=migrate_document};pg_document_record record={.address={.kind={"edit",4},.scope=PG_DOC_SESSION},.id=2};assert(!pg_document_materialize(&definition,&record,stored,base,&migrated));base=migrated;}
 pg_document_change edit={0};assert(!pg_document_change_open(&edit,base)&&!pg_document_change_append(&edit,ops));pg_document_prepared prepared={0};assert(!pg_document_change_prepare(&edit,&prepared));struct json_object *write=0;assert(!pg_document_plan_change(2,stored,version,&prepared,step==2,&write)&&write&&!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CHANGE,write));json_object_put(prepared.value);json_object_put(prepared.ops);assert(!pg_document_change_discard(&edit)&&!pg_record_tx_commit(&tx));struct json_object *after=0;assert(!pg_record_memory_document(&m,2,UINT64_MAX,&after)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(after,"value"),"text")),step==0?"ab":step==1?"migrated":"migrated!")&&json_object_get_int(json_object_object_get(after,"deltasSinceBase"))==(step==0?1:0)&&json_object_get_uint64(json_object_object_get(after,"version"))==version);json_object_put(after);json_object_put(snapshot);json_object_put(migrated);json_object_put(write);json_object_put(ops);
 }
 struct json_object *current=0;assert(!pg_record_memory_document(&m,2,UINT64_MAX,&current));pg_document_prepared noop={json_object_object_get(current,"value"),json_tokener_parse("[]")};struct json_object *write=0;assert(noop.ops&&!pg_document_plan_change(2,2,2,&noop,1,&write)&&!write&&!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_commit(&tx)&&m.next_sequence==5);json_object_put(noop.ops);json_object_put(current);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(old,"value"),"text")),"a"));json_object_put(old);assert(!pg_record_memory_close(&m));
}
static void loaded_documents(void){
 _Static_assert(sizeof(pg_document_loaded)==120&&offsetof(pg_document_loaded,record)==24&&offsetof(pg_document_loaded,stored_version)==88&&offsetof(pg_document_loaded,busy)==112,"loaded ABI");_Static_assert(sizeof(pg_document_storage)==24,"document service ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"load\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));pg_document_storage storage;pg_record_memory_documents(&m,&storage);pg_document_definition definition={.kind={"load",4},.version=1,.scope=PG_DOC_SESSION};pg_document_address address={.kind={"load",4},.scope=PG_DOC_SESSION};pg_document_loaded loaded={0};assert(!pg_document_loaded_open(&loaded,&storage,&definition,&address)&&loaded.active&&!loaded.busy&&loaded.record.id==2&&loaded.stored_version==1&&loaded.value_version==1&&loaded.deltas_since_base==0);assert(pg_document_loaded_open(&loaded,&storage,&definition,&address)==PG_DOC_INVALID);pg_document_change edit={0};assert(!pg_document_loaded_begin(&loaded,&edit));struct json_object *snapshot=0;assert(!pg_document_change_snapshot(&edit,&snapshot)&&!strcmp(json_object_get_string(json_object_object_get(snapshot,"text")),"old"));json_object_put(snapshot);assert(!pg_document_change_discard(&edit)&&!pg_document_loaded_close(&loaded)&&!pg_document_loaded_close(&loaded)&&!loaded.record_owner&&!loaded.value&&!loaded.active);
 definition.version=2;assert(pg_document_loaded_open(&loaded,&storage,&definition,&address)==PG_DOC_MIGRATION_REQUIRED&&!loaded.active&&!loaded.value&&!loaded.record.address.kind.data);definition.migrate=migrate_document;assert(!pg_document_loaded_open(&loaded,&storage,&definition,&address)&&loaded.stored_version==1&&loaded.value_version==2&&!strcmp(json_object_get_string(json_object_object_get(loaded.value,"text")),"migrated"));assert(!pg_document_loaded_begin(&loaded,&edit));pg_document_prepared prepared={0};assert(!pg_document_change_prepare(&edit,&prepared));struct json_object *write=0;assert(!pg_document_plan_change(loaded.record.id,loaded.stored_version,loaded.value_version,&prepared,0,&write)&&write);pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CHANGE,write));json_object_put(write);json_object_put(prepared.value);json_object_put(prepared.ops);assert(!pg_record_tx_commit(&tx)&&!pg_document_loaded_close(&loaded));definition.version=1;definition.migrate=0;assert(pg_document_loaded_open(&loaded,&storage,&definition,&address)==PG_DOC_NEWER_VERSION&&!loaded.active&&!loaded.record_owner);
 address.kind=(pg_slice){"missing",7};definition.kind=address.kind;assert(pg_document_loaded_open(&loaded,&storage,&definition,&address)==PG_DOC_MISSING&&!loaded.active);address.kind=(pg_slice){"load",4};definition.kind=address.kind;definition.version=2;
#ifdef PG_MEMORY_FAULTS
 doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_document_loaded_open(&loaded,&storage,&definition,&address));doc_tracking=0;unsigned count=doc_calls;assert(!pg_document_loaded_close(&loaded));for(unsigned i=1;i<=count;++i){doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_document_loaded_open(&loaded,&storage,&definition,&address);doc_tracking=0;assert(status!=PG_DOC_OK&&!loaded.active&&!loaded.busy&&!loaded.record_owner&&!loaded.value&&!loaded.record.address.kind.data);}doc_fail=0;
#endif
 assert(!pg_document_loaded_open(&loaded,&storage,&definition,&address)&&!pg_record_memory_close(&m));assert(!pg_document_loaded_begin(&loaded,&edit)&&!pg_document_loaded_close(&loaded)&&!pg_document_change_snapshot(&edit,&snapshot));json_object_put(snapshot);assert(!pg_document_change_discard(&edit));
 struct json_object *record=json_tokener_parse("{\"id\":1.5,\"kind\":\"x\",\"scope\":{\"kind\":\"session\"}}");pg_document_record native={0},zero={0};assert(record&&pg_document_record_decode(record,&native)==PG_DOC_INVALID&&!memcmp(&native,&zero,sizeof(native)));json_object_put(record);
}
typedef struct {pg_record_memory *memory;pg_document_cache *cache;uint64_t now;unsigned finds,reads,flushes,pin,fail_flush;} cache_fixture;
static uint64_t cache_clock(void *context){cache_fixture *f=context;assert(f->cache->busy&&pg_document_cache_close(f->cache)==PG_DOC_INVALID);return f->now;}
static int cache_pinned(void *context,uint64_t id){cache_fixture *f=context;return f->pin&&id==2;}
static int cache_flush(void *context,const pg_document_loaded *loaded){cache_fixture *f=context;++f->flushes;assert(loaded->active&&loaded->record.id==2&&f->cache->busy);return (int)f->fail_flush;}
static int cache_find(void *context,const pg_document_address *address,uint64_t at,struct json_object **out){cache_fixture *f=context;++f->finds;return pg_record_memory_find_document(f->memory,address,at,out);}
static int cache_read(void *context,uint64_t id,uint64_t at,struct json_object **out){cache_fixture *f=context;++f->reads;return pg_record_memory_document(f->memory,id,at,out);}
static void document_cache(void){
 _Static_assert(sizeof(pg_document_cache)==112&&offsetof(pg_document_cache,busy)==104&&offsetof(pg_document_cache,pinned)==96,"cache ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"cache\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));pg_document_cache cache={0};cache_fixture f={.memory=&m,.cache=&cache};pg_document_storage storage={&f,cache_find,cache_read};pg_document_cache_hooks hooks={&f,cache_clock,cache_flush,cache_pinned};pg_document_definition one={.kind={"cache",5},.version=1,.scope=PG_DOC_SESSION},two=one;two.version=2;two.migrate=migrate_document;pg_document_address address={.kind={"cache",5},.scope=PG_DOC_SESSION};pg_document_loaded *loaded=0,*same=0;assert(!pg_document_cache_open(&cache,&storage,&hooks,100)&&!pg_document_cache_get(&cache,&one,&address,&loaded)&&cache.count==1&&f.finds==1&&f.reads==1);f.now=5;assert(!pg_document_cache_get(&cache,&one,&address,&same)&&same==loaded&&f.finds==1);pg_document_definition mismatch=one;mismatch.scope=PG_DOC_TASK;assert(pg_document_cache_get(&cache,&mismatch,&address,&same)==PG_DOC_SEMANTICS&&!same&&cache.count==1&&f.finds==1&&!cache.busy);f.now=100;assert(!pg_document_cache_sweep(&cache)&&!cache.count);assert(!pg_document_cache_get(&cache,&one,&address,&loaded)&&f.finds==2);f.now=115;assert(!pg_document_cache_get(&cache,&one,&address,&loaded));f.now=214;assert(!pg_document_cache_sweep(&cache)&&cache.count==1);f.now=215;assert(!pg_document_cache_sweep(&cache)&&!cache.count);
 f.pin=1;assert(!pg_document_cache_get(&cache,&one,&address,&loaded)&&!cache.idle_head);f.now=1000;assert(!pg_document_cache_sweep(&cache)&&cache.count==1);f.pin=0;assert(!pg_document_cache_touch(&cache,&address));f.now=1099;assert(!pg_document_cache_sweep(&cache)&&cache.count==1);f.now=1100;assert(!pg_document_cache_sweep(&cache)&&!cache.count);assert(!pg_document_cache_get(&cache,&one,&address,&loaded));unsigned reads=f.reads;assert(!pg_document_cache_get(&cache,&two,&address,&loaded)&&loaded->value_version==2&&loaded->stored_version==1&&f.reads==reads+1&&f.flushes==1);assert(!pg_document_cache_get(&cache,&two,&address,&same)&&same==loaded&&f.reads==reads+1);f.fail_flush=1;assert(pg_document_cache_get(&cache,&one,&address,&same)==PG_DOC_STORAGE_FAILED&&!same&&cache.count==1&&f.reads==reads+1);assert(!pg_document_cache_get(&cache,&two,&address,&same)&&same==loaded);f.fail_flush=0;assert(!pg_document_cache_get(&cache,&one,&address,&loaded)&&f.flushes==3&&loaded->value_version==1);assert(!pg_document_cache_evict(&cache,&address,999)&&cache.count==1&&!pg_document_cache_evict(&cache,&address,2)&&!cache.count);assert(!pg_document_cache_retention(&cache,0)&&!pg_document_cache_get(&cache,&one,&address,&loaded)&&cache.count==1&&!pg_document_cache_sweep(&cache)&&!cache.count);assert(!pg_document_cache_close(&cache)&&!pg_document_cache_close(&cache)&&pg_document_cache_get(&cache,&one,&address,&loaded)==PG_DOC_INVALID&&!loaded);
#ifdef PG_MEMORY_FAULTS
 doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_document_cache_open(&cache,&storage,&hooks,100));doc_tracking=0;unsigned count=doc_calls;assert(!pg_document_cache_close(&cache));for(unsigned i=1;i<=count;++i){doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_document_cache_open(&cache,&storage,&hooks,100);doc_tracking=0;assert(status==PG_DOC_ALLOCATION&&!cache.table&&!cache.busy);}doc_fail=0;
 assert(!pg_document_cache_open(&cache,&storage,&hooks,100));doc_tracking=1;doc_calls=0;assert(!pg_document_cache_get(&cache,&one,&address,&loaded));doc_tracking=0;count=doc_calls;assert(!pg_document_cache_close(&cache));for(unsigned i=1;i<=count;++i){assert(!pg_document_cache_open(&cache,&storage,&hooks,100));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_document_cache_get(&cache,&one,&address,&loaded);doc_tracking=0;assert(status!=PG_DOC_OK&&!loaded&&!cache.count&&!cache.head&&!cache.idle_head&&!cache.idle_tail&&!cache.busy);assert(!pg_document_cache_close(&cache));}doc_fail=0;
#endif
 assert(!pg_record_memory_close(&m));
}
static void cache_many_addresses(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));struct json_object *writes=json_object_new_array();assert(writes);for(unsigned i=0;i<160;++i){char text[512];int size=snprintf(text,sizeof(text),"{\"type\":\"document.create\",\"record\":{\"id\":%u,\"kind\":\"family\",\"key\":\"key%u\\u0000中\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"n\":%u}}}",i+2,i,i);assert(size>0&&(size_t)size<sizeof(text));struct json_object *write=json_tokener_parse(text);assert(write&&!json_object_array_add(writes,write));}pg_record_storage store=pg_record_memory_storage(&m);uint64_t seq;assert(!store.commit(&m,writes,doc_adopt,0,&seq));json_object_put(writes);pg_document_cache cache={0};cache_fixture f={.memory=&m,.cache=&cache};pg_document_storage storage={&f,cache_find,cache_read};pg_document_cache_hooks hooks={&f,cache_clock,0,cache_pinned};pg_document_definition definition={.kind={"family",6},.version=1,.scope=PG_DOC_SESSION,.family=1};assert(!pg_document_cache_open(&cache,&storage,&hooks,101));pg_document_loaded *original[160];
 for(unsigned pass=0;pass<2;++pass)for(unsigned i=0;i<160;++i){char key[64];int length=snprintf(key,sizeof(key),"key%u",i);assert(length>0);key[length]=0;memcpy(key+length+1,"中",3);pg_document_address address={.kind={"family",6},.key={key,(size_t)length+4},.scope=PG_DOC_SESSION,.has_key=1};pg_document_loaded *value=0;assert(!pg_document_cache_get(&cache,&definition,&address,&value)&&value->record.id==i+2&&json_object_get_int(json_object_object_get(value->value,"n"))==(int)i);if(!pass)original[i]=value;else assert(original[i]==value);}
 assert(cache.count==160&&f.finds==160&&f.reads==160);f.now=10;char key0[]={'k','e','y','0',0,(char)0xe4,(char)0xb8,(char)0xad};pg_document_address first={.kind={"family",6},.key={key0,sizeof(key0)},.scope=PG_DOC_SESSION,.has_key=1};assert(!pg_document_cache_touch(&cache,&first));f.now=101;assert(!pg_document_cache_sweep(&cache)&&!cache.count);/* 10ms touch is inside ceil(101/10), so it does not postpone expiry. */
 f.now=200;pg_document_loaded *value=0;assert(!pg_document_cache_get(&cache,&definition,&first,&value));f.pin=1;assert(!pg_document_cache_touch(&cache,&first)&&!cache.idle_head);f.now=1000;assert(!pg_document_cache_sweep(&cache)&&cache.count==1);f.pin=0;assert(!pg_document_cache_touch(&cache,&first));f.now=1101;assert(!pg_document_cache_sweep(&cache)&&!cache.count&&!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
}
typedef struct {pg_record_memory memory;pg_record_storage storage;pg_document_loaded loaded;pg_document_adoption adoption;unsigned mode,adopted,published;} document_commit_fixture;
static int document_adopt_callback(void *context,const struct json_object *writes,uint64_t seq){document_commit_fixture *f=context;(void)writes;assert(f->memory.next_sequence==seq+1&&!f->published);++f->adopted;
#ifdef PG_MEMORY_FAULTS
 doc_tracking=1;doc_calls=0;doc_fail=1;
#endif
 int status=pg_document_adoption_apply(&f->adoption);
#ifdef PG_MEMORY_FAULTS
 doc_tracking=0;assert(doc_calls==0);doc_fail=0;
#endif
 return status;}
static void document_publish_callback(void *context,const struct json_object *writes,uint64_t seq){document_commit_fixture *f=context;(void)writes;assert(seq==2&&f->adopted==1&&!strcmp(json_object_get_string(json_object_object_get(f->loaded.value,"text")),"new"));++f->published;}
static int document_storage_callback(void *context,const struct json_object *writes,pg_records_adopt adopt,void *owner,uint64_t *seq){document_commit_fixture *f=context;if(f->mode==1)return 2;int status=f->storage.commit(&f->memory,writes,adopt,owner,seq);return f->mode==2?2:status;}
static void document_adoption_commit(void){
 for(unsigned mode=0;mode<3;++mode){document_commit_fixture f={.mode=mode};assert(!pg_record_memory_open(&f.memory)&&!doc_commit(&f.memory,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"adopt\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));f.storage=pg_record_memory_storage(&f.memory);pg_document_storage docs;pg_record_memory_documents(&f.memory,&docs);pg_document_definition definition={.kind={"adopt",5},.version=1,.scope=PG_DOC_SESSION};pg_document_address address={.kind={"adopt",5},.scope=PG_DOC_SESSION};assert(!pg_document_loaded_open(&f.loaded,&docs,&definition,&address));pg_document_change edit={0};struct json_object *ops=json_tokener_parse("[[\"s\",[\"text\"],\"new\"]]");assert(ops&&!pg_document_loaded_begin(&f.loaded,&edit)&&!pg_document_change_append(&edit,ops));pg_document_prepared prepared={0};assert(!pg_document_change_prepare(&edit,&prepared));struct json_object *write=0;assert(!pg_document_plan_change(2,1,1,&prepared,0,&write)&&!pg_document_adoption_prepare(&f.adoption,&f.loaded,&prepared,PG_ADOPTION_DELTA));pg_record_session session={.storage={&f,document_storage_callback},.context=&f,.adopt=document_adopt_callback,.publish=document_publish_callback};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&f.memory,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CHANGE,write));json_object_object_add(prepared.value,"text",json_object_new_string("external"));int result=pg_record_tx_commit(&tx);assert(result==(mode?PG_TX_COMMIT_FAILED:PG_TX_OK)&&f.adopted==(mode==1?0u:1u)&&f.published==(mode?0u:1u)&&session.failed==(mode?PG_RECORDS_STORAGE_FAILED:0)&&!strcmp(json_object_get_string(json_object_object_get(f.loaded.value,"text")),mode==1?"old":"new")&&f.loaded.deltas_since_base==(mode==1?0u:1u));assert(!pg_document_adoption_discard(&f.adoption)&&!pg_document_change_discard(&edit)&&!pg_document_loaded_close(&f.loaded)&&!pg_record_memory_close(&f.memory));json_object_put(write);json_object_put(prepared.value);json_object_put(prepared.ops);json_object_put(ops);
 }
}
typedef struct {pg_record_tx *tx;unsigned calls,checkpoint,saw_adopt,saw_publish,fail;} docs_settlement_fixture;
static int docs_checkpoint(void *context,const struct json_object *value,const struct json_object *ops,uint64_t deltas){docs_settlement_fixture *f=context;assert(value&&ops&&deltas==0&&f->tx->sealed&&f->tx->busy&&pg_record_tx_discard(f->tx)==PG_TX_INVALID);++f->calls;return f->fail?2:(int)f->checkpoint;}
static int docs_host_adopt(void *context,const struct json_object *writes,uint64_t seq){docs_settlement_fixture *f=context;assert(writes&&seq>1&&f->saw_adopt==f->saw_publish);++f->saw_adopt;return 0;}
static void docs_host_publish(void *context,const struct json_object *writes,uint64_t seq){docs_settlement_fixture *f=context;assert(writes&&seq>1&&f->saw_adopt==f->saw_publish+1);++f->saw_publish;}
static void unified_document_settlement(void){
 _Static_assert(sizeof(pg_document_tx_entry)==152&&offsetof(pg_document_tx_entry,checkpoint)==136,"Doc Tx entry ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"one\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}},{\"type\":\"document.create\",\"record\":{\"id\":3,\"kind\":\"two\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));pg_document_storage storage;pg_record_memory_documents(&m,&storage);pg_document_definition definitions[2]={{{"one",3},1,PG_DOC_SESSION,0,0,0,0,0},{{"two",3},1,PG_DOC_SESSION,0,0,0,0,0}};pg_document_loaded loaded[2]={{0}};pg_document_change changes[2]={{0}};pg_document_tx_entry entries[2]={{0}};pg_record_tx tx={0};docs_settlement_fixture f={.tx=&tx};pg_record_session session={.storage=pg_record_memory_storage(&m),.context=&f,.adopt=docs_host_adopt,.publish=docs_host_publish};struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(ops);
 for(unsigned i=0;i<2;++i){pg_document_address address={.kind=definitions[i].kind,.scope=PG_DOC_SESSION};assert(!pg_document_loaded_open(&loaded[i],&storage,&definitions[i],&address)&&!pg_document_loaded_begin(&loaded[i],&changes[i])&&!pg_document_change_append(&changes[i],ops));entries[i]=(pg_document_tx_entry){.loaded=&loaded[i],.change=&changes[i],.context=&f,.checkpoint=docs_checkpoint};}f.checkpoint=1;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_commit(&tx,entries,2)&&f.calls==2&&f.saw_adopt==1&&f.saw_publish==1&&session.context==&f&&session.adopt==docs_host_adopt&&session.publish==docs_host_publish&&m.next_sequence==3);for(unsigned i=0;i<2;++i){assert(!strcmp(json_object_get_string(json_object_object_get(loaded[i].value,"text")),"old!")&&!loaded[i].deltas_since_base&&!entries[i].write&&!entries[i].prepared.value&&!entries[i].prepared.ops&&changes[i].sealed);}
 f.checkpoint=0;f.calls=0;for(unsigned i=0;i<2;++i)assert(!pg_document_loaded_begin(&loaded[i],&changes[i]));assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_commit(&tx,entries,2)&&!f.calls&&m.next_sequence==3&&f.saw_adopt==1);
 f.fail=1;for(unsigned i=0;i<2;++i)assert(!pg_document_loaded_begin(&loaded[i],&changes[i])&&!pg_document_change_append(&changes[i],ops));assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&pg_document_tx_commit(&tx,entries,2)==PG_TX_INVALID&&!session.failed&&m.next_sequence==3&&!strcmp(json_object_get_string(json_object_object_get(loaded[0].value,"text")),"old!"));f.fail=0;
 for(unsigned i=0;i<2;++i)assert(!pg_document_loaded_begin(&loaded[i],&changes[i])&&!pg_document_change_append(&changes[i],ops));assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));struct json_object *bad=json_tokener_parse("{\"type\":\"conversation\",\"value\":{\"id\":9,\"owner\":{\"conversationId\":1,\"taskId\":999}}}");assert(bad&&!pg_record_tx_stage(&tx,PG_TX_CONVERSATION,bad));unsigned calls=f.calls;assert(pg_document_tx_commit(&tx,entries,2)==PG_TX_INVALID&&f.calls==calls&&!session.failed&&m.next_sequence==3);json_object_put(bad);
 for(unsigned i=0;i<2;++i)assert(!pg_document_loaded_begin(&loaded[i],&changes[i])&&!pg_document_change_append(&changes[i],ops));assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));bad=json_tokener_parse("{\"type\":\"document.copy\",\"record\":{\"id\":9},\"source\":{\"id\":2,\"at\":\"current\"}}");assert(bad&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_COPY,bad));calls=f.calls;assert(pg_document_tx_commit(&tx,entries,2)==PG_TX_INVALID&&f.calls==calls&&!session.failed&&m.next_sequence==3);json_object_put(bad);
 for(unsigned i=0;i<2;++i)assert(!pg_document_loaded_begin(&loaded[i],&changes[i]));assert(!pg_document_change_append(&changes[0],ops)&&!pg_record_tx_begin(&tx,&session,&m,tx_read));struct json_object *task=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":8,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}");assert(task&&!pg_record_tx_stage(&tx,PG_TX_TASK,task));calls=f.calls;struct json_object *unchanged=loaded[1].value;assert(!pg_document_tx_commit(&tx,entries,2)&&f.calls==calls+1&&m.next_sequence==4&&loaded[0].deltas_since_base==1&&!loaded[1].deltas_since_base&&loaded[1].value==unchanged&&!strcmp(json_object_get_string(json_object_object_get(loaded[0].value,"text")),"old!!")&&session.context==&f);json_object_put(task);
 for(unsigned i=0;i<2;++i)assert(!pg_document_loaded_close(&loaded[i]));json_object_put(ops);assert(!pg_record_memory_close(&m));
}
#ifdef PG_MEMORY_FAULTS
static void document_settlement_faults(void){
 unsigned count=0;for(unsigned attempt=0;attempt<=count;++attempt){pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"edit\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));pg_document_storage storage;pg_record_memory_documents(&m,&storage);pg_document_definition definition={.kind={"edit",4},.version=1,.scope=PG_DOC_SESSION};pg_document_address address={.kind={"edit",4},.scope=PG_DOC_SESSION};pg_document_loaded loaded={0};pg_document_change change={0};struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(ops&&!pg_document_loaded_open(&loaded,&storage,&definition,&address)&&!pg_document_loaded_begin(&loaded,&change)&&!pg_document_change_append(&change,ops));pg_document_tx_entry entry={.loaded=&loaded,.change=&change};pg_record_tx tx={0};docs_settlement_fixture f={.tx=&tx,.checkpoint=1};entry.context=&f;entry.checkpoint=docs_checkpoint;pg_record_session session={.storage=pg_record_memory_storage(&m),.context=&f,.adopt=docs_host_adopt,.publish=docs_host_publish};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));doc_tracking=1;doc_calls=0;doc_fail=attempt;int status=pg_document_tx_commit(&tx,&entry,1);doc_tracking=0;if(!attempt){assert(!status);count=doc_calls;}else assert(status!=PG_TX_OK&&m.next_sequence==2&&!f.saw_adopt&&!f.saw_publish&&!strcmp(json_object_get_string(json_object_object_get(loaded.value,"text")),"old"));assert(session.context==&f&&session.adopt==docs_host_adopt&&session.publish==docs_host_publish&&!tx.busy&&!tx.writes&&tx.sealed&&change.sealed&&!entry.write&&!entry.prepared.value&&!entry.prepared.ops&&!entry.adoption.expected&&!entry.adoption.candidate);doc_fail=0;json_object_put(ops);assert(!pg_document_loaded_close(&loaded)&&!pg_record_memory_close(&m));}
}
#endif
static void managed_document_access(void){
 _Static_assert(sizeof(pg_document_tx)==48&&offsetof(pg_document_tx,busy)==40,"managed Doc Tx ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m));struct json_object *batch=json_object_new_array();assert(batch);for(unsigned i=0;i<16;++i){char text[512];int length=snprintf(text,sizeof(text),"{\"type\":\"document.create\",\"record\":{\"id\":%u,\"kind\":\"family\",\"key\":\"k%u\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}",i+2,i);assert(length>0&&(size_t)length<sizeof(text));struct json_object *write=json_tokener_parse(text);assert(write&&!json_object_array_add(batch,write));}pg_record_storage provider=pg_record_memory_storage(&m);uint64_t seq;assert(!provider.commit(&m,batch,doc_adopt,0,&seq));json_object_put(batch);pg_document_cache cache={0};cache_fixture cache_host={.memory=&m,.cache=&cache};pg_document_storage docs={&cache_host,cache_find,cache_read};pg_document_cache_hooks hooks={&cache_host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&docs,&hooks,1000));pg_record_session session={.storage=provider};pg_record_tx tx={0};pg_document_tx scope={0};pg_document_definition definition={.kind={"family",6},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_change *saved[16],*again=0;struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(ops&&!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));for(unsigned i=0;i<16;++i){char key[16];int length=snprintf(key,sizeof(key),"k%u",i);pg_document_address address={.kind={"family",6},.key={key,(size_t)length},.scope=PG_DOC_SESSION,.has_key=1};assert(!pg_document_tx_get(&scope,&definition,&address,0,0,&saved[i])&&!pg_document_change_append(saved[i],ops));}assert(scope.count==16&&scope.capacity>=16&&cache_host.finds==16);pg_document_address first={.kind={"family",6},.key={"k0",2},.scope=PG_DOC_SESSION,.has_key=1};pg_document_definition other=definition;other.version=2;assert(!pg_document_tx_get(&scope,&other,&first,0,0,&again)&&again==saved[0]&&cache_host.finds==16);struct json_object *snapshot=0;assert(!pg_document_change_snapshot(again,&snapshot)&&!strcmp(json_object_get_string(json_object_object_get(snapshot,"text")),"old!"));json_object_put(snapshot);assert(!pg_document_tx_finish(&scope)&&scope.sealed&&!scope.entries&&!scope.count&&m.next_sequence==3&&pg_document_tx_get(&scope,&definition,&first,0,0,&again)==PG_DOC_INVALID&&!again);assert(!pg_record_memory_document(&m,2,UINT64_MAX,&snapshot)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(snapshot,"value"),"text")),"old!"));json_object_put(snapshot);
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));struct json_object *table=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":100,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}");assert(table&&!pg_record_tx_stage(&tx,PG_TX_TASK,table)&&!pg_document_tx_get(&scope,&definition,&first,0,0,&again)&&!pg_document_change_append(again,ops));assert(!pg_document_tx_abort(&scope)&&!pg_document_tx_abort(&scope)&&!scope.entries&&tx.sealed&&m.next_sequence==3);json_object_put(table);pg_document_address missing=first;missing.key=(pg_slice){"missing",7};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&pg_document_tx_get(&scope,&definition,&missing,0,0,&again)==PG_DOC_MISSING&&!again&&!scope.count&&!tx.busy&&!scope.busy&&!pg_document_tx_abort(&scope));
#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_document_tx_get(&scope,&definition,&first,0,0,&again));doc_tracking=0;unsigned count=doc_calls;assert(!pg_document_tx_abort(&scope));for(unsigned i=1;i<=count;++i){assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_document_tx_get(&scope,&definition,&first,0,0,&again);doc_tracking=0;assert(status!=PG_DOC_OK&&!again&&!scope.count&&!scope.busy&&!tx.busy);assert(!pg_document_tx_abort(&scope));}doc_fail=0;
#endif
 assert(!doc_commit(&m,"[{\"type\":\"task\",\"value\":{\"id\":100,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}},{\"type\":\"document.create\",\"record\":{\"id\":101,\"kind\":\"task-doc\",\"scope\":{\"kind\":\"task\",\"taskId\":100}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{}}}]"));pg_document_definition task_definition={.kind={"task-doc",8},.version=1,.scope=PG_DOC_TASK};pg_document_address task_address={.kind={"task-doc",8},.owner=100,.scope=PG_DOC_TASK};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_get(&scope,&task_definition,&task_address,0,0,&again));table=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":100,\"conversationId\":1,\"state\":{\"status\":\"terminal\"}}}");assert(table&&!pg_record_tx_stage(&tx,PG_TX_TASK,table)&&pg_document_tx_get(&scope,&task_definition,&task_address,0,0,&again)==PG_DOC_INVALID&&!again&&scope.count==1&&!scope.busy&&!tx.busy&&!pg_document_tx_abort(&scope));json_object_put(table);
 json_object_put(ops);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
}
static int new_document_initial(void *context,struct json_object *seed,struct json_object **out){(void)context;assert(seed&&json_object_object_get(seed,"text"));*out=json_object_get(seed);return 0;}
static void document_creation_factory(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_document_definition definition={.kind={"created",7},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_address address={.kind={"created",7},.key={"member",6},.scope=PG_DOC_SESSION,.has_key=1};pg_document_initializer initializer={0,new_document_initial};struct json_object *seed=json_tokener_parse("{\"text\":\"initial\"}"),*value=0,*record=0;assert(seed&&!pg_document_initialize(&definition,seed,&initializer,&value));uint64_t id=0;assert(!pg_record_memory_mint(&m,&id)&&!pg_document_record_create(&definition,&address,id,&record));struct json_object *write=json_object_new_object(),*content=json_object_new_object();assert(write&&content&&!json_object_object_add(write,"type",json_object_new_string("document.create"))&&!json_object_object_add(write,"record",record)&&!json_object_object_add(content,"kind",json_object_new_string("base"))&&!json_object_object_add(content,"version",json_object_new_int(1))&&!json_object_object_add(content,"value",value)&&!json_object_object_add(write,"content",content));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CREATE,write)&&!pg_record_tx_commit(&tx));json_object_object_add(seed,"text",json_object_new_string("caller"));pg_document_storage storage;pg_record_memory_documents(&m,&storage);pg_document_loaded loaded={0};assert(!pg_document_loaded_open(&loaded,&storage,&definition,&address)&&loaded.record.id==id&&!strcmp(json_object_get_string(json_object_object_get(loaded.value,"text")),"initial")&&json_object_get_uint64(json_object_object_get(loaded.record_owner,"createdAt"))==1);assert(!pg_document_loaded_close(&loaded));json_object_put(seed);json_object_put(write);assert(!pg_record_memory_close(&m));
}
typedef struct {
 pg_document_install *plans;
 pg_document_cache_reservation *reservation;
 size_t count;
} install_fixture;
static int install_adopt(void *context,const struct json_object *batch,uint64_t seq){
 (void)batch;
 install_fixture *f=context;for(size_t i=0;i<f->count;++i)assert(!pg_document_install_apply(&f->plans[i],f->reservation,seq,123));return 0;
}
static void document_installation(void){
 _Static_assert(sizeof(pg_document_install)==32&&sizeof(pg_document_cache_reservation)==32,"install ABI");
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));
 pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};
 pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};
 assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
 pg_document_definition def={.kind={"new",3},.version=1,.scope=PG_DOC_SESSION,.family=1};
 pg_document_install plans[32]={0};pg_document_cache_reservation reservation={0};
 struct json_object *batch=json_object_new_array(),*value=json_tokener_parse("{\"text\":\"initial\"}");
 assert(batch&&value);
 for(unsigned i=0;i<32;++i){
  char key[16];int n=snprintf(key,sizeof(key),"key%u",i);
  pg_document_address address={.kind={"new",3},.key={key,(size_t)n},.scope=PG_DOC_SESSION,.has_key=1};
  struct json_object *record=0;assert(!pg_document_record_create(&def,&address,i+2,&record));
  assert(!pg_document_install_prepare(&cache,&def,record,value,&plans[i])&&!cache.count);
  struct json_object *write=json_object_new_object(),*content=json_object_new_object();assert(write&&content);
  assert(!json_object_object_add(write,"type",json_object_new_string("document.create"))&&!json_object_object_add(write,"record",record));
  assert(!json_object_object_add(content,"kind",json_object_new_string("base"))&&!json_object_object_add(content,"version",json_object_new_int(1))&&!json_object_object_add(content,"value",json_object_get(value))&&!json_object_object_add(write,"content",content)&&!json_object_array_add(batch,write));
 }
 assert(pg_document_cache_reserve(&cache,SIZE_MAX,&reservation)==PG_DOC_CAPACITY&&!reservation.cache&&!cache.busy);
 assert(!pg_document_cache_reserve(&cache,32,&reservation)&&reservation.limit==32);
 assert(pg_document_install_apply(&plans[0],&reservation,0,123)==PG_DOC_INVALID&&!cache.count);
 install_fixture f={plans,&reservation,32};pg_record_session session={.storage=pg_record_memory_storage(&m),.context=&f,.adopt=install_adopt};
 assert(!pg_records_commit(&session,batch)&&session.sequence==1&&cache.count==32&&reservation.expected_count==32);
 json_object_object_add(value,"text",json_object_new_string("caller"));
 for(unsigned i=0;i<32;++i){
  char key[16];int n=snprintf(key,sizeof(key),"key%u",i);
  pg_document_address address={.kind={"new",3},.key={key,(size_t)n},.scope=PG_DOC_SESSION,.has_key=1};pg_document_loaded *loaded=0;
  assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&loaded->record.id==i+2&&loaded->stored_version==1&&loaded->value_version==1&&!loaded->deltas_since_base&&json_object_get_int(json_object_object_get(loaded->record_owner,"createdAt"))==1&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"initial"));
  assert(pg_document_install_apply(&plans[i],&reservation,1,123)==PG_DOC_INVALID&&!pg_document_install_discard(&plans[i]));
 }
 assert(!host.finds&&!host.reads);
#ifdef PG_MEMORY_FAULTS
 doc_tracking=1;doc_calls=0;doc_fail=1;assert(pg_document_cache_reserve(&cache,1,&reservation)==PG_DOC_ALLOCATION);doc_tracking=0;doc_fail=0;assert(!reservation.cache&&!cache.busy&&cache.count==32);
#endif
 pg_document_address address={.kind={"new",3},.key={"key0",4},.scope=PG_DOC_SESSION,.has_key=1};struct json_object *record=0;
 assert(!pg_document_record_create(&def,&address,100,&record)&&!pg_document_install_prepare(&cache,&def,record,value,&plans[0])&&!pg_document_cache_reserve(&cache,1,&reservation));
 assert(pg_document_install_apply(&plans[0],&reservation,2,123)==PG_DOC_SEMANTICS&&cache.count==32&&!plans[0].busy&&!cache.busy&&!pg_document_install_discard(&plans[0]));
 assert(!json_object_object_add(record,"createdAt",json_object_new_int(1))&&pg_document_install_prepare(&cache,&def,record,value,&plans[0])==PG_DOC_INVALID&&!plans[0].node);
 json_object_object_del(record,"createdAt");json_object_object_add(record,"retiredAt",0);
 assert(pg_document_install_prepare(&cache,&def,record,value,&plans[0])==PG_DOC_INVALID&&!plans[0].node);json_object_object_del(record,"retiredAt");
#ifdef PG_MEMORY_FAULTS
 doc_tracking=1;doc_calls=doc_fail=0;assert(!pg_document_install_prepare(&cache,&def,record,value,&plans[0]));doc_tracking=0;unsigned total=doc_calls;assert(!pg_document_install_discard(&plans[0]));
 for(unsigned i=1;i<=total;++i){doc_tracking=1;doc_calls=0;doc_fail=i;int status=pg_document_install_prepare(&cache,&def,record,value,&plans[0]);doc_tracking=0;assert(status!=PG_DOC_OK&&!cache.busy&&!plans[0].node&&!plans[0].state&&!pg_document_install_discard(&plans[0]));}doc_fail=0;
#endif
 address.key=(pg_slice){"other",5};json_object_put(record);record=0;assert(!pg_document_record_create(&def,&address,100,&record));
 assert(!pg_document_install_prepare(&cache,&def,record,value,&plans[0])&&!pg_document_cache_reserve(&cache,1,&reservation));
#ifdef PG_MEMORY_FAULTS
 doc_tracking=1;doc_calls=0;doc_fail=1;assert(pg_document_install_apply(&plans[0],&reservation,2,123)==PG_DOC_ALLOCATION);doc_tracking=0;assert(doc_calls==1&&plans[0].node&&plans[0].state==1&&cache.count==32&&!cache.busy&&!plans[0].busy);doc_fail=0;
 doc_tracking=1;doc_calls=0;assert(!pg_document_install_apply(&plans[0],&reservation,2,123));doc_tracking=0;
 /* The one intercepted operation is insertion; all allocation/copy wrappers stay unused. */
 assert(doc_calls==1&&cache.count==33&&!pg_document_install_discard(&plans[0]));
 assert(!pg_document_install_prepare(&cache,&def,record,value,&plans[0])&&!pg_document_cache_reserve(&cache,1,&reservation));
 assert(!pg_document_cache_evict(&cache,&address,100)&&pg_document_install_apply(&plans[0],&reservation,3,123)==PG_DOC_INVALID);
 assert(!pg_document_cache_reserve(&cache,1,&reservation));
#endif
 assert(!pg_document_cache_close(&cache)&&pg_document_install_apply(&plans[0],&reservation,2,123)==PG_DOC_INVALID&&!pg_document_install_discard(&plans[0]));
 json_object_put(record);json_object_put(value);json_object_put(batch);assert(!pg_record_memory_close(&m));
}
typedef struct {pg_document_tx *scope;unsigned initial_calls,checkpoint_calls,fail;} managed_create_fixture;
static int managed_initial(void *context,struct json_object *seed,struct json_object **out){
 managed_create_fixture *f=context;++f->initial_calls;assert(f->scope->busy&&f->scope->records->busy);*out=json_object_get(seed);return f->fail?1:0;
}
static int new_checkpoint(void *context,const struct json_object *value,const struct json_object *ops,uint64_t count){
 managed_create_fixture *f=context;(void)value;(void)ops;(void)count;++f->checkpoint_calls;return 0;
}
static void managed_document_creation(void){
 _Static_assert(sizeof(pg_document_request)==56,"Doc request ABI");
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));
 pg_document_cache cache={0};cache_fixture cache_host={.memory=&m,.cache=&cache};pg_document_storage storage={&cache_host,cache_find,cache_read};pg_document_cache_hooks hooks={&cache_host,cache_clock,0,0};
 assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
 pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};
 managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"family",6},.version=1,.scope=PG_DOC_SESSION,.family=1};
 struct json_object *seed=json_tokener_parse("{\"text\":\"initial\"}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(seed&&ops);
 pg_document_address address={.kind={"family",6},.key={"k0",2},.scope=PG_DOC_SESSION,.has_key=1};
 pg_document_request request={&def,&address,seed,&initial,tx_mint,&f,new_checkpoint};pg_document_change *edit=0,*again=0;
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
 for(unsigned i=0;i<16;++i){char key[16];int n=snprintf(key,sizeof(key),"k%u",i);address.key=(pg_slice){key,(size_t)n};assert(!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_change_append(edit,ops)&&!pg_document_tx_doc(&scope,&request,&again)&&edit==again&&!cache.count&&!m.count);}
 assert(f.initial_calls==16&&!f.checkpoint_calls&&scope.count==16&&m.next_id==18);
 assert(!pg_document_tx_finish(&scope)&&scope.sealed&&!scope.entries&&cache.count==16&&m.count==16&&m.next_sequence==2&&!f.checkpoint_calls);
 address.key=(pg_slice){"k0",2};pg_document_loaded *loaded=0;unsigned finds=cache_host.finds;
 assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&cache_host.finds==finds&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"initial!")&&json_object_get_int(json_object_object_get(loaded->record_owner,"createdAt"))==1);
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_change_append(edit,ops));
 address.key=(pg_slice){"fresh",5};assert(!pg_document_tx_doc(&scope,&request,&again)&&!pg_document_tx_finish(&scope)&&m.next_sequence==3&&cache.count==17&&f.initial_calls==17&&f.checkpoint_calls==1);
 address.key=(pg_slice){"k0",2};assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"initial!!"));
 address.key=(pg_slice){"aborted",7};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_tx_abort(&scope)&&m.count==17&&cache.count==17&&m.next_sequence==3);
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));f.fail=1;uint64_t next=m.next_id;assert(pg_document_tx_doc(&scope,&request,&edit)==PG_DOC_INITIAL_FAILED&&!edit&&!scope.count&&m.next_id==next&&!tx.busy&&!scope.busy);f.fail=0;assert(!pg_document_tx_abort(&scope));
 session.read_only=1;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_doc(&scope,&request,&edit)&&pg_document_tx_finish(&scope)!=PG_TX_OK&&!session.failed&&m.count==17&&cache.count==17);session.read_only=0;
 pg_document_definition conv=def;conv.scope=PG_DOC_CONVERSATION;conv.history=PG_DOC_REWINDABLE;conv.fork=PG_DOC_AS_OF;address.scope=PG_DOC_CONVERSATION;address.owner=1;request.definition=&conv;
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));unsigned calls=f.initial_calls;next=m.next_id;assert(pg_document_tx_doc(&scope,&request,&edit)==PG_DOC_INVALID&&!edit&&f.initial_calls==calls&&m.next_id==next);
 struct json_object *table=json_tokener_parse("{\"type\":\"conversation\",\"value\":{\"id\":1}}");assert(table&&!pg_record_tx_stage(&tx,PG_TX_CONVERSATION,table)&&!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_tx_finish(&scope)&&m.count==19&&cache.count==18);json_object_put(table);
 pg_document_definition task=def;task.scope=PG_DOC_TASK;address.scope=PG_DOC_TASK;address.owner=100;request.definition=&task;
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));calls=f.initial_calls;assert(pg_document_tx_doc(&scope,&request,&edit)==PG_DOC_INVALID&&f.initial_calls==calls);
 table=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":100,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}");assert(table&&!pg_record_tx_stage(&tx,PG_TX_TASK,table)&&!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_tx_finish(&scope)&&cache.count==19);json_object_put(table);
 address.key=(pg_slice){"terminal",8};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));table=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":100,\"conversationId\":1,\"state\":{\"status\":\"terminal\"}}}");calls=f.initial_calls;assert(table&&!pg_record_tx_stage(&tx,PG_TX_TASK,table)&&pg_document_tx_doc(&scope,&request,&edit)==PG_DOC_INVALID&&f.initial_calls==calls&&!pg_document_tx_abort(&scope));json_object_put(table);
 json_object_put(seed);json_object_put(ops);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
}
typedef struct {pg_record_storage inner;unsigned late,published;} create_storage_fixture;
static int create_storage_failure(void *context,const struct json_object *writes,pg_records_adopt adopt,void *adopt_context,uint64_t *seq){
 create_storage_fixture *f=context;if(!f->late)return 7;int status=f->inner.commit(f->inner.context,writes,adopt,adopt_context,seq);assert(!status);return 7;
}
static void create_storage_publish(void *context,const struct json_object *writes,uint64_t seq){create_storage_fixture *f=context;(void)writes;(void)seq;++f->published;}
static void managed_creation_backend_failure(void){
 for(unsigned late=0;late<2;++late){
  pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  create_storage_fixture backend={pg_record_memory_storage(&m),late,0};pg_record_session session={.storage={&backend,create_storage_failure},.context=&backend,.publish=create_storage_publish};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
  managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"new",3},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_address address={.kind={"new",3},.key={"key",3},.scope=PG_DOC_SESSION,.has_key=1};struct json_object *seed=json_tokener_parse("{\"text\":\"initial\"}");assert(seed);pg_document_request request={&def,&address,seed,&initial,tx_mint,0,0};pg_document_change *edit=0;
  assert(!pg_document_tx_doc(&scope,&request,&edit)&&pg_document_tx_finish(&scope)!=PG_TX_OK&&session.failed&&!backend.published&&m.count==late&&cache.count==late&&scope.sealed&&!scope.entries&&session.context==&backend&&session.publish==create_storage_publish);
  if(late){pg_document_loaded *loaded=0;assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&loaded->record.id==2&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"initial"));}
  json_object_put(seed);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
#ifdef PG_MEMORY_FAULTS
static void managed_creation_faults(void){
 unsigned counts[2]={0};
 for(unsigned mode=0;mode<2;++mode)for(unsigned attempt=0;attempt<=counts[mode];++attempt){
  pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
  managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"new",3},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_address address={.kind={"new",3},.key={"key",3},.scope=PG_DOC_SESSION,.has_key=1};struct json_object *seed=json_tokener_parse("{\"text\":\"initial\"}");assert(seed);pg_document_request request={&def,&address,seed,&initial,tx_mint,0,0};pg_document_change *edit=0;
  if(mode)assert(!pg_document_tx_doc(&scope,&request,&edit));
  doc_tracking=1;doc_calls=0;doc_fail=attempt;int status=mode?pg_document_tx_finish(&scope):pg_document_tx_doc(&scope,&request,&edit);doc_tracking=0;
  if(!attempt){assert(!status);counts[mode]=doc_calls;}else{
   assert(status!=0&&!scope.busy&&!tx.busy);
   if(!mode)assert(!edit&&!scope.count&&!m.count&&!cache.count);
   /* Injected insertion failure is after Storage confirmation: durable fact remains, Session fails. */
   else if(m.count)assert(session.failed&&m.count==1&&cache.count==0);
   else assert(!cache.count&&m.next_sequence==1);
  }
  doc_fail=0;if(!mode)assert(!pg_document_tx_abort(&scope));else assert(scope.sealed&&!scope.entries);
  json_object_put(seed);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
#endif
static void managed_document_retirement(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"life\",\"key\":\"k\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));
 pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
 pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"life",4},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_address address={.kind={"life",4},.key={"k",1},.scope=PG_DOC_SESSION,.has_key=1};
 struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(seed&&ops);pg_document_request request={&def,&address,seed,&initial,tx_mint,&f,new_checkpoint};pg_document_change *old=0,*fresh=0,*again=0;
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_doc(&scope,&request,&old)&&!pg_document_change_append(old,ops));
 assert(!pg_document_tx_retire(&scope,&def,&address)&&!pg_document_tx_retire(&scope,&def,&address)&&scope.count==1&&!pg_document_tx_doc(&scope,&request,&fresh)&&fresh!=old&&!pg_document_change_append(fresh,ops)&&!pg_document_tx_doc(&scope,&request,&again)&&again==fresh&&scope.count==2);
 struct json_object *prior=0;assert(!pg_document_change_snapshot(old,&prior)&&!strcmp(json_object_get_string(json_object_object_get(prior,"text")),"old!"));json_object_put(prior);
 assert(!pg_document_tx_finish(&scope)&&m.next_sequence==3&&cache.count==1&&f.initial_calls==1&&f.checkpoint_calls==1);
 pg_document_loaded *loaded=0;unsigned finds=host.finds;assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&loaded->record.id==3&&host.finds==finds&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"new!")&&json_object_get_int(json_object_object_get(loaded->record_owner,"createdAt"))==2);
 struct json_object *record=json_object_object_get(m.rows[0].value,"record");assert(json_object_get_int(json_object_object_get(record,"id"))==2&&json_object_get_int(json_object_object_get(record,"retiredAt"))==2);
 /* Retire without acquiring: a newer token does not migrate/read the value. */
 assert(!pg_document_cache_evict(&cache,&address,3)&&!cache.count);pg_document_definition newer=def;newer.version=2;unsigned reads=host.reads;
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_retire(&scope,&newer,&address)&&host.reads==reads&&scope.count==1&&!pg_document_tx_finish(&scope)&&!cache.count&&m.next_sequence==4);
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_retire(&scope,&def,&address)&&!scope.count&&!pg_document_tx_finish(&scope)&&m.next_sequence==4);
 /* Retire a new incarnation in its creation commit: persist both, install none. */
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_doc(&scope,&request,&fresh)&&!pg_document_tx_retire(&scope,&def,&address)&&!pg_document_tx_finish(&scope)&&!cache.count&&m.next_sequence==5&&m.count==3);
 /* Repeated replacement keeps the latest draft and a single live address. */
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
 for(unsigned i=0;i<8;++i){assert(!pg_document_tx_doc(&scope,&request,&fresh));if(i<7)assert(!pg_document_tx_retire(&scope,&def,&address));}
 assert(!pg_document_tx_finish(&scope)&&cache.count==1&&m.count==11&&m.next_sequence==6&&!pg_document_cache_get(&cache,&def,&address,&loaded)&&loaded->record.id==12);
 session.read_only=1;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_retire(&scope,&def,&address)&&pg_document_tx_finish(&scope)!=PG_TX_OK&&cache.count==1&&!session.failed&&m.next_sequence==6);session.read_only=0;
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_retire(&scope,&def,&address)&&!pg_document_tx_abort(&scope)&&cache.count==1&&m.next_sequence==6);
 /* Clear an acquired incarnation without replacement, then recreate an absent address. */
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_doc(&scope,&request,&old)&&!pg_document_tx_retire(&scope,&def,&address)&&!pg_document_tx_finish(&scope)&&!cache.count);
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_retire(&scope,&def,&address)&&!pg_document_tx_doc(&scope,&request,&fresh)&&!pg_document_tx_finish(&scope)&&cache.count==1);
 json_object_put(seed);json_object_put(ops);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
}
#ifdef PG_MEMORY_FAULTS
static void managed_retirement_faults(void){
 unsigned counts[3]={0};
 for(unsigned mode=0;mode<3;++mode)for(unsigned attempt=0;attempt<=counts[mode];++attempt){
  pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"life\",\"key\":\"k\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));
  pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"life",4},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_address address={.kind={"life",4},.key={"k",1},.scope=PG_DOC_SESSION,.has_key=1};
  struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}");assert(seed);pg_document_request request={&def,&address,seed,&initial,tx_mint,0,0};pg_document_change *edit=0;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
  if(mode==1)assert(!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_tx_retire(&scope,&def,&address)&&!pg_document_tx_doc(&scope,&request,&edit));
  if(mode==2){pg_document_loaded *loaded=0;assert(!pg_document_cache_get(&cache,&def,&address,&loaded));}
  doc_tracking=1;doc_calls=0;doc_fail=attempt;int status=mode==1?pg_document_tx_finish(&scope):pg_document_tx_retire(&scope,&def,&address);doc_tracking=0;
  if(!attempt){assert(!status);counts[mode]=doc_calls;}else{
   assert(status!=0&&!scope.busy&&!tx.busy);
   if(mode!=1)assert(!scope.count&&m.count==1&&m.next_sequence==2);
   else if(m.next_sequence==3)assert(session.failed&&m.count==2&&!cache.count);
   else assert(cache.count==1&&m.count==1&&m.next_sequence==2);
  }
  doc_fail=0;if(mode!=1)assert(!pg_document_tx_abort(&scope));else assert(scope.sealed&&!scope.entries);
  json_object_put(seed);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
#endif
#ifdef PG_MEMORY_FAULTS
typedef struct {pg_records_adopt adopt;void *context;unsigned calls;} retirement_probe;
static void retirement_adopt_probe(void *context,uint64_t seq){
 retirement_probe *p=context;doc_tracking=1;doc_calls=0;p->adopt(p->context,seq);doc_tracking=0;p->calls=doc_calls;
}
static int retirement_probe_storage(void *context,const struct json_object *writes,pg_records_adopt adopt,void *adopt_context,uint64_t *seq){
 pg_record_storage *storage=context;retirement_probe probe={adopt,adopt_context,0};int status=storage->commit(storage->context,writes,retirement_adopt_probe,&probe,seq);assert(!status&&probe.calls==32);return status;
}
static void managed_retirement_many(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
 pg_record_storage provider=pg_record_memory_storage(&m);pg_record_session session={.storage={&provider,retirement_probe_storage}};pg_record_tx tx={0};pg_document_tx scope={0};managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"many",4},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_address address={.kind={"many",4},.scope=PG_DOC_SESSION,.has_key=1};
 struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}");assert(seed);pg_document_request request={&def,&address,seed,&initial,tx_mint,0,0};pg_document_change *edit=0;
 for(unsigned pass=0;pass<2;++pass){
  assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
  for(unsigned i=0;i<32;++i){char key[16];int n=snprintf(key,sizeof(key),"k%u",i);address.key=(pg_slice){key,(size_t)n};if(pass)assert(!pg_document_tx_retire(&scope,&def,&address));assert(!pg_document_tx_doc(&scope,&request,&edit));}
  assert(!pg_document_tx_finish(&scope)&&cache.count==32&&m.count==32*(pass+1)&&!session.failed);
 }
 json_object_put(seed);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
}
#endif
static void managed_retirement_backend_failure(void){
 for(unsigned late=0;late<2;++late){
  pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"life\",\"key\":\"k\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}]"));
  pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  create_storage_fixture backend={pg_record_memory_storage(&m),late,0};pg_record_session session={.storage={&backend,create_storage_failure},.context=&backend,.publish=create_storage_publish};pg_record_tx tx={0};pg_document_tx scope={0};managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"life",4},.version=1,.scope=PG_DOC_SESSION,.family=1};pg_document_address address={.kind={"life",4},.key={"k",1},.scope=PG_DOC_SESSION,.has_key=1};
  struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}");assert(seed);pg_document_request request={&def,&address,seed,&initial,tx_mint,0,0};pg_document_change *edit=0;pg_document_loaded *loaded=0;
  assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache)&&!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_tx_retire(&scope,&def,&address)&&!pg_document_tx_doc(&scope,&request,&edit)&&pg_document_tx_finish(&scope)!=PG_TX_OK&&session.failed&&!backend.published&&cache.count==1);
  assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&loaded->record.id==2+late&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),late?"new":"old"));
  json_object_put(seed);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
typedef struct {pg_record_memory *memory;pg_document_tx *scope;pg_document_change *edit;unsigned calls,mode,read_fail;} task_doc_scan_fixture;
static int task_doc_scan(void *context,const pg_document_query *query,pg_memory_page *page){
 task_doc_scan_fixture *f=context;++f->calls;assert(f->scope->busy&&f->scope->records->busy&&f->scope->records->sealed&&query->scope==PG_DOC_TASK&&query->at==UINT64_MAX&&query->limit==256);
 if(f->edit)assert(f->edit->sealed&&pg_document_tx_abort(f->scope)==PG_TX_INVALID);
 if(f->mode==1){page->items=json_object_new_array();assert(page->items);return 7;}
 if(f->mode==2){page->items=json_tokener_parse("[{\"id\":99999,\"kind\":\"wrong\",\"scope\":{\"kind\":\"session\"},\"createdAt\":1}]");assert(page->items);return 0;}
 if(f->mode==3){page->items=json_object_new_array();assert(page->items);page->has_next=1;page->after=1;return 0;}
 int status=pg_record_memory_scan_documents(f->memory,query,page);
 if(!status&&f->mode==4){page->has_next=1;page->after=0;}return status;
}
static int task_doc_read(void *context,unsigned table,uint64_t id,struct json_object **out){
 task_doc_scan_fixture *f=context;if(f->read_fail==2)return pg_record_memory_read(f->memory,(pg_record_table)table,2000,out);if(f->read_fail){*out=json_object_new_object();assert(*out);return 7;}return pg_record_memory_read(f->memory,(pg_record_table)table,id,out);
}
static void stage_task_state(pg_record_tx *tx,uint64_t id,uint64_t conversation,const char *status){
 char text[256];int n=snprintf(text,sizeof(text),"{\"type\":\"task\",\"value\":{\"id\":%llu,\"conversationId\":%llu,\"state\":{\"status\":\"%s\"}}}",(unsigned long long)id,(unsigned long long)conversation,status);assert(n>0&&(size_t)n<sizeof(text));struct json_object *value=json_tokener_parse(text);assert(value&&!pg_record_tx_stage(tx,PG_TX_TASK,value));json_object_put(value);
}
static void initialize_task_docs(pg_record_memory *m,unsigned count){
 assert(!pg_record_memory_open(m)&&!doc_commit(m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"task\",\"value\":{\"id\":1000,\"conversationId\":1,\"state\":{\"status\":\"running\"}}},{\"type\":\"task\",\"value\":{\"id\":2000,\"conversationId\":1,\"state\":{\"status\":\"running\"}}},{\"type\":\"task\",\"value\":{\"id\":3000,\"conversationId\":1,\"state\":{\"status\":\"running\"}}}]"));
 struct json_object *writes=json_object_new_array();assert(writes);
 for(unsigned i=0;i<count;++i){unsigned owner=i<520?1000:i<525?2000:3000;char text[512];int n=snprintf(text,sizeof(text),"{\"type\":\"document.create\",\"record\":{\"id\":%u,\"kind\":\"progress\",\"key\":\"k%u\",\"scope\":{\"kind\":\"task\",\"taskId\":%u}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"old\"}}}",i+10,i,owner);assert(n>0&&(size_t)n<sizeof(text));struct json_object *write=json_tokener_parse(text);assert(write&&!json_object_array_add(writes,write));}
 pg_record_storage provider=pg_record_memory_storage(m);uint64_t seq;assert(!provider.commit(m,writes,doc_adopt,0,&seq));json_object_put(writes);
}
static void terminal_task_documents(void){
 _Static_assert(sizeof(pg_document_query)==64&&sizeof(pg_document_scan)==16,"Doc lifecycle query ABI");
 pg_record_memory m={0};initialize_task_docs(&m,530);pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
 pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};task_doc_scan_fixture scanner={.memory=&m,.scope=&scope};pg_document_scan scan={&scanner,task_doc_scan};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
 managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"progress",8},.version=1,.scope=PG_DOC_TASK,.family=1};pg_document_address address={.kind={"progress",8},.key={"k0",2},.owner=1000,.scope=PG_DOC_TASK,.has_key=1};struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(seed&&ops);pg_document_request request={&def,&address,seed,&initial,tx_mint,&f,new_checkpoint};pg_document_change *edit=0;assert(!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_change_append(edit,ops));scanner.edit=edit;
 address.key=(pg_slice){"k1",2};pg_document_loaded *loaded=0;assert(!pg_document_cache_get(&cache,&def,&address,&loaded));
 address.key=(pg_slice){"k2",2};assert(!pg_document_tx_retire(&scope,&def,&address));
 address.key=(pg_slice){"fresh",5};assert(!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_change_append(edit,ops));
 address.key=(pg_slice){"k525",4};address.owner=3000;assert(!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_change_append(edit,ops));
 stage_task_state(&tx,1000,1,"terminal");stage_task_state(&tx,2000,1,"terminal");
 assert(!pg_document_tx_settle(&scope,&scan)&&scanner.calls==4&&f.initial_calls==1&&f.checkpoint_calls==2&&!session.failed&&cache.count==1&&m.next_sequence==4);
 pg_document_query query={.scope=PG_DOC_TASK,.owner=1000,.at=UINT64_MAX,.limit=1000};pg_memory_page page={0};assert(!pg_record_memory_scan_documents(&m,&query,&page)&&json_object_array_length(page.items)==0);json_object_put(page.items);
 query.owner=2000;assert(!pg_record_memory_scan_documents(&m,&query,&page)&&json_object_array_length(page.items)==0);json_object_put(page.items);query.owner=3000;assert(!pg_record_memory_scan_documents(&m,&query,&page)&&json_object_array_length(page.items)==5);json_object_put(page.items);
 assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"old!"));
 unsigned retired=0;for(size_t i=0;i<m.count;++i)if(m.rows[i].table==PG_TABLE_DOCUMENT){struct json_object *record=json_object_object_get(m.rows[i].value,"record");if(json_object_object_get(record,"retiredAt")){++retired;assert(json_object_get_int(json_object_object_get(record,"retiredAt"))==3);}}assert(retired==526);
 scanner.edit=0;assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));stage_task_state(&tx,1000,1,"terminal");assert(pg_document_tx_settle(&scope,&scan)!=PG_TX_OK&&!session.failed&&m.next_sequence==4&&scanner.calls==4);
 json_object_put(seed);json_object_put(ops);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
}
static void terminal_task_rejections(void){
 for(unsigned mode=0;mode<9;++mode){
  pg_record_memory m={0};initialize_task_docs(&m,3);pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};task_doc_scan_fixture scanner={.memory=&m,.scope=&scope,.mode=mode<5?mode:0,.read_fail=mode==6?1:mode==7?2:0};pg_document_scan scan={&scanner,task_doc_scan};session.read_only=mode==8;
  assert(!pg_record_tx_begin(&tx,&session,&scanner,task_doc_read)&&!pg_document_tx_open(&scope,&tx,&cache));stage_task_state(&tx,1000,mode==5?2:1,"terminal");
  assert((mode?pg_document_tx_settle(&scope,&scan):pg_document_tx_finish(&scope))!=PG_TX_OK&&!session.failed&&m.next_sequence==3&&scope.sealed&&!scope.entries);
  struct json_object *task=0;assert(!pg_record_memory_read(&m,PG_TABLE_TASK,1000,&task)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(task,"state"),"status")),"running"));json_object_put(task);
  pg_document_query q={.scope=PG_DOC_TASK,.owner=1000,.at=UINT64_MAX,.limit=10};pg_memory_page page={0};assert(!pg_record_memory_scan_documents(&m,&q,&page)&&json_object_array_length(page.items)==3);json_object_put(page.items);
  assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
static void new_terminal_task_documents(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
 pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));stage_task_state(&tx,100,1,"pending");
 managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"progress",8},.version=1,.scope=PG_DOC_TASK,.family=1};pg_document_address address={.kind={"progress",8},.key={"new",3},.owner=100,.scope=PG_DOC_TASK,.has_key=1};struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}");assert(seed);pg_document_request request={&def,&address,seed,&initial,tx_mint,0,0};pg_document_change *edit=0;assert(!pg_document_tx_doc(&scope,&request,&edit));stage_task_state(&tx,100,1,"terminal");assert(!pg_document_tx_finish(&scope)&&!cache.count&&m.count==2&&m.next_sequence==2);
 struct json_object *stored=0;assert(pg_record_memory_document(&m,2,UINT64_MAX,&stored)==PG_MEMORY_MISSING&&!stored);
 json_object_put(seed);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
}
#ifdef PG_MEMORY_FAULTS
static void terminal_task_faults(void){
 unsigned counts[2]={0};for(unsigned mode=0;mode<2;++mode)for(unsigned attempt=0;attempt<=counts[mode];++attempt){
  pg_record_memory m={0};initialize_task_docs(&m,8);pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};task_doc_scan_fixture scanner={.memory=&m,.scope=&scope};pg_document_scan scan={&scanner,task_doc_scan};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));
  managed_create_fixture f={.scope=&scope};pg_document_initializer initial={&f,managed_initial};pg_document_definition def={.kind={"progress",8},.version=1,.scope=PG_DOC_TASK,.family=1};pg_document_address address={.kind={"progress",8},.key={"k0",2},.owner=1000,.scope=PG_DOC_TASK,.has_key=1};struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(seed&&ops);
  if(mode){pg_document_request request={&def,&address,seed,&initial,tx_mint,0,0};pg_document_change *edit=0;assert(!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_change_append(edit,ops));scanner.edit=edit;address.key=(pg_slice){"new",3};assert(!pg_document_tx_doc(&scope,&request,&edit));}
  stage_task_state(&tx,1000,1,"terminal");
  doc_tracking=1;doc_calls=0;doc_fail=attempt;int status=pg_document_tx_settle(&scope,&scan);doc_tracking=0;
  if(!attempt){assert(!status);counts[mode]=doc_calls;}else{assert(status!=PG_TX_OK&&m.next_sequence==3&&!scope.entries&&scope.sealed&&cache.count==mode);if(mode){address.key=(pg_slice){"k0",2};pg_document_loaded *loaded=0;assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"old"));}}
  doc_fail=0;json_object_put(seed);json_object_put(ops);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
#endif
static void fork_document_setup(pg_record_memory *m){
 assert(!pg_record_memory_open(m)&&!doc_commit(m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":20,\"conversationId\":1,\"kind\":\"point\"}},{\"type\":\"document.create\",\"record\":{\"id\":10,\"kind\":\"past\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"at\"}}},{\"type\":\"document.create\",\"record\":{\"id\":30,\"kind\":\"now\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"latest\",\"fork\":\"current\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"before\"}}}]")&&!doc_commit(m,"[{\"type\":\"document.change\",\"id\":10,\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"after\"}}},{\"type\":\"document.change\",\"id\":30,\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"current\"}}}]"));
}
static uint64_t fork_document_child(pg_record_tx *tx,pg_record_memory *m){
 pg_fork_storage service={m,fork_entry,fork_scan,tx_mint};struct json_object *child=0;assert(!pg_record_tx_fork_conversation(tx,1,20,0,&service,&child));uint64_t id=json_object_get_uint64(json_object_object_get(child,"id"));json_object_put(child);assert(id==31);return id;
}
typedef struct {unsigned writes,creates,copies,retires;} fork_doc_publication;
static void fork_doc_published(void *context,const struct json_object *writes,uint64_t seq){
 fork_doc_publication *p=context;assert(seq==3);p->writes=(unsigned)json_object_array_length(writes);for(size_t i=0;i<p->writes;++i){const char *type=json_object_get_string(json_object_object_get(json_object_array_get_idx(writes,i),"type"));p->creates+=!strcmp(type,"document.create");p->copies+=!strcmp(type,"document.copy");p->retires+=!strcmp(type,"document.retire");}
}
static int fork_doc_migrate(void *context,struct json_object *value,uint64_t from,struct json_object **out){
 unsigned *calls=context;++*calls;assert(from==1&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"at"));assert(!json_object_object_add(value,"text",json_object_new_string("migrated")));*out=json_object_get(value);return 0;
}
static void fork_document_acquisition(void){
 for(unsigned migration=0;migration<2;++migration){
  pg_record_memory m={0};fork_document_setup(&m);pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  fork_doc_publication publication={0};pg_record_session session={.storage=pg_record_memory_storage(&m),.context=&publication,.publish=fork_doc_published};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));uint64_t child=fork_document_child(&tx,&m),next=m.next_id;unsigned migrated=0;managed_create_fixture checkpoint={.scope=&scope};
  pg_document_definition past={.kind={"past",4},.version=migration?2:1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_REWINDABLE,.fork=PG_DOC_AS_OF,.context=&migrated,.migrate=migration?fork_doc_migrate:0},now={.kind={"now",3},.version=1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_LATEST,.fork=PG_DOC_CURRENT};
  pg_document_address address={.kind={"past",4},.owner=(int64_t)child,.scope=PG_DOC_CONVERSATION};pg_document_request request={.definition=&past,.address=&address,.context=&checkpoint,.checkpoint=new_checkpoint};pg_document_change *edit=0,*again=0;assert(!pg_document_tx_doc(&scope,&request,&edit)&&!pg_document_tx_doc(&scope,&request,&again)&&edit==again&&host.reads==1&&!host.finds&&m.next_id==next&&migrated==migration);
  struct json_object *snapshot=0,*ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(ops&&!pg_document_change_snapshot(edit,&snapshot)&&!strcmp(json_object_get_string(json_object_object_get(snapshot,"text")),migration?"migrated":"at"));json_object_put(snapshot);assert(!pg_document_change_append(edit,ops));
  address.kind=(pg_slice){"now",3};request.definition=&now;assert(!pg_document_tx_doc(&scope,&request,&edit)&&host.reads==2&&!host.finds&&!pg_document_change_append(edit,ops)&&!pg_document_tx_finish(&scope)&&!session.failed&&m.count==7&&cache.count==2&&publication.writes==3&&publication.creates==2&&!publication.copies&&!publication.retires&&!checkpoint.checkpoint_calls);
  pg_document_loaded *loaded=0;address.kind=(pg_slice){"past",4};assert(!pg_document_cache_get(&cache,&past,&address,&loaded)&&loaded->record.id==32&&loaded->stored_version==(migration?2u:1u)&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),migration?"migrated!":"at!")&&host.reads==2&&!host.finds);
  struct json_object *source=0;assert(!pg_record_memory_document(&m,10,UINT64_MAX,&source)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(source,"value"),"text")),"after"));json_object_put(source);json_object_put(ops);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
static int fork_replacement_initial(void *context,struct json_object *seed,struct json_object **out){assert(!seed);*out=json_object_get(context);return 0;}
static void fork_document_retirement(void){
 for(unsigned mode=0;mode<4;++mode){unsigned acquired=mode&1,recreate=mode>>1;
  pg_record_memory m={0};fork_document_setup(&m);pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  fork_doc_publication publication={0};pg_record_session session={.storage=pg_record_memory_storage(&m),.context=&publication,.publish=fork_doc_published};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));uint64_t child=fork_document_child(&tx,&m);
  pg_document_definition def={.kind={"past",4},.version=1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_REWINDABLE,.fork=PG_DOC_AS_OF};pg_document_address address={.kind={"past",4},.owner=(int64_t)child,.scope=PG_DOC_CONVERSATION};pg_document_request request={.definition=&def,.address=&address};pg_document_change *edit=0,*old=0;if(acquired)assert(!pg_document_tx_doc(&scope,&request,&old));pg_document_definition retire_def=def;retire_def.version=2;assert(!pg_document_tx_retire(&scope,&retire_def,&address)&&host.reads==acquired&&!host.finds);
  struct json_object *seed=json_tokener_parse("{\"text\":\"replacement\"}");assert(seed);pg_document_initializer initial={seed,fork_replacement_initial};request.initializer=&initial;request.mint=tx_mint;
  if(recreate)assert(!pg_document_tx_doc(&scope,&request,&edit)&&edit!=old&&host.reads==acquired);
  assert(!pg_document_tx_finish(&scope)&&cache.count==recreate&&publication.writes==4+recreate&&publication.retires==1&&publication.creates==acquired+recreate&&publication.copies==2-acquired);
  struct json_object *found=0;if(!recreate)assert(pg_record_memory_find_document(&m,&address,UINT64_MAX,&found)==PG_MEMORY_MISSING&&!found);else{pg_document_loaded *loaded=0;assert(!pg_document_cache_get(&cache,&def,&address,&loaded)&&loaded->record.id==34&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"replacement"));}
  json_object_put(seed);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
static void fork_document_source_conflict(void){
 for(unsigned kind=0;kind<2;++kind)for(unsigned order=0;order<2;++order){
  pg_record_memory m={0};fork_document_setup(&m);pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));uint64_t child=fork_document_child(&tx,&m);managed_create_fixture f={.scope=&scope};
  pg_document_definition def={.kind=kind?(pg_slice){"now",3}:(pg_slice){"past",4},.version=1,.scope=PG_DOC_CONVERSATION,.history=kind?PG_DOC_LATEST:PG_DOC_REWINDABLE,.fork=kind?PG_DOC_CURRENT:PG_DOC_AS_OF};
  pg_document_address address={.kind=def.kind,.scope=PG_DOC_CONVERSATION};pg_document_request request={.definition=&def,.address=&address,.context=&f,.checkpoint=new_checkpoint};pg_document_change *edit=0;struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"!\"]]");assert(ops);
  for(unsigned i=0;i<2;++i){address.owner=(i==order)?1:(int64_t)child;assert(!pg_document_tx_doc(&scope,&request,&edit));if(i==order)assert(!pg_document_change_append(edit,ops));}
  assert(pg_document_tx_finish(&scope)==PG_TX_INVALID&&!session.failed&&m.next_sequence==3&&m.count==4&&cache.count==1&&!f.checkpoint_calls);
  struct json_object *conversation=0;assert(pg_record_memory_read(&m,PG_TABLE_CONVERSATION,child,&conversation)==PG_MEMORY_MISSING&&!conversation);json_object_put(ops);assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
#ifdef PG_MEMORY_FAULTS
static void fork_document_faults(void){
 unsigned counts[3]={0};for(unsigned mode=0;mode<3;++mode)for(unsigned attempt=0;attempt<=counts[mode];++attempt){
  pg_record_memory m={0};fork_document_setup(&m);pg_document_cache cache={0};cache_fixture host={.memory=&m,.cache=&cache};pg_document_storage storage={&host,cache_find,cache_read};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));uint64_t child=fork_document_child(&tx,&m);
  pg_document_definition past={.kind={"past",4},.version=1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_REWINDABLE,.fork=PG_DOC_AS_OF},now={.kind={"now",3},.version=1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_LATEST,.fork=PG_DOC_CURRENT};pg_document_address address={.kind=past.kind,.owner=(int64_t)child,.scope=PG_DOC_CONVERSATION};pg_document_request request={.definition=&past,.address=&address};pg_document_change *edit=0;
  if(mode==2){assert(!pg_document_tx_doc(&scope,&request,&edit));address.kind=now.kind;request.definition=&now;assert(!pg_document_tx_doc(&scope,&request,&edit));}
  doc_tracking=1;doc_calls=0;doc_fail=attempt;int status=mode==2?pg_document_tx_finish(&scope):mode==1?pg_document_tx_retire(&scope,&past,&address):pg_document_tx_doc(&scope,&request,&edit);doc_tracking=0;
  if(!attempt){assert(!status);counts[mode]=doc_calls;}else{
   assert(status!=0&&!tx.busy&&!scope.busy);
   if(mode<2)assert(!scope.count&&!cache.count&&m.count==4&&m.next_sequence==3&&!session.failed);
   else if(m.next_sequence==4)assert(session.failed&&m.count==7&&cache.count<2);
   else assert(!cache.count&&m.count==4&&m.next_sequence==3);
  }
  doc_fail=0;if(mode<2)assert(!pg_document_tx_abort(&scope));else assert(scope.sealed&&!scope.entries);
  assert(!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
#endif

typedef struct {cache_fixture base;unsigned mode;} copy_read_fixture;
static int copy_read_variant(void *context,uint64_t id,uint64_t at,struct json_object **out){
 copy_read_fixture *f=context;++f->base.reads;int status=pg_record_memory_document(f->base.memory,id,at,out);if(status)return status;
 struct json_object *record=json_object_object_get(*out,"record");switch(f->mode){
 case 0:return 1;
 case 1:json_object_object_add(record,"kind",json_object_new_string("wrong"));break;
 case 2:json_object_object_add(record,"id",json_object_new_int(30));break;
 case 3:json_object_object_add(record,"fork",json_object_new_string("initial"));break;
 case 4:json_object_object_add(*out,"version",json_object_new_int(2));break;
 case 5:json_object_object_add(*out,"version",json_object_new_double(1.5));break;
 case 6:return 7;
 case 10:{struct json_object *scope=json_tokener_parse("{\"kind\":\"task\",\"taskId\":1000}");assert(scope);json_object_object_add(record,"scope",scope);break;}
 default:break;
 }return 0;
}
static int copy_forbidden_initial(void *context,struct json_object *seed,struct json_object **out){(void)context;(void)seed;(void)out;assert(0);return 1;}
static void fork_document_rejections(void){
 for(unsigned mode=0;mode<13;++mode){
  pg_record_memory m={0};fork_document_setup(&m);pg_document_cache cache={0};copy_read_fixture host={.base={.memory=&m,.cache=&cache},.mode=mode};pg_document_storage storage={&host,cache_find,copy_read_variant};pg_document_cache_hooks hooks={&host,cache_clock,0,0};assert(!pg_document_cache_open(&cache,&storage,&hooks,1000));
  pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_document_tx scope={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_document_tx_open(&scope,&tx,&cache));uint64_t child=fork_document_child(&tx,&m);
  pg_document_definition def={.kind={"past",4},.version=mode==12?2:1,.scope=PG_DOC_CONVERSATION,.history=mode==11?PG_DOC_LATEST:PG_DOC_REWINDABLE,.fork=mode==11?PG_DOC_CURRENT:PG_DOC_AS_OF};pg_document_address address={.kind=def.kind,.owner=(int64_t)child,.scope=PG_DOC_CONVERSATION};pg_document_initializer initial={0,copy_forbidden_initial};pg_document_request request={.definition=&def,.address=&address,.initializer=&initial,.mint=tx_mint};pg_document_change *edit=0;
  struct json_object *source=json_object_object_get(json_object_array_get_idx(tx.writes,1),"source");if(mode==7)json_object_object_add(source,"at",json_object_new_string_len("current\0tail",12));if(mode==8)json_object_object_add(source,"id",json_object_new_double(10.5));if(mode==9)json_object_object_add(source,"at",json_object_new_double(1.5));
  int status=pg_document_tx_doc(&scope,&request,&edit);assert(status!=PG_DOC_OK&&!edit&&!scope.count&&!cache.count&&!tx.busy&&!scope.busy&&!session.failed&&m.next_id==34&&json_object_array_length(tx.writes)==3);if(mode==4)assert(status==PG_DOC_NEWER_VERSION);if(mode==12)assert(status==PG_DOC_MIGRATION_REQUIRED);
  assert(!pg_document_tx_abort(&scope)&&!pg_document_cache_close(&cache)&&!pg_record_memory_close(&m));
 }
}
int main(void){
 fork_document_acquisition();
 fork_document_rejections();
 fork_document_retirement();
 fork_document_source_conflict();
#ifdef PG_MEMORY_FAULTS
 fork_document_faults();
#endif
 terminal_task_documents();
 terminal_task_rejections();
 new_terminal_task_documents();
#ifdef PG_MEMORY_FAULTS
 terminal_task_faults();
#endif
 managed_document_retirement();
 managed_retirement_backend_failure();
#ifdef PG_MEMORY_FAULTS
 managed_retirement_many();
#endif
#ifdef PG_MEMORY_FAULTS
 managed_retirement_faults();
#endif
 managed_document_creation();
 managed_creation_backend_failure();
#ifdef PG_MEMORY_FAULTS
 managed_creation_faults();
#endif
 document_installation();
 document_creation_factory();
 managed_document_access();
#ifdef PG_MEMORY_FAULTS
 document_settlement_faults();
#endif
 unified_document_settlement();
 document_adoption_commit();
 cache_many_addresses();
 document_cache();
 loaded_documents();
 document_edit_commit();
 final_owners();
 conversation_hooks();
 transaction_queries();
 fork_plans();
 typed_conversations();
 typed_entries();
#ifdef PG_MEMORY_FAULTS
 transaction_faults();
#endif
 transaction_batches();
 document_copies();
 document_writes();
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
