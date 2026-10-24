#include "src/t14-abi/client-route-v1.h"
#define main entry_suite_main
#include "entry-fixture.h"
#undef main
static const pangu_service_api *route;
static pangu_service_router router;static pangu_service_slot service_slots[1000];
typedef struct {int value,calls;} service_state;
typedef struct {pangu_service_header header;pangu_i32 (*evaluate)(void *,pangu_i32,pangu_i32 *);} test_service;
static service_state service_states[1000];static test_service tables[1000];
static int route_cases,service_calls,impl_calls[2],published,route_refusals,budget_refusals,route_aliases,route_variant,check_budget,hold_mode;
static pangu_service_lease held,old_lease;static int holding,has_old;
static pangu_u64 epoch;static int async_mode,async_armed,async_publishes,cancelled_publications;static const pangu_entry_context *async_context;static pangu_lifecycle_setup_ticket async_scope;
static pangu_i32 eval_a(void *opaque,pangu_i32 value,pangu_i32 *out) {service_state *s=opaque;*out=s->value+value;s->calls++;impl_calls[0]++;return 0;}
static pangu_i32 eval_b(void *opaque,pangu_i32 value,pangu_i32 *out) {service_state *s=opaque;*out=value+s->value;s->calls++;impl_calls[1]++;return 0;}
static void query_for(pangu_u32 b,pangu_u32 mode,const void *scope,pangu_service_query *q) {
 memset(q,0,sizeof(*q));pangu_plan_edge e=router.graph->bindings[b];const pangu_plan_requirement *req=&router.plan->lowered.graph.requirements[e.requirement];memcpy(q->service,router.manifest->definitions[req->service].name,32);q->version=req->version;q->minimum_size=sizeof(test_service);q->binding=b;q->consumer=req->consumer;q->mode=mode;q->scope=scope;
}
static void invoke(const pangu_service_lease *lease,pangu_u32 b) {
 pangu_u32 p=router.graph->bindings[b].provider;assert(lease->state==&service_states[p] && lease->table==&tables[p].header);const test_service *table=(const void *)lease->table;pangu_i32 result=0;assert(!table->evaluate(lease->state,17,&result) && result==service_states[p].value+17);service_calls++;
}
static void expect_pure(const pangu_service_query *q,int code) {
 pangu_service_lease lease;pangu_lifecycle_report report;memset(&lease,0xa5,sizeof lease);memset(&report,0xa5,sizeof report);pangu_owner before[9];pangu_owned_operation ops[9][3];pangu_lifecycle_runtime old=run;memcpy(before,owners,sizeof owners);memcpy(ops,operations,sizeof operations);
 assert(route->acquire(&router,q,100000,&lease,&report)==code && entry_poison(&lease,sizeof lease) && entry_poison(&report,sizeof report) && !memcmp(before,owners,sizeof owners) && !memcmp(ops,operations,sizeof operations) && !memcmp(&old,&run,sizeof run));route_refusals++;
}
static void acquire_call(pangu_u32 b,pangu_u32 mode,const void *scope,int budget) {
 pangu_service_query q;query_for(b,mode,scope,&q);pangu_service_lease lease;pangu_lifecycle_report report;
 if(budget) {
  pangu_owner before[9];pangu_owned_operation ops[9][3];pangu_lifecycle_runtime old=run;pangu_service_slot slots[1000];memcpy(before,owners,sizeof owners);memcpy(ops,operations,sizeof operations);memcpy(slots,service_slots,sizeof slots);
  for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);memset(&lease,0xa5,sizeof lease);memset(&report,0xa5,sizeof report);int rc=route->acquire(&router,&q,fuel,&lease,&report);if(!rc) {assert(report.work<=fuel);break;}
   assert(rc==2 && entry_poison(&lease,sizeof lease) && !memcmp(before,owners,sizeof owners) && !memcmp(ops,operations,sizeof operations) && !memcmp(&old,&run,sizeof run) && !memcmp(slots,service_slots,sizeof slots));if(!entry_poison(&report,sizeof report)) assert(report.work<=fuel && report.pending && report.reason==1);budget_refusals++;
  }
 }else assert(!route->acquire(&router,&q,100000,&lease,&report) && report.work<=100000);
 invoke(&lease,b);assert(!route->release(&router,&lease));assert(route->release(&router,&lease)==10);route_refusals++;old_lease=lease;has_old=1;
}
static pangu_i32 route_cleanup(void *opaque) {
 const pangu_entry_context *c=opaque;pangu_cleanup_ticket scope;assert(!life->cleanup_scope(&run,c->position,&scope));
 for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==c->instance_id) acquire_call(b,2,&scope,check_budget);
 cleanups++;return 0;
}
static pangu_i32 route_load(const pangu_entry_context *c) {
 for(pangu_u32 i=0;i<router.slot_count;i++) if(current_plan->lowered.graph.provider_owners[i]==c->instance_id) {
  {test_service wrong=tables[i];pangu_service_slot before=service_slots[i];wrong.header.version++;assert(route->publish(&router,c,i,&wrong.header,&service_states[i],100000)==9 && !memcmp(&before,&service_slots[i],sizeof before));wrong=tables[i];wrong.header.struct_size=15;assert(route->publish(&router,c,i,&wrong.header,&service_states[i],100000)==9 && !memcmp(&before,&service_slots[i],sizeof before));wrong=tables[i];wrong.header.target=2;assert(route->publish(&router,c,i,&wrong.header,&service_states[i],100000)==9 && !memcmp(&before,&service_slots[i],sizeof before));route_refusals+=3;}
  if(check_budget) {
   pangu_owner before[9];pangu_owned_operation ops[9][3];pangu_lifecycle_runtime old=run;memcpy(before,owners,sizeof owners);memcpy(ops,operations,sizeof operations);
   pangu_service_slot slots[1000];memcpy(slots,service_slots,sizeof slots);
   for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);int rc=route->publish(&router,c,i,&tables[i].header,&service_states[i],fuel);if(!rc) break;assert(rc==2 && !memcmp(before,owners,sizeof owners) && !memcmp(ops,operations,sizeof operations) && !memcmp(&old,&run,sizeof run) && !memcmp(slots,service_slots,sizeof slots));budget_refusals++;}
  }else assert(!route->publish(&router,c,i,&tables[i].header,&service_states[i],100000));
  assert(route->publish(&router,c,i,&tables[i].header,&service_states[i],100000)==4);published++;route_refusals++;
 }
 assert(!life->register_resource(&run,c->position,route_cleanup,(void *)c));
 pangu_lifecycle_setup_ticket scope;assert(!life->setup_scope(&run,&scope));for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==c->instance_id) acquire_call(b,1,&scope,check_budget);
 return 0;
}
static pangu_i32 route_entry(void *opaque) {
 const pangu_entry_context *c=opaque;entries_called++;
 if(async_armed) {async_armed=0;async_context=c;assert(!life->setup_defer(&run,&async_scope));return 5;}
 return route_load(c);
}
static void negative_routes(void) {
 for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) {
  pangu_service_query q;query_for(b,0,0,&q);pangu_service_query bad=q;bad.service[0]^=1;expect_pure(&bad,9);bad=q;bad.version++;expect_pure(&bad,9);bad=q;bad.minimum_size=sizeof(test_service)+1;expect_pure(&bad,9);bad=q;bad.binding=current_plan->graph_report.binding_count;expect_pure(&bad,3);bad=q;bad.consumer=0xffffffffU;expect_pure(&bad,9);bad=q;bad.mode=3;expect_pure(&bad,1);
  pangu_u32 ci=q.consumer;records[ci].context.owner_epoch++;expect_pure(&q,10);records[ci].context.owner_epoch--;pangu_u32 p=current_graph->bindings[b].provider;pangu_service_slot slot=service_slots[p];service_slots[p].published=0;expect_pure(&q,3);service_slots[p]=slot;service_slots[p].epoch++;expect_pure(&q,9);service_slots[p]=slot;tables[p].header.target=2;expect_pure(&q,9);tables[p].header.target=1;
  struct {void *p;pangu_u64 n;} targets[]={{&router,sizeof router},{service_slots,sizeof service_slots},{records,sizeof records},{&run,sizeof run},{owners,sizeof owners},{operations,sizeof operations},{resources,sizeof resources},{protected_regions,sizeof protected_regions},{(void *)current_plan,sizeof(*current_plan)},{(void *)current_graph,sizeof(*current_graph)},{(void *)router.manifest,sizeof(*router.manifest)},{(void *)life,sizeof(*life)},{&tables[p],sizeof tables[p]}};
  for(pangu_u32 i=0;i<sizeof targets/sizeof targets[0];i++) {unsigned char before[80];pangu_u64 n=targets[i].n<80 ? targets[i].n : 80;memcpy(before,targets[i].p,n);pangu_lifecycle_report report;memset(&report,0xa5,sizeof report);assert(route->acquire(&router,&q,100000,targets[i].p,&report)==1 && !memcmp(before,targets[i].p,n) && entry_poison(&report,sizeof report));route_aliases++;}
  for(pangu_u32 other=0;other<router.slot_count;other++) if(service_slots[other].published) {test_service before=tables[other];pangu_lifecycle_report report;memset(&report,0xa5,sizeof report);assert(route->acquire(&router,&q,100000,(void *)&tables[other],&report)==1 && !memcmp(&before,&tables[other],sizeof before) && entry_poison(&report,sizeof report));route_aliases++;}
  pangu_service_lease lease;memset(&lease,0xa5,sizeof lease);assert(route->acquire(&router,&q,100000,&lease,(void *)&lease)==1 && entry_poison(&lease,sizeof lease));route_aliases++;
 }
}
static void route_drive(const pangu_static_manifest *m,const pangu_prepare_result *plan,const pangu_graph_output *graph,const pangu_u32 *entry_map,int variant,int budget) {
 route_variant=variant;check_budget=budget;holding=0;async_armed=async_mode;async_context=0;memset(&run,0,sizeof run);memset(disposed,0,sizeof disposed);memset(service_slots,0,sizeof service_slots);memset(service_states,0,sizeof service_states);entries_called=cleanups=0;current_plan=plan;current_graph=graph;epoch++;
 for(pangu_u32 i=0;i<m->instance_count;i++) {if(!owners[i].abi_version || owners[i].state==1) memset(&owners[i],0,sizeof owners[i]);assert(!life->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));instances_in[i]=(pangu_entry_instance){&owners[i],0};}
 for(pangu_u32 i=0;i<plan->lowered.registry.provider_count;i++) {tables[i]=(test_service){{plan->lowered.registry.providers[i].version,sizeof(test_service),1,0},variant ? eval_b : eval_a};service_states[i].value=(int)plan->lowered.registry.providers[i].instance*11+3;}
 pangu_entry_request req={m,&compiled,entry_map,m->module_count,plan,graph,life,&run,instances_in,m->instance_count};pangu_entry_storage st={records,9,sources,9,module_scratch,9};pangu_entry_prepared ep;assert(!bridge->prepare(&req,&st,100000,&ep));pangu_lifecycle_storage ls={items,9,stop_edges,1000};pangu_lifecycle_prepared lp;assert(!life->prepare(plan,graph,sources,m->instance_count,&ls,100000,&lp));pangu_lifecycle_graph sg={stop_edges,lp.edge_count,disposed,9};run.items=items;run.count=lp.item_count;run.stop_graph=&sg;assert(!life->bind_geometry(&run,protected_regions,64,100000));router=(pangu_service_router){life,&run,m,plan,graph,records,service_slots,plan->lowered.registry.provider_count};
 pangu_lifecycle_report report;int rc=life->start(&run,1,&report),ticks=0;while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2) {assert(async_context && !run.callback_open && run.setup_waiting);
  if(async_mode==2) {pangu_lifecycle_report stopped;int sr=life->stop(&run,1,&stopped);assert(sr==2 || sr==5);for(pangu_u32 p=0;p<router.slot_count;p++) if(plan->lowered.graph.provider_owners[p]==async_context->instance_id) {pangu_service_slot before=service_slots[p];assert(route->publish(&router,async_context,p,&tables[p].header,&service_states[p],100000)==10 && !memcmp(&before,&service_slots[p],sizeof before));cancelled_publications++;}}
  else {assert(!route_load(async_context));async_publishes++;}
  assert(!life->setup_finish(&run,&async_scope,0));async_context=0;
 }else assert(report.reason==1);rc=life->pump(&run,1,&report);}
 if(async_mode==2 && run.count) {assert(rc==6 && run.phase==4 && entries_called==1 && !cleanups);route_cases++;return;}assert(!rc && entries_called==(int)run.count);
 if(has_old) {pangu_owner before[9];pangu_owned_operation ops[9][3];memcpy(before,owners,sizeof owners);memcpy(ops,operations,sizeof operations);assert(route->release(&router,&old_lease)!=0 && !memcmp(before,owners,sizeof owners) && !memcmp(ops,operations,sizeof operations));route_refusals++;has_old=0;}
 negative_routes();for(pangu_u32 b=0;b<plan->graph_report.binding_count;b++) acquire_call(b,0,0,budget);
 if(hold_mode && plan->graph_report.binding_count) {pangu_service_query q;query_for(0,0,0,&q);assert(!route->acquire(&router,&q,100000,&held,&report));holding=1;}
 rc=life->stop(&run,1,&report);ticks=0;
 while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2 && holding) {assert(owners[consumer(0)].state==3 && owners[provider(0)].state==2);invoke(&held,0);assert(!route->release(&router,&held));holding=0;}else assert(report.reason==1);rc=life->stop(&run,1,&report);}
 assert(!rc && !holding && cleanups==entries_called);
 for(pangu_u32 p=0;p<router.slot_count;p++) {pangu_service_slot before=service_slots[p];const pangu_entry_context *ctx=&records[plan->lowered.graph.provider_owners[p]].context;assert(route->publish(&router,ctx,p,&tables[p].header,&service_states[p],100000)==10 && !memcmp(&before,&service_slots[p],sizeof before));route_refusals++;}
 for(pangu_u32 b=0;b<plan->graph_report.binding_count;b++) {pangu_service_query q;query_for(b,0,0,&q);pangu_service_lease lease;memset(&lease,0xa5,sizeof lease);assert(route->acquire(&router,&q,100000,&lease,&report)==10 && entry_poison(&lease,sizeof lease));route_refusals++;}
 route_cases++;
}
#ifndef PANGU_ROUTE_TEST_MAIN
#define PANGU_ROUTE_TEST_MAIN main
#endif
int PANGU_ROUTE_TEST_MAIN(void) {
 real_life=life=pangu_lifecycle_query(1,152,1,0);bridge=pangu_entry_bridge_query(1,24,1,0);route=pangu_service_route_query(1,40,1,0);assert(route && route->abi_version==1 && route->struct_size==40 && route->target==1 && !route->reserved);assert(!pangu_service_route_query(2,40,1,0) && !pangu_service_route_query(1,39,1,0) && !pangu_service_route_query(1,40,2,0) && !pangu_service_route_query(1,40,1,1));pangu_prepare_result plan;
 for(pangu_u64 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {const host_graph_case *c=&host_graph_cases[i];for(int variant=0;variant<4;variant++) {async_mode=variant>=2;install(c->installed,variant&1);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=route_entry;assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,&compiled,&corpus_storage,10000000,&plan)==c->status);if(c->status) continue;hold_mode=1;route_drive(c->manifest,&plan,&corpus_output,corpus_entry_output,variant&1,0);}}
 for(int variant=0;variant<4;variant++) for(int backend=0;backend<2;backend++) {async_mode=backend;install(&installed,backend);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=route_entry;const pangu_static_manifest *m=variant>=2 ? &multi_manifest : &manifest;if(variant==1) instances[2].scope=2;if(variant>=2) multi_manifest.provisions=variant==3 ? reverse_provisions : provisions;assert(!pangu_discover_prepare(m,source_contracts,4,&compiled,&storage,1000000,&plan));hold_mode=1;route_drive(m,&plan,&output,entry_output,backend,1);if(variant==1) instances[2].scope=1;}
 async_mode=2;for(int backend=0;backend<2;backend++) {install(&installed,backend);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=route_entry;assert(!pangu_discover_prepare(&manifest,source_contracts,4,&compiled,&storage,1000000,&plan));route_drive(&manifest,&plan,&output,entry_output,backend,0);}
 assert(impl_calls[0] && impl_calls[1] && async_publishes && cancelled_publications);printf("route_cases=%d service_calls=%d implementation_a=%d implementation_b=%d published=%d refusals=%d insufficient_fuel=%d alias_refusals=%d deferred_loads=%d cancelled_publications=%d\n",route_cases,service_calls,impl_calls[0],impl_calls[1],published,route_refusals,budget_refusals,route_aliases,async_publishes,cancelled_publications);return 0;
}
