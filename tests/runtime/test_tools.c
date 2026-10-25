#include "../../src/runtime/tools.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {int validation,permission,execution,deny,invalid,overflow;int order;} fixture;
static int validate(void *ctx,pg_slice args){fixture *f=ctx;(void)args;++f->validation;assert(f->order==0);f->order=1;return !f->invalid;}
static int allow(void *ctx,pg_slice args){fixture *f=ctx;(void)args;++f->permission;assert(f->order==1);f->order=2;return !f->deny;}
static int run(void *ctx,pg_slice args,char *out,size_t cap,size_t *size){fixture *f=ctx;(void)args;++f->execution;assert(f->order==2);f->order=3;if(f->overflow){*size=cap;return 0;}assert(cap>=2);memcpy(out,"ok",2);*size=2;return 0;}
static pg_slice s(const char *v){return (pg_slice){v,strlen(v)};}
int main(void){fixture f={0};pg_tool_registry r={0};pg_tool_definition d={"echo","test","{\"type\":\"object\"}",&f,validate,allow,run,0};
 assert(!pg_tool_registry_init(&r,&d,1));pg_tools service=pg_tool_registry_service(&r);assert(!service.replay_safe(service.context,(pg_slice){"echo",4}));d.replay_safe=1;assert(!pg_tool_registry_init(&r,&d,1));assert(service.replay_safe(service.context,(pg_slice){"echo",4})==1);assert(!service.replay_safe(service.context,(pg_slice){"missing",7}));d.replay_safe=2;assert(pg_tool_registry_init(&r,&d,1));assert(r.entries[0].replay_safe==1);d.replay_safe=1;char out[128];size_t n=0;
 assert(service.authorize(service.context,s("echo"),s("{}")));assert(!service.execute(service.context,s("echo"),s("{}"),out,sizeof(out),&n));assert(n==2 && f.execution==1 && f.order==3);
 f=(fixture){.deny=1};assert(service.execute(service.context,s("echo"),s("{}"),out,sizeof(out),&n));assert(!f.execution && f.validation==1 && f.permission==1);
 f=(fixture){.invalid=1};assert(service.execute(service.context,s("echo"),s("{}"),out,sizeof(out),&n));assert(!f.permission && !f.execution);
 f=(fixture){0};assert(service.execute(service.context,s("echo"),s("{}junk"),out,sizeof(out),&n));assert(!f.validation && !f.execution);
 assert(service.execute(service.context,s("echo"),s("[]"),out,sizeof(out),&n));assert(!f.validation);
 assert(!service.authorize(service.context,s("other"),s("{}")));assert(service.execute(service.context,s("other"),s("{}"),out,sizeof(out),&n));assert(!f.execution);
 f=(fixture){.overflow=1};assert(service.execute(service.context,s("echo"),s("{}"),out,sizeof(out),&n));assert(n<sizeof(out));
 pg_tool_definition dup[2]={d,d};assert(pg_tool_registry_init(&r,dup,2));assert(r.count==1 && r.entries[0].context==&f);
 d.schema="[]";assert(pg_tool_registry_init(&r,&d,1));d.schema="{\"type\":\"array\"}";assert(pg_tool_registry_init(&r,&d,1));
 assert(pg_tool_registry_init(&r,dup,PG_TOOL_LIMIT+1));assert(r.count==1);
 puts("PASS: registry transaction, duplicate/schema/name lookup, validation-before-policy-before-execution, denial, malformed args and output limit");return 0;}
