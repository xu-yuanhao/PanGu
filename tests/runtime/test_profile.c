#define _GNU_SOURCE
#include "../../src/runtime/profile_layout.h"
#include "../../src/runtime/agent_layout.h"
#include "../../src/runtime/journal.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
_Static_assert(offsetof(pg_session_control,profile_size)==PG_SC_PROFILE_SIZE,"profile size offset");
_Static_assert(offsetof(pg_session_control,profile)==PG_SC_PROFILE,"profile bytes offset");
_Static_assert(offsetof(pg_agent,profile_verified)==PG_A_VERIFIED,"verified flag");
static pg_agent agent;
static pg_journal journal={.file=-1,.directory=-1};
static unsigned starts;
static int start(void *c,const pg_session *s){(void)c;(void)s;++starts;return 0;}
static int next(void *c,pg_model_event *e){(void)c;*e=(pg_model_event){.kind=PG_MODEL_WAIT};return 0;}
static void noop(void *c){(void)c;}
static int validate(void *c,pg_slice s){(void)c;(void)s;return 1;}
static int execute(void *c,pg_slice s,char *o,size_t n,size_t *size){(void)c;(void)s;(void)o;(void)n;*size=0;return 0;}
static pg_profile_spec spec(void){return (pg_profile_spec){.provider={"fixture",7},.model={"model-a",7},.workspace={"/tmp/workspace",14},.implementation={"readonly-v1",11},.device=7,.inode=19};}
static unsigned char identity[4096];static size_t length;
static void init(void){assert(!pg_agent_init(&agent,(pg_model){0,start,next,noop,noop},(pg_tools){0}));}
static void encode(void){pg_profile_spec p=spec();assert(!pg_profile_encode(&p,identity,sizeof(identity),&length));assert(length>48&&length<4096);}
static void encoding(void){pg_profile_spec p=spec();unsigned char original[4096],changed[4096];size_t n=0,m=0;assert(!pg_profile_encode(&p,original,sizeof(original),&n));assert(!memcmp(original,"PGCFG1",6));
 for(unsigned i=0;i<6;++i){p=spec();if(i==0)p.model=(pg_slice){"model-b",7};if(i==1)p.workspace=(pg_slice){"/tmp/other",10};if(i==2)++p.device;if(i==3)++p.inode;if(i==4)p.implementation=(pg_slice){"readonly-v2",11};if(i==5)p.provider=(pg_slice){"other",5};assert(!pg_profile_encode(&p,changed,sizeof(changed),&m));assert(n!=m||memcmp(original,changed,n));}
 p=spec();for(size_t cap=0;cap<=n;++cap){size_t out=123;int rc=pg_profile_encode(&p,changed,cap,&out);assert((cap<n&&rc&&out==0)||(cap==n&&!rc&&out==n));}
 p.model=(pg_slice){"x\0y",3};m=9;assert(pg_profile_encode(&p,changed,sizeof(changed),&m)&&!m);
 pg_tool_definition definition={"echo","description","{\"type\":\"object\"}",0,validate,validate,execute,0,0,0};pg_tool_registry registry;assert(!pg_tool_registry_init(&registry,&definition,1));p=spec();p.tools=&registry;assert(!pg_profile_encode(&p,original,sizeof(original),&n));registry.entries[0].description="changed";assert(!pg_profile_encode(&p,changed,sizeof(changed),&m));assert(n!=m||memcmp(original,changed,n));
 char maximum[4047];memset(maximum,'x',sizeof(maximum));p=(pg_profile_spec){.provider={"a",1},.model={maximum,4045},.workspace={"b",1},.implementation={"c",1}};assert(!pg_profile_encode(&p,changed,sizeof(changed),&m)&&m==4096);p.model.size=4046;assert(pg_profile_encode(&p,changed,sizeof(changed),&m)&&!m);assert(pg_profile_encode(0,changed,sizeof(changed),&m)&&!m);assert(pg_profile_encode(&p,0,sizeof(changed),&m)&&!m);assert(pg_profile_encode(&p,changed,sizeof(changed),0));
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *map=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page,(size_t)page,PROT_NONE));p=spec();assert(!pg_profile_encode(&p,map+page-4096,4096,&n));assert(!munmap(map,(size_t)page*2));}
static void persistence(void){char directory[]="/tmp/pangu-profile-XXXXXX";assert(mkdtemp(directory));int root=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(root>=0);encode();init();assert(!pg_journal_open(&journal,root,"session.pgj",&agent.session,&agent.commits));assert(!pg_agent_bind_profile(&agent,(pg_slice){(char*)identity,length}));assert(agent.profile_verified&&agent.commits.profile_size==length&&agent.commits.sequence==1);pg_client client=pg_agent_client(&agent);assert(!client.submit(client.context,(pg_slice){"pending",7}));uint64_t sequence=agent.commits.sequence;assert(!pg_agent_suspend(&agent));pg_journal_close(&journal);
 init();journal=(pg_journal){.file=-1,.directory=-1};assert(!pg_journal_open(&journal,root,"session.pgj",&agent.session,&agent.commits));assert(!agent.profile_verified&&agent.commits.sequence==sequence&&agent.commits.run.phase==PG_RUN_MODEL);assert(pg_agent_resume(&agent,0)&&!starts);client=pg_agent_client(&agent);assert(client.submit(client.context,(pg_slice){"duplicate",9}));unsigned char changed[4096];memcpy(changed,identity,length);changed[length-1]^=1;assert(pg_agent_bind_profile(&agent,(pg_slice){(char*)changed,length}));assert(agent.commits.sequence==sequence&&!agent.profile_verified);assert(!pg_agent_bind_profile(&agent,(pg_slice){(char*)identity,length}));assert(agent.commits.sequence==sequence&&agent.profile_verified);assert(!pg_agent_resume(&agent,0));pg_event e;assert(client.poll(client.context,&e)&&e.kind==PG_RUN_STARTED);assert(!client.poll(client.context,&e)&&starts==1);client.cancel(client.context);unsigned ended=0;for(unsigned i=0;i<8;++i)if(client.poll(client.context,&e)&&e.kind==PG_RUN_FINISHED){assert(e.outcome==PG_CANCELLED);ended=1;break;}assert(ended);assert(!pg_agent_bind_profile(&agent,(pg_slice){(char*)changed,length}));assert(!memcmp(agent.commits.profile,changed,length));pg_journal_close(&journal);
 init();journal=(pg_journal){.file=-1,.directory=-1};assert(!pg_journal_open(&journal,root,"session.pgj",&agent.session,&agent.commits));assert(!pg_agent_bind_profile(&agent,(pg_slice){(char*)changed,length}));assert(pg_agent_bind_profile(&agent,(pg_slice){0,0})&&!agent.profile_verified);assert(!pg_agent_bind_profile(&agent,(pg_slice){(char*)changed,length}));pg_journal_close(&journal);close(root);char path[512];snprintf(path,sizeof(path),"%s/session.pgj",directory);assert(!unlink(path)&&!rmdir(directory));}
int main(void){encoding();persistence();puts("PASS: exact profile framing, model/tool/workspace changes, byte quotas/guard and unverified/mismatched active resume rejection");return 0;}
