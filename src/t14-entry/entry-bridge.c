#include "src/t14-abi/entry-bridge.h"
static int range(const void *p,pangu_u64 n,pangu_u64 size,pangu_u64 align) {
 pangu_u64 a=(pangu_u64)p;return n<=~(pangu_u64)0/size && (!n || (p && !(a&(align-1)) && n*size<=~(pangu_u64)0-a));
}
static int overlap(const void *p,pangu_u64 n,const void *q,pangu_u64 m) {
 pangu_u64 a=(pangu_u64)p,b=(pangu_u64)q;return n && m && (a<=b ? b-a<n : a-b<m);
}
static int host_valid(const pangu_lifecycle_api *api) {
 return range(api,1,sizeof(*api),8) && api->abi_version==1 && api->struct_size==152 && api->target==1 && !api->reserved && api->owner_init && api->register_resource && api->register_async && api->prepare && api->bind_geometry && api->start && api->pump && api->stop && api->setup_scope && api->setup_defer && api->setup_finish && api->cleanup_scope && api->disposer_finish && api->call && api->setup_call && api->cleanup_call && api->call_finish;
}
static pangu_i32 entry_setup(pangu_owner *owner,void *opaque) {
 pangu_entry_record *record=opaque;if(!range(record,1,sizeof(*record),8)) return 1;pangu_entry_context *c=&record->context;
 if(c->abi_version!=1 || c->struct_size!=64 || c->reserved || record->reserved || !record->entry || !host_valid(c->host) || !range(c->runtime,1,sizeof(*c->runtime),8) || c->owner!=owner) return 1;
 if(record->entered) return 10;
 pangu_lifecycle_runtime *r=c->runtime;
 if(r->phase!=1 || !r->callback_open || r->stop_requested || c->position>=r->launched || r->items[c->position].context!=record || r->items[c->position].owner!=owner || owner->epoch!=c->owner_epoch || owner->state!=PANGU_OWNER_LOADING) return 10;
 record->entered=1;
 pangu_lifecycle_setup_ticket scope;pangu_i32 rc=c->host->setup_scope(r,&scope);if(rc) return r->stop_requested ? 6 : rc;
 if(r->stop_requested || r->phase!=1 || !r->callback_open || owner->state!=PANGU_OWNER_LOADING) return 6;
 if(owner->epoch!=c->owner_epoch) return 10;
 if(scope.runtime!=r || scope.operation.owner!=owner || scope.operation.epoch!=c->owner_epoch || scope.operation.index!=r->setup_operation.index || scope.operation.generation!=r->setup_operation.generation || scope.operation.index>=owner->operation_capacity || !scope.operation.generation || !owner->operations[scope.operation.index].active || owner->operations[scope.operation.index].generation!=scope.operation.generation) return 10;
 return record->entry(c);
}
typedef struct {const void *p;pangu_u64 n,size,align;} span;
static pangu_i32 protect(span in,const span *out,pangu_u64 *fuel) {
 if(!*fuel) return 2;(*fuel)--;if(!range(in.p,in.n,in.size,in.align)) return 1;
 for(pangu_u32 i=0;i<4;i++) {if(!*fuel) return 2;(*fuel)--;if(overlap(in.p,in.n*in.size,out[i].p,out[i].n*out[i].size)) return 1;}return 0;
}
static pangu_i32 same(const void *a,const void *b,pangu_u64 n,pangu_u64 *fuel,pangu_u32 *equal) {
 const unsigned char *x=a,*y=b;*equal=0;for(pangu_u64 i=0;i<n;i++) {if(!*fuel) return 2;(*fuel)--;if(x[i]!=y[i]) return 0;}*equal=1;return 0;
}
static pangu_i32 prepare(const pangu_entry_request *q,const pangu_entry_storage *s,pangu_u64 fuel,pangu_entry_prepared *info) {
 if(!range(q,1,sizeof(*q),8) || !range(s,1,sizeof(*s),8) || !fuel) return 1;
 if(!range(q->manifest,1,sizeof(*q->manifest),8) || !range(q->installed,1,sizeof(*q->installed),8) || !range(q->plan,1,sizeof(*q->plan),8) || !range(q->graph,1,sizeof(*q->graph),8) || !range(q->runtime,1,sizeof(*q->runtime),8) || !host_valid(q->host)) return 1;
 const pangu_static_manifest *m=q->manifest;const pangu_installed_table *t=q->installed;const pangu_prepare_result *p=q->plan;const pangu_graph_output *g=q->graph;
 if(p->stage!=6 || p->reserved || p->lowered.report.status || p->graph_report.status || p->lowered.graph.registry!=&p->lowered.registry) return 10;
 pangu_u64 n=q->instance_count,ready=p->graph_report.order_count,initial=fuel;
 if(n>0xffffffffU || n!=m->instance_count || n!=p->lowered.graph.instance_count || m->module_count>0xffffffffU || t->count>0xffffffffU || ready>n || p->graph_report.pending_count!=n-ready || g->state_capacity<n || g->order_capacity<ready || q->entry_capacity<m->module_count) return 1;
 if(s->record_capacity<n || s->item_capacity<n || s->scratch_capacity<n) return 2;
 if(q->runtime->phase || q->runtime->busy || q->runtime->started || q->runtime->stop_requested || q->runtime->protected_spans) return 10;
 span out[]={{s->records,s->record_capacity,sizeof(pangu_entry_record),8},{s->items,s->item_capacity,sizeof(pangu_lifecycle_item),8},{s->module_scratch,s->scratch_capacity,4,4},{info,1,sizeof(*info),8}};
 #define STEP() do {if(!fuel) return 2;fuel--;} while(0)
 for(pangu_u32 i=0;i<4;i++) {STEP();if(!range(out[i].p,out[i].n,out[i].size,out[i].align)) return 1;for(pangu_u32 j=0;j<i;j++) {STEP();if(overlap(out[i].p,out[i].n*out[i].size,out[j].p,out[j].n*out[j].size)) return 1;}}
 const pangu_plan_registry *v=&p->lowered.registry;const pangu_graph_input *in=&p->lowered.graph;
 span inputs[]={
  {q,1,sizeof(*q),8},{s,1,sizeof(*s),8},{m,1,sizeof(*m),8},{t,1,sizeof(*t),8},{p,1,sizeof(*p),8},{g,1,sizeof(*g),8},{q->host,1,sizeof(*q->host),8},{q->runtime,1,sizeof(*q->runtime),8},
  {q->entry_map,q->entry_capacity,4,4},{q->instances,n,sizeof(pangu_entry_instance),8},
  {m->scopes,m->scope_count,sizeof(pangu_plan_scope),4},{m->definitions,m->definition_count,sizeof(pangu_manifest_definition),4},{m->modules,m->module_count,sizeof(pangu_manifest_module),4},{m->instances,m->instance_count,sizeof(pangu_manifest_instance),4},{m->bindings,m->binding_count,sizeof(pangu_manifest_binding),4},{m->provisions,m->provision_count,sizeof(pangu_manifest_provision),4},{m->requirements,m->requirement_count,sizeof(pangu_manifest_requirement),4},
  {t->entries,t->count,sizeof(pangu_static_entry),8},{t->sets,t->count,sizeof(pangu_installed_contract_set),8},
  {v->scopes,v->scope_count,sizeof(pangu_plan_scope),4},{v->services,v->service_count,sizeof(pangu_plan_service),4},{v->bindings,v->binding_count,sizeof(pangu_plan_binding),4},{v->providers,v->provider_count,sizeof(pangu_plan_provider),4},
  {in->instances,n,sizeof(pangu_plan_instance),4},{in->provider_owners,v->provider_count,4,4},{in->requirements,in->requirement_count,sizeof(pangu_plan_requirement),4},
  {g->order,g->order_capacity,4,4},{g->states,g->state_capacity,4,4},{g->bindings,g->binding_capacity,sizeof(pangu_plan_edge),4}};
 for(pangu_u64 i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++) {pangu_i32 rc=protect(inputs[i],out,&fuel);if(rc) return rc;}
 for(pangu_u64 i=0;i<t->count;i++) {span contracts={t->sets[i].records,t->sets[i].count,sizeof(pangu_module_contract),4};pangu_i32 rc=protect(contracts,out,&fuel);if(rc) return rc;}
 for(pangu_u64 i=0;i<n;i++) {
  STEP();pangu_owner *o=q->instances[i].owner;span frame={o,1,sizeof(*o),8};pangu_i32 rc=protect(frame,out,&fuel);if(rc) return rc;
  if(o->abi_version!=1 || o->struct_size!=104 || o->state!=1 || o->busy || o->count || o->inflight || !o->epoch || !o->operation_capacity || o->operation_capacity>0xffffffffU) return 1;
  span resources={o->resources,o->capacity,sizeof(pangu_owned_resource),8},operations={o->operations,o->operation_capacity,sizeof(pangu_owned_operation),4};rc=protect(resources,out,&fuel);if(rc) return rc;rc=protect(operations,out,&fuel);if(rc) return rc;
  for(pangu_u64 j=0;j<i;j++) {STEP();if(q->instances[j].owner==o) return 1;}
 }
 pangu_u32 pending=0;
 for(pangu_u64 i=0;i<n;i++) {STEP();if(g->states[i]>1) return 16;pending+=g->states[i];}
 if(pending!=p->graph_report.pending_count) return 16;
 for(pangu_u64 i=0;i<ready;i++) {STEP();if(g->order[i]>=n || g->states[g->order[i]]) return 16;for(pangu_u64 j=0;j<i;j++) {STEP();if(g->order[j]==g->order[i]) return 16;}}
 for(pangu_u64 i=0;i<m->module_count;i++) {
  STEP();pangu_u32 slot=q->entry_map[i];if(slot>=t->count || !t->entries[slot].entry) return 16;pangu_u32 eq;pangu_i32 rc=same(&m->modules[i],&t->entries[slot].module,sizeof(pangu_manifest_module),&fuel,&eq);if(rc) return rc;if(!eq) return 16;
 }
 for(pangu_u64 i=0;i<n;i++) {
  pangu_u32 found=0xffffffffU;
  for(pangu_u64 j=0;j<m->module_count;j++) {STEP();if(m->instances[i].version!=m->modules[j].version) continue;pangu_u32 eq;pangu_i32 rc=same(m->instances[i].module,m->modules[j].name,32,&fuel,&eq);if(rc) return rc;if(eq) {if(found!=0xffffffffU) return 4;found=(pangu_u32)j;}}
  if(found==0xffffffffU) return 3;STEP();s->module_scratch[i]=found;
 }
 pangu_u64 commit=n*(ready+1)+1;if(fuel<commit) return 2;fuel-=commit;
 for(pangu_u64 i=0;i<n;i++) {
  pangu_u32 position=0xffffffffU;for(pangu_u64 j=0;j<ready;j++) if(g->order[j]==i) position=(pangu_u32)j;
  pangu_entry_context context={1,64,q->host,q->runtime,q->instances[i].owner,q->instances[i].userdata,q->instances[i].owner->epoch,(pangu_u32)i,position,0};s->records[i].context=context;s->records[i].entry=t->entries[q->entry_map[s->module_scratch[i]]].entry;s->records[i].entered=0;s->records[i].reserved=0;
  s->items[i]=(pangu_lifecycle_item){q->instances[i].owner,entry_setup,&s->records[i]};
 }
 info->instance_count=n;info->work=initial-fuel;return 0;
 #undef STEP
}
static const pangu_entry_api bridge_api={PANGU_ENTRY_ABI,PANGU_ENTRY_API_SIZE,PANGU_LIFECYCLE_TARGET,0,prepare};
const pangu_entry_api *pangu_entry_bridge_query(pangu_u32 abi,pangu_u32 size,pangu_u32 target,pangu_u32 flags) {return abi==PANGU_ENTRY_ABI && size==PANGU_ENTRY_API_SIZE && target==PANGU_LIFECYCLE_TARGET && !flags ? &bridge_api : (void *)0;}
