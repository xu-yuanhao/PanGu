#define _GNU_SOURCE
#include "../../src/runtime/bash_tool_layout.h"
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
static pg_process process;
static pg_bash_tool tool;
static pg_execution_env execution_environment;
static pg_tool_registry registry;
static pg_tools svc;
static char result[4096];static size_t n;
static int fail_temp,write_mode,fail_close,fail_unlink,token_fault,token_calls,spool_fd=-1;
int __real_mkostemp(char *,int);
ssize_t __real_write(int,const void *,size_t);
int __real_close(int);
int __real_unlink(const char *);
struct json_tokener *__real_json_tokener_new(void);
int __wrap_mkostemp(char *path,int flags){if(fail_temp){fail_temp=0;errno=EACCES;return -1;}spool_fd=__real_mkostemp(path,flags);return spool_fd;}
ssize_t __wrap_write(int fd,const void *data,size_t size){if(fd==spool_fd){if(write_mode==1){write_mode=4;errno=EINTR;return -1;}if(write_mode==4&&size>2)size=2;if(write_mode==2){write_mode=0;errno=ENOSPC;return -1;}if(write_mode==3){write_mode=0;return 0;}}return __real_write(fd,data,size);}
int __wrap_close(int fd){int hit=fd==spool_fd;int rc=__real_close(fd);if(hit){spool_fd=-1;if(fail_close){fail_close=0;errno=EIO;return -1;}}return rc;}
int __wrap_unlink(const char *path){if(fail_unlink&&!strcmp(path,tool.path)){fail_unlink=0;errno=EACCES;return -1;}return __real_unlink(path);}
struct json_tokener *__wrap_json_tokener_new(void){if(token_fault&&++token_calls==token_fault)return 0;return __real_json_tokener_new();}
static pg_slice s(const char *p){return (pg_slice){p,strlen(p)};}
static uint64_t now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static void setup(int root){pg_process_init(&process);execution_environment=(pg_execution_env){.abi=PG_EXECUTION_ENV_ABI,.size=sizeof(pg_execution_env),.process=pg_process_service(&process),.directory=root};assert(!pg_bash_tool_init(&tool,&execution_environment));pg_tool_definition d=pg_bash_tool_definition(&tool);assert(!d.replay_safe&&d.async);assert(!pg_tool_registry_init(&registry,&d,1));svc=pg_tool_registry_service(&registry);}
static int start(const char *args){return svc.execute(svc.context,s("bash"),s(args),result,sizeof(result),&n);}
static void released(void){uint64_t until=now()+3000;while(svc.close(svc.context)){assert(now()<until);usleep(1000);}assert(!registry.active&&process.pid==-1&&tool.file==-1);}
static int drain(int rc,unsigned *chunks){uint64_t until=now()+6000;while(rc>=PG_TOOL_WAIT){assert(now()<until);if(rc==PG_TOOL_OUTPUT){assert(n&&n<sizeof(result));++*chunks;}else usleep(1000);rc=svc.next(svc.context,result,sizeof(result),&n);}assert(n<sizeof(result));result[n]=0;if(tool.stage==5){struct json_object *metadata=0;assert(!pg_bash_tool_snapshot(&tool,&metadata));struct json_object *structured=json_object_object_get(metadata,"structuredOutput");assert(json_object_get_int(json_object_object_get(structured,"exitCode"))==tool.exit_code);assert(json_object_get_boolean(json_object_object_get(structured,"truncated"))==(int)(tool.flags&1));assert((size_t)json_object_get_string_len(json_object_object_get(structured,"output"))==tool.tail_size);assert(json_object_equal(json_object_object_get(json_object_array_get_idx(json_object_object_get(metadata,"output"),0),"text"),json_object_object_get(structured,"output")));if(tool.reason||tool.exit_code||tool.flags&3)assert(json_object_array_length(json_object_object_get(metadata,"diagnostics")));json_object_put(metadata);}released();return rc;}
static int run(const char *args){unsigned chunks=0;return drain(start(args),&chunks);}
static int run_command(const char *command){struct json_object *o=json_object_new_object();assert(o&&!json_object_object_add(o,"command",json_object_new_string(command)));const char *args=json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN);int rc=run(args);json_object_put(o);return rc;}
static size_t fd_count(void){DIR *d=opendir("/proc/self/fd");assert(d);size_t count=0;while(readdir(d))++count;assert(!closedir(d));return count;}
static void remove_artifact(void){if(tool.path[0]){assert(!unlink(tool.path));tool.path[0]=0;}}
static void arguments(void){const char *bad[]={"{\"command\\u0000ignored\":\"true\"}","{\"command\":\"true\",\"timeout\\u0000x\":1}","{}","[]","null","{\"command\":1}","{\"command\":\"a\\u0000b\"}","{\"command\":\"true\",\"timeout\":0}","{\"command\":\"true\",\"timeout\":-1}","{\"command\":\"true\",\"timeout\":null}","{\"command\":\"true\",\"timeout\":\"1\"}","{\"command\":\"true\",\"timeout\":2147483.648}","{\"command\":\"true\",\"other\":1}","{\"command\":\"true\",\"timeout\":1,\"other\":1}","{\"command\":\"true\"}x","{\"command\":\"true\",\"timeout\":NaN}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){assert(start(bad[i])==PG_TOOL_ERROR&&!registry.active&&tool.stage==0&&process.pid==-1&&tool.file==-1);}
 assert(run("{\"command\":\"\"}")==PG_TOOL_OK&&strstr(result,"exit=0"));
 assert(run("{\"command\":\"printf hello; printf err >&2; exit 7\"}")==PG_TOOL_ERROR&&strstr(result,"hello")&&strstr(result,"err")&&strstr(result,"exit=7")&&!tool.path[0]);
 assert(run("{\"command\":\"kill -TERM $$\"}")==PG_TOOL_ERROR&&strstr(result,"exit=143"));
 assert(run("{\"command\":\"printf timeout; sleep 2\",\"timeout\":0.03}")==PG_TOOL_ERROR&&strstr(result,"timeout")&&strstr(result,"reason=timeout"));
}
static void long_output(void){assert(run("{\"command\":\"head -c 10000 /dev/zero | tr '\\u0000' x\"}")==PG_TOOL_ERROR);/* NUL commands are rejected before execution */
 assert(run("{\"command\":\"printf '%060000d' 0\"}")==PG_TOOL_OK&&tool.tail_size==PG_BASH_RETAIN&&strstr(result,"earlier output omitted")&&tool.path[0]);struct stat st;assert(!stat(tool.path,&st)&&(st.st_mode&0777)==0600&&st.st_size==60000);int fd=open(tool.path,O_RDONLY);assert(fd>=0);char bytes[60000];assert(read(fd,bytes,sizeof(bytes))==60000);for(size_t i=0;i<sizeof(bytes)-1;++i)assert(bytes[i]=='0');assert(!close(fd));remove_artifact();
 assert(run("{\"command\":\"printf '\\n%.0s' {1..2500}\"}")==PG_TOOL_OK&&tool.tail_size==2000&&tool.path[0]);remove_artifact();
 assert(run_command("printf '\\357\\273\\277\\344'; sleep .02; printf '\\270\\255'; printf '\\377' >&2")==PG_TOOL_OK&&strstr(result,"中")&&strstr(result,"\xef\xbf\xbd")&&!strstr(result,"\xef\xbb\xbf"));
}
static void raw_and_tail(void){
 assert(run_command("printf '%010000d' 0")==PG_TOOL_OK&&tool.tail_size==10000&&!tool.path[0]&&!(tool.flags&1));
 assert(run_command("printf '\\357\\273\\277'; printf '%060000d' 0; printf '\\377'")==PG_TOOL_OK&&tool.path[0]);int fd=open(tool.path,O_RDONLY);assert(fd>=0);unsigned char data[60004];assert(read(fd,data,sizeof(data))==(ssize_t)sizeof(data)&&!memcmp(data,"\xef\xbb\xbf",3)&&data[60003]==0xff);assert(!close(fd));assert(memmem(result,n,"\xef\xbf\xbd",3));remove_artifact();
 assert(run_command("printf 'a'; printf '\\360\\237\\231\\202%.0s' {1..15000}")==PG_TOOL_OK&&tool.tail_size==PG_BASH_RETAIN&&tool.path[0]);for(size_t i=0;i<tool.tail_size;i+=4)assert(!memcmp(tool.tail+i,"\xf0\x9f\x99\x82",4));remove_artifact();
}
typedef struct {int directory;char *const *environment;unsigned calls,reject;} prepare_fixture;
static int prepare_command(void *context,pg_execution_request *request){prepare_fixture *fixture=context;fixture->calls++;assert(request->command.size==15&&!memcmp(request->command.data,"printf original",15));if(fixture->reject)return 1;request->command=s(": > prepared; printf '%s' \"$PANGU_TEST_PREPARED\"");request->directory=fixture->directory;request->environment=fixture->environment;return 0;}
static void execution_prepare(int root){
 assert(!mkdirat(root,"prepared-dir",0700));int directory=openat(root,"prepared-dir",O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(directory>=0);char *environment[]={"PANGU_TEST_PREPARED=ready",0};prepare_fixture fixture={directory,environment,0,0};
 setup(root);execution_environment.context=&fixture;execution_environment.prepare=prepare_command;
 assert(run_command("printf original")==PG_TOOL_OK&&strstr(result,"ready")&&fixture.calls==1&&faccessat(directory,"prepared",F_OK,0)==0);
 assert(!unlinkat(directory,"prepared",0));fixture.reject=1;assert(run_command("printf original")==PG_TOOL_ERROR&&strstr(result,"preparation rejected")&&fixture.calls==2&&tool.stage==6&&process.pid==-1&&tool.file==-1&&faccessat(directory,"prepared",F_OK,0)<0&&errno==ENOENT);
 assert(!close(directory)&&!unlinkat(root,"prepared-dir",AT_REMOVEDIR));
}
static void cancellation(void){unsigned chunks=0;int rc=start("{\"command\":\"printf begin; sleep 2; printf late\"}");assert(rc==PG_TOOL_WAIT);uint64_t until=now()+1000;do{assert(now()<until);usleep(1000);rc=svc.next(svc.context,result,sizeof(result),&n);}while(rc==PG_TOOL_WAIT);assert(rc==PG_TOOL_OUTPUT&&n==5&&!memcmp(result,"begin",5));svc.cancel(svc.context);svc.cancel(svc.context);assert(drain(rc,&chunks)==PG_TOOL_ERROR&&strstr(result,"begin")&&!strstr(result,"late")&&strstr(result,"reason=cancelled")&&!tool.path[0]);
 rc=start("{\"command\":\"printf abandoned; sleep 2\"}");assert(rc==PG_TOOL_WAIT);char path[64];strcpy(path,tool.path);released();assert(access(path,F_OK)<0&&errno==ENOENT);
}
static void failures(void){
 fail_temp=1;assert(run_command("printf never")==PG_TOOL_ERROR&&!tool.path[0]);
 for(int i=1;i<=3;++i){token_calls=0;token_fault=i;assert(run_command("printf never")==PG_TOOL_ERROR&&process.pid==-1);token_fault=0;}
 write_mode=1;assert(run_command("printf short-eintr")==PG_TOOL_OK&&strstr(result,"short-eintr")&&!tool.path[0]);write_mode=0;
 for(int i=2;i<=3;++i){write_mode=i;assert(run_command("printf disk-error; sleep .01")==PG_TOOL_ERROR&&tool.captured==0&&!tool.path[0]);}
 fail_close=1;assert(run_command("printf close-error")==PG_TOOL_ERROR&&tool.path[0]&&strstr(result,"partial raw output"));remove_artifact();
 fail_unlink=1;assert(run_command("printf unlink-error")==PG_TOOL_ERROR&&tool.path[0]);remove_artifact();
 unsigned chunks=0;int rc=start("{\"command\":\"printf detach; sleep 2\"}");assert(rc==PG_TOOL_WAIT);fail_unlink=1;uint64_t until=now()+1000;int saw_error=0;while((rc=svc.close(svc.context))){if(rc<0)saw_error=1;assert(registry.active&&now()<until);usleep(1000);}assert(saw_error&&!registry.active&&!tool.path[0]);(void)chunks;
 assert(run_command("printf '%s' '\\u0000'")==PG_TOOL_OK&&strstr(result,"\\u0000"));
}
static void quota(void){assert(run_command("head -c 18000000 /dev/zero")==PG_TOOL_ERROR&&tool.captured==PG_BASH_CAPTURE_LIMIT&&tool.path[0]);struct stat st;assert(!stat(tool.path,&st)&&st.st_size==PG_BASH_CAPTURE_LIMIT);assert(memmem(result,n,"partial raw output",18));remove_artifact();}
static void layout_and_provider(int root){
 pg_bash_tool before=tool;pg_execution_env bad=execution_environment;bad.process.next=0;
 assert(pg_bash_tool_init(&tool,&bad)&&!memcmp(&before,&tool,sizeof(tool)));
 long page=sysconf(_SC_PAGESIZE);size_t pages=(sizeof(pg_bash_tool)+(size_t)page-1)/(size_t)page;size_t length=(size_t)page*(pages+2);
 char *map=mmap(0,length,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page,(size_t)page*pages,PROT_READ|PROT_WRITE));
 pg_bash_tool *tail=(pg_bash_tool*)(map+(pages+1)*page-sizeof(*tail));pg_process_init(&process);assert(!pg_bash_tool_init(tail,&execution_environment));pg_tool_definition d=pg_bash_tool_definition(tail);
 char *output=mmap(0,(size_t)page*2,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(output!=MAP_FAILED&&!mprotect(output,(size_t)page,PROT_READ|PROT_WRITE));char *end=output+page-4096;size_t size;
 assert(d.execute(tail,s("{\"command\":\"printf guard\"}"),end,4095,&size)==PG_TOOL_ERROR&&!size&&tail->stage==0);
 int rc=d.execute(tail,s("{\"command\":\"printf guard\"}"),end,4096,&size);uint64_t until=now()+2000;
 while(rc>=PG_TOOL_WAIT){assert(now()<until);usleep(1000);rc=d.async->next(tail,end,4096,&size);}assert(rc==PG_TOOL_OK&&size<4096&&memmem(end,size,"guard",5));while(d.async->close(tail)){assert(now()<until);usleep(1000);}assert(process.pid==-1&&!munmap(output,(size_t)page*2)&&!munmap(map,length));
 setup(root);
}
static pg_agent loop_agent;
static int answering;
static int loop_start(void *ctx,const pg_session *session){(void)ctx;answering=session->messages[session->count-1].role==PG_TOOL_RESULT;if(answering){const pg_message *m=&session->messages[session->count-1];assert(!m->is_error&&m->size<4096&&memmem(m->text,m->size,"full raw output",15));assert(process.pid==-1&&tool.file==-1&&!registry.active);}return 0;}
static int loop_next(void *ctx,pg_model_event *event){(void)ctx;*event=(pg_model_event){0};if(answering)event->kind=PG_MODEL_DONE;else{event->kind=PG_MODEL_CALL;event->text=s("{\"command\":\"printf '%060000d' 0\"}");event->call_id=s("bash-proof");event->tool_name=s("bash");}return 0;}
static void loop_noop(void *ctx){(void)ctx;}
static void long_loop(int root){for(unsigned attempt=0;attempt<5;++attempt){setup(root);assert(!pg_agent_init(&loop_agent,(pg_model){0,loop_start,loop_next,loop_noop,loop_noop},svc));pg_client client=pg_agent_client(&loop_agent);assert(!client.submit(client.context,s("unit command request")));uint64_t until=now()+3000;size_t streamed=0;int ended=0;while(!ended){assert(now()<until);pg_event event;if(!client.poll(client.context,&event)){usleep(1000);continue;}if(event.kind==PG_TOOL_OUTPUT_DELTA)streamed+=event.text.size;if(event.kind==PG_RUN_FINISHED){assert(event.outcome==PG_SUCCESS);ended=1;}}assert(streamed==60000&&tool.tail_size==PG_BASH_RETAIN&&tool.path[0]);struct stat st;assert(!stat(tool.path,&st)&&st.st_size==60000);remove_artifact();}}
int main(void){char path[]="/tmp/pangu-bash-test-XXXXXX";assert(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(root>=0);setup(root);size_t descriptors=fd_count();arguments();long_output();raw_and_tail();execution_prepare(root);setup(root);cancellation();failures();quota();layout_and_provider(root);long_loop(root);assert(fd_count()==descriptors&&!close(root)&&!rmdir(path));puts("bash tool: injected ExecutionEnv preparation, command/status/UTF-8, timeout/cancel, tail/spill/quota, parser/IO faults and page boundaries OK");return 0;}
