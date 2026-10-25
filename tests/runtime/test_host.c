#define _GNU_SOURCE
#include "../../src/runtime/host_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
typedef struct {unsigned id,fail,skip,pending,bad,starts,stops;pg_host *host;size_t owner;int *publication;void *observed;} fixture;
static unsigned trace[128],events;
static int values[32];
static const char *const names[]={"base","consumer","leaf"};
static int start(void *data,pg_host *host,size_t owner){fixture *f=data;f->host=host;f->owner=owner;++f->starts;trace[events++]=f->id;
 assert(pg_host_start(host));assert(pg_host_init(host,0,0));assert(pg_host_close(host)==-1);assert(!pg_host_find(host,"base"));
 if(f->id){f->observed=pg_host_get(host,owner,names[f->id-1]);assert(f->observed);assert(!pg_host_get(host,owner,"leaf"));}
 assert(pg_host_provide(host,owner,"undeclared",&values[0]));assert(pg_host_provide(host,owner,names[f->id],0));
 if(!f->skip){assert(!pg_host_provide(host,owner,names[f->id],f->publication));assert(pg_host_provide(host,owner,names[f->id],f->publication));}
 return f->fail?1:0;}
static int stop(void *data){fixture *f=data;assert(pg_host_close(f->host)==-1);++f->stops;trace[events++]=10+f->id;if(f->id)assert(pg_host_get(f->host,f->owner,names[f->id-1])==f->observed);if(f->bad)return -1;if(f->pending){--f->pending;return 1;}return 0;}
static void definitions(pg_plugin *p,fixture *f){for(unsigned i=0;i<3;++i){f[i]=(fixture){.id=i,.publication=&values[i]};p[i]=(pg_plugin){names[i],&f[i],i?names+i-1:0,i?1:0,names+i,1,start,stop};}}
static int bulk_start(void *data,pg_host *host,size_t owner){unsigned *calls=data;++*calls;const pg_plugin *p=&host->plugins[owner];for(size_t i=0;i<p->require_count;++i)assert(pg_host_get(host,owner,p->requires[i]));for(size_t i=0;i<p->provide_count;++i)assert(!pg_host_provide(host,owner,p->provides[i],&values[i]));return 0;}
static int bulk_stop(void *data){++*(unsigned*)data;return 0;}
static void limits_and_guards(void){
 pg_host host={0};pg_plugin plugins[32];char labels[32][64];const char *exports[32];unsigned calls[32]={0};
 for(unsigned i=0;i<32;++i){snprintf(labels[i],sizeof(labels[i]),"service-%u",i);exports[i]=labels[i];plugins[31-i]=(pg_plugin){labels[i],&calls[i],i?exports+i-1:0,i?1:0,exports+i,1,bulk_start,bulk_stop};}
 assert(!pg_host_init(&host,plugins,32)&&!pg_host_start(&host));assert(host.live_count==32&&host.service_count==32);for(unsigned i=0;i<32;++i)assert(calls[i]==1&&pg_host_find(&host,labels[i]));assert(!pg_host_close(&host));for(unsigned i=0;i<32;++i)assert(calls[i]==2);
 unsigned one=0;pg_plugin many={"all",&one,0,0,exports,32,bulk_start,bulk_stop};assert(!pg_host_init(&host,&many,1)&&!pg_host_start(&host));assert(host.service_count==32&&!pg_host_close(&host)&&one==2);many.provide_count=33;assert(pg_host_init(&host,&many,1)&&host.error==PG_HOST_INVALID);
 long page=sysconf(_SC_PAGESIZE);assert(page>=(long)sizeof(pg_host));char *map=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page,(size_t)page,PROT_NONE));
 char *name=map+page-64;memset(name,'x',63);name[63]=0;const char *last[]={name};many=(pg_plugin){name,&one,0,0,last,1,bulk_start,bulk_stop};assert(!pg_host_init(&host,&many,1)&&!pg_host_start(&host)&&pg_host_find(&host,name)&&!pg_host_close(&host));name[63]='x';assert(pg_host_init(&host,&many,1)&&host.error==PG_HOST_INVALID);
 pg_host *tail=(pg_host*)(map+page-sizeof(pg_host));memset(tail,0,sizeof(*tail));many=(pg_plugin){"tail",&one,0,0,exports,1,bulk_start,bulk_stop};assert(!pg_host_init(tail,&many,1)&&!pg_host_start(tail)&&!pg_host_close(tail));assert(!munmap(map,(size_t)page*2));
}

typedef struct {int value,*observed;unsigned pending,starts,stops,fail,stop_error;pg_host *host;size_t owner;} scoped_fixture;
static int scoped_start(void *data,pg_host *h,size_t owner){
 scoped_fixture *f=data;f->host=h;f->owner=owner;++f->starts;
 if(h->parent){unsigned phase=h->parent->phase;size_t count=h->parent->child_count;assert(pg_host_close(h->parent)==-1&&h->parent->phase==phase&&h->parent->child_count==count);}
 const pg_plugin *p=&h->plugins[owner];
 if(p->require_count){f->observed=pg_host_get(h,owner,p->requires[0]);assert(f->observed);}
 for(size_t i=0;i<p->provide_count;++i)assert(!pg_host_provide(h,owner,p->provides[i],&f->value));
 return f->fail?1:0;
}
static int scoped_stop(void *data){scoped_fixture *f=data;++f->stops;
 if(f->observed)assert(pg_host_get(f->host,f->owner,"base")==f->observed);
 if(f->stop_error)return -1;
 if(f->pending){--f->pending;return 1;}return 0;
}
static void child_scopes(void){
 const char *base[]={"base"},*output[]={"output"},*missing[]={"missing"};
 scoped_fixture a={.value=1},b={.value=2},c={.value=3,.pending=1},d={.value=4};
 pg_plugin provider={"root",&a,0,0,base,1,scoped_start,scoped_stop};
 pg_plugin consumer={"consumer",&c,base,1,output,1,scoped_start,scoped_stop};
 pg_plugin local={"override",&b,0,0,base,1,scoped_start,scoped_stop};
 pg_host root={0},child={0},override={0},grandchild={0},bad={0};
 assert(pg_host_init_child(&child,&root,&consumer,1));
 assert(!pg_host_init(&root,&provider,1)&&!pg_host_start(&root));
 assert(!pg_host_init_child(&child,&root,&consumer,1)&&!pg_host_start(&child));
 assert(c.observed==&a.value&&pg_host_find(&child,"base")==&a.value&&root.child_count==1);
 assert(!pg_host_init_child(&override,&root,&local,1)&&!pg_host_start(&override));
 assert(pg_host_find(&override,"base")==&b.value&&pg_host_find(&root,"base")==&a.value);
 pg_plugin nested=consumer;nested.context=&d;assert(!pg_host_init_child(&grandchild,&override,&nested,1)&&!pg_host_start(&grandchild));assert(d.observed==&b.value);
 pg_plugin absent=consumer;absent.requires=missing;assert(pg_host_init_child(&bad,&root,&absent,1)&&bad.error==PG_HOST_MISSING&&root.child_count==2);
 assert(!pg_host_close(&override)&&override.child_count==0&&d.stops==1&&b.stops==1&&root.child_count==1&&root.first_child==&child);
 assert(pg_host_close(&root)==1&&root.child_count==1&&!a.stops&&c.stops==1);
 assert(pg_host_init_child(&bad,&root,&consumer,1));
 assert(!pg_host_close(&root)&&c.stops==2&&a.stops==1&&!root.child_count&&!child.parent);
 assert(!pg_host_close(&child)&&!pg_host_close(&root));
 a.stops=0;c.stops=0;c.stop_error=1;
 assert(!pg_host_init(&root,&provider,1)&&!pg_host_start(&root));
 assert(!pg_host_init_child(&child,&root,&consumer,1)&&!pg_host_start(&child));
 assert(pg_host_close(&root)==-1&&root.error==PG_HOST_STOP_FAILED&&!a.stops&&child.parent==&root);
 c.stop_error=0;assert(!pg_host_close(&root)&&a.stops==1);
 assert(!pg_host_init(&root,&provider,1)&&!pg_host_start(&root));
 c.fail=1;assert(!pg_host_init_child(&child,&root,&consumer,1));
 assert(pg_host_start(&child)&&child.error==PG_HOST_START_FAILED&&!root.child_count&&pg_host_find(&root,"base")==&a.value);c.fail=0;
 pg_plugin self=local;self.requires=base;self.require_count=1;
 assert(pg_host_init_child(&bad,&root,&self,1)&&bad.error==PG_HOST_CYCLE&&!root.child_count);
 assert(!pg_host_close(&root));

 /* Prepared children and independently removed middle siblings retain links. */
 assert(!pg_host_init(&root,&provider,1)&&!pg_host_start(&root));
 pg_host children[33]={0};for(unsigned i=0;i<32;++i)assert(!pg_host_init_child(&children[i],&root,0,0));
 assert(pg_host_init_child(&children[32],&root,0,0));assert(!pg_host_close(&children[15])&&root.child_count==31);
 assert(!pg_host_init_child(&children[32],&root,0,0)&&root.child_count==32);assert(!pg_host_close(&root)&&!root.child_count);
 assert(!pg_host_init(&root,0,0)&&!pg_host_start(&root));pg_host chain[33]={0};pg_host *parent=&root;
 for(unsigned i=0;i<32;++i){assert(!pg_host_init_child(&chain[i],parent,0,0)&&!pg_host_start(&chain[i]));parent=&chain[i];}
 assert(pg_host_init_child(&chain[32],parent,0,0));assert(!pg_host_close(&root));
}

int main(void){child_scopes();limits_and_guards();pg_host h={0};pg_plugin p[3],reverse[3];fixture f[3];definitions(p,f);for(unsigned i=0;i<3;++i)reverse[i]=p[2-i];
 assert(!pg_host_init(&h,reverse,3));assert(!events&&!pg_host_find(&h,"base"));assert(!pg_host_start(&h));assert(events==3&&trace[0]==0&&trace[1]==1&&trace[2]==2);assert(pg_host_find(&h,"leaf")==&values[2]);assert(pg_host_provide(&h,0,"leaf",&values[2]));
 f[2].pending=1;assert(pg_host_close(&h)==1&&h.live_count==3&&!f[1].stops);assert(!pg_host_find(&h,"base"));assert(!pg_host_close(&h)&&h.live_count==0);assert(events==7&&trace[3]==12&&trace[4]==12&&trace[5]==11&&trace[6]==10);assert(!pg_host_close(&h)&&events==7);
 for(unsigned mode=0;mode<2;++mode){events=0;definitions(p,f);f[1].fail=!mode;f[1].skip=mode;f[1].pending=2;assert(!pg_host_init(&h,p,3));assert(pg_host_start(&h)&&h.error==(mode?PG_HOST_UNPUBLISHED:PG_HOST_START_FAILED));assert(h.error_owner==1);assert(h.live_count==2&&f[1].stops==1&&!f[0].stops&&!f[2].starts);assert(pg_host_close(&h)==1);assert(!pg_host_close(&h)&&f[0].stops==1&&h.live_count==0);}
 events=0;definitions(p,f);assert(!pg_host_init(&h,p,3)&&!pg_host_start(&h));f[2].bad=1;assert(pg_host_close(&h)==-1&&h.live_count==3&&h.error==PG_HOST_STOP_FAILED&&!f[1].stops);f[2].bad=0;assert(!pg_host_close(&h));
 events=0;definitions(p,f);const char *missing[]={"missing"};p[0].requires=missing;p[0].require_count=1;assert(pg_host_init(&h,p,3)&&h.error==PG_HOST_MISSING&&!events);assert(!pg_host_close(&h));
 const char *mixed[]={"leaf","missing"};definitions(p,f);p[0].requires=mixed;p[0].require_count=2;assert(pg_host_init(&h,p,3)&&h.error==PG_HOST_MISSING);
 definitions(p,f);p[0].requires=names+2;p[0].require_count=1;assert(pg_host_init(&h,p,3)&&h.error==PG_HOST_CYCLE&&!events);
 definitions(p,f);p[1].provides=names;assert(pg_host_init(&h,p,3)&&h.error==PG_HOST_DUPLICATE&&!events);
 definitions(p,f);p[1].name=names[0];assert(pg_host_init(&h,p,3)&&h.error==PG_HOST_DUPLICATE);
 definitions(p,f);p[0].start=0;assert(pg_host_init(&h,p,3)&&h.error==PG_HOST_INVALID);
 events=0;pg_host other={0};pg_plugin other_p[3];fixture other_f[3];int alternate=42;definitions(p,f);definitions(other_p,other_f);other_f[0].publication=&alternate;
 assert(!pg_host_init(&h,p,3)&&!pg_host_start(&h));assert(!pg_host_init(&other,other_p,3)&&!pg_host_start(&other));assert(pg_host_find(&h,"base")==&values[0]&&pg_host_find(&other,"base")==&alternate&&other_f[1].observed==&alternate&&f[1].observed==&values[0]);assert(!pg_host_get(&h,0,"consumer"));assert(pg_host_init(&h,p,3));assert(!pg_host_close(&other)&&pg_host_find(&h,"base")==&values[0]&&!pg_host_close(&h));
 assert(pg_host_init(&h,0,1)&&pg_host_init(&h,p,33)&&pg_host_init(0,p,1));assert(!pg_host_init(&h,0,0)&&!pg_host_start(&h)&&!pg_host_close(&h));
 puts("PASS: dependency order, scoped lookup/publication, preflight refusal, partial-start rollback, pending/error cleanup, inherited/overridden child scopes and reverse release");return 0;}
