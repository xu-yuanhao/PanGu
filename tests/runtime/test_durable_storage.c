#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/durable_session.h"
#include "../../src/runtime/record_sqlite.h"
#include "../../src/runtime/record_jsonl.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <sqlite3.h>
typedef struct {
 union {pg_record_sqlite sql;pg_record_jsonl jsonl;} backend;
 pg_record_memory *memory;
 pg_record_session records;
 pg_durable_session session;
 pg_document_cache cache;
 pg_document_holds holds;
 pg_hold_timer timer;
 pg_record_tx tx;
 pg_document_tx docs;
 pg_document_definition definition;
 pg_document_address address;
 pg_timer_job fire,queued;
 void *fire_context,*queued_context;
 unsigned kind,armed,closed;
 uint64_t now;
 char path[512];
} fixture;
static uint64_t clock_now(void *p){return ((fixture *)p)->now;}
static int read_record(void *p,unsigned table,uint64_t id,struct json_object **v){return pg_record_memory_read(p,(pg_record_table)table,id,v);}
static int arm(void *p,uint64_t delay,pg_timer_job fire,void *context,void **handle){fixture *f=p;assert(delay&&!f->armed);f->fire=fire;f->fire_context=context;f->armed=1;*handle=f;return 0;}
static void cancel(void *p,void *handle){fixture *f=p;assert(handle==f&&f->armed);f->armed=0;}
static int enqueue(void *p,pg_timer_job job,void *context){fixture *f=p;assert(!f->queued);f->queued=job;f->queued_context=context;return 0;}
static int backend_close(void *p){fixture *f=p;assert(f->cache.closed&&!f->holds.active&&!f->armed&&!f->queued);++f->closed;return (f->kind?pg_record_jsonl_close(&f->backend.jsonl):pg_record_sqlite_close(&f->backend.sql))?-1:0;}
static void backend_open(fixture *f){
 assert(!(f->kind?pg_record_jsonl_open(&f->backend.jsonl,f->path,1):pg_record_sqlite_open(&f->backend.sql,f->path,0)));
 f->memory=f->kind?&f->backend.jsonl.memory:&f->backend.sql.memory;
 f->records=(pg_record_session){.storage=f->kind?pg_record_jsonl_storage(&f->backend.jsonl):pg_record_sqlite_storage(&f->backend.sql),.sequence=f->memory->next_sequence-1};
}
static void seed(fixture *f){
 struct json_object *j=json_tokener_parse("[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"document.create\",\"record\":{\"id\":2,\"kind\":\"checkpoint\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"latest\",\"fork\":\"initial\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"text\":\"seed\"}}}]");
 assert(j&&!pg_records_commit(&f->records,j));json_object_put(j);
}
static void open_session(fixture *f){
 pg_document_storage storage;pg_record_memory_documents(f->memory,&storage);
 pg_document_cache_hooks hooks={.context=f,.clock=clock_now};assert(!pg_document_cache_open(&f->cache,&storage,&hooks,0));
 assert(!pg_document_holds_open(&f->holds,&f->cache,f,clock_now,0));
 pg_hold_timer_host timer={f,arm,cancel,enqueue};assert(!pg_hold_timer_open(&f->timer,&f->holds,&f->records,&timer));
 pg_durable_close close={f,backend_close};assert(!pg_durable_session_open(&f->session,&f->records,&f->cache,&f->holds,&f->timer,&close));
 f->definition=(pg_document_definition){.kind={"checkpoint",10},.version=1,.scope=PG_DOC_CONVERSATION,.history=PG_DOC_LATEST,.fork=PG_DOC_INITIAL};
 f->address=(pg_document_address){.kind={"checkpoint",10},.owner=1,.scope=PG_DOC_CONVERSATION};
}
static void edit(fixture *f,const char *suffix){
 assert(!pg_durable_session_begin(&f->session,&f->tx,&f->docs,f->memory,read_record));
 pg_document_change *change=0;assert(!pg_document_tx_get(&f->docs,&f->definition,&f->address,0,0,&change));
 char text[100];snprintf(text,sizeof(text),"[[\"a\",[\"text\"],\"%s\"]]",suffix);struct json_object *ops=json_tokener_parse(text);assert(ops&&!pg_document_change_append(change,ops));json_object_put(ops);
}
static int migrate(void *context,struct json_object *value,uint64_t version,struct json_object **out){
 (void)context;assert(version==1);*out=json_object_get(value);return json_object_object_add(*out,"text",json_object_new_string("seed+migrated"));
}
static void value(fixture *f,const char *expected){struct json_object *v=0;assert(!pg_record_memory_document(f->memory,2,UINT64_MAX,&v));assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(v,"value"),"text")),expected));json_object_put(v);}
static void cleanup(fixture *f,const char *parent){
 if(f->kind){DIR *d=opendir(f->path);assert(d);struct dirent *e;while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;char p[800];snprintf(p,sizeof(p),"%s/%s",f->path,e->d_name);assert(!unlink(p));}assert(!closedir(d)&&!rmdir(f->path));}else assert(!unlink(f->path));assert(!rmdir(parent));
}
int main(void){
 for(unsigned kind=0;kind<2;kind++)for(unsigned mode=0;mode<6;mode++){
  char parent[]="/tmp/pangu-durable-store-XXXXXX";assert(mkdtemp(parent));fixture f={.kind=kind,.now=100};snprintf(f.path,sizeof(f.path),"%s/%s",parent,kind?".pangu":"pangu.sqlite");backend_open(&f);seed(&f);open_session(&f);
  if(mode==5){f.definition.version=2;f.definition.migrate=migrate;}
  edit(&f,"+held");int settled=pg_durable_session_settle(&f.session,1,10,0);assert(!settled);if(mode==5)assert(!f.holds.count&&!f.armed);else{assert(f.holds.count==1&&f.armed);value(&f,"seed");}
  const char *expected=mode==5?"seed+migrated+held":"seed+held";
  if(mode==1){edit(&f,"+mixed");struct json_object *j=json_tokener_parse("{\"type\":\"entry\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"checkpoint\"}}");assert(j&&!pg_record_tx_stage(&f.tx,PG_TX_ENTRY,j));json_object_put(j);assert(!pg_durable_session_settle(&f.session,0,0,0)&&!f.holds.count);expected="seed+held+mixed";}
  if(mode==2){edit(&f,"+closing");assert(pg_durable_session_close(&f.session)==1&&!f.closed);assert(!pg_durable_session_settle(&f.session,0,0,0));expected="seed+held+closing";}
  if(mode==3){f.fire(f.fire_context);assert(f.queued);assert(pg_durable_session_close(&f.session)==1&&!f.closed);pg_timer_job job=f.queued;void *ctx=f.queued_context;f.queued=0;f.queued_context=0;job(ctx);}
  if(mode==4){f.records.failed=PG_RECORDS_STORAGE_FAILED;expected="seed";}
  int result=mode==4?-1:0;assert(pg_durable_session_close(&f.session)==result&&f.closed==1&&f.memory->closed&&f.cache.closed&&!f.holds.active&&!f.armed);assert(pg_durable_session_close(&f.session)==result&&f.closed==1);
  backend_open(&f);value(&f,expected);if(mode==1){struct json_object *row=0;assert(!pg_record_memory_read(f.memory,PG_TABLE_ENTRY,3,&row));json_object_put(row);}assert(!(kind?pg_record_jsonl_close(&f.backend.jsonl):pg_record_sqlite_close(&f.backend.sql)));cleanup(&f,parent);
 }
 puts("PASS: SQLite/JSONL Durable Session deferred close, mixed commit, admitted close, queued timer drainage failed close and migration reopen");return 0;
}
