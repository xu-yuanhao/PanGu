#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/read_tool.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static pg_slice s(const char *v){return (pg_slice){v,strlen(v)};}
int main(void){char root[]="/tmp/pangu-read-XXXXXX",path[512];assert(mkdtemp(root));
 snprintf(path,sizeof(path),"%s/sample.c",root);FILE *f=fopen(path,"w");assert(f);fputs("int answer = 42;\n",f);fclose(f);
 pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,root));pg_tool_definition d=pg_read_tool_definition(&reader);pg_tool_registry r={0};assert(!pg_tool_registry_init(&r,&d,1));pg_tools tools=pg_tool_registry_service(&r);char out[128];size_t n;
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));assert(n==17&&!memcmp(out,"int answer = 42;\n",17));
 const char *bad[]={"{\"path\":\"../etc/passwd\"}","{\"path\":\"/etc/passwd\"}","{\"path\":\"./sample.c\"}","{\"path\":\"sample.c/\"}","{\"path\":\"sample.c\",\"extra\":1}","{\"path\":4}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i)assert(tools.execute(tools.context,s("read"),s(bad[i]),out,sizeof(out),&n));
 snprintf(path,sizeof(path),"%s/link",root);assert(!symlink("sample.c",path));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"link\"}"),out,sizeof(out),&n));unlink(path);
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,8,&n));
 snprintf(path,sizeof(path),"%s/sub",root);assert(!mkdir(path,0700));
 snprintf(path,sizeof(path),"%s/sub/nested",root);f=fopen(path,"w");assert(f);fputs("hello",f);fclose(f);
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sub/nested\"}"),out,sizeof(out),&n));assert(n==5);
 snprintf(path,sizeof(path),"%s/alias",root);assert(!symlink("sub",path));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"alias/nested\"}"),out,sizeof(out),&n));unlink(path);
 snprintf(path,sizeof(path),"%s/sub/nested",root);unlink(path);snprintf(path,sizeof(path),"%s/sub",root);rmdir(path);
 snprintf(path,sizeof(path),"%s/sample.c",root);f=fopen(path,"wb");assert(f);fputc(0,f);fclose(f);assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));
 f=fopen(path,"wb");assert(f);fputc(0xff,f);fclose(f);assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));
 pg_read_tool_close(&reader);assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));pg_read_tool_close(&reader);
 snprintf(path,sizeof(path),"%s/sample.c",root);unlink(path);rmdir(root);
 puts("PASS: workspace read, schema checks, traversal/absolute/symlink refusal, output quota and closed root");return 0;}
