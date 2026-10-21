#include "src/t14-abi/plugin-host2.h"
#include "route-geometry-private.h"
static const pangu_plugin_sdk client_api;
static const pangu_plugin_host_api host_api;
static const pangu_plugin_sdk2 client_api2;
static const pangu_plugin_host_api2 host_api2;
static void copy(void *dst,const void *src,pangu_u64 n) {unsigned char *d=dst;const unsigned char *s=src;for(pangu_u64 i=0;i<n;i++) d[i]=s[i];}
static int route_valid(const pangu_service_api *a) {return range(SP(a,1,pangu_service_api,8)) && a->abi_version==1 && a->struct_size==40 && a->target==1 && !a->reserved && a->publish && a->acquire && a->release;}
static pangu_i32 identity(const pangu_plugin_context *c,pangu_plugin_binding **out,int entered) {
 if(!range(SP(c,1,pangu_plugin_context,8)) || c->struct_size!=32 || !((c->abi_version==1 && c->host==&client_api) || (c->abi_version==2 && c->host==&client_api2.base)) || !range(SP(c->opaque,1,pangu_plugin_binding,8))) return 1;
 pangu_plugin_binding *b=(void *)c->opaque;const pangu_plugin_bank *bank=b->bank;
 if(c!=&b->context || b->reserved || (entered && b->entered!=1) || !range(SP(bank,1,pangu_plugin_bank,8)) || !range(SP(bank->router,1,pangu_service_router,8)) || !route_valid(bank->route) || !range(SP(b->record,1,pangu_entry_record,8))) return 10;
 const pangu_entry_context *e=&b->record->context;const pangu_service_router *v=bank->router;
 if(!range(SP(v->plan,1,pangu_prepare_result,8)) || !range(SP(bank->bindings,bank->count,pangu_plugin_binding,8)) || bank->count!=v->plan->lowered.graph.instance_count || e->instance_id>=bank->count || &bank->bindings[e->instance_id]!=b || b->record!=&v->records[e->instance_id] || e->userdata!=b || !range(SP(e->owner,1,pangu_owner,8)) || b->epoch!=e->owner_epoch || b->epoch!=e->owner->epoch || !b->plugin.entry) return 10;
 *out=b;return 0;
}
static pangu_i32 bank_guard(const pangu_plugin_binding *binding,const span *out,pangu_u32 count,pangu_u64 *fuel) {
 const pangu_plugin_bank *bank=binding->bank;pangu_i32 rc;
 span inputs[]={SP(bank,1,pangu_plugin_bank,8),SP(bank->bindings,bank->count,pangu_plugin_binding,8),SP(bank->route,1,pangu_service_api,8),SP(&client_api,1,pangu_plugin_sdk,8),SP(&host_api,1,pangu_plugin_host_api,8)};
 for(pangu_u32 i=0;i<sizeof inputs/sizeof inputs[0];i++) {rc=protect(inputs[i],out,count,fuel);if(rc) return rc;}
 if(binding->context.abi_version==2) {
  const pangu_plugin_bank2 *v=(const void *)bank;if(!range(SP(v,1,pangu_plugin_bank2,8)) || v->calls.abi_version!=1 || v->calls.struct_size!=56 || v->calls.router!=bank->router || v->calls.route!=bank->route || v->calls.count>0xffffffffU || !range(SP(v->calls_api,1,pangu_invocation_api2,8)) || v->calls_api->abi_version!=2 || v->calls_api->struct_size!=40 || v->calls_api->target!=1 || v->calls_api->reserved || !v->calls_api->base || !v->calls_api->accept) return 10;
  span extra[]={SP(v,1,pangu_plugin_bank2,8),SP(v->calls.slots,v->calls.count,pangu_invocation_slot,8),SP(v->calls_api,1,pangu_invocation_api2,8),SP(v->calls_api->base,1,pangu_invocation_api,8),SP(v->calls.child,1,pangu_lifecycle_child_api,8),SP(&client_api2,1,pangu_plugin_sdk2,8),SP(&host_api2,1,pangu_plugin_host_api2,8)};
  for(pangu_u32 i=0;i<sizeof extra/sizeof extra[0];i++) {rc=protect(extra[i],out,count,fuel);if(rc) return rc;}
 }
 return 0;
}
static pangu_i32 guards(const pangu_plugin_binding *b,const span *out,pangu_u32 count,pangu_u64 *fuel) {
 const pangu_plugin_bank *bank=b->bank;pangu_i32 rc=geometry(bank->router,out,count,0,fuel);if(rc) return rc;rc=bank_guard(b,out,count,fuel);if(rc) return rc;
 for(pangu_u64 i=0;i<bank->router->slot_count;i++) {if(!*fuel) return 2;(*fuel)--;const pangu_service_slot *s=&bank->router->slots[i];if(!s->published) continue;if(s->published!=1 || s->table_size<16) return 9;rc=protect(SP(s->table,s->table_size,unsigned char,8),out,count,fuel);if(rc) return rc;}
 return 0;
}
static pangu_i32 scope(const pangu_plugin_binding *b,pangu_u32 *mode,pangu_lifecycle_setup_ticket *setup,pangu_cleanup_ticket *cleanup) {
 const pangu_entry_context *e=&b->record->context;const pangu_service_router *v=b->bank->router;pangu_lifecycle_runtime *r=v->runtime;
 if(e->runtime!=r || e->host!=v->host || e->position>=r->launched || r->items[e->position].owner!=e->owner || r->items[e->position].context!=b->record) return 10;
 if(r->phase==1) {pangu_i32 rc=v->host->setup_scope(r,setup);if(rc) return rc;if(setup->runtime!=r || setup->operation.owner!=e->owner || setup->operation.epoch!=b->epoch) return 10;*mode=1;return 0;}
 if(r->phase==2 && !r->stop_requested && e->owner->state==2) {*mode=0;return 0;}
 if(r->phase==3) {pangu_i32 rc=v->host->cleanup_scope(r,e->position,cleanup);if(rc) return rc;if(cleanup->owner!=e->owner || cleanup->epoch!=b->epoch) return 10;*mode=2;return 0;}
 return 10;
}
static pangu_i32 equal_name(const unsigned char *a,const unsigned char *b,pangu_u64 *fuel,pangu_u32 *equal) {
 *equal=0;for(pangu_u32 i=0;i<32;i++) {if(!*fuel) return 2;(*fuel)--;if(a[i]!=b[i]) return 0;}*equal=1;return 0;
}
/* Find by immutable stable identity. index only written after all matching edges
 * scanned; explicit ordinal preserves MANY in original resolved order. */
static pangu_i32 select(const pangu_plugin_binding *b,const pangu_plugin_key *key,pangu_u32 ordinal,int providing,pangu_u64 *fuel,pangu_u64 *count,pangu_u32 *index) {
 if(key->reserved || !key->version) return 1;
 const pangu_service_router *v=b->bank->router;const pangu_graph_input *g=&v->plan->lowered.graph;const pangu_plan_registry *reg=&v->plan->lowered.registry;pangu_u32 id=b->record->context.instance_id,declared=providing;*count=0;*index=0xffffffffU;
 if(!providing) for(pangu_u64 i=0;i<g->requirement_count;i++) {if(!*fuel) return 2;(*fuel)--;const pangu_plan_requirement *req=&g->requirements[i];if(req->consumer!=id || req->version!=key->version) continue;if(req->service>=v->manifest->definition_count) return 16;pangu_u32 eq;pangu_i32 rc=equal_name(key->name,v->manifest->definitions[req->service].name,fuel,&eq);if(rc) return rc;if(eq) declared=1;}
 if(!declared) return 3;
 pangu_u64 n=providing ? reg->provider_count : v->plan->graph_report.binding_count;
 for(pangu_u64 i=0;i<n;i++) {if(!*fuel) return 2;(*fuel)--;pangu_u32 service,version;
  if(providing) {if(g->provider_owners[i]!=id) continue;service=reg->providers[i].service;version=reg->providers[i].version;}
  else {pangu_plan_edge e=v->graph->bindings[i];if(e.requirement>=g->requirement_count || e.provider>=reg->provider_count) return 16;const pangu_plan_requirement *req=&g->requirements[e.requirement];if(req->consumer!=id) continue;service=req->service;version=req->version;}
  if(version!=key->version) continue;if(service>=v->manifest->definition_count) return 16;pangu_u32 eq;pangu_i32 rc=equal_name(key->name,v->manifest->definitions[service].name,fuel,&eq);if(rc) return rc;if(eq) {if(*count==ordinal) *index=(pangu_u32)i;(*count)++;}
 }
 return 0;
}
static pangu_i32 provide(const pangu_plugin_context *c,const pangu_plugin_request *q,const void *table,void *state,pangu_sdk_u64 fuel) {
 pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;rc=guards(b,(void *)0,0,&fuel);if(rc) return rc;
 if(!range(SP(q,1,pangu_plugin_request,4)) || q->minimum_size!=16) return 1;
 pangu_u64 count;pangu_u32 index;rc=select(b,&q->key,q->ordinal,1,&fuel,&count,&index);if(rc) return rc;if(index==0xffffffffU) return 3;
 if(!range(SP(table,1,pangu_service_header,8))) return 1;const pangu_service_header *header=table;if(header->struct_size<16) return 9;span table_span=SP(table,header->struct_size,unsigned char,8);if(!range(table_span)) return 1;rc=bank_guard(b,&table_span,1,&fuel);if(rc) return rc;
 return b->bank->route->publish(b->bank->router,&b->record->context,index,table,state,fuel);
}
static pangu_i32 count(const pangu_plugin_context *c,const pangu_plugin_key *key,pangu_sdk_u64 fuel,pangu_plugin_count *out) {
 pangu_u64 initial=fuel;pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;span dest=SP(out,1,pangu_plugin_count,8);rc=guards(b,&dest,1,&fuel);if(rc) return rc;rc=protect(SP(key,1,pangu_plugin_key,4),&dest,1,&fuel);if(rc) return rc;
 pangu_lifecycle_setup_ticket setup;pangu_cleanup_ticket cleanup;pangu_u32 mode;if(!fuel) return 2;fuel--;rc=scope(b,&mode,&setup,&cleanup);if(rc) return rc;
 pangu_u64 n;pangu_u32 index;rc=select(b,key,0,0,&fuel,&n,&index);if(rc) return rc;if(!fuel) return 2;fuel--;*out=(pangu_plugin_count){n,initial-fuel};return 0;
}
static pangu_i32 acquire_sdk(const pangu_plugin_context *c,const pangu_plugin_request *q,pangu_sdk_u64 fuel,pangu_plugin_lease *out,pangu_plugin_report *report) {
 pangu_u64 initial=fuel;pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;span outputs[]={SP(out,1,pangu_plugin_lease,8),SP(report,1,pangu_plugin_report,8)};rc=guards(b,outputs,2,&fuel);if(rc) return rc;rc=protect(SP(q,1,pangu_plugin_request,4),outputs,2,&fuel);if(rc) return rc;if(q->minimum_size<16) return 1;
 pangu_u64 n;pangu_u32 index;rc=select(b,&q->key,q->ordinal,0,&fuel,&n,&index);if(rc) return rc;if(index==0xffffffffU) return 3;
 pangu_lifecycle_setup_ticket setup;pangu_cleanup_ticket cleanup;pangu_u32 mode;if(!fuel) return 2;fuel--;rc=scope(b,&mode,&setup,&cleanup);if(rc) return rc;
 pangu_service_query query;copy(query.service,q->key.name,32);query.version=q->key.version;query.minimum_size=q->minimum_size;query.binding=index;query.consumer=b->record->context.instance_id;query.mode=mode;query.reserved=0;query.scope=mode==1 ? (void *)&setup : mode==2 ? (void *)&cleanup : (void *)0;
 pangu_service_lease lease;pangu_lifecycle_report result;/* A pure router refusal leaves report unpublished. */
 result.reserved=0xa5a5a5a5U;rc=b->bank->route->acquire(b->bank->router,&query,fuel,&lease,&result);
 if(result.reserved==0) {result.work+=initial-fuel;copy(report,&result,sizeof result);}if(!rc) copy(out,&lease,sizeof lease);return rc;
}
static pangu_i32 release_sdk(const pangu_plugin_context *c,const pangu_plugin_lease *in,pangu_sdk_u64 fuel) {
 pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;rc=guards(b,(void *)0,0,&fuel);if(rc) return rc;if(!range(SP(in,1,pangu_plugin_lease,8))) return 1;pangu_service_lease lease;copy(&lease,in,sizeof lease);
 if(lease.ticket.consumer.owner!=b->record->context.owner || lease.ticket.consumer.epoch!=b->epoch) return 10;
 if(!fuel) return 2;fuel--;return b->bank->route->release(b->bank->router,&lease);
}
static pangu_i32 defer_sdk(const pangu_plugin_context *c,pangu_sdk_u64 fuel,pangu_plugin_completion *out) {
 pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;span dest=SP(out,1,pangu_plugin_completion,8);rc=guards(b,&dest,1,&fuel);if(rc) return rc;pangu_lifecycle_setup_ticket ticket;
 if(fuel<2) return 2;fuel-=2;rc=b->bank->router->host->setup_scope(b->bank->router->runtime,&ticket);if(rc) return rc;if(ticket.operation.owner!=b->record->context.owner || ticket.operation.epoch!=b->epoch) return 10;
 rc=b->bank->router->host->setup_defer(b->bank->router->runtime,&ticket);if(!rc) copy(out,&ticket,sizeof ticket);return rc;
}
static pangu_i32 finish_setup(const pangu_plugin_context *c,const pangu_plugin_completion *in,pangu_sdk_i32 status,pangu_sdk_u64 fuel) {
 pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;rc=guards(b,(void *)0,0,&fuel);if(rc) return rc;if(!range(SP(in,1,pangu_plugin_completion,8))) return 1;pangu_lifecycle_setup_ticket ticket;copy(&ticket,in,sizeof ticket);if(ticket.runtime!=b->bank->router->runtime || ticket.operation.owner!=b->record->context.owner || ticket.operation.epoch!=b->epoch) return 10;
 if(!fuel) return 2;fuel--;return b->bank->router->host->setup_finish(b->bank->router->runtime,&ticket,status);
}
#define CLEANUP_TAG 0x50414e4755000001ULL
static pangu_i32 finish_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *in,pangu_sdk_i32 status,pangu_sdk_u64 fuel) {
 pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;rc=guards(b,(void *)0,0,&fuel);if(rc) return rc;if(!range(SP(in,1,pangu_plugin_completion,8))) return 1;pangu_cleanup_ticket ticket;copy(&ticket,in,sizeof ticket);if(in->opaque[3]!=CLEANUP_TAG || ticket.owner!=b->record->context.owner || ticket.epoch!=b->epoch) return 10;
 if(!fuel) return 2;fuel--;return b->bank->router->host->disposer_finish(b->bank->router->runtime,b->record->context.position,&ticket,status);
}
static pangu_i32 dispose_sync(void *opaque) {pangu_plugin_binding *b=opaque;return b->plugin.dispose(&b->context,(void *)0);}
static pangu_i32 dispose_async(void *opaque,const pangu_cleanup_ticket *ticket) {pangu_plugin_binding *b=opaque;pangu_plugin_completion token={{0,0,0,CLEANUP_TAG}};copy(&token,ticket,sizeof(*ticket));return b->plugin.dispose(&b->context,&token);}
static pangu_i32 bind(pangu_plugin_bank *bank,const pangu_service_router *v,const pangu_service_api *route,pangu_plugin_binding *bindings,pangu_u64 n,const pangu_plugin_definition *plugins,void *const *userdata,pangu_u64 fuel) {
 if(!route_valid(route) || !range(SP(v,1,pangu_service_router,8))) return 1;span outputs[]={SP(bank,1,pangu_plugin_bank,8),SP(bindings,n,pangu_plugin_binding,8)};pangu_i32 rc=geometry(v,outputs,2,0,&fuel);if(rc) return rc;
 span inputs[]={SP(route,1,pangu_service_api,8),SP(plugins,n,pangu_plugin_definition,8),SP(userdata,userdata ? n : 0,void *,8),SP(&client_api,1,pangu_plugin_sdk,8),SP(&host_api,1,pangu_plugin_host_api,8)};
 for(pangu_u32 i=0;i<sizeof inputs/sizeof inputs[0];i++) {rc=protect(inputs[i],outputs,2,&fuel);if(rc) return rc;}
 if(n!=v->plan->lowered.graph.instance_count || v->runtime->phase || v->runtime->busy || v->runtime->started || v->runtime->stop_requested) return 10;
 for(pangu_u64 i=0;i<n;i++) {if(!fuel) return 2;fuel--;const pangu_entry_context *e=&v->records[i].context;if(!plugins[i].entry || plugins[i].cleanup_mode>1 || plugins[i].reserved || (!plugins[i].dispose && plugins[i].cleanup_mode) || e->instance_id!=i || e->userdata!=&bindings[i] || e->owner_epoch!=e->owner->epoch || e->owner->state!=1 || v->records[i].entered) return 1;}
 for(pangu_u64 i=0;i<v->slot_count;i++) {if(!fuel) return 2;fuel--;if(v->slots[i].published) return 10;}
 if(fuel<n+1) return 2;
 *bank=(pangu_plugin_bank){v,route,bindings,n};for(pangu_u64 i=0;i<n;i++) {pangu_plugin_context c={1,32,&client_api,&bindings[i],userdata ? userdata[i] : (void *)0};bindings[i]=(pangu_plugin_binding){c,bank,&v->records[i],v->records[i].context.owner_epoch,plugins[i],0,0};}return 0;
}
static pangu_i32 enter(const pangu_entry_context *e,pangu_u64 fuel) {
 if(!range(SP(e,1,pangu_entry_context,8)) || !range(SP(e->userdata,1,pangu_plugin_binding,8))) return 1;pangu_plugin_binding *b=e->userdata,*check;pangu_i32 rc=identity(&b->context,&check,0);if(rc) return rc;if(e!=&b->record->context || b->entered || !b->record->entered) return 10;
 rc=guards(b,(void *)0,0,&fuel);if(rc) return rc;pangu_lifecycle_runtime *r=b->bank->router->runtime;
 if(e->abi_version!=1 || e->struct_size!=64 || e->reserved || r->phase!=1 || !r->callback_open || r->stop_requested || e->position>=r->launched || r->items[e->position].context!=b->record || r->items[e->position].owner!=e->owner || e->owner->state!=1) return 10;
 if(fuel<(b->plugin.dispose ? 2U : 1U)) return 2;b->entered=1;
 if(b->plugin.dispose) {rc=b->plugin.cleanup_mode ? b->bank->router->host->register_async(r,e->position,dispose_async,b) : b->bank->router->host->register_resource(r,e->position,dispose_sync,b);if(rc) return rc;}
 if(r->stop_requested || r->phase!=1 || !r->callback_open || e->owner->epoch!=b->epoch || e->owner->state!=1) return 6;
 return b->plugin.entry(&b->context);
}
static pangu_i32 count2(const pangu_plugin_context *c,const pangu_plugin_key *key,pangu_sdk_u64 fuel,pangu_plugin_count *out) {
 pangu_u64 initial=fuel;pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;if(c->abi_version!=2) return 10;span dest=SP(out,1,pangu_plugin_count,8);rc=guards(b,&dest,1,&fuel);if(rc) return rc;rc=protect(SP(key,1,pangu_plugin_key,4),&dest,1,&fuel);if(rc) return rc;
 pangu_u32 state=b->record->context.owner->state;if(state<1 || state>3) return 10;pangu_u64 n;pangu_u32 index;rc=select(b,key,0,0,&fuel,&n,&index);if(rc) return rc;if(!fuel) return 2;fuel--;*out=(pangu_plugin_count){n,initial-fuel};return 0;
}
static pangu_i32 acquire2(const pangu_plugin_context *c,const pangu_plugin_lease *parent,const pangu_plugin_request *q,pangu_sdk_u64 fuel,pangu_plugin_lease *out,pangu_plugin_report *report) {
 pangu_u64 initial=fuel;pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;if(c->abi_version!=2) return 10;span outputs[]={SP(out,1,pangu_plugin_lease,8),SP(report,1,pangu_plugin_report,8)};rc=guards(b,outputs,2,&fuel);if(rc) return rc;rc=protect(SP(q,1,pangu_plugin_request,4),outputs,2,&fuel);if(rc) return rc;if(q->minimum_size<16) return 1;
 if(parent) {rc=protect(SP(parent,1,pangu_plugin_lease,8),outputs,2,&fuel);if(rc) return rc;}
 pangu_u64 n;pangu_u32 index;rc=select(b,&q->key,q->ordinal,0,&fuel,&n,&index);if(rc) return rc;if(index==0xffffffffU) return 3;
 pangu_lifecycle_setup_ticket setup;pangu_cleanup_ticket cleanup;pangu_u32 mode=0;
 if(!parent) {if(!fuel) return 2;fuel--;rc=scope(b,&mode,&setup,&cleanup);if(rc) return rc;}
 pangu_service_query query;copy(query.service,q->key.name,32);query.version=q->key.version;query.minimum_size=q->minimum_size;query.binding=index;query.consumer=b->record->context.instance_id;query.mode=mode;query.reserved=0;query.scope=mode==1 ? (void *)&setup : mode==2 ? (void *)&cleanup : (void *)0;
 pangu_plugin_bank2 *v=(void *)b->bank;pangu_plugin_lease lease;pangu_lifecycle_report result;result.reserved=0xa5a5a5a5U;
 rc=parent ? v->calls_api->base->open_child(&v->calls,parent,query.consumer,&query,fuel,&lease,&result) : v->calls_api->base->open_root(&v->calls,&query,fuel,&lease,&result);
 if(result.reserved==0) {result.work+=initial-fuel;copy(report,&result,sizeof result);}if(!rc) copy(out,&lease,sizeof lease);return rc;
}
static pangu_i32 acquire_root2(const pangu_plugin_context *c,const pangu_plugin_request *q,pangu_sdk_u64 fuel,pangu_plugin_lease *out,pangu_plugin_report *report) {return acquire2(c,(void *)0,q,fuel,out,report);}
static pangu_i32 acquire_child2(const pangu_plugin_context *c,const pangu_plugin_lease *parent,const pangu_plugin_request *q,pangu_sdk_u64 fuel,pangu_plugin_lease *out,pangu_plugin_report *report) {if(!parent) return 1;return acquire2(c,parent,q,fuel,out,report);}
static pangu_i32 release2(const pangu_plugin_context *c,const pangu_plugin_lease *lease,pangu_sdk_u64 fuel) {
 pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;if(c->abi_version!=2) return 10;rc=guards(b,(void *)0,0,&fuel);if(rc) return rc;pangu_plugin_bank2 *v=(void *)b->bank;return v->calls_api->base->close(&v->calls,b->record->context.instance_id,lease,fuel);
}
static pangu_i32 accept2(const pangu_plugin_context *c,const pangu_plugin_lease *lease,pangu_sdk_u64 fuel) {
 pangu_plugin_binding *b;pangu_i32 rc=identity(c,&b,1);if(rc) return rc;if(c->abi_version!=2) return 10;rc=guards(b,(void *)0,0,&fuel);if(rc) return rc;pangu_plugin_bank2 *v=(void *)b->bank;return v->calls_api->accept(&v->calls,b->record->context.instance_id,lease,fuel);
}
static pangu_i32 bind2(pangu_plugin_bank2 *bank,const pangu_service_router *v,const pangu_service_api *route,const pangu_invocation_api2 *calls,const pangu_lifecycle_child_api *child,pangu_plugin_binding *bindings,pangu_u64 n,const pangu_plugin_definition *plugins,void *const *userdata,pangu_invocation_slot *slots,pangu_u64 capacity,pangu_u64 epoch,pangu_u64 fuel) {
 if(!route_valid(route) || !range(SP(v,1,pangu_service_router,8)) || !range(SP(calls,1,pangu_invocation_api2,8)) || calls->abi_version!=2 || calls->struct_size!=40 || calls->target!=1 || calls->reserved || !calls->prepare_bind || !calls->accept || !range(SP(calls->base,1,pangu_invocation_api,8)) || calls->base->abi_version!=1 || calls->base->struct_size!=48 || calls->base->target!=1 || calls->base->reserved || !calls->base->bind || !calls->base->open_root || !calls->base->open_child || !calls->base->close) return 1;
 span outputs[]={SP(bank,1,pangu_plugin_bank2,8),SP(bindings,n,pangu_plugin_binding,8),SP(slots,capacity,pangu_invocation_slot,8)};pangu_i32 rc=geometry(v,outputs,3,0,&fuel);if(rc) return rc;
 span inputs[]={SP(route,1,pangu_service_api,8),SP(plugins,n,pangu_plugin_definition,8),SP(userdata,userdata ? n : 0,void *,8),SP(calls,1,pangu_invocation_api2,8),SP(calls->base,1,pangu_invocation_api,8),SP(child,1,pangu_lifecycle_child_api,8),SP(&client_api,1,pangu_plugin_sdk,8),SP(&host_api,1,pangu_plugin_host_api,8),SP(&client_api2,1,pangu_plugin_sdk2,8),SP(&host_api2,1,pangu_plugin_host_api2,8)};
 for(pangu_u32 i=0;i<sizeof inputs/sizeof inputs[0];i++) {rc=protect(inputs[i],outputs,3,&fuel);if(rc) return rc;}
 if(n!=v->plan->lowered.graph.instance_count || v->runtime->phase || v->runtime->busy || v->runtime->started || v->runtime->stop_requested) return 10;
 for(pangu_u64 i=0;i<n;i++) {if(!fuel) return 2;fuel--;const pangu_entry_context *e=&v->records[i].context;if(!plugins[i].entry || plugins[i].cleanup_mode>1 || plugins[i].reserved || (!plugins[i].dispose && plugins[i].cleanup_mode) || e->instance_id!=i || e->userdata!=&bindings[i] || e->owner_epoch!=e->owner->epoch || e->owner->state!=1 || v->records[i].entered) return 1;}
 for(pangu_u64 i=0;i<v->slot_count;i++) {if(!fuel) return 2;fuel--;if(v->slots[i].published) return 10;}
 pangu_invocation_bind_plan plan;rc=calls->prepare_bind(&bank->calls,v,route,child,slots,capacity,epoch,fuel,&plan);if(rc) return rc;if(plan.work>fuel) return 10;fuel-=plan.work;if(plan.bind_fuel>fuel || n+1>fuel-plan.bind_fuel) return 2;
 /* prepare_bind and bind are a pure/atomic matched provider contract. All output
  * geometry/descriptors and total fuel are fixed before the first publication. */
 rc=calls->base->bind(&bank->calls,v,route,child,slots,capacity,epoch,plan.bind_fuel);if(rc) return rc;
 bank->base=(pangu_plugin_bank){v,route,bindings,n};bank->calls_api=calls;
 for(pangu_u64 i=0;i<n;i++) {pangu_plugin_context c={2,32,&client_api2.base,&bindings[i],userdata ? userdata[i] : (void *)0};bindings[i]=(pangu_plugin_binding){c,&bank->base,&v->records[i],v->records[i].context.owner_epoch,plugins[i],0,0};}return 0;
}
static pangu_i32 enter_version(const pangu_entry_context *e,pangu_u64 fuel,pangu_u32 version) {
 if(!range(SP(e,1,pangu_entry_context,8)) || !range(SP(e->userdata,1,pangu_plugin_binding,8))) return 1;const pangu_plugin_binding *b=e->userdata;
 const pangu_plugin_sdk *expected=version==1 ? &client_api : &client_api2.base;
 if(b->context.abi_version!=version || b->context.struct_size!=32 || b->context.host!=expected) return version==1 ? 1 : 10;
 return enter(e,fuel);
}
static pangu_i32 enter1(const pangu_entry_context *e,pangu_u64 fuel) {return enter_version(e,fuel,1);}
static pangu_i32 enter2(const pangu_entry_context *e,pangu_u64 fuel) {return enter_version(e,fuel,2);}
static const pangu_plugin_sdk client_api={1,72,1,0,provide,count,acquire_sdk,release_sdk,defer_sdk,finish_setup,finish_dispose};
static const pangu_plugin_host_api host_api={1,40,1,0,&client_api,bind,enter1};
static const pangu_plugin_sdk2 client_api2={{2,88,1,0,provide,count2,acquire_root2,release2,defer_sdk,finish_setup,finish_dispose},acquire_child2,accept2};
static const pangu_plugin_host_api2 host_api2={2,48,1,0,&host_api,&client_api2,bind2,enter2};
const pangu_plugin_host_api *pangu_plugin_host_query(pangu_u32 abi,pangu_u32 size,pangu_u32 target,pangu_u32 flags) {if(abi==2 && size==48 && target==1 && !flags) return (const pangu_plugin_host_api *)(const void *)&host_api2;return abi==1 && size==40 && target==1 && !flags ? &host_api : (void *)0;}
