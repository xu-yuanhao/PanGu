#define _GNU_SOURCE
#include "../../src/runtime/journal.h"
#include "../../src/runtime/journal_layout.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
_Static_assert(sizeof(pg_journal)==PG_J_BYTES,"journal size");
_Static_assert(offsetof(pg_journal,sequence)==PG_J_SEQUENCE,"journal sequence");
_Static_assert(offsetof(pg_journal,end)==PG_J_END,"journal end");
_Static_assert((O_RDWR|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC)==PG_J_OPEN_FLAGS,"open flags");
_Static_assert((O_CREAT|O_EXCL|PG_J_OPEN_FLAGS)==PG_J_CREATE_FLAGS,"create flags");
_Static_assert(F_DUPFD_CLOEXEC==PG_J_DUPFD,"duplicate fd");
_Static_assert((LOCK_EX|LOCK_NB)==6,"lock flags");
typedef void (*probe_fn)(void);
unsigned pg_journal_abi_probe(probe_fn,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
void pg_journal_abi_corrupt(void);
static int root;
static pg_session view,expected,other;
static pg_session_control control;
static pg_journal journal={.file=-1,.directory=-1};
static int write_mode,write_calls,read_short,sync_fail,read_fail,truncate_fail;
ssize_t __real_pwrite(int,const void *,size_t,off_t);
ssize_t __real_pread(int,void *,size_t,off_t);
int __real_fsync(int);
int __real_ftruncate(int,off_t);
int pg_fixture_ftruncate(int fd,off_t size){if(truncate_fail){errno=EIO;return -1;}return __real_ftruncate(fd,size);}
ssize_t pg_fixture_pwrite(int fd,const void *data,size_t n,off_t at){
 ++write_calls;
 if(write_mode==1){errno=ENOSPC;return -1;}
 if(write_mode==2){if(write_calls==1)return __real_pwrite(fd,data,n/2,at);errno=ENOSPC;return -1;}
 if(write_mode==3){ssize_t rc=__real_pwrite(fd,data,n/2,at);assert(rc>0);_exit(73);}
 if(write_mode==4)return 0;
 if(write_mode==5){if(write_calls==1){errno=EINTR;return -1;}if(n>3)n=3;}
 return __real_pwrite(fd,data,n,at);
}
ssize_t pg_fixture_pread(int fd,void *data,size_t n,off_t at){if(read_fail){errno=EIO;return -1;}if(read_short&&n>3)n=3;return __real_pread(fd,data,n,at);}
int pg_fixture_fsync(int fd){if(sync_fail){--sync_fail;errno=EIO;return -1;}return __real_fsync(fd);}
static void fresh(void){memset(&view,0,sizeof(view));memset(&control,0,sizeof(control));journal=(pg_journal){.file=-1,.directory=-1};write_mode=write_calls=read_short=sync_fail=read_fail=truncate_fail=0;}
static pg_message message(pg_role role,const char *text){pg_message m={.role=role,.size=strlen(text)};memcpy(m.text,text,m.size+1);return m;}
static void commit_text(pg_role role,const char *text){pg_message m=message(role,text);pg_session_change c={.kind=PG_SESSION_APPEND,.messages=&m,.count=1};assert(!pg_session_commit(&view,&control,&c));}
static void system_text(void){pg_session_change c={.kind=PG_SESSION_SYSTEM,.text={"system",6}};assert(!pg_session_commit(&view,&control,&c));}
static size_t file_bytes(const char *name,unsigned char *out,size_t cap){int fd=openat(root,name,O_RDONLY);assert(fd>=0);ssize_t n=read(fd,out,cap);assert(n>=0);close(fd);return (size_t)n;}
static void file_write(const char *name,const unsigned char *data,size_t n){int fd=openat(root,name,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(fd>=0);assert(write(fd,data,n)==(ssize_t)n);assert(!close(fd));}
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t checksum(const unsigned char *p,size_t n){uint64_t hash=UINT64_C(0xcbf29ce484222325);for(size_t i=0;i<n;++i)hash=(hash^p[i])*UINT64_C(0x100000001b3);return hash;}
static void rehash(unsigned char *frame){size_t n=u32(frame+12);uint64_t hash=checksum(frame,32);memcpy(frame+32,&hash,8);hash=checksum(frame,n-16);memcpy(frame+n-16,&hash,8);}
static void reopen(const char *name){pg_journal_close(&journal);fresh();assert(!pg_journal_open(&journal,root,name,&view,&control));}
static void roundtrip(void){fresh();assert(!pg_journal_open(&journal,root,"session",&view,&control));assert(control.storage.context==&journal);system_text();commit_text(PG_USER,"你好 😀");pg_message pair[2]={message(PG_ASSISTANT,"checking"),message(PG_TOOL_CALL,"{}")};strcpy(pair[1].call_id,"read-1");strcpy(pair[1].tool_name,"read");pg_session_change c={.kind=PG_SESSION_APPEND,.messages=pair,.count=2};assert(!pg_session_commit(&view,&control,&c));pg_message result=message(PG_TOOL_RESULT,"ok");strcpy(result.call_id,"read-1");c.messages=&result;c.count=1;assert(!pg_session_commit(&view,&control,&c));commit_text(PG_ASSISTANT,"done");expected=view;uint64_t end=journal.end;assert(control.sequence==5&&view.count==5);
 memset(&other,0,sizeof(other));pg_session_control second_control={0};pg_journal second={.file=-1,.directory=-1};assert(pg_journal_open(&second,root,"session",&other,&second_control));assert(second.file==-1&&!second_control.sequence&&!other.count);
 reopen("session");assert(control.sequence==5&&journal.end==end&&!memcmp(&view,&expected,sizeof(view)));
 c=(pg_session_change){.kind=PG_SESSION_RESET};assert(!pg_session_commit(&view,&control,&c));assert(!view.count&&view.system_size==6);commit_text(PG_USER,"new");expected=view;reopen("session");assert(control.sequence==7&&!memcmp(&view,&expected,sizeof(view)));
 pg_journal_close(&journal);pg_journal_close(&journal);
 pid_t child=fork();assert(child>=0);if(!child){fresh();assert(!pg_journal_open(&journal,root,"session",&view,&control));assert(control.sequence==7&&!memcmp(&view,&expected,sizeof(view)));pg_journal_close(&journal);_exit(0);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
}
static void torn_and_corrupt(void){unsigned char bytes[4096];fresh();assert(!pg_journal_open(&journal,root,"frames",&view,&control));system_text();commit_text(PG_USER,"input");pg_journal_close(&journal);size_t n=file_bytes("frames",bytes,sizeof(bytes));size_t first=u32(bytes+12),second=u32(bytes+first+12);assert(first+second==n);
 for(size_t cut=0;cut<second;++cut){file_write("torn",bytes,first+cut);fresh();assert(!pg_journal_open(&journal,root,"torn",&view,&control));assert(control.sequence==1&&!view.count&&view.system_size==6&&journal.end==first);struct stat info;assert(!fstat(journal.file,&info)&&(size_t)info.st_size==first);pg_journal_close(&journal);}
 for(size_t at=0;at<n;++at){bytes[at]^=1;file_write("corrupt",bytes,n);fresh();control.storage=(pg_storage){(void*)(uintptr_t)7,pg_session_volatile_commit};assert(pg_journal_open(&journal,root,"corrupt",&view,&control));assert(!view.count&&!view.system_size&&!control.sequence&&control.storage.context==(void*)(uintptr_t)7);int fd=openat(root,"corrupt",O_RDONLY);struct stat info;assert(fd>=0&&!fstat(fd,&info)&&(size_t)info.st_size==n);close(fd);bytes[at]^=1;}
 file_write("torn",bytes,first+second-1);fresh();truncate_fail=1;assert(pg_journal_open(&journal,root,"torn",&view,&control));assert(!view.count&&!view.system_size&&journal.file==-1);
 fresh();read_fail=1;assert(pg_journal_open(&journal,root,"frames",&view,&control));assert(!view.count&&!view.system_size&&journal.file==-1);
 const size_t semantic_offsets[]={8,16,24,28,40,48};
 for(size_t i=0;i<sizeof(semantic_offsets)/sizeof(semantic_offsets[0]);++i){unsigned char saved[4096];memcpy(saved,bytes,n);unsigned char *frame=saved+first;size_t at=semantic_offsets[i];frame[at]=255;rehash(frame);file_write("corrupt",saved,n);fresh();assert(pg_journal_open(&journal,root,"corrupt",&view,&control));assert(!view.count&&!view.system_size&&!control.sequence);}
 file_write("short-io",bytes,n);fresh();read_short=1;assert(!pg_journal_open(&journal,root,"short-io",&view,&control));assert(control.sequence==2&&view.count==1);write_mode=5;commit_text(PG_ASSISTANT,"short writes");assert(write_calls>2&&control.sequence==3);pg_journal_close(&journal);
}
static void failures(void){for(int mode=1;mode<=4;++mode){char name[32];snprintf(name,sizeof(name),"failure-%d",mode);fresh();assert(!pg_journal_open(&journal,root,name,&view,&control));system_text();pg_journal_close(&journal);
 if(mode==3){pid_t child=fork();assert(child>=0);if(!child){fresh();assert(!pg_journal_open(&journal,root,name,&view,&control));write_mode=3;commit_text(PG_USER,"crash");_exit(99);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==73);fresh();assert(!pg_journal_open(&journal,root,name,&view,&control));assert(control.sequence==1&&!view.count);pg_journal_close(&journal);continue;}
 fresh();assert(!pg_journal_open(&journal,root,name,&view,&control));write_mode=mode;pg_message m=message(PG_USER,"fail");pg_session_change c={.kind=PG_SESSION_APPEND,.messages=&m,.count=1};assert(pg_session_commit(&view,&control,&c));assert(control.failed&&journal.failed&&!view.count&&control.sequence==1);int calls=write_calls;assert(pg_session_commit(&view,&control,&c)&&write_calls==calls);reopen(name);assert(control.sequence==1&&!view.count);pg_journal_close(&journal);
 }
 fresh();assert(!pg_journal_open(&journal,root,"sync-failure",&view,&control));system_text();sync_fail=1;pg_message m=message(PG_USER,"possibly committed");pg_session_change c={.kind=PG_SESSION_APPEND,.messages=&m,.count=1};assert(pg_session_commit(&view,&control,&c));assert(control.failed&&journal.failed&&!view.count);reopen("sync-failure");assert(control.sequence==2&&view.count==1);pg_journal_close(&journal);
 fresh();sync_fail=1;assert(pg_journal_open(&journal,root,"sync-failure",&view,&control));assert(!view.count&&!control.sequence&&journal.file==-1);fresh();assert(!pg_journal_open(&journal,root,"sync-failure",&view,&control));pg_journal_close(&journal);
}
static void maximum(void){static pg_message pair[2];memset(pair,0,sizeof(pair));for(unsigned i=0;i<2;++i){pair[i].role=PG_TOOL_CALL;pair[i].size=PG_TEXT_LIMIT-1;memset(pair[i].text,'t',PG_TEXT_LIMIT-1);memset(pair[i].call_id,'i',PG_ID_LIMIT-1);memset(pair[i].tool_name,'n',PG_ID_LIMIT-1);}
 fresh();assert(!pg_journal_open(&journal,root,"maximum",&view,&control));pg_session_change c={.kind=PG_SESSION_APPEND,.messages=pair,.count=2};assert(!pg_session_commit(&view,&control,&c));assert(journal.end==8530);expected=view;reopen("maximum");assert(control.sequence==1&&view.count==2&&!memcmp(&view,&expected,sizeof(view)));pg_journal_close(&journal);
}
static void abi(void){assert(pg_journal_abi_probe(pg_journal_abi_corrupt,0,0,0,0,0)==65);fresh();assert(!pg_journal_abi_probe((probe_fn)pg_journal_open,(uintptr_t)&journal,(uintptr_t)root,(uintptr_t)"abi",(uintptr_t)&view,(uintptr_t)&control));assert(journal.file>=0&&control.storage.context==&journal);pg_session_change c={.kind=PG_SESSION_SYSTEM,.text={"abi",3}};assert(!pg_journal_abi_probe((probe_fn)pg_session_commit,(uintptr_t)&view,(uintptr_t)&control,(uintptr_t)&c,0,0));assert(control.sequence==1&&journal.sequence==1&&view.system_size==3);assert(!pg_journal_abi_probe((probe_fn)pg_journal_close,(uintptr_t)&journal,0,0,0,0));assert(journal.file==-1);}
static void paths_and_quota(void){const char *bad[]={"",".","..","../escape","a/b"};for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i){fresh();assert(pg_journal_open(&journal,root,bad[i],&view,&control));assert(journal.file==-1&&journal.directory==-1);}
 assert(!symlinkat("session",root,"link"));fresh();assert(pg_journal_open(&journal,root,"link",&view,&control));assert(journal.file==-1);
 assert(!mkfifoat(root,"fifo",0600));fresh();assert(pg_journal_open(&journal,root,"fifo",&view,&control));assert(journal.file==-1);
 int fd=openat(root,"oversized",O_CREAT|O_RDWR,0600);assert(fd>=0&&!ftruncate(fd,PG_JOURNAL_LIMIT+1));close(fd);fresh();assert(pg_journal_open(&journal,root,"oversized",&view,&control));
 fresh();assert(!pg_journal_open(&journal,root,"quota",&view,&control));journal.end=PG_JOURNAL_LIMIT;pg_message m=message(PG_USER,"quota");pg_session_change c={.kind=PG_SESSION_APPEND,.messages=&m,.count=1};assert(pg_session_commit(&view,&control,&c));assert(!control.failed&&!journal.failed&&!write_calls&&!view.count);journal.end=0;pg_journal_close(&journal);
}
int main(void){char directory[]="/tmp/pangu-journal-XXXXXX";assert(mkdtemp(directory));root=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(root>=0);roundtrip();torn_and_corrupt();failures();maximum();abi();paths_and_quota();const char *names[]={"session","frames","torn","corrupt","short-io","failure-1","failure-2","failure-3","failure-4","sync-failure","link","fifo","oversized","quota","maximum","abi"};for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i)assert(!unlinkat(root,names[i],0));close(root);assert(!rmdir(directory));puts("PASS: journal atomic batches/reopen/process restart/lock, every tail cut and byte corruption, short IO/EINTR, write/fsync/crash failures, safe paths and quotas");return 0;}
