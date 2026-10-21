#include "src/focused-interop-cpp/observer.h"
extern __attribute__((visibility("hidden"))) int pangu_route_cpp_evaluate(void *,int,int *);
extern __attribute__((visibility("hidden"))) int pangu_route_cpp_invoke(const void *,void *,unsigned int,int,int *);
extern __attribute__((visibility("hidden"))) int pangu_route_asm_preserve(const void *,void *,unsigned int,int,int *);
#include "src/t14-abi/interop-contract-v1.h"
#define PANGU_ROUTE_TEST_MAIN route_suite_main
#include "route-fixture.h"
#undef PANGU_ROUTE_TEST_MAIN
_Static_assert(sizeof(test_service)==PANGU_INTEROP_SERVICE_SIZE,"independent C service size");
_Static_assert(__builtin_offsetof(test_service,evaluate)==PANGU_INTEROP_METHOD_OFFSET,"independent C method offset");
_Static_assert(sizeof(service_state)==sizeof(pangu_interop_state),"state layout");
static int interop_cases,interop_calls,matrix[3][3],cpp_provider_calls,consumer_refusals,provider_errors,late_refusals,async_loads,interop_backend,interop_armed;
static const pangu_entry_context *interop_pending;static pangu_lifecycle_setup_ticket interop_scope;
void pangu_cpp_provider_observed(void *opaque) {assert(opaque);cpp_provider_calls++;}
static pangu_i32 c_invoke(const void *opaque,void *state,pangu_u32 version,pangu_i32 input,pangu_i32 *out) {
 if(!opaque || !out || ((pangu_u64)opaque&7)) return 1;const test_service *t=opaque;
 if(!version || t->header.version!=version || t->header.struct_size<24 || t->header.target!=1 || t->header.reserved || !t->evaluate) return 9;return t->evaluate(state,input,out);
}
static pangu_interop_i32 (*const consumers[3])(const void *,void *,pangu_interop_u32,pangu_interop_i32,pangu_interop_i32 *)={c_invoke,pangu_route_asm_preserve,pangu_route_cpp_invoke};
static void interop_invoke(const pangu_service_lease *lease,pangu_u32 binding) {
 pangu_u32 p=current_graph->bindings[binding].provider;assert(lease->table==&tables[p].header && lease->state==&service_states[p]);
 pangu_i32 inputs[]={17,-17,2147483647-service_states[p].value,(-2147483647-1)};
 for(pangu_u32 c=0;c<3;c++) for(pangu_u32 i=0;i<sizeof inputs/sizeof inputs[0];i++) {
  pangu_i32 result=0;assert(!consumers[c](lease->table,lease->state,tables[p].header.version,inputs[i],&result));long long expected=(long long)service_states[p].value+inputs[i];assert(expected>=-2147483648LL && expected<=2147483647LL && result==expected);matrix[interop_backend][c]++;interop_calls++;
 }
}
static void interop_binding(pangu_u32 b,pangu_u32 mode,const void *scope) {
 pangu_service_query q;query_for(b,mode,scope,&q);pangu_service_lease lease;pangu_lifecycle_report report;assert(!route->acquire(&router,&q,100000,&lease,&report) && report.work<=100000);interop_invoke(&lease,b);assert(!route->release(&router,&lease));assert(route->release(&router,&lease)==10);late_refusals++;
}
static pangu_i32 interop_cleanup(void *opaque) {
 const pangu_entry_context *c=opaque;pangu_cleanup_ticket scope;assert(!life->cleanup_scope(&run,c->position,&scope));for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==c->instance_id) interop_binding(b,2,&scope);cleanups++;return 0;
}
static pangu_i32 interop_load(const pangu_entry_context *c) {
 for(pangu_u32 p=0;p<router.slot_count;p++) if(current_plan->lowered.graph.provider_owners[p]==c->instance_id) assert(!route->publish(&router,c,p,&tables[p].header,&service_states[p],100000));
 assert(!life->register_resource(&run,c->position,interop_cleanup,(void *)c));pangu_lifecycle_setup_ticket scope;assert(!life->setup_scope(&run,&scope));for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==c->instance_id) interop_binding(b,1,&scope);return 0;
}
static pangu_i32 interop_entry(void *opaque) {
 const pangu_entry_context *c=opaque;entries_called++;if(interop_armed) {interop_armed=0;interop_pending=c;assert(!life->setup_defer(&run,&interop_scope));return 5;}return interop_load(c);
}
static void interop_drive(const pangu_static_manifest *m,const pangu_prepare_result *p,const pangu_graph_output *g,const pangu_u32 *map,int backend,int deferred) {
 interop_backend=backend;interop_armed=deferred;interop_pending=0;memset(&run,0,sizeof run);memset(disposed,0,sizeof disposed);memset(service_slots,0,sizeof service_slots);memset(service_states,0,sizeof service_states);entries_called=cleanups=0;current_plan=p;current_graph=g;epoch++;
 for(pangu_u32 i=0;i<m->instance_count;i++) {if(!owners[i].abi_version || owners[i].state==1) memset(&owners[i],0,sizeof owners[i]);assert(!life->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));instances_in[i]=(pangu_entry_instance){&owners[i],0};}
 for(pangu_u32 i=0;i<p->lowered.registry.provider_count;i++) {tables[i]=(test_service){{p->lowered.registry.providers[i].version,sizeof(test_service),1,0},backend==0 ? eval_a : backend==1 ? eval_b : pangu_route_cpp_evaluate};service_states[i].value=(int)p->lowered.registry.providers[i].instance*11+3;}
 pangu_entry_request req={m,&compiled,map,m->module_count,p,g,life,&run,instances_in,m->instance_count};pangu_entry_storage st={records,9,sources,9,module_scratch,9};pangu_entry_prepared ep;assert(!bridge->prepare(&req,&st,100000,&ep));pangu_lifecycle_storage ls={items,9,stop_edges,1000};pangu_lifecycle_prepared lp;assert(!life->prepare(p,g,sources,m->instance_count,&ls,100000,&lp));pangu_lifecycle_graph sg={stop_edges,lp.edge_count,disposed,9};run.items=items;run.count=lp.item_count;run.stop_graph=&sg;assert(!life->bind_geometry(&run,protected_regions,64,100000));router=(pangu_service_router){life,&run,m,p,g,records,service_slots,p->lowered.registry.provider_count};
 pangu_lifecycle_report report;int rc=life->start(&run,1,&report),ticks=0;
 while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2) {assert(deferred && interop_pending && !run.callback_open);assert(!interop_load(interop_pending));assert(!life->setup_finish(&run,&interop_scope,0));interop_pending=0;async_loads++;}else assert(report.reason==1);rc=life->pump(&run,1,&report);}assert(!rc && entries_called==(int)run.count);
 for(pangu_u32 b=0;b<p->graph_report.binding_count;b++) interop_binding(b,0,0);
 pangu_service_lease live;int retained=p->graph_report.binding_count!=0;if(retained) {pangu_service_query q;query_for(0,0,0,&q);assert(!route->acquire(&router,&q,100000,&live,&report));}
 rc=life->stop(&run,1,&report);ticks=0;while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2) {assert(retained && owners[consumer(0)].state==3 && owners[provider(0)].state==2);interop_invoke(&live,0);assert(!route->release(&router,&live));retained=0;}else assert(report.reason==1);rc=life->stop(&run,1,&report);}assert(!rc && !retained && cleanups==entries_called);
 for(pangu_u32 b=0;b<p->graph_report.binding_count;b++) {pangu_service_query q;query_for(b,0,0,&q);pangu_service_lease lease;memset(&lease,0xa5,sizeof lease);assert(route->acquire(&router,&q,100000,&lease,&report)==10 && entry_poison(&lease,sizeof lease));late_refusals++;}interop_cases++;
}
static pangu_i32 failure_method(void *state,pangu_i32 input,pangu_i32 *out) {(void)state;(void)input;(void)out;return -92;}
static void consumer_boundaries(void) {
 service_state state={3,0};test_service table={{7,24,1,0},eval_a};pangu_i32 out;
 for(pangu_u32 c=0;c<3;c++) {
  for(int kind=0;kind<7;kind++) {test_service bad=table;if(kind==0) bad.header.version=8;if(kind==1) bad.header.struct_size=23;if(kind==2) bad.header.target=2;if(kind==3) bad.header.reserved=1;if(kind==4) bad.evaluate=0;if(kind==5) bad.header.version=0;out=0x12345678;int before=state.calls;assert(consumers[c](&bad,&state,kind==5 ? 0 : kind==6 ? 8 : 7,17,&out)==9 && out==0x12345678 && state.calls==before);consumer_refusals++;}
  out=0x12345678;assert(consumers[c](0,&state,7,17,&out)==1 && out==0x12345678);assert(consumers[c](&table,&state,7,17,0)==1);_Alignas(8) unsigned char unaligned[32];assert(consumers[c](unaligned+1,&state,7,17,&out)==1 && out==0x12345678);consumer_refusals+=3;
  struct {test_service base;pangu_u64 extension;} extended={table,0};extended.base.header.struct_size=32;assert(!consumers[c](&extended,&state,7,-17,&out) && out==-14);
  test_service bad=table;bad.evaluate=failure_method;out=0x12345678;assert(consumers[c](&bad,&state,7,17,&out)==-92 && out==0x12345678);provider_errors++;
 }
 for(int kind=0;kind<3;kind++) {service_state state={kind==0 ? 2147483647 : kind==1 ? (-2147483647-1) : 1,kind==2 ? 2147483647 : 0};service_state prior=state;out=0x12345678;assert(pangu_route_cpp_evaluate(&state,kind==1 ? -1 : 1,&out)==11 && out==0x12345678 && !memcmp(&state,&prior,sizeof state));provider_errors++;}
}
int main(void) {
 pangu_route_cpp_set_observer(pangu_cpp_provider_observed);
 life=real_life=pangu_lifecycle_query(1,152,1,0);bridge=pangu_entry_bridge_query(1,24,1,0);route=pangu_service_route_query(1,40,1,0);assert(life && bridge && route);pangu_prepare_result p;consumer_boundaries();
 for(pangu_u64 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {const host_graph_case *c=&host_graph_cases[i];for(int backend=0;backend<3;backend++) for(int reverse=0;reverse<2;reverse++) {install(c->installed,reverse);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=interop_entry;assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,&compiled,&corpus_storage,10000000,&p)==c->status);if(c->status) continue;for(int deferred=0;deferred<2;deferred++) interop_drive(c->manifest,&p,&corpus_output,corpus_entry_output,backend,deferred);}}
 for(int variant=0;variant<4;variant++) for(int backend=0;backend<3;backend++) for(int reverse=0;reverse<2;reverse++) {install(&installed,reverse);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=interop_entry;const pangu_static_manifest *m=variant>=2 ? &multi_manifest : &manifest;if(variant==1) instances[2].scope=2;if(variant>=2) multi_manifest.provisions=variant==3 ? reverse_provisions : provisions;assert(!pangu_discover_prepare(m,source_contracts,4,&compiled,&storage,1000000,&p));for(int deferred=0;deferred<2;deferred++) interop_drive(m,&p,&output,entry_output,backend,deferred);if(variant==1) instances[2].scope=1;}
 for(int p=0;p<3;p++) for(int c=0;c<3;c++) assert(matrix[p][c]>0);
 printf("interop_cases=%d actual_service_calls=%d cpp_provider_calls=%d consumer_refusals=%d provider_errors=%d late_refusals=%d deferred_loads=%d matrix=",interop_cases,interop_calls,cpp_provider_calls,consumer_refusals,provider_errors,late_refusals,async_loads);for(int p=0;p<3;p++) for(int c=0;c<3;c++) printf("%s%d",p || c ? "," : "",matrix[p][c]);puts("");return 0;
}
