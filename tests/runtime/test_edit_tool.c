#define _GNU_SOURCE
#include "../../src/runtime/edit_tool.h"
#include "../../src/runtime/edit_text.h"
#include "../../src/runtime/edit_diff.h"
#include "../../src/runtime/read_tool.h"
#include "../../src/runtime/write_tool.h"
#include "../../src/runtime/journal.h"
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
_Static_assert(sizeof(pg_edit_tool)==64 && offsetof(pg_edit_tool,files)==0 && offsetof(pg_edit_tool,details)==56,"edit context ABI");
_Static_assert(json_type_object==4&&json_type_array==5&&json_type_string==6,"JSON types");
_Static_assert((JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8)==17,"JSON flags");
static pg_tool_registry registry;
static pg_tool_registry files;
static pg_tools file_service;
static pg_edit_tool editor;
static pg_tools service;
static pg_tool_definition definition;
static unsigned reads,writes,checks;
static int deny_read,deny_write;
static int partial_read,oversize_read,fail_after_write,crash_after_write;
static unsigned api_calls,fail_api;
static char last_output[128];
static int fault(void){return ++api_calls==fail_api;}
json_tokener *__real_json_tokener_new(void);
json_tokener *__wrap_json_tokener_new(void){return fault()?0:__real_json_tokener_new();}
json_object *__real_json_object_new_object(void);
json_object *__wrap_json_object_new_object(void){return fault()?0:__real_json_object_new_object();}
json_object *__real_json_object_new_string_len(const char *,int);
json_object *__wrap_json_object_new_string_len(const char *p,int n){return fault()?0:__real_json_object_new_string_len(p,n);}
int __real_json_object_object_add(json_object *,const char *,json_object *);
int __wrap_json_object_object_add(json_object *p,const char *key,json_object *v){return fault()?-1:__real_json_object_object_add(p,key,v);}
const char *__real_json_object_to_json_string_ext(json_object *,int);
const char *__wrap_json_object_to_json_string_ext(json_object *o,int flags){return fault()?0:__real_json_object_to_json_string_ext(o,flags);}
static pg_slice s(const char *p){return (pg_slice){p,strlen(p)};}
static int forward_allow(void *p,pg_slice name,pg_slice args){(void)p;++checks;if((name.size==4&&deny_read)||(name.size==5&&deny_write))return 0;return file_service.authorize(file_service.context,name,args);}
static int forward_execute(void *p,pg_slice name,pg_slice args,char *out,size_t cap,size_t *size){(void)p;if(name.size==4){++reads;if(oversize_read){*size=cap;return 0;}if(partial_read){memcpy(out,"alpha",5);*size=5;return PG_TOOL_PARTIAL;}}else{assert(name.size==5);++writes;}int rc=file_service.execute(file_service.context,name,args,out,cap,size);if(name.size==5&&!rc){if(crash_after_write)_exit(73);if(fail_after_write)return 1;}return rc;}
static int invoke(const char *args){char out[128]={0};size_t n=777;char *copy=strdup(args);assert(copy);int rc=service.execute(service.context,s("edit"),s(args),out,sizeof(out),&n);assert(!strcmp(args,copy));free(copy);if(!rc)assert(json_object_get_uint64(json_object_object_get(editor.details,"firstChangedLine"))>0);assert(n>0&&n<sizeof(out));memcpy(last_output,out,sizeof(out));return rc;}
static void put_bytes(const void *bytes,size_t size){int fd=open("file.txt",O_WRONLY|O_CREAT|O_TRUNC,0600);assert(fd>=0);const char *p=bytes;while(size){ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;assert(n>0);p+=n;size-=(size_t)n;}assert(!close(fd));}
static void put(const char *bytes){put_bytes(bytes,strlen(bytes));}
static void expect_bytes(const void *bytes,size_t size){int fd=open("file.txt",O_RDONLY);assert(fd>=0);char *content=malloc(size?size:1);assert(content);size_t used=0;while(used<size){ssize_t n=read(fd,content+used,size-used);if(n<0&&errno==EINTR)continue;assert(n>0);used+=(size_t)n;}char extra;assert(read(fd,&extra,1)==0&&!memcmp(content,bytes,size));free(content);assert(!close(fd));}
static void expect(const char *bytes){expect_bytes(bytes,strlen(bytes));}
static unsigned fds(void){DIR *dir=opendir("/proc/self/fd");assert(dir);unsigned n=0;while(readdir(dir))++n;assert(!closedir(dir));return n;}
static pg_agent agent;
static unsigned stage,answer;
static int model_start(void *ctx,const pg_session *session){(void)ctx;stage=0;answer=session->messages[session->count-1].role==PG_TOOL_RESULT;return 0;}
static int model_next(void *ctx,pg_model_event *e){(void)ctx;*e=(pg_model_event){0};if(!stage++){e->kind=PG_MODEL_TEXT;e->text=s(answer?"done":"editing");}else if(answer)e->kind=PG_MODEL_DONE;else{e->kind=PG_MODEL_CALL;e->call_id=s("edit-1");e->tool_name=s("edit");e->text=s("{\"path\":\"file.txt\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}");}return 0;}
static void model_close(void *ctx){(void)ctx;}
static void recovery(int root){
 put("alpha\n");pid_t child=fork();assert(child>=0);
 if(!child){pg_journal journal={.file=-1,.directory=-1};crash_after_write=1;assert(!pg_agent_init(&agent,(pg_model){0,model_start,model_next,model_close,model_close},service));assert(!pg_journal_open(&journal,root,"edit.pgj",&agent.session,&agent.commits));pg_client c=pg_agent_client(&agent);assert(!c.submit(c.context,s("edit")));for(unsigned i=0;i<40;++i){pg_event e;c.poll(c.context,&e);}_exit(99);}
 int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==73);expect("ALPHA\n");struct stat before,after;assert(!stat("file.txt",&before));
 pg_journal journal={.file=-1,.directory=-1};assert(!pg_agent_init(&agent,(pg_model){0,model_start,model_next,model_close,model_close},service));assert(!pg_journal_open(&journal,root,"edit.pgj",&agent.session,&agent.commits));assert(agent.commits.run.phase==PG_RUN_TOOL_UNSAFE);unsigned r=reads,w=writes;assert(!pg_agent_resume(&agent,0));pg_client c=pg_agent_client(&agent);unsigned finished=0;for(unsigned i=0;i<40;++i){pg_event e;if(c.poll(c.context,&e)&&e.kind==PG_RUN_FINISHED){assert(e.outcome==PG_SUCCESS);finished=1;break;}}assert(finished&&reads==r&&writes==w);assert(agent.session.messages[3].role==PG_TOOL_RESULT&&agent.session.messages[3].is_error&&strstr(agent.session.messages[3].text,"partially run"));expect("ALPHA\n");assert(!stat("file.txt",&after)&&before.st_mtim.tv_sec==after.st_mtim.tv_sec&&before.st_mtim.tv_nsec==after.st_mtim.tv_nsec);pg_journal_close(&journal);assert(!unlink("edit.pgj"));
}
static void json_cases(void){
 json_object *cases=json_object_from_fd(STDIN_FILENO),*answers=json_object_new_array();assert(cases&&answers&&json_object_is_type(cases,json_type_array));
 for(size_t i=0;i<json_object_array_length(cases);++i){json_object *item=json_object_array_get_idx(cases,i),*content=0,*args=0;assert(json_object_object_get_ex(item,"content",&content)&&json_object_is_type(content,json_type_string));assert(json_object_object_get_ex(item,"arguments",&args));put(json_object_get_string(content));int rc=invoke(json_object_to_json_string_ext(args,JSON_C_TO_STRING_PLAIN));char bytes[4096];int fd=open("file.txt",O_RDONLY);assert(fd>=0);ssize_t n=read(fd,bytes,sizeof(bytes));assert(n>=0&&n<4096&&!close(fd));json_object *answer=json_object_new_object();assert(answer);json_object_object_add(answer,"status",json_object_new_int(rc));json_object_object_add(answer,"output",json_object_new_string_len(bytes,(int)n));assert(!json_object_array_add(answers,answer));}
 puts(json_object_to_json_string_ext(answers,JSON_C_TO_STRING_PLAIN));json_object_put(answers);json_object_put(cases);
}
static void success_summaries(void){
 const struct {const char *before,*args;unsigned line,blocks;} cases[]={
  {"head\nalpha\n","{\"path\":\"file.txt\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}",2,1},
  {"\357\273\277head\r\nalpha\r\n","{\"path\":\"file.txt\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}",2,1},
  {"prefix\nalpha\nsuffix","{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"alpha\",\"newText\":\"A\"},{\"oldText\":\"prefix\",\"newText\":\"P\"}]}",1,2},
  {"prefix\nalpha\n","{\"path\":\"file.txt\",\"oldText\":\"alpha\\n\",\"newText\":\"\"}",2,1},
  {"first\n尾巴\n","{\"path\":\"file.txt\",\"oldText\":\"尾巴\",\"newText\":\"改好\"}",2,1},
  {"alpha\n","{\"path\":\"file.txt\",\"oldText\":\"alpha\\n\",\"newText\":\"alpha\\nx\"}",2,1},
  {"alpha\n","{\"path\":\"file.txt\",\"oldText\":\"alpha\\n\",\"newText\":\"alpha\"}",1,1},
 };
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i){put(cases[i].before);assert(!invoke(cases[i].args));char expected[128];snprintf(expected,sizeof(expected),"Successfully replaced %u block(s); first changed line %u",cases[i].blocks,cases[i].line);assert(!strcmp(last_output,expected));}
 char many[2048];memset(many,'\n',2001);memcpy(many+2001,"alpha",6);put(many);assert(!invoke("{\"path\":\"file.txt\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}"));assert(!strcmp(last_output,"Successfully replaced 1 block(s); first changed line 2002"));
 char content[128]={0};json_object *args=json_object_new_object(),*edits=json_object_new_array();assert(args&&edits);assert(!json_object_object_add(args,"path",json_object_new_string("file.txt")));assert(!json_object_object_add(args,"edits",edits));
 for(unsigned i=0;i<16;++i){char old[8],replacement[8];snprintf(old,sizeof(old),"T%02u",i);snprintf(replacement,sizeof(replacement),"R%02u",i);strcat(content,old);strcat(content,"\n");json_object *edit=json_object_new_object();assert(edit&&!json_object_object_add(edit,"oldText",json_object_new_string(old))&&!json_object_object_add(edit,"newText",json_object_new_string(replacement))&&!json_object_array_add(edits,edit));}
 put(content);assert(!invoke(json_object_to_json_string_ext(args,JSON_C_TO_STRING_PLAIN)));assert(!strcmp(last_output,"Successfully replaced 16 block(s); first changed line 1"));json_object_put(args);
 put("alpha\n");
}

static void diff_contract(void){
 struct json_object *value=0;
 assert(!pg_edit_diff_create(s("case.txt"),s("one\ntwo\n"),s("one\nTHREE\n"),&value));
 assert(!strcmp(json_object_get_string(json_object_object_get(value,"diff"))," 1 one\n-2 two\n+2 THREE"));
 assert(!strcmp(json_object_get_string(json_object_object_get(value,"patch")),"--- case.txt\n+++ case.txt\n@@ -1,2 +1,2 @@\n one\n-two\n+THREE\n"));
 assert(json_object_get_int(json_object_object_get(value,"firstChangedLine"))==2);json_object_put(value);
 assert(!pg_edit_diff_create(s("case.txt"),s("a"),s("b"),&value));
 assert(strstr(json_object_get_string(json_object_object_get(value,"patch")),"-a\n\\ No newline at end of file\n+b\n\\ No newline at end of file\n"));json_object_put(value);
 assert(!pg_edit_diff_create(s("case.txt"),s("same"),s("same"),&value));
 assert(!json_object_object_get(value,"firstChangedLine")&&!strcmp(json_object_get_string(json_object_object_get(value,"diff")),""));json_object_put(value);
}
static int diff_json(void){char *line=0;size_t capacity=0;
 while(getline(&line,&capacity,stdin)>0){struct json_object *input=json_tokener_parse(line),*before=json_object_object_get(input,"before"),*after=json_object_object_get(input,"after"),*value=0;assert(input&&before&&after);
  assert(!pg_edit_diff_create(s("case.txt"),(pg_slice){json_object_get_string(before),(size_t)json_object_get_string_len(before)},(pg_slice){json_object_get_string(after),(size_t)json_object_get_string_len(after)},&value));
  puts(json_object_to_json_string_ext(value,JSON_C_TO_STRING_PLAIN));json_object_put(value);json_object_put(input);
 }free(line);return 0;}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--diff-json"))return diff_json();
 diff_contract();
 char root[]="/tmp/pangu-edit-tool-XXXXXX";assert(mkdtemp(root));int saved=open(".",O_RDONLY|O_DIRECTORY);assert(saved>=0&&!chdir(root));
 pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,"."));pg_write_tool writer={reader.directory};
 pg_tool_definition file_entries[]={pg_read_snapshot_definition(&reader),pg_write_file_definition(&writer)};assert(!pg_tool_registry_init(&files,file_entries,2));file_service=pg_tool_registry_service(&files);
 service=pg_tool_registry_service(&registry);editor.files=(pg_tools){0,forward_allow,forward_execute,0,0,0,0};definition=pg_edit_tool_definition(&editor);assert(!definition.replay_safe&&!strcmp(definition.name,"edit"));
 pg_tool_definition entries[]={pg_read_tool_definition(&reader),pg_write_tool_definition(&writer),definition};assert(!pg_tool_registry_init(&registry,entries,3));
 if(argc==2&&!strcmp(argv[1],"--json")){json_cases();assert(!unlink("file.txt"));pg_edit_tool_clear(&editor);pg_read_tool_close(&reader);assert(!fchdir(saved)&&!close(saved)&&!rmdir(root));return 0;}
 success_summaries();
 const char *shapes[]={
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}]}",
  "{\"path\":\"file.txt\",\"edits\":{\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}}",
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}],\"oldText\":null,\"newText\":null}",
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}],\"oldText\":7,\"newText\":\"x\"}",
  "{\"path\":\"file.txt\",\"edits\":\"[{\\\"oldText\\\":\\\"alpha\\\",\\\"newText\\\":\\\"ALPHA\\\"}]\"}",
  "{\"path\":\"file.txt\",\"edits\":\"{\\\"oldText\\\":\\\"alpha\\\",\\\"newText\\\":\\\"ALPHA\\\"}\"}",
  "{\"path\":\"file.txt\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}",
  "{\"path\":\"file.txt\",\"edits\":\"invalid\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}",
  "{\"path\":\"file.txt\",\"edits\":7,\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}",
  "{\"path\":\"file.txt\",\"edits\":{\"junk\":1},\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}",
  "{\"path\":\"file.txt\",\"edits\":[],\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}"
 };
 for(size_t i=0;i<sizeof(shapes)/sizeof(*shapes);++i){put("alpha\nbeta\n");unsigned r=reads,w=writes,c=checks;assert(definition.validate(definition.context,s(shapes[i])));assert(!invoke(shapes[i]));assert(reads==r+1&&writes==w+1&&checks==c+2);expect("ALPHA\nbeta\n");}
 {char large[10000];size_t used=0;for(unsigned i=0;i<500;++i){int n=snprintf(large+used,sizeof(large)-used,"alpha-%u payload\n",i);assert(n>0);used+=(size_t)n;}put(large);unsigned w=writes;assert(invoke(shapes[0])&&writes==w);int fd=open("file.txt",O_RDONLY);assert(fd>=0);char bytes[10000];ssize_t n=read(fd,bytes,sizeof(bytes));assert(n==(ssize_t)used&&!memcmp(bytes,large,used)&&!close(fd));}
 {size_t prefix=196608,input_size=prefix+7;char *original=malloc(input_size),*expected=malloc(input_size+1);assert(original&&expected);memset(original,'a',prefix);memcpy(original+prefix,"target\n",7);memset(expected,'a',prefix);memcpy(expected+prefix,"changed\n",8);put_bytes(original,input_size);assert(!invoke("{\"path\":\"file.txt\",\"oldText\":\"target\",\"newText\":\"changed\"}"));expect_bytes(expected,prefix+8);free(expected);free(original);}
 put("alpha\n\n[read: more lines; use offset=2 to continue]\n");assert(!invoke(shapes[0]));expect("ALPHA\n\n[read: more lines; use offset=2 to continue]\n");
 {char original[2100],expected[2100];memcpy(original,"alpha",5);memset(original+5,'\n',2001);original[2006]=0;memcpy(expected,original,2007);memcpy(expected,"ALPHA",5);put(original);assert(!invoke(shapes[0]));expect(expected);}
 put("alpha\n");partial_read=1;{unsigned w=writes;assert(invoke(shapes[0])&&writes==w);expect("alpha\n");}partial_read=0;
 put("alpha\nbeta\ngamma\n");assert(!invoke("{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"gamma\",\"newText\":\"GAMMA\"}],\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}"));expect("ALPHA\nbeta\nGAMMA\n");
 put("\xef\xbb\xbfone\r\nsay ‘hello’ — world  \r\nkeep  \r\n");assert(!invoke("{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"say 'hello' - world\",\"newText\":\"changed\\nnext\"}]}"));expect("\xef\xbb\xbfone\r\nchanged\r\nnext\r\nkeep  \r\n");
 const char *bad[]={"", "{}", "[]", "null", "{\"path\":\"file.txt\",\"edits\":[]}",
  "{\"path\":\"file.txt\",\"edits\":\"garbage\"}","{\"path\":\"file.txt\",\"edits\":null}",
  "{\"path\":\"file.txt\",\"edits\":[{}]}","{\"path\":\"file.txt\",\"edits\":[null]}",
  "{\"path\":\"file.txt\",\"oldText\":null,\"newText\":null}",
  "{\"path\":\"file.txt\",\"edits\":{\"oldText\":\"\",\"newText\":\"x\"},\"oldText\":\"original\",\"newText\":\"wrong\"}",
  "{\"path\":\"file.txt\",\"edits\":{\"oldText\":\"original\",\"newText\":\"x\",\"extra\":1},\"oldText\":\"original\",\"newText\":\"wrong\"}",
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"a\",\"newText\":7}]}",
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"\",\"newText\":\"b\"}]}",
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"a\\u0000b\",\"newText\":\"c\"}]}",

  "{\"path\":\"file.txt\",\"oldText\":\"a\"}","{\"path\":\"file.txt\",\"oldText\":\"a\",\"newText\":\"b\",\"other\":1}",
  "{\"path\":\"file.txt\",\"oldText\":\"a\",\"newText\":\"b\"} {}"};
 put("original\n");unsigned before=fds();for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){unsigned r=reads,w=writes;assert(!definition.validate(definition.context,s(bad[i])));assert(invoke(bad[i]));assert(reads==r&&writes==w);expect("original\n");}
 const char *rejected[]={"{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"missing\",\"newText\":\"x\"}]}",
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"original\",\"newText\":\"original\"}]}",
  "{\"path\":\"file.txt\",\"edits\":[{\"oldText\":\"original\",\"newText\":\"x\"},{\"oldText\":\"ginal\",\"newText\":\"y\"}]}"};
 for(size_t i=0;i<sizeof(rejected)/sizeof(*rejected);++i){unsigned w=writes;assert(invoke(rejected[i]));assert(writes==w);expect("original\n");}
 put("alpha alpha\n");unsigned w=writes;assert(invoke(shapes[0])&&writes==w);expect("alpha alpha\n");
 put("alpha\n");deny_read=1;unsigned r=reads;w=writes;assert(invoke(shapes[0])&&reads==r&&writes==w);expect("alpha\n");deny_read=0;
 deny_write=1;r=reads;w=writes;assert(invoke(shapes[0])&&reads==r+1&&writes==w);expect("alpha\n");deny_write=0;
 oversize_read=1;w=writes;assert(invoke(shapes[0])&&writes==w);expect("alpha\n");oversize_read=0;
 fail_after_write=1;assert(invoke(shapes[0])&&strstr(last_output,"partially changed"));expect("ALPHA\n");fail_after_write=0;put("alpha\n");
	char escaped[4096],escaped_expected[4096];memset(escaped,'\t',3000);memcpy(escaped+3000,"alpha\n",7);memcpy(escaped_expected,escaped,sizeof(escaped));memcpy(escaped_expected+3000,"ALPHA\n",6);put(escaped);assert(!invoke(shapes[0]));expect(escaped_expected);put("alpha\n");
 assert(!rename("file.txt","quote\"name.txt"));assert(!invoke("{\"path\":\"quote\\\"name.txt\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}"));assert(!rename("quote\"name.txt","file.txt"));expect("ALPHA\n");put("alpha\n");
 assert(invoke("{\"path\":\"missing/file.txt\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}"));assert(access("missing",F_OK));
 assert(!rename("file.txt","@file"));assert(!invoke("{\"path\":\"@@file\",\"oldText\":\"alpha\",\"newText\":\"AT\"}"));assert(!rename("@file","file.txt"));expect("AT\n");put("alpha\n");
 assert(!rename("file.txt","space\xc2\xa0" "file"));char unicode_url[4096];snprintf(unicode_url,sizeof(unicode_url),"{\"path\":\"file://%s/space%%C2%%A0file\",\"oldText\":\"alpha\",\"newText\":\"URL\"}",root);assert(!invoke(unicode_url));assert(!rename("space\xc2\xa0" "file","file.txt"));expect("URL\n");put("alpha\n");
 char absolute_args[4096];snprintf(absolute_args,sizeof(absolute_args),"{\"path\":\"%s/file.txt\",\"oldText\":\"alpha\",\"newText\":\"ABS\"}",root);assert(!invoke(absolute_args));expect("ABS\n");put("alpha\n");assert(!invoke("{\"path\":\"./file.txt\",\"oldText\":\"alpha\",\"newText\":\"DOT\"}"));expect("DOT\n");put("alpha\n");
 assert(!symlink("file.txt","link"));assert(!invoke("{\"path\":\"link\",\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}"));expect("ALPHA\n");put("alpha\n");assert(!unlink("link"));
 api_calls=0;assert(!invoke(shapes[0]));unsigned points=api_calls;assert(points>10);for(unsigned failure=1;failure<=points;++failure){put("alpha\n");api_calls=0;fail_api=failure;assert(invoke(shapes[0]));expect("alpha\n");}fail_api=0;printf("edit allocation/setup failure points: %u\n",points);put("alpha\n");
 char out[128];size_t n=999;r=reads;w=writes;assert(definition.execute(definition.context,s(shapes[0]),out,1,&n)&&!n&&reads==r&&writes==w);
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *map=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page,(size_t)page,PROT_NONE));size_t len=strlen(shapes[0]);char *tail=map+page-len;memcpy(tail,shapes[0],len);assert(!definition.execute(definition.context,(pg_slice){tail,len},out,sizeof(out),&n));expect("ALPHA\n");put("alpha\n");r=reads;w=writes;n=999;assert(definition.execute(definition.context,s(shapes[0]),out,63,&n)&&!n&&reads==r&&writes==w);
 char *output_tail=map+page-64;assert(!definition.execute(definition.context,s(shapes[0]),output_tail,64,&n));assert(n==strlen("Successfully replaced 1 block(s); first changed line 1")&&!strcmp(output_tail,"Successfully replaced 1 block(s); first changed line 1"));
 assert(!munmap(map,(size_t)page*2));
 assert(fds()==before);recovery(reader.directory);assert(fds()==before);assert(!unlink("file.txt"));pg_edit_tool_clear(&editor);pg_read_tool_close(&reader);assert(!fchdir(saved)&&!close(saved)&&!rmdir(root));
 puts("edit tool: canonical/repair shapes, untouched args, batch/fuzzy/BOM, policy order, no-write refusals, borrowed service, page tails and unsafe post-write crash recovery OK");return 0;
}
