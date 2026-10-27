#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/record_sqlite.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <json-c/json.h>
#include <sqlite3.h>
_Static_assert(sizeof(pg_record_sqlite)==72,"SQLite ABI");
_Static_assert(offsetof(pg_record_sqlite,database)==48,"SQLite database offset");
static const char *first="[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":2,\"conversationId\":1,\"kind\":\"test\",\"head\":2}},{\"type\":\"task\",\"value\":{\"id\":3,\"kind\":\"test.task\",\"version\":1,\"input\":null,\"background\":false,\"abortRequested\":false,\"conversationId\":1,\"owner\":{\"kind\":\"conversation\",\"conversationId\":1},\"state\":{\"status\":\"pending\",\"checkpoint\":{\"x\":1}}}},{\"type\":\"submission\",\"value\":{\"id\":4,\"conversationId\":1,\"requestId\":\"request\",\"status\":\"queued\"}},{\"type\":\"document.create\",\"record\":{\"id\":5,\"kind\":\"test\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":1}}}]";
static const char *second="[{\"type\":\"entry\",\"value\":{\"id\":6,\"conversationId\":1,\"kind\":\"test\",\"head\":6}},{\"type\":\"document.change\",\"id\":5,\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":2}}}]";
typedef struct {pg_record_sqlite *store;unsigned calls;uint64_t sequence;} adoption;
static void adopted(void *p,uint64_t seq){
 adoption *a=p; a->calls++;a->sequence=seq;
 struct json_object *row=0;
 assert(!pg_record_memory_read(&a->store->memory,PG_TABLE_CONVERSATION,1,&row));json_object_put(row);
 assert(pg_record_sqlite_close(a->store)==PG_SQLITE_INVALID);
 sqlite3 *independent=0;const char *file=sqlite3_db_filename(a->store->database,"main");
 assert(sqlite3_open_v2(file,&independent,SQLITE_OPEN_READONLY,0)==SQLITE_OK);
 sqlite3_stmt *q=0;assert(sqlite3_prepare_v2(independent,"SELECT MAX(seq) FROM pangu_durable_commits_v1",-1,&q,0)==SQLITE_OK);
 assert(sqlite3_step(q)==SQLITE_ROW&&sqlite3_column_int64(q,0)==(sqlite3_int64)seq);
 assert(!sqlite3_finalize(q));assert(!sqlite3_close(independent));
}
static int session_adopt(void *p,const struct json_object *batch,uint64_t seq){(void)batch;adopted(p,seq);return 0;}
static void trace(pg_record_sqlite *s){
 struct json_object *out=json_object_new_array(),*v=0;uint64_t seq=0;
 for(unsigned table=0;table<4;table++){
  assert(!pg_record_memory_read(&s->memory,(pg_record_table)table,table+1,&v));assert(!json_object_array_add(out,v));
 }
 assert(!pg_record_memory_entry(&s->memory,6,1,&v,&seq));assert(!json_object_array_add(out,v));assert(!json_object_array_add(out,json_object_new_int64((int64_t)seq)));
 assert(!pg_record_memory_document(&s->memory,5,1,&v));assert(!json_object_array_add(out,v));
 assert(!pg_record_memory_document(&s->memory,5,UINT64_MAX,&v));assert(!json_object_array_add(out,v));
 uint64_t id=0;assert(!pg_record_memory_mint(&s->memory,&id));assert(!json_object_array_add(out,json_object_new_int64((int64_t)id)));
 puts(json_object_to_json_string_ext(out,JSON_C_TO_STRING_PLAIN));json_object_put(out);
}
static int commit(pg_record_sqlite *s,const char *text,adoption *a,uint64_t *seq){
 struct json_object *j=json_tokener_parse(text);assert(j);pg_record_storage api=pg_record_sqlite_storage(s);
 int r=api.commit(api.context,j,adopted,a,seq);json_object_put(j);return r;
}
static void check(pg_record_sqlite *s){
 assert(s->memory.next_sequence==3&&s->memory.next_id==7);
 struct json_object *v=0;uint64_t seq=0;
 assert(!pg_record_memory_entry(&s->memory,6,1,&v,&seq)&&seq==2);json_object_put(v);
 assert(!pg_record_memory_document(&s->memory,5,1,&v));json_object_put(v);
 assert(!pg_record_memory_document(&s->memory,5,UINT64_MAX,&v));
 assert(strstr(json_object_to_json_string(v),"2"));json_object_put(v);
 pg_record_request_query q={.conversation=1,.request="request",.size=7};
 assert(!pg_record_memory_submission_by_request(&s->memory,&q,&v));json_object_put(v);
 pg_memory_query query={.table=PG_TABLE_ENTRY,.limit=10,.flags=PG_SCAN_CONVERSATION,.conversation_id=1};pg_memory_page page={0};
 assert(!pg_record_memory_scan(&s->memory,&query,&page)&&json_object_array_length(page.items)==2);json_object_put(page.items);
}
static void sql(const char *path,const char *query){sqlite3 *db=0;assert(!sqlite3_open(path,&db));assert(!sqlite3_exec(db,query,0,0,0));assert(!sqlite3_close(db));}
int main(int argc,char **argv){
 if(argc==3&&!strcmp(argv[1],"--verify")){pg_record_sqlite s={0};assert(!pg_record_sqlite_open(&s,argv[2],1));check(&s);assert(!pg_record_sqlite_close(&s));return 0;}
 char path[]="/tmp/pangu-sqlite-XXXXXX";int fd=mkstemp(path);assert(fd>=0);assert(!close(fd));
 pg_record_sqlite s={0};assert(!pg_record_sqlite_open(&s,path,0));adoption a={.store=&s};uint64_t seq=0;
 pg_record_session session={.storage=pg_record_sqlite_storage(&s),.context=&a,.adopt=session_adopt};
 struct json_object *batch=json_tokener_parse(first);assert(!pg_records_commit(&session,batch)&&session.sequence==1&&a.calls==1);json_object_put(batch);
 batch=json_tokener_parse(second);assert(!pg_records_commit(&session,batch)&&session.sequence==2&&a.calls==2);json_object_put(batch);check(&s);
 assert(commit(&s,first,&a,&seq)==PG_MEMORY_INVALID&&a.calls==2&&s.memory.next_sequence==3&&!s.poisoned);
 assert(!pg_record_sqlite_close(&s)&&!pg_record_sqlite_close(&s));
 if(argc==2&&!strcmp(argv[1],"--trace")){assert(!pg_record_sqlite_open(&s,path,1));trace(&s);assert(!pg_record_sqlite_close(&s));assert(!unlink(path));return 0;}
 pid_t child=fork();assert(child>=0);if(!child){execl(argv[0],argv[0],"--verify",path,(char *)0);_exit(127);}int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 assert(!pg_record_sqlite_open(&s,path,1));check(&s);assert(commit(&s,"[]",&a,&seq)==PG_MEMORY_INVALID&&!seq);assert(!pg_record_sqlite_close(&s));
 /* Two owners can read one snapshot; only the current sequence can commit. */
 pg_record_sqlite stale={0};assert(!pg_record_sqlite_open(&s,path,0));assert(!pg_record_sqlite_open(&stale,path,0));
 assert(!commit(&s,"[]",&a,&seq)&&seq==3);adoption b={.store=&stale};
 assert(commit(&stale,"[]",&b,&seq)==PG_MEMORY_STORAGE&&!seq&&!b.calls&&stale.poisoned&&stale.memory.next_sequence==3);
 assert(!pg_record_sqlite_close(&stale));assert(!pg_record_sqlite_close(&s));
 /* A provider failure cannot publish or apply the prepared batch. */
 assert(!pg_record_sqlite_open(&s,path,0));sql(path,"CREATE TRIGGER reject_batch BEFORE INSERT ON pangu_durable_commits_v1 BEGIN SELECT RAISE(ABORT,'test'); END");
 unsigned calls=a.calls;uint64_t before=s.memory.next_sequence;
 pg_record_session failed={.storage=pg_record_sqlite_storage(&s),.sequence=before-1,.context=&a,.adopt=session_adopt};
 batch=json_tokener_parse("[{\"type\":\"entry\",\"value\":{\"id\":7,\"conversationId\":1,\"kind\":\"test\"}}]");
 assert(pg_records_commit(&failed,batch)==PG_RECORDS_STORAGE_FAILED&&failed.failed==PG_RECORDS_STORAGE_FAILED&&a.calls==calls&&s.memory.next_sequence==before&&s.poisoned);json_object_put(batch);
 struct json_object *missing=0;assert(pg_record_memory_read(&s.memory,PG_TABLE_ENTRY,7,&missing)==PG_MEMORY_MISSING&&!missing);
 assert(!pg_record_sqlite_close(&s));sql(path,"DROP TRIGGER reject_batch");
 /* Complete but invalid data is corruption, never an empty successful open. */
 sql(path,"UPDATE pangu_durable_commits_v1 SET writes='[{}]' WHERE seq=2");
 assert(pg_record_sqlite_open(&s,path,0)==PG_SQLITE_CORRUPT&&!s.database&&s.closed);
 sql(path,"UPDATE pangu_durable_commits_v1 SET writes='[] garbage' WHERE seq=2");
 assert(pg_record_sqlite_open(&s,path,0)==PG_SQLITE_CORRUPT&&!s.database);
 assert(!unlink(path));assert(pg_record_sqlite_open(&s,path,1)==PG_SQLITE_IO&&!s.database);
 puts("PASS: SQLite durable adoption, mixed tables/documents, reexec recovery, queries, read-only, stale owner, rollback and corruption");return 0;
}
