#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/durable_session.h"
#include "../../src/runtime/record_sqlite.h"
#include "../../src/runtime/record_jsonl.h"
#include "../../src/runtime/task_runner.h"
#include "../../src/runtime/durable_tool.h"
#include "../../src/runtime/read_tool.h"
#include "../../src/runtime/write_tool.h"
#include "../../src/runtime/bash_tool.h"
#include <time.h>
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
 fixture f;pg_task_runner_host host;pg_task_definition definition;pg_task_phase phase;pg_task_scheduler *scheduler;
 unsigned mode,calls,cleanups,available,reports,seen[1024],seen_count,first_restart;
} task_case;
_Static_assert(sizeof(pg_task_runner)==120&&offsetof(pg_task_runner,error)==96&&offsetof(pg_task_runner,mutation)==112,"Task runner ABI");
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
 assert(t->seen_count<1024);if(!t->seen_count)t->first_restart=(unsigned)json_object_get_int(json_object_object_get(record,"restarts"));t->seen[t->seen_count++]=n;
 if(t->mode==1)return 0;
 if(t->mode==2){runner->error=(pg_slice){"boom",4};return -17;}
 if(t->mode==6){runner->operation=t;return 1;}
 unsigned to=(unsigned)json_object_get_int(json_object_object_get(json_object_object_get(record,"input"),"to"));
 char text[256];if(t->mode==7)snprintf(text,sizeof(text),"{\"status\":\"running\",\"checkpoint\":{\"phase\":\"count\",\"n\":0.0}}");
 else if(n+1>=to)snprintf(text,sizeof(text),"{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":%u}}",n+1);
 else if(t->mode==4&&!n)snprintf(text,sizeof(text),"{\"status\":\"sleeping\",\"checkpoint\":{\"phase\":\"count\",\"n\":1},\"until\":150}");
 else snprintf(text,sizeof(text),"{\"status\":\"running\",\"checkpoint\":{\"phase\":\"count\",\"n\":%u}}",n+1);
 struct json_object *next=json_tokener_parse(text);assert(next);int cr=pg_task_runner_commit(runner,next);assert(!cr);json_object_put(next);
 if(t->mode==9){assert(t->scheduler&&pg_task_scheduler_close(t->scheduler)==PG_TASK_PENDING);}
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
static void scheduler_tests(void){
 _Static_assert(sizeof(pg_task_scheduler)==72&&offsetof(pg_task_scheduler,next_wake)==48,"Scheduler ABI");
 for(unsigned kind=0;kind<2;kind++)for(unsigned scenario=0;scenario<5;scenario++){
  char parent[]="/tmp/pangu-scheduler-XXXXXX";assert(mkdtemp(parent));task_case t={.f={.kind=kind,.now=100}};
  snprintf(t.f.path,sizeof(t.f.path),"%s/%s",parent,kind?".pangu":"pangu.sqlite");backend_open(&t.f);seed(&t.f);open_session(&t.f);task_bind(&t);
  unsigned count=scenario==3?260:3;uint64_t ids[260];for(unsigned i=0;i<count;i++)ids[i]=task_create(&t,0,scenario>=3?1:3);
  pg_task_scheduler scheduler={0};t.scheduler=&scheduler;
  if(scenario==4)t.mode=9;
  if(scenario==2){
   assert(!pg_durable_session_close(&t.f.session));pid_t child=fork();assert(child>=0);
   if(!child){backend_open(&t.f);open_session(&t.f);t.mode=6;assert(!pg_task_scheduler_open(&scheduler,&t.f.session,&t.host,3));assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_PENDING&&t.calls==count);_exit(0);}
   int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));backend_open(&t.f);open_session(&t.f);
  }
  if(scenario==1){t.mode=4;t.available=0;}
  assert(!pg_task_scheduler_open(&scheduler,&t.f.session,&t.host,3)&&scheduler.count==count&&!t.calls);
  if(scenario==2)for(unsigned i=0;i<count;i++){struct json_object *row=task_row(&t,ids[i]);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(row,"state"),"status")),"pending")&&json_object_get_int(json_object_object_get(row,"restarts"))==1);json_object_put(row);}
  if(scenario==1){
   assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_BLOCKED&&!t.calls);t.available=1;
   assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_PENDING&&t.calls==count&&scheduler.next_wake==150);
   assert(!pg_task_scheduler_close(&scheduler)&&!pg_task_scheduler_close(&scheduler));
   assert(!pg_task_scheduler_open(&scheduler,&t.f.session,&t.host,3));assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_PENDING&&t.calls==count);
   ids[count++]=task_create(&t,0,1);assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_PROGRESS&&t.calls==count);
   t.f.now=150;
  }
  if(scenario==4){assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_CLOSED&&t.calls==1&&scheduler.count==2);assert(!pg_task_scheduler_close(&scheduler)&&!pg_task_scheduler_close(&scheduler)&&!pg_durable_session_close(&t.f.session));cleanup(&t.f,parent);continue;}
  int result=PG_TASK_PENDING;for(unsigned i=0;i<10&&result!=PG_TASK_DONE;i++){result=pg_task_scheduler_poll(&scheduler);assert(result<=PG_TASK_BLOCKED);}
  assert(result==PG_TASK_DONE&&!scheduler.count);
  if(scenario==0){assert(t.seen_count==9);for(unsigned i=0;i<9;i++)assert(t.seen[i]==i/3);}
  for(unsigned i=0;i<count;i++){struct json_object *row=task_row(&t,ids[i]);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(row,"state"),"status")),"terminal"));json_object_put(row);}
  assert(!pg_task_scheduler_close(&scheduler)&&!pg_durable_session_close(&t.f.session));cleanup(&t.f,parent);
 }
 puts("PASS: Durable Scheduler stable phase fairness, live discovery/pagination, registry unblock, sleep deadline and all-task crash recovery on SQLite/JSONL");
}
typedef struct {
 task_case base;uint64_t ids[8];unsigned count,behavior[8],policy[8],calls[8],resumes[8],gate,aborted[8],abort_count,check_marks;
 struct json_object *on[8];pg_task_runner *round_runner;uint64_t round_entry;unsigned round_kind;
} wait_case;
static unsigned wait_index(wait_case *w,uint64_t id){for(unsigned i=0;i<w->count;i++)if(w->ids[i]==id)return i;assert(!"unknown task");return 0;}
static pg_task_status phase_round_stage(void *p,pg_record_tx *tx,pg_document_tx *docs,struct json_object **next){
 wait_case *w=p;task_case *t=&w->base;pg_document_change *change=0;
 assert(!pg_document_tx_get(docs,&t->f.definition,&t->f.address,0,0,&change));
 struct json_object *ops=json_tokener_parse("[[\"a\",[\"text\"],\"+phase\"]]");assert(ops&&!pg_document_change_append(change,ops));json_object_put(ops);
 struct json_object *draft=json_tokener_parse("{\"kind\":\"pangu.assistant\",\"model\":[{\"role\":\"assistant\",\"content\":[]}]}");struct json_object *entry=0;
 assert(draft&&!pg_record_tx_append_entry(tx,1,draft,w->round_runner->id,task_mint,&entry));w->round_entry=(uint64_t)json_object_get_int64(json_object_object_get(entry,"id"));json_object_put(draft);json_object_put(entry);
 if(w->round_kind==3)return PG_TASK_PROGRESS;
 struct json_object *definition=json_tokener_parse("{\"name\":\"counter\",\"version\":1}"),*input=json_tokener_parse("{\"to\":1}"),*options=json_tokener_parse("{\"ownership\":{\"kind\":\"task\"}}");assert(definition&&input&&options);
 assert(!json_object_object_add(json_object_object_get(options,"ownership"),"taskId",json_object_new_int64((int64_t)w->round_runner->id)));
 pg_task_creation creation={.context=t,.initial=task_initial,.mint=task_mint,.conversation_id=1};uint64_t child=0;assert(!pg_record_tx_create_task(tx,definition,input,options,&creation,&child));json_object_put(definition);json_object_put(input);json_object_put(options);
 w->ids[w->count]=child;w->behavior[w->count++]=0;
 if(w->round_kind==2)*next=json_tokener_parse("{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":null}}");
 else{char text[256];snprintf(text,sizeof(text),"{\"status\":\"waiting\",\"checkpoint\":{\"phase\":\"count\",\"n\":1},\"on\":[%llu],\"policy\":\"allSettled\"}",(unsigned long long)(w->round_kind==4?w->round_runner->id:child));*next=json_tokener_parse(text);}
 assert(*next);return w->round_kind==1?PG_TASK_INVALID:PG_TASK_PROGRESS;
}
static int wait_phase(void *p,pg_task_runner *runner,const struct json_object *record){
 wait_case *w=p;unsigned i=wait_index(w,runner->id);w->calls[i]++;assert(!w->base.f.records.busy);
 unsigned n=(unsigned)json_object_get_int(json_object_object_get(json_object_object_get(json_object_object_get(record,"state"),"checkpoint"),"n"));
 if(w->behavior[i]==2&&!w->gate)return 1;
 if(w->behavior[i]==4&&!n){w->round_runner=runner;uint64_t before=w->base.f.records.sequence;int r=pg_task_runner_commit_with(runner,w,phase_round_stage);struct json_object *entry=0;
  assert(!runner->mutation&&pg_durable_session_idle(&w->base.f.session));
  if(r){assert(r==PG_TASK_INVALID&&w->base.f.records.sequence==before&&!w->base.f.records.failed);value(&w->base.f,"seed");assert(task_read(&w->base,PG_TABLE_ENTRY,w->round_entry,&entry)==PG_MEMORY_MISSING&&!entry);assert(task_read(&w->base,PG_TABLE_TASK,w->ids[1],&entry)==PG_MEMORY_MISSING&&!entry);return -r;}
  assert(w->base.f.records.sequence==before+1);value(&w->base.f,"seed+phase");assert(!task_read(&w->base,PG_TABLE_ENTRY,w->round_entry,&entry));json_object_put(entry);if(w->round_kind!=3)return 0;
 }

 struct json_object *next=0;
 if(w->behavior[i]==1&&!n){
  next=json_tokener_parse("{\"status\":\"waiting\",\"checkpoint\":{\"phase\":\"count\",\"n\":1}}");assert(next&&w->on[i]);
  assert(!json_object_object_add(next,"on",json_object_get(w->on[i]))&&!json_object_object_add(next,"policy",json_object_new_string(w->policy[i]?"failFast":"allSettled")));
 }else if(w->behavior[i]==3)next=json_tokener_parse("{\"status\":\"terminal\",\"outcome\":{\"status\":\"failed\",\"error\":{\"message\":\"test failed\"}}}");
 else{if(w->behavior[i]==1)w->resumes[i]++;next=json_tokener_parse("{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":null}}");}
 assert(next);int result=pg_task_runner_commit(runner,next);json_object_put(next);return result?-(int)result:0;
}
static int wait_abort(void *p,pg_task_runner *runner,const struct json_object *record){
 wait_case *w=p;unsigned i=wait_index(w,runner->id);(void)record;assert(!w->base.f.records.busy&&w->abort_count<8);
 if(w->check_marks)for(unsigned j=0;j<4;j++){struct json_object *r=task_row(&w->base,w->ids[j]);assert(json_object_get_boolean(json_object_object_get(r,"abortRequested")));json_object_put(r);}
 w->aborted[w->abort_count++]=i;struct json_object *next=json_tokener_parse("{\"status\":\"terminal\",\"outcome\":{\"status\":\"aborted\"}}");assert(next);int result=pg_task_runner_commit(runner,next);json_object_put(next);return result?-(int)result:0;
}
static void wait_spawn(wait_case *w,uint64_t owner,uint64_t conversation,unsigned behavior){unsigned i=w->count++;assert(i<8);w->ids[i]=task_create_at(&w->base,owner,conversation,1);w->behavior[i]=behavior;}
static void wait_on(wait_case *w,unsigned i,unsigned policy,unsigned count,const unsigned *targets){w->policy[i]=policy;w->on[i]=json_object_new_array();assert(w->on[i]);for(unsigned j=0;j<count;j++)assert(!json_object_array_add(w->on[i],json_object_new_int64((int64_t)w->ids[targets[j]])));}
static void wait_save(wait_case *w,struct json_object *record){pg_record_tx tx={0};pg_document_tx docs={0};assert(!pg_durable_session_begin(&w->base.f.session,&tx,&docs,w->base.f.memory,read_record)&&!pg_record_tx_set_task(&tx,record,&w->base,task_now)&&!pg_durable_session_settle(&w->base.f.session,0,0,0));}
static void wait_background(wait_case *w,unsigned i){struct json_object *row=task_row(&w->base,w->ids[i]);assert(!json_object_object_add(row,"background",json_object_new_boolean(1)));wait_save(w,row);json_object_put(row);}
static uint64_t wait_conversation(wait_case *w,uint64_t owner){pg_record_tx tx={0};pg_document_tx docs={0};struct json_object *c=0;assert(!pg_durable_session_begin(&w->base.f.session,&tx,&docs,w->base.f.memory,read_record)&&!pg_record_tx_create_conversation(&tx,owner,0,(int(*)(void *,uint64_t *))pg_record_memory_mint,&c));uint64_t id=(uint64_t)json_object_get_int64(json_object_object_get(c,"id"));json_object_put(c);assert(!pg_durable_session_settle(&w->base.f.session,0,0,0));return id;}
static const char *wait_outcome(wait_case *w,unsigned i){static char text[32];struct json_object *r=task_row(&w->base,w->ids[i]);snprintf(text,sizeof(text),"%s",json_object_get_string(json_object_object_get(json_object_object_get(json_object_object_get(r,"state"),"outcome"),"status")));json_object_put(r);return text;}
static void wait_tests(void){
 for(unsigned kind=0;kind<2;kind++)for(unsigned scenario=0;scenario<19;scenario++){
  char parent[]="/tmp/pangu-wait-XXXXXX";assert(mkdtemp(parent));wait_case w={.base={.f={.kind=kind,.now=100}}};task_case *t=&w.base;
  snprintf(t->f.path,sizeof(t->f.path),"%s/%s",parent,kind?".pangu":"pangu.sqlite");backend_open(&t->f);seed(&t->f);open_session(&t->f);task_bind(t);
  t->phase.context=&w;t->phase.poll=wait_phase;t->phase.cleanup=0;t->definition.context=&w;t->definition.abort=wait_abort;t->definition.abort_cleanup=0;
  if(scenario>=14){w.round_kind=scenario-14;wait_spawn(&w,0,1,4);}
  else if(scenario<2){wait_spawn(&w,0,1,1);wait_spawn(&w,w.ids[0],1,3);wait_spawn(&w,w.ids[0],1,2);wait_on(&w,0,scenario,2,(unsigned[]){1,2});}
  else if(scenario==2){wait_spawn(&w,0,1,1);wait_on(&w,0,0,1,(unsigned[]){0});}
  else if(scenario==3){wait_spawn(&w,0,1,1);w.on[0]=json_tokener_parse("[999999]");}
  else if(scenario==4){wait_spawn(&w,0,1,1);wait_spawn(&w,0,1,2);wait_on(&w,0,1,1,(unsigned[]){1});w.gate=1;}
  else if(scenario==5||scenario==6){wait_spawn(&w,0,1,1);wait_spawn(&w,scenario==6?w.ids[0]:0,1,1);wait_on(&w,0,0,1,(unsigned[]){1});wait_on(&w,1,0,1,(unsigned[]){0});}
  else if(scenario==7){wait_spawn(&w,0,1,1);uint64_t c=wait_conversation(&w,w.ids[0]);wait_spawn(&w,0,c,1);wait_on(&w,0,0,1,(unsigned[]){1});wait_on(&w,1,0,1,(unsigned[]){0});}
  else if(scenario==8||scenario==13){if(scenario==13)w.gate=1;wait_spawn(&w,0,1,2);uint64_t c=wait_conversation(&w,w.ids[0]);wait_spawn(&w,0,c,1);wait_background(&w,1);wait_on(&w,1,0,1,(unsigned[]){0});}
  else if(scenario==9){for(unsigned i=0;i<4;i++)wait_spawn(&w,0,1,i==3?0:1);wait_on(&w,0,0,2,(unsigned[]){1,2});wait_on(&w,1,0,1,(unsigned[]){3});wait_on(&w,2,0,1,(unsigned[]){3});}
  else if(scenario==10){wait_spawn(&w,0,1,2);wait_spawn(&w,w.ids[0],1,2);wait_spawn(&w,w.ids[1],1,2);uint64_t c=wait_conversation(&w,w.ids[0]);wait_spawn(&w,0,c,2);wait_spawn(&w,0,c,2);wait_background(&w,4);w.check_marks=1;}
  else if(scenario==12){wait_spawn(&w,0,1,1);wait_spawn(&w,w.ids[0],1,3);wait_spawn(&w,w.ids[0],1,2);wait_spawn(&w,w.ids[1],1,2);wait_spawn(&w,w.ids[0],1,2);wait_on(&w,0,1,2,(unsigned[]){1,2});}
  else{wait_spawn(&w,0,1,1);wait_spawn(&w,w.ids[0],1,2);wait_on(&w,0,0,1,(unsigned[]){1});}
  pg_task_scheduler scheduler={0};assert(!pg_task_scheduler_open(&scheduler,&t->f.session,&t->host,3));
  int result=pg_task_scheduler_poll(&scheduler);assert(result<=PG_TASK_BLOCKED);
  if(scenario<2){assert(w.calls[0]==1&&w.calls[2]==1&&!w.abort_count);result=pg_task_scheduler_poll(&scheduler);assert(result<=PG_TASK_BLOCKED);if(!scenario)assert(!w.abort_count&&!w.resumes[0]);else assert(w.abort_count==1&&w.aborted[0]==2);w.gate=1;}
  if(scenario==8){assert(!w.resumes[1]);w.gate=1;}
  if(scenario==10){struct json_object *r=task_row(t,w.ids[0]);assert(!json_object_object_add(r,"abortRequested",json_object_new_boolean(1)));wait_save(&w,r);json_object_put(r);for(unsigned j=0;j<5;j++)assert(pg_task_scheduler_poll(&scheduler)<=PG_TASK_BLOCKED);assert(w.abort_count==4&&w.aborted[0]==2&&w.aborted[3]==0);w.gate=1;}
  if(scenario==11){assert(!w.resumes[0]&&!pg_task_scheduler_close(&scheduler)&&!pg_durable_session_close(&t->f.session));backend_open(&t->f);open_session(&t->f);assert(!pg_task_scheduler_open(&scheduler,&t->f.session,&t->host,3));assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_PENDING&&!w.resumes[0]);w.gate=1;}
  if(scenario==12){for(unsigned j=0;j<4;j++)assert(pg_task_scheduler_poll(&scheduler)<=PG_TASK_BLOCKED);assert(w.abort_count==2&&w.resumes[0]==1);struct json_object *r=task_row(t,w.ids[4]);assert(!json_object_get_boolean(json_object_object_get(r,"abortRequested")));json_object_put(r);w.gate=1;}
  for(unsigned j=0;j<20&&result!=PG_TASK_DONE;j++){result=pg_task_scheduler_poll(&scheduler);assert(result<=PG_TASK_BLOCKED);}assert(result==PG_TASK_DONE);
  if(scenario>=14){assert(!strcmp(wait_outcome(&w,0),scenario==14||scenario==17?"completed":"faulted"));value(&t->f,scenario==14||scenario==17?"seed+phase":"seed");}
  if(scenario<2)assert(!strcmp(wait_outcome(&w,0),"completed")&&!strcmp(wait_outcome(&w,1),"failed")&&!strcmp(wait_outcome(&w,2),scenario?"aborted":"completed")&&w.resumes[0]==1);
  if(scenario>=2&&scenario<=4){assert(!strcmp(wait_outcome(&w,0),"faulted"));struct json_object *r=task_row(t,w.ids[0]);const char *message=json_object_get_string(json_object_object_get(json_object_object_get(json_object_object_get(json_object_object_get(r,"state"),"outcome"),"error"),"message"));assert(strstr(message,scenario==2?"wait cycle":scenario==3?"missing task":"only on tasks it owns"));json_object_put(r);}
  if(scenario>=5&&scenario<=7)assert(!strcmp(wait_outcome(&w,0),"completed")&&!strcmp(wait_outcome(&w,1),"faulted"));
  if(scenario==8||scenario==13)assert(!strcmp(wait_outcome(&w,1),"completed")&&w.resumes[1]==1);
  if(scenario==9||scenario==11)for(unsigned i=0;i<w.count;i++)assert(!strcmp(wait_outcome(&w,i),"completed"));
  if(scenario==10||scenario==12)assert(!strcmp(wait_outcome(&w,4),"completed"));
  assert(!pg_task_scheduler_close(&scheduler)&&!pg_durable_session_close(&t->f.session));for(unsigned i=0;i<w.count;i++)json_object_put(w.on[i]);cleanup(&t->f,parent);
 }
 puts("PASS: atomic phase Entry/Document/child/wait commit and callback/cycle/owner rollback; Durable allSettled/failFast, missing/foreign/self/mutual/owned-conversation wait cycles, background isolation, diamond joins, cancellation cascade and waiting reopen on SQLite/JSONL");
}

typedef struct {task_case base;pg_durable_tool tool;pg_durable_tool_host tool_host;pg_tool_registry registry;pg_read_tool reader;pg_process process;pg_bash_tool shell;unsigned leased,acquired,released,busy_once,progress,unavailable; } durable_tool_case;
static int durable_tool_acquire(void *p,uint64_t id,pg_tools *out){durable_tool_case *t=p;(void)id;if(t->unavailable)return 2;if(t->busy_once){t->busy_once=0;return 1;}assert(!t->leased);t->leased=1;t->acquired++;*out=pg_tool_registry_service(&t->registry);return 0;}
static void durable_tool_release(void *p,uint64_t id,const pg_tools *tools){durable_tool_case *t=p;(void)id;assert(t->leased);if(tools->close){int result=1;for(unsigned i=0;i<1000&&result==1;i++){result=tools->close(tools->context);if(result==1)nanosleep(&(struct timespec){0,1000000},0);}assert(!result);}t->leased=0;t->released++;}
static void durable_tool_progress(void *p,uint64_t id,pg_slice text){durable_tool_case *t=p;(void)id;assert(text.size<=4096);t->progress++;}
static int durable_tool_resolve(void *p,pg_slice kind,const pg_task_definition **out){durable_tool_case *t=p;assert(kind.size==10&&!memcmp(kind.data,"pangu.tool",10));*out=&t->tool.definition;return 0;}
static pg_record_storage tool_crash_storage;
static int durable_tool_crash_commit(void *p,const struct json_object *writes,pg_records_adopt adopt,void *ctx,uint64_t *seq){
 for(size_t i=0;i<json_object_array_length(writes);i++){struct json_object *write=json_object_array_get_idx(writes,i),*record=json_object_object_get(write,"value");const char *kind=json_object_get_string(json_object_object_get(record,"kind"));if(kind&&!strcmp(kind,"pangu.tool-result"))_exit(0);}
 return tool_crash_storage.commit(p,writes,adopt,ctx,seq);
}
static void durable_tool_tests(void){
 _Static_assert(sizeof(pg_durable_tool)==168&&offsetof(pg_durable_tool,phases)==88,"Durable Tool ABI");
 for(unsigned kind=0;kind<2;kind++)for(unsigned scenario=0;scenario<14;scenario++){
  char parent[]="/tmp/pangu-durable-tool-XXXXXX";assert(mkdtemp(parent));durable_tool_case t={.base={.f={.kind=kind,.now=100}},.reader={.directory=-1}};task_case *base=&t.base;
  snprintf(base->f.path,sizeof(base->f.path),"%s/%s",parent,kind?".pangu":"pangu.sqlite");backend_open(&base->f);seed(&base->f);open_session(&base->f);task_bind(base);
  char path[512];snprintf(path,sizeof(path),"%s/hello.txt",parent);FILE *file=fopen(path,"w");assert(file&&fputs("durable real read\n",file)>=0&&!fclose(file));
  assert(!pg_read_tool_open(&t.reader,parent));pg_write_tool writer={.directory=t.reader.directory};pg_tool_definition read=scenario>=6&&scenario<12?pg_write_tool_definition(&writer):pg_read_tool_definition(&t.reader);if(scenario>=8&&scenario<=10){pg_process_init(&t.process);pg_process_ops ops=pg_process_service(&t.process);assert(!pg_bash_tool_init(&t.shell,&ops,t.reader.directory,0));read=pg_bash_tool_definition(&t.shell);}assert(!pg_tool_registry_init(&t.registry,&read,1));
  t.tool_host=(pg_durable_tool_host){&t,durable_tool_acquire,durable_tool_release,task_mint,durable_tool_progress};assert(!pg_durable_tool_init(&t.tool,&t.tool_host));base->host.resolve=durable_tool_resolve;
  pg_record_tx tx={0};pg_document_tx docs={0};struct json_object *entry=0,*draft=json_tokener_parse("{\"kind\":\"pangu.assistant\",\"model\":[{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"read file\"},{\"type\":\"toolCall\",\"id\":\"read1\",\"name\":\"read\",\"arguments\":{\"path\":\"hello.txt\"}}]}]}");if(scenario==6||scenario==7||scenario==11){struct json_object *call=json_object_array_get_idx(json_object_object_get(json_object_array_get_idx(json_object_object_get(draft,"model"),0),"content"),1);assert(!json_object_object_add(call,"name",json_object_new_string("write"))&&!json_object_object_add(json_object_object_get(call,"arguments"),"content",json_object_new_string("durable real write\n")));}
  if(scenario>=8&&scenario<=10){struct json_object *call=json_object_array_get_idx(json_object_object_get(json_object_array_get_idx(json_object_object_get(draft,"model"),0),"content"),1);struct json_object *arguments=json_object_new_object();assert(arguments&&!json_object_object_add(call,"name",json_object_new_string("bash"))&&!json_object_object_add(arguments,"command",json_object_new_string(scenario==8?"printf 'durable real bash\\n'":"printf 'durable real bash\\n'; /bin/sleep 30"))&&!json_object_object_add(call,"arguments",arguments));}
  assert(draft&&!pg_durable_session_begin(&base->f.session,&tx,&docs,base->f.memory,read_record)&&!pg_record_tx_append_entry(&tx,1,draft,0,(int(*)(void *,uint64_t *))pg_record_memory_mint,&entry));json_object_put(draft);
  uint64_t assistant=(uint64_t)json_object_get_int64(json_object_object_get(entry,"id"));json_object_put(entry);assert(!pg_durable_session_settle(&base->f.session,0,0,0));
  assert(!pg_durable_session_begin(&base->f.session,&tx,&docs,&t,task_read));struct json_object *definition=json_tokener_parse("{\"name\":\"pangu.tool\",\"version\":2}"),*input=json_tokener_parse("{\"kind\":\"model\",\"assistant\":0,\"callId\":\"read1\"}"),*options=json_tokener_parse("{\"ownership\":{\"kind\":\"conversation\"}}");assert(!json_object_object_add(input,"assistant",json_object_new_int64((int64_t)assistant)));pg_task_creation creation={.context=&t,.initial=t.tool.definition.initial,.mint=task_mint,.conversation_id=1};uint64_t id=0;assert(!pg_record_tx_create_task(&tx,definition,input,options,&creation,&id)&&!pg_durable_session_settle(&base->f.session,0,0,0));json_object_put(definition);json_object_put(input);json_object_put(options);
  if(scenario==1||scenario==2||scenario==3||scenario==7||scenario==13){struct json_object *row=task_row(base,id);struct json_object *state=json_tokener_parse(scenario==2||scenario==7?"{\"status\":\"pending\",\"checkpoint\":{\"phase\":\"execute\",\"arguments\":{\"path\":\"hello.txt\"},\"replay\":\"unsafe\"}}":"{\"status\":\"pending\",\"checkpoint\":{\"phase\":\"execute\",\"arguments\":{\"path\":\"hello.txt\"},\"replay\":\"safe\"}}");assert(!json_object_object_add(row,"state",state)&&!pg_durable_session_begin(&base->f.session,&tx,&docs,&t,task_read)&&!pg_record_tx_set_task(&tx,row,base,task_now)&&!pg_durable_session_settle(&base->f.session,0,0,0));json_object_put(row);assert(!pg_durable_session_close(&base->f.session));backend_open(&base->f);open_session(&base->f);if(scenario==3){read.replay_safe=0;assert(!pg_tool_registry_init(&t.registry,&read,1));}}
  if(scenario==4){struct json_object *row=task_row(base,id);assert(!json_object_object_add(row,"abortRequested",json_object_new_boolean(1))&&!pg_durable_session_begin(&base->f.session,&tx,&docs,&t,task_read)&&!pg_record_tx_set_task(&tx,row,base,task_now)&&!pg_durable_session_settle(&base->f.session,0,0,0));json_object_put(row);}
  if(scenario==5)t.busy_once=1;
  if(scenario>=12)t.unavailable=1;
  pg_task_scheduler scheduler={0};
  if(scenario==11){assert(!pg_durable_session_close(&base->f.session));pid_t child=fork();assert(child>=0);if(!child){backend_open(&base->f);open_session(&base->f);tool_crash_storage=base->f.records.storage;base->f.records.storage.commit=durable_tool_crash_commit;assert(!pg_task_scheduler_open(&scheduler,&base->f.session,&base->host,3));for(unsigned j=0;j<20;j++)(void)pg_task_scheduler_poll(&scheduler);_exit(99);}int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));file=fopen(path,"r");assert(file);char written[80];assert(fgets(written,sizeof(written),file)&&!strcmp(written,"durable real write\n")&&!fclose(file));file=fopen(path,"w");assert(file&&fputs("changed after interrupted effect\n",file)>=0&&!fclose(file));backend_open(&base->f);open_session(&base->f);}
  assert(!pg_task_scheduler_open(&scheduler,&base->f.session,&base->host,3));int result=1;
  if(scenario==9||scenario==10){assert(pg_task_scheduler_poll(&scheduler)==PG_TASK_PENDING&&t.leased);if(scenario==9){struct json_object *r=task_row(base,id);assert(!json_object_object_add(r,"abortRequested",json_object_new_boolean(1))&&!pg_durable_session_begin(&base->f.session,&tx,&docs,&t,task_read)&&!pg_record_tx_set_task(&tx,r,base,task_now)&&!pg_durable_session_settle(&base->f.session,0,0,0));json_object_put(r);}else{assert(!pg_task_scheduler_close(&scheduler)&&!t.leased&&!pg_durable_session_close(&base->f.session));backend_open(&base->f);open_session(&base->f);assert(!pg_task_scheduler_open(&scheduler,&base->f.session,&base->host,3));}}
  for(unsigned j=0;j<1000&&result!=PG_TASK_DONE;j++){result=pg_task_scheduler_poll(&scheduler);assert(result<=PG_TASK_BLOCKED);if(result!=PG_TASK_DONE)nanosleep(&(struct timespec){0,1000000},0);}assert(result==PG_TASK_DONE&&!t.leased&&t.acquired==t.released);
  struct json_object *row=task_row(base,id),*outcome=json_object_object_get(json_object_object_get(row,"state"),"outcome");const char *expected=scenario==2||scenario==3||scenario==7||scenario==10||scenario==11||scenario==13?"failed":scenario==4||scenario==9?"aborted":"completed";assert(!strcmp(json_object_get_string(json_object_object_get(outcome,"status")),expected));uint64_t receipt=(uint64_t)json_object_get_int64(json_object_object_get(json_object_object_get(outcome,"result"),"entryId"));json_object_put(row);assert(!task_read(base,PG_TABLE_ENTRY,receipt,&entry));struct json_object *message=json_object_array_get_idx(json_object_object_get(entry,"model"),0);assert(!strcmp(json_object_get_string(json_object_object_get(message,"toolCallId")),"read1"));const char *text=json_object_get_string(json_object_object_get(json_object_array_get_idx(json_object_object_get(message,"content"),0),"text"));assert(strstr(text,scenario==2||scenario==3||scenario==7||scenario==10||scenario==11||scenario==13?"interrupted":scenario==4||scenario==9?"aborted":scenario==6?"wrote":scenario==8?"durable real bash":scenario==12?"not available":"durable real read"));json_object_put(entry);
  if(scenario==6||scenario==7||scenario==11){file=fopen(path,"r");assert(file);char actual[80];assert(fgets(actual,sizeof(actual),file)&&!fclose(file));assert(!strcmp(actual,scenario==6?"durable real write\n":scenario==11?"changed after interrupted effect\n":"durable real read\n"));}
  assert(!pg_task_scheduler_close(&scheduler)&&!pg_durable_session_close(&base->f.session));pg_read_tool_close(&t.reader);assert(!unlink(path));cleanup(&base->f,parent);
 }
 puts("PASS: native pangu.tool actual workspace read/write/bash, atomic transcript receipt, safe/unsafe/current-policy recovery, async abort/drain, suspend/reopen and busy lease on SQLite/JSONL");
}

int main(void){
 durable_tool_tests();
 wait_tests();
 scheduler_tests();
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
