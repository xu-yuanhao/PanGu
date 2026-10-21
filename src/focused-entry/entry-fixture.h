#include "src/t14-abi/client-entry-v1.h"
#include "src/focused-test-support/assert.h"
#include "src/focused-test-support/stdio.h"
#include "src/focused-test-support/string.h"
#include "plan-fixture.h"
extern __attribute__((visibility("hidden"))) pangu_i32 pangu_test_asm_entry(void *);
static int assembly_mode,assembly_entries;
static const pangu_lifecycle_api *life,*real_life;static pangu_lifecycle_api shim;static int scope_injecting,scope_reentrant_refusals,failed_plan_refusals;static const pangu_entry_api *bridge;
static pangu_lifecycle_runtime run;static pangu_owner owners[9];static pangu_owned_resource resources[9][2];static pangu_owned_operation operations[9][3];
static pangu_entry_record records[9];static pangu_lifecycle_item sources[9],items[9];static pangu_entry_instance instances_in[9];static pangu_u32 module_scratch[9],disposed[9];static pangu_lifecycle_edge stop_edges[1000];static pangu_lifecycle_span protected_regions[64];
static pangu_static_entry compiled_entries[9];static pangu_installed_contract_set compiled_sets[9];static pangu_installed_table compiled;
static const pangu_prepare_result *current_plan;static const pangu_graph_output *current_graph;static pangu_lifecycle_setup_ticket pending_setup;static pangu_cleanup_ticket pending_cleanup;static pangu_u32 cleanup_pos;
typedef struct {pangu_u32 instance,handler;} payload;static payload payloads[9];
static int selected,mode,entries_called,cleanups,dependency_probes,cases,quota_refusals,metadata_refusals,alias_refusals,early_refusals,reentrant_refusals;
static pangu_u32 consumer(pangu_u32 b) {return current_plan->lowered.graph.requirements[current_graph->bindings[b].requirement].consumer;}
static pangu_u32 provider(pangu_u32 b) {return current_plan->lowered.graph.provider_owners[current_graph->bindings[b].provider];}
static pangu_i32 scope_with_cancel(pangu_lifecycle_runtime *r,pangu_lifecycle_setup_ticket *scope) {
 pangu_i32 rc=real_life->setup_scope(r,scope);
 if(!rc && mode==6 && selected>=0 && r->setup_operation.owner==&owners[selected]) {scope->operation.generation++;return 0;}
 if(!rc && !scope_injecting && selected>=0 && r->setup_operation.owner==&owners[selected]) {
  scope_injecting=1;pangu_lifecycle_item *item=&r->items[r->launched-1];int before=entries_called;
  assert(item->setup(item->owner,item->context)==10 && entries_called==before);scope_reentrant_refusals++;
  pangu_lifecycle_report report;int stopped=real_life->stop(r,1,&report);assert((stopped==2 || stopped==5) && r->stop_requested);scope_injecting=0;
 }return rc;
}
static pangu_i32 release(void *opaque) {
 const pangu_entry_context *c=opaque;assert(c->abi_version==1 && c->struct_size==64 && c->owner_epoch==c->owner->epoch);cleanups++;pangu_cleanup_ticket scope;assert(!c->host->cleanup_scope(c->runtime,c->position,&scope));
 for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==c->instance_id) {pangu_lifecycle_call_ticket call;pangu_lifecycle_report report;assert(!c->host->cleanup_call(c->runtime,b,&scope,100,&call,&report));assert(owners[provider(b)].state==2);dependency_probes++;assert(!c->host->call_finish(c->runtime,&call));}
 assert(c->host->register_resource(c->runtime,c->position,release,opaque)==10);return 0;
}
static pangu_i32 async_release(void *opaque,const pangu_cleanup_ticket *ticket) {const pangu_entry_context *c=opaque;pending_cleanup=*ticket;cleanup_pos=c->position;return 5;}
static pangu_i32 plugin(void *opaque,pangu_u32 handler) {
 const pangu_entry_context *c=opaque;const payload *data=c->userdata;assert(c->abi_version==1 && c->struct_size==64 && !c->reserved && c->host==life && c->runtime==&run && c->owner==&owners[c->instance_id] && c->owner_epoch==1 && data->instance==c->instance_id && data->handler==handler);entries_called++;int before=entries_called;assert(c->runtime->items[c->position].setup(c->owner,c->runtime->items[c->position].context)==10 && entries_called==before);reentrant_refusals++;
 assert(!c->host->register_resource(c->runtime,c->position,release,(void *)c));pangu_lifecycle_setup_ticket scope;assert(!c->host->setup_scope(c->runtime,&scope));
 for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==c->instance_id) {pangu_lifecycle_call_ticket call;pangu_lifecycle_report report;assert(!c->host->setup_call(c->runtime,b,&scope,100,&call,&report));assert(owners[provider(b)].state==2);dependency_probes++;assert(!c->host->call_finish(c->runtime,&call));}
 if((int)c->instance_id==selected) {if(mode==1 || mode==7 || mode==8) {assert(!c->host->setup_defer(c->runtime,&pending_setup));if(mode==8) assert(!c->host->setup_finish(c->runtime,&pending_setup,0));return 5;}if(mode==9) return 5;if(mode==2) return -61;if(mode==3) {assert(!c->host->register_async(c->runtime,c->position,async_release,(void *)c));return -61;}}
 return 0;
}
pangu_i32 entry_asm_dispatch(void *p) {assembly_entries++;return plugin(p,0);}
#define HANDLER(n) static pangu_i32 entry##n(void *p) {return plugin(p,n);}
HANDLER(0) HANDLER(1) HANDLER(2) HANDLER(3) HANDLER(4) HANDLER(5) HANDLER(6) HANDLER(7) HANDLER(8)
static pangu_static_entry_fn handlers[]={entry0,entry1,entry2,entry3,entry4,entry5,entry6,entry7,entry8};
static void install(const pangu_installed_table *source,int reverse) {
 assert(source->count<=9);compiled=(pangu_installed_table){compiled_entries,compiled_sets,source->count};
 for(pangu_u64 i=0;i<source->count;i++) {pangu_u64 from=reverse ? source->count-i-1 : i;compiled_entries[i]=source->entries[from];compiled_entries[i].entry=assembly_mode && from==0 ? pangu_test_asm_entry : handlers[from];compiled_sets[i]=source->sets[from];}
}
static int entry_poison(const void *p,pangu_u64 bytes) {const unsigned char *b=p;for(pangu_u64 i=0;i<bytes;i++) if(b[i]!=0xa5) return 0;return 1;}
typedef struct {const void *p;pangu_u64 size;} protected_source;
typedef struct {protected_source spans[96];pangu_u64 count,used;unsigned char bytes[65536];} entry_snapshot;
static void save_span(entry_snapshot *snap,const void *data,pangu_u64 bytes) {
 if(!bytes) return;assert(snap->count<96 && bytes<=sizeof(snap->bytes)-snap->used);snap->spans[snap->count++]=(protected_source){data,bytes};memcpy(snap->bytes+snap->used,data,bytes);snap->used+=bytes;
}
static void capture_inputs(entry_snapshot *snap,const pangu_entry_request *q,const pangu_entry_storage *storage) {
 snap->count=snap->used=0;const pangu_static_manifest *m=q->manifest;const pangu_graph_output *g=q->graph;const pangu_plan_registry *v=&q->plan->lowered.registry;const pangu_graph_input *in=&q->plan->lowered.graph;
 #define SAVE(ptr,n,type) save_span(snap,ptr,(n)*sizeof(type))
 SAVE(q,1,pangu_entry_request);SAVE(storage,1,pangu_entry_storage);SAVE(m,1,pangu_static_manifest);SAVE(q->installed,1,pangu_installed_table);SAVE(q->entry_map,q->entry_capacity,pangu_u32);SAVE(q->plan,1,pangu_prepare_result);SAVE(g,1,pangu_graph_output);SAVE(q->host,1,pangu_lifecycle_api);SAVE(q->runtime,1,pangu_lifecycle_runtime);SAVE(q->instances,q->instance_count,pangu_entry_instance);SAVE(payloads,9,payload);
 SAVE(m->scopes,m->scope_count,pangu_plan_scope);SAVE(m->definitions,m->definition_count,pangu_manifest_definition);SAVE(m->modules,m->module_count,pangu_manifest_module);SAVE(m->instances,m->instance_count,pangu_manifest_instance);SAVE(m->bindings,m->binding_count,pangu_manifest_binding);SAVE(m->provisions,m->provision_count,pangu_manifest_provision);SAVE(m->requirements,m->requirement_count,pangu_manifest_requirement);
 SAVE(q->installed->entries,q->installed->count,pangu_static_entry);SAVE(q->installed->sets,q->installed->count,pangu_installed_contract_set);for(pangu_u64 i=0;i<q->installed->count;i++) SAVE(q->installed->sets[i].records,q->installed->sets[i].count,pangu_module_contract);
 SAVE(v->scopes,v->scope_count,pangu_plan_scope);SAVE(v->services,v->service_count,pangu_plan_service);SAVE(v->bindings,v->binding_count,pangu_plan_binding);SAVE(v->providers,v->provider_count,pangu_plan_provider);SAVE(in->instances,in->instance_count,pangu_plan_instance);SAVE(in->provider_owners,v->provider_count,pangu_u32);SAVE(in->requirements,in->requirement_count,pangu_plan_requirement);SAVE(g->order,g->order_capacity,pangu_u32);SAVE(g->states,g->state_capacity,pangu_u32);SAVE(g->bindings,g->binding_capacity,pangu_plan_edge);
 for(pangu_u64 i=0;i<q->instance_count;i++) {pangu_owner *o=q->instances[i].owner;SAVE(o,1,pangu_owner);SAVE(o->resources,o->capacity,pangu_owned_resource);SAVE(o->operations,o->operation_capacity,pangu_owned_operation);}
 #undef SAVE
}
static int inputs_unchanged(const entry_snapshot *snap) {
 pangu_u64 offset=0;for(pangu_u64 i=0;i<snap->count;i++) {if(memcmp(snap->bytes+offset,snap->spans[i].p,snap->spans[i].size)) return 0;offset+=snap->spans[i].size;}return 1;
}
static void bridge_aliases(const pangu_entry_request *q,pangu_entry_storage *storage,pangu_entry_prepared *info) {
 const pangu_static_manifest *m=q->manifest;const pangu_graph_output *g=q->graph;
 protected_source src[]={
 {q,sizeof(*q)},{m,sizeof(*m)},{q->installed,sizeof(*q->installed)},{q->entry_map,q->entry_capacity*4},{q->plan,sizeof(*q->plan)},{g,sizeof(*g)},{q->host,sizeof(*q->host)},{q->runtime,sizeof(*q->runtime)},{q->instances,q->instance_count*sizeof(pangu_entry_instance)},
 {m->modules,m->module_count*sizeof(pangu_manifest_module)},{m->instances,m->instance_count*sizeof(pangu_manifest_instance)},{g->order,g->order_capacity*4},{g->states,g->state_capacity*4},{g->bindings,g->binding_capacity*sizeof(pangu_plan_edge)},
 {q->installed->entries,q->installed->count*sizeof(pangu_static_entry)},{q->installed->sets,q->installed->count*sizeof(pangu_installed_contract_set)}};
 for(pangu_u64 k=0;k<sizeof(src)/sizeof(src[0]);k++) for(int which=0;which<4;which++) {
  pangu_entry_storage bad=*storage;pangu_entry_prepared *result=info;void *target=(void *)src[k].p;
  if(which==0) bad.records=target;if(which==1) bad.items=target;if(which==2) bad.module_scratch=target;if(which==3) result=target;
  unsigned char before[8192];assert(src[k].size<=sizeof before);if(src[k].size) memcpy(before,src[k].p,src[k].size);
  memset(records,0xa5,sizeof records);memset(sources,0xa5,sizeof sources);memset(module_scratch,0xa5,sizeof module_scratch);memset(info,0xa5,sizeof(*info));
  assert(bridge->prepare(q,&bad,100000,result)==1 && !entries_called && entry_poison(records,sizeof records) && entry_poison(sources,sizeof sources) && entry_poison(module_scratch,sizeof module_scratch) && entry_poison(info,sizeof(*info)));
  assert(!src[k].size || !memcmp(before,src[k].p,src[k].size));alias_refusals++;
 }
 if(q->instance_count) {
  pangu_owner *owner=q->instances[0].owner;protected_source nodes[]={{owner,sizeof(*owner)},{owner->resources,owner->capacity*sizeof(pangu_owned_resource)},{owner->operations,owner->operation_capacity*sizeof(pangu_owned_operation)}};
  for(int k=0;k<3;k++) {pangu_entry_storage bad=*storage;bad.records=(void *)nodes[k].p;unsigned char before[256];assert(nodes[k].size<=sizeof before);memcpy(before,nodes[k].p,nodes[k].size);assert(bridge->prepare(q,&bad,100000,info)==1 && !memcmp(before,nodes[k].p,nodes[k].size) && !entries_called);alias_refusals++;}
 }
 pangu_entry_storage bad=*storage;bad.records=(void *)&bad;pangu_entry_storage copy=bad;assert(bridge->prepare(q,&bad,100000,info)==1 && !memcmp(&bad,&copy,sizeof bad));alias_refusals++;
 bad=*storage;bad.items=(void *)bad.records;assert(bridge->prepare(q,&bad,100000,info)==1);alias_refusals++;
 assert(!bridge->prepare(q,storage,100000,info));
}
static void drive(const pangu_static_manifest *manifest,const pangu_prepare_result *plan,const pangu_graph_output *graph,const pangu_u32 *entry_map,int target,int kind,pangu_u64 quantum,int quota) {
 life=real_life;if(kind==5 || kind==6) {shim=*real_life;shim.setup_scope=scope_with_cancel;life=&shim;}
 memset(&run,0,sizeof run);memset(disposed,0,sizeof disposed);entries_called=cleanups=0;selected=target<0 ? -1 : (int)graph->order[target];mode=kind;current_plan=plan;current_graph=graph;
 pangu_u64 n=manifest->instance_count;
 for(pangu_u64 i=0;i<n;i++) {memset(&owners[i],0,sizeof owners[i]);assert(!life->owner_init(&owners[i],resources[i],2,operations[i],3,1,4));payloads[i].instance=(pangu_u32)i;payloads[i].handler=99;
  for(pangu_u64 j=0;j<compiled.count;j++) if(compiled_entries[j].module.version==manifest->instances[i].version && !memcmp(compiled_entries[j].module.name,manifest->instances[i].module,32)) for(pangu_u32 h=0;h<9;h++) if(compiled_entries[j].entry==handlers[h]) payloads[i].handler=h;
  for(pangu_u64 j=0;j<compiled.count;j++) if(compiled_entries[j].module.version==manifest->instances[i].version && !memcmp(compiled_entries[j].module.name,manifest->instances[i].module,32) && compiled_entries[j].entry==pangu_test_asm_entry) payloads[i].handler=0;
  assert(payloads[i].handler!=99);instances_in[i]=(pangu_entry_instance){&owners[i],&payloads[i]};
 }
 pangu_entry_request request={manifest,&compiled,entry_map,manifest->module_count,plan,graph,life,&run,instances_in,n};pangu_entry_storage storage={records,9,sources,9,module_scratch,9};pangu_entry_prepared info;
 if(quota) {entry_snapshot original;capture_inputs(&original,&request,&storage);
  for(pangu_u64 fuel=1;fuel<100000;fuel++) {memset(records,0xa5,sizeof records);memset(sources,0xa5,sizeof sources);memset(&info,0xa5,sizeof info);int rc=bridge->prepare(&request,&storage,fuel,&info);if(!rc) {assert(info.work==fuel && inputs_unchanged(&original));break;}assert(inputs_unchanged(&original));assert(rc==2 && entry_poison(records,sizeof records) && entry_poison(sources,sizeof sources) && entry_poison(&info,sizeof info) && !entries_called);quota_refusals++;assert(fuel<99999);}
 } else assert(!bridge->prepare(&request,&storage,100000,&info));
 assert(info.instance_count==n && !entries_called && !callback_calls);if(quota) bridge_aliases(&request,&storage,&info);
 for(pangu_u64 i=0;i<n;i++) {assert((records[i].entry==handlers[payloads[i].handler] || (payloads[i].handler==0 && records[i].entry==pangu_test_asm_entry)) && records[i].context.instance_id==i && records[i].context.owner==&owners[i]);if(graph->states[i]) assert(records[i].context.position==0xffffffffU);}
 /* Wrong table metadata/map refuses before publishing records or executing code. */
 if(quota && manifest->module_count>1) {
  pangu_u32 wrong[9];memcpy(wrong,entry_map,manifest->module_count*4);pangu_u32 first=wrong[0];wrong[0]=wrong[1];wrong[1]=first;request.entry_map=wrong;memset(records,0xa5,sizeof records);memset(sources,0xa5,sizeof sources);memset(&info,0xa5,sizeof info);
  assert(bridge->prepare(&request,&storage,100000,&info)==16 && entry_poison(records,sizeof records) && entry_poison(sources,sizeof sources) && entry_poison(&info,sizeof info) && !entries_called);metadata_refusals++;request.entry_map=entry_map;assert(!bridge->prepare(&request,&storage,100000,&info));
 }
 for(pangu_u64 i=0;i<n;i++) {assert(sources[i].setup(&owners[i],sources[i].context)==10 && !entries_called);early_refusals++;}
 if(kind==4) records[graph->order[target]].context.owner_epoch++;
 pangu_lifecycle_storage life_storage={items,9,stop_edges,1000};pangu_lifecycle_prepared prepared;assert(!life->prepare(plan,graph,sources,n,&life_storage,100000,&prepared));pangu_lifecycle_graph stop={stop_edges,prepared.edge_count,disposed,9};run.items=items;run.count=prepared.item_count;run.stop_graph=&stop;assert(!life->bind_geometry(&run,protected_regions,64,100000));
 pangu_lifecycle_report report;int rc=life->start(&run,quantum,&report),ticks=0;
 for(;;) {assert(report.work<=quantum);if(!report.pending) break;if(report.reason==2) {if(run.setup_waiting) {if(kind==1) {pangu_lifecycle_report stopped;int sr=life->stop(&run,quantum,&stopped);assert((sr==2 || sr==5) && stopped.work<=quantum);}else assert(kind==7 && owners[selected].state==1);int before=entries_called;assert(sources[selected].setup(&owners[selected],sources[selected].context)==10 && entries_called==before);assert(!life->setup_finish(&run,&pending_setup,0));}else assert(!life->disposer_finish(&run,cleanup_pos,&pending_cleanup,-72));}else assert(report.reason==1 && rc==2);rc=life->pump(&run,quantum,&report);assert(++ticks<100000);}
 if(kind && kind!=7 && kind!=8) {assert(rc==((kind==1 || kind==5) ? 6 : (kind==4 || kind==6) ? 10 : kind==9 ? 5 : -61) && run.phase==4 && !run.started && entries_called==target+(kind!=4 && kind!=5 && kind!=6) && cleanups==target+(kind!=4 && kind!=5 && kind!=6));if(kind==3) assert(run.cleanup_error==-72);}else {assert(!rc && run.phase==2 && entries_called==(int)prepared.item_count);do {rc=life->stop(&run,quantum,&report);assert(report.work<=quantum && ++ticks<100000);}while(report.pending);assert(!rc && cleanups==(int)prepared.item_count);}
 for(pangu_u64 i=0;i<n;i++) if(graph->states[i]) assert(owners[i].state==1 && !owners[i].count);
 for(pangu_u64 i=0;i<n;i++) {int before=entries_called;assert(sources[i].setup(&owners[i],sources[i].context)==10 && entries_called==before);early_refusals++;}
 cases++;
}
int main(void) {
 real_life=life=pangu_lifecycle_query(1,152,1,0);bridge=pangu_entry_bridge_query(1,24,1,0);assert(life && bridge && bridge->abi_version==1 && bridge->struct_size==24 && bridge->target==1 && !bridge->reserved);
 assert(!pangu_entry_bridge_query(2,24,1,0) && !pangu_entry_bridge_query(1,23,1,0) && !pangu_entry_bridge_query(1,24,2,0) && !pangu_entry_bridge_query(1,24,1,1));pangu_prepare_result plan;
 for(pangu_u64 i=0;i<sizeof(host_graph_cases)/sizeof(host_graph_cases[0]);i++) {const host_graph_case *c=&host_graph_cases[i];for(int reverse=0;reverse<2;reverse++) {install(c->installed,reverse);assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,&compiled,&corpus_storage,10000000,&plan)==c->status);assert(!callback_calls);if(c->status) {
 pangu_entry_request req={c->manifest,&compiled,corpus_entry_output,9,&plan,&corpus_output,life,&run,instances_in,c->manifest->instance_count};pangu_entry_storage st={records,9,sources,9,module_scratch,9};pangu_entry_prepared result;memset(records,0xa5,sizeof records);memset(sources,0xa5,sizeof sources);memset(&result,0xa5,sizeof result);int prior=entries_called;
 assert(bridge->prepare(&req,&st,100000,&result)==10 && entries_called==prior && entry_poison(records,sizeof records) && entry_poison(sources,sizeof sources) && entry_poison(&result,sizeof result));failed_plan_refusals++;continue;}
  for(pangu_u64 q=1;q<=4;q+=3) drive(c->manifest,&plan,&corpus_output,corpus_entry_output,-1,0,q,q==1 && !reverse);
  for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++) for(int kind=1;kind<=9;kind++) drive(c->manifest,&plan,&corpus_output,corpus_entry_output,(int)pos,kind,1,0);
 }}
 for(int variant=0;variant<4;variant++) for(int reverse=0;reverse<2;reverse++) {
 install(&installed,reverse);const pangu_static_manifest *m=variant>=2 ? &multi_manifest : &manifest;
 if(variant==1) instances[2].scope=2;if(variant>=2) multi_manifest.provisions=variant==3 ? reverse_provisions : provisions;
 assert(!pangu_discover_prepare(m,source_contracts,4,&compiled,&storage,1000000,&plan));
 for(pangu_u64 q=1;q<=4;q+=3) drive(m,&plan,&output,entry_output,-1,0,q,0);
 for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++) for(int kind=1;kind<=9;kind++) drive(m,&plan,&output,entry_output,(int)pos,kind,1,0);
 if(variant==1) instances[2].scope=1;
 }
 assembly_mode=1;for(int reverse=0;reverse<2;reverse++) {install(&installed,reverse);assert(!pangu_discover_prepare(&manifest,source_contracts,4,&compiled,&storage,1000000,&plan));for(pangu_u64 q=1;q<=4;q+=3) drive(&manifest,&plan,&output,entry_output,-1,0,q,0);for(int kind=1;kind<=9;kind++) drive(&manifest,&plan,&output,entry_output,0,kind,1,0);}
 assert(assembly_entries>0);
 printf("entry_bridge_cases=%d dependency_probes=%d insufficient_fuel=%d wrong_map_refusals=%d alias_refusals=%d early_late_refusals=%d reentrant_refusals=%d scope_reentrant_refusals=%d failed_plan_refusals=%d assembly_entries=%d\n",cases,dependency_probes,quota_refusals,metadata_refusals,alias_refusals,early_refusals,reentrant_refusals,scope_reentrant_refusals,failed_plan_refusals,assembly_entries);return 0;
}
