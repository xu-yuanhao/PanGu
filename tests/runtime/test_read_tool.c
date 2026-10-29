#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/read_tool.h"
#include "../../src/runtime/read_tool_layout.h"
#include "../../src/runtime/tool_path.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <json-c/json.h>
static pg_slice s(const char *v){return (pg_slice){v,strlen(v)};}
static const char *mutation_path;
static unsigned mutation_mode,stat_calls;
int __real_fstat(int,struct stat *);
int __wrap_fstat(int fd,struct stat *st){
 if(mutation_mode&&++stat_calls>=2&&(stat_calls==2||mutation_mode==2)){
  FILE *f=fopen(mutation_path,mutation_mode==3?"ab":"wb");assert(f&&fwrite("after!",1,6,f)==6&&!fclose(f));
  struct timespec times[2]={{100+(time_t)stat_calls,0},{100+(time_t)stat_calls,0}};assert(!utimensat(AT_FDCWD,mutation_path,times,0));
 }
 return __real_fstat(fd,st);
}
static void changing_files(pg_tools tools,const char *path){
 for(unsigned mode=1;mode<=3;mode++){FILE *f=fopen(path,"wb");assert(f&&fwrite("before",1,6,f)==6&&!fclose(f));mutation_path=path;mutation_mode=mode;stat_calls=0;char out[128];size_t n=0;int rc=tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n);mutation_mode=0;if(mode==2)assert(rc==PG_TOOL_ERROR&&strstr(out,"changed"));else assert(!rc&&n==6&&!memcmp(out,mode==1?"after!":"before",6));assert(stat_calls==(mode==3?2:3));}
}
static unsigned next_offset(pg_read_tool *reader){struct json_object *result=0;assert(!pg_read_tool_snapshot(reader,&result));unsigned next=0;if(result){const char *hint=strstr(json_object_to_json_string(result),"offset=");if(hint)assert(sscanf(hint,"offset=%u",&next)==1);json_object_put(result);}return next;}
static void slices(pg_tools tools,const char *path){
 FILE *f=fopen(path,"wb");assert(f&&fwrite("a\nb\nc\nd",1,7,f)==7&&!fclose(f));
 const char *args[]={"\"offset\":0","\"offset\":-4","\"offset\":2.5","\"limit\":0","\"limit\":-1","\"limit\":-1e20","\"limit\":1.5","\"offset\":2.5,\"limit\":1.5","\"offset\":3,\"limit\":-1","\"limit\":2147483648"};
 const char *expected[]={"a\nb\nc\nd","a\nb\nc\nd","b\nc\nd","","a\nb\nc","","a","b\nc","","a\nb\nc\nd"};
 for(size_t i=0;i<sizeof(args)/sizeof(*args);i++){char json[256],out[256];size_t n=0;snprintf(json,sizeof(json),"{\"path\":\"sample.c\",%s}",args[i]);int rc=tools.execute(tools.context,s("read"),s(json),out,sizeof(out),&n);assert(rc==PG_TOOL_OK||rc==PG_TOOL_PARTIAL);assert(n==strlen(expected[i])&&!memcmp(out,expected[i],n));}
}
static void pages(pg_tools tools,pg_read_tool *reader,const char *path){
 FILE *f=fopen(path,"wb");assert(f);char original[90000];size_t total=0;for(unsigned i=1;i<=1200;++i){int n=snprintf(original+total,sizeof(original)-total,"line-%04u: abcdefghijklmnopqrstuvwxyz0123456789 中文🙂\n",i);assert(n>0);total+=(size_t)n;}assert(total>51200&&fwrite(original,1,total,f)==total&&!fclose(f));
 char out[PG_RTL_OUTPUT_MAX],args[256],assembled[90000];size_t n,used=0;unsigned offset=1,rounds=0;
 for(;;){snprintf(args,sizeof(args),"{\"path\":\"sample.c\",\"offset\":%u}",offset);int rc=tools.execute(tools.context,s("read"),s(args),out,sizeof(out),&n);assert(rc==PG_TOOL_OK||rc==PG_TOOL_PARTIAL);assert(n<sizeof(out)&&out[n]==0);unsigned next=next_offset(reader);assert(used+n<=sizeof(assembled));memcpy(assembled+used,out,n);used+=n;if(!next)break;assert(next>offset&&rc==PG_TOOL_PARTIAL&&used<sizeof(assembled));assembled[used++]='\n';offset=next;assert(++rounds<10);}
 assert(rounds>=1&&used==total&&!memcmp(assembled,original,total));
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *map=mmap(0,(size_t)page*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page*2,(size_t)page,PROT_NONE));for(size_t cap=96;cap<=4096;cap+=250){char *tail=map+page*2-cap;tail[-1]='G';int rc=tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":2,\"limit\":100}"),tail,cap,&n);assert((rc==PG_TOOL_PARTIAL||rc==PG_TOOL_ERROR)&&n<cap&&tail[-1]=='G');}assert(!munmap(map,(size_t)page*3));

 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":1201}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&n==0);
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":1202}"),out,sizeof(out),&n)==PG_TOOL_ERROR&&strstr(out,"beyond end"));
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":2,\"limit\":1}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&strstr(out,"line-0002")&&!strstr(out,"line-0003")&&next_offset(reader)==3);
 const char *bad[]={"{\"path\":\"sample.c\",\"offset\":true}","{\"path\":\"sample.c\",\"offset\":\"2\"}","{\"path\":\"sample.c\",\"limit\":null}","{\"path\":\"sample.c\",\"limit\":2,\"extra\":1}"};for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(tools.execute(tools.context,s("read"),s(bad[i]),out,sizeof(out),&n)==PG_TOOL_ERROR);
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<2500;++i)assert(fputc('\n',f)!=EOF);assert(!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&next_offset(reader)==2001);
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<4094;++i)assert(fputc('a',f)!=EOF);assert(fputs("\n🙂\n",f)>=0&&!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":2}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&n==5&&!memcmp(out,"🙂\n",5));
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<49000;++i)assert(fputc('a',f)!=EOF);assert(fputc('\n',f)!=EOF);for(unsigned i=0;i<3000;++i)assert(fputc('b',f)!=EOF);assert(fputc('\n',f)!=EOF&&!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&n==49000&&next_offset(reader)==2);
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\\u0000suffix\":2}"),out,sizeof(out),&n)==PG_TOOL_ERROR);
 f=fopen(path,"wb");assert(f);char lines[65536];for(size_t i=0;i<sizeof(lines);i+=2){lines[i]='a';lines[i+1]='\n';}for(unsigned i=0;i<20;++i)assert(fwrite(lines,1,sizeof(lines),f)==sizeof(lines));assert(!fclose(f));assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\",\"offset\":550000,\"limit\":1}"),out,sizeof(out),&n)==PG_TOOL_PARTIAL&&n==1&&out[0]=='a'&&next_offset(reader)==550001);
}
static void prepared_paths(void){
 const char *cases[][2]={{"a//b/./c/../d/","a/b/d"},{"missing/../file","file"},{"link/../file","file"},{"../../a/../x","../../x"},{"/a/../../b","/b"},{"a/..","."},{"/..","/"},{"@./中文","中文"},{"a\xc2\xa0" "b","a b"},{"x\xe2\x80\xaf" "y","x y"},{"file:///tmp/a%20b/../c","/tmp/c"},{"file://localhost/tmp/%E4%B8%AD","/tmp/中"},{"file://remote/tmp/x","file:/remote/tmp/x"},{"file:///tmp/a%2Fb","file:/tmp/a%2Fb"},{"file:///tmp/%FF","file:/tmp/%FF"},{"file:///tmp/a%zz","file:/tmp/a%zz"},{"file://%6cocalhost/tmp/a","/tmp/a"},{"file://localhost","/"},{"file://%00/tmp/a","file:/%00/tmp/a"},{"file:///tmp/a?x=%2f","/tmp/a"}};
 char prepared[4096];for(size_t i=0;i<sizeof(cases)/sizeof(*cases);i++){assert(!pg_tool_path_prepare(s(cases[i][0]),prepared,sizeof(prepared)));if(strcmp(prepared,cases[i][1]))fprintf(stderr,"path %s -> %s, wanted %s\n",cases[i][0],prepared,cases[i][1]);assert(!strcmp(prepared,cases[i][1]));}
 assert(pg_tool_path_prepare(s("file:///tmp/%00"),prepared,sizeof(prepared))&&!prepared[0]);
 char *old=getenv("HOME"),*copy=old?strdup(old):0;assert(!old||copy);assert(!setenv("HOME","/tmp/pangu-path-home",1));assert(!pg_tool_path_prepare(s("~/missing/../file"),prepared,sizeof(prepared))&&!strcmp(prepared,"/tmp/pangu-path-home/file"));assert(!pg_tool_path_prepare(s("~"),prepared,sizeof(prepared))&&!strcmp(prepared,"/tmp/pangu-path-home"));assert(!(copy?setenv("HOME",copy,1):unsetenv("HOME")));free(copy);
 memset(prepared,'x',sizeof(prepared));assert(pg_tool_path_prepare(s("a/b"),prepared,2)&&!prepared[0]);char nul[]={'a',0,'b'};assert(pg_tool_path_prepare((pg_slice){nul,sizeof(nul)},prepared,sizeof(prepared))&&!prepared[0]);
}
int main(void){prepared_paths();char root[]="/tmp/pangu-read-XXXXXX",path[512];assert(mkdtemp(root));
 snprintf(path,sizeof(path),"%s/sample.c",root);FILE *f=fopen(path,"w");assert(f);fputs("int answer = 42;\n",f);fclose(f);
 pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,root));pg_tool_definition d=pg_read_tool_definition(&reader);pg_tool_registry r={0};assert(!pg_tool_registry_init(&r,&d,1));pg_tools tools=pg_tool_registry_service(&r);char out[65536];size_t n;
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));assert(n==17&&!memcmp(out,"int answer = 42;\n",17));
 f=fopen(path,"wb");assert(f);const unsigned char bom_text[]={0xef,0xbb,0xbf,'b','o','m','\n'};assert(fwrite(bom_text,1,sizeof(bom_text),f)==sizeof(bom_text)&&!fclose(f));
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)&&n==4&&!memcmp(out,"bom\n",4));
 pg_tool_definition snapshot=pg_read_snapshot_definition(&reader);assert(!snapshot.execute(snapshot.context,s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)&&n==sizeof(bom_text)&&!memcmp(out,bom_text,sizeof(bom_text)));
 f=fopen(path,"wb");assert(f);fputs("int answer = 42;\n",f);assert(!fclose(f));
 assert(d.validate(d.context,s("{\"path\":\"literal\\\\u0000\"}")));assert(!d.validate(d.context,s("{\"path\\u0000suffix\":\"sample.c\"}")));
 const char *bad[]={"{\"path\":\"sample.c\",\"extra\":1}","{\"path\":4}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i)assert(tools.execute(tools.context,s("read"),s(bad[i]),out,sizeof(out),&n));
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"missing-prefix/../sample.c\"}"),out,sizeof(out),&n)&&n==17);char resolved_file[1024];snprintf(resolved_file,sizeof(resolved_file),"{\"path\":\"file://%s/sample.c\"}",root);assert(!tools.execute(tools.context,s("read"),s(resolved_file),out,sizeof(out),&n)&&n==17);
 pages(tools,&reader,path);slices(tools,path);changing_files(tools,path);
 snprintf(path,sizeof(path),"%s/sample.c",root);f=fopen(path,"w");assert(f);fputs("int answer = 42;\n",f);assert(!fclose(f));
 snprintf(path,sizeof(path),"%s/link",root);assert(!symlink("sample.c",path));assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"link\"}"),out,sizeof(out),&n));unlink(path);
 assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,8,&n));
 snprintf(path,sizeof(path),"%s/sub",root);assert(!mkdir(path,0700));
 snprintf(path,sizeof(path),"%s/sub/nested",root);f=fopen(path,"w");assert(f);fputs("hello",f);fclose(f);
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sub/nested\"}"),out,sizeof(out),&n));assert(n==5);
 assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sub/../sample.c\"}"),out,sizeof(out),&n)&&n==17);char absolute[1024];snprintf(absolute,sizeof(absolute),"{\"path\":\"%s/./sample.c\"}",root);assert(!tools.execute(tools.context,s("read"),s(absolute),out,sizeof(out),&n)&&n==17);
 snprintf(path,sizeof(path),"%s/alias",root);assert(!symlink("sub",path));assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"alias/nested\"}"),out,sizeof(out),&n));unlink(path);
 snprintf(path,sizeof(path),"%s/sub/nested",root);unlink(path);snprintf(path,sizeof(path),"%s/sub",root);rmdir(path);
 snprintf(path,sizeof(path),"%s/sample.c",root);f=fopen(path,"wb");assert(f);fputc(0,f);fclose(f);assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)&&n==1&&out[0]==0&&out[1]==0);
 f=fopen(path,"wb");assert(f);fputc(0xff,f);fclose(f);assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)&&n==3&&!memcmp(out,"\xef\xbf\xbd",3));
 f=fopen(path,"wb");assert(f);const unsigned char malformed[]={0xe2,0x82,'A',0xe0,0x80,0x80,0xed,0xa0,0x80,0xf4,0x90,0x80,0x80};assert(fwrite(malformed,1,sizeof(malformed),f)==sizeof(malformed)&&!fclose(f));const unsigned char replacement_expected[]={0xef,0xbf,0xbd,'A',0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd,0xef,0xbf,0xbd};assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)&&n==sizeof(replacement_expected)&&!memcmp(out,replacement_expected,n));
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<4095;++i)assert(fputc('a',f)!=EOF);const unsigned char split_utf8[]={0xc3,0xa9};assert(fwrite(split_utf8,1,sizeof(split_utf8),f)==sizeof(split_utf8)&&!fclose(f));assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)&&n==4097&&(unsigned char)out[4095]==0xc3&&(unsigned char)out[4096]==0xa9);
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<4095;++i)assert(fputc('a',f)!=EOF);assert(fputc(0xe2,f)!=EOF&&fputc('A',f)!=EOF&&!fclose(f));assert(!tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n)&&n==4099&&!memcmp(out+4095,"\xef\xbf\xbd" "A",4));
 pg_read_tool_close(&reader);assert(tools.execute(tools.context,s("read"),s("{\"path\":\"sample.c\"}"),out,sizeof(out),&n));pg_read_tool_close(&reader);
 snprintf(path,sizeof(path),"%s/sample.c",root);unlink(path);rmdir(root);
 puts("PASS: workspace read, schema checks, POSIX absolute/dot/symlink paths, output quota and closed root");return 0;}
