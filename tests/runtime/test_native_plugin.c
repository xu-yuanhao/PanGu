#define _GNU_SOURCE
#include "../../src/runtime/native_plugin.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(pg_native_manifest)==32,"manifest");
_Static_assert(offsetof(pg_native_plugin,library)==sizeof(pg_host),"library");
_Static_assert(sizeof(pg_native_plugin)==sizeof(pg_host)+16,"native layout");
static int value=73;
static int start(void *ctx,pg_host *host,size_t owner){return pg_host_provide(host,owner,"seed",ctx);}
static int stop(void *ctx){(void)ctx;return 0;}
static void unloaded(const char *path){void *handle=dlopen(path,RTLD_NOW|RTLD_NOLOAD);assert(!handle);}
int main(int argc,char **argv){
 assert(argc==6);pg_host root={0};const char *services[]={"seed"};pg_plugin p={"provider",&value,0,0,services,1,start,stop};pg_native_plugin plugin={0};
 assert(pg_native_open(0,argv[1],0)&&pg_native_open(&plugin,0,0));assert(pg_native_open(&plugin,"relative.so",0)&&plugin.error==PG_NATIVE_INVALID);assert(!pg_native_close(&plugin));
 char long_path[4096];memset(long_path,'a',sizeof(long_path));long_path[0]='/';assert(pg_native_open(&plugin,long_path,0)&&plugin.error==PG_NATIVE_INVALID);
 assert(pg_native_open(&plugin,"/nonexistent/pangu-plugin.so",0)&&plugin.error==PG_NATIVE_OPEN&&!plugin.library);
 assert(!pg_host_init(&root,&p,1)&&!pg_host_start(&root));
 assert(!pg_native_open(&plugin,argv[1],&root)&&root.child_count==1);int *state=pg_host_find(&plugin.host,"native.output");assert(state&&state[0]==73&&state[2]==1);
 assert(pg_native_open(&plugin,argv[1],&root));state[1]=2;
 assert(pg_native_close(&plugin)==1&&plugin.library&&root.child_count==1);assert(pg_native_close(&plugin)==1&&plugin.library);assert(!pg_native_close(&plugin)&&!plugin.library&&!root.child_count);unloaded(argv[1]);assert(!pg_native_close(&plugin));
 value=91;assert(!pg_native_open(&plugin,argv[1],&root));state=pg_host_find(&plugin.host,"native.output");assert(state&&state[0]==91);assert(!pg_host_close(&root)&&plugin.library);assert(!pg_native_close(&plugin));unloaded(argv[1]);
 for(int i=2;i<=4;++i){assert(pg_native_open(&plugin,argv[i],0));assert(plugin.error==(i==2?PG_NATIVE_ABI_MISMATCH:i==3?PG_NATIVE_SYMBOL:PG_NATIVE_HOST));assert(!pg_native_close(&plugin));unloaded(argv[i]);}
 assert(!pg_host_init(&root,&p,1)&&!pg_host_start(&root));assert(pg_native_open(&plugin,argv[5],&root)&&plugin.error==PG_NATIVE_HOST&&plugin.library&&root.child_count==1);assert(pg_native_close(&plugin)==1&&plugin.library);assert(!pg_native_close(&plugin)&&!plugin.library&&!root.child_count);unloaded(argv[5]);assert(!pg_host_close(&root));
 puts("PASS: real native DSO, ABI/export/dependency refusal, provider substitution, parent close, pending cleanup and unload ordering");return 0;
}
