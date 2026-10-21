#ifndef PANGU_ROUTE_QUERY_PRIVATE_H
#define PANGU_ROUTE_QUERY_PRIVATE_H
#include "route-geometry-private.h"
static pangu_i32 select_route(const pangu_service_router *v,const pangu_service_query *q,pangu_u64 *remaining,const span *outputs,const pangu_service_slot **selected) {
 if(!range(SP(q,1,pangu_service_query,8))) return 1;pangu_u64 fuel=*remaining;
 pangu_i32 rc=geometry(v,outputs,2,0,&fuel);if(rc) return rc;
 rc=protect(SP(q,1,pangu_service_query,8),outputs,2,&fuel);if(rc) return rc;
 /* Every published table is immutable borrowed metadata, including tables not
  * selected by this query; outputs may not overwrite another provider's ABI. */
 for(pangu_u64 i=0;i<v->slot_count;i++) {if(!fuel) return 2;fuel--;const pangu_service_slot *s=&v->slots[i];if(!s->published) continue;if(s->published!=1 || s->table_size<16) return 9;rc=protect(SP(s->table,s->table_size,unsigned char,8),outputs,2,&fuel);if(rc) return rc;}
 if(q->reserved || q->mode>2 || !q->version || q->minimum_size<16 || (q->mode==0 && q->scope)) return 1;
 if(q->mode) {rc=protect(SP(q->scope,1,unsigned char,8),outputs,2,&fuel);if(rc) return rc;span scope=SP(q->scope,q->mode==1 ? sizeof(pangu_lifecycle_setup_ticket) : sizeof(pangu_cleanup_ticket),unsigned char,8);rc=protect(scope,outputs,2,&fuel);if(rc) return rc;}
 if(q->binding>=v->plan->graph_report.binding_count) return 3;
 pangu_plan_edge edge=v->graph->bindings[q->binding];const pangu_graph_input *g=&v->plan->lowered.graph;const pangu_plan_registry *reg=&v->plan->lowered.registry;
 if(edge.requirement>=g->requirement_count || edge.provider>=reg->provider_count) return 16;
 const pangu_plan_requirement *req=&g->requirements[edge.requirement];const pangu_plan_provider *p=&reg->providers[edge.provider];
 if(req->consumer!=q->consumer || req->service!=p->service || req->version!=q->version || p->version!=q->version || p->service>=v->manifest->definition_count || req->consumer>=g->instance_count || g->provider_owners[edge.provider]>=g->instance_count) return 9;
 const pangu_manifest_definition *d=&v->manifest->definitions[p->service];if(d->version!=q->version) return 9;
 for(pangu_u32 i=0;i<32;i++) {if(!fuel) return 2;fuel--;if(q->service[i]!=d->name[i]) return 9;}
 const pangu_entry_context *c=&v->records[req->consumer].context,*pc=&v->records[g->provider_owners[edge.provider]].context;
 if(c->abi_version!=1 || pc->abi_version!=1 || c->struct_size!=64 || pc->struct_size!=64 || c->reserved || pc->reserved || c->host!=v->host || pc->host!=v->host || c->runtime!=v->runtime || pc->runtime!=v->runtime || c->instance_id!=req->consumer || pc->instance_id!=g->provider_owners[edge.provider] || c->owner_epoch!=c->owner->epoch || pc->owner_epoch!=pc->owner->epoch) return 10;
 pangu_lifecycle_edge actual=v->runtime->stop_graph->edges[q->binding];
 if(c->position!=actual.consumer || pc->position!=actual.provider || c->position>=v->runtime->count || pc->position>=v->runtime->count || v->runtime->items[c->position].owner!=c->owner || v->runtime->items[pc->position].owner!=pc->owner || v->runtime->items[c->position].context!=&v->records[req->consumer] || v->runtime->items[pc->position].context!=&v->records[g->provider_owners[edge.provider]]) return 16;
 const pangu_service_slot *slot=&v->slots[edge.provider];if(!slot->published) return 3;
 if(slot->published!=1 || slot->reserved || slot->provider!=edge.provider || slot->owner!=pc->owner || slot->epoch!=pc->owner_epoch || slot->epoch!=pc->owner->epoch || slot->version!=q->version || slot->target!=1 || slot->table_size<q->minimum_size || !range(SP(slot->table,slot->table_size,unsigned char,8))) return 9;
 rc=protect(SP(slot->table,slot->table_size,unsigned char,8),outputs,2,&fuel);if(rc) return rc;
 if(slot->table->version!=slot->version || slot->table->struct_size!=slot->table_size || slot->table->target!=1 || slot->table->reserved) return 9;
 *remaining=fuel;*selected=slot;return 0;
}
#endif
