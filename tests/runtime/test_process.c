#define _GNU_SOURCE
#include "../../src/runtime/process_layout.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static pg_process process;
static pg_process_ops service;
static int root;
static unsigned setup_calls,setup_fail;
static int in_setup;
static int read_error,wait_error,reap_error,kill_errors,clock_error,endless_output;
static int setup_fault(void){return in_setup&&++setup_calls==setup_fail;}
#define WRAP(name,signature,args) int __real_##name signature; int __wrap_##name signature {if(setup_fault())return ENOMEM;return __real_##name args;}
WRAP(posix_spawn_file_actions_init,(posix_spawn_file_actions_t *p),(p))
WRAP(posix_spawnattr_init,(posix_spawnattr_t *p),(p))
WRAP(posix_spawn_file_actions_addfchdir_np,(posix_spawn_file_actions_t *p,int fd),(p,fd))
WRAP(posix_spawn_file_actions_addopen,(posix_spawn_file_actions_t *p,int fd,const char *path,int flags,mode_t mode),(p,fd,path,flags,mode))
WRAP(posix_spawn_file_actions_adddup2,(posix_spawn_file_actions_t *p,int from,int to),(p,from,to))
WRAP(posix_spawn_file_actions_addclosefrom_np,(posix_spawn_file_actions_t *p,int low),(p,low))
WRAP(posix_spawnattr_setsigmask,(posix_spawnattr_t *p,const sigset_t *s),(p,s))
WRAP(posix_spawnattr_setsigdefault,(posix_spawnattr_t *p,const sigset_t *s),(p,s))
WRAP(posix_spawnattr_setpgroup,(posix_spawnattr_t *p,pid_t group),(p,group))
WRAP(posix_spawnattr_setflags,(posix_spawnattr_t *p,short flags),(p,flags))
WRAP(posix_spawn,(pid_t *pid,const char *path,const posix_spawn_file_actions_t *actions,const posix_spawnattr_t *attr,char *const argv[],char *const env[]),(pid,path,actions,attr,argv,env))
int __real_pipe2(int [2],int);
int __wrap_pipe2(int p[2],int flags){if(setup_fault()){errno=EMFILE;return -1;}return __real_pipe2(p,flags);}
int __real_fcntl(int,int,...);
int __wrap_fcntl(int fd,int cmd,...){if(setup_fault()){errno=EIO;return -1;}if(cmd==F_GETFD||cmd==F_GETFL)return __real_fcntl(fd,cmd);va_list ap;va_start(ap,cmd);int value=va_arg(ap,int);va_end(ap);return __real_fcntl(fd,cmd,value);}
int __real_sigemptyset(sigset_t *);
int __wrap_sigemptyset(sigset_t *p){if(setup_fault()){errno=EIO;return -1;}return __real_sigemptyset(p);}
int __real_sigaddset(sigset_t *,int);
int __wrap_sigaddset(sigset_t *p,int sig){if(setup_fault()){errno=EIO;return -1;}return __real_sigaddset(p,sig);}
int __real_sigaction(int,const struct sigaction *,struct sigaction *);
int __wrap_sigaction(int sig,const struct sigaction *a,struct sigaction *old){if(setup_fault()){errno=EIO;return -1;}return __real_sigaction(sig,a,old);}
int __real_clock_gettime(clockid_t,struct timespec *);
int __wrap_clock_gettime(clockid_t id,struct timespec *p){if(setup_fault()||clock_error){clock_error=0;errno=EIO;return -1;}return __real_clock_gettime(id,p);}
ssize_t __real_read(int,void *,size_t);
ssize_t __wrap_read(int fd,void *p,size_t n){if(endless_output&&process.exited&&process.reason){assert(n);*(char *)p='x';return 1;}if(read_error){errno=read_error;read_error=0;return -1;}return __real_read(fd,p,n);}
int __real_waitid(idtype_t,id_t,siginfo_t *,int);
int __wrap_waitid(idtype_t type,id_t id,siginfo_t *info,int flags){if(wait_error){errno=wait_error;wait_error=0;return -1;}return __real_waitid(type,id,info,flags);}
pid_t __real_waitpid(pid_t,int *,int);
pid_t __wrap_waitpid(pid_t pid,int *status,int flags){if(reap_error){errno=reap_error;reap_error=0;return -1;}return __real_waitpid(pid,status,flags);}
int __real_kill(pid_t,int);
int __wrap_kill(pid_t pid,int sig){if(kill_errors){--kill_errors;errno=EPERM;return -1;}return __real_kill(pid,sig);}
static uint64_t now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static unsigned fds(void){DIR *d=opendir("/proc/self/fd");assert(d);unsigned n=0;while(readdir(d))++n;assert(!closedir(d));return n;}
typedef struct {char output[2][70000];size_t size[2];pg_process_event end;} collected;
static collected result;
static pg_process_request request(void){return (pg_process_request){.directory=root};}
static void start(const char *command,uint64_t timeout){pg_process_request r=request();r.timeout_ms=timeout;assert(!service.start(service.context,&r,(pg_slice){command,strlen(command)}));}
static pg_process_event collect(void){uint64_t until=now()+5000;for(;;){assert(now()<until);pg_process_event e;int rc=service.next(service.context,&e);assert(rc>=0);if(!rc){assert(e.kind==PG_PROCESS_WAIT);usleep(1000);continue;}if(e.kind==PG_PROCESS_OUTPUT){assert(e.stream<2&&e.bytes.data==process.buffer&&e.bytes.size<=4096);assert(result.size[e.stream]+e.bytes.size<sizeof(result.output[0]));memcpy(result.output[e.stream]+result.size[e.stream],e.bytes.data,e.bytes.size);result.size[e.stream]+=e.bytes.size;}else{assert(e.kind==PG_PROCESS_EXIT);result.end=e;return e;}}}
static void reset(void){assert(!service.close(service.context));memset(&result,0,sizeof(result));assert(!service.close(service.context));}
static void normal(void){
 start("printf out; printf err >&2",0);pg_process_event e=collect();assert(!e.exit_code&&e.reason==PG_PROCESS_NORMAL&&!e.error);assert(result.size[0]==3&&!memcmp(result.output[0],"out",3));assert(result.size[1]==3&&!memcmp(result.output[1],"err",3));assert(!pg_process_next(&process,&e)&&e.kind==PG_PROCESS_WAIT);reset();
 start("printf partial; exit 7",0);e=collect();assert(e.exit_code==7&&!e.signal&&e.reason==PG_PROCESS_NORMAL&&!memcmp(result.output[0],"partial",7));reset();
 start("kill -9 $$",0);e=collect();assert(e.exit_code==137&&e.signal==9&&e.reason==PG_PROCESS_NORMAL);reset();
 start("exec >/dev/null 2>/dev/null; sleep .03; exit 7",0);e=collect();assert(e.exit_code==7&&e.reason==PG_PROCESS_NORMAL&&!result.size[0]&&!result.size[1]);reset();
 start("printf 'a\\000b'",0);e=collect();assert(!e.exit_code&&result.size[0]==3&&!memcmp(result.output[0],"a\0b",3));reset();
 start("head -c 32768 /dev/zero; head -c 32768 /dev/zero >&2",0);e=collect();assert(!e.exit_code&&result.size[0]==32768&&result.size[1]==32768);reset();
 start("if read x; then printf bad; else printf eof; fi",0);e=collect();assert(!e.exit_code&&result.size[0]==3&&!memcmp(result.output[0],"eof",3));reset();
 pg_process_request r=request();assert(!pg_process_start(&process,&r,(pg_slice){0,0}));e=collect();assert(!e.exit_code&&!result.size[0]&&!result.size[1]);reset();
}
static void cancellation(void){
 uint64_t begin=now();start("printf begin; sleep 2; printf late",0);assert(now()-begin<1000);int pid=process.pid;pg_process_request r=request();assert(pg_process_start(&process,&r,(pg_slice){"exit 0",6})==EBUSY&&process.pid==pid);
 uint64_t limit=now()+1000;for(;;){pg_process_event e;assert(now()<limit);int rc=pg_process_next(&process,&e);if(rc&&e.kind==PG_PROCESS_OUTPUT){assert(e.bytes.size==5&&!memcmp(e.bytes.data,"begin",5));break;}usleep(1000);}
 service.cancel(service.context);service.cancel(service.context);pg_process_event e=collect();assert(e.reason==PG_PROCESS_CANCELLED&&e.exit_code==137&&e.signal==9);assert(waitpid(pid,0,WNOHANG)==-1&&errno==ECHILD);reset();
 start("printf before; sleep 2; printf late",20);e=collect();assert(e.reason==PG_PROCESS_TIMED_OUT&&e.exit_code==137);assert(result.size[0]==6&&!memcmp(result.output[0],"before",6));reset();
 start("sleep 2",0);pid=process.pid;limit=now()+1000;int pending;do{pending=pg_process_close(&process);assert(pending>=0&&now()<limit);if(pending)usleep(1000);}while(pending);assert(process.pid==-1&&process.state==0&&waitpid(pid,0,WNOHANG)==-1&&errno==ECHILD);reset();
}
static void inheritance(void){
 assert(!setenv("PG_PROCESS_TEST","parent-only",1));start("printf '%s' \"${PG_PROCESS_TEST-unset}\"",0);pg_process_event e=collect();assert(!e.exit_code&&result.size[0]==5&&!memcmp(result.output[0],"unset",5));reset();
 char *env[]={"PATH=/usr/bin:/bin","PG_PROCESS_TEST=provided",0};pg_process_request r=request();r.environment=env;const char *command="printf '%s:' \"$PG_PROCESS_TEST\"; pwd -P";assert(!pg_process_start(&process,&r,(pg_slice){command,strlen(command)}));e=collect();char expected[512];char link[64],cwd[400];snprintf(link,sizeof(link),"/proc/self/fd/%d",root);ssize_t n=readlink(link,cwd,sizeof(cwd)-1);assert(n>0);cwd[n]=0;snprintf(expected,sizeof(expected),"provided:%s\n",cwd);assert(!e.exit_code&&result.size[0]==strlen(expected)&&!memcmp(result.output[0],expected,strlen(expected)));reset();
 int leak=fcntl(root,F_DUPFD,50);assert(leak>=50);char script[128];snprintf(script,sizeof(script),"if [ -e /proc/$$/fd/%d ]; then printf leak; exit 1; else printf clean; fi",leak);start(script,0);e=collect();assert(!e.exit_code&&result.size[0]==5&&!memcmp(result.output[0],"clean",5)&&fcntl(leak,F_GETFD)>=0);assert(!close(leak));reset();
 struct sigaction saved,ignored={.sa_handler=SIG_IGN};assert(!sigemptyset(&ignored.sa_mask)&&!sigaction(SIGPIPE,&ignored,&saved));start("kill -PIPE $$",0);assert(!sigaction(SIGPIPE,&saved,0));e=collect();assert(e.exit_code==141&&e.signal==13);reset();
 assert(!sigaction(SIGCHLD,&ignored,&saved));r=request();assert(pg_process_start(&process,&r,(pg_slice){"touch forbidden",15})==ENOTSUP);assert(!sigaction(SIGCHLD,&saved,0));assert(faccessat(root,"forbidden",F_OK,0)&&errno==ENOENT);reset();
 ignored.sa_handler=SIG_DFL;ignored.sa_flags=SA_NOCLDWAIT;assert(!sigaction(SIGCHLD,&ignored,&saved));assert(pg_process_start(&process,&r,(pg_slice){"touch forbidden",15})==ENOTSUP);assert(!sigaction(SIGCHLD,&saved,0));reset();
}
static void failures(void){
 pg_process copy=process;pg_process_request r=request();r.shell="/nonexistent/pangu-shell";assert(pg_process_start(&process,&r,(pg_slice){"exit 0",6})==ENOENT&&!memcmp(&copy,&process,sizeof(copy)));
 r=request();r.shell="relative";assert(pg_process_start(&process,&r,(pg_slice){"exit 0",6})==EINVAL&&!memcmp(&copy,&process,sizeof(copy)));
 r=request();r.timeout_ms=2147483648ULL;assert(pg_process_start(&process,&r,(pg_slice){"exit 0",6})==EINVAL&&!memcmp(&copy,&process,sizeof(copy)));
 r=request();assert(pg_process_start(&process,&r,(pg_slice){"a\0b",3})==EINVAL&&!memcmp(&copy,&process,sizeof(copy)));
 char maximum[4096];memset(maximum,'x',sizeof(maximum));assert(pg_process_start(&process,&r,(pg_slice){maximum,sizeof(maximum)})==EINVAL);maximum[0]='#';assert(!pg_process_start(&process,&r,(pg_slice){maximum,4095}));assert(!collect().exit_code);reset();
 r.directory=-1;assert(pg_process_start(&process,&r,(pg_slice){"exit 0",6})==EINVAL);assert(!memcmp(&copy,&process,sizeof(copy)));
}
static void setup_errors(void){
 pg_process_request r=request();unsigned before=fds();setup_calls=0;setup_fail=0;in_setup=1;int rc=pg_process_start(&process,&r,(pg_slice){"printf good",11});in_setup=0;assert(!rc);unsigned points=setup_calls;assert(points>20);assert(!collect().exit_code);reset();pg_process copy=process;
 for(unsigned i=1;i<=points;++i){setup_calls=0;setup_fail=i;in_setup=1;rc=pg_process_start(&process,&r,(pg_slice){"touch forbidden",15});in_setup=0;assert(rc>0&&!memcmp(&copy,&process,sizeof(copy)));assert(fds()==before);assert(faccessat(root,"forbidden",F_OK,0)&&errno==ENOENT);assert(waitpid(-1,0,WNOHANG)==-1&&errno==ECHILD);}
 setup_fail=0;printf("process setup failure points: %u\n",points);
}
static void polling_errors(void){
 for(unsigned i=0;i<3;++i){start("printf recovered",0);if(i==0)read_error=EINTR;if(i==1)read_error=EAGAIN;if(i==2)wait_error=EINTR;pg_process_event e=collect();assert(e.reason==PG_PROCESS_NORMAL&&!e.error&&result.size[0]==9&&!memcmp(result.output[0],"recovered",9));reset();}
 start("exit 7",0);reap_error=EINTR;pg_process_event e=collect();assert(e.exit_code==7&&!e.error);reset();
 start("sleep 2",0);read_error=EIO;e=collect();assert(e.reason==PG_PROCESS_FAILED&&e.error==EIO);reset();
 start("sleep 2",0);wait_error=EIO;e=collect();assert(e.reason==PG_PROCESS_FAILED&&e.error==EIO);reset();
 start("sleep 2",0);pg_process_cancel(&process);kill_errors=2;assert(pg_process_next(&process,&e)>=0&&!process.killed&&process.error==EPERM);e=collect();assert(e.reason==PG_PROCESS_CANCELLED&&e.error==EPERM&&!kill_errors);reset();
 start("sleep 2",0);clock_error=1;int rc=pg_process_next(&process,&e);assert(rc>=0);if(!rc||e.kind!=PG_PROCESS_EXIT)e=collect();assert(e.reason==PG_PROCESS_FAILED&&e.error==EIO);reset();
 start("sleep 2",0);pg_process_cancel(&process);endless_output=1;uint64_t limit=now()+800;unsigned done=0;while(!done){assert(now()<limit);if(pg_process_next(&process,&e)&&e.kind==PG_PROCESS_EXIT){assert(e.reason==PG_PROCESS_CANCELLED);done=1;}usleep(1000);}endless_output=0;reset();
}
static void page_tail(void){
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *memory=mmap(0,(size_t)page*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED&&!mprotect(memory+page*2,(size_t)page,PROT_NONE));pg_process *guard=(pg_process *)(void *)(memory+page*2-sizeof(pg_process));pg_process_init(guard);pg_process_request r=request();assert(!pg_process_start(guard,&r,(pg_slice){"printf guard",12}));uint64_t deadline=now()+1000;unsigned output=0,done=0;while(!done){pg_process_event e;assert(now()<deadline);if(pg_process_next(guard,&e)){if(e.kind==PG_PROCESS_OUTPUT){assert(e.bytes.size==5&&!memcmp(e.bytes.data,"guard",5));output=1;}else{assert(!e.exit_code);done=1;}}else usleep(1000);}assert(output&&!pg_process_close(guard));assert(!munmap(memory,(size_t)page*3));
 memory=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED&&!mprotect(memory+page,(size_t)page,PROT_NONE));char *command=memory+page-6;memcpy(command,"exit 0",6);assert(!pg_process_start(&process,&r,(pg_slice){command,6}));assert(!collect().exit_code);reset();assert(!munmap(memory,(size_t)page*2));
}
static void descendant_output(void){
 start("(sleep .03; printf a; sleep .03; printf b; sleep .03; printf c; sleep .03; printf d) & exit 0",0);pg_process_event e=collect();assert(!e.exit_code&&result.size[0]==4&&!memcmp(result.output[0],"abcd",4));reset();
 uint64_t begin=now();start("sleep 1 & printf quiet; exit 0",0);e=collect();assert(!e.exit_code&&result.size[0]==5&&!memcmp(result.output[0],"quiet",5)&&now()-begin<800);reset();usleep(1050000);
}
static void low_descriptors(void){pid_t child=fork();assert(child>=0);if(!child){assert(dup2(root,0)==0);assert(!close(1)&&!close(2));root=0;reset();start("printf out; printf err >&2",0);pg_process_event e=collect();assert(!e.exit_code&&result.size[0]==3&&result.size[1]==3&&fcntl(0,F_GETFD)>=0);reset();_exit(0);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));}
int main(void){assert(SIG_IGN==(void (*)(int))1&&SIG_DFL==(void (*)(int))0);char directory[]="/tmp/pangu-process-XXXXXX";assert(mkdtemp(directory));root=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(root>=0);pg_process_init(&process);service=pg_process_service(&process);assert(service.context==&process&&service.start&&service.next&&service.cancel&&service.close);unsigned before=fds();normal();cancellation();inheritance();failures();setup_errors();polling_errors();page_tail();descendant_output();low_descriptors();assert(fds()==before);assert(!close(root)&&!rmdir(directory));puts("process: real spawn/streams/exit, nonblocking wait, timeout/cancel/close/reap, cwd/env/fds/signals, setup/poll failures, guards and idle-stdio grace OK");return 0;}
