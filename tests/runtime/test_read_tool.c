#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/read_tool.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
static pg_slice s(const char *v){return (pg_slice){v,strlen(v)};}
static void pages(pg_tools tools,const char *path){
 FILE *f=fopen(path,"wb");assert(f);char original[24000];size_t total=0;for(unsigned i=1;i<=300;++i){int n=snprintf(original+total,sizeof(original)-total,"line-%03u: abcdefghijklmnopqrstuvwxyz0123456789 中文🙂\n",i);assert(n>0);total+=(size_t)n;}assert(fwrite(original,1,total,f)==total&&!fclose(f));
 char out[4096],args[256],assembled[24000];size_t n,used=0;unsigned offset=1,rounds=0;
 for(;;){snprintf(args,sizeof(args),"{\"path\":\"sample.c\",\"offset\":%u}",offset);int rc=tools.execute(tools.context,s("read"),s(args),out,sizeof(out),&n);assert(rc==PG_TOOL_OK||rc==PG_TOOL_PARTIAL);assert(n<sizeof(out)&&out[n]==0);char *hint=strstr(out,"\n\n[read: more lines; use offset=");if(!hint){assert(used+n<=sizeof(assembled));memcpy(assembled+used,out,n);used+=n;break;}unsigned next=0;assert(sscanf(hint,"\n\n[read: more lines; use offset=%u",&next)==1&&next>offset&&rc==PG_TOOL_PARTIAL);size_t bytes=(size_t)(hint-out);assert(used+bytes+1<sizeof(assembled));memcpy(assembled+used,out,bytes);used+=bytes;assembled[used++]='\n';offset=next;assert(++rounds<10);}
 assert(rounds>=2&&used==total&&!memcmp(assembled,original,total));
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *map=mmap(0,(size_t)page*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page*2,(size_t)page,PROT_NONE));for(size_t cap=96;cap<=4096;cap+=250){char *tail=map+page*2-cap;tail[-1]='G';int rc=tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":2,\"limit\":100}"),tail,cap,&n);assert((rc==PG_TOOL_PARTIAL||rc==PG_TOOL_ERROR)&&n<cap&&tail[-1]=='G');}assert(!munmap(map,(size_t)page*3));

 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":301}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&n==0);
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":302}"),out,sizeof(out),&n)==PG_TOOL_ERROR&&strstr(out,"beyond end"));
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":2,\"limit\":1}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&strstr(out,"line-002")&&!strstr(out,"line-003")&&strstr(out,"offset=3"));
 const char *bad[]={"{\"path\":\"sample.c\",\"offset\":0}","{\"path\":\"sample.c\",\"limit\":-1}","{\"path\":\"sample.c\",\"limit\":1.5}","{\"path\":\"sample.c\",\"offset\":true}","{\"path\":\"sample.c\",\"offset\":\"2\"}","{\"path\":\"sample.c\",\"limit\":null}","{\"path\":\"sample.c\",\"limit\":2147483648}","{\"path\":\"sample.c\",\"limit\":2,\"extra\":1}"};for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(tools.execute(tools.context,s("read"),s(bad[i]),out,sizeof(out),&n)==PG_TOOL_ERROR);
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<2500;++i)assert(fputc('\n',f)!=EOF);assert(!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&strstr(out,"offset=2001"));
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<4094;++i)assert(fputc('a',f)!=EOF);assert(fputs("\n🙂\n",f)>=0&&!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":2}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&n==5&&!memcmp(out,"🙂\n",5));
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<4000;++i)assert(fputc('a',f)!=EOF);assert(fputc('\n',f)!=EOF);for(unsigned i=0;i<100;++i)assert(fputc('b',f)!=EOF);assert(fputc('\n',f)!=EOF&&!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&n>4000&&n<4096&&strstr(out,"offset=2"));
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\\u0000suffix\":2}"),out,sizeof(out),&n)==PG_TOOL_ERROR);
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<1048577;++i)assert(fputc('a',f)!=EOF);assert(!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":2}"),out,sizeof(out),&n)==PG_TOOL_ERROR&&strstr(out,"scan limit"));
}
int main(void){char root[]="/tmp/pangu-read-XXXXXX",path[512];assert(mkdtemp(root));
 snprintf(path,sizeof(path),"%s/sample.c",root);FILE *f=fopen(path,"w");assert(f);fputs("int answer = 42;\n",f);fclose(f);
 pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,root));pg_tool_definition d=pg_read_tool_definition(&reader);pg_tool_registry r={0};assert(!pg_tool_registry_init(&r,&d,1));pg_tools tools=pg_tool_registry_service(&r);char out[128];size_t n;
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));assert(n==17&&!memcmp(out,"int answer = 42;\n",17));
 assert(d.validate(d.context,s("{\"path\":\"literal\\\\u0000\"}")));assert(!d.validate(d.context,s("{\"path\\u0000suffix\":\"sample.c\"}")));
 const char *bad[]={"{\"path\":\"sample.c/\"}","{\"path\":\"sample.c\",\"extra\":1}","{\"path\":4}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i)assert(tools.execute(tools.context,s("read"),s(bad[i]),out,sizeof(out),&n));
 pages(tools,path);
 snprintf(path,sizeof(path),"%s/sample.c",root);f=fopen(path,"w");assert(f);fputs("int answer = 42;\n",f);assert(!fclose(f));
 snprintf(path,sizeof(path),"%s/link",root);assert(!symlink("sample.c",path));assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"link\"}"),out,sizeof(out),&n));unlink(path);
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,8,&n));
 snprintf(path,sizeof(path),"%s/sub",root);assert(!mkdir(path,0700));
 snprintf(path,sizeof(path),"%s/sub/nested",root);f=fopen(path,"w");assert(f);fputs("hello",f);fclose(f);
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sub/nested\"}"),out,sizeof(out),&n));assert(n==5);
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sub/../sample.c\"}"),out,sizeof(out),&n)&&n==17);char absolute[1024];snprintf(absolute,sizeof(absolute),"{\"path\":\"%s/./sample.c\"}",root);assert(!tools.execute(tools.context,s("read"),s(absolute),out,sizeof(out),&n)&&n==17);
 snprintf(path,sizeof(path),"%s/alias",root);assert(!symlink("sub",path));assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"alias/nested\"}"),out,sizeof(out),&n));unlink(path);
 snprintf(path,sizeof(path),"%s/sub/nested",root);unlink(path);snprintf(path,sizeof(path),"%s/sub",root);rmdir(path);
 snprintf(path,sizeof(path),"%s/sample.c",root);f=fopen(path,"wb");assert(f);fputc(0,f);fclose(f);assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));
 f=fopen(path,"wb");assert(f);fputc(0xff,f);fclose(f);assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));
 pg_read_tool_close(&reader);assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));pg_read_tool_close(&reader);
 snprintf(path,sizeof(path),"%s/sample.c",root);unlink(path);rmdir(root);
 puts("PASS: workspace read, schema checks, POSIX absolute/dot/symlink paths, output quota and closed root");return 0;}
