#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/record_jsonl.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <json-c/json.h>
_Static_assert(sizeof(pg_record_jsonl)==72&&offsetof(pg_record_jsonl,directory)==48,"JSONL ABI");
static const char *first="[{\"type\":\"conversation\",\"value\":{\"id\":1}},{\"type\":\"entry\",\"value\":{\"id\":2,\"conversationId\":1,\"kind\":\"test\",\"head\":2}},{\"type\":\"task\",\"value\":{\"id\":3,\"kind\":\"test.task\",\"version\":1,\"input\":null,\"background\":false,\"abortRequested\":false,\"conversationId\":1,\"owner\":{\"kind\":\"conversation\",\"conversationId\":1},\"state\":{\"status\":\"pending\",\"checkpoint\":{\"x\":1}}}},{\"type\":\"submission\",\"value\":{\"id\":4,\"conversationId\":1,\"requestId\":\"request\",\"status\":\"queued\"}},{\"type\":\"document.create\",\"record\":{\"id\":5,\"kind\":\"test\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":1},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":1}}}]";
static const char *second="[{\"type\":\"entry\",\"value\":{\"id\":6,\"conversationId\":1,\"kind\":\"test\",\"head\":6}},{\"type\":\"document.change\",\"id\":5,\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":2}}}]";
static char path[512];
static unsigned calls,fail_write;
ssize_t __real_write(int,const void *,size_t);
ssize_t __wrap_write(int fd,const void *data,size_t size){
 if(fail_write&&!--fail_write){errno=EIO;return -1;}
 if(fail_write&&size>1)size/=2;
 return __real_write(fd,data,size);
}
static void adopted(void *p,uint64_t seq){
 pg_record_jsonl *s=p;calls++;
 assert(s->memory.next_sequence==seq+1&&pg_record_jsonl_close(s)==PG_JSONL_INVALID);
 char file[600];snprintf(file,sizeof(file),"%s/main.jsonl",path);FILE *f=fopen(file,"r");assert(f);
 char *line=0;size_t cap=0;uint64_t last=0;
 while(getline(&line,&cap,f)>=0){struct json_object *j=json_tokener_parse(line);assert(j);last=(uint64_t)json_object_get_int64(json_object_object_get(j,"seq"));json_object_put(j);}
 free(line);assert(!fclose(f));assert(last==seq);
}
static void trace(pg_record_jsonl *s){
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
static int session_adopt(void *p,const struct json_object *batch,uint64_t seq){(void)batch;adopted(p,seq);return 0;}
static int commit(pg_record_jsonl *s,const char *text,uint64_t *seq){struct json_object *j=json_tokener_parse(text);assert(j);pg_record_storage api=pg_record_jsonl_storage(s);int result=api.commit(api.context,j,adopted,s,seq);json_object_put(j);return result;}
static void check(pg_record_jsonl *s){
 struct json_object *v=0;uint64_t seq=0;
 assert(s->memory.next_id==7&&s->memory.next_sequence==3);
 assert(!pg_record_memory_entry(&s->memory,6,1,&v,&seq)&&seq==2);json_object_put(v);
 assert(!pg_record_memory_document(&s->memory,5,UINT64_MAX,&v));struct json_object *value=json_object_object_get(v,"value");assert(json_object_get_int(json_object_object_get(value,"x"))==2);json_object_put(v);
 assert(!pg_record_memory_document(&s->memory,5,1,&v));value=json_object_object_get(v,"value");assert(json_object_get_int(json_object_object_get(value,"x"))==1);json_object_put(v);
 pg_record_request_query q={.conversation=1,.request="request",.size=7};assert(!pg_record_memory_submission_by_request(&s->memory,&q,&v));json_object_put(v);
}
static void append(const char *name,const char *data){char file[600];snprintf(file,sizeof(file),"%s/%s",path,name);FILE *f=fopen(file,"a");assert(f);assert(fwrite(data,1,strlen(data),f)==strlen(data));assert(!fclose(f));}
static void remove_file(const char *name){char file[600];snprintf(file,sizeof(file),"%s/%s",path,name);assert(!unlink(file));}
static void cleanup(void){DIR *d=opendir(path);assert(d);struct dirent *e;while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;remove_file(e->d_name);}assert(!closedir(d));assert(!rmdir(path));}
int main(int argc,char **argv){
 if(argc==3&&!strcmp(argv[1],"--verify")){snprintf(path,sizeof(path),"%s",argv[2]);pg_record_jsonl s={0};assert(!pg_record_jsonl_open(&s,path,0));check(&s);assert(!pg_record_jsonl_close(&s));return 0;}
 char parent[]="/tmp/pangu-jsonl-XXXXXX";assert(mkdtemp(parent));snprintf(path,sizeof(path),"%s/.pangu",parent);
 pg_record_jsonl s={0},other={0};assert(!pg_record_jsonl_open(&s,path,1));assert(pg_record_jsonl_open(&other,path,0)==PG_JSONL_IO&&!other.opened);
 uint64_t seq=0;pg_record_session session={.storage=pg_record_jsonl_storage(&s),.context=&s,.adopt=session_adopt};
 struct json_object *batch=json_tokener_parse(first);assert(!pg_records_commit(&session,batch)&&session.sequence==1&&calls==1);json_object_put(batch);
 batch=json_tokener_parse(second);assert(!pg_records_commit(&session,batch)&&session.sequence==2&&calls==2);json_object_put(batch);check(&s);assert(!pg_record_jsonl_close(&s)&&!pg_record_jsonl_close(&s));
 if(argc==2&&!strcmp(argv[1],"--trace")){assert(!pg_record_jsonl_open(&s,path,0));trace(&s);assert(!pg_record_jsonl_close(&s));cleanup();assert(!rmdir(parent));return 0;}
 pid_t child=fork();assert(child>=0);if(!child){execl(argv[0],argv[0],"--verify",path,(char *)0);_exit(127);}int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 append("main.jsonl","{\"torn\":");append("doc-5.jsonl","{\"torn\":");
 append("task-3.jsonl","{\"format\":1,\"type\":\"record\",\"seq\":3,\"ordinal\":0,\"payload\":{\"type\":\"task\",\"value\":{\"id\":3,\"state\":{\"status\":\"pending\"}}}}\n{\"torn\":");
 assert(!pg_record_jsonl_open(&s,path,0));check(&s);assert(!pg_record_jsonl_close(&s));
 assert(!pg_record_jsonl_open(&s,path,0));fail_write=2;unsigned before=calls;
 assert(commit(&s,"[{\"type\":\"task\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test.task\",\"state\":{\"status\":\"pending\",\"checkpoint\":{\"x\":9}}}}]",&seq)==PG_MEMORY_STORAGE&&!seq&&s.poisoned&&calls==before&&s.memory.next_sequence==3);
 assert(!pg_record_jsonl_close(&s));assert(!pg_record_jsonl_open(&s,path,0));check(&s);assert(!pg_record_jsonl_close(&s));
 /* A final terminal task can be recovered after its live sidecar is reclaimed. */
 assert(!pg_record_jsonl_open(&s,path,0));assert(!commit(&s,"[{\"type\":\"task\",\"value\":{\"id\":3,\"conversationId\":1,\"kind\":\"test.task\",\"state\":{\"status\":\"terminal\"}}}]",&seq)&&seq==3);assert(!pg_record_jsonl_close(&s));remove_file("task-3.jsonl");
 assert(!pg_record_jsonl_open(&s,path,0));struct json_object *v=0;assert(!pg_record_memory_read(&s.memory,PG_TABLE_TASK,3,&v));json_object_put(v);assert(!pg_record_jsonl_close(&s));
 /* Current-only checkpoints compact on recovery, and remain readable on the next recovery. */
 assert(!pg_record_jsonl_open(&s,path,1));
 assert(!commit(&s,"[{\"type\":\"document.create\",\"record\":{\"id\":8,\"kind\":\"session-checkpoint\",\"scope\":{\"kind\":\"session\"}},\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":1}}}]",&seq)&&seq==4);
 assert(!commit(&s,"[{\"type\":\"document.change\",\"id\":8,\"content\":{\"kind\":\"base\",\"version\":1,\"value\":{\"x\":9}}}]",&seq)&&seq==5);assert(!pg_record_jsonl_close(&s));
 for(unsigned i=0;i<2;i++){assert(!pg_record_jsonl_open(&s,path,1));assert(!pg_record_memory_document(&s.memory,8,UINT64_MAX,&v));assert(json_object_get_int(json_object_object_get(json_object_object_get(v,"value"),"x"))==9);json_object_put(v);assert(!pg_record_jsonl_close(&s));}
 char compact[600];snprintf(compact,sizeof(compact),"%s/doc-8.jsonl",path);FILE *f=fopen(compact,"r");assert(f);char *line=0;size_t cap=0;assert(getline(&line,&cap,f)>0&&getline(&line,&cap,f)<0);free(line);assert(!fclose(f));
 assert(!pg_record_jsonl_open(&s,path,0));
 assert(!commit(&s,"[{\"type\":\"document.copy\",\"record\":{\"id\":10,\"kind\":\"test\",\"scope\":{\"kind\":\"conversation\",\"conversationId\":9},\"history\":\"rewindable\",\"fork\":\"asOf\"},\"source\":{\"id\":5,\"at\":\"current\"}}]",&seq)&&seq==6);assert(!pg_record_jsonl_close(&s));
 assert(!pg_record_jsonl_open(&s,path,0));assert(!pg_record_memory_document(&s.memory,10,UINT64_MAX,&v));assert(json_object_get_int(json_object_object_get(json_object_object_get(v,"value"),"x"))==2);json_object_put(v);assert(!pg_record_jsonl_close(&s));
 /* Confirmed rewindable content is required, even when JSON parses. */
 remove_file("doc-5.jsonl");assert(pg_record_jsonl_open(&s,path,0)==PG_JSONL_CORRUPT&&!s.opened);
 cleanup();assert(!rmdir(parent));puts("PASS: JSONL mixed persistence, reexec recovery, torn/unconfirmed tails, terminal sidecars, lease and corruption");return 0;
}
