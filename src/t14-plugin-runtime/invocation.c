#include "src/t14-abi/invocation2.h"
#include "route-query-private.h"
#define NONE (~(pangu_u64)0)
#define CAP_TAG 0x50414e4755434150ULL
static const pangu_invocation_api api;
static const pangu_invocation_api2 api2;
static void copy(void *out,const void *in,pangu_u64 n) {unsigned char *a=out;const unsigned char *b=in;for(pangu_u64 i=0;i<n;i++) a[i]=b[i];}
static int route_valid(const pangu_service_api *a) {return range(SP(a,1,pangu_service_api,8)) && a->abi_version==1 && a->struct_size==40 && a->target==1 && !a->reserved && a->publish && a->acquire && a->release;}
static int extension_valid(const pangu_lifecycle_child_api *a,const pangu_service_router *v) {return range(SP(a,1,pangu_lifecycle_child_api,8)) && a->abi_version==2 && a->struct_size==32 && a->target==1 && !a->reserved && a->base==v->host && a->enter_child;}
static pangu_i32 guards(pangu_invocation_bank *b,const span *out,pangu_u32 count,pangu_u64 *fuel) {
 if(!range(SP(b,1,pangu_invocation_bank,8)) || b->abi_version!=1 || b->struct_size!=56 || !b->epoch || b->count>0xffffffffU || !range(SP(b->router,1,pangu_service_router,8)) || !route_valid(b->route) || !extension_valid(b->child,b->router)) return 1;
 pangu_i32 rc=geometry(b->router,out,count,0,fuel);if(rc) return rc;
 span inputs[]={SP(b,1,pangu_invocation_bank,8),SP(b->slots,b->count,pangu_invocation_slot,8),SP(b->route,1,pangu_service_api,8),SP(b->child,1,pangu_lifecycle_child_api,8),SP(&api,1,pangu_invocation_api,8),SP(&api2,1,pangu_invocation_api2,8)};
 for(pangu_u32 i=0;i<sizeof inputs/sizeof inputs[0];i++) {rc=protect(inputs[i],out,count,fuel);if(rc) return rc;}
 return 0;
}
static pangu_i32 validate(pangu_invocation_bank *b,const pangu_plugin_lease *cap,pangu_invocation_slot **out) {
 if(!range(SP(cap,1,pangu_plugin_lease,8))) return 1;
 if(cap->opaque[0]!=(pangu_u64)b || cap->opaque[1]!=b->epoch || cap->opaque[2]>=b->count || !cap->opaque[3] || cap->opaque[6]!=CAP_TAG || cap->opaque[7]) return 10;
 pangu_invocation_slot *s=&b->slots[cap->opaque[2]];
 if(s->active!=1 || s->reserved || s->generation!=cap->opaque[3] || cap->opaque[4]!=s->lease.ticket.consumer.epoch || cap->opaque[5]!=s->lease.ticket.provider.epoch || cap->table!=s->lease.table || cap->state!=s->lease.state || s->lease.ticket.runtime!=b->router->runtime) return 10;
 *out=s;return 0;
}
static pangu_plugin_lease encode(pangu_invocation_bank *b,pangu_u64 index) {
 const pangu_invocation_slot *s=&b->slots[index];pangu_plugin_lease cap={{(pangu_u64)b,b->epoch,index,s->generation,s->lease.ticket.consumer.epoch,s->lease.ticket.provider.epoch,CAP_TAG,0},s->lease.table,s->lease.state};return cap;
}
static pangu_i32 find_free(pangu_invocation_bank *b,pangu_u64 *fuel,pangu_u64 *index) {
 for(pangu_u64 i=0;i<b->count;i++) {if(!*fuel) return 2;(*fuel)--;if(!b->slots[i].active && b->slots[i].generation!=NONE) {*index=i;return 0;}}return 2;
}
static pangu_i32 check_bind(pangu_invocation_bank *b,const pangu_service_router *v,const pangu_service_api *route,const pangu_lifecycle_child_api *child,pangu_invocation_slot *slots,pangu_u64 n,pangu_u64 epoch,pangu_u64 *remaining) {
 pangu_u64 fuel=*remaining;
 if(!epoch || n>0xffffffffU || !route_valid(route) || !range(SP(v,1,pangu_service_router,8)) || !extension_valid(child,v)) return 1;
 span outputs[]={SP(b,1,pangu_invocation_bank,8),SP(slots,n,pangu_invocation_slot,8)};pangu_i32 rc=geometry(v,outputs,2,0,&fuel);if(rc) return rc;
 span inputs[]={SP(route,1,pangu_service_api,8),SP(child,1,pangu_lifecycle_child_api,8),SP(&api,1,pangu_invocation_api,8),SP(&api2,1,pangu_invocation_api2,8)};for(pangu_u32 i=0;i<sizeof inputs/sizeof inputs[0];i++) {rc=protect(inputs[i],outputs,2,&fuel);if(rc) return rc;}
 if(v->runtime->phase || v->runtime->busy || v->runtime->started || v->runtime->stop_requested) return 10;
 if(b->abi_version) {if(b->abi_version!=1 || b->struct_size!=56 || epoch<=b->epoch || b->count>0xffffffffU || !range(SP(b->slots,b->count,pangu_invocation_slot,8))) return 10;for(pangu_u64 i=0;i<b->count;i++) {if(!fuel) return 2;fuel--;if(b->slots[i].active) return 10;}}
 else {const unsigned char *bytes=(const void *)b;for(pangu_u64 i=0;i<sizeof(*b);i++) {if(!fuel) return 2;fuel--;if(bytes[i]) return 1;}}
 for(pangu_u64 i=0;i<v->slot_count;i++) {if(!fuel) return 2;fuel--;if(v->slots[i].published) return 10;}
 if(fuel<n+1) return 2;fuel-=n+1;*remaining=fuel;return 0;
}
static pangu_i32 bind(pangu_invocation_bank *b,const pangu_service_router *v,const pangu_service_api *route,const pangu_lifecycle_child_api *child,pangu_invocation_slot *slots,pangu_u64 n,pangu_u64 epoch,pangu_u64 fuel) {
 pangu_i32 rc=check_bind(b,v,route,child,slots,n,epoch,&fuel);if(rc) return rc;
 for(pangu_u64 i=0;i<n;i++) {unsigned char *bytes=(void *)&slots[i];for(pangu_u64 j=0;j<sizeof(slots[i]);j++) bytes[j]=0;}
 *b=(pangu_invocation_bank){1,56,epoch,v,route,child,slots,n};return 0;
}
static pangu_i32 open_root(pangu_invocation_bank *b,const pangu_service_query *q,pangu_u64 fuel,pangu_plugin_lease *out,pangu_lifecycle_report *report) {
 pangu_u64 initial=fuel;span outputs[]={SP(out,1,pangu_plugin_lease,8),SP(report,1,pangu_lifecycle_report,8)};pangu_i32 rc=guards(b,outputs,2,&fuel);if(rc) return rc;const pangu_service_slot *selected;rc=select_route(b->router,q,&fuel,outputs,&selected);if(rc) return rc;
 pangu_u64 index;rc=find_free(b,&fuel,&index);if(rc) return rc;if(!fuel) return 2;fuel--;
 unsigned char scope[32]={0};if(q->mode) copy(scope,q->scope,q->mode==1 ? sizeof(pangu_lifecycle_setup_ticket) : sizeof(pangu_cleanup_ticket));
 pangu_service_lease lease;pangu_lifecycle_report result;result.reserved=0xa5a5a5a5U;rc=b->route->acquire(b->router,q,fuel,&lease,&result);if(result.reserved==0) {result.work+=initial-fuel;*report=result;}if(rc) return rc;
 pangu_u64 generation=b->slots[index].generation+1;pangu_invocation_slot node={generation,1,0,NONE,0,index,generation,lease,q->mode,0,{0}};copy(node.scope,scope,32);b->slots[index]=node;*out=encode(b,index);return 0;
}
static pangu_i32 open_child(pangu_invocation_bank *b,const pangu_plugin_lease *parent,pangu_u32 receiver,const pangu_service_query *q,pangu_u64 fuel,pangu_plugin_lease *out,pangu_lifecycle_report *report) {
 pangu_u64 initial=fuel;span outputs[]={SP(out,1,pangu_plugin_lease,8),SP(report,1,pangu_lifecycle_report,8)};pangu_i32 rc=guards(b,outputs,2,&fuel);if(rc) return rc;rc=protect(SP(parent,1,pangu_plugin_lease,8),outputs,2,&fuel);if(rc) return rc;const pangu_service_slot *selected;rc=select_route(b->router,q,&fuel,outputs,&selected);if(rc) return rc;
 if(q->mode || q->scope || q->consumer!=receiver || receiver>=b->router->plan->lowered.graph.instance_count) return 1;
 pangu_invocation_slot *p;rc=validate(b,parent,&p);if(rc) return rc;const pangu_entry_context *e=&b->router->records[receiver].context;
 if(p->lease.ticket.provider.owner!=e->owner || p->lease.ticket.provider.epoch!=e->owner_epoch || e->owner_epoch!=e->owner->epoch || p->root>=b->count) return 10;
 pangu_invocation_slot *root=&b->slots[p->root];if(root->active!=1 || root->parent!=NONE || root->root!=p->root || root->generation!=p->root_generation || root->root_generation!=root->generation || root->kind>2 || root->reserved || p->children==0xffffffffU) return 10;
 pangu_u64 index;rc=find_free(b,&fuel,&index);if(rc) return rc;if(!fuel) return 2;fuel--;
 /* Copies allow runtime geometry to protect manager metadata in future bundles. */
 pangu_lifecycle_call_ticket parent_ticket=p->lease.ticket,root_ticket=root->lease.ticket,ticket;pangu_u64 scope_words[4];copy(scope_words,root->scope,32);pangu_lifecycle_report result;result.reserved=0xa5a5a5a5U;
 rc=b->child->enter_child(b->router->runtime,q->binding,&parent_ticket,&root_ticket,root->kind,root->kind ? (const void *)scope_words : (void *)0,fuel,&ticket,&result);if(result.reserved==0) {result.work+=initial-fuel;*report=result;}if(rc) return rc;
 pangu_u64 generation=b->slots[index].generation+1;pangu_service_lease lease={ticket,selected->table,selected->state};pangu_invocation_slot node={generation,1,0,parent->opaque[2],p->generation,p->root,p->root_generation,lease,0,0,{0}};b->slots[index]=node;p->children++;*out=encode(b,index);return 0;
}
static pangu_i32 close_cap(pangu_invocation_bank *b,pangu_u32 consumer,const pangu_plugin_lease *cap,pangu_u64 fuel) {
 /* Treat the read-only external token as an output span for metadata exclusion. */
 span source=SP(cap,1,pangu_plugin_lease,8);pangu_i32 rc=guards(b,&source,1,&fuel);if(rc) return rc;pangu_invocation_slot *s;rc=validate(b,cap,&s);if(rc) return rc;
 if(consumer>=b->router->plan->lowered.graph.instance_count) return 10;const pangu_entry_context *e=&b->router->records[consumer].context;if(s->lease.ticket.consumer.owner!=e->owner || s->lease.ticket.consumer.epoch!=e->owner_epoch || e->owner_epoch!=e->owner->epoch) return 10;if(s->children) return 2;
 pangu_invocation_slot *parent=(void *)0;if(s->parent!=NONE) {if(s->parent>=b->count) return 10;parent=&b->slots[s->parent];if(parent->active!=1 || parent->generation!=s->parent_generation || !parent->children) return 10;}
 if(!fuel) return 2;fuel--;pangu_service_lease lease=s->lease;rc=b->route->release(b->router,&lease);if(rc) return rc;s->active=0;if(parent) parent->children--;return 0;
}
static pangu_i32 prepare_bind(pangu_invocation_bank *b,const pangu_service_router *v,const pangu_service_api *route,const pangu_lifecycle_child_api *child,pangu_invocation_slot *slots,pangu_u64 n,pangu_u64 epoch,pangu_u64 fuel,pangu_invocation_bind_plan *out) {
 pangu_u64 initial=fuel;span dest=SP(out,1,pangu_invocation_bind_plan,8);pangu_i32 rc=geometry(v,&dest,1,0,&fuel);if(rc) return rc;
 span inputs[]={SP(b,1,pangu_invocation_bank,8),SP(slots,n,pangu_invocation_slot,8),SP(route,1,pangu_service_api,8),SP(child,1,pangu_lifecycle_child_api,8),SP(&api,1,pangu_invocation_api,8),SP(&api2,1,pangu_invocation_api2,8)};
 for(pangu_u32 i=0;i<sizeof inputs/sizeof inputs[0];i++) {rc=protect(inputs[i],&dest,1,&fuel);if(rc) return rc;}
 if(b->abi_version==1) {if(b->count>0xffffffffU) return 1;rc=protect(SP(b->slots,b->count,pangu_invocation_slot,8),&dest,1,&fuel);if(rc) return rc;}
 pangu_u64 before=fuel;rc=check_bind(b,v,route,child,slots,n,epoch,&fuel);if(rc) return rc;pangu_u64 required=before-fuel;if(!fuel) return 2;fuel--;*out=(pangu_invocation_bind_plan){initial-fuel,required};return 0;
}
static int live_ticket(pangu_invocation_bank *b,const pangu_lifecycle_call_ticket *t) {
 pangu_lifecycle_runtime *r=b->router->runtime;if(t->runtime!=r || t->reserved || !r->stop_graph || t->binding>=r->stop_graph->edge_count) return 0;
 pangu_lifecycle_edge e=r->stop_graph->edges[t->binding];if(e.consumer>=r->launched || e.provider>=e.consumer || r->stop_graph->disposed[e.consumer] || r->stop_graph->disposed[e.provider]) return 0;
 pangu_owner *owners[2]={r->items[e.consumer].owner,r->items[e.provider].owner};const pangu_operation_ticket *parts[2]={&t->consumer,&t->provider};
 for(pangu_u32 i=0;i<2;i++) {const pangu_operation_ticket *p=parts[i];pangu_owner *o=owners[i];if(p->owner!=o || p->epoch!=o->epoch || !p->generation || p->index>=o->operation_capacity || !o->inflight || !o->operations[p->index].active || o->operations[p->index].generation!=p->generation) return 0;}return 1;
}
static pangu_i32 accept(pangu_invocation_bank *b,pangu_u32 receiver,const pangu_plugin_lease *cap,pangu_u64 fuel) {
 span source=SP(cap,1,pangu_plugin_lease,8);pangu_i32 rc=guards(b,&source,1,&fuel);if(rc) return rc;pangu_invocation_slot *s;rc=validate(b,cap,&s);if(rc) return rc;
 if(receiver>=b->router->plan->lowered.graph.instance_count || s->root>=b->count) return 10;
 const pangu_entry_context *e=&b->router->records[receiver].context;const pangu_invocation_slot *root=&b->slots[s->root];
 if(root->active!=1 || root->parent!=NONE || root->root!=s->root || root->generation!=s->root_generation || root->root_generation!=root->generation || root->kind>2 || root->reserved || s->lease.ticket.provider.owner!=e->owner || s->lease.ticket.provider.epoch!=e->owner_epoch || e->owner_epoch!=e->owner->epoch) return 10;
 if(fuel<2) return 2;fuel-=2;if(!live_ticket(b,&s->lease.ticket) || !live_ticket(b,&root->lease.ticket)) return 10;return 0;
}
static const pangu_invocation_api api={1,48,1,0,bind,open_root,open_child,close_cap};
static const pangu_invocation_api2 api2={2,40,1,0,&api,prepare_bind,accept};
const pangu_invocation_api *pangu_invocation_query(pangu_u32 version,pangu_u32 size,pangu_u32 target,pangu_u32 flags) {if(version==2 && size==40 && target==1 && !flags) return (const pangu_invocation_api *)(const void *)&api2;return version==1 && size==48 && target==1 && !flags ? &api : (void *)0;}
