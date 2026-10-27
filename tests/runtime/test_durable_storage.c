#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/durable_session.h"
#include "../../src/runtime/record_sqlite.h"
#include "../../src/runtime/record_jsonl.h"
#include "../../src/runtime/task_runner.h"
#include <sys/wait.h>
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <json-c/json_object_iterator.h>
_Static_assert(sizeof(struct json_object_iterator)==8,"json-c iterator ABI");
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
typedef struct {
 fixture f;pg_task_runner_host host;pg_task_definition definition;pg_task_phase phase;
 unsigned mode,calls,cleanups,available,reports,seen[16],seen_count,first_restart;
} task_case;
_Static_assert(sizeof(pg_task_runner)==112&&offsetof(pg_task_runner,error)==96,"Task runner ABI");
_Static_assert(sizeof(pg_task_definition)==80&&offsetof(pg_task_definition,migrate)==72,"Task definition ABI");
static int task_read(void *p,unsigned table,uint64_t id,struct json_object **v){task_case *t=p;return pg_record_memory_read(t->f.memory,(pg_record_table)table,id,v);}
static int task_scan(void *p,const pg_memory_query *q,pg_memory_page *page){return pg_record_memory_scan(((task_case *)p)->f.memory,q,page);}
static int task_documents(void *p,const pg_document_query *q,pg_memory_page *page){return pg_record_memory_scan_documents(((task_case *)p)->f.memory,q,page);}
static double task_now(void *p){return (double)((task_case *)p)->f.now;}
static int task_resolve(void *p,pg_slice kind,const pg_task_definition **out){task_case *t=p;*out=0;if(!t->available)return 1;assert(kind.size==7&&!memcmp(kind.data,"counter",7));*out=&t->definition;return 0;}
static int task_initial(void *p,const struct json_object *input,struct json_object **out){(void)p;(void)input;*out=json_tokener_parse("{\"phase\":\"count\",\"n\":0}");return *out?0:1;}
static int task_mint(void *p,uint64_t *id){return pg_record_memory_mint(((task_case *)p)->f.memory,id);}
static void task_cleanup(void *p,pg_task_runner *runner){task_case *t=p;t->cleanups++;runner->operation=0;}
static void task_report(void *p,uint64_t id,int error){task_case *t=p;assert(error<0&&!t->f.records.busy);struct json_object *row=0;assert(!task_read(t,PG_TABLE_TASK,id,&row));assert(strcmp(json_object_get_string(json_object_object_get(json_object_object_get(row,"state"),"status")),"running"));json_object_put(row);t->reports++;}
static int task_abort(void *p,pg_task_runner *runner,const struct json_object *record){task_case *t=p;assert(!t->f.records.busy&&json_object_get_boolean(json_object_object_get(record,"abortRequested")));if(t->mode==8)return 0;struct json_object *next=json_tokener_parse("{\"status\":\"terminal\",\"outcome\":{\"status\":\"aborted\",\"reason\":\"test\"}}");assert(next&&!pg_task_runner_commit(runner,next));json_object_put(next);return 0;}
static int task_phase(void *p,pg_task_runner *runner,const struct json_object *record){
 task_case *t=p;assert(!t->f.records.busy&&runner->busy&&runner->phase_record==record);t->calls++;
 assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(record,"state"),"status")),"running"));
 if(runner->operation)return 0;
 unsigned n=(unsigned)json_object_get_int(json_object_object_get(json_object_object_get(json_object_object_get(record,"state"),"checkpoint"),"n"));
 assert(t->seen_count<16);if(!t->seen_count)t->first_restart=(unsigned)json_object_get_int(json_object_object_get(record,"restarts"));t->seen[t->seen_count++]=n;
 if(t->mode==1)return 0;
 if(t->mode==2){runner->error=(pg_slice){"boom",4};return -17;}
 if(t->mode==6){runner->operation=t;return 1;}
 unsigned to=(unsigned)json_object_get_int(json_object_object_get(json_object_object_get(record,"input"),"to"));
 char text[256];if(t->mode==7)snprintf(text,sizeof(text),"{\"status\":\"running\",\"checkpoint\":{\"phase\":\"count\",\"n\":0.0}}");
 else if(n+1>=to)snprintf(text,sizeof(text),"{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":%u}}",n+1);
 else if(t->mode==4&&!n)snprintf(text,sizeof(text),"{\"status\":\"sleeping\",\"checkpoint\":{\"phase\":\"count\",\"n\":1},\"until\":150}");
 else snprintf(text,sizeof(text),"{\"status\":\"running\",\"checkpoint\":{\"phase\":\"count\",\"n\":%u}}",n+1);
 struct json_object *next=json_tokener_parse(text);assert(next);int cr=pg_task_runner_commit(runner,next);assert(!cr);json_object_put(next);
 if(t->mode==3&&!n){runner->operation=t;return 1;}return 0;
}
static int task_migration(void *p,const struct json_object *input,const struct json_object *checkpoint,uint64_t from,struct json_object **out){(void)p;(void)input;(void)checkpoint;assert(from==1);*out=json_tokener_parse("{\"input\":{\"to\":3},\"checkpoint\":{\"phase\":\"count\",\"n\":1}}");return *out?0:1;}
static void task_bind(task_case *t){
 t->available=1;t->host=(pg_task_runner_host){t,task_read,task_scan,task_documents,task_resolve,task_now,task_report};
 t->phase=(pg_task_phase){{"count",5},t,task_phase,task_cleanup};
 t->definition=(pg_task_definition){.name={"counter",7},.version=1,.context=t,.initial=task_initial,.phases=&t->phase,.phase_count=1,.abort=task_abort,.abort_cleanup=task_cleanup};
}
static uint64_t task_create_at(task_case *t,uint64_t owner,uint64_t conversation,unsigned to){
 pg_record_tx tx={0};pg_document_tx docs={0};assert(!pg_durable_session_begin(&t->f.session,&tx,&docs,t->f.memory,read_record));
 struct json_object *def=json_tokener_parse("{\"name\":\"counter\",\"version\":1}"),*input=json_object_new_object(),*options=json_object_new_object(),*ownership=json_object_new_object();
 assert(def&&input&&options&&ownership&&!json_object_object_add(input,"to",json_object_new_int((int)to)));
 assert(!json_object_object_add(ownership,"kind",json_object_new_string(owner?"task":"conversation")));
 if(owner)assert(!json_object_object_add(ownership,"taskId",json_object_new_int64((int64_t)owner)));
 assert(!json_object_object_add(options,"ownership",ownership));
 pg_task_creation services={.context=t->definition.context,.initial=t->definition.initial,.mint=task_mint,.conversation_id=conversation};uint64_t id=0;
 assert(!pg_record_tx_create_task(&tx,def,input,options,&services,&id)&&!pg_durable_session_settle(&t->f.session,0,0,0));
 json_object_put(def);json_object_put(input);json_object_put(options);return id;
}
static uint64_t task_create(task_case *t,uint64_t owner,unsigned to){return task_create_at(t,owner,1,to);}
static struct json_object *task_row(task_case *t,uint64_t id){struct json_object *j=0;assert(!task_read(t,PG_TABLE_TASK,id,&j));return j;}
static void task_finish(task_case *t,pg_task_runner *runner){for(unsigned i=0;i<12;i++){int r=pg_task_runner_poll(runner);if(r==PG_TASK_DONE)return;assert(r==PG_TASK_PROGRESS||r==PG_TASK_PENDING);}assert(!"task did not settle");(void)t;}
static void task_tests(void){
 for(unsigned kind=0;kind<2;kind++)for(unsigned mode=0;mode<9;mode++){
  char parent[]="/tmp/pangu-task-XXXXXX";assert(mkdtemp(parent));task_case t={.f={.kind=kind,.now=100},.mode=mode};snprintf(t.f.path,sizeof(t.f.path),"%s/%s",parent,kind?".pangu":"pangu.sqlite");backend_open(&t.f);seed(&t.f);open_session(&t.f);task_bind(&t);
  uint64_t id=task_create(&t,0,3);pg_task_runner runner={0};assert(!pg_task_runner_open(&runner,&t.f.session,id,&t.host,3));
  if(mode==5){t.available=0;assert(pg_task_runner_poll(&runner)==PG_TASK_BLOCKED&&!t.calls);t.available=1;t.definition.version=2;t.definition.migrate=task_migration;}
  if(mode==4){assert(pg_task_runner_poll(&runner)==PG_TASK_PENDING&&t.calls==1);assert(pg_task_runner_poll(&runner)==PG_TASK_PENDING&&t.calls==1);t.f.now=150;}
  if(mode==6||mode==8){if(mode==6)assert(pg_task_runner_poll(&runner)==PG_TASK_PENDING);struct json_object *row=task_row(&t,id);assert(!json_object_object_add(row,"abortRequested",json_object_new_boolean(1)));pg_record_tx tx={0};pg_document_tx docs={0};assert(!pg_durable_session_begin(&t.f.session,&tx,&docs,t.f.memory,read_record)&&!pg_record_tx_set_task(&tx,row,&t,task_now)&&!pg_durable_session_settle(&t.f.session,0,0,0));json_object_put(row);}
  task_finish(&t,&runner);struct json_object *row=task_row(&t,id),*state=json_object_object_get(row,"state"),*outcome=json_object_object_get(state,"outcome");assert(!strcmp(json_object_get_string(json_object_object_get(state,"status")),"terminal"));
  const char *expected=mode==1||mode==2||mode==7||mode==8?"faulted":mode==6?"aborted":"completed";assert(!strcmp(json_object_get_string(json_object_object_get(outcome,"status")),expected));
  if(mode==1||mode==2||mode==7||mode==8)assert(t.reports==1);else if(mode!=6)assert(json_object_get_int(json_object_object_get(outcome,"result"))==3);
  if(mode==5)assert(json_object_get_int(json_object_object_get(row,"version"))==2&&t.seen[0]==1);
  if(mode==1||mode==7)assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(outcome,"error"),"message")),"Task counter phase count returned without durable progress"));
  json_object_put(row);assert(!pg_task_runner_close(&runner)&&!pg_task_runner_close(&runner));assert(!pg_durable_session_close(&t.f.session));backend_open(&t.f);row=task_row(&t,id);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(row,"state"),"status")),"terminal"));json_object_put(row);assert(!(kind?pg_record_jsonl_close(&t.f.backend.jsonl):pg_record_sqlite_close(&t.f.backend.sql)));cleanup(&t.f,parent);
 }
 for(unsigned kind=0;kind<2;kind++)for(unsigned interruption=0;interruption<3;interruption++){
  char parent[]="/tmp/pangu-task-recovery-XXXXXX";assert(mkdtemp(parent));task_case t={.f={.kind=kind,.now=100},.mode=interruption==2?6:3};snprintf(t.f.path,sizeof(t.f.path),"%s/%s",parent,kind?".pangu":"pangu.sqlite");backend_open(&t.f);seed(&t.f);open_session(&t.f);task_bind(&t);uint64_t id=task_create(&t,0,3);pg_task_runner runner={0};
  if(interruption){assert(!pg_durable_session_close(&t.f.session));pid_t child=fork();assert(child>=0);if(!child){backend_open(&t.f);open_session(&t.f);assert(!pg_task_runner_open(&runner,&t.f.session,id,&t.host,3));assert(pg_task_runner_poll(&runner)==PG_TASK_PENDING);_exit(0);}int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));}
  else{assert(!pg_task_runner_open(&runner,&t.f.session,id,&t.host,3));assert(pg_task_runner_poll(&runner)==PG_TASK_PENDING);assert(!pg_task_runner_close(&runner));struct json_object *parked=task_row(&t,id);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(parked,"state"),"status")),"pending")&&!json_object_object_get(parked,"restarts"));json_object_put(parked);assert(!pg_durable_session_close(&t.f.session));}
  t.mode=0;t.seen_count=0;t.f.now=200;backend_open(&t.f);open_session(&t.f);assert(!pg_task_runner_open(&runner,&t.f.session,id,&t.host,interruption==2?1:3));
  if(interruption==2){assert(pg_task_runner_poll(&runner)==PG_TASK_BLOCKED&&!t.calls);struct json_object *parked=task_row(&t,id);assert(json_object_get_int(json_object_object_get(parked,"restarts"))==1);json_object_put(parked);}
  else{task_finish(&t,&runner);assert(t.seen[0]==1&&t.first_restart==(interruption?1u:0u));struct json_object *done=task_row(&t,id);assert(json_object_get_double(json_object_object_get(done,"startedAt"))==100&&json_object_get_double(json_object_object_get(done,"endedAt"))==200&&!json_object_object_get(done,"restarts"));json_object_put(done);}
  assert(!pg_task_runner_close(&runner)&&!pg_durable_session_close(&t.f.session));cleanup(&t.f,parent);
 }
 for(unsigned kind=0;kind<2;kind++)for(unsigned nested=0;nested<2;nested++){
  char parent[]="/tmp/pangu-task-owned-XXXXXX";assert(mkdtemp(parent));task_case t={.f={.kind=kind,.now=100}};snprintf(t.f.path,sizeof(t.f.path),"%s/%s",parent,kind?".pangu":"pangu.sqlite");backend_open(&t.f);seed(&t.f);open_session(&t.f);task_bind(&t);
  uint64_t owner=task_create(&t,0,1),conversation=1,child=0;
  if(nested){pg_record_tx tx={0};pg_document_tx docs={0};struct json_object *c=0;assert(!pg_durable_session_begin(&t.f.session,&tx,&docs,t.f.memory,read_record)&&!pg_record_tx_create_conversation(&tx,owner,0,(int(*)(void *,uint64_t *))pg_record_memory_mint,&c));conversation=(uint64_t)json_object_get_int64(json_object_object_get(c,"id"));json_object_put(c);assert(!pg_durable_session_settle(&t.f.session,0,0,0));}
  child=task_create_at(&t,nested?0:owner,conversation,1);
  pg_task_runner a={0},b={0};assert(!pg_task_runner_open(&a,&t.f.session,owner,&t.host,3)&&!pg_task_runner_open(&b,&t.f.session,child,&t.host,3));assert(pg_task_runner_poll(&a)==PG_TASK_PENDING);struct json_object *row=task_row(&t,owner);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(row,"state"),"status")),"completing"));json_object_put(row);
  assert(pg_task_runner_poll(&b)==PG_TASK_DONE&&pg_task_runner_poll(&a)==PG_TASK_DONE);assert(!pg_task_runner_close(&a)&&!pg_task_runner_close(&b)&&!pg_durable_session_close(&t.f.session));cleanup(&t.f,parent);
 }
 puts("PASS: durable Task phases, cooperative pending, sleep/wake, migration, fault/abort and outcome reopen on SQLite/JSONL");
}
int main(void){
 task_tests();
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
