#include "src/t14-abi/service-route.h"
#include "route-geometry-private.h"
#include "route-query-private.h"
static int live_setup(const pangu_service_router *v,const pangu_entry_context *c) {
 pangu_lifecycle_runtime *r=v->runtime;
 if(c->abi_version!=1 || c->struct_size!=64 || c->reserved || c->host!=v->host || c->runtime!=r || c->instance_id>=v->plan->lowered.graph.instance_count || c!=&v->records[c->instance_id].context || !v->records[c->instance_id].entered || c->position>=r->launched || r->phase!=1 || r->stop_requested || !(r->callback_open || (r->setup_waiting && !r->setup_completed)) || r->items[c->position].owner!=c->owner || r->items[c->position].context!=&v->records[c->instance_id] || c->owner->epoch!=c->owner_epoch || c->owner->state!=1) return 0;
 const pangu_operation_ticket *t=&r->setup_operation;return t->owner==c->owner && t->epoch==c->owner_epoch && t->generation && t->index<c->owner->operation_capacity && c->owner->operations[t->index].active && c->owner->operations[t->index].generation==t->generation;
}
static pangu_i32 publish(const pangu_service_router *v,const pangu_entry_context *c,pangu_u32 provider,const pangu_service_header *h,void *state,pangu_u64 fuel) {
 if(!range(SP(v,1,pangu_service_router,8)) || !range(SP(c,1,pangu_entry_context,8)) || !range(SP(h,1,pangu_service_header,8))) return 1;
 span out=SP(v->slots,v->slot_count,pangu_service_slot,8);pangu_i32 rc=geometry(v,&out,1,1,&fuel);if(rc) return rc;
 if(provider>=v->slot_count) return 3;
 if(!live_setup(v,c)) return 10;
 const pangu_plan_provider *p=&v->plan->lowered.registry.providers[provider];
 if(v->plan->lowered.graph.provider_owners[provider]!=c->instance_id || p->service>=v->manifest->definition_count || h->version!=p->version || h->target!=1 || h->reserved || h->struct_size<16) return 9;
 span table=SP(h,h->struct_size,unsigned char,8);rc=geometry(v,&table,1,0,&fuel);if(rc) return rc;
 const unsigned char *b=(const void *)&v->slots[provider];for(pangu_u64 i=0;i<sizeof(pangu_service_slot);i++) {if(!fuel) return 2;fuel--;if(b[i]) return 4;}
 if(!fuel) return 2;
 v->slots[provider]=(pangu_service_slot){h,state,c->owner,c->owner_epoch,provider,h->version,h->struct_size,h->target,1,0};return 0;
}
static pangu_i32 acquire(const pangu_service_router *v,const pangu_service_query *q,pangu_u64 fuel,pangu_service_lease *out,pangu_lifecycle_report *report) {
 pangu_u64 initial=fuel;span outputs[]={SP(out,1,pangu_service_lease,8),SP(report,1,pangu_lifecycle_report,8)};const pangu_service_slot *slot;pangu_i32 rc=select_route(v,q,&fuel,outputs,&slot);if(rc) return rc;
 if(!fuel) return 2;fuel--;pangu_lifecycle_call_ticket ticket;pangu_lifecycle_report result;
 if(q->mode==0) rc=v->host->call(v->runtime,q->binding,fuel,&ticket,&result);
 else if(q->mode==1) rc=v->host->setup_call(v->runtime,q->binding,q->scope,fuel,&ticket,&result);
 else rc=v->host->cleanup_call(v->runtime,q->binding,q->scope,fuel,&ticket,&result);
 result.work+=initial-fuel;*report=result;
 if(!rc) *out=(pangu_service_lease){ticket,slot->table,slot->state};return rc;
}
static pangu_i32 release(const pangu_service_router *v,const pangu_service_lease *lease) {
 if(!range(SP(v,1,pangu_service_router,8)) || !host_valid(v->host) || !range(SP(lease,1,pangu_service_lease,8)) || lease->ticket.runtime!=v->runtime) return 1;
 return v->host->call_finish(v->runtime,&lease->ticket);
}
static const pangu_service_api api={1,40,1,0,publish,acquire,release};
const pangu_service_api *pangu_service_route_query(pangu_u32 abi,pangu_u32 size,pangu_u32 target,pangu_u32 flags) {return abi==1 && size==40 && target==1 && !flags ? &api : (void *)0;}
