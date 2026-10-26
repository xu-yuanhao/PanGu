#define _GNU_SOURCE
#include "../../src/runtime/record_memory.h"
#include "../../src/runtime/delta.h"
#include "../../src/runtime/document_tx.h"
#include "../../src/runtime/holds.h"
#include "../../src/runtime/hold_timer.h"
#include "../../src/runtime/durable_session.h"
#include "../../src/runtime/document_reader.h"
#include <assert.h>
#include <math.h>
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
struct json_object *__real_json_object_new_boolean(json_bool);
struct json_object *__wrap_json_object_new_boolean(json_bool n){return doc_fault()?0:__real_json_object_new_boolean(n);}
struct json_object *__real_json_object_new_double(double);
struct json_object *__wrap_json_object_new_double(double n){return doc_fault()?0:__real_json_object_new_double(n);}
struct json_object *__real_json_object_new_string(const char *);
struct json_object *__real_json_object_new_string_len(const char *,int);
const char *__real_json_object_to_json_string_ext(struct json_object *,int);
struct json_object *__wrap_json_object_new_string_len(const char *s,int n){return doc_fault()?0:__real_json_object_new_string_len(s,n);}
const char *__wrap_json_object_to_json_string_ext(struct json_object *o,int flags){return doc_fault()?0:__real_json_object_to_json_string_ext(o,flags);}
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




typedef struct {unsigned calls,mode;pg_record_tx *tx;} task_cache_provider;
static int task_cache_read(void *context,unsigned table,uint64_t id,struct json_object **out){task_cache_provider *f=context;++f->calls;assert(table==PG_TABLE_TASK&&f->tx->busy&&pg_record_tx_discard(f->tx)==PG_TX_INVALID);if(id==99)return 1;
#ifdef PG_MEMORY_FAULTS
 unsigned tracking=doc_tracking;doc_tracking=0;
#endif
 *out=json_object_new_object();assert(*out&&!json_object_object_add(*out,"id",json_object_new_int64((int64_t)id))&&!json_object_object_add(*out,"n",json_object_new_int64(f->calls)));
#ifdef PG_MEMORY_FAULTS
 doc_tracking=tracking;
#endif
 return f->mode?7:0;}
typedef struct {pg_record_memory *memory;unsigned tasks;} task_rebuild_provider;
static int task_rebuild_read(void *context,unsigned table,uint64_t id,struct json_object **out){task_rebuild_provider *p=context;if(table==PG_TABLE_TASK)++p->tasks;return pg_record_memory_read(p->memory,(pg_record_table)table,id,out);}

typedef struct {pg_record_tx *tx;unsigned calls;double value;} task_clock_fixture;
static double task_clock(void *context){task_clock_fixture *f=context;assert(f->tx->busy&&pg_record_tx_discard(f->tx)==PG_TX_INVALID);++f->calls;return f->value;}
static struct json_object *task_draft(const char *status,unsigned conversation){char text[200];int n=snprintf(text,sizeof(text),"{\"id\":10,\"conversationId\":%u,\"state\":{\"status\":\"%s\"}}",conversation,status);assert(n>0);struct json_object *out=json_tokener_parse(text);assert(out);return out;}

typedef struct {pg_record_tx *tx;pg_record_memory *memory;unsigned initial,mint,inject,fail;} task_create_fixture;
static int task_initial(void *ctx,const struct json_object *input,struct json_object **out){task_create_fixture *f=ctx;assert(f->tx->busy);++f->initial;struct json_object *probe=0;assert(pg_record_tx_read(f->tx,0,1,&probe)==PG_TX_INVALID&&!probe);*out=json_object_new_object();if(!*out)return 1;struct json_object *phase=json_object_new_string("start"),*copy=0;if(!phase)return 1;if(json_object_object_add(*out,"phase",phase)){json_object_put(phase);return 1;}if(input&&json_object_deep_copy((struct json_object*)input,&copy,0)){json_object_put(copy);return 1;}if(json_object_object_add(*out,"input",copy)){json_object_put(copy);return 1;}return f->fail==1;}
static int task_mint(void *ctx,uint64_t *out){task_create_fixture *f=ctx;assert(f->tx->busy&&f->initial);++f->mint;if(f->fail==2)return 1;return pg_record_memory_mint(f->memory,out);}
static int task_inject(void *ctx,struct json_object **out){task_create_fixture *f=ctx;assert(f->tx->busy&&f->mint);++f->inject;*out=json_object_new_object();if(!*out)return 1;struct json_object *v=json_object_new_string("context");if(!v)return 1;if(json_object_object_add(*out,"parent",v)){json_object_put(v);return 1;}return f->fail==3;}
static void task_create_tests(void){
 _Static_assert(sizeof(pg_task_creation)==40&&offsetof(pg_task_creation,conversation_id)==32,"task creation ABI");
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));
 for(unsigned fail=0;fail<4;++fail){pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));task_create_fixture f={.tx=&tx,.memory=&m,.fail=fail};pg_task_creation services={&f,task_initial,task_mint,task_inject,1};struct json_object *definition=json_tokener_parse("{\"name\":\"work\",\"version\":2}"),*input=json_tokener_parse("{\"path\":\"a\"}"),*options=json_tokener_parse("{\"ownership\":{\"kind\":\"conversation\"},\"background\":true,\"abandonOnRestart\":true}"),*tasks=0;uint64_t id=999,before=m.next_id;
 int status=pg_record_tx_create_task(&tx,definition,input,options,&services,&id);assert(!tx.busy&&tx.has_table_write&&!session.failed);if(fail){assert(status!=PG_TX_OK&&!id&&!json_object_array_length(tx.writes));assert(m.next_id==before+(fail==3));assert(!pg_record_tx_discard(&tx));}else{assert(!status&&id==before&&f.initial==1&&f.mint==1&&f.inject==1);assert(!json_object_object_add(input,"outside",json_object_new_boolean(1)));assert(!pg_record_tx_staged_tasks(&tx,&tasks));struct json_object *record=json_object_array_get_idx(tasks,0);assert(!json_object_object_get(json_object_object_get(record,"input"),"outside")&&json_object_get_boolean(json_object_object_get(record,"background"))&&json_object_get_boolean(json_object_object_get(record,"abandonOnRestart")));json_object_put(tasks);tasks=0;assert(!pg_record_tx_commit(&tx)&&!pg_record_memory_read(&m,PG_TABLE_TASK,id,&tasks));assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(tasks,"state"),"status")),"pending"));json_object_put(tasks);}
 json_object_put(definition);json_object_put(input);json_object_put(options);}
 assert(!pg_record_memory_close(&m));
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));task_create_fixture f={.tx=&tx,.memory=&m};pg_task_creation services={&f,task_initial,task_mint,task_inject,1};struct json_object *definition=json_tokener_parse("{\"name\":\"work\",\"version\":2}"),*input=json_tokener_parse("{\"path\":\"a\"}"),*options=json_tokener_parse("{\"ownership\":{\"kind\":\"conversation\"},\"abandonOnRestart\":true}"),*prior=json_tokener_parse("{\"type\":\"submission\",\"value\":{\"id\":20,\"conversationId\":1}}"),*before=0;assert(!pg_record_tx_stage(&tx,PG_TX_SUBMISSION,prior)&&!json_object_deep_copy(tx.writes,&before,0));json_object_put(prior);uint64_t id=0;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_record_tx_create_task(&tx,definition,input,options,&services,&id);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!tx.busy&&tx.has_table_write&&!session.failed);if(target){assert(status!=PG_TX_OK&&!id&&json_object_equal(before,tx.writes));++failures;}else{assert(!status&&id&&json_object_array_length(tx.writes)==2);total=calls;}json_object_put(before);json_object_put(definition);json_object_put(input);json_object_put(options);assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&m));}
 printf("PASS: full task creation, initializer/mint/inject failure order and %u allocation/insert/clone failures preserve prior candidates\n",failures);
#endif
}
static void task_family_tests(void){
 for(unsigned terminal=0;terminal<2;++terminal){pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));task_create_fixture f={.tx=&tx,.memory=&m};pg_task_creation services={&f,task_initial,task_mint,0,1};struct json_object *definition=json_tokener_parse("{\"name\":\"work\",\"version\":1}"),*options=json_tokener_parse("{\"ownership\":{\"kind\":\"conversation\"}}"),*tasks=0;uint64_t owner=0,child=0;assert(!pg_record_tx_create_task(&tx,definition,0,options,&services,&owner));json_object_put(options);options=json_tokener_parse("{\"ownership\":{\"kind\":\"task\"},\"abandonOnRestart\":true}");assert(!json_object_object_add(json_object_object_get(options,"ownership"),"taskId",json_object_new_int64((int64_t)owner)));assert(!pg_record_tx_create_task(&tx,definition,0,options,&services,&child)&&child>owner&&!f.inject&&!pg_record_tx_staged_tasks(&tx,&tasks)&&json_object_array_length(tasks)==2);struct json_object *value=json_object_array_get_idx(tasks,1);assert(json_object_get_uint64(json_object_object_get(value,"owner"))==owner&&!json_object_object_get(value,"trace")&&!json_object_object_get(value,"links")&&!json_object_get_boolean(json_object_object_get(value,"background")));
 if(terminal){struct json_object *write=json_tokener_parse("{\"type\":\"task\",\"value\":{\"conversationId\":1,\"state\":{\"status\":\"terminal\"}}}");assert(!json_object_object_add(json_object_object_get(write,"value"),"id",json_object_new_int64((int64_t)owner))&&!pg_record_tx_stage(&tx,PG_TX_TASK,write));json_object_put(write);assert(pg_record_tx_commit(&tx)!=PG_TX_OK&&!session.failed&&m.count==1);}else{assert(!pg_record_tx_commit(&tx)&&m.count==3);struct json_object *saved=0;assert(!pg_record_memory_read(&m,PG_TABLE_TASK,child,&saved)&&json_object_equal(value,saved));json_object_put(saved);}
 json_object_put(tasks);json_object_put(options);json_object_put(definition);assert(!pg_record_memory_close(&m));}
}
typedef struct {pg_durable_storage *guard;unsigned mode,calls,cancel_checks,error_checks;} guard_fixture;
static int guard_error(void *ctx,int status){guard_fixture *f=ctx;++f->error_checks;return status==8;}
static int guard_cancelled(void *ctx){guard_fixture *f=ctx;++f->cancel_checks;return f->mode==4;}
static int guard_read(void *ctx,unsigned table,uint64_t id,struct json_object **out){guard_fixture *f=ctx;++f->calls;assert(table==2&&id==10&&f->guard->busy&&f->guard->session->busy);*out=json_object_new_object();assert(*out);if(f->mode==5)f->guard->session->records->failed=PG_RECORDS_ADOPTION_FAILED;if(f->mode==6){struct json_object *nested=0;assert(pg_durable_storage_read(f->guard,table,id,&nested)==PG_GUARDED_STORAGE_REENTRY&&!nested);}if(f->mode==7)assert(pg_durable_session_close(f->guard->session)==1);return f->mode==1?1:f->mode==2||f->mode==4?7:f->mode==3?8:0;}
static int guard_mint(void *ctx,uint64_t *out){guard_fixture *f=ctx;++f->calls;assert(f->guard->busy&&f->guard->session->busy);*out=f->mode==8?0:100;if(f->mode==5)f->guard->session->records->failed=PG_RECORDS_ADOPTION_FAILED;return f->mode==2||f->mode==4?7:f->mode==3?8:0;}
typedef struct {pg_record_memory *memory;unsigned fail_read,fail_mint;} guarded_memory_fixture;
static int guarded_memory_read(void *ctx,unsigned table,uint64_t id,struct json_object **out){guarded_memory_fixture *f=ctx;if(f->fail_read){*out=json_object_new_object();return 7;}return pg_record_memory_read(f->memory,(pg_record_table)table,id,out);}
static int guarded_memory_mint(void *ctx,uint64_t *out){guarded_memory_fixture *f=ctx;return f->fail_mint?7:pg_record_memory_mint(f->memory,out);}
static void guarded_transaction_tests(void){
 for(unsigned mode=0;mode<3;++mode){pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));pg_record_session records={.storage=pg_record_memory_storage(&m)};pg_durable_session session={.records=&records,.phase=1};guarded_memory_fixture f={&m,mode==1,mode==2};pg_durable_storage guard={.session=&session,.context=&f,.read=guarded_memory_read,.mint=guarded_memory_mint};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&records,&guard,pg_durable_storage_read));session.active_records=&tx;struct json_object *draft=json_tokener_parse("{\"conversationId\":1,\"status\":\"queued\"}"),*out=0;int status=pg_record_tx_create_submission(&tx,draft,pg_durable_storage_mint,&out);assert(!tx.busy&&!guard.busy&&!session.busy);if(mode){assert(status!=PG_TX_OK&&!out&&records.failed==PG_RECORDS_STORAGE_FAILED&&m.count==1&&!json_object_array_length(tx.writes));assert(!pg_record_tx_discard(&tx));}else{assert(!status&&out&&!records.failed);assert(!pg_record_tx_commit(&tx)&&m.count==2);}json_object_put(out);json_object_put(draft);assert(!pg_record_memory_close(&m));}
}
static int guard_scan(void *ctx,const pg_memory_query *query,pg_memory_page *out){struct json_object *value=0;assert(query->table==2);int status=guard_read(ctx,2,10,&value);out->items=value;out->after=123;out->order=2;out->has_next=1;return status;}
static int guard_head(void *ctx,uint64_t conversation,uint64_t maximum,struct json_object **out){assert(conversation==1&&maximum==99);return guard_read(ctx,2,10,out);}
static int guard_request(void *ctx,const pg_record_request_query *query,struct json_object **out){assert(query->conversation==1&&query->size==3&&!memcmp(query->request,"a\0b",3));return guard_read(ctx,2,10,out);}
static int guard_entry(void *ctx,uint64_t id,uint64_t conversation,struct json_object **out,uint64_t *seq){assert(id==10&&conversation==1);*seq=77;return guard_read(ctx,2,10,out);}
static void guarded_query_tests(void){
 _Static_assert(sizeof(pg_durable_queries)==48&&offsetof(pg_durable_queries,entry)==40,"query guard ABI");
 for(unsigned operation=0;operation<4;++operation)for(unsigned mode=0;mode<8;++mode){pg_record_session records={0};pg_durable_session session={.records=&records,.phase=1};guard_fixture f={.mode=mode};pg_durable_storage guard={.session=&session,.context=&f,.read=guard_read,.request_error=guard_error,.cancelled=guard_cancelled};f.guard=&guard;pg_durable_queries services={&guard,&f,guard_scan,guard_head,guard_request,guard_entry};pg_memory_query query={.table=2};pg_record_request_query request={1,"a\0b",3};pg_memory_page page={0};struct json_object *out=0;uint64_t sequence=999;int status=operation==0?pg_durable_storage_scan(&services,&query,&page):operation==1?pg_durable_storage_head(&services,1,99,&out):operation==2?pg_durable_storage_request(&services,&request,&out):pg_durable_storage_entry(&services,10,1,&out,&sequence);int want=mode==5?64:mode==2||mode==4?7:mode==3?8:mode==1?1:0;assert(status==want&&!guard.busy&&!session.busy&&f.calls==1);assert(records.failed==(mode==5?PG_RECORDS_ADOPTION_FAILED:mode==2?PG_RECORDS_STORAGE_FAILED:0));if(status){assert(!out&&!page.items&&!page.after&&!page.has_next&&!page.order);if(operation==3)assert(!sequence);}else if(operation==0){assert(page.items&&page.after==123&&page.order==2&&page.has_next);}else{assert(out);if(operation==3)assert(sequence==77);}json_object_put(page.items);json_object_put(out);if(records.failed){unsigned before=f.calls;assert(pg_durable_storage_scan(&services,&query,&page)==64&&!page.items&&f.calls==before);}}
}
static int memory_guard_scan(void *ctx,const pg_memory_query *query,pg_memory_page *out){return pg_record_memory_scan(ctx,query,out);}
static int memory_guard_head(void *ctx,uint64_t conversation,uint64_t max,struct json_object **out){return pg_record_memory_head(ctx,conversation,max,out);}
static int memory_guard_request(void *ctx,const pg_record_request_query *query,struct json_object **out){return pg_record_memory_submission_by_request(ctx,query,out);}
static int memory_guard_entry(void *ctx,uint64_t id,uint64_t conversation,struct json_object **out,uint64_t *seq){return pg_record_memory_entry(ctx,id,conversation,out,seq);}
static int memory_request_error(void *ctx,int status){(void)ctx;return status==PG_MEMORY_INVALID;}
static void guarded_memory_query_tests(void){
 pg_record_memory memory={0};assert(!pg_record_memory_open(&memory)&&!doc_commit(&memory,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"note\",\"head\":3}},{\"type\":\"submission\",\"value\":{\"id\":5,\"conversationId\":1,\"requestId\":\"req\",\"status\":\"queued\"}}]"));pg_record_session records={.storage=pg_record_memory_storage(&memory)};pg_durable_session session={.records=&records,.phase=1};pg_durable_storage guard={.session=&session,.context=&memory,.request_error=memory_request_error};pg_durable_queries services={&guard,&memory,memory_guard_scan,memory_guard_head,memory_guard_request,memory_guard_entry};pg_record_queries queries={&services,pg_durable_storage_scan,pg_durable_storage_head};pg_record_requests requests={&services,pg_durable_storage_request};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&records,0,0));pg_memory_query query={.table=PG_TABLE_ENTRY,.limit=1,.flags=PG_SCAN_CONVERSATION,.conversation_id=1};pg_memory_page page={0};assert(!pg_record_tx_scan(&tx,&queries,&query,&page)&&json_object_array_length(page.items)==1);json_object_put(page.items);struct json_object *out=0;assert(!pg_record_tx_latest_head(&tx,&queries,1,&out)&&json_object_get_uint64(json_object_object_get(out,"id"))==3);json_object_put(out);pg_record_request_query request={1,"req",3};assert(!pg_record_tx_submission_by_request(&tx,&requests,&request,&out)&&json_object_get_uint64(json_object_object_get(out,"id"))==5);json_object_put(out);out=0;uint64_t seq=0;assert(!pg_durable_storage_entry(&services,3,1,&out,&seq)&&seq==1);json_object_put(out);out=0;query.limit=0;assert(pg_record_tx_scan(&tx,&queries,&query,&page)==PG_TX_READ_FAILED&&!page.items&&!records.failed);query.limit=1;assert(!pg_record_tx_scan(&tx,&queries,&query,&page));json_object_put(page.items);assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&memory));
}
static void guarded_query_fault_tests(void){
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){pg_record_memory memory={0};assert(!pg_record_memory_open(&memory)&&!doc_commit(&memory,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));pg_record_session records={0};pg_durable_session session={.records=&records,.phase=1};pg_durable_storage guard={.session=&session,.context=&memory,.request_error=memory_request_error};pg_durable_queries services={.guard=&guard,.context=&memory,.scan=memory_guard_scan};pg_memory_query query={.table=0,.limit=1};pg_memory_page page={0};doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_durable_storage_scan(&services,&query,&page);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!guard.busy&&!session.busy);if(target){assert(status==PG_MEMORY_ALLOCATION&&records.failed==PG_RECORDS_STORAGE_FAILED&&!page.items&&!page.after&&!page.order&&!page.has_next);++failures;}else{assert(!status&&page.items&&!records.failed);total=calls;json_object_put(page.items);}assert(!pg_record_memory_close(&memory));}
 printf("PASS: guarded Storage queries, partial page cleanup and %u direct allocation/clone failures latch first failure\n",failures);
#endif
}
typedef struct {pg_session_listeners *listeners;unsigned kind,id,fail,mutate,calls;unsigned *log,*used;} listener_fixture;
static int session_test_listener(void *ctx,const void *publication,void *delivery);
static int session_late_listener(void *ctx,const void *publication,void *delivery){listener_fixture *f=ctx;assert(publication==f->log&&delivery==f->used);f->log[(*f->used)++]=99;return 0;}
static int session_test_listener(void *ctx,const void *publication,void *delivery){listener_fixture *f=ctx;assert((f->kind==2?publication==0:publication==f->log)&&delivery==f->used);++f->calls;f->log[(*f->used)++]=f->id;if(f->mutate){pg_session_unlisten(f->listeners,f->kind,session_test_listener,f+1);if(f->kind!=2)assert(!pg_session_listen(f->listeners,f->kind,session_late_listener,f));assert(pg_session_deliver(f->listeners,f->kind,publication,delivery)!=0&&pg_session_listeners_close(f->listeners,delivery)!=0);}return f->fail?7:0;}
static void session_listener_report(void *ctx,int error){unsigned *reported=ctx;assert(error==7);++*reported;}
static void session_listener_tests(void){
 _Static_assert(sizeof(pg_session_listeners)==56&&sizeof(pg_session_listener_entry)==24,"listener ABI");
 for(unsigned kind=0;kind<3;++kind){pg_record_session records={0};unsigned log[32]={0},used=0,reports=0;pg_session_listeners listeners={.records=&records,.context=&reports,.report=session_listener_report};listener_fixture fixtures[3];for(unsigned i=0;i<3;++i){fixtures[i]=(listener_fixture){&listeners,kind,i+1,i==0,i==0,0,log,&used};assert(!pg_session_listen(&listeners,kind,session_test_listener,&fixtures[i]));assert(!pg_session_listen(&listeners,kind,session_test_listener,&fixtures[i]));}assert(listeners.count==3);assert(!(kind==2?pg_session_listeners_close(&listeners,&used):pg_session_deliver(&listeners,kind,log,&used)));assert(used==3&&log[0]==1&&log[1]==2&&log[2]==3&&reports==(kind!=0)&&records.failed==(kind==0?PG_RECORDS_ADOPTION_FAILED:0));if(kind!=2){fixtures[0].mutate=0;assert(!pg_session_deliver(&listeners,kind,log,&used)&&used==6&&log[3]==1&&log[4]==3&&log[5]==99);}assert(!pg_session_listeners_close(&listeners,&used)&&!listeners.entries&&!listeners.count&&listeners.closed);assert(pg_session_listen(&listeners,kind,session_test_listener,&fixtures[0])!=0);}
 pg_record_session records={0};pg_session_listeners listeners={.records=&records};unsigned log[1],used=0;listener_fixture fixture={&listeners,1,1,0,0,0,log,&used};for(unsigned i=0;i<1000;++i){assert(!pg_session_listen(&listeners,1,session_test_listener,&fixture));pg_session_unlisten(&listeners,1,session_test_listener,&fixture);}assert(!listeners.count&&listeners.capacity==8&&!pg_session_listeners_close(&listeners,&used));
#ifdef PG_MEMORY_FAULTS
 for(unsigned mode=0;mode<3;++mode){listeners=(pg_session_listeners){.records=&records};fixture.listeners=&listeners;if(mode)assert(!pg_session_listen(&listeners,1,session_test_listener,&fixture));doc_calls=0;doc_fail=1;doc_tracking=1;int status=mode==2?pg_session_listeners_close(&listeners,&used):mode?pg_session_deliver(&listeners,1,log,&used):pg_session_listen(&listeners,1,session_test_listener,&fixture);doc_tracking=0;doc_fail=0;assert(status&&!listeners.busy&&!used&&listeners.count==(mode!=0)&&!listeners.closed&&!records.failed);assert(!pg_session_listeners_close(&listeners,&used));}
#endif
 puts("PASS: listener identity/order, dispatch snapshots, mutation/cancellation, contained errors, close and allocation failures");
}
typedef struct {unsigned order[4],count;const pg_session_publications *payload;} publication_listener_fixture;
static int publication_internal(void *ctx,const void *value,void *delivery){publication_listener_fixture *f=ctx;assert(delivery==f&&value==f->payload->observed);f->order[f->count++]=0;return 0;}
static int publication_external(void *ctx,const void *value,void *delivery){publication_listener_fixture *f=ctx;assert(delivery==f&&value==f->payload->stored);f->order[f->count++]=1;return 0;}
static void publication_tests(void){
 _Static_assert(sizeof(pg_session_publication_request)==40&&sizeof(pg_session_publications)==16,"publication ABI");
 struct json_object *writes=json_tokener_parse("[{\"type\":\"entry\",\"value\":{\"id\":1}},{\"type\":\"document.change\",\"id\":2},{\"type\":\"task\",\"value\":{\"id\":3}},{\"type\":\"entry\\u0000ignored\",\"value\":{}},{\"type\":\"submission\",\"value\":{\"id\":4}},{\"type\":\"conversation\",\"value\":{\"id\":5}}]"),*observed=json_tokener_parse("[{\"type\":\"document\",\"value\":{\"text\":\"new\"},\"ops\":[]}]"),*stored=json_tokener_parse("[{\"type\":\"document\",\"value\":{\"text\":\"new\"},\"ops\":[[\"r\",{}]]}]");assert(writes&&observed&&stored);
 for(unsigned mode=0;mode<3;++mode)for(unsigned shared=0;shared<2;++shared){pg_session_publication_request request={writes,observed,shared?observed:stored,7,mode,0};pg_session_publications out={0};assert(!pg_session_publications_prepare(&request,&out));struct json_object *changes=0;if(mode!=2){assert(out.observed&&!strcmp(json_object_get_string(json_object_object_get(out.observed,"kind")),mode==1?"memory":"stored"));changes=json_object_object_get(out.observed,"changes");assert(json_object_array_length(changes)==(mode==0?5:1)&&json_object_equal(json_object_array_get_idx(changes,mode==0?4:0),json_object_array_get_idx(observed,0)));assert((json_object_object_get(out.observed,"seq")!=0)==(mode==0));}else assert(!out.observed);if(mode!=1){assert(out.stored&&json_object_get_int(json_object_object_get(out.stored,"seq"))==7);changes=json_object_object_get(out.stored,"changes");assert(json_object_array_length(changes)==(mode==0?5:1)&&json_object_equal(json_object_array_get_idx(changes,mode==0?4:0),json_object_array_get_idx(request.stored_documents,0)));if(mode==0)assert((out.observed==out.stored)==shared);}else assert(!out.stored);
 pg_record_session records={0};pg_session_listeners listeners={.records=&records};publication_listener_fixture f={.payload=&out};assert(!pg_session_listen(&listeners,0,publication_internal,&f)&&!pg_session_listen(&listeners,1,publication_external,&f)&&!pg_session_publications_deliver(&listeners,&out,&f));assert(f.count==(mode==0?2:1)&&f.order[0]==(mode==2?1:0));if(!mode)assert(f.order[1]==1);assert(!pg_session_listeners_close(&listeners,&f));pg_session_publications_discard(&out);assert(!out.observed&&!out.stored);}
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;for(unsigned target=0;target<=total;++target){pg_session_publication_request request={writes,observed,stored,7,0,0};pg_session_publications out={0};doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_session_publications_prepare(&request,&out);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;if(target){assert(status&&!out.observed&&!out.stored);++failures;}else{assert(!status&&out.observed&&out.stored);total=calls;pg_session_publications_discard(&out);}}printf("PASS: six stored/memory/held publication modes and %u direct allocation/insert/clone failures\n",failures);
#endif
 json_object_put(writes);json_object_put(observed);json_object_put(stored);
}
static void document_notice_tests(void){
 _Static_assert(sizeof(pg_document_publication_request)==56,"Document notice ABI");struct json_object *record=json_tokener_parse("{\"id\":10,\"kind\":\"doc\",\"createdAt\":2,\"retiredAt\":7}"),*source=json_tokener_parse("{\"id\":3,\"at\":2}"),*value=json_tokener_parse("{\"text\":\"new\"}"),*ops=json_tokener_parse("[[\"a\",[\"text\"],\"tail\"]]");assert(record&&source&&value&&ops);
 unsigned modes[]={0,PG_NOTICE_RETIRE,PG_NOTICE_COPY,PG_NOTICE_PUBLISHES,PG_NOTICE_CREATED|PG_NOTICE_PUBLISHES};
 for(unsigned i=0;i<5;++i){pg_document_publication_request request={record,source,value,ops,1,2,modes[i],0};struct json_object *out=0;assert(!pg_document_publication_prepare(&request,&out));if(!i)assert(!out);else{assert(out&&json_object_get_int(json_object_object_get(out,"conversationId"))==1&&json_object_equal(json_object_object_get(out,"record"),record));if(i==2)assert(json_object_equal(json_object_object_get(out,"source"),source)&&!json_object_object_get(out,"value"));else{assert(json_object_array_length(json_object_object_get(out,"ops"))==(i==3?1:0));if(i==1){struct json_object *null_value=(void*)1;assert(json_object_object_get_ex(out,"value",&null_value)&&!null_value&&!json_object_object_get(out,"version"));}else assert(json_object_equal(json_object_object_get(out,"value"),value)&&json_object_get_int(json_object_object_get(out,"version"))==2);}assert(!json_object_object_add(json_object_object_get(out,"record"),"outside",json_object_new_int(1))&&!json_object_object_get(record,"outside"));}json_object_put(out);}
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;for(unsigned target=0;target<=total;++target){pg_document_publication_request request={record,source,value,ops,1,2,PG_NOTICE_PUBLISHES,0};struct json_object *out=0;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_publication_prepare(&request,&out);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;if(target){assert(status&&!out);++failures;}else{assert(!status&&out);total=calls;json_object_put(out);}}printf("PASS: Document notice semantics/ownership and %u direct allocation/insert/clone failures\n",failures);
#endif
 json_object_put(record);json_object_put(source);json_object_put(value);json_object_put(ops);
}
static void publication_confirm_tests(void){
 struct json_object *empty=json_object_new_array(),*documents=json_tokener_parse("[{\"type\":\"document\",\"record\":{\"id\":1,\"createdAt\":0},\"version\":1,\"value\":{},\"ops\":[]},{\"type\":\"document\",\"record\":{\"id\":2,\"createdAt\":2,\"retiredAt\":0},\"value\":null,\"ops\":[]},{\"type\":\"document.copy\",\"record\":{\"id\":3,\"createdAt\":0},\"source\":{\"id\":1,\"at\":2}}]");assert(empty&&documents);
 for(unsigned shared=0;shared<2;++shared){struct json_object *stored=0;assert(!json_object_deep_copy(documents,&stored,0));pg_session_publication_request request={empty,documents,shared?documents:stored,1,0,0};pg_session_publications out={0};assert(!pg_session_publications_prepare(&request,&out));
#ifdef PG_MEMORY_FAULTS
 doc_calls=0;doc_fail=1;doc_tracking=1;
#endif
 assert(!pg_session_publications_confirm(&out,7));
#ifdef PG_MEMORY_FAULTS
 doc_tracking=0;doc_fail=0;assert(!doc_calls);
#endif
 struct json_object *channels[]={out.observed,out.stored};for(unsigned c=0;c<2;++c){assert(json_object_get_uint64(json_object_object_get(channels[c],"seq"))==7);struct json_object *changes=json_object_object_get(channels[c],"changes");for(unsigned i=0;i<3;++i){struct json_object *record=json_object_object_get(json_object_array_get_idx(changes,i),"record");assert(json_object_get_uint64(json_object_object_get(record,"createdAt"))==(i==1?2:7));if(i==1)assert(json_object_get_uint64(json_object_object_get(record,"retiredAt"))==7);}}assert(json_object_get_int(json_object_object_get(json_object_object_get(json_object_array_get_idx(documents,0),"record"),"createdAt"))==0);pg_session_publications_discard(&out);json_object_put(stored);}
 for(unsigned mode=0;mode<5;++mode){struct json_object *stored=0;assert(!json_object_deep_copy(documents,&stored,0));pg_session_publication_request request={empty,documents,stored,1,0,0};pg_session_publications out={0};assert(!pg_session_publications_prepare(&request,&out));struct json_object *last=json_object_object_get(json_object_array_get_idx(json_object_object_get(out.stored,"changes"),2),"record");if(mode==0)json_object_object_del(last,"createdAt");if(mode==1)assert(!json_object_object_add(last,"createdAt",json_object_new_string("bad")));if(mode==2)assert(!json_object_object_add(last,"createdAt",json_object_new_int64(-1)));struct json_object *before=0;assert(!json_object_deep_copy(out.observed,&before,0));assert(pg_session_publications_confirm(&out,mode==3?0:mode==4?UINT64_MAX:7)&&json_object_equal(before,out.observed));assert(json_object_get_int(json_object_object_get(out.stored,"seq"))==1);json_object_put(before);pg_session_publications_discard(&out);json_object_put(stored);}
 json_object_put(empty);json_object_put(documents);puts("PASS: allocation-free publication confirmation, shared/split lifecycle stamps and rejection before mutation");
}
static void storage_guard_tests(void){
 _Static_assert(sizeof(pg_durable_storage)==56&&offsetof(pg_durable_storage,busy)==48,"guard descriptor ABI");
 for(unsigned mint=0;mint<2;++mint)for(unsigned mode=0;mode<9;++mode){pg_record_session records={0};pg_durable_session session={.records=&records,.phase=1};guard_fixture f={.mode=mode};pg_durable_storage guard={.session=&session,.context=&f,.read=guard_read,.mint=guard_mint,.request_error=guard_error,.cancelled=guard_cancelled};f.guard=&guard;struct json_object *out=0;uint64_t id=999;int status=mint?pg_durable_storage_mint(&guard,&id):pg_durable_storage_read(&guard,2,10,&out);unsigned failed=mode==5?PG_RECORDS_ADOPTION_FAILED:mode==2||(mint&&(mode==3||mode==4||mode==8))?PG_RECORDS_STORAGE_FAILED:0;int want=mode==5?64:mint&&mode==8?66:mode==2||mode==4?7:mode==3?8:!mint&&mode==1?1:0;assert(status==want&&records.failed==failed&&!guard.busy&&!session.busy&&f.calls==1);if(!status){if(mint)assert(id==100);else assert(out);}else{assert(!out);if(mint)assert(!id);}json_object_put(out);out=0;
 if(failed){unsigned before=f.calls;assert(pg_durable_storage_read(&guard,2,10,&out)==PG_GUARDED_STORAGE_FAILED&&!out&&f.calls==before);assert(pg_durable_storage_mint(&guard,&id)==PG_GUARDED_STORAGE_FAILED&&!id&&f.calls==before);}
 if(mint)assert(!f.error_checks&&!f.cancel_checks);else if(mode==3)assert(f.error_checks==1&&!f.cancel_checks);else if(mode==2||mode==4)assert(f.error_checks==1&&f.cancel_checks==1);
 }
 pg_record_session records={0};pg_durable_session session={.records=&records,.phase=2};guard_fixture f={0};pg_durable_storage guard={.session=&session,.context=&f,.read=guard_read,.mint=guard_mint};f.guard=&guard;struct json_object *out=0;assert(pg_durable_storage_read(&guard,2,10,&out)==PG_GUARDED_STORAGE_CLOSED&&!out&&!f.calls);pg_record_tx tx={0};session.active_records=&tx;session.busy=1;assert(!pg_durable_storage_read(&guard,2,10,&out)&&out&&session.busy==1);json_object_put(out);out=0;assert(pg_durable_storage_read(0,0,0,&out)==PG_GUARDED_STORAGE_INVALID&&!out);
}
static void task_options_tests(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"submission\",\"value\":{\"id\":10,\"conversationId\":1,\"status\":\"queued\",\"trace\":{\"parent\":\"old\"}}},{\"type\":\"submission\",\"value\":{\"id\":11,\"conversationId\":1,\"status\":\"queued\"}},{\"type\":\"submission\",\"value\":{\"id\":12,\"conversationId\":1,\"status\":\"queued\",\"trace\":null}},{\"type\":\"submission\",\"value\":{\"id\":13,\"conversationId\":1,\"status\":\"queued\",\"trace\":{\"parent\":\"third\"}}}]"));
 const char *inputs[]={"{\"trace\":{\"parent\":\"given\"},\"links\":[{}]}","{\"submissionIds\":[],\"trace\":{},\"links\":[{}]}","{\"submissionIds\":[10,11,13,12],\"trace\":{},\"links\":[{}]}","{\"submissionIds\":[99,13]}","{\"submissionIds\":[12]}"};
 for(unsigned mode=0;mode<5;++mode){pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));struct json_object *write=json_tokener_parse("{\"type\":\"submission\",\"value\":{\"id\":10,\"conversationId\":1,\"status\":\"queued\",\"trace\":{\"parent\":\"new\"}}}"),*options=json_tokener_parse(inputs[mode]),*out=0;assert(write&&options&&!pg_record_tx_stage(&tx,PG_TX_SUBMISSION,write)&&!pg_record_tx_task_options(&tx,options,&out)&&out&&tx.has_table_write&&!tx.busy&&json_object_array_length(tx.writes)==1);
 if(mode==0)assert(json_object_equal(out,options));if(mode==1)assert(!json_object_object_get(out,"trace")&&!json_object_object_get(out,"links"));if(mode==2){assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(out,"trace"),"parent")),"new"));struct json_object *links=json_object_object_get(out,"links");assert(json_object_array_length(links)==2&&!strcmp(json_object_get_string(json_object_object_get(json_object_array_get_idx(links,0),"parent")),"third")&&!json_object_array_get_idx(links,1));}if(mode>=3)assert(!strcmp(json_object_get_string(json_object_object_get(out,"trace")),"root"));json_object_put(out);json_object_put(write);json_object_put(options);assert(!pg_record_tx_discard(&tx));}
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));struct json_object *options=json_tokener_parse(inputs[2]),*out=0;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_record_tx_task_options(&tx,options,&out);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!tx.busy&&tx.has_table_write&&!json_object_array_length(tx.writes)&&!session.failed);if(target){assert(status!=PG_TX_OK&&!out);++failures;}else{assert(!status&&out);total=calls;json_object_put(out);}json_object_put(options);assert(!pg_record_tx_discard(&tx));}
 printf("PASS: task submission trace/links, staged precedence, absent/null/empty and %u direct allocation/insert/clone failures\n",failures);
#endif
 assert(!pg_record_memory_close(&m));
}
static void set_task_tests(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));task_clock_fixture clock={&tx,0,100.5};const char *states[]={"pending","running","waiting","running","terminal"};struct json_object *out=0;
 for(unsigned i=0;i<5;++i){struct json_object *value=task_draft(states[i],1);clock.value=100.5+i;assert(!pg_record_tx_set_task(&tx,value,&clock,task_clock)&&json_object_array_length(tx.writes)==1);assert(!json_object_object_add(value,"outside",json_object_new_int64(1)));json_object_put(value);assert(!pg_record_tx_staged_tasks(&tx,&out)&&json_object_array_length(out)==1);struct json_object *candidate=json_object_array_get_idx(out,0);assert(!json_object_object_get(candidate,"outside"));if(i)assert(json_object_get_double(json_object_object_get(candidate,"startedAt"))==101.5);if(i==4)assert(json_object_get_double(json_object_object_get(candidate,"endedAt"))==104.5);json_object_put(out);out=0;}
 assert(clock.calls==2);struct json_object *value=task_draft("pending",1);assert(pg_record_tx_set_task(&tx,value,&clock,task_clock)==PG_TX_INVALID&&clock.calls==2);json_object_put(value);assert(!pg_record_tx_commit(&tx)&&!pg_record_memory_read(&m,PG_TABLE_TASK,10,&out)&&json_object_get_double(json_object_object_get(out,"startedAt"))==101.5);json_object_put(out);out=0;
 tx=(pg_record_tx){0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));value=task_draft("waiting",1);assert(!pg_record_tx_set_task(&tx,value,&clock,task_clock));json_object_put(value);clock.value=NAN;value=task_draft("running",1);assert(pg_record_tx_set_task(&tx,value,&clock,task_clock)==PG_TX_INVALID&&!pg_record_tx_staged_tasks(&tx,&out)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(json_object_array_get_idx(out,0),"state"),"status")),"waiting"));json_object_put(out);out=0;json_object_put(value);value=task_draft("running",2);assert(pg_record_tx_set_task(&tx,value,&clock,task_clock)==PG_TX_INVALID&&!pg_record_tx_discard(&tx));json_object_put(value);assert(!pg_record_memory_close(&m));
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){assert(!pg_record_memory_open(&m));session=(pg_record_session){.storage=pg_record_memory_storage(&m)};tx=(pg_record_tx){0};clock=(task_clock_fixture){&tx,0,1.25};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));value=task_draft("pending",1);assert(!pg_record_tx_set_task(&tx,value,&clock,task_clock));json_object_put(value);struct json_object *before=0;assert(!pg_record_tx_staged_tasks(&tx,&before));value=task_draft("running",1);doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_record_tx_set_task(&tx,value,&clock,task_clock);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!tx.busy&&!session.failed&&tx.has_table_write&&json_object_array_length(tx.writes)==1);assert(!pg_record_tx_staged_tasks(&tx,&out));if(target){assert(status!=PG_TX_OK&&json_object_equal(before,out));++failures;}else {assert(!status);total=calls;}json_object_put(before);json_object_put(out);out=0;json_object_put(value);assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&m));}
 printf("PASS: setTask terminal/conversation guard, lifecycle timestamps/isolation and %u direct allocation/insert/clone failures\n",failures);
#endif
}
static void task_cache_tests(void){
 for(unsigned error=0;error<2;++error){pg_record_memory memory={0};assert(!pg_record_memory_open(&memory));pg_record_session session={.storage=pg_record_memory_storage(&memory)};pg_record_tx tx={0};task_cache_provider f={.tx=&tx,.mode=error};assert(!pg_record_tx_begin(&tx,&session,&f,task_cache_read));struct json_object *out=0;
 for(unsigned i=0;i<3;++i){assert(pg_record_tx_read(&tx,PG_TABLE_TASK,2,&out)==(error?PG_TX_READ_FAILED:PG_TX_OK)&&f.calls==1&&!tx.busy);if(!error){assert(json_object_get_int(json_object_object_get(out,"n"))==1&&!json_object_object_add(out,"outside",json_object_new_int64(1)));json_object_put(out);out=0;}}
 for(unsigned i=0;i<2;++i)assert(pg_record_tx_read(&tx,PG_TABLE_TASK,99,&out)==PG_TX_MISSING&&!out&&f.calls==2);
 assert(pg_record_tx_read(&tx,PG_TABLE_TASK,3,&out)==(error?PG_TX_READ_FAILED:PG_TX_OK));json_object_put(out);out=0;
 for(unsigned id=3;id>=2;--id){char text[160];int len=snprintf(text,sizeof(text),"{\"type\":\"task\",\"value\":{\"id\":%u,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}",id);assert(len>0);struct json_object *write=json_tokener_parse(text);assert(write&&!pg_record_tx_stage(&tx,PG_TX_TASK,write));json_object_put(write);}
 assert(!pg_record_tx_staged_tasks(&tx,&out)&&json_object_array_length(out)==2&&json_object_get_int(json_object_object_get(json_object_array_get_idx(out,0),"id"))==2&&json_object_get_int(json_object_object_get(json_object_array_get_idx(out,1),"id"))==3);json_object_put(out);out=0;assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&memory));
 }
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){pg_record_memory memory={0};assert(!pg_record_memory_open(&memory));pg_record_session session={.storage=pg_record_memory_storage(&memory)};pg_record_tx tx={0};task_cache_provider f={.tx=&tx};assert(!pg_record_tx_begin(&tx,&session,&f,task_cache_read));struct json_object *out=0;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_record_tx_read(&tx,PG_TABLE_TASK,2,&out);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;if(target){assert(status!=PG_TX_OK&&!out&&!tx.busy);++failures;}else{assert(!status&&out);json_object_put(out);out=0;total=calls;}assert(!pg_record_tx_read(&tx,PG_TABLE_TASK,2,&out)&&out&&!json_object_object_get(out,"outside"));json_object_put(out);assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&memory));}
 printf("PASS: committed Task cached success/missing/error and read-established order; %u direct allocation/insert/clone failures\n",failures);
#endif
}

static void task_cache_rebuild(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"fork\"}},{\"type\":\"task\",\"value\":{\"id\":5,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};task_rebuild_provider p={&m,0};assert(!pg_record_tx_begin(&tx,&session,&p,task_rebuild_read));struct json_object *out=0;assert(!pg_record_tx_read(&tx,PG_TABLE_TASK,5,&out));json_object_put(out);out=0;assert(pg_record_tx_read(&tx,PG_TABLE_TASK,99,&out)==PG_TX_MISSING&&p.tasks==2);
 pg_fork_storage fork={&m,fork_entry,fork_scan,tx_mint};assert(!pg_record_tx_fork_conversation(&tx,1,3,0,&fork,&out));json_object_put(out);out=0;
 for(unsigned id=99;;id=5){char text[180];int n=snprintf(text,sizeof(text),"{\"type\":\"task\",\"value\":{\"id\":%u,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}",id);assert(n>0);struct json_object *write=json_tokener_parse(text);assert(write&&!pg_record_tx_stage(&tx,PG_TX_TASK,write));json_object_put(write);if(id==5)break;}
 assert(!pg_record_tx_staged_tasks(&tx,&out)&&json_object_array_length(out)==2&&json_object_get_int(json_object_object_get(json_object_array_get_idx(out,0),"id"))==5&&json_object_get_int(json_object_object_get(json_object_array_get_idx(out,1),"id"))==99);json_object_put(out);assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&m));puts("PASS: Task cached order/ownership survives fork batch rebuild");
}
static void candidate_snapshots(void){
 for(unsigned mode=0;mode<2;++mode){unsigned count=0;
 for(unsigned attempt=0;attempt<=count;++attempt){pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));struct json_object *out=0;assert(!pg_record_tx_staged_tasks(&tx,&out)&&!json_object_array_length(out));json_object_put(out);out=0;
 for(unsigned i=0;i<100;++i){char text[256];unsigned id=i%40+100;int n=snprintf(text,sizeof(text),"{\"type\":\"task\",\"value\":{\"id\":%u,\"conversationId\":1,\"state\":{\"status\":\"pending\"},\"n\":%u}}",id,i);assert(n>0&&(size_t)n<sizeof(text));struct json_object *write=json_tokener_parse(text);assert(write&&!pg_record_tx_stage(&tx,PG_TX_TASK,write));json_object_put(write);}
 for(unsigned i=0;i<3;++i){char text[96];int n=snprintf(text,sizeof(text),"{\"type\":\"conversation\",\"value\":{\"id\":%u}}",i+1);assert(n>0);struct json_object *write=json_tokener_parse(text);assert(write&&!pg_record_tx_stage(&tx,PG_TX_CONVERSATION,write));json_object_put(write);}
#ifdef PG_MEMORY_FAULTS
 doc_calls=0;doc_fail=attempt;doc_tracking=1;
#endif
 int status=mode?pg_record_tx_staged_conversations(&tx,&out):pg_record_tx_staged_tasks(&tx,&out);
#ifdef PG_MEMORY_FAULTS
 doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
#else
 unsigned calls=0;
#endif
 assert(!tx.busy&&json_object_array_length(tx.writes)==103&&tx.has_table_write&&!session.failed);
 if(attempt){assert(status==PG_TX_ALLOCATION&&!out);}else{assert(!status&&out&&json_object_array_length(out)==(mode?3:40));count=calls;for(unsigned i=0;i<(mode?3u:40u);++i){struct json_object *record=json_object_array_get_idx(out,i);assert(json_object_get_uint64(json_object_object_get(record,"id"))==(mode?i+1:i+100));if(!mode)assert(json_object_get_uint64(json_object_object_get(record,"n"))==(i<20?i+80:i+40));}assert(!json_object_object_add(json_object_array_get_idx(out,0),"local",json_object_new_int64(1)));json_object_put(out);out=0;assert(!(mode?pg_record_tx_staged_conversations(&tx,&out):pg_record_tx_staged_tasks(&tx,&out))&&!json_object_object_get(json_object_array_get_idx(out,0),"local"));json_object_put(out);out=0;}
 assert(!pg_record_tx_discard(&tx)&&pg_record_tx_staged_tasks(&tx,&out)==PG_TX_SEALED&&!out&&!pg_record_memory_close(&m));
 }printf("PASS: staged %s latest/order/ownership; %u direct allocation/insert/clone failures\n",mode?"Conversations":"Tasks",count);
 }
}
static void create_submission_tests(void){
 for(unsigned staged=0;staged<2;++staged){pg_record_memory m={0};assert(!pg_record_memory_open(&m));if(!staged)assert(!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));if(staged){struct json_object *c=0;assert(!pg_record_tx_create_conversation(&tx,0,1,tx_mint,&c));json_object_put(c);}
 struct json_object *draft=json_tokener_parse("{\"id\":999,\"conversationId\":1,\"requestId\":\"r\",\"type\":\"write\",\"status\":\"queued\",\"data\":{\"x\":1}}"),*out=0;uint64_t minted=m.next_id;assert(draft&&!pg_record_tx_create_submission(&tx,draft,tx_mint,&out)&&json_object_get_uint64(json_object_object_get(out,"id"))==minted&&tx.has_table_write&&m.next_id==minted+1);assert(!json_object_object_add(json_object_object_get(out,"data"),"x",json_object_new_int64(9)));assert(!json_object_object_add(json_object_object_get(draft,"data"),"x",json_object_new_int64(8)));json_object_put(out);out=0;struct json_object *change=json_tokener_parse("{\"status\":\"unanswered\"}");assert(change&&!pg_record_tx_submission_change(&tx,minted,change));json_object_put(change);assert(!pg_record_tx_commit(&tx)&&!pg_record_memory_read(&m,PG_TABLE_SUBMISSION,minted,&out)&&json_object_get_int(json_object_object_get(json_object_object_get(out,"data"),"x"))==1&&!strcmp(json_object_get_string(json_object_object_get(out,"status")),"unanswered"));json_object_put(out);json_object_put(draft);assert(!pg_record_memory_close(&m));}
 pg_record_memory m={0};assert(!pg_record_memory_open(&m));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));struct json_object *draft=json_tokener_parse("{\"conversationId\":99,\"status\":\"queued\"}"),*out=0;uint64_t next=m.next_id;assert(pg_record_tx_create_submission(&tx,draft,tx_mint,&out)==PG_TX_INVALID&&!out&&m.next_id==next&&tx.has_table_write&&!json_object_array_length(tx.writes));assert(pg_record_tx_read(&tx,0,99,&out)==PG_TX_READ_AFTER_WRITE&&!pg_record_tx_discard(&tx));json_object_put(draft);assert(!pg_record_memory_close(&m));
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}}]"));session=(pg_record_session){.storage=pg_record_memory_storage(&m)};tx=(pg_record_tx){0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));draft=json_tokener_parse("{\"conversationId\":1,\"requestId\":\"r\",\"type\":\"write\",\"status\":\"queued\"}");doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_record_tx_create_submission(&tx,draft,tx_mint,&out);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
 if(target){assert(status!=PG_TX_OK&&!out&&!tx.busy&&tx.has_table_write&&!json_object_array_length(tx.writes)&&!session.failed&&m.next_id>=2);++failures;}else{assert(!status&&out);json_object_put(out);out=0;total=calls;}json_object_put(draft);assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&m));}
 printf("PASS: createSubmission staged/committed owner, mint, isolation and %u direct allocation/insert/clone failures\n",failures);
#endif
}
static void typed_entry_tests(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"k\\u0000中\"}},{\"type\":\"entry\",\"value\":{\"id\":4,\"conversationId\":1,\"kind\":\"\"}}]"));pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));pg_entry_kind kind={"k\0中",5},wrong={"k",1},empty={0,0};struct json_object *out=0;
 assert(!pg_record_tx_entry(&tx,3,&kind,&out)&&json_object_get_int(json_object_object_get(out,"id"))==3);json_object_put(out);out=0;
 assert(pg_record_tx_entry(&tx,3,&wrong,&out)==PG_TX_MISSING&&!out&&!tx.busy);assert(!pg_record_tx_entry(&tx,3,0,&out));json_object_put(out);out=0;assert(!pg_record_tx_entry(&tx,4,&empty,&out));json_object_put(out);out=0;assert(pg_record_tx_entry(&tx,999,&kind,&out)==PG_TX_MISSING&&!out);
 struct json_object *write=json_tokener_parse("{\"type\":\"task\",\"value\":{\"id\":5,\"conversationId\":1,\"state\":{\"status\":\"pending\"}}}");assert(write&&!pg_record_tx_stage(&tx,PG_TX_TASK,write)&&pg_record_tx_entry(&tx,3,&kind,&out)==PG_TX_READ_AFTER_WRITE&&!out);json_object_put(write);assert(!pg_record_tx_discard(&tx)&&pg_record_tx_entry(&tx,3,&kind,&out)==PG_TX_SEALED);
#ifdef PG_MEMORY_FAULTS
 assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));doc_calls=0;doc_fail=1;doc_tracking=1;int status=pg_record_tx_entry(&tx,3,&kind,&out);doc_tracking=0;doc_fail=0;assert(status==PG_TX_READ_FAILED&&!out&&!tx.busy&&!pg_record_tx_discard(&tx));
#endif
 assert(!pg_record_memory_close(&m));puts("PASS: committed typed Entry exact kind, empty/NUL keys, missing and Tx barriers");
}
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
static void held_content_storage(void){
 pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"held\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"latest\",\"fork\":\"initial\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"\"}}}]"));pg_pending_ops q={0};assert(!pg_pending_ops_open(&q));pg_record_session session={.storage=pg_record_memory_storage(&m)};
 const char *batches[]={"[[\"a\",[\"text\"],\"a\"]]","[[\"r\",{\"text\":\"reset\"}]]","[[\"a\",[\"text\"],\"B\"]]"};
 const char *values[]={"{\"text\":\"ab\"}","{\"text\":\"reset\"}","{\"text\":\"resetB\"}"};
 for(unsigned step=0;step<3;++step){
  struct json_object *batch=json_tokener_parse(batches[step]),*extra=json_tokener_parse(step==0?"[[\"a\",[\"text\"],\"b\"]]":"[]"),*value=json_tokener_parse(values[step]);assert(batch&&extra&&value&&!pg_pending_ops_push(&q,batch));
  pg_document_held_content planned={0};assert(!pg_document_store_held(&q,extra,value,1,step==2,&planned)&&!pg_pending_ops_size(&q));struct json_object *write=json_object_new_object();assert(write&&!json_object_object_add(write,"type",json_object_new_string("document.change"))&&!json_object_object_add(write,"id",json_object_new_int(2))&&!json_object_object_add(write,"content",json_object_get(planned.content)));
  pg_record_tx tx={0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read)&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_CHANGE,write)&&!pg_record_tx_commit(&tx));
  struct json_object *stored=0;assert(!pg_record_memory_document(&m,2,UINT64_MAX,&stored)&&json_object_equal(json_object_object_get(stored,"value"),value)&&json_object_get_uint64(json_object_object_get(stored,"deltasSinceBase"))==(step==2?0u:step+1));
  json_object_put(stored);json_object_put(write);json_object_put(planned.content);json_object_put(planned.ops);json_object_put(batch);json_object_put(extra);json_object_put(value);
 }
 assert(!pg_pending_ops_close(&q)&&!pg_record_memory_close(&m));
}
typedef struct {pg_document_loaded *loaded;pg_pending_ops pending;uint64_t deadline;unsigned base;} deferred_slot;
typedef struct {deferred_slot slots[8];size_t count;} deferred_token;
typedef struct {
 pg_record_memory memory;pg_document_cache cache;pg_record_session session;pg_record_tx tx;pg_document_tx scope;pg_document_definition definition;pg_document_address address;
 deferred_slot slots[8];size_t count;uint64_t now,within;unsigned prepared,applied,discarded,observed,adopted,published,fail_prepare,fail_apply,checkpoint,flushing,flush_base[8];
} deferred_fixture;
static uint64_t deferred_clock(void *context){deferred_fixture *f=context;assert(f->cache.busy);return f->now;}
static int deferred_pinned(void *context,uint64_t id){deferred_fixture *f=context;for(size_t i=0;i<f->count;++i)if(f->slots[i].loaded->record.id==id)return 1;return 0;}
static int deferred_prepare(void *context,const pg_document_tx_entry *entries,size_t count,uint64_t within,void **out){
 deferred_fixture *f=context;++f->prepared;f->within=within;assert(f->session.busy&&f->tx.busy&&f->tx.sealed&&f->scope.busy);*out=0;
 deferred_token *token=calloc(1,sizeof(*token));if(!token)return PG_DOC_ALLOCATION;*out=token;token->count=f->count;
 for(size_t i=0;i<f->count;++i){token->slots[i]=f->slots[i];json_object_get(token->slots[i].pending.ops);json_object_get(token->slots[i].pending.leaves);}
 for(size_t i=0;i<count;++i){const pg_document_tx_entry *entry=entries+i;if(!entry->write)continue;size_t at=0;while(at<token->count&&token->slots[at].loaded!=entry->loaded)++at;
  if(at==token->count){if(at==8)return PG_DOC_CAPACITY;token->slots[at].loaded=entry->loaded;token->slots[at].deadline=UINT64_MAX;if(pg_pending_ops_open(&token->slots[at].pending))return PG_DOC_INVALID;++token->count;}
  int status=pg_pending_ops_push(&token->slots[at].pending,entry->prepared.ops);if(status)return status;token->slots[at].base|=entry->kind==1;uint64_t deadline=UINT64_MAX;if(within!=UINT64_MAX&&f->now<=UINT64_MAX-within)deadline=f->now+within;if(deadline<token->slots[at].deadline)token->slots[at].deadline=deadline;
 }
 if(f->fail_prepare==2){f->session.failed=PG_RECORDS_STORAGE_FAILED;return PG_DOC_OK;}return f->fail_prepare?PG_DOC_ALLOCATION:PG_DOC_OK;
}
static int deferred_apply(void *context,void *candidate){
 deferred_fixture *f=context;deferred_token *token=candidate;assert(f->session.busy&&f->tx.busy);if(f->fail_apply)return 7;
 for(size_t i=0;i<f->count;++i)assert(!pg_pending_ops_close(&f->slots[i].pending));f->count=token->count;
 for(size_t i=0;i<token->count;++i){f->slots[i]=token->slots[i];memset(&token->slots[i].pending,0,sizeof(pg_pending_ops));}token->count=0;++f->applied;
 for(size_t i=0;i<f->count;++i)assert(!pg_document_cache_touch(&f->cache,&f->slots[i].loaded->record.address));return 0;
}
static void deferred_discard(void *context,void *candidate){deferred_fixture *f=context;assert(f->session.busy&&f->scope.busy);++f->discarded;deferred_token *token=candidate;if(token){for(size_t i=0;i<token->count;++i)assert(!pg_pending_ops_close(&token->slots[i].pending));free(token);}}
static void deferred_observe(void *context,const pg_document_tx_entry *entries,size_t count){
 deferred_fixture *f=context;assert(f->session.busy&&f->tx.sealed&&f->scope.busy&&f->count);pg_record_tx other={0};assert(pg_record_tx_begin(&other,&f->session,&f->memory,tx_read)==PG_TX_INVALID);
 for(size_t i=0;i<count;++i)if(entries[i].write)assert(entries[i].loaded->stored_version==entries[i].adoption.stored_version&&entries[i].loaded->deltas_since_base==entries[i].adoption.deltas_since_base);++f->observed;
}
static int deferred_adopt(void *context,const struct json_object *writes,uint64_t seq){deferred_fixture *f=context;assert(writes&&seq==f->memory.next_sequence-1);++f->adopted;if(f->flushing)for(size_t i=0;i<f->count;++i){if(f->flush_base[i])f->slots[i].loaded->deltas_since_base=0;else ++f->slots[i].loaded->deltas_since_base;}return 0;}
static void deferred_publish(void *context,const struct json_object *writes,uint64_t seq){deferred_fixture *f=context;assert(writes&&f->session.sequence==seq);++f->published;if(f->flushing){size_t count=f->count;f->count=0;for(size_t i=0;i<count;++i){assert(!pg_pending_ops_close(&f->slots[i].pending)&&!pg_document_cache_touch(&f->cache,&f->slots[i].loaded->record.address));}}}
static int deferred_checkpoint(void *context,const struct json_object *value,const struct json_object *ops,uint64_t deltas){deferred_fixture *f=context;assert(value&&ops&&(deltas==1||deltas==UINT64_C(9007199254740991))&&f->tx.sealed);return (int)f->checkpoint;}
static pg_document_deferred deferred_service(deferred_fixture *f){return (pg_document_deferred){f,deferred_prepare,deferred_apply,deferred_discard,deferred_observe};}
static void deferred_open(deferred_fixture *f,unsigned scope,unsigned history,unsigned fork){
 memset(f,0,sizeof(*f));f->now=100;assert(!pg_record_memory_open(&f->memory));char text[1024];const char *scope_text=scope==PG_DOC_SESSION?"{\"kind\":\"session\"}":scope==PG_DOC_TASK?"{\"kind\":\"task\",\"taskId\":100}":"{\"kind\":\"conversation\",\"conversationId\":1}";char policies[96]="";if(scope==PG_DOC_CONVERSATION){int n=snprintf(policies,sizeof(policies),",\"history\":\"%s\",\"fork\":\"%s\"",history==PG_DOC_LATEST?"latest":"rewindable",fork==PG_DOC_INITIAL?"initial":fork==PG_DOC_CURRENT?"current":"asOf");assert(n>0&&(size_t)n<sizeof(policies));}
 int n=snprintf(text,sizeof(text),"[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"task\",\"value\":{\"id\":100,\"conversationId\":1,\"state\":{\"status\":\"running\"}}},{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"defer\",\"scope\":%s%s},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"\"}}}]",scope_text,policies);assert(n>0&&(size_t)n<sizeof(text)&&!doc_commit(&f->memory,text)&&!doc_commit(&f->memory,"[{\"type\":\"document.change\",\"id\":2,\"content\":{\"kind\":\"delta\",\"version\":1,\"ops\":[[\"a\",[\"text\"],\"seed\"]]}}]"));
 pg_document_storage storage;pg_record_memory_documents(&f->memory,&storage);pg_document_cache_hooks hooks={f,deferred_clock,0,deferred_pinned};assert(!pg_document_cache_open(&f->cache,&storage,&hooks,0));f->session=(pg_record_session){.storage=pg_record_memory_storage(&f->memory),.sequence=2,.context=f,.adopt=deferred_adopt,.publish=deferred_publish};
 f->definition=(pg_document_definition){.kind={"defer",5},.version=1,.scope=scope,.history=history,.fork=fork};f->address=(pg_document_address){.kind={"defer",5},.owner=scope==PG_DOC_SESSION?0:scope==PG_DOC_TASK?100:1,.scope=scope};
}
static void deferred_edit(deferred_fixture *f,const char *suffix){
 assert(!pg_record_tx_begin(&f->tx,&f->session,&f->memory,tx_read)&&!pg_document_tx_open(&f->scope,&f->tx,&f->cache));pg_document_change *edit=0;assert(!pg_document_tx_get(&f->scope,&f->definition,&f->address,f,deferred_checkpoint,&edit));if(suffix){struct json_object *op=json_object_new_array(),*tuple=json_object_new_array(),*path=json_tokener_parse("[\"text\"]");assert(op&&tuple&&path&&!json_object_array_add(tuple,json_object_new_string("a"))&&!json_object_array_add(tuple,path)&&!json_object_array_add(tuple,json_object_new_string(suffix))&&!json_object_array_add(op,tuple)&&!pg_document_change_append(edit,op));json_object_put(op);}
}
static void deferred_flush(deferred_fixture *f){
 struct json_object *writes=json_object_new_array();assert(writes);for(size_t i=0;i<f->count;++i){pg_document_held_content content={0};struct json_object *empty=json_object_new_array();assert(empty&&!pg_document_store_held(&f->slots[i].pending,empty,f->slots[i].loaded->value,f->slots[i].loaded->value_version,f->slots[i].base,&content));json_object_put(empty);f->flush_base[i]=!strcmp(json_object_get_string(json_object_object_get(content.content,"kind")),"base");struct json_object *write=json_object_new_object();assert(write&&!json_object_object_add(write,"type",json_object_new_string("document.change"))&&!json_object_object_add(write,"id",json_object_new_int64((int64_t)f->slots[i].loaded->record.id))&&!json_object_object_add(write,"content",content.content)&&!json_object_array_add(writes,write));json_object_put(content.ops);}
 f->flushing=1;assert(!pg_records_commit(&f->session,writes)&&!f->count);f->flushing=0;json_object_put(writes);
}
static void deferred_close(deferred_fixture *f){for(size_t i=0;i<f->count;++i)assert(!pg_pending_ops_close(&f->slots[i].pending));f->count=0;assert(!pg_document_cache_close(&f->cache)&&!pg_record_memory_close(&f->memory));}
typedef struct {deferred_fixture *fixture;unsigned mode,prepared,adopted,published,discarded;struct json_object *notice,*stamp;uint64_t sequence;pg_session_publications publications;} notification_fixture;
static int notification_prepare(void *ctx,pg_record_tx *tx,const pg_document_tx_entry *entries,size_t count,void **token){notification_fixture *f=ctx;assert(tx->busy&&tx->sealed&&count==1&&entries[0].prepared.ops&&entries[0].loaded);++f->prepared;*token=f;struct json_object *record=0;assert(!json_object_deep_copy(entries[0].loaded->record_owner,&record,0));unsigned flags=PG_NOTICE_PUBLISHES;if(entries[0].reserved&2){flags|=PG_NOTICE_RETIRE;assert(!json_object_object_add(record,"retiredAt",json_object_new_int64(0)));}pg_document_publication_request request={record,0,entries[0].prepared.value,entries[0].prepared.ops,1,entries[0].loaded->value_version,flags,0};int status=pg_document_publication_prepare(&request,&f->notice);json_object_put(record);if(status)return PG_DOC_ALLOCATION;struct json_object *documents=json_object_new_array();assert(documents&&!json_object_array_add(documents,json_object_get(f->notice)));pg_session_publication_request publication={tx->writes,documents,documents,1,0,0};assert(!pg_session_publications_prepare(&publication,&f->publications));json_object_put(documents);if(flags&PG_NOTICE_RETIRE)f->stamp=json_object_object_get(json_object_object_get(json_object_array_get_idx(json_object_object_get(f->publications.observed,"changes"),0),"record"),"retiredAt");return f->mode==1?PG_DOC_INITIAL_FAILED:0;}
static int notification_adopt(void *ctx,void *token,uint64_t seq){notification_fixture *f=ctx;assert(token==f&&f->notice&&!f->published);++f->adopted;f->sequence=seq;assert(!pg_session_publications_confirm(&f->publications,seq));return f->mode==2?1:0;}
static void notification_publish(void *ctx,void *token,const struct json_object *writes,uint64_t seq){notification_fixture *f=ctx;assert(token==f&&f->adopted&&f->sequence==seq&&json_object_array_length(writes));++f->published;assert(json_object_get_uint64(json_object_object_get(f->publications.stored,"seq"))==seq);if(f->stamp)assert(json_object_get_uint64(f->stamp)==seq);else assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(f->notice,"value"),"text")),"seednotice"));}
static void notification_discard(void *ctx,void *token){notification_fixture *f=ctx;assert(token==f);++f->discarded;pg_session_publications_discard(&f->publications);json_object_put(f->notice);f->notice=0;f->stamp=0;}
static int notification_storage_failure(void *ctx,const struct json_object *writes,pg_records_adopt adopt,void *owner,uint64_t *seq){(void)ctx;(void)writes;(void)adopt;(void)owner;(void)seq;return 7;}
static void document_notification_pipeline_tests(void){
 _Static_assert(sizeof(pg_document_notifications)==40,"notification host ABI");
 for(unsigned mode=0;mode<5;++mode){deferred_fixture f;deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);notification_fixture fixture={.fixture=&f,.mode=mode};pg_document_notifications host={&fixture,notification_prepare,notification_adopt,notification_publish,notification_discard};deferred_edit(&f,"notice");if(mode==3)assert(!pg_document_tx_retire(&f.scope,&f.definition,&f.address));if(mode==4)f.session.storage.commit=notification_storage_failure;uint64_t seq=f.memory.next_sequence;int status=pg_document_tx_observed(&f.scope,0,0,&host);assert(fixture.prepared==1&&fixture.discarded==1&&!fixture.notice&&!f.tx.busy&&!f.scope.entries);if(mode==1){assert(status!=PG_TX_OK&&!fixture.adopted&&!fixture.published&&!f.session.failed&&f.memory.next_sequence==seq);}else if(mode==4){assert(status==PG_TX_COMMIT_FAILED&&!fixture.adopted&&!fixture.published&&f.session.failed==PG_RECORDS_STORAGE_FAILED&&f.memory.next_sequence==seq);}else if(mode==2){assert(status==PG_TX_COMMIT_FAILED&&fixture.adopted==1&&!fixture.published&&f.session.failed==PG_RECORDS_ADOPTION_FAILED&&f.memory.next_sequence==seq+1);}else{assert(!status&&fixture.adopted==1&&fixture.published==1&&!f.session.failed&&f.memory.next_sequence==seq+1);if(mode==3)assert(!f.cache.count);}deferred_close(&f);}
 puts("PASS: real DocTx notifications prepare before Storage, sequence adoption, publish, retirement ownership and failure cleanup");
}
static void deferred_memory_transactions(void){
 _Static_assert(sizeof(pg_document_deferred)==40,"Deferred host ABI");deferred_fixture f;deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);pg_document_deferred host=deferred_service(&f);
 deferred_edit(&f,"a");assert(!pg_document_tx_deferred(&f.scope,0,&host,500)&&f.count==1&&f.observed==1&&!f.adopted&&!f.published&&f.session.sequence==2&&f.memory.next_sequence==3&&f.slots[0].deadline==600&&!f.cache.idle_head);
 f.now=150;deferred_edit(&f,"b");f.checkpoint=1;assert(!pg_document_tx_deferred(&f.scope,0,&host,1000)&&f.count==1&&f.slots[0].base&&f.slots[0].deadline==600&&f.slots[0].loaded->deltas_since_base==1&&!strcmp(json_object_get_string(json_object_object_get(f.slots[0].loaded->value,"text")),"seedab"));
 f.now=200;deferred_edit(&f,"c");assert(!pg_document_tx_deferred(&f.scope,0,&host,50)&&f.slots[0].deadline==250&&f.within==50&&pg_pending_ops_size(&f.slots[0].pending)==1);deferred_edit(&f,0);assert(!pg_document_tx_deferred(&f.scope,0,&host,0)&&f.observed==3&&f.prepared==3);
 deferred_edit(&f,"d");assert(!pg_document_tx_deferred(&f.scope,0,&host,UINT64_MAX)&&f.within==UINT64_MAX&&f.slots[0].deadline==250&&!pg_document_cache_sweep(&f.cache)&&f.cache.count==1);
 struct json_object *stored=0;assert(!pg_record_memory_document(&f.memory,2,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),"seed"));json_object_put(stored);
 f.fail_prepare=1;deferred_edit(&f,"rejected");assert(pg_document_tx_deferred(&f.scope,0,&host,10)==PG_TX_ALLOCATION&&!f.session.failed&&f.count==1&&!strcmp(json_object_get_string(json_object_object_get(f.slots[0].loaded->value,"text")),"seedabcd"));f.fail_prepare=0;
 deferred_flush(&f);assert(f.adopted==1&&f.published==1&&f.observed==4&&f.session.sequence==3&&!f.count&&!pg_record_memory_document(&f.memory,2,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),"seedabcd")&&!json_object_get_uint64(json_object_object_get(stored,"deltasSinceBase")));json_object_put(stored);deferred_close(&f);
}
static void deferred_other_reads_and_documents(void){
 for(unsigned multiple=0;multiple<2;++multiple){deferred_fixture f;deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);const char *scope=multiple?"{\"kind\":\"conversation\",\"conversationId\":1}":"{\"kind\":\"session\"}";char write[512];int n=snprintf(write,sizeof(write),"[{\"type\":\"document.create\",\"record\":{\"id\":3,\"kind\":\"other\",\"scope\":%s%s},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"\"}}}]",scope,multiple?",\"history\":\"latest\",\"fork\":\"initial\"":"");assert(n>0&&(size_t)n<sizeof(write)&&!doc_commit(&f.memory,write));f.session.sequence=3;
  pg_document_definition other={.kind={"other",5},.version=1,.scope=multiple?PG_DOC_CONVERSATION:PG_DOC_SESSION,.history=multiple?PG_DOC_LATEST:0,.fork=multiple?PG_DOC_INITIAL:0};pg_document_address address={.kind=other.kind,.scope=other.scope,.owner=multiple?1:0};pg_document_deferred host=deferred_service(&f);f.checkpoint=multiple;deferred_edit(&f,"a");pg_document_change *edit=0;assert(!pg_document_tx_get(&f.scope,&other,&address,0,0,&edit));if(multiple){struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"x\"]]");assert(ops&&!pg_document_change_append(edit,ops));json_object_put(ops);}
  assert(!pg_document_tx_deferred(&f.scope,0,&host,100)&&f.count==1+multiple&&!f.published&&f.observed==1&&f.memory.next_sequence==4&&!pg_document_cache_sweep(&f.cache)&&f.cache.count==1+multiple);deferred_flush(&f);assert(f.session.sequence==4&&!f.count);deferred_close(&f);
 }
}
static void deferred_counter_cap(void){
 for(unsigned mixed=0;mixed<2;++mixed){deferred_fixture f;deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);pg_document_deferred host=deferred_service(&f);deferred_edit(&f,"a");pg_document_loaded *loaded=f.scope.entries[0].loaded;loaded->deltas_since_base=UINT64_C(9007199254740991);
  if(mixed){struct json_object *draft=json_tokener_parse("{\"kind\":\"mixed\"}"),*entry=0;assert(draft&&!pg_record_tx_append_entry(&f.tx,1,draft,0,tx_mint,&entry));json_object_put(draft);json_object_put(entry);}
  int status=pg_document_tx_deferred(&f.scope,0,&host,100);assert(status==(mixed?PG_TX_INVALID:PG_TX_OK)&&!f.session.failed&&f.memory.next_sequence==3&&loaded->deltas_since_base==UINT64_C(9007199254740991)&&f.observed==!mixed&&f.count==!mixed);deferred_close(&f);
 }
}
static void deferred_fallbacks(void){
 for(unsigned mode=0;mode<9;++mode){deferred_fixture f;deferred_open(&f,mode==0?PG_DOC_SESSION:mode==1?PG_DOC_TASK:PG_DOC_CONVERSATION,mode<2?0:mode==2?PG_DOC_REWINDABLE:PG_DOC_LATEST,mode<2?0:mode==3?PG_DOC_CURRENT:PG_DOC_INITIAL);pg_document_deferred host=deferred_service(&f);if(mode==4){f.definition.version=2;f.definition.migrate=migrate_document;}
  pg_document_definition fresh=f.definition;fresh.kind=(pg_slice){"fresh",5};
  deferred_edit(&f,"!");if(mode==8){struct json_object *seed=json_tokener_parse("{\"text\":\"fresh\"}");assert(seed);pg_document_initializer initial={seed,fork_replacement_initial};pg_document_address address=f.address;address.kind=fresh.kind;pg_document_request request={.definition=&fresh,.address=&address,.initializer=&initial,.mint=tx_mint};pg_document_change *edit=0;assert(!pg_document_tx_doc(&f.scope,&request,&edit));json_object_put(seed);}if(mode==5){struct json_object *draft=json_tokener_parse("{\"kind\":\"extra\"}"),*entry=0;assert(draft&&!pg_record_tx_append_entry(&f.tx,1,draft,0,tx_mint,&entry));json_object_put(draft);json_object_put(entry);}if(mode==6)assert(!pg_document_tx_retire(&f.scope,&f.definition,&f.address));if(mode==7){f.session.read_only=1;}
  int status=pg_document_tx_deferred(&f.scope,0,&host,100);assert(status==(mode==7?PG_TX_COMMIT_FAILED:PG_TX_OK)&&!f.prepared&&!f.applied&&!f.discarded&&!f.observed&&!f.count&&!f.session.failed&&f.published==(mode==7?0u:1u)&&f.memory.next_sequence==(mode==7?3u:4u));deferred_close(&f);
 }
 deferred_fixture f;deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);pg_document_deferred host=deferred_service(&f);deferred_edit(&f,"memory");f.fail_apply=1;assert(pg_document_tx_deferred(&f.scope,0,&host,0)==PG_TX_COMMIT_FAILED&&f.session.failed==PG_RECORDS_ADOPTION_FAILED&&!f.observed&&!f.adopted&&!f.published&&f.memory.next_sequence==3&&f.discarded==1);deferred_close(&f);
 deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);host=deferred_service(&f);deferred_edit(&f,"failed");f.fail_prepare=2;assert(pg_document_tx_deferred(&f.scope,0,&host,0)==PG_TX_COMMIT_FAILED&&f.session.failed==PG_RECORDS_STORAGE_FAILED&&!f.applied&&!f.observed&&!f.adopted&&!f.published&&f.memory.next_sequence==3&&f.discarded==1);deferred_close(&f);
}
#ifdef PG_MEMORY_FAULTS
static void deferred_faults(void){
 unsigned totals[2]={0};for(unsigned mode=0;mode<2;++mode)for(unsigned attempt=0;attempt<=totals[mode];++attempt){deferred_fixture f;deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);pg_document_deferred host=deferred_service(&f);if(mode){deferred_edit(&f,"a");assert(!pg_document_tx_deferred(&f.scope,0,&host,1000));}deferred_edit(&f,"b");
  doc_tracking=1;doc_calls=0;doc_fail=attempt;int status=pg_document_tx_deferred(&f.scope,0,&host,100);doc_tracking=0;if(!attempt){assert(!status);totals[mode]=doc_calls;}else{assert(status!=PG_TX_OK&&!f.session.failed&&f.count==mode&&f.observed==mode&&f.memory.next_sequence==3&&!f.session.busy&&f.scope.sealed&&!f.scope.entries);if(mode)assert(f.slots[0].deadline==1100&&!strcmp(json_object_get_string(json_object_object_get(f.slots[0].loaded->value,"text")),"seeda")&&!strcmp(json_object_get_string(json_object_array_get_idx(json_object_array_get_idx(f.slots[0].pending.ops,0),2)),"a"));}
  doc_fail=0;deferred_close(&f);
 }
}
#endif

typedef struct {deferred_fixture f;pg_document_holds holds;} native_holds_fixture;
static uint64_t native_hold_clock(void *context){return ((native_holds_fixture*)context)->f.now;}
static int native_hold_pinned(void *context,uint64_t id){return pg_document_holds_pinned(&((native_holds_fixture*)context)->holds,id);}
static void native_hold_observe(void *context,const pg_document_tx_entry *entries,size_t count){
 native_holds_fixture *n=context;assert(n->holds.busy&&n->f.session.busy&&count&&entries&&n->holds.count);++n->f.observed;
 assert(pg_document_holds_close(&n->holds)==PG_DOC_INVALID);
}
static void native_hold_open(native_holds_fixture *n,pg_document_deferred *host){
 memset(n,0,sizeof(*n));deferred_open(&n->f,PG_DOC_CONVERSATION,PG_DOC_LATEST,PG_DOC_INITIAL);
 assert(!pg_document_holds_open(&n->holds,&n->f.cache,n,native_hold_clock,native_hold_observe));
 n->f.cache.context=n;n->f.cache.pinned=native_hold_pinned;assert(!pg_document_holds_bind(&n->holds,host));
}
static void native_hold_close(native_holds_fixture *n){assert(!pg_document_holds_discard_all(&n->holds)&&!pg_document_holds_close(&n->holds));deferred_close(&n->f);}

typedef struct {struct json_object *observed,*stored;unsigned internal_calls,external_calls;} default_notification_capture;
static int default_internal(void *ctx,const void *publication,void *delivery){default_notification_capture *f=ctx;assert(delivery==f&&!f->external_calls);++f->internal_calls;f->observed=json_object_get((struct json_object*)publication);return 0;}
static int default_external(void *ctx,const void *publication,void *delivery){default_notification_capture *f=ctx;assert(delivery==f&&f->internal_calls==1);++f->external_calls;f->stored=json_object_get((struct json_object*)publication);return 0;}
static void default_notification_finish(pg_session_listeners *listeners,default_notification_capture *capture){assert(capture->internal_calls==1&&capture->external_calls==1&&!pg_session_listeners_close(listeners,capture));json_object_put(capture->observed);json_object_put(capture->stored);}
static void default_notifications_tests(void){
 _Static_assert(sizeof(pg_session_notification_host)==24,"default notification host ABI");
 pg_record_session idle_records={0},other_records={0};pg_record_tx idle_tx={.session=&idle_records};pg_session_listeners idle_listeners={.records=&idle_records};pg_session_notification_host idle_owner={&idle_listeners,0,0};pg_document_notifications idle_host={0};assert(!pg_session_notifications_bind(&idle_owner,&idle_host));void *empty_token=(void*)1;
#ifdef PG_MEMORY_FAULTS
 doc_calls=0;doc_fail=1;doc_tracking=1;
#endif
 assert(!idle_host.prepare(idle_host.context,&idle_tx,0,0,&empty_token)&&!empty_token);
#ifdef PG_MEMORY_FAULTS
 doc_tracking=0;doc_fail=0;assert(!doc_calls);
#endif
 assert(!idle_host.adopt(idle_host.context,0,3));idle_host.publish(idle_host.context,0,0,3);idle_host.discard(idle_host.context,0);idle_listeners.records=&other_records;assert(idle_host.prepare(idle_host.context,&idle_tx,0,0,&empty_token)==PG_DOC_INVALID&&!empty_token);

 for(unsigned scope=0;scope<3;++scope)for(unsigned retired=0;retired<2;++retired){deferred_fixture f;deferred_open(&f,scope,scope==PG_DOC_CONVERSATION?PG_DOC_LATEST:0,scope==PG_DOC_CONVERSATION?PG_DOC_INITIAL:0);default_notification_capture capture={0};pg_session_listeners listeners={.records=&f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,default_external,&capture));pg_session_notification_host owner={&listeners,0,&capture};pg_document_notifications host={0};assert(!pg_session_notifications_bind(&owner,&host));deferred_edit(&f,"default");if(retired)assert(!pg_document_tx_retire(&f.scope,&f.definition,&f.address));assert(!pg_document_tx_observed(&f.scope,0,0,&host));assert(capture.observed==capture.stored);struct json_object *changes=json_object_object_get(capture.observed,"changes"),*notice=json_object_array_get_idx(changes,0),*record=json_object_object_get(notice,"record");assert(json_object_array_length(changes)==1&&json_object_get_int(json_object_object_get(capture.observed,"seq"))==3&&json_object_get_int(json_object_object_get(record,"createdAt"))==1);if(scope==PG_DOC_SESSION)assert(!json_object_object_get(notice,"conversationId"));else assert(json_object_get_int(json_object_object_get(notice,"conversationId"))==1);if(retired)assert(json_object_get_int(json_object_object_get(record,"retiredAt"))==3&&!json_object_object_get(notice,"value"));else assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(notice,"value"),"text")),"seeddefault"));default_notification_finish(&listeners,&capture);deferred_close(&f);}
 for(unsigned base=0;base<2;++base)for(unsigned own=0;own<2;++own){native_holds_fixture n;pg_document_deferred deferred;native_hold_open(&n,&deferred);n.f.checkpoint=base;deferred_edit(&n.f,"held");assert(!pg_document_tx_deferred(&n.f.scope,0,&deferred,100));n.f.checkpoint=0;default_notification_capture capture={0};pg_session_listeners listeners={.records=&n.f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,default_external,&capture));pg_session_notification_host owner={&listeners,&n.holds,&capture};pg_document_notifications host={0};pg_document_merge merge={0};assert(!pg_session_notifications_bind(&owner,&host)&&!pg_document_holds_merge_bind(&n.holds,&merge));deferred_edit(&n.f,own?"own":0);struct json_object *draft=json_tokener_parse("{\"kind\":\"tick\"}"),*entry=0;assert(!pg_record_tx_append_entry(&n.f.tx,1,draft,0,tx_mint,&entry));json_object_put(draft);json_object_put(entry);int result=pg_document_tx_observed(&n.f.scope,0,&merge,&host);assert(!result&&!n.holds.count);struct json_object *observed=json_object_object_get(capture.observed,"changes"),*stored=json_object_object_get(capture.stored,"changes");assert(capture.observed!=capture.stored&&json_object_array_length(observed)==own+1&&json_object_array_length(stored)==2);struct json_object *stored_ops=json_object_object_get(json_object_array_get_idx(stored,1),"ops");assert(json_object_array_length(stored_ops)==1&&!strcmp(json_object_get_string(json_object_array_get_idx(json_object_array_get_idx(stored_ops,0),2)),own?"heldown":"held"));if(own){struct json_object *ops=json_object_object_get(json_object_array_get_idx(observed,1),"ops");assert(!strcmp(json_object_get_string(json_object_array_get_idx(json_object_array_get_idx(ops,0),2)),"own"));}default_notification_finish(&listeners,&capture);native_hold_close(&n);}
 puts("PASS: default DocTx notification host, Session/conversation/task ownership, retirement and held observed/stored ops");
}
static void default_creation_copy_notifications(void){
 for(unsigned mode=0;mode<3;++mode){deferred_fixture f;deferred_open(&f,PG_DOC_CONVERSATION,PG_DOC_LATEST,mode==2?PG_DOC_CURRENT:PG_DOC_INITIAL);if(mode==2){assert(!doc_commit(&f.memory,"[{\"type\":\"conversation\",\"value\":{\"id\":50}}]"));f.session.sequence=3;}default_notification_capture capture={0};pg_session_listeners listeners={.records=&f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,default_external,&capture));pg_session_notification_host owner={&listeners,0,&capture};pg_document_notifications host={0};assert(!pg_session_notifications_bind(&owner,&host)&&!pg_record_tx_begin(&f.tx,&f.session,&f.memory,tx_read)&&!pg_document_tx_open(&f.scope,&f.tx,&f.cache));pg_document_definition definition=f.definition;definition.kind=(pg_slice){"fresh",5};pg_document_address address=f.address;address.kind=definition.kind;
 if(mode<2){struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}");pg_document_initializer initializer={seed,fork_replacement_initial};pg_document_request request={.definition=&definition,.address=&address,.initializer=&initializer,.mint=tx_mint};pg_document_change *edit=0;assert(!pg_document_tx_doc(&f.scope,&request,&edit));json_object_put(seed);if(mode)assert(!pg_document_tx_retire(&f.scope,&definition,&address));}else{struct json_object *write=json_tokener_parse("{\"type\":\"document.copy\",\"record\":{\"id\":200,\"kind\":\"defer\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":50},\"history\":\"latest\",\"fork\":\"current\"},\"source\":{\"id\":2,\"at\":\"current\"}}");assert(write&&!pg_record_tx_stage(&f.tx,PG_TX_DOCUMENT_COPY,write));json_object_put(write);}uint64_t seq=f.memory.next_sequence;assert(!pg_document_tx_observed(&f.scope,0,0,&host));struct json_object *changes=json_object_object_get(capture.observed,"changes"),*notice=json_object_array_get_idx(changes,0),*record=json_object_object_get(notice,"record");assert(json_object_array_length(changes)==1&&json_object_get_uint64(json_object_object_get(record,"createdAt"))==seq);if(mode==1)assert(json_object_get_uint64(json_object_object_get(record,"retiredAt"))==seq);if(mode==2)assert(!strcmp(json_object_get_string(json_object_object_get(notice,"type")),"document.copy")&&json_object_get_int(json_object_object_get(notice,"conversationId"))==50&&json_object_get_int(json_object_object_get(json_object_object_get(notice,"source"),"id"))==2);else assert(json_object_array_length(json_object_object_get(notice,"ops"))==0);default_notification_finish(&listeners,&capture);deferred_close(&f);}
}
static void default_notification_faults(void){
#ifdef PG_MEMORY_FAULTS
 for(unsigned held=0;held<2;++held){unsigned total=0,failures=0;for(unsigned target=0;target<=total;++target){native_holds_fixture n;pg_document_deferred deferred;native_hold_open(&n,&deferred);if(held){deferred_edit(&n.f,"held");assert(!pg_document_tx_deferred(&n.f.scope,0,&deferred,100));}default_notification_capture capture={0};pg_session_listeners listeners={.records=&n.f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,default_external,&capture));pg_session_notification_host owner={&listeners,&n.holds,&capture};pg_document_notifications host={0};pg_document_merge merge={0};assert(!pg_session_notifications_bind(&owner,&host)&&!pg_document_holds_merge_bind(&n.holds,&merge));deferred_edit(&n.f,"own");uint64_t before=n.f.memory.next_sequence;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_tx_observed(&n.f.scope,0,held?&merge:0,&host);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!n.f.tx.busy&&!n.holds.busy&&!n.f.scope.entries);if(target){assert(status!=PG_TX_OK||n.f.session.failed);if(n.f.memory.next_sequence==before)assert(n.holds.count==held&&!capture.internal_calls&&!capture.external_calls);++failures;}else{assert(!status&&!n.f.session.failed&&capture.internal_calls==1&&capture.external_calls==1);total=calls;}assert(!pg_session_listeners_close(&listeners,&capture));json_object_put(capture.observed);json_object_put(capture.stored);native_hold_close(&n);}printf("PASS: default notification %s settlement, %u allocation/insert/clone failures and partial cleanup\n",held?"held":"ordinary",failures);}
#endif
}
static void default_deferred_notification_faults(void){
#ifdef PG_MEMORY_FAULTS
 for(unsigned mode=0;mode<3;++mode){unsigned total=0,failures=0;for(unsigned target=0;target<=total;++target){native_holds_fixture n;pg_document_deferred deferred;native_hold_open(&n,&deferred);if(mode==2){deferred_edit(&n.f,"held");assert(!pg_document_tx_deferred(&n.f.scope,0,&deferred,100));}default_notification_capture capture={0};pg_session_listeners listeners={.records=&n.f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,default_external,&capture));pg_session_notification_host owner={&listeners,&n.holds,&capture};pg_document_notifications host={0};pg_document_merge merge={0};assert(!pg_session_deferred_notifications_bind(&owner,&host)&&!pg_document_holds_merge_bind(&n.holds,&merge));deferred_edit(&n.f,"own");if(mode==1){struct json_object *draft=json_tokener_parse("{\"kind\":\"tick\"}"),*entry=0;assert(!pg_record_tx_append_entry(&n.f.tx,1,draft,0,tx_mint,&entry));json_object_put(draft);json_object_put(entry);}doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_tx_deferred_observed(&n.f.scope,0,&deferred,100,&merge,&host);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!n.f.tx.busy&&!n.holds.busy&&!n.f.scope.entries);if(target){assert(status!=PG_TX_OK||n.f.session.failed);if(!n.f.session.failed)assert(n.holds.count==(mode==2)&&!capture.internal_calls&&!capture.external_calls);++failures;}else{assert(!status&&!n.f.session.failed&&capture.internal_calls==1&&capture.external_calls==(mode==1)&&n.holds.count==(mode!=1));total=calls;}assert(!pg_session_listeners_close(&listeners,&capture));json_object_put(capture.observed);json_object_put(capture.stored);native_hold_close(&n);}printf("PASS: deferred notification mode %u, %u allocation/insert/clone failures and partial cleanup\n",mode,failures);}
#endif
}
static int held_external_notice(void *ctx,const void *publication,void *delivery){default_notification_capture *f=ctx;assert(delivery==f&&!f->internal_calls&&!f->external_calls);++f->external_calls;f->stored=json_object_get((struct json_object*)publication);return 0;}
static void held_notification_tests(void){
 _Static_assert(sizeof(pg_hold_notifications)==56,"hold notification ABI");
 for(unsigned base=0;base<2;++base)for(unsigned selection=0;selection<3;++selection){native_holds_fixture n;pg_document_deferred deferred;native_hold_open(&n,&deferred);n.f.checkpoint=base;deferred_edit(&n.f,"held");assert(!pg_document_tx_deferred(&n.f.scope,0,&deferred,100));default_notification_capture capture={0};pg_session_listeners listeners={.records=&n.f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,held_external_notice,&capture));pg_session_notification_host owner={&listeners,&n.holds,&capture};pg_hold_notifications notifications={0};assert(!pg_session_hold_notifications_bind(&owner,&notifications));uint64_t seq=n.f.memory.next_sequence;assert(!pg_document_holds_flush_observed(&n.holds,&n.f.session,selection==2?99:selection==1?2:0,&notifications));assert(!capture.internal_calls&&n.f.observed==1);if(selection==2)assert(!capture.external_calls&&n.holds.count==1&&n.f.memory.next_sequence==seq);else{assert(capture.external_calls==1&&!n.holds.count&&n.f.memory.next_sequence==seq+1&&json_object_get_uint64(json_object_object_get(capture.stored,"seq"))==seq&&!json_object_object_get(capture.stored,"kind"));struct json_object *notice=json_object_array_get_idx(json_object_object_get(capture.stored,"changes"),0),*ops=json_object_object_get(notice,"ops");assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(notice,"value"),"text")),"seedheld")&&!strcmp(json_object_get_string(json_object_array_get_idx(json_object_array_get_idx(ops,0),2)),"held"));}assert(!pg_session_listeners_close(&listeners,&capture));json_object_put(capture.stored);native_hold_close(&n);}
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;for(unsigned target=0;target<=total;++target){native_holds_fixture n;pg_document_deferred deferred;native_hold_open(&n,&deferred);deferred_edit(&n.f,"held");assert(!pg_document_tx_deferred(&n.f.scope,0,&deferred,100));default_notification_capture capture={0};pg_session_listeners listeners={.records=&n.f.session};assert(!pg_session_listen(&listeners,1,held_external_notice,&capture));pg_session_notification_host owner={&listeners,&n.holds,&capture};pg_hold_notifications notifications={0};assert(!pg_session_hold_notifications_bind(&owner,&notifications));uint64_t seq=n.f.memory.next_sequence;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_holds_flush_observed(&n.holds,&n.f.session,0,&notifications);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!n.holds.busy&&!n.f.session.busy);if(target){assert(status!=PG_RECORDS_OK||n.f.session.failed);if(n.f.memory.next_sequence==seq)assert(n.holds.count==1&&!capture.external_calls);++failures;}else{assert(!status&&!n.holds.count&&capture.external_calls==1&&!n.f.session.failed);total=calls;}assert(!pg_session_listeners_close(&listeners,&capture));json_object_put(capture.stored);native_hold_close(&n);}printf("PASS: held-only durable notifications, selected/missing/base/delta and %u allocation/insert/clone failures\n",failures);
#endif
}
typedef struct {native_holds_fixture *n;unsigned adopted,published,mode;} native_flush_fixture;
static int native_flush_adopt(void *context,const struct json_object *writes,uint64_t seq){
 native_flush_fixture *f=context;native_holds_fixture *n=f->n;assert(writes&&seq&&n->holds.busy&&n->f.session.busy&&n->holds.count&&!f->published);++f->adopted;
 assert(pg_document_holds_flush(&n->holds,&n->f.session)==PG_RECORDS_INVALID);
 for(size_t i=0;i<n->holds.count;++i)assert(n->holds.items[i].loaded->deltas_since_base==(n->holds.items[i].base?0:2));
 return f->mode==5;
}
static void native_flush_publish(void *context,const struct json_object *writes,uint64_t seq){
 native_flush_fixture *f=context;assert(writes&&seq&&f->adopted==1&&!f->n->holds.count&&f->n->holds.busy&&f->n->f.session.busy);++f->published;
 assert(pg_document_holds_flush(&f->n->holds,&f->n->f.session)==PG_RECORDS_INVALID);
}
static int native_flush_backend(void *context,const struct json_object *writes,pg_records_adopt adopt,void *owner,uint64_t *seq){
 native_flush_fixture *f=context;if(f->mode==1)return 1;if(f->mode==2){*seq=3;return 0;}
 pg_record_storage real=pg_record_memory_storage(&f->n->f.memory);int status=real.commit(real.context,writes,adopt,owner,seq);if(status)return status;
 if(f->mode==3)adopt(owner,*seq);if(f->mode==4)++*seq;return f->mode==6;
}
static void native_flush_tests(void){
 native_holds_fixture n;pg_document_deferred host;
 for(unsigned base=0;base<2;++base){native_hold_open(&n,&host);n.f.checkpoint=base;deferred_edit(&n.f,"a");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,10));deferred_edit(&n.f,"b");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,10));pg_document_loaded *loaded=n.holds.items[0].loaded;struct json_object *value=loaded->value;
 native_flush_fixture f={.n=&n};n.f.session.context=&f;n.f.session.adopt=native_flush_adopt;n.f.session.publish=native_flush_publish;
 assert(!pg_document_holds_flush(&n.holds,&n.f.session)&&!n.holds.count&&!n.holds.busy&&loaded->value==value&&loaded->stored_version==1&&loaded->deltas_since_base==(base?0:2)&&n.f.session.sequence==3&&f.adopted==1&&f.published==1&&n.f.observed==2&&n.f.session.context==&f&&n.f.session.adopt==native_flush_adopt&&n.f.session.publish==native_flush_publish);
 struct json_object *stored=0;assert(!pg_record_memory_document(&n.f.memory,2,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),"seedab"));json_object_put(stored);
 n.f.session.read_only=1;assert(!pg_document_holds_flush(&n.holds,&n.f.session)&&f.published==1);native_hold_close(&n);
 }
 for(unsigned mode=1;mode<=6;++mode){native_hold_open(&n,&host);deferred_edit(&n.f,"a");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,10));pg_document_loaded *loaded=n.holds.items[0].loaded;struct json_object *value=loaded->value,*ops=n.holds.items[0].pending.ops;
 native_flush_fixture f={.n=&n,.mode=mode};n.f.session.context=&f;n.f.session.adopt=native_flush_adopt;n.f.session.publish=native_flush_publish;n.f.session.storage=(pg_record_storage){&f,native_flush_backend};
 unsigned expected=(mode==1||mode==6)?PG_RECORDS_STORAGE_FAILED:PG_RECORDS_ADOPTION_FAILED;
 assert(pg_document_holds_flush(&n.holds,&n.f.session)==expected&&n.f.session.failed==expected&&n.holds.count==1&&!n.holds.busy&&!n.f.session.busy&&loaded->value==value&&n.holds.items[0].pending.ops==ops&&!f.published&&pg_document_holds_pinned(&n.holds,2)&&n.f.session.context==&f&&n.f.session.adopt==native_flush_adopt&&n.f.session.publish==native_flush_publish);
 assert(pg_document_holds_flush(&n.holds,&n.f.session)==expected);native_hold_close(&n);
 }
 native_hold_open(&n,&host);deferred_edit(&n.f,"a");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,10));n.f.session.read_only=1;assert(pg_document_holds_flush(&n.holds,&n.f.session)==PG_RECORDS_READ_ONLY&&!n.f.session.failed&&n.holds.count==1);n.f.session.read_only=0;
 pg_document_loaded *loaded=n.holds.items[0].loaded;loaded->deltas_since_base=UINT64_C(9007199254740991);assert(pg_document_holds_flush(&n.holds,&n.f.session)==PG_RECORDS_INVALID&&!n.f.session.failed&&n.holds.count==1);loaded->deltas_since_base=1;assert(!pg_document_holds_flush(&n.holds,&n.f.session));native_hold_close(&n);
#ifdef PG_MEMORY_FAULTS
 unsigned failures=0,total=0;
 for(unsigned target=0;target<=total;++target){native_hold_open(&n,&host);deferred_edit(&n.f,"a");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,10));loaded=n.holds.items[0].loaded;struct json_object *value=loaded->value,*ops=n.holds.items[0].pending.ops;void *context=n.f.session.context;
  doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_holds_flush(&n.holds,&n.f.session);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
  assert(!n.holds.busy&&!n.f.session.busy&&loaded->value==value&&n.f.session.context==context&&n.f.session.adopt==deferred_adopt&&n.f.session.publish==deferred_publish);
  if(target){assert(status!=PG_RECORDS_OK&&n.holds.count==1&&n.holds.items[0].pending.ops==ops&&pg_document_holds_pinned(&n.holds,2)&&loaded->deltas_since_base==1);assert(n.f.session.failed==0||n.f.session.failed==PG_RECORDS_STORAGE_FAILED);++failures;}
  else {assert(!status&&!n.holds.count&&loaded->deltas_since_base==2);total=calls;}
  native_hold_close(&n);
 }
 printf("PASS: native holds flush, durable counters/observers, fail-stop and %u direct allocation/insert/clone failures\n",failures);
#endif
}
static void native_holds_tests(void){
 _Static_assert(sizeof(pg_document_holds)==56&&sizeof(pg_document_hold)==56,"holds ABI");
 native_holds_fixture n;pg_document_deferred host;native_hold_open(&n,&host);
 assert(pg_document_holds_deadline(&n.holds)==UINT64_MAX&&!pg_document_holds_pinned(&n.holds,2));
 deferred_edit(&n.f,"a");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,500));
 pg_document_loaded *loaded=n.holds.items[0].loaded;
 assert(n.holds.count==1&&n.f.observed==1&&pg_document_holds_pinned(&n.holds,2)&&pg_document_holds_deadline(&n.holds)==600);
 assert(pg_document_holds_close(&n.holds)==PG_DOC_SEMANTICS);
 n.f.now=200;n.f.checkpoint=1;deferred_edit(&n.f,"b");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,100));
 n.f.now=250;n.f.checkpoint=0;deferred_edit(&n.f,"c");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,UINT64_MAX));
 assert(n.holds.count==1&&n.holds.items[0].base&&pg_document_holds_deadline(&n.holds)==300&&n.f.observed==3&&!n.f.adopted&&!n.f.published&&n.f.session.sequence==2);
 assert(!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"seedabc")&&loaded->deltas_since_base==1);
 assert(!pg_document_cache_sweep(&n.f.cache)&&n.f.cache.count==1);
 assert(!pg_document_holds_flush(&n.holds,&n.f.session)&&!n.holds.count&&loaded->deltas_since_base==0&&n.f.adopted==1&&n.f.published==1&&n.f.observed==3&&n.f.session.sequence==3);
 assert(!pg_document_holds_pinned(&n.holds,2)&&pg_document_holds_deadline(&n.holds)==UINT64_MAX);
 assert(pg_document_holds_release(&n.holds,loaded)==PG_DOC_MISSING);native_hold_close(&n);
 native_hold_open(&n,&host);
 pg_document_definition definitions[24];pg_document_address addresses[24];char names[24][24];
 for(unsigned i=0;i<24;++i){int len=snprintf(names[i],sizeof(names[i]),"hold.%u",i);assert(len>0);char text[512];int size=snprintf(text,sizeof(text),"[{\"type\":\"document.create\",\"record\":{\"id\":%u,\"kind\":\"%s\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"latest\",\"fork\":\"initial\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"\"}}}]",i+3,names[i]);assert(size>0&&(size_t)size<sizeof(text)&&!doc_commit(&n.f.memory,text));definitions[i]=n.f.definition;definitions[i].kind=(pg_slice){names[i],(size_t)len};addresses[i]=n.f.address;addresses[i].kind=definitions[i].kind;}
 n.f.session.sequence=n.f.memory.next_sequence-1;
 for(unsigned round=0;round<2;++round){assert(!pg_record_tx_begin(&n.f.tx,&n.f.session,&n.f.memory,tx_read)&&!pg_document_tx_open(&n.f.scope,&n.f.tx,&n.f.cache));
  for(unsigned i=0;i<24;++i){pg_document_change *change=0;assert(!pg_document_tx_get(&n.f.scope,&definitions[i],&addresses[i],0,0,&change));struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"x\"]]");assert(ops&&!pg_document_change_append(change,ops));json_object_put(ops);}
  assert(!pg_document_tx_deferred(&n.f.scope,0,&host,round?50:100)&&n.holds.count==24&&n.f.cache.count==24&&!n.holds.busy);
 }
 assert(pg_document_holds_deadline(&n.holds)==150);
 for(unsigned i=0;i<24;++i){assert(pg_document_holds_pinned(&n.holds,i+3)&&n.holds.items[i].loaded->record.id==i+3&&!strcmp(json_object_get_string(json_object_object_get(n.holds.items[i].loaded->value,"text")),"xx")&&pg_pending_ops_size(&n.holds.items[i].pending)==1);}
 void *candidate=0,*refused=0;assert(!host.prepare(host.context,0,0,0,&candidate)&&candidate&&n.holds.busy);assert(host.prepare(host.context,0,0,0,&refused)==PG_DOC_INVALID&&!refused);host.discard(host.context,refused);assert(n.holds.busy);host.discard(host.context,candidate);assert(!n.holds.busy&&n.holds.count==24&&pg_document_holds_deadline(&n.holds)==150);
 pg_document_loaded *retained[24];for(unsigned i=0;i<24;++i)retained[i]=n.holds.items[i].loaded;
 assert(!pg_document_holds_flush(&n.holds,&n.f.session)&&!n.holds.count&&n.f.published==1&&n.f.adopted==1&&n.f.observed==2);
 for(unsigned i=0;i<24;++i){struct json_object *stored=0;assert(retained[i]->deltas_since_base==1&&!pg_document_holds_pinned(&n.holds,i+3)&&!pg_record_memory_document(&n.f.memory,i+3,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),"xx"));json_object_put(stored);}
 native_hold_close(&n);
 for(unsigned mode=0;mode<2;++mode){native_hold_open(&n,&host);n.f.now=UINT64_MAX-5;deferred_edit(&n.f,"x");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,mode?UINT64_MAX:10)&&pg_document_holds_deadline(&n.holds)==UINT64_MAX);native_hold_close(&n);}
#ifdef PG_MEMORY_FAULTS
 unsigned failures=0;
 for(unsigned existing=0;existing<2;++existing){unsigned total=0;
 for(unsigned target=0;target<=total;++target){native_hold_open(&n,&host);
  if(existing){deferred_edit(&n.f,"a");assert(!pg_document_tx_deferred(&n.f.scope,0,&host,500));}
  deferred_edit(&n.f,"b");loaded=n.f.scope.entries[0].loaded;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_tx_deferred(&n.f.scope,0,&host,1);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
  assert(!n.holds.busy&&!n.f.session.failed);
  if(target){assert(status!=PG_TX_OK&&n.holds.count==existing&&pg_document_holds_pinned(&n.holds,2)==(int)existing&&pg_document_holds_deadline(&n.holds)==(existing?600:UINT64_MAX)&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),existing?"seeda":"seed"));++failures;}
  else {assert(!status&&n.holds.count==1&&pg_document_holds_pinned(&n.holds,2)&&pg_document_holds_deadline(&n.holds)==101);total=calls;}
  native_hold_close(&n);
 }}
 printf("PASS: native holds accumulation, pins, deadlines, confirmed release; %u direct allocation/insert/clone failures\n",failures);
#endif
}


typedef struct {native_holds_fixture *n;struct json_object *batch;unsigned adopts,publishes,fail;} native_merge_fixture;
static int native_merge_adopt(void *context,const struct json_object *writes,uint64_t seq){native_merge_fixture *f=context;assert(writes&&seq&&f->n->f.session.busy&&f->n->holds.busy);++f->adopts;return f->fail;}
static void native_merge_publish(void *context,const struct json_object *writes,uint64_t seq){native_merge_fixture *f=context;assert(writes&&seq&&f->adopts==1&&f->n->f.session.busy&&f->n->holds.busy);++f->publishes;f->batch=json_object_get((struct json_object*)writes);}
static void native_merge_entry(deferred_fixture *f){struct json_object *entry=json_tokener_parse("{\"type\":\"entry\",\"value\":{\"id\":30,\"conversationId\":1,\"kind\":\"derived\"}}");assert(entry&&!pg_record_tx_stage(&f->tx,PG_TX_ENTRY,entry));json_object_put(entry);}
static void native_merge_open(native_holds_fixture *n,pg_document_deferred *deferred,pg_document_merge *merge,unsigned base){native_hold_open(n,deferred);n->f.checkpoint=base;deferred_edit(&n->f,"held");assert(!pg_document_tx_deferred(&n->f.scope,0,deferred,100)&&!pg_document_holds_merge_bind(&n->holds,merge));n->f.checkpoint=0;}

static const pg_document_definition merge_other_def={.kind={"other",5},.version=1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_LATEST,.fork=PG_DOC_INITIAL};
static const pg_document_address merge_other_address={.kind={"other",5},.owner=1,.scope=PG_DOC_CONVERSATION};
static void native_merge_other_get(native_holds_fixture *n,unsigned edit){pg_document_change *change=0;assert(!pg_document_tx_get(&n->f.scope,&merge_other_def,&merge_other_address,0,0,&change));if(edit){struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"b\"]]");assert(ops&&!pg_document_change_append(change,ops));json_object_put(ops);}}
static void native_merge_other_hold(native_holds_fixture *n,pg_document_deferred *host){assert(!doc_commit(&n->f.memory,"[{\"type\":\"document.create\",\"record\":{\"id\":3,\"kind\":\"other\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"latest\",\"fork\":\"initial\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"\"}}}]"));n->f.session.sequence=n->f.memory.next_sequence-1;assert(!pg_record_tx_begin(&n->f.tx,&n->f.session,&n->f.memory,tx_read)&&!pg_document_tx_open(&n->f.scope,&n->f.tx,&n->f.cache));native_merge_other_get(n,1);assert(!pg_document_tx_deferred(&n->f.scope,0,host,500)&&n->holds.count==2);}
static void native_merge_tests(void){
 _Static_assert(sizeof(pg_document_merge)==32,"merge service ABI");
 for(unsigned mode=0;mode<8;++mode){native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,mode==7);pg_document_loaded *loaded=n.holds.items[0].loaded;
 native_merge_fixture f={.n=&n};n.f.session.context=&f;n.f.session.adopt=native_merge_adopt;n.f.session.publish=native_merge_publish;
 if(mode==4||mode==2){assert(!pg_record_tx_begin(&n.f.tx,&n.f.session,&n.f.memory,tx_read)&&!pg_document_tx_open(&n.f.scope,&n.f.tx,&n.f.cache));}
 else deferred_edit(&n.f,(mode==1||mode==3)?0:"+now");
 if(mode==1||mode==2)native_merge_entry(&n.f);
 if(mode==4||mode==5)assert(!pg_document_tx_retire(&n.f.scope,&n.f.definition,&n.f.address));
 if(mode==6)n.f.checkpoint=1;
 assert(!pg_document_tx_merge(&n.f.scope,0,&merge)&&!n.holds.busy&&!n.f.session.busy&&!n.f.session.failed&&n.f.session.context==&f&&n.f.session.adopt==native_merge_adopt&&n.f.session.publish==native_merge_publish);
 if(mode==2||mode==3){assert(n.holds.count==1&&n.holds.items[0].loaded==loaded&&loaded->deltas_since_base==1&&n.f.session.sequence==(mode==2?3u:2u)&&f.publishes==(mode==2?1u:0u));}
 else {assert(!n.holds.count&&n.f.session.sequence==3&&f.adopts==1&&f.publishes==1&&f.batch);struct json_object *first=json_object_array_get_idx(f.batch,mode==1?1:0);assert(!strcmp(json_object_get_string(json_object_object_get(first,"type")),"document.change"));
  if(mode==4||mode==5){assert(n.f.cache.count==0&&json_object_array_length(f.batch)==2&&!strcmp(json_object_get_string(json_object_object_get(json_object_array_get_idx(f.batch,1),"type")),"document.retire"));struct json_object *record=0;assert(pg_record_memory_find_document(&n.f.memory,&n.f.address,UINT64_MAX,&record)==PG_MEMORY_MISSING&&!record);}
  else {struct json_object *stored=0;const char *expected=mode==1?"seedheld":"seedheld+now";assert(!pg_record_memory_document(&n.f.memory,2,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),expected)&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),expected)&&loaded->deltas_since_base==((mode==6||mode==7)?0:2));json_object_put(stored);}
 }
 json_object_put(f.batch);native_hold_close(&n);
 }

 for(unsigned mode=0;mode<4;++mode){native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,0);native_merge_other_hold(&n,&deferred);pg_document_loaded *other=n.holds.items[1].loaded;
 if(mode>=2){assert(!pg_record_tx_begin(&n.f.tx,&n.f.session,&n.f.memory,tx_read)&&!pg_document_tx_open(&n.f.scope,&n.f.tx,&n.f.cache)&&!pg_document_tx_retire(&n.f.scope,&n.f.definition,&n.f.address));}else deferred_edit(&n.f,mode?"+now":0);
 if(mode&1)native_merge_other_get(&n,mode==3);native_merge_entry(&n.f);
 assert(!pg_document_tx_merge(&n.f.scope,0,&merge)&&!n.holds.busy&&!n.f.session.failed&&n.holds.count==((mode&1)?0u:1u)&&n.f.published==1&&n.f.adopted==1&&n.f.observed==2);
 if(!(mode&1)){assert(n.holds.items[0].loaded==other&&pg_document_holds_pinned(&n.holds,3)&&pg_document_holds_deadline(&n.holds)==600&&other->deltas_since_base==0);assert(!pg_document_holds_flush(&n.holds,&n.f.session));}
 else assert(!pg_document_holds_pinned(&n.holds,3)&&other->deltas_since_base==1);
 native_hold_close(&n);
 }

 for(unsigned mode=0;mode<3;++mode){native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,0);deferred_edit(&n.f,"+now");if(mode==1)native_merge_entry(&n.f);if(mode==2)assert(!pg_document_tx_retire(&n.f.scope,&n.f.definition,&n.f.address));
 assert(!pg_document_tx_deferred_merge(&n.f.scope,0,&deferred,10,&merge)&&!n.holds.busy&&!n.f.session.failed&&n.holds.count==(mode?0u:1u)&&n.f.session.sequence==(mode?3u:2u));
 if(!mode){assert(n.f.observed==2&&!n.f.adopted&&!n.f.published&&pg_document_holds_deadline(&n.holds)==110);assert(!pg_document_holds_flush(&n.holds,&n.f.session));}
 else assert(n.f.observed==1&&n.f.adopted==1&&n.f.published==1);
 native_hold_close(&n);
 }
 for(unsigned mode=1;mode<=6;++mode){native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,0);pg_document_loaded *loaded=n.holds.items[0].loaded;deferred_edit(&n.f,"+now");native_flush_fixture storage={.n=&n,.mode=mode};native_merge_fixture f={.n=&n,.fail=mode==5};n.f.session.storage=(pg_record_storage){&storage,native_flush_backend};n.f.session.context=&f;n.f.session.adopt=native_merge_adopt;n.f.session.publish=native_merge_publish;
 assert(pg_document_tx_merge(&n.f.scope,0,&merge)==PG_TX_COMMIT_FAILED&&n.f.session.failed&&!n.holds.busy&&!f.publishes);
 if(mode==1||mode==2){assert(n.holds.count==1&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"seedheld"));}
 else assert(!n.holds.count&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"seedheld+now"));
 native_hold_close(&n);
 }
#ifdef PG_MEMORY_FAULTS
 for(unsigned retire=0;retire<3;++retire){unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,0);pg_document_loaded *loaded=n.holds.items[0].loaded;struct json_object *value=loaded->value,*ops=n.holds.items[0].pending.ops;
 if(retire==2)native_merge_other_hold(&n,&deferred);
 if(retire==1){assert(!pg_record_tx_begin(&n.f.tx,&n.f.session,&n.f.memory,tx_read)&&!pg_document_tx_open(&n.f.scope,&n.f.tx,&n.f.cache)&&!pg_document_tx_retire(&n.f.scope,&n.f.definition,&n.f.address));}else deferred_edit(&n.f,"+now");
 if(retire==2)native_merge_other_get(&n,1);
 doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_tx_merge(&n.f.scope,0,&merge);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
 assert(!n.holds.busy&&!n.f.session.busy&&n.f.session.context==&n.f&&n.f.session.adopt==deferred_adopt&&n.f.session.publish==deferred_publish);
 if(target){assert(status!=PG_TX_OK&&n.holds.count==(retire==2?2u:1u)&&n.holds.items[0].pending.ops==ops&&loaded->value==value&&loaded->deltas_since_base==1&&pg_document_holds_pinned(&n.holds,2));++failures;}else {assert(!status&&!n.holds.count);total=calls;}
 native_hold_close(&n);
 }
 printf("PASS: held merge %s; %u direct allocation/insert/clone failures\n",retire==2?"multiple edits":retire?"record-only retirement":"loaded edit",failures);
 }
#endif
}


static uint64_t native_reload_clock(void *context){native_holds_fixture *n=context;assert(n->f.cache.busy&&pg_document_cache_close(&n->f.cache)==PG_DOC_INVALID);if(n->f.cache.busy==3)assert(pg_document_cache_touch_flushing(&n->f.cache,&n->f.address)==PG_DOC_INVALID);return n->f.now;}
static int native_reload(void *context,const pg_document_loaded *loaded){native_holds_fixture *n=context;assert(n->f.cache.busy==2&&pg_document_cache_touch(&n->f.cache,&loaded->record.address)==PG_DOC_INVALID&&pg_document_cache_close(&n->f.cache)==PG_DOC_INVALID);return pg_document_holds_flush_one(&n->holds,&n->f.session,loaded->record.id);}
static int native_reload_migrate(void *context,struct json_object *state,uint64_t version,struct json_object **out){(void)context;assert(version==1);struct json_object *flag=json_object_new_int64(1);if(!flag)return 1;if(json_object_object_add(state,"migrated",flag)){json_object_put(flag);return 1;}*out=json_object_get(state);return 0;}
static void native_reload_tests(void){
 native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,0);native_merge_other_hold(&n,&deferred);n.f.cache.flush=native_reload;n.f.cache.clock=native_reload_clock;
 pg_document_definition two=n.f.definition;two.version=2;two.migrate=native_reload_migrate;pg_document_loaded *loaded=0,*other=n.holds.items[1].loaded;
 assert(pg_document_cache_touch_flushing(&n.f.cache,&n.f.address)==PG_DOC_INVALID);
 assert(!pg_document_cache_get(&n.f.cache,&two,&n.f.address,&loaded)&&loaded&&loaded->value_version==2&&loaded->stored_version==1&&loaded->deltas_since_base==2&&!strcmp(json_object_get_string(json_object_object_get(loaded->value,"text")),"seedheld")&&json_object_get_int(json_object_object_get(loaded->value,"migrated"))==1);
 assert(n.holds.count==1&&n.holds.items[0].loaded==other&&pg_document_holds_deadline(&n.holds)==600&&pg_document_holds_pinned(&n.holds,3)&&!pg_document_holds_pinned(&n.holds,2)&&n.f.adopted==1&&n.f.published==1&&n.f.observed==2&&n.f.session.sequence==4&&!n.f.cache.busy);
 assert(!pg_document_cache_get(&n.f.cache,&n.f.definition,&n.f.address,&loaded)&&loaded->value_version==1&&n.f.session.sequence==4);
 assert(!pg_record_tx_begin(&n.f.tx,&n.f.session,&n.f.memory,tx_read)&&!pg_document_tx_open(&n.f.scope,&n.f.tx,&n.f.cache));pg_document_change *edit=0;assert(!pg_document_tx_get(&n.f.scope,&two,&n.f.address,0,0,&edit));struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"+v2\"]]");assert(ops&&!pg_document_change_append(edit,ops));json_object_put(ops);assert(!pg_document_tx_merge(&n.f.scope,0,&merge)&&n.holds.count==1&&n.f.session.sequence==5);
 struct json_object *stored=0;assert(!pg_record_memory_document(&n.f.memory,2,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),"seedheld+v2")&&json_object_get_int(json_object_object_get(json_object_object_get(stored,"value"),"migrated"))==1);json_object_put(stored);
 n.f.session.read_only=1;assert(!pg_document_holds_flush_one(&n.holds,&n.f.session,99)&&n.holds.count==1&&pg_document_holds_flush_one(&n.holds,&n.f.session,0)==PG_RECORDS_INVALID);n.f.session.read_only=0;assert(!pg_document_holds_flush_one(&n.holds,&n.f.session,3)&&!n.holds.count&&!n.f.cache.busy);native_hold_close(&n);
 native_merge_open(&n,&deferred,&merge,0);native_merge_other_hold(&n,&deferred);n.f.cache.flush=native_reload;n.f.cache.clock=native_reload_clock;two=n.f.definition;two.version=2;two.migrate=native_reload_migrate;pg_document_loaded *old=n.holds.items[0].loaded;native_flush_fixture backend={.n=&n,.mode=1};n.f.session.storage=(pg_record_storage){&backend,native_flush_backend};assert(pg_document_cache_get(&n.f.cache,&two,&n.f.address,&loaded)==PG_DOC_STORAGE_FAILED&&!loaded&&n.holds.count==2&&n.holds.items[0].loaded==old&&n.f.cache.count==2&&!n.f.cache.busy&&n.f.session.failed==PG_RECORDS_STORAGE_FAILED);native_hold_close(&n);
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){native_merge_open(&n,&deferred,&merge,0);native_merge_other_hold(&n,&deferred);n.f.cache.flush=native_reload;n.f.cache.clock=native_reload_clock;two=n.f.definition;two.version=2;two.migrate=native_reload_migrate;old=n.holds.items[0].loaded;other=n.holds.items[1].loaded;struct json_object *value=old->value;doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_cache_get(&n.f.cache,&two,&n.f.address,&loaded);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
 assert(!n.holds.busy&&!n.f.cache.busy&&!n.f.session.busy&&pg_document_holds_pinned(&n.holds,3));
 if(target){assert(status!=PG_DOC_OK&&!loaded&&(n.holds.count==1||n.holds.count==2));if(n.holds.count==2)assert(n.holds.items[0].loaded==old&&old->value==value&&n.f.cache.count==2);else assert(n.holds.items[0].loaded==other&&!n.f.session.failed);++failures;}else{assert(!status&&n.holds.count==1&&loaded->value_version==2);total=calls;}
 native_hold_close(&n);
 }
 printf("PASS: selected holds flush and version reload; %u direct allocation/insert/clone failures\n",failures);
#endif
}
static void native_finish_tests(void){
 for(unsigned mode=0;mode<3;++mode){native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,0);native_merge_other_hold(&n,&deferred);native_flush_fixture backend={.n=&n,.mode=1};if(mode)n.f.session.storage=(pg_record_storage){&backend,native_flush_backend};if(mode==2)n.f.session.failed=PG_RECORDS_ADOPTION_FAILED;
 unsigned expected=mode==0?0:mode==1?PG_RECORDS_STORAGE_FAILED:PG_RECORDS_ADOPTION_FAILED;
 assert(pg_document_holds_finish(&n.holds,&n.f.session)==expected&&!n.holds.active&&!n.holds.count&&!n.holds.items&&!n.holds.busy&&n.f.session.failed==expected&&!n.f.session.busy&&n.f.observed==2&&n.f.published==(mode?0u:1u));assert(pg_document_holds_finish(&n.holds,&n.f.session)==expected&&pg_document_holds_bind(&n.holds,&deferred)==PG_DOC_INVALID);deferred_close(&n.f);
 }
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){native_holds_fixture n;pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&n,&deferred,&merge,0);native_merge_other_hold(&n,&deferred);doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_document_holds_finish(&n.holds,&n.f.session);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!n.holds.active&&!n.holds.busy&&!n.holds.items&&!n.holds.count&&!n.f.session.busy);
 if(target){assert(status!=0&&n.f.session.failed==(unsigned)status&&pg_document_holds_finish(&n.holds,&n.f.session)==(unsigned)status);++failures;}else {assert(!status&&!n.f.session.failed&&n.f.published==1);total=calls;}deferred_close(&n.f);
 }
 printf("PASS: holds finish drains healthy/cleans failed, repeated finish, and %u direct allocation/insert/clone failures\n",failures);
#endif
}


typedef struct {pg_timer_job callback;void *context;} native_timer_handle;
typedef struct {native_holds_fixture n;pg_hold_timer timer;pg_hold_timer_host host;native_timer_handle *armed;pg_timer_job pending;void *pending_context;uint64_t delay;unsigned arms,cancels,enqueues,mode;} native_timer_fixture;
static int native_foreground(void *context,uint64_t delay,pg_timer_job callback,void *owner,void **out){native_timer_fixture *f=context;assert(f->timer.busy&&!f->armed);++f->arms;f->delay=delay;*out=0;if(f->mode==1)return 1;native_timer_handle *handle=calloc(1,sizeof(*handle));if(!handle)return 1;handle->callback=callback;handle->context=owner;f->armed=handle;*out=handle;return f->mode==2;}
static void native_timer_cancel(void *context,void *handle){native_timer_fixture *f=context;assert(handle&&handle==f->armed&&f->timer.busy);++f->cancels;f->armed=0;free(handle);}
static int native_timer_enqueue(void *context,pg_timer_job job,void *owner){native_timer_fixture *f=context;assert(!f->pending);++f->enqueues;if(f->mode==3)return 1;f->pending=job;f->pending_context=owner;return 0;}
static void native_timer_open(native_timer_fixture *f,unsigned other){memset(f,0,sizeof(*f));pg_document_deferred deferred;pg_document_merge merge;native_merge_open(&f->n,&deferred,&merge,0);if(other)native_merge_other_hold(&f->n,&deferred);f->host=(pg_hold_timer_host){f,native_foreground,native_timer_cancel,native_timer_enqueue};assert(!pg_hold_timer_open(&f->timer,&f->n.holds,&f->n.f.session,&f->host));}
static void native_timer_fire(native_timer_fixture *f){assert(f->armed);f->armed->callback(f->armed->context);}
static void native_timer_run(native_timer_fixture *f){assert(f->pending&&!f->n.f.session.busy&&!f->n.holds.busy&&!f->n.f.cache.busy);pg_timer_job job=f->pending;void *context=f->pending_context;f->pending=0;f->pending_context=0;job(context);}
static void native_timer_close(native_timer_fixture *f){assert(!f->pending&&!pg_hold_timer_stop(&f->timer)&&!f->armed);native_hold_close(&f->n);}
static void native_timer_edit(native_timer_fixture *f,const char *suffix){deferred_edit(&f->n.f,suffix);f->n.f.scope.entries[0].checkpoint=0;}
static void native_timer_tests(void){
 _Static_assert(sizeof(pg_hold_timer)==72&&sizeof(pg_hold_timer_host)==32,"hold timer ABI");
 native_timer_fixture f;native_timer_open(&f,1);assert(!pg_hold_timer_sync(&f.timer)&&f.arms==1&&f.delay==100);assert(!pg_hold_timer_sync(&f.timer)&&f.arms==1&&!f.cancels);
 pg_document_deferred deferred;assert(!pg_document_holds_bind(&f.n.holds,&deferred));f.n.f.now=150;native_timer_edit(&f,"a");assert(!pg_document_tx_deferred(&f.n.f.scope,0,&deferred,500)&&!pg_hold_timer_sync(&f.timer)&&f.arms==1);
 native_timer_edit(&f,"b");assert(!pg_document_tx_deferred(&f.n.f.scope,0,&deferred,10)&&!pg_hold_timer_sync(&f.timer)&&f.arms==2&&f.cancels==1&&f.delay==10);
 f.n.f.now=160;f.n.f.session.busy=1;native_timer_fire(&f);native_timer_fire(&f);assert(f.enqueues==1&&f.pending&&!f.n.f.adopted&&!f.n.f.published&&!f.n.f.session.failed);f.n.f.session.busy=0;native_timer_edit(&f,"c");assert(!pg_document_tx_deferred(&f.n.f.scope,0,&deferred,0)&&!pg_hold_timer_sync(&f.timer)&&f.arms==2&&f.cancels==1);native_timer_run(&f);
 assert(!f.timer.ticket&&!f.n.holds.count&&f.n.f.adopted==1&&f.n.f.published==1&&f.n.f.observed==5&&f.cancels==2);struct json_object *stored=0;assert(!pg_record_memory_document(&f.n.f.memory,2,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),"seedheldabc"));json_object_put(stored);assert(!pg_hold_timer_sync(&f.timer)&&f.arms==2);native_timer_close(&f);
 native_timer_open(&f,0);assert(!pg_document_holds_bind(&f.n.holds,&deferred));assert(!pg_document_holds_flush(&f.n.holds,&f.n.f.session));native_timer_edit(&f,"infinite");assert(!pg_document_tx_deferred(&f.n.f.scope,0,&deferred,UINT64_MAX)&&!pg_hold_timer_sync(&f.timer)&&!f.arms);native_timer_edit(&f,"due");assert(!pg_document_tx_deferred(&f.n.f.scope,0,&deferred,0));f.n.f.now=200;assert(!pg_hold_timer_sync(&f.timer)&&f.arms==1&&!f.delay);native_timer_close(&f);
 native_timer_open(&f,1);assert(!pg_hold_timer_sync(&f.timer));native_timer_fire(&f);assert(pg_hold_timer_stop(&f.timer)==1&&f.cancels==1&&f.pending&&f.n.holds.count==2);assert(pg_hold_timer_stop(&f.timer)==1&&f.cancels==1);native_timer_run(&f);assert(!f.n.f.published&&f.n.holds.count==2&&!pg_hold_timer_stop(&f.timer));assert(!pg_document_holds_finish(&f.n.holds,&f.n.f.session)&&f.n.f.published==1);deferred_close(&f.n.f);
 for(unsigned mode=1;mode<=3;++mode){native_timer_open(&f,0);f.mode=mode;if(mode<3)assert(pg_hold_timer_sync(&f.timer)==PG_RECORDS_ADOPTION_FAILED);else {assert(!pg_hold_timer_sync(&f.timer));native_timer_fire(&f);assert(!f.pending&&pg_hold_timer_sync(&f.timer)==PG_RECORDS_ADOPTION_FAILED);}assert(f.n.f.session.failed==PG_RECORDS_ADOPTION_FAILED&&f.n.holds.count==1&&!f.timer.busy);native_timer_close(&f);}
 native_timer_open(&f,0);assert(!pg_hold_timer_sync(&f.timer));f.n.f.session.failed=PG_RECORDS_STORAGE_FAILED;native_timer_fire(&f);native_timer_run(&f);assert(f.n.holds.count==1&&!f.n.f.published&&f.n.f.session.failed==PG_RECORDS_STORAGE_FAILED);native_timer_close(&f);
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){native_timer_open(&f,1);assert(!pg_hold_timer_sync(&f.timer));native_timer_fire(&f);doc_calls=0;doc_fail=target;doc_tracking=1;native_timer_run(&f);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!f.timer.ticket&&!f.timer.busy&&!f.armed);
 if(target){assert(f.n.f.session.failed&&f.timer.error==f.n.f.session.failed&&f.n.holds.count==2&&!f.n.f.published);++failures;}else {assert(!f.n.f.session.failed&&!f.n.holds.count&&f.n.f.published==1);total=calls;}native_timer_close(&f);}
 for(unsigned target=1;target<=2;++target){native_timer_open(&f,0);doc_calls=0;doc_fail=target;doc_tracking=1;int status=pg_hold_timer_sync(&f.timer);doc_tracking=0;doc_fail=0;assert(status!=0&&f.n.f.session.failed==(unsigned)status&&!f.timer.ticket&&!f.armed&&f.n.holds.count==1);native_timer_close(&f);++failures;}
 printf("PASS: foreground hold timer, earliest/rearm/infinity, queued flush/stop, errors and %u direct allocation/insert/clone failures\n",failures);
#endif
}


typedef struct {native_timer_fixture native;pg_durable_session session;unsigned closes,pending,fail_backend,close_on_publish,close_on_memory;const char *expected;} durable_fixture;
static uint64_t durable_hold_clock(void *context){return ((durable_fixture*)context)->native.n.f.now;}
static void durable_memory_observe(void *context,const pg_document_tx_entry *entries,size_t count){durable_fixture *f=context;native_hold_observe(&f->native.n,entries,count);if(f->close_on_memory)assert(f->native.armed&&pg_durable_session_close(&f->session)==1&&f->session.phase==2);}
static int durable_backend_close(void *context){durable_fixture *f=context;++f->closes;assert(f->session.busy&&f->native.n.f.cache.closed&&!f->native.n.holds.active&&!f->native.timer.phase&&!f->native.pending&&!f->native.armed&&!f->session.active_documents);if(f->pending){--f->pending;return 1;}if(f->expected){struct json_object *stored=0;assert(!pg_record_memory_document(&f->native.n.f.memory,2,UINT64_MAX,&stored)&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),f->expected));json_object_put(stored);}assert(!pg_record_memory_close(&f->native.n.f.memory));return f->fail_backend?-1:0;}
static int durable_adopt(void *context,const struct json_object *writes,uint64_t seq){durable_fixture *f=context;return deferred_adopt(&f->native.n.f,writes,seq);}
static void durable_publish(void *context,const struct json_object *writes,uint64_t seq){durable_fixture *f=context;deferred_publish(&f->native.n.f,writes,seq);if(f->close_on_publish){assert(pg_durable_session_close(&f->session)==1&&f->session.phase==2);pg_record_tx tx={0};pg_document_tx docs={0};assert(pg_durable_session_begin(&f->session,&tx,&docs,&f->native.n.f.memory,tx_read)==PG_TX_INVALID&&!tx.writes);}}
static void durable_open(durable_fixture *f){memset(f,0,sizeof(*f));pg_document_deferred deferred;native_hold_open(&f->native.n,&deferred);f->native.host=(pg_hold_timer_host){&f->native,native_foreground,native_timer_cancel,native_timer_enqueue};assert(!pg_hold_timer_open(&f->native.timer,&f->native.n.holds,&f->native.n.f.session,&f->native.host));f->native.n.holds.context=f;f->native.n.holds.clock=durable_hold_clock;f->native.n.holds.observe=durable_memory_observe;f->native.n.f.session.context=f;f->native.n.f.session.adopt=durable_adopt;f->native.n.f.session.publish=durable_publish;pg_durable_close backend={f,durable_backend_close};assert(!pg_durable_session_open(&f->session,&f->native.n.f.session,&f->native.n.f.cache,&f->native.n.holds,&f->native.timer,&backend)&&pg_durable_session_idle(&f->session));}
static void durable_edit(durable_fixture *f,const char *suffix){deferred_fixture *n=&f->native.n.f;assert(!pg_durable_session_begin(&f->session,&n->tx,&n->scope,&n->memory,tx_read)&&!pg_durable_session_idle(&f->session));pg_document_change *edit=0;assert(!pg_document_tx_get(&n->scope,&n->definition,&n->address,0,0,&edit));if(suffix){char text[256];int size=snprintf(text,sizeof(text),"[[\"a\",[\"text\"],\"%s\"]]",suffix);assert(size>0&&(size_t)size<sizeof(text));struct json_object *ops=json_tokener_parse(text);assert(ops&&!pg_document_change_append(edit,ops));json_object_put(ops);}}
static void durable_closed(durable_fixture *f,int result){unsigned calls=f->closes;assert(pg_durable_session_close(&f->session)==result&&f->closes==calls&&!pg_durable_session_idle(&f->session)&&!f->native.pending&&!f->native.armed&&f->native.n.f.memory.closed&&f->native.n.f.cache.closed&&!f->native.n.holds.active&&f->native.n.f.cache.context==&f->native.n&&f->native.n.f.cache.pinned==native_hold_pinned&&!f->native.n.f.cache.flush);pg_record_tx tx={0};pg_document_tx docs={0};assert(pg_durable_session_begin(&f->session,&tx,&docs,&f->native.n.f.memory,tx_read)==PG_TX_INVALID&&!tx.writes);}

static void durable_merge_setup(durable_fixture *f){deferred_fixture *n=&f->native.n.f;assert(!pg_durable_session_begin(&f->session,&n->tx,&n->scope,&n->memory,tx_read));struct json_object *create=json_tokener_parse("{\"type\":\"document.create\",\"record\":{\"id\":3,\"kind\":\"other\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"latest\",\"fork\":\"initial\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"\"}}}");assert(create&&!pg_record_tx_stage(&n->tx,PG_TX_DOCUMENT_CREATE,create));json_object_put(create);assert(!pg_durable_session_settle(&f->session,0,0,0));durable_edit(f,"held");assert(!pg_durable_session_settle(&f->session,1,10,0));assert(!pg_durable_session_begin(&f->session,&n->tx,&n->scope,&n->memory,tx_read));native_merge_other_get(&f->native.n,1);assert(!pg_durable_session_settle(&f->session,1,100,0)&&f->native.n.holds.count==2);durable_edit(f,"+now");}
static void durable_session_tests(void){
 _Static_assert(sizeof(pg_durable_session)==192&&offsetof(pg_durable_session,active_documents)==160&&offsetof(pg_durable_session,phase)==172,"durable Session ABI");
 durable_fixture f;durable_open(&f);durable_edit(&f,"a");assert(!pg_durable_session_settle(&f.session,1,100,0)&&f.native.arms==1&&f.native.n.holds.count==1&&pg_durable_session_idle(&f.session));native_timer_fire(&f.native);durable_edit(&f,0);assert(!pg_durable_session_idle(&f.session)&&!pg_durable_session_abort(&f.session)&&pg_durable_session_idle(&f.session));native_timer_run(&f.native);assert(!f.native.n.holds.count&&f.native.n.f.session.sequence==3);durable_edit(&f,"b");assert(!pg_durable_session_settle(&f.session,0,0,0)&&f.native.n.f.session.sequence==4);f.expected="seedab";assert(!pg_durable_session_close(&f.session)&&f.closes==1);durable_closed(&f,0);

 durable_open(&f);pg_durable_session duplicate={0};pg_durable_close duplicate_backend={&f,durable_backend_close};assert(pg_durable_session_open(&duplicate,&f.native.n.f.session,&f.native.n.f.cache,&f.native.n.holds,&f.native.timer,&duplicate_backend)==PG_RECORDS_INVALID&&!duplicate.phase&&f.native.n.f.cache.context==&f.session);
 pg_record_tx bad_tx={0};pg_document_tx bad_docs={.busy=1};assert(pg_durable_session_begin(&f.session,&bad_tx,&bad_docs,&f.native.n.f.memory,tx_read)==PG_TX_INVALID&&!bad_tx.writes&&!f.session.active_documents&&!f.session.busy);assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);
 durable_open(&f);durable_edit(&f,"aborted");assert(pg_durable_session_close(&f.session)==1&&!pg_durable_session_abort(&f.session));f.expected="seed";assert(!pg_durable_session_close(&f.session)&&!f.native.n.f.published);durable_closed(&f,0);
 durable_open(&f);durable_edit(&f,"admitted");assert(pg_durable_session_close(&f.session)==1&&f.session.phase==2&&!f.closes);pg_record_tx refused={0};pg_document_tx refused_docs={0};assert(pg_durable_session_begin(&f.session,&refused,&refused_docs,&f.native.n.f.memory,tx_read)==PG_TX_INVALID&&!refused.writes);assert(!pg_durable_session_settle(&f.session,1,0,0)&&f.native.n.holds.count==1&&!f.native.arms);f.expected="seedadmitted";assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);
 for(unsigned admitted=0;admitted<2;++admitted){durable_open(&f);durable_edit(&f,"a");assert(!pg_durable_session_settle(&f.session,1,100,0));native_timer_fire(&f.native);if(admitted)durable_edit(&f,"b");assert(pg_durable_session_close(&f.session)==1&&f.native.pending&&!f.closes);if(admitted)assert(!pg_durable_session_settle(&f.session,0,0,0));assert(pg_durable_session_idle(&f.session));native_timer_run(&f.native);f.expected=admitted?"seedab":"seeda";assert(!pg_durable_session_close(&f.session)&&f.native.n.f.published==1);durable_closed(&f,0);}
 durable_open(&f);f.close_on_memory=1;durable_edit(&f,"memory");assert(!pg_durable_session_settle(&f.session,1,100,0)&&f.session.phase==2&&f.native.arms==1);f.expected="seedmemory";assert(!pg_durable_session_close(&f.session)&&f.native.cancels==1);durable_closed(&f,0);
 durable_open(&f);f.close_on_publish=1;durable_edit(&f,"x");assert(!pg_durable_session_settle(&f.session,0,0,0)&&f.session.phase==2&&!f.closes);f.expected="seedx";assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);
 durable_open(&f);f.pending=2;assert(pg_durable_session_close(&f.session)==1&&f.session.phase==3&&!f.native.n.f.memory.closed&&!pg_durable_session_idle(&f.session));assert(pg_durable_session_close(&f.session)==1&&f.closes==2);assert(!pg_durable_session_close(&f.session)&&f.closes==3);durable_closed(&f,0);
 for(unsigned failed=0;failed<2;++failed){durable_open(&f);durable_edit(&f,"unstored");assert(!pg_durable_session_settle(&f.session,1,100,0));if(failed)f.native.n.f.session.failed=PG_RECORDS_ADOPTION_FAILED;f.fail_backend=1;f.expected=failed?"seed":"seedunstored";assert(pg_durable_session_close(&f.session)==-1&&f.session.result==(failed?PG_RECORDS_ADOPTION_FAILED:PG_RECORDS_STORAGE_FAILED)&&f.closes==1);durable_closed(&f,-1);}
 durable_open(&f);durable_edit(&f,"held");assert(!pg_durable_session_settle(&f.session,1,100,0));pg_document_definition two=f.native.n.f.definition;two.version=2;two.migrate=native_reload_migrate;pg_document_loaded *loaded=0;assert(!pg_document_cache_get(&f.native.n.f.cache,&two,&f.native.n.f.address,&loaded)&&loaded->value_version==2&&!f.native.n.holds.count&&!f.native.armed&&f.native.cancels==1);f.expected="seedheld";assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);
#ifdef PG_MEMORY_FAULTS
 for(unsigned closing=0;closing<3;++closing){unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){durable_open(&f);if(closing==2)durable_merge_setup(&f);else durable_edit(&f,"fault");if(closing==1)assert(!pg_durable_session_settle(&f.session,1,100,0));doc_calls=0;doc_fail=target;doc_tracking=1;int status=closing==1?pg_durable_session_close(&f.session):pg_durable_session_settle(&f.session,closing==0,100,0);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
 if(target){assert(status!=0);if(closing==0&&!f.native.n.f.session.failed)assert(!f.native.n.holds.count);if(closing==2&&f.native.timer.error)assert(f.native.n.f.session.failed==f.native.timer.error);++failures;}else{assert(!status);total=calls;}
 if(closing!=1){int expected=f.native.n.f.session.failed?-1:0;assert(pg_durable_session_close(&f.session)==expected);durable_closed(&f,expected);}else durable_closed(&f,status?-1:0);
 }
 printf("PASS: Durable Session %s; %u direct allocation/insert/clone failures\n",closing==2?"merged adoption/timer rearm":closing?"close cleanup":"deferred settlement/timer sync",failures);
 }
#endif
}


static void durable_read_only_tests(void){
 for(unsigned deferred=0;deferred<2;++deferred)for(unsigned mode=0;mode<8;++mode){durable_fixture f;durable_open(&f);deferred_fixture *n=&f.native.n.f;n->session.read_only=1;uint64_t seq=n->memory.next_sequence;size_t rows=n->memory.count;assert(!pg_durable_session_begin(&f.session,&n->tx,&n->scope,&n->memory,tx_read));struct json_object *out=0;
 if(mode==1||mode==7){pg_document_change *edit=0;struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"refused\"]]");assert(!pg_document_tx_get(&n->scope,&n->definition,&n->address,0,0,&edit)&&!pg_document_change_append(edit,ops));json_object_put(ops);}
 if(mode==2||mode==7){struct json_object *draft=json_tokener_parse("{\"kind\":\"note\"}");assert(!pg_record_tx_append_entry(&n->tx,1,draft,0,tx_mint,&out));json_object_put(draft);json_object_put(out);out=0;}
 if(mode==3){task_create_fixture initial={.tx=&n->tx,.memory=&n->memory};pg_task_creation host={&initial,task_initial,task_mint,0,1};struct json_object *definition=json_tokener_parse("{\"name\":\"work\",\"version\":1}"),*options=json_tokener_parse("{\"ownership\":{\"kind\":\"conversation\"}}");uint64_t id=0;assert(!pg_record_tx_create_task(&n->tx,definition,0,options,&host,&id)&&id);json_object_put(definition);json_object_put(options);}
 if(mode==4){struct json_object *draft=json_tokener_parse("{\"conversationId\":1,\"status\":\"queued\"}");assert(!pg_record_tx_create_submission(&n->tx,draft,tx_mint,&out));json_object_put(draft);json_object_put(out);out=0;}
 if(mode==5)assert(!pg_document_tx_retire(&n->scope,&n->definition,&n->address));
 pg_document_definition fresh=n->definition;fresh.kind=(pg_slice){"fresh",5};pg_document_address address=n->address;address.kind=fresh.kind;
 if(mode==6){struct json_object *seed=json_tokener_parse("{\"text\":\"new\"}");pg_document_initializer initial={seed,fork_replacement_initial};pg_document_request request={.definition=&fresh,.address=&address,.initializer=&initial,.mint=tx_mint};pg_document_change *edit=0;assert(!pg_document_tx_doc(&n->scope,&request,&edit));json_object_put(seed);}
 assert((pg_durable_session_settle(&f.session,deferred,100,0)==PG_TX_OK)==(mode==0));assert(!n->session.failed&&!f.native.n.holds.count&&!f.native.armed&&!n->adopted&&!n->published&&!n->observed&&n->memory.next_sequence==seq&&n->memory.count==rows&&n->session.sequence==seq-1&&pg_durable_session_idle(&f.session));assert(!pg_durable_snapshot(&f.session,&n->definition,&n->address,&out)&&!strcmp(json_object_get_string(json_object_object_get(out,"text")),"seed"));json_object_put(out);out=0;if(mode==6)assert(pg_durable_snapshot(&f.session,&fresh,&address,&out)==PG_DOC_MISSING&&!out);assert(!pg_durable_session_begin(&f.session,&n->tx,&n->scope,&n->memory,tx_read)&&!pg_durable_session_settle(&f.session,deferred,100,0));assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);
 }
 puts("PASS: 16 read-only Session normal/deferred transactions, empty success, write refusal and continued reads/close");
}
static void durable_default_notifications_tests(void){
 for(unsigned held=0;held<2;++held){durable_fixture f;durable_open(&f);if(held){durable_edit(&f,"held");assert(!pg_durable_session_settle(&f.session,1,100,0));}default_notification_capture capture={0};pg_session_listeners listeners={.records=&f.native.n.f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,default_external,&capture));pg_session_notification_host owner={&listeners,&f.native.n.holds,&capture};pg_document_notifications host={0};assert(!pg_session_notifications_bind(&owner,&host));durable_edit(&f,"notify");assert(!pg_durable_session_settle_notified(&f.session,0,&host)&&!f.session.active_records&&!f.session.active_documents&&!f.session.busy&&!f.native.n.holds.count&&!f.native.armed&&pg_durable_session_idle(&f.session));struct json_object *stored=json_object_array_get_idx(json_object_object_get(capture.stored,"changes"),0);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(stored,"value"),"text")),held?"seedheldnotify":"seednotify"));default_notification_finish(&listeners,&capture);f.expected=held?"seedheldnotify":"seednotify";assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);}
 puts("PASS: actual Durable Session notified settlement, normal and held merge, timer sync and ordered close");
}
static void durable_deferred_notification_tests(void){
 for(unsigned mode=0;mode<5;++mode){durable_fixture f;durable_open(&f);if(mode==4){durable_edit(&f,"held");assert(!pg_durable_session_settle(&f.session,1,100,0));}default_notification_capture capture={0};pg_session_listeners listeners={.records=&f.native.n.f.session};assert(!pg_session_listen(&listeners,0,default_internal,&capture)&&!pg_session_listen(&listeners,1,default_external,&capture));pg_session_notification_host owner={&listeners,&f.native.n.holds,&capture};pg_document_notifications host={0};assert(!pg_session_deferred_notifications_bind(&owner,&host));if(mode==3)f.native.n.f.session.read_only=1;durable_edit(&f,mode==2?0:"notify");if(mode==1||mode==4){struct json_object *draft=json_tokener_parse("{\"kind\":\"tick\"}"),*entry=0;assert(!pg_record_tx_append_entry(&f.native.n.f.tx,1,draft,0,tx_mint,&entry));json_object_put(draft);json_object_put(entry);}int status=pg_durable_session_defer_notified(&f.session,100,0,&host);assert(!f.session.active_records&&!f.session.active_documents&&!f.session.busy&&!f.native.n.f.session.failed);if(mode==3){assert(status!=PG_TX_OK&&!capture.internal_calls&&!capture.external_calls&&!f.native.n.holds.count);}else{assert(!status);if(mode==2)assert(!capture.internal_calls&&!capture.external_calls&&!f.native.n.holds.count);else{assert(capture.internal_calls==1&&capture.external_calls==(mode==1||mode==4));struct json_object *changes=json_object_object_get(capture.observed,"changes"),*notice=json_object_array_get_idx(changes,mode==1||mode==4?1:0);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(notice,"value"),"text")),mode==4?"seedheldnotify":"seednotify"));if(!mode){assert(!strcmp(json_object_get_string(json_object_object_get(capture.observed,"kind")),"memory")&&!json_object_object_get(capture.observed,"seq")&&!capture.stored&&f.native.n.holds.count==1&&f.native.armed&&f.native.n.f.session.sequence==2&&f.native.n.f.memory.next_sequence==3);}else assert(!strcmp(json_object_get_string(json_object_object_get(capture.observed,"kind")),"stored")&&!f.native.n.holds.count&&!f.native.armed&&f.native.n.f.session.sequence==3);}}
 assert(!pg_session_listeners_close(&listeners,&capture));json_object_put(capture.observed);json_object_put(capture.stored);f.expected=mode==2||mode==3?"seed":mode==4?"seedheldnotify":"seednotify";assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);
 }
 puts("PASS: actual Session deferred memory notifications, mixed/held fallback, empty/read-only and close");
}
static void durable_stage(durable_fixture *f,const char *text){deferred_fixture *n=&f->native.n.f;assert(!pg_durable_session_begin(&f->session,&n->tx,&n->scope,&n->memory,tx_read));struct json_object *batch=json_tokener_parse(text);assert(batch);for(size_t i=0;i<json_object_array_length(batch);++i){struct json_object *write=json_object_array_get_idx(batch,i);const char *type=json_object_get_string(json_object_object_get(write,"type"));unsigned kind=!strcmp(type,"entry")?PG_TX_ENTRY:!strcmp(type,"conversation")?PG_TX_CONVERSATION:!strcmp(type,"document.create")?PG_TX_DOCUMENT_CREATE:!strcmp(type,"document.change")?PG_TX_DOCUMENT_CHANGE:PG_TX_DOCUMENT_RETIRE;assert(!pg_record_tx_stage(&n->tx,kind,write));}json_object_put(batch);assert(!pg_durable_session_settle(&f->session,0,0,0));}
static void durable_history_seed(durable_fixture *f){
 durable_stage(f,"[{\"type\":\"entry\",\"value\":{\"id\":15,\"conversationId\":1,\"kind\":\"before\"}}]");
 durable_stage(f,"[{\"type\":\"entry\",\"value\":{\"id\":20,\"conversationId\":1,\"kind\":\"first\"}},{\"type\":\"document.create\",\"record\":{\"id\":10,\"kind\":\"history\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"one\"}}},{\"type\":\"document.create\",\"record\":{\"id\":12,\"kind\":\"history\",\"key\":\"k\\u0000中\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"family\"}}}]");
 durable_stage(f,"[{\"type\":\"entry\",\"value\":{\"id\":21,\"conversationId\":1,\"kind\":\"second\"}},{\"type\":\"document.change\",\"id\":10,\"content\":{\"kind\":\"delta\",\"version\":1,\"ops\":[[\"a\",[\"text\"],\"two\"]]}}]");
 durable_stage(f,"[{\"type\":\"entry\",\"value\":{\"id\":22,\"conversationId\":1,\"kind\":\"retired\"}},{\"type\":\"document.retire\",\"id\":10}]");
 durable_stage(f,"[{\"type\":\"entry\",\"value\":{\"id\":23,\"conversationId\":1,\"kind\":\"new\"}},{\"type\":\"document.create\",\"record\":{\"id\":11,\"kind\":\"history\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"new\"}}}]");
 durable_stage(f,"[{\"type\":\"conversation\",\"value\":{\"id\":50,\"parent\":{\"conversationId\":1,\"at\":20}}}]");
}
static pg_document_history_reader durable_history(durable_fixture *f){pg_document_history_reader h={.context=&f->native.n.f.memory,.entry=fork_entry};pg_record_memory_documents(&f->native.n.f.memory,&h.documents);return h;}
static const pg_document_definition history_def={.kind={"history",7},.version=1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_REWINDABLE,.fork=PG_DOC_AS_OF};
static const pg_document_address history_address={.kind={"history",7},.owner=1,.scope=PG_DOC_CONVERSATION};
static void reader_current_tests(void){
 durable_fixture f;durable_open(&f);struct json_object *value=0;assert(!pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value)&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"seed"));assert(!json_object_object_add(value,"text",json_object_new_string("outside")));json_object_put(value);value=0;
 f.native.n.f.session.read_only=1;assert(!pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value)&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"seed"));json_object_put(value);value=0;f.native.n.f.session.read_only=0;
 durable_edit(&f,"draft");assert(!pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value)&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"seed"));json_object_put(value);value=0;pg_document_definition two=f.native.n.f.definition;two.version=2;two.migrate=native_reload_migrate;assert(pg_durable_snapshot(&f.session,&two,&f.native.n.f.address,&value)==PG_DOCUMENT_READ_PENDING&&!value&&f.native.n.f.session.sequence==2);pg_document_history_reader h=durable_history(&f);assert(pg_durable_snapshot_as_of(&f.session,&h,&history_def,&history_address,20,&value)==PG_DOCUMENT_READ_PENDING&&!value);assert(!pg_durable_session_abort(&f.session));
 durable_edit(&f,"held");assert(!pg_durable_session_settle(&f.session,1,100,0));assert(!pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value)&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"seedheld")&&!f.native.n.f.published);json_object_put(value);value=0;
 assert(!pg_durable_snapshot(&f.session,&two,&f.native.n.f.address,&value)&&json_object_get_int(json_object_object_get(value,"migrated"))==1&&!f.native.n.holds.count&&!f.native.armed&&f.native.n.f.session.sequence==3);f.expected="seedheld";assert(!pg_durable_session_close(&f.session));assert(!strcmp(json_object_get_string(json_object_object_get(value,"text")),"seedheld"));json_object_put(value);value=0;assert(pg_durable_snapshot(&f.session,&two,&f.native.n.f.address,&value)==PG_DOCUMENT_SESSION_CLOSED&&!value);durable_closed(&f,0);
 durable_open(&f);f.native.n.f.session.failed=PG_RECORDS_STORAGE_FAILED;assert(pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value)==PG_DOCUMENT_SESSION_FAILED&&!value&&!f.native.n.f.cache.count);assert(pg_durable_session_close(&f.session)==-1);durable_closed(&f,-1);
}
static void reader_history_tests(void){
 durable_fixture f;durable_open(&f);durable_history_seed(&f);pg_document_history_reader h=durable_history(&f);uint64_t sequence=f.native.n.f.session.sequence;struct json_object *value=0;const uint64_t entries[]={15,20,21,22,23};const char *answers[]={0,"one","onetwo",0,"new"};
 f.native.n.f.session.read_only=1;for(unsigned i=0;i<5;++i){int status=pg_durable_snapshot_as_of(&f.session,&h,&history_def,&history_address,entries[i],&value);assert(status==(answers[i]?PG_DOC_OK:PG_DOC_MISSING));if(answers[i]){assert(!strcmp(json_object_get_string(json_object_object_get(value,"text")),answers[i]));json_object_put(value);value=0;}else assert(!value);}
 pg_document_address forked=history_address;forked.owner=50;assert(!pg_durable_snapshot_as_of(&f.session,&h,&history_def,&forked,20,&value)&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"one"));json_object_put(value);value=0;assert(pg_durable_snapshot_as_of(&f.session,&h,&history_def,&forked,21,&value)==PG_DOCUMENT_ENTRY_NOT_VISIBLE&&!value&&!f.native.n.f.session.failed);
 pg_document_definition migrated=history_def;migrated.version=2;migrated.migrate=native_reload_migrate;assert(!pg_durable_snapshot_as_of(&f.session,&h,&migrated,&history_address,20,&value)&&json_object_get_int(json_object_object_get(value,"migrated"))==1);json_object_put(value);value=0;
 pg_document_definition family=history_def;family.family=1;pg_document_address address=history_address;address.has_key=1;address.key=(pg_slice){"k\0中",5};assert(!pg_durable_snapshot_as_of(&f.session,&h,&family,&address,21,&value)&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"family"));json_object_put(value);value=0;
 assert(pg_durable_snapshot_as_of(&f.session,&h,&f.native.n.f.definition,&f.native.n.f.address,20,&value)==PG_DOC_SEMANTICS&&!value&&!f.native.n.f.session.failed);assert(!f.native.n.f.cache.count&&f.native.n.f.session.sequence==sequence&&!f.native.arms);assert(!pg_durable_session_close(&f.session));durable_closed(&f,0);
}
#ifdef PG_MEMORY_FAULTS
static void reader_faults(void){
 for(unsigned mode=0;mode<3;++mode){unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){durable_fixture f;durable_open(&f);if(mode==2)durable_history_seed(&f);struct json_object *value=0;pg_document_history_reader h=durable_history(&f);pg_document_definition def=mode==2?history_def:f.native.n.f.definition;if(mode==2){def.version=2;def.migrate=native_reload_migrate;}if(mode==1){assert(!pg_durable_snapshot(&f.session,&def,&f.native.n.f.address,&value));json_object_put(value);value=0;}uint64_t sequence=f.native.n.f.session.sequence;
 doc_calls=0;doc_fail=target;doc_tracking=1;int status=mode==2?pg_durable_snapshot_as_of(&f.session,&h,&def,&history_address,20,&value):pg_durable_snapshot(&f.session,&def,&f.native.n.f.address,&value);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;assert(!f.session.busy&&f.native.n.f.session.sequence==sequence&&!f.native.n.f.cache.busy);
 if(target){assert(status!=PG_DOC_OK&&!value);if(mode==1)assert(status==PG_DOC_ALLOCATION&&!f.native.n.f.session.failed&&f.native.n.f.cache.count==1);++failures;}else {assert(!status&&value);json_object_put(value);value=0;total=calls;}if(mode==2)assert(!f.native.n.f.cache.count);
 int result=f.native.n.f.session.failed?-1:0;assert(pg_durable_session_close(&f.session)==result);durable_closed(&f,result);
 }
 printf("PASS: %s snapshots; %u direct allocation/insert/clone failures\n",mode==2?"historical":mode?"warm current":"cold current",failures);
 }
}
#endif


typedef struct {durable_fixture *f;pg_document_storage docs;unsigned mode,entries,finds,reads;} reader_probe;
static int reader_probe_entry(void *context,uint64_t conversation,uint64_t id,struct json_object **out,uint64_t *seq){reader_probe *p=context;++p->entries;assert(p->f->session.busy);if(p->mode==1)return 1;if(p->mode==9)assert(pg_durable_session_close(&p->f->session)==1);int status=fork_entry(&p->f->native.n.f.memory,conversation,id,out,seq);if(status)return status;if(p->mode==2)return 7;if(p->mode==3)assert(!json_object_object_add(*out,"id",json_object_new_int64(999)));if(p->mode==4)*seq=0;if(p->mode==7)p->f->native.n.f.session.failed=PG_RECORDS_ALLOCATION;return 0;}
static int reader_probe_find(void *context,const pg_document_address *address,uint64_t point,struct json_object **out){reader_probe *p=context;++p->finds;assert(p->f->session.busy);int status=p->docs.find(p->docs.context,address,point,out);if(status)return status;if(p->mode==5)return 7;if(p->mode==8||p->mode==10)p->f->native.n.f.session.failed=PG_RECORDS_ALLOCATION;return 0;}
static int reader_probe_read(void *context,uint64_t id,uint64_t point,struct json_object **out){reader_probe *p=context;++p->reads;assert(p->f->session.busy);int status=p->docs.read(p->docs.context,id,point,out);if(status)return status;if(p->mode==6||p->mode==12)return 1;if(p->mode==11)p->f->native.n.f.session.failed=PG_RECORDS_ALLOCATION;return 0;}
typedef struct {durable_fixture *fixture;unsigned operation,mode,calls;} guarded_doc_fixture;
static int guarded_doc_status(guarded_doc_fixture *f,unsigned operation){++f->calls;if(f->operation!=operation)return 0;if(f->mode==3)f->fixture->native.n.f.session.failed=PG_RECORDS_ADOPTION_FAILED;return f->mode==1?8:f->mode==2||f->mode==4?7:0;}
static int guarded_doc_find(void *ctx,const pg_document_address *address,uint64_t point,struct json_object **out){guarded_doc_fixture *f=ctx;int status=pg_record_memory_find_document(&f->fixture->native.n.f.memory,address,point,out);return status?status:guarded_doc_status(f,0);}
static int guarded_doc_read(void *ctx,uint64_t id,uint64_t point,struct json_object **out){guarded_doc_fixture *f=ctx;int status=pg_record_memory_document(&f->fixture->native.n.f.memory,id,point,out);return status?status:guarded_doc_status(f,1);}
static int guarded_doc_scan(void *ctx,const pg_document_query *query,pg_memory_page *out){guarded_doc_fixture *f=ctx;int status=pg_record_memory_scan_documents(&f->fixture->native.n.f.memory,query,out);return status?status:guarded_doc_status(f,2);}
static int guarded_doc_entry(void *ctx,uint64_t id,uint64_t conversation,struct json_object **out,uint64_t *seq){guarded_doc_fixture *f=ctx;int status=pg_record_memory_entry(&f->fixture->native.n.f.memory,id,conversation,out,seq);return status?status:guarded_doc_status(f,3);}
static int guarded_doc_request_error(void *ctx,int status){(void)ctx;return status==8;}
static int guarded_doc_cancelled(void *ctx){return ((guarded_doc_fixture*)ctx)->mode==4;}
static void document_storage_guard_tests(void){
 _Static_assert(sizeof(pg_durable_documents)==48,"document guard ABI");
 for(unsigned historical=0;historical<2;++historical)for(unsigned operation=0;operation<4;++operation)for(unsigned mode=0;mode<5;++mode){if(!historical&&operation==3)continue;durable_fixture f;durable_open(&f);if(historical)durable_history_seed(&f);guarded_doc_fixture probe={&f,operation,mode,0};pg_durable_storage guard={.session=&f.session,.context=&probe,.request_error=guarded_doc_request_error,.cancelled=guarded_doc_cancelled};pg_durable_documents providers={&guard,&probe,guarded_doc_find,guarded_doc_read,guarded_doc_scan,guarded_doc_entry};pg_document_storage docs={&providers,pg_durable_document_find,pg_durable_document_read};pg_document_history_reader history={.documents=docs,.context=&providers,.entry=pg_durable_document_entry};struct json_object *value=0;int status=0;
 if(operation==2){pg_document_query query={.scope=PG_DOC_CONVERSATION,.owner=1,.at=UINT64_MAX,.limit=10};pg_memory_page page={0};status=pg_durable_document_scan(&providers,&query,&page);assert(!guard.busy&&!f.session.busy);if(mode){assert(!page.items&&!page.after&&!page.order&&!page.has_next);assert(status==(mode==1||mode==4?PG_GUARDED_STORAGE_EXEMPT:mode==2?7:PG_GUARDED_STORAGE_FAILED));}else assert(page.items);json_object_put(page.items);}
 else {if(!historical)f.native.n.f.cache.storage=docs;status=historical?pg_durable_snapshot_as_of(&f.session,&history,&history_def,&history_address,20,&value):pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value);assert(!guard.busy&&!f.session.busy);if(mode){assert(!value&&status==(mode==2||mode==3?PG_DOCUMENT_SESSION_FAILED:PG_DOC_STORAGE_FAILED));}else assert(!status&&value);json_object_put(value);}
 assert(f.native.n.f.session.failed==(mode==2?PG_RECORDS_STORAGE_FAILED:mode==3?PG_RECORDS_ADOPTION_FAILED:0));if(mode==1||mode==4){probe.mode=0;value=0;if(operation==2){pg_document_query query={.scope=PG_DOC_CONVERSATION,.owner=1,.at=UINT64_MAX,.limit=10};pg_memory_page page={0};assert(!pg_durable_document_scan(&providers,&query,&page));json_object_put(page.items);}else{assert(!(historical?pg_durable_snapshot_as_of(&f.session,&history,&history_def,&history_address,20,&value):pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value))&&value);json_object_put(value);}}
 int result=mode==2||mode==3?-1:0;assert(pg_durable_session_close(&f.session)==result);durable_closed(&f,result);
 }
 puts("PASS: 35 current/historical Document guard paths, exempt retries, scan metadata and first failure");
}
static void reader_guard_tests(void){
 for(unsigned mode=1;mode<=12;++mode){durable_fixture f;durable_open(&f);if(mode<10)durable_history_seed(&f);reader_probe p={.f=&f,.mode=mode};pg_record_memory_documents(&f.native.n.f.memory,&p.docs);pg_document_history_reader history={.documents={&p,reader_probe_find,reader_probe_read},.context=&p,.entry=reader_probe_entry};struct json_object *value=0;
 if(mode>=10)f.native.n.f.cache.storage=history.documents;
 int status=mode>=10?pg_durable_snapshot(&f.session,&f.native.n.f.definition,&f.native.n.f.address,&value):pg_durable_snapshot_as_of(&f.session,&history,&history_def,&history_address,20,&value);
 unsigned expected=mode==1?PG_DOCUMENT_ENTRY_NOT_VISIBLE:mode==9?PG_DOC_OK:mode==7||mode==8||mode==10||mode==11?PG_DOCUMENT_SESSION_FAILED:PG_DOC_STORAGE_FAILED;
 assert(status==(int)expected&&!f.session.busy);if(!status)assert(value&&!strcmp(json_object_get_string(json_object_object_get(value,"text")),"one"));else assert(!value);
 if(mode==1||mode==2||mode==3||mode==4||mode==7)assert(!p.finds&&!p.reads);if(mode==5||mode==8||mode==10)assert(p.finds==1&&!p.reads);
 if(mode>=10)assert(f.native.n.f.cache.storage.context==&p&&f.native.n.f.cache.storage.find==reader_probe_find&&f.native.n.f.cache.storage.read==reader_probe_read);
 int result=(mode==1||mode==6||mode==9||mode==12)?0:-1;assert(pg_durable_session_close(&f.session)==result);if(!status)assert(!strcmp(json_object_get_string(json_object_object_get(value,"text")),"one"));json_object_put(value);durable_closed(&f,result);
 }
 puts("PASS: snapshot guarded reads, partial outputs, first failure, no further backend calls and close waits for an admitted read");
}


static uint64_t request_id(pg_record_memory *m,uint64_t conversation,const char *key,size_t size){pg_record_request_query query={conversation,key,size};struct json_object *out=0;int status=pg_record_memory_submission_by_request(m,&query,&out);assert(status==PG_MEMORY_OK||status==PG_MEMORY_MISSING);uint64_t id=out?json_object_get_uint64(json_object_object_get(out,"id")):0;json_object_put(out);return id;}
static const char *request_seed="[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"conversation\",\"value\":{\"id\":2}},{\"type\":\"submission\",\"value\":{\"id\":10,\"conversationId\":1,\"requestId\":\"r\",\"status\":\"queued\"}},{\"type\":\"submission\",\"value\":{\"id\":20,\"conversationId\":1,\"requestId\":\"s\",\"status\":\"queued\"}}]";
static const char *request_replay="[{\"type\":\"submission\",\"value\":{\"id\":20,\"conversationId\":1,\"requestId\":\"r\",\"status\":\"queued\"}},{\"type\":\"submission\",\"value\":{\"id\":10,\"conversationId\":1,\"requestId\":\"r\",\"status\":\"queued\"}},{\"type\":\"submission\",\"value\":{\"id\":10,\"conversationId\":1,\"requestId\":\"t\",\"status\":\"queued\"}}]";
static int request_bad(void *context,const pg_record_request_query *query,struct json_object **out){pg_record_tx *tx=context;assert(pg_record_tx_commit(tx)==PG_TX_INVALID&&pg_record_tx_discard(tx)==PG_TX_INVALID);(void)query;*out=json_object_new_object();assert(*out);return 9;}
static void request_index_tests(void){
 _Static_assert(sizeof(pg_record_request_query)==24&&sizeof(pg_record_requests)==16,"request query ABI");pg_record_memory m={0};assert(!pg_record_memory_open(&m)&&!doc_commit(&m,request_seed)&&request_id(&m,1,"r",1)==10&&request_id(&m,1,"s",1)==20);
 assert(!doc_commit(&m,request_replay)&&!request_id(&m,1,"r",1)&&!request_id(&m,1,"s",1)&&request_id(&m,1,"t",1)==10);
 assert(!doc_commit(&m,"[{\"type\":\"submission\",\"value\":{\"id\":20,\"conversationId\":1,\"requestId\":\"r\",\"status\":\"queued\"}}]")&&request_id(&m,1,"r",1)==20);
 assert(!doc_commit(&m,"[{\"type\":\"submission\",\"value\":{\"id\":20,\"conversationId\":2,\"requestId\":\"r\",\"status\":\"queued\"}}]")&&!request_id(&m,1,"r",1)&&request_id(&m,2,"r",1)==20);
 assert(!doc_commit(&m,"[{\"type\":\"submission\",\"value\":{\"id\":20,\"conversationId\":2,\"status\":\"queued\"}},{\"type\":\"submission\",\"value\":{\"id\":30,\"conversationId\":1,\"requestId\":\"\",\"status\":\"queued\"}},{\"type\":\"submission\",\"value\":{\"id\":40,\"conversationId\":1.0,\"requestId\":\"k\\u0000中\",\"status\":\"queued\"}}]")&&!request_id(&m,2,"r",1)&&request_id(&m,1,0,0)==30&&request_id(&m,1,"k\0中",5)==40);
 uint64_t seq=m.next_sequence;assert(doc_commit(&m,"[{\"type\":\"submission\",\"value\":{\"id\":30,\"conversationId\":1,\"requestId\":\"wrong\",\"status\":\"queued\"}},{\"type\":\"conversation\",\"value\":{\"id\":1}}]")==PG_MEMORY_INVALID&&m.next_sequence==seq&&request_id(&m,1,0,0)==30&&!request_id(&m,1,"wrong",5));
 pg_record_session session={.storage=pg_record_memory_storage(&m)};pg_record_tx tx={0};pg_record_requests service;pg_record_memory_requests(&m,&service);assert(service.context==&m&&service.find&&!pg_record_tx_begin(&tx,&session,&m,tx_read));pg_record_request_query query={1,"k\0中",5};struct json_object *out=0;assert(!pg_record_tx_submission_by_request(&tx,&service,&query,&out)&&json_object_get_int(json_object_object_get(out,"id"))==40);assert(!json_object_object_add(out,"outside",json_object_new_int64(1)));json_object_put(out);out=0;assert(!pg_record_tx_submission_by_request(&tx,&service,&query,&out)&&!json_object_object_get(out,"outside"));json_object_put(out);out=0;
 pg_record_requests broken={&tx,request_bad};assert(pg_record_tx_submission_by_request(&tx,&broken,&query,&out)==PG_TX_READ_FAILED&&!out&&!tx.busy);
 struct json_object *write=json_tokener_parse("{\"type\":\"document.retire\",\"id\":999}");assert(write&&!pg_record_tx_stage(&tx,PG_TX_DOCUMENT_RETIRE,write)&&!pg_record_tx_submission_by_request(&tx,&service,&query,&out));json_object_put(out);out=0;json_object_put(write);write=json_object_new_object();assert(pg_record_tx_stage(&tx,PG_TX_TASK,write)==PG_TX_INVALID&&pg_record_tx_submission_by_request(&tx,&service,&query,&out)==PG_TX_READ_AFTER_WRITE&&!out);json_object_put(write);assert(!pg_record_tx_discard(&tx)&&pg_record_tx_submission_by_request(&tx,&service,&query,&out)==PG_TX_SEALED);
 assert(!pg_record_memory_close(&m)&&pg_record_memory_submission_by_request(&m,&query,&out)==PG_MEMORY_INVALID&&!out&&!pg_record_memory_open(&m)&&!request_id(&m,1,"t",1)&&!pg_record_memory_close(&m));
#ifdef PG_MEMORY_FAULTS
 unsigned total=0,failures=0;
 for(unsigned target=0;target<=total;++target){assert(!pg_record_memory_open(&m)&&!doc_commit(&m,request_seed));uint64_t before=m.next_sequence;doc_calls=0;doc_fail=target;doc_tracking=1;int status=doc_commit(&m,request_replay);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
 if(target){assert(status==PG_MEMORY_ALLOCATION&&m.next_sequence==before&&request_id(&m,1,"r",1)==10&&request_id(&m,1,"s",1)==20&&!request_id(&m,1,"t",1));assert(!doc_commit(&m,request_replay));++failures;}else {assert(!status);total=calls;}assert(!request_id(&m,1,"r",1)&&request_id(&m,1,"t",1)==10&&!pg_record_memory_close(&m));}
 printf("PASS: atomic conversation/request index replay; %u direct allocation/insert/serialization/clone failures\n",failures);
 total=0;failures=0;
 for(unsigned target=0;target<=total;++target){assert(!pg_record_memory_open(&m));doc_calls=0;doc_fail=target;doc_tracking=1;int status=doc_commit(&m,request_seed);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;
 if(target){assert(status==PG_MEMORY_ALLOCATION&&m.next_sequence==1&&!m.count&&!request_id(&m,1,"r",1));assert(!doc_commit(&m,request_seed));++failures;}else {assert(!status);total=calls;}assert(request_id(&m,1,"r",1)==10&&request_id(&m,1,"s",1)==20&&!pg_record_memory_close(&m));}
 printf("PASS: first request index and row allocation; %u direct allocation/insert/serialization/clone failures\n",failures);

 for(unsigned kind=0;kind<2;++kind){total=0;failures=0;for(unsigned target=0;target<=total;++target){assert(!pg_record_memory_open(&m)&&!doc_commit(&m,request_seed));session=(pg_record_session){.storage=pg_record_memory_storage(&m)};tx=(pg_record_tx){0};assert(!pg_record_tx_begin(&tx,&session,&m,tx_read));pg_record_memory_requests(&m,&service);query=(pg_record_request_query){1,"r",1};doc_calls=0;doc_fail=target;doc_tracking=1;int status=kind?pg_record_tx_submission_by_request(&tx,&service,&query,&out):pg_record_memory_submission_by_request(&m,&query,&out);doc_tracking=0;unsigned calls=doc_calls;doc_fail=0;if(target){assert(status!=0&&!out&&!tx.busy&&!m.busy&&request_id(&m,1,"r",1)==10);++failures;}else{assert(!status&&out);json_object_put(out);out=0;total=calls;}assert(!pg_record_tx_discard(&tx)&&!pg_record_memory_close(&m));}printf("PASS: request %s lookup; %u direct allocation/serialization/clone failures\n",kind?"transaction":"Storage",failures);}
#endif
}

int main(void){
 native_holds_tests();
 default_notifications_tests();
 default_creation_copy_notifications();
 default_notification_faults();
 default_deferred_notification_faults();
 held_notification_tests();
 native_flush_tests();
 native_merge_tests();
 native_reload_tests();
 native_finish_tests();
 native_timer_tests();
 durable_session_tests();
 reader_current_tests();
 reader_history_tests();
 reader_guard_tests();
 document_storage_guard_tests();
 durable_read_only_tests();
 durable_default_notifications_tests();
 durable_deferred_notification_tests();
#ifdef PG_MEMORY_FAULTS
 reader_faults();
#endif
 deferred_memory_transactions();
 document_notification_pipeline_tests();
 deferred_other_reads_and_documents();
 deferred_fallbacks();
 deferred_counter_cap();
#ifdef PG_MEMORY_FAULTS
 deferred_faults();
#endif
 held_content_storage();
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
 task_options_tests();
 task_create_tests();
 task_family_tests();
 storage_guard_tests();
 session_listener_tests();
 publication_tests();
 document_notice_tests();
 publication_confirm_tests();
 guarded_transaction_tests();
 guarded_query_tests();
 guarded_memory_query_tests();
 guarded_query_fault_tests();
 set_task_tests();
 task_cache_tests();
 task_cache_rebuild();
 candidate_snapshots();
 create_submission_tests();
 typed_entry_tests();
 request_index_tests();
 scan_tests();
 puts("PASS: real memory table writes, global IDs, fork-aware scans/entry/head, paging/filters, detached ownership, failure rollback and lifetime");return 0;
}
