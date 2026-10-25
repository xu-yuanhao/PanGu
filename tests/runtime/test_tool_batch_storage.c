#define _GNU_SOURCE
#include "../../src/runtime/journal_layout.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ucontext.h>
#include <signal.h>
#include <unistd.h>
static pg_session view,saved;
static pg_session_control control,saved_control;
static pg_message records[PG_SESSION_BATCH_LIMIT];
static pg_journal journal={.file=-1,.directory=-1};
static unsigned commits,mode,mutate_source;
static int fail_pwrite,fail_fsync;static size_t write_left;
ssize_t __real_pwrite(int,const void *,size_t,off_t);
int __real_fsync(int);
ssize_t __wrap_pwrite(int fd,const void *bytes,size_t n,off_t offset){if(fail_pwrite&&fd==journal.file){if(!write_left){errno=EIO;return -1;}if(n>write_left)n=write_left;ssize_t result=__real_pwrite(fd,bytes,n,offset);if(result>0)write_left-=(size_t)result;return result;}return __real_pwrite(fd,bytes,n,offset);}
int __wrap_fsync(int fd){if(fail_fsync&&fd==journal.file){fail_fsync=0;errno=EIO;return -1;}return __real_fsync(fd);}
static pg_message original_records[PG_SESSION_BATCH_LIMIT];
static pg_store_result observe(void *ctx,const pg_session_change *change,uint64_t seq,uint64_t *next){(void)ctx;++commits;assert(change->messages!=records);assert(!memcmp(&view,&saved,sizeof(view)));if(mode==1)return PG_STORE_REJECTED;if(mode==2)return PG_STORE_UNCERTAIN;assert(change->count<=PG_SESSION_BATCH_LIMIT);*next=seq+1;if(mutate_source)memset(records,0,sizeof(records));return PG_STORE_OK;}
static void fresh(void){memset(&view,0,sizeof(view));memset(&control,0,sizeof(control));control.storage=(pg_storage){0,pg_session_volatile_commit};commits=mode=mutate_source=0;}
static void input(void){pg_message message={.role=PG_USER,.size=3};memcpy(message.text,"run",4);pg_run_checkpoint cp={.id=1,.phase=PG_RUN_MODEL,.call_index=UINT64_MAX};pg_session_change change={.messages=&message,.count=1,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&change));}
static void prepare(unsigned count,int maximum){memset(records,0,sizeof(records));records[0].role=PG_ASSISTANT;if(maximum){records[0].size=4095;memset(records[0].text,'a',4095);memset(records[0].call_id,'a',63);memset(records[0].tool_name,'b',63);}for(unsigned i=1;i<=count;++i){pg_message *m=&records[i];m->role=PG_TOOL_CALL;m->size=maximum?4095:2;memset(m->text,maximum?'x':'{',m->size);if(!maximum)m->text[1]='}';snprintf(m->call_id,sizeof(m->call_id),"call-%u",i);snprintf(m->tool_name,sizeof(m->tool_name),"tool-%u",i);if(maximum){memset(m->call_id,'i',63);m->call_id[62]=(char)('0'+i);memset(m->tool_name,'n',63);}}}
static pg_run_checkpoint cp_for(unsigned call){pg_run_checkpoint cp=control.run;cp.phase=PG_RUN_TOOL_UNSAFE;cp.turns=1;cp.call_index=call;return cp;}
static void atomic_memory(void){fresh();input();prepare(8,0);pg_run_checkpoint cp=cp_for(2);pg_session_change c={.messages=records,.count=9,.checkpoint=&cp};saved=view;saved_control=control;control.storage=(pg_storage){0,observe};mode=1;assert(pg_session_commit(&view,&control,&c)&&commits==1&&!memcmp(&view,&saved,sizeof(view))&&control.sequence==saved_control.sequence&&!control.failed);
 mode=0;records[8].is_error=2;assert(pg_session_commit(&view,&control,&c)&&commits==1);records[8].is_error=0;
 c.count=10;assert(pg_session_commit(&view,&control,&c)&&commits==1);c.count=9;c.checkpoint=0;assert(pg_session_commit(&view,&control,&c)&&commits==1);c.checkpoint=&cp;
 char old_id[64];memcpy(old_id,records[8].call_id,64);strcpy(records[8].call_id,records[1].call_id);assert(pg_session_commit(&view,&control,&c)&&commits==1);memcpy(records[8].call_id,old_id,64);
 memcpy(original_records,records,sizeof(records));mutate_source=1;
 assert(!pg_session_commit(&view,&control,&c)&&view.count==10&&commits==2&&control.run.call_index==2);assert(!memcmp(&view.messages[1],original_records,sizeof(records)));
 fresh();input();saved=view;prepare(8,0);cp=cp_for(2);control.storage=(pg_storage){0,observe};mode=2;c.checkpoint=&cp;assert(pg_session_commit(&view,&control,&c)&&control.failed&&!memcmp(&view,&saved,sizeof(view)));unsigned before=commits;assert(pg_session_commit(&view,&control,&c)&&commits==before);
 fresh();input();prepare(8,0);cp=cp_for(2);c.checkpoint=&cp;view.count=56;assert(pg_session_commit(&view,&control,&c)&&view.count==56);
}
static void result(pg_message *m,unsigned i){memset(m,0,sizeof(*m));m->role=PG_TOOL_RESULT;m->size=2;memcpy(m->text,"ok",3);snprintf(m->call_id,sizeof(m->call_id),"call-%u",i);}
static void progress(void){fresh();input();prepare(3,0);pg_run_checkpoint cp=cp_for(2);pg_session_change c={.messages=records,.count=4,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&c));cp.call_index=4;assert(pg_session_commit(&view,&control,&(pg_session_change){.kind=PG_SESSION_CHECKPOINT,.checkpoint=&cp}));cp.call_index=2;pg_message extra=records[1];strcpy(extra.call_id,"call-4");assert(pg_session_commit(&view,&control,&(pg_session_change){.messages=&extra,.count=1,.checkpoint=&cp})&&view.count==5);pg_message answer;result(&answer,1);c.messages=&answer;c.count=1;cp.call_index=3;assert(!pg_session_commit(&view,&control,&c)&&view.count==6);
 saved=view;saved_control=control;cp.call_index=4;assert(pg_session_commit(&view,&control,&c)&&!memcmp(&view,&saved,sizeof(view))&&control.sequence==saved_control.sequence);
 result(&answer,2);assert(!pg_session_commit(&view,&control,&c));cp.call_index=UINT64_MAX;cp.phase=PG_RUN_MODEL;c.kind=PG_SESSION_CHECKPOINT;c.messages=0;c.count=0;assert(pg_session_commit(&view,&control,&c));
 result(&answer,3);c.kind=PG_SESSION_APPEND;c.messages=&answer;c.count=1;assert(!pg_session_commit(&view,&control,&c)&&view.count==8&&control.run.phase==PG_RUN_MODEL);
}
static void open_empty(int root,const char *name){fresh();journal=(pg_journal){.file=-1,.directory=-1};assert(!pg_journal_open(&journal,root,name,&view,&control));}
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write_file(int root,const char *name,const unsigned char *bytes,size_t length){int fd=openat(root,name,O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC,0600);assert(fd>=0);size_t pos=0;while(pos<length){ssize_t n=write(fd,bytes+pos,length-pos);assert(n>0);pos+=(size_t)n;}assert(!close(fd));}
static unsigned char file[PG_JOURNAL_RECORD_MAX+512],mutated[sizeof(file)];
static void wire(int root){open_empty(root,"batch");input();size_t prefix=(size_t)journal.end;prepare(8,1);pg_run_checkpoint cp=cp_for(2);pg_session_change c={.messages=records,.count=9,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&c));size_t length=(size_t)journal.end;saved=view;saved_control=control;assert(length-prefix==38229&&length-prefix<=PG_JOURNAL_RECORD_MAX);pg_journal_close(&journal);int fd=openat(root,"batch",O_RDONLY|O_CLOEXEC);assert(fd>=0&&read(fd,file,sizeof(file))==(ssize_t)length&&!close(fd));assert(u32(file+prefix+8)==5&&u32(file+prefix+28)==9);
 open_empty(root,"batch");assert(view.count==10&&control.run.call_index==2&&!memcmp(&view,&saved,sizeof(view)));pg_journal_close(&journal);
 pid_t child=fork();assert(child>=0);if(!child){open_empty(root,"batch");_exit(view.count==10&&control.run.call_index==2?0:1);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 // Sample page/record boundaries, including final byte and every cut of a small batch.
 size_t cuts[]={prefix,prefix+1,prefix+39,prefix+40,prefix+4096,prefix+8192,length-17,length-16,length-1};
 for(size_t i=0;i<sizeof(cuts)/sizeof(*cuts);++i){write_file(root,"cut",file,cuts[i]);open_empty(root,"cut");assert(view.count==1&&journal.end==prefix);pg_journal_close(&journal);assert(!unlinkat(root,"cut",0));}
 memcpy(mutated,file,length);mutated[length-17]^=1;write_file(root,"bad",mutated,length);fresh();journal=(pg_journal){.file=-1,.directory=-1};assert(pg_journal_open(&journal,root,"bad",&view,&control)&&!view.count&&!control.sequence);assert(!unlinkat(root,"bad",0));assert(!unlinkat(root,"batch",0));
}
static uint64_t hash(const unsigned char *p,size_t n){uint64_t h=UINT64_C(0xcbf29ce484222325);for(size_t i=0;i<n;++i)h=(h^p[i])*UINT64_C(0x100000001b3);return h;}
static void put32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static void put64(unsigned char *p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=(unsigned char)(v>>(i*8));}
static void checksums(unsigned char *frame,size_t n){put64(frame+32,hash(frame,32));put64(frame+n-16,hash(frame,n-16));}
static void reject_file(int root,const unsigned char *data,size_t n){write_file(root,"invalid",data,n);fresh();journal=(pg_journal){.file=-1,.directory=-1};assert(pg_journal_open(&journal,root,"invalid",&view,&control)&&!view.count&&!control.sequence);struct stat st;assert(!fstatat(root,"invalid",&st,0)&&st.st_size==(off_t)n);assert(!unlinkat(root,"invalid",0));}
static void small_and_forged(int root){open_empty(root,"small");input();size_t prefix=(size_t)journal.end;prepare(3,0);pg_run_checkpoint cp=cp_for(2);pg_session_change c={.messages=records,.count=4,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&c));size_t length=(size_t)journal.end;pg_journal_close(&journal);int fd=openat(root,"small",O_RDONLY);assert(fd>=0&&read(fd,file,sizeof(file))==(ssize_t)length&&!close(fd));
 for(size_t cut=prefix;cut<length;++cut){write_file(root,"cut",file,cut);open_empty(root,"cut");assert(view.count==1&&journal.end==prefix);pg_journal_close(&journal);assert(!unlinkat(root,"cut",0));}
 for(size_t byte=prefix;byte<length;++byte){memcpy(mutated,file,length);mutated[byte]^=1;reject_file(root,mutated,length);}
 for(unsigned field=0;field<6;++field){memcpy(mutated,file,length);unsigned char *frame=mutated+prefix;size_t n=length-prefix;switch(field){case 0:put32(frame+28,2);break;case 1:put32(frame+28,10);break;case 2:put32(frame+24,1);break;case 3:put32(frame+8,4);break;case 4:put64(frame+n-56+16,4);break;case 5:put32(frame+8,6);break;}checksums(frame,n);reject_file(root,mutated,length);}
 memcpy(mutated,file,length);put32(mutated+prefix+8,2);put32(mutated+prefix+12,8593);put32(mutated+prefix+28,2);put64(mutated+prefix+32,hash(mutated+prefix,32));reject_file(root,mutated,prefix+40);
 memcpy(mutated,file,length);put32(mutated+prefix+12,PG_JOURNAL_RECORD_MAX+1);put64(mutated+prefix+32,hash(mutated+prefix,32));reject_file(root,mutated,prefix+40);
 assert(!unlinkat(root,"small",0));
}
static void uncertain_io(int root){size_t cuts[]={40,8192,38228};for(unsigned i=0;i<3;++i){open_empty(root,"partial");input();saved=view;size_t prefix=(size_t)journal.end;prepare(8,1);pg_run_checkpoint cp=cp_for(2);pg_session_change c={.messages=records,.count=9,.checkpoint=&cp};fail_pwrite=1;write_left=cuts[i];assert(pg_session_commit(&view,&control,&c)&&control.failed&&journal.failed&&!memcmp(&view,&saved,sizeof(view))&&control.sequence==1);fail_pwrite=0;pg_journal_close(&journal);open_empty(root,"partial");assert(view.count==1&&journal.end==prefix);pg_journal_close(&journal);assert(!unlinkat(root,"partial",0));}
 open_empty(root,"uncertain");input();saved=view;prepare(8,1);pg_run_checkpoint cp=cp_for(2);pg_session_change c={.messages=records,.count=9,.checkpoint=&cp};fail_fsync=1;assert(pg_session_commit(&view,&control,&c)&&control.failed&&!memcmp(&view,&saved,sizeof(view))&&control.sequence==1);pg_journal_close(&journal);open_empty(root,"uncertain");assert(view.count==10&&control.sequence==2&&control.run.call_index==2);pg_journal_close(&journal);assert(!unlinkat(root,"uncertain",0));
}
static void sequential_reopen(int root){open_empty(root,"round");input();prepare(3,0);pg_run_checkpoint cp=cp_for(2);pg_session_change c={.messages=records,.count=4,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&c));
 for(unsigned i=1;i<=3;++i){pg_message answer;result(&answer,i);cp=control.run;if(i<3)cp.call_index=2+i;else{cp.phase=PG_RUN_MODEL;cp.call_index=UINT64_MAX;}c=(pg_session_change){.messages=&answer,.count=1,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&c));pg_journal_close(&journal);open_empty(root,"round");assert(view.count==5+i&&control.run.call_index==cp.call_index&&control.run.phase==cp.phase);}
 pg_journal_close(&journal);assert(!unlinkat(root,"round",0));
 open_empty(root,"cancel");input();prepare(8,0);cp=cp_for(2);c=(pg_session_change){.messages=records,.count=9,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&c));cp.phase=PG_RUN_ABORTING;c=(pg_session_change){.kind=PG_SESSION_CHECKPOINT,.checkpoint=&cp};assert(!pg_session_commit(&view,&control,&c));for(unsigned i=0;i<8;++i){result(&records[i],i+1);records[i].is_error=1;}cp.phase=PG_RUN_TERMINAL;cp.call_index=UINT64_MAX;cp.outcome=PG_CANCELLED;c=(pg_session_change){.messages=records,.count=7,.checkpoint=&cp};assert(pg_session_commit(&view,&control,&c)&&view.count==10);c.count=8;assert(!pg_session_commit(&view,&control,&c));pg_journal_close(&journal);open_empty(root,"cancel");assert(view.count==18&&control.run.phase==PG_RUN_TERMINAL&&control.run.outcome==PG_CANCELLED);pg_journal_close(&journal);assert(!unlinkat(root,"cancel",0));
}
static int stack_root;
static void small_stack_entry(void){signal(SIGSEGV,SIG_DFL);fresh();journal=(pg_journal){.file=-1,.directory=-1};int rc=pg_journal_open(&journal,stack_root,"stack-probe",&view,&control);_exit(rc?2:0);}
static void stack_guard(int root){long page=sysconf(_SC_PAGESIZE);size_t lower=128*1024,upper=32*1024,length=lower+(size_t)page+upper;char *map=mmap(0,length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+lower,(size_t)page,PROT_NONE));stack_root=root;pid_t pid=fork();assert(pid>=0);if(!pid){struct rlimit limit={0,0};setrlimit(RLIMIT_CORE,&limit);ucontext_t context;assert(!getcontext(&context));context.uc_stack.ss_sp=map+lower+page;context.uc_stack.ss_size=upper;context.uc_link=0;makecontext(&context,small_stack_entry,0);setcontext(&context);_exit(3);}int status;assert(waitpid(pid,&status,0)==pid&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGSEGV);assert(faccessat(root,"stack-probe",F_OK,0)<0&&errno==ENOENT);assert(!munmap(map,length));}
int main(void){atomic_memory();progress();char path[]="/tmp/pangu-batch-store-XXXXXX";assert(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(root>=0);wire(root);small_and_forged(root);uncertain_io(root);sequential_reopen(root);stack_guard(root);assert(!close(root)&&!rmdir(path));puts("batch storage: bounded atomic intent, reject/uncertain, ordered results, v5 maximum/reopen/restart/torn/corrupt/uncertain IO and stack guard OK");return 0;}
