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
static unsigned allocation_failure,array_add_calls,doc_tracking,doc_calls,doc_fail;
static int doc_fault(void){return doc_tracking&&++doc_calls==doc_fail;}
int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
void *__real_calloc(size_t,size_t);
void *__real_reallocarray(void *,size_t,size_t);
struct json_object *__real_json_object_new_array_ext(int);
int __real_json_object_array_add(struct json_object *,struct json_object *);
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
int main(void){
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
