#ifndef PANGU_LIFECYCLE_CHILD_PRIVATE_H
#define PANGU_LIFECYCLE_CHILD_PRIVATE_H
#include "src/t14-abi/lifecycle-child.h"
static pangu_i32 enter_child(pangu_lifecycle_runtime *r,pangu_u32 binding,const pangu_lifecycle_call_ticket *parent,const pangu_lifecycle_call_ticket *root,pangu_u32 kind,const void *scope,pangu_u64 fuel,pangu_lifecycle_call_ticket *out,pangu_lifecycle_report *report) {
 pangu_u64 scope_bytes=kind==1 ? sizeof(pangu_lifecycle_setup_ticket) : sizeof(pangu_cleanup_ticket);
 if(kind>2 || (!kind && scope) || !geo_external(r,parent,sizeof(*parent),8) || !geo_external(r,root,sizeof(*root),8) || !geo_external(r,out,sizeof(*out),8) || !geo_external(r,report,sizeof(*report),8) || geo_overlap(out,sizeof(*out),report,sizeof(*report)) || geo_overlap(out,sizeof(*out),parent,sizeof(*parent)) || geo_overlap(report,sizeof(*report),parent,sizeof(*parent)) || geo_overlap(out,sizeof(*out),root,sizeof(*root)) || geo_overlap(report,sizeof(*report),root,sizeof(*root)) || (kind && (!geo_external(r,scope,scope_bytes,8) || geo_overlap(out,sizeof(*out),scope,scope_bytes) || geo_overlap(report,sizeof(*report),scope,scope_bytes)))) return 1;
 pangu_u64 initial=fuel;
 #define RESULT(why,code) do {report->work=initial-fuel;report->state=r->phase;report->pending=(why)==1;report->reason=why;report->reserved=0;return code;} while(0)
 #define STEP() do {if(!fuel) {RESULT(1,2);}fuel--;} while(0)
 STEP();if(parent->runtime!=r || root->runtime!=r || parent->reserved || root->reserved) {RESULT(0,10);}
 pangu_owner *pc,*pp,*rc,*rp,*consumer,*provider;pangu_i32 status=pair(r,parent->binding,&pc,&pp);if(status) {RESULT(0,status);}status=pair(r,root->binding,&rc,&rp);if(status) {RESULT(0,status);}status=pair(r,binding,&consumer,&provider);if(status) {RESULT(0,status);}
 STEP();if(pp!=consumer || !operation_live(pc,&parent->consumer) || !operation_live(pp,&parent->provider) || !operation_live(rc,&root->consumer) || !operation_live(rp,&root->provider)) {RESULT(0,10);}
 if(kind==0 && (r->phase!=2 || r->stop_requested)) {RESULT(0,10);}
 if(kind==1) {
  const pangu_lifecycle_setup_ticket *s=scope;const pangu_operation_ticket *a=&s->operation,*b=&r->setup_operation;
  if(r->phase!=1 || r->stop_requested || s->runtime!=r || a->owner!=rc || !operation_live(rc,a) || !(r->callback_open || (r->setup_waiting && !r->setup_completed)) || a->owner!=b->owner || a->epoch!=b->epoch || a->index!=b->index || a->generation!=b->generation) {RESULT(0,10);}
 }
 if(kind==2 && (!r->stop_requested || r->phase!=3)) {RESULT(0,10);}
 STEP();status=pangu_owner_call_ready(rc,kind,kind==2 ? scope : (void *)0);if(status) {RESULT(0,status);}
 STEP();status=pangu_owner_call_ready(consumer,0,(void *)0);if(status) {RESULT(0,status);}
 STEP();status=pangu_owner_call_ready(provider,0,(void *)0);if(status) {RESULT(0,status);}
 pangu_u64 ci=0,pi=0;
 for(;ci<consumer->operation_capacity;ci++) {STEP();if(!consumer->operations[ci].active && consumer->operations[ci].generation!=0xffffffffU) break;}
 if(ci==consumer->operation_capacity || consumer->inflight==consumer->operation_capacity) {RESULT(0,2);}
 for(;pi<provider->operation_capacity;pi++) {STEP();if(!provider->operations[pi].active && provider->operations[pi].generation!=0xffffffffU) break;}
 if(pi==provider->operation_capacity || provider->inflight==provider->operation_capacity) {RESULT(0,2);}
 STEP();pangu_lifecycle_call_ticket ticket={r,binding,0,{0},{0}};status=pangu_owner_enter_slot(consumer,0,ci,&ticket.consumer);if(status) {RESULT(0,status);}status=pangu_owner_enter_slot(provider,0,pi,&ticket.provider);if(status) {(void)pangu_owner_leave(consumer,&ticket.consumer);RESULT(0,status);}*out=ticket;RESULT(0,0);
 #undef STEP
 #undef RESULT
}
static const pangu_lifecycle_child_api lifecycle_child_api={2,32,1,0,&lifecycle_api,enter_child};
#endif
