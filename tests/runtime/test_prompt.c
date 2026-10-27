#define _GNU_SOURCE
#include "../../src/runtime/prompt.h"
#include "../../src/runtime/read_tool.h"
#include <assert.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
static unsigned calls,fail_at,executed;
static int mode,denied;
static const char *expected="rules\".txt";
static pg_slice s(const char *p){return (pg_slice){p,strlen(p)};}
json_object *__real_json_object_new_object(void);
json_object *__real_json_object_new_string_len(const char *,int);
int __real_json_object_object_add(json_object *,const char *,json_object *);
const char *__real_json_object_to_json_string_ext(json_object *,int);
json_object *__wrap_json_object_new_object(void){return ++calls==fail_at?0:__real_json_object_new_object();}
json_object *__wrap_json_object_new_string_len(const char *p,int n){return ++calls==fail_at?0:__real_json_object_new_string_len(p,n);}
int __wrap_json_object_object_add(json_object *o,const char *k,json_object *v){return ++calls==fail_at?-1:__real_json_object_object_add(o,k,v);}
const char *__wrap_json_object_to_json_string_ext(json_object *o,int f){return ++calls==fail_at?0:__real_json_object_to_json_string_ext(o,f);}
static int allow(void *ctx,pg_slice name,pg_slice args){(void)ctx;assert(name.size==4&&!memcmp(name.data,"read",4));json_object *o=json_tokener_parse(args.data),*v=0;assert(o&&json_object_object_get_ex(o,"path",&v)&&!strcmp(json_object_get_string(v),expected));json_object_put(o);return !denied;}
static int execute(void *ctx,pg_slice name,pg_slice args,char *out,size_t cap,size_t *n){(void)ctx;(void)name;(void)args;++executed;*n=0;if(mode==4){*n=cap;return PG_TOOL_OK;}if(mode==5){*n=4096;return PG_TOOL_OK;}if(cap<4)return PG_TOOL_ERROR;memcpy(out,"abc",3);*n=3;return mode==1?PG_TOOL_PARTIAL:mode==2?PG_TOOL_WAIT:mode==3?PG_TOOL_ERROR:PG_TOOL_OK;}
_Static_assert(sizeof(pg_prompt_workspace_source)==88&&offsetof(pg_prompt_workspace_source,tools)==24&&offsetof(pg_prompt_workspace_source,workspace)==32,"prompt source layout");
static unsigned resource_mode;
static int resource_probe(void *context,const char *path){(void)context;if(resource_mode==2)return -1;if(resource_mode==1&&(!strcmp(path,".pangu/SYSTEM.md")||!strcmp(path,".pangu/APPEND_SYSTEM.md")))return 1;return !strcmp(path,"AGENTS.override.md")||!strcmp(path,"AGENTS.md");}
static int resource_allow(void *context,pg_slice name,pg_slice args){(void)context;(void)name;(void)args;return 1;}
static int resource_execute(void *context,pg_slice name,pg_slice args,char *out,size_t cap,size_t *size){(void)context;(void)name;const char *text="\xef\xbb\xbf" "Always use project conventions.";if(strstr(args.data,"APPEND_SYSTEM.md"))text="Append resource.";else if(strstr(args.data,"SYSTEM.md"))text="Custom PanGu prefix.";else assert(strstr(args.data,"AGENTS.override.md"));*size=strlen(text);assert(*size<cap);memcpy(out,text,*size);return PG_TOOL_OK;}
static unsigned user_modes;
static int user_probe(void *context,const char *path){(void)context;if(user_modes&8)return -1;if(!strcmp(path,"SYSTEM.md"))return !!(user_modes&1);if(!strcmp(path,"APPEND_SYSTEM.md"))return !!(user_modes&2);return (user_modes&4)&&!strcmp(path,"AGENTS.md");}
static int user_execute(void *context,pg_slice name,pg_slice args,char *out,size_t capacity,size_t *size){(void)context;(void)name;const char *text=strstr(args.data,"APPEND_SYSTEM.md")?"User append.":strstr(args.data,"SYSTEM.md")?"User prefix.":"User context.";*size=strlen(text);assert(*size<capacity);memcpy(out,text,*size);return PG_TOOL_OK;}
static void workspace_prompt(void){
 pg_tools source={.authorize=resource_allow,.execute=resource_execute};pg_tool_registry tools={0};tools.count=2;tools.entries[0].name="read";tools.entries[1].name="edit";
 pg_prompt_workspace_source input={&source,0,resource_probe,&tools,{"/workspace",10},0,0,0,{0,0}};char output[4096];size_t size=0;
 assert(!pg_prompt_workspace(&input,output,sizeof(output),&size)&&size==strlen(output));
 assert(strstr(output,"inside PanGu")&&strstr(output,"- read: Read file contents")&&strstr(output,"- edit: Make precise file edits"));assert(!strstr(output,"- bash:")&&!strstr(output,"Use bash for"));
 assert(strstr(output,"<project_instructions path=\"AGENTS.override.md\">\nAlways use project conventions."));assert(strstr(output,"<cwd>\n/workspace\n</cwd>"));assert(!strstr(output,"\xef\xbb\xbf"));
 assert(pg_prompt_workspace(&input,output,20,&size)&&!size&&!output[0]);
 resource_mode=1;assert(!pg_prompt_workspace(&input,output,sizeof(output),&size));assert(!strncmp(output,"Custom PanGu prefix.",20)&&!strstr(output,"<tools>")&&strstr(output,"<addendum>\nAppend resource.\n</addendum>"));assert(strstr(output,"AGENTS.override.md"));
 resource_mode=2;assert(pg_prompt_workspace(&input,output,sizeof(output),&size)&&!size&&!output[0]);resource_mode=0;
 pg_tools user={.authorize=resource_allow,.execute=user_execute};input.user_files=&user;input.user_probe=user_probe;input.user_directory=(pg_slice){"/user/.pangu/agent",18};user_modes=7;
 assert(!pg_prompt_workspace(&input,output,sizeof(output),&size)&&!strncmp(output,"User prefix.",12)&&strstr(output,"User append.")&&strstr(output,"User context.")<strstr(output,"Always use project conventions."));
 resource_mode=1;assert(!pg_prompt_workspace(&input,output,sizeof(output),&size)&&!strncmp(output,"Custom PanGu prefix.",20)&&!strstr(output,"User prefix.")&&!strstr(output,"User append.")&&strstr(output,"User context."));resource_mode=0;
 user_modes=4;assert(!pg_prompt_workspace(&input,output,sizeof(output),&size)&&strstr(output,"inside PanGu")&&strstr(output,"User context.")&&!strstr(output,"User append."));user_modes=8;assert(pg_prompt_workspace(&input,output,sizeof(output),&size)&&!size&&!output[0]);
 input.user_files=0;input.user_probe=0;input.user_directory=(pg_slice){0};user_modes=0;
}
int main(void){workspace_prompt();pg_tools files={0,allow,execute,0,0,0,0};char out[5000];size_t n;
 assert(!pg_prompt_from_file(&files,s(expected),out,sizeof(out),&n)&&n==3&&!memcmp(out,"abc",4));
 for(unsigned i=1;i<=4;++i){calls=0;fail_at=i;unsigned before=executed;assert(pg_prompt_from_file(&files,s(expected),out,sizeof(out),&n)&&!n&&!out[0]&&before==executed);}fail_at=0;
 denied=1;unsigned before=executed;assert(pg_prompt_from_file(&files,s(expected),out,sizeof(out),&n)&&!n&&!out[0]&&before==executed);denied=0;
 for(mode=1;mode<=5;++mode)assert(pg_prompt_from_file(&files,s(expected),out,sizeof(out),&n)&&!n&&!out[0]);mode=0;
 assert(pg_prompt_from_file(0,s(expected),out,sizeof(out),&n)&&!n);assert(pg_prompt_from_file(&files,(pg_slice){0,1},out,sizeof(out),&n)&&!n);assert(pg_prompt_from_file(&files,(pg_slice){"x\0y",3},out,sizeof(out),&n)&&!n);assert(pg_prompt_from_file(&files,s(""),out,sizeof(out),&n)&&!n);assert(pg_prompt_from_file(&files,s(expected),out,0,&n)&&!n);assert(pg_prompt_from_file(&files,s(expected),out,sizeof(out),0));
 char large_path[4096];memset(large_path,'"',sizeof(large_path));before=executed;assert(pg_prompt_from_file(&files,(pg_slice){large_path,4095},out,sizeof(out),&n)&&!n&&before==executed);assert(pg_prompt_from_file(&files,(pg_slice){large_path,4096},out,sizeof(out),&n)&&!n);
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *map=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page,(size_t)page,PROT_NONE));assert(!pg_prompt_from_file(&files,s(expected),map+page-4,4,&n)&&n==3&&!memcmp(map+page-4,"abc",4));assert(!munmap(map,(size_t)page*2));
 char root[]="/tmp/pangu-prompt-XXXXXX";assert(mkdtemp(root));pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,root));pg_tool_definition d=pg_read_snapshot_definition(&reader);pg_tool_registry registry;assert(!pg_tool_registry_init(&registry,&d,1));files=pg_tool_registry_service(&registry);
 int fd=openat(reader.directory,"rules.txt",O_CREAT|O_WRONLY|O_TRUNC,0600);assert(fd>=0&&write(fd,"Always answer 中文.\n",22)==22&&!close(fd));assert(!pg_prompt_from_file(&files,s("rules.txt"),out,sizeof(out),&n)&&n==22);
 assert(pg_prompt_from_file(&files,s("missing"),out,sizeof(out),&n)&&!n&&!out[0]);assert(pg_prompt_from_file(&files,s("../rules.txt"),out,sizeof(out),&n)&&!n);assert(!symlinkat("rules.txt",reader.directory,"link"));assert(pg_prompt_from_file(&files,s("link"),out,sizeof(out),&n)&&!n);assert(!unlinkat(reader.directory,"link",0));
 fd=openat(reader.directory,"rules.txt",O_WRONLY|O_TRUNC);assert(fd>=0&&!close(fd));assert(!pg_prompt_from_file(&files,s("rules.txt"),out,sizeof(out),&n)&&!n&&!out[0]);
 fd=openat(reader.directory,"rules.txt",O_WRONLY|O_TRUNC);assert(fd>=0);memset(out,'x',4096);assert(write(fd,out,4095)==4095&&!close(fd));assert(!pg_prompt_from_file(&files,s("rules.txt"),out,sizeof(out),&n)&&n==4095);
 fd=openat(reader.directory,"rules.txt",O_WRONLY|O_APPEND);assert(fd>=0&&write(fd,"x",1)==1&&!close(fd));assert(pg_prompt_from_file(&files,s("rules.txt"),out,sizeof(out),&n)&&!n&&!out[0]);
 assert(!unlinkat(reader.directory,"rules.txt",0));pg_read_tool_close(&reader);assert(!rmdir(root));puts("PASS: injected prompt source, escaped path, four allocation failures, denial/partial/protocol rejection and real complete-file loading");return 0;}
