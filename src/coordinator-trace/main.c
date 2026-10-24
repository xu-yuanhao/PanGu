#include "src/t14-abi/client-abi-v1.h"
#include "src/focused-test-support/assert.h"
#include "src/focused-test-support/stdio.h"

#define FUEL 1000000U
typedef struct model model;
typedef struct {model *model;int id;} context;
struct model {
 pangu_lifecycle_runtime runtime;
 pangu_owner owners[4];pangu_owned_resource resources[4][2];pangu_owned_operation operations[4][3];
 pangu_lifecycle_item items[4];pangu_lifecycle_span spans[32];
 pangu_lifecycle_edge edges[3];pangu_u32 disposed[4];pangu_lifecycle_graph graph;
 context contexts[4];pangu_lifecycle_call_ticket held;
 int case_id,seq,fail_at,cancel_at,cleanup_fail,hold_at,trace[32],n;
};
static model models[7];
static const pangu_lifecycle_api *api;

static void event(model *m,const char *kind,int id,int rc,const pangu_lifecycle_report *report) {
 assert(m->seq>=0 && m->seq<128);
 assert(m->runtime.count<=4 && m->runtime.started<=m->runtime.launched && m->runtime.launched<=m->runtime.count);
 for(int i=0;i<4;i++) assert(m->owners[i].inflight<=3 && m->owners[i].count<=2);
 if(report) assert(report->work<=FUEL && report->reserved==0);
 printf("{\"case\":%d,\"seq\":%d,\"event\":\"%s\",\"id\":%d,\"rc\":%d,\"phase\":%d,\"count\":%d,\"started\":%d,\"launched\":%d,\"stop\":%d,\"load_error\":%d,\"cleanup_error\":%d,\"state\":[%d,%d,%d,%d],\"inflight\":[%d,%d,%d,%d],\"resources\":[%d,%d,%d,%d],\"has_report\":%d,\"work\":%d,\"pending\":%d,\"reason\":%d}\n",
 m->case_id,m->seq++,kind,id,rc,(int)m->runtime.phase,(int)m->runtime.count,(int)m->runtime.started,(int)m->runtime.launched,
 (int)m->runtime.stop_requested,m->runtime.load_error,m->runtime.cleanup_error,
 (int)m->owners[0].state,(int)m->owners[1].state,(int)m->owners[2].state,(int)m->owners[3].state,
 (int)m->owners[0].inflight,(int)m->owners[1].inflight,(int)m->owners[2].inflight,(int)m->owners[3].inflight,
 (int)m->owners[0].count,(int)m->owners[1].count,(int)m->owners[2].count,(int)m->owners[3].count,
 report!=0,report?(int)report->work:0,report?(int)report->pending:0,report?(int)report->reason:0);
}

static pangu_i32 release(void *opaque) {
 context *c=opaque;model *m=c->model;int id=c->id;
 assert(m->n<32);m->trace[m->n++]=100+id;
 for(int j=0;j<id;j++) assert(m->owners[j].state==PANGU_OWNER_ACTIVE);
 assert(!m->owners[id].inflight && !m->owners[id].count);
 event(m,"cleanup-begin",id,0,0);
 pangu_lifecycle_report report={0};int rc=api->stop(&m->runtime,FUEL,&report);
 assert(rc==5);event(m,"stop-reentrant-cleanup",id,rc,&report);
 rc=api->register_resource(&m->runtime,(pangu_u32)id,release,opaque);
 assert(rc==10 && !m->owners[id].count);event(m,"register-during-cleanup",id,rc,0);
 rc=id==m->cleanup_fail ? -90 : 0;event(m,"cleanup-return",id,rc,0);return rc;
}

static pangu_i32 setup(pangu_owner *owner,void *opaque) {
 context *c=opaque;model *m=c->model;int id=c->id;
 assert(owner==&m->owners[id] && owner->inflight==1);
 assert(m->runtime.started==(pangu_u64)id+1 && m->n<32);
 m->trace[m->n++]=id;event(m,"setup-begin",id,0,0);
 int rc=api->register_resource(&m->runtime,(pangu_u32)id,release,opaque);
 assert(!rc && owner->count==1);event(m,"setup-registered",id,rc,0);
 if(id==m->hold_at) {
  assert(id>0);pangu_lifecycle_setup_ticket scope;
  assert(!api->setup_scope(&m->runtime,&scope));pangu_lifecycle_report report={0};
  rc=api->setup_call(&m->runtime,(pangu_u32)id-1,&scope,FUEL,&m->held,&report);
  assert(!rc && owner->inflight==2 && m->owners[id-1].inflight==1);
  event(m,"hold-acquired",id,rc,&report);
 }
 if(id==m->cancel_at) {
  pangu_lifecycle_report report={0};rc=api->stop(&m->runtime,FUEL,&report);
  assert(rc==5);event(m,"stop-reentrant-setup",id,rc,&report);
  rc=api->register_resource(&m->runtime,(pangu_u32)id,release,opaque);
  assert(rc==10 && owner->count==1);event(m,"register-after-stop",id,rc,0);
 }
 rc=id==m->fail_at ? -70 : 0;event(m,"setup-return",id,rc,0);return rc;
}

static model *fresh(int case_id,int count,int graph) {
 model *m=&models[case_id-1];m->case_id=case_id;
 m->fail_at=m->cancel_at=m->cleanup_fail=m->hold_at=-1;
 for(int i=0;i<4;i++) {
  assert(!api->owner_init(&m->owners[i],m->resources[i],2,m->operations[i],3,1,4));
  m->contexts[i]=(context){m,i};m->items[i]=(pangu_lifecycle_item){&m->owners[i],setup,&m->contexts[i]};
 }
 m->runtime.items=m->items;m->runtime.count=(pangu_u64)count;
 if(graph) {
  for(int i=1;i<count;i++) m->edges[i-1]=(pangu_lifecycle_edge){(pangu_u32)i,(pangu_u32)i-1};
  m->graph=(pangu_lifecycle_graph){m->edges,(pangu_u64)count-1,m->disposed,4};m->runtime.stop_graph=&m->graph;
 }
 event(m,"case-begin",-1,0,0);return m;
}

static int bind(model *m) {
 int rc=api->bind_geometry(&m->runtime,m->spans,32,FUEL);event(m,"bind",-1,rc,0);return rc;
}
static int start(model *m,const char *kind,int has_report) {
 pangu_lifecycle_report report={0};int rc=api->start(&m->runtime,FUEL,&report);
 event(m,kind,-1,rc,has_report?&report:0);return rc;
}
static int stop(model *m,const char *kind) {
 pangu_lifecycle_report report={0};int rc=api->stop(&m->runtime,FUEL,&report);event(m,kind,-1,rc,&report);return rc;
}
static void finish_hold(model *m) {
 pangu_lifecycle_call_ticket extra;pangu_lifecycle_report report={0};
 int rc=api->call(&m->runtime,m->held.binding,FUEL,&extra,&report);
 assert(rc==10);event(m,"business-after-stop",-1,rc,&report);
 rc=api->call_finish(&m->runtime,&m->held);assert(!rc);event(m,"hold-finished",-1,rc,0);
 rc=api->call_finish(&m->runtime,&m->held);assert(rc==10);event(m,"hold-finish-duplicate",-1,rc,0);
}
static void end(model *m) {event(m,"case-end",-1,0,0);}

int main(void) {
 api=pangu_lifecycle_query(1,152,1,0);assert(api);
 model *m=fresh(1,3,0);assert(!bind(m));assert(!start(m,"start-return",1));assert(m->runtime.started==3);
 assert(!stop(m,"stop-return") && m->n==6);
 for(int i=0;i<3;i++) assert(m->trace[i]==i && m->trace[i+3]==102-i);
 assert(!stop(m,"stop-again") && m->n==6);end(m);

 m=fresh(2,4,0);m->fail_at=m->cleanup_fail=2;assert(!bind(m));assert(start(m,"start-return",1)==-70);
 assert(m->runtime.load_error==-70 && m->runtime.cleanup_error==-90 && !m->runtime.started);
 assert(m->n==6 && m->trace[3]==102 && m->trace[4]==101 && m->trace[5]==100 && m->owners[3].state==1);
 assert(stop(m,"stop-again")==-90 && m->n==6);end(m);

 m=fresh(3,4,0);m->cancel_at=1;assert(!bind(m));assert(start(m,"start-return",1)==6);
 assert(m->n==4 && m->trace[0]==0 && m->trace[1]==1 && m->trace[2]==101 && m->trace[3]==100);
 assert(m->owners[2].state==1 && m->owners[3].state==1);end(m);

 m=fresh(4,3,1);m->hold_at=2;assert(!bind(m));assert(!start(m,"start-return",1));
 assert(stop(m,"stop-wait")==5 && m->runtime.started==3 && m->n==3);
 assert(m->owners[0].state==2 && m->owners[1].state==2);assert(start(m,"start-again",0)==10);
 finish_hold(m);assert(!stop(m,"stop-return") && m->n==6 && m->trace[3]==102 && m->trace[5]==100);end(m);

 m=fresh(5,4,1);m->fail_at=m->hold_at=1;assert(!bind(m));assert(start(m,"start-return",1)==5);
 assert(m->runtime.started==2 && m->runtime.load_error==-70 && m->n==2 && m->owners[0].state==2);
 finish_hold(m);assert(!stop(m,"stop-return") && m->runtime.load_error==-70);
 assert(m->n==4 && m->trace[2]==101 && m->trace[3]==100);end(m);

 m=fresh(6,2,0);m->items[1].owner=&m->owners[0];assert(bind(m)==1);
 assert(start(m,"start-return",0)==1 && !m->n && !m->runtime.started);end(m);

 m=fresh(7,0,0);assert(!bind(m));assert(!start(m,"start-return",1));assert(!stop(m,"stop-return") && !m->n);end(m);
 return 0;
}
