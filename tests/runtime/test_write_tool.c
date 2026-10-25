#define _GNU_SOURCE
#include "../../src/runtime/write_tool_layout.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_mode,truncate_fail,sync_fail;
static unsigned writes;
ssize_t __real_write(int,const void *,size_t);
ssize_t __wrap_write(int fd,const void *p,size_t n){
 ++writes;
 if(write_mode==1 && writes==1){errno=EINTR;return -1;}
 if(write_mode==2){errno=ENOSPC;return -1;}
 if(write_mode==3)return 0;
 if(write_mode==4 && writes>1){errno=ENOSPC;return -1;}
 if(n>2)n=2;
 return __real_write(fd,p,n);
}
int __real_ftruncate(int,off_t);
int __wrap_ftruncate(int fd,off_t n){if(truncate_fail){errno=EIO;return -1;}return __real_ftruncate(fd,n);}
int __real_fsync(int);
int __wrap_fsync(int fd){if(sync_fail){errno=EIO;return -1;}return __real_fsync(fd);}
static pg_tool_definition tool;
static pg_tools service;
static pg_slice slice(const char *s){return (pg_slice){s,strlen(s)};}
static int run(const char *args){char out[128]={0};size_t size=999;writes=0;int rc=service.execute(service.context,slice("write"),slice(args),out,sizeof(out),&size);assert(size>0&&size<sizeof(out));assert(rc==0||rc==1);return rc;}
static void expect_file(const char *path,const char *bytes,size_t n){char b[4096];int fd=open(path,O_RDONLY|O_NOFOLLOW);assert(fd>=0);ssize_t got=read(fd,b,sizeof(b));assert(got==(ssize_t)n&&!memcmp(b,bytes,n));assert(!close(fd));}
static unsigned descriptors(void){DIR *d=opendir("/proc/self/fd");assert(d);unsigned count=0;while(readdir(d))++count;assert(!closedir(d));return count;}
int main(void){
 char temporary[]="/tmp/pangu-write-XXXXXX";assert(mkdtemp(temporary));
 int previous=open(".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(previous>=0&&!chdir(temporary));
 int root=open(".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(root>=0);
 pg_write_tool context={root};tool=pg_write_tool_definition(&context);
 assert(!strcmp(tool.name,"write")&&!tool.replay_safe);
 pg_tool_registry registry;assert(!pg_tool_registry_init(&registry,&tool,1));service=pg_tool_registry_service(&registry);
 assert(!run("{\"path\":\"a/b/file\",\"content\":\"hello\\n世界\"}"));
 expect_file("a/b/file","hello\n世界",12);
 assert(!run("{\"path\":\"a/b/file\",\"content\":\"x\"}"));expect_file("a/b/file","x",1);
 assert(!run("{\"path\":\"a/b/file\",\"content\":\"\"}"));expect_file("a/b/file","",0);
 assert(!run("{\"path\":\"binary\",\"content\":\"a\\u0000b\"}"));expect_file("binary","a\0b",3);
 const char *bad[]={"", "{}", "[]", "{\"path\":\"f\"}","{\"path\":\"f\",\"content\":7}",
  "{\"path\":\"f\",\"content\":\"x\",\"extra\":0}","{\"path\":\"\",\"content\":\"x\"}",
  "{\"path\":\"/tmp/escape\",\"content\":\"x\"}","{\"path\":\"../escape\",\"content\":\"x\"}",
  "{\"path\":\"a/../escape\",\"content\":\"x\"}","{\"path\":\"./f\",\"content\":\"x\"}",
  "{\"path\":\"a//f\",\"content\":\"x\"}","{\"path\":\"a/\",\"content\":\"x\"}",
  "{\"path\":\"f\\u0000suffix\",\"content\":\"x\"}","{\"path\":\"f\",\"content\":\"x\"} {}",
  "{\"path\":\"f\",\"content\":\"\xff\"}","{\"path\":\"f\",\"content\":\"x\",}"};
 unsigned before=descriptors();
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){assert(!tool.validate(tool.context,slice(bad[i])));assert(run(bad[i]));}
 assert(access("f",F_OK)&&errno==ENOENT);
 assert(!symlink("a","link"));assert(run("{\"path\":\"link/new\",\"content\":\"x\"}"));assert(access("a/new",F_OK));
 assert(!symlink("binary","sym"));assert(run("{\"path\":\"sym\",\"content\":\"x\"}"));expect_file("binary","a\0b",3);
 assert(!link("binary","hard"));assert(run("{\"path\":\"hard\",\"content\":\"x\"}"));expect_file("binary","a\0b",3);
 assert(!mkfifo("fifo",0600));assert(run("{\"path\":\"fifo\",\"content\":\"x\"}"));
 assert(run("{\"path\":\"a\",\"content\":\"x\"}"));
 assert(!run("{\"path\":\"keep\",\"content\":\"original\"}"));
 truncate_fail=1;assert(run("{\"path\":\"keep\",\"content\":\"bad\"}"));truncate_fail=0;expect_file("keep","original",8);
 write_mode=1;assert(!run("{\"path\":\"keep\",\"content\":\"retried\"}"));expect_file("keep","retried",7);assert(writes>2);
 write_mode=2;assert(run("{\"path\":\"keep\",\"content\":\"bad\"}"));expect_file("keep","",0);
 write_mode=3;assert(run("{\"path\":\"keep\",\"content\":\"bad\"}"));expect_file("keep","",0);
 write_mode=4;assert(run("{\"path\":\"keep\",\"content\":\"partial\"}"));expect_file("keep","pa",2);
 write_mode=0;sync_fail=1;assert(run("{\"path\":\"keep\",\"content\":\"written\"}"));sync_fail=0;expect_file("keep","written",7);
 for(unsigned i=0;i<32;++i){assert(run("{\"path\":\"sym\",\"content\":\"x\"}"));assert(!run("{\"path\":\"keep\",\"content\":\"ok\"}"));}
 assert(descriptors()==before);
 char out[64];size_t n=99;assert(tool.execute(tool.context,slice("{\"path\":\"small\",\"content\":\"x\"}"),out,1,&n)&&!n);assert(access("small",F_OK));
 context.directory=-1;assert(!tool.authorize(tool.context,slice("{}")));assert(run("{\"path\":\"closed\",\"content\":\"x\"}"));assert(access("closed",F_OK));
 assert(!unlink("a/b/file")&&!rmdir("a/b")&&!rmdir("a"));
 const char *names[]={"binary","link","sym","hard","fifo","keep"};for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)assert(!unlink(names[i]));
 assert(!close(root)&&!fchdir(previous)&&!close(previous)&&!rmdir(temporary));
 puts("write tool: paths, overwrite, mkdir, Unicode/NUL, refusal, short writes, EINTR, partial errors and descriptor cleanup OK");return 0;
}
