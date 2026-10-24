#include "src/t14-abi/client-abi-v1.h"
#include "src/focused-test-support/assert.h"
#include "src/focused-test-support/stdio.h"
#include "src/focused-test-support/string.h"
#include "plan-fixture.h"

#define FUEL 1000000U
static const pangu_lifecycle_api *api;
static pangu_lifecycle_runtime live;
static pangu_owner owners[9];static pangu_owned_resource resources[9][2];
static pangu_owned_operation operations[9][3];static pangu_lifecycle_item source_items[9],activation[9];
static pangu_lifecycle_span spans[64];static pangu_lifecycle_edge lifecycle_edges[1000];
static pangu_u32 positions[9];
static const pangu_prepare_result *current;static const pangu_graph_output *current_output;
static int selected,mode,setup_count,cleanup_count,cleanup_begin_count;
static int sync_mode,sync_fail_index,sync_cancel_index,sync_events[32],sync_ne,sync_call_count,sync_no_edge_holds,sync_scenarios;
static pangu_lifecycle_graph sync_graph;static pangu_u32 sync_disposed[9];static pangu_u64 prepared_edge_count;
static pangu_lifecycle_setup_ticket setup_ticket;
static pangu_cleanup_ticket cleanup_ticket;
static pangu_u64 epoch;
static int scenarios,by_mode[7],invalid_ticket_refusals,inline_finishes,async_cleanup_finishes;

static int pump(void) {
 pangu_lifecycle_report report={0};int rc=api->pump(&live,FUEL,&report);
 assert(report.work<=FUEL);return rc;
}
static int stop(void) {
 pangu_lifecycle_report report={0};int rc=api->stop(&live,FUEL,&report);
 assert(report.work<=FUEL);return rc;
}
static pangu_i32 cleanup(void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;cleanup_count++;if(sync_mode){assert(sync_ne<32);sync_events[sync_ne++]=100+(int)id;}
 for(pangu_u32 i=0;i<current->graph_report.binding_count;i++) {
  pangu_plan_edge edge=current_output->bindings[i];
  if(current->lowered.graph.requirements[edge.requirement].consumer==id) {
   pangu_u32 provider=current->lowered.graph.provider_owners[edge.provider];
   assert(owners[provider].state==PANGU_OWNER_ACTIVE);
  }
 }
 return 0;
}
static pangu_i32 deferred_cleanup(void *opaque,const pangu_cleanup_ticket *ticket) {
 (void)opaque;cleanup_ticket=*ticket;cleanup_begin_count++;return 5;
}
static pangu_i32 setup(pangu_owner *owner,void *opaque) {
 int id=(int)(pangu_u64)opaque;setup_count++;
 assert(owner==&owners[id] && owner->inflight==1);
 assert(!api->register_resource(&live,positions[id],cleanup,opaque));
 if(sync_mode){assert(sync_ne<32);sync_events[sync_ne++]=id;if(id==sync_cancel_index) assert(stop()==5);return id==sync_fail_index?-71:0;}
 if(id!=selected) return 0;
 if(mode==3) assert(!api->register_async(&live,positions[id],deferred_cleanup,opaque));
 assert(!api->setup_defer(&live,&setup_ticket));
 pangu_lifecycle_setup_ticket other;
 assert(api->setup_defer(&live,&other)==10);
 assert(pump()==5);
 if(mode==4) {
  assert(!api->setup_finish(&live,&setup_ticket,0));
  assert(api->setup_finish(&live,&setup_ticket,0)==10);inline_finishes++;
 }
 if(mode==5) return 0;
 if(mode==6) {
  assert(stop()==5);
  assert(api->register_resource(&live,positions[id],cleanup,opaque)==10);
  assert(api->setup_defer(&live,&other)==10);
 }
 return 5;
}
static void prepare_runtime(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 if(live.phase) {
  assert(live.phase==4 && !live.started && !live.setup_waiting);
  for(pangu_u32 i=0;i<9;i++) assert(!owners[i].inflight && !owners[i].count);
 }
 memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;epoch++;
 assert(epoch && plan->lowered.graph.instance_count<=9);
 for(pangu_u32 i=0;i<9;i++) {
  if(!owners[i].abi_version || owners[i].state==PANGU_OWNER_LOADING) memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  source_items[i]=(pangu_lifecycle_item){&owners[i],setup,(void *)(pangu_u64)i};
 }
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};pangu_lifecycle_prepared prepared;
 assert(!api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,&storage_view,FUEL,&prepared));
 assert(prepared.item_count==plan->graph_report.order_count);prepared_edge_count=prepared.edge_count;
 for(pangu_u32 i=0;i<prepared.item_count;i++) positions[out->order[i]]=i;
 live.items=activation;live.count=prepared.item_count;
 /* Match the historical prefix teardown semantics; branch scheduling has its own corpus. */
 assert(!api->bind_geometry(&live,spans,64,FUEL));current=plan;current_output=out;
}

static void init_refusal_owners(pangu_u64 n) {
 memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;epoch++;assert(epoch && n<=9);
 for(pangu_u32 i=0;i<n;i++) {if(!owners[i].abi_version || owners[i].state==PANGU_OWNER_LOADING)memset(&owners[i],0,sizeof owners[i]);assert(!owners[i].inflight && !owners[i].count);assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));source_items[i]=(pangu_lifecycle_item){&owners[i],setup,(void *)(pangu_u64)i};}
}
static void assert_prepare_refused(const pangu_prepare_result *plan,const pangu_graph_output *out,const pangu_lifecycle_storage *storage,pangu_i32 expected) {
 pangu_lifecycle_prepared info={999,998,997};memset(activation,0xa5,sizeof activation);memset(lifecycle_edges,0xa5,sizeof lifecycle_edges);pangu_lifecycle_item copy[9];pangu_lifecycle_edge edge_copy[1000];memcpy(copy,activation,sizeof activation);memcpy(edge_copy,lifecycle_edges,sizeof lifecycle_edges);
 assert(api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,storage,FUEL,&info)==expected);assert(info.item_count==999 && info.edge_count==998 && info.work==997);assert(!memcmp(copy,activation,sizeof activation) && !memcmp(edge_copy,lifecycle_edges,sizeof lifecycle_edges));assert(!setup_count);
}
static void adapter_prepare_refusals(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 pangu_u64 n=plan->lowered.graph.instance_count;init_refusal_owners(n);pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};
 pangu_lifecycle_prepared info={999,998,997};memset(activation,0xa5,sizeof activation);memset(lifecycle_edges,0xa5,sizeof lifecycle_edges);pangu_lifecycle_item copy[9];pangu_lifecycle_edge edge_copy[1000];memcpy(copy,activation,sizeof activation);memcpy(edge_copy,lifecycle_edges,sizeof lifecycle_edges);
 pangu_u64 fuel=1;for(;fuel<100000;fuel++){pangu_i32 rc=api->prepare(plan,out,source_items,n,&storage_view,fuel,&info);if(!rc) break;assert(rc==2 && info.item_count==999 && info.edge_count==998 && info.work==997);assert(!memcmp(copy,activation,sizeof activation)&&!memcmp(edge_copy,lifecycle_edges,sizeof lifecycle_edges)&&!setup_count);}
 assert(fuel<100000 && info.item_count==plan->graph_report.order_count && info.edge_count==plan->graph_report.binding_count && info.work<=fuel && !setup_count);
}
static void adapter_public_drive(const pangu_prepare_result *plan,const pangu_graph_output *out,int fault,int variant) {
 sync_mode=1;sync_fail_index=sync_cancel_index=-1;sync_ne=0;mode=variant;selected=-1;prepare_runtime(plan,out);pangu_u32 started_expected=(fault<0 || variant==3)?(pangu_u32)live.count:(pangu_u32)fault+1;int target=fault<0?-1:(int)out->order[fault];if(variant==1)sync_fail_index=target;if(variant==2)sync_cancel_index=target;
 pangu_lifecycle_report report={0};pangu_i32 rc=api->start(&live,FUEL,&report);assert(report.work<=FUEL);
 if(variant==1)assert(rc==-71);else if(variant==2)assert(rc==6);else assert(!rc);
 if(variant==3){
  pangu_u32 binding=0xffffffffU,position=positions[target];
  for(pangu_u32 i=0;i<prepared_edge_count;i++) if(lifecycle_edges[i].consumer==position || lifecycle_edges[i].provider==position){binding=i;break;}
  if(binding==0xffffffffU){sync_no_edge_holds++;assert(!stop());}
  else {
   memset(sync_disposed,0,sizeof sync_disposed);sync_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared_edge_count,sync_disposed,9};live.stop_graph=&sync_graph;
   pangu_lifecycle_call_ticket ticket={0};report=(pangu_lifecycle_report){0};assert(!api->call(&live,binding,FUEL,&ticket,&report));assert(report.work<=FUEL);sync_call_count++;
   pangu_lifecycle_edge edge=sync_graph.edges[binding];pangu_u32 consumer_position=edge.consumer,provider_position=edge.provider;
   assert(owners[out->order[consumer_position]].inflight && owners[out->order[provider_position]].inflight);
   assert(stop()==5);assert(owners[out->order[consumer_position]].state!=PANGU_OWNER_DISPOSED && owners[out->order[provider_position]].state!=PANGU_OWNER_DISPOSED);
   assert(!api->call_finish(&live,&ticket));assert(!stop());
  }
 } else assert(!stop());
 assert(!live.started && setup_count==(int)started_expected && cleanup_count==(int)started_expected);
 for(pangu_u32 i=0;i<started_expected;i++) assert(sync_events[i]==(int)out->order[i]);
 assert(sync_ne==2*(int)started_expected);
 if(variant!=3 || !live.stop_graph) for(pangu_u32 i=0;i<started_expected;i++) assert(sync_events[started_expected+i]==100+(int)out->order[started_expected-i-1]);
 if(variant==3 && live.stop_graph){
  for(pangu_u32 i=0;i<prepared_edge_count;i++) {pangu_u32 consumer=out->order[lifecycle_edges[i].consumer],provider=out->order[lifecycle_edges[i].provider];int ci=-1,pi=-1;for(int j=0;j<sync_ne;j++){if(sync_events[j]==100+(int)consumer)ci=j;if(sync_events[j]==100+(int)provider)pi=j;}assert(ci>=0&&pi>=0&&ci<pi);}
 }
 for(pangu_u32 i=0;i<plan->lowered.graph.instance_count;i++){if(out->states[i]||positions[i]>=started_expected){assert(owners[i].state==PANGU_OWNER_LOADING&&!owners[i].inflight&&!owners[i].count);}else assert(owners[i].state==PANGU_OWNER_DISPOSED&&!owners[i].inflight&&!owners[i].count);}
 int count=cleanup_count;assert(!stop()&&cleanup_count==count);sync_scenarios++;sync_mode=0;
}
static void adapter_public_core(void) {
 pangu_prepare_result plan;int valid=0,rejected=0;
 for(pangu_u32 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++){
  const host_graph_case *c=&host_graph_cases[i];assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,FUEL,&plan)==c->status);assert(!callback_calls);
  if(c->status){rejected++;init_refusal_owners(c->manifest->instance_count);pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};assert_prepare_refused(&plan,&corpus_output,&storage_view,10);continue;}
  valid++;adapter_prepare_refusals(&plan,&corpus_output);adapter_public_drive(&plan,&corpus_output,-1,0);adapter_prepare_refusals(&plan,&corpus_output);
  for(pangu_u32 position=0;position<plan.graph_report.order_count;position++){
   adapter_public_drive(&plan,&corpus_output,(int)position,1);
   adapter_public_drive(&plan,&corpus_output,(int)position,2);
   adapter_public_drive(&plan,&corpus_output,(int)position,3);
  }
 }
 assert(!prepare(FUEL,&plan));adapter_public_drive(&plan,&output,-1,0);instances[2].scope=2;assert(!prepare(FUEL,&plan));adapter_public_drive(&plan,&output,-1,0);instances[2].scope=1;
 for(int reverse=0;reverse<2;reverse++){multi_manifest.provisions=reverse?reverse_provisions:provisions;assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));for(int variant=0;variant<4;variant++)adapter_public_drive(&plan,&output,variant?2:-1,variant);}
 assert(!prepare(FUEL,&plan));init_refusal_owners(4);pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};
 plan.stage=5;assert_prepare_refused(&plan,&output,&storage_view,10);plan.stage=6;
 output_bindings[0].provider=999;assert_prepare_refused(&plan,&output,&storage_view,16);output_bindings[0].provider=1;
 output_bindings[0].requirement=999;assert_prepare_refused(&plan,&output,&storage_view,16);output_bindings[0].requirement=0;
 output_order[1]=2;output_order[2]=1;assert_prepare_refused(&plan,&output,&storage_view,16);output_order[1]=1;output_order[2]=2;
 output_states[2]=1;assert_prepare_refused(&plan,&output,&storage_view,16);output_states[2]=0;
 pangu_lifecycle_storage short_storage={activation,3,lifecycle_edges,1000};assert_prepare_refused(&plan,&output,&short_storage,2);
 source_items[2].owner=source_items[1].owner;assert_prepare_refused(&plan,&output,&storage_view,1);source_items[2].owner=&owners[2];
 assert(valid==95&&rejected==145&&sync_scenarios==675);
 printf("adapter_public_scenarios=%d valid_plans=%d rejected_plans=%d public_dependency_calls=%d no_edge_owner_only_holds=%d\n",sync_scenarios,valid,rejected,sync_call_count,sync_no_edge_holds);
}

static void run_deferred(const pangu_prepare_result *plan,const pangu_graph_output *out,pangu_u32 position,int variant) {
 mode=variant;selected=(int)out->order[position];prepare_runtime(plan,out);
 pangu_lifecycle_report report={0};int rc=api->start(&live,FUEL,&report);assert(report.work<=FUEL);
 if(mode==4) {
  assert(!rc && setup_count==(int)live.count);assert(!stop());scenarios++;by_mode[mode]++;return;
 }
 assert(rc==5 && setup_count==(int)position+1 && live.started==position+1);
 assert(owners[selected].inflight==1 && !cleanup_count);
 assert(pump()==5 && !cleanup_count);
 pangu_lifecycle_setup_ticket bad=setup_ticket;bad.operation.epoch++;
 assert(api->setup_finish(&live,&bad,0)==10);invalid_ticket_refusals++;
 bad=setup_ticket;bad.operation.generation++;
 assert(api->setup_finish(&live,&bad,0)==10);invalid_ticket_refusals++;
 bad=setup_ticket;bad.runtime=0;
 assert(api->setup_finish(&live,&bad,0)==10);invalid_ticket_refusals++;
 if(mode==2) {
  assert(stop()==5 && owners[selected].state==3);
  assert(api->register_resource(&live,position,cleanup,0)==10);
 }
 int failure=mode==1 || mode==3;
 assert(!api->setup_finish(&live,&setup_ticket,failure?-77:0));
 assert(api->setup_finish(&live,&setup_ticket,0)==10);
 assert(owners[selected].inflight==1 && !cleanup_count);
 rc=pump();
 if(mode==3) {
  assert(rc==5 && cleanup_begin_count==1 && live.started==position+1);
  for(pangu_u32 i=0;i<plan->graph_report.binding_count;i++) {
   pangu_plan_edge edge=out->bindings[i];
   if(plan->lowered.graph.requirements[edge.requirement].consumer==(pangu_u32)selected) {
    assert(owners[plan->lowered.graph.provider_owners[edge.provider]].state==2);
   }
  }
  assert(!api->disposer_finish(&live,position,&cleanup_ticket,-92));async_cleanup_finishes++;
  rc=pump();assert(live.cleanup_error==-92);
 }
 int expected=failure?-77:(mode==2 || mode==6)?6:mode==5?10:0;
 assert(rc==expected);
 if(!mode) {
  assert(setup_count==(int)live.count && owners[selected].state==2);assert(!stop());
 } else {
  assert(!live.started && setup_count==(int)position+1 && cleanup_count==(int)position+1);
  assert(owners[selected].state==4);
 }
 assert(api->setup_finish(&live,&setup_ticket,0)==10);
 for(pangu_u32 i=0;i<plan->lowered.graph.instance_count;i++) if(out->states[i]) assert(owners[i].state==1);
 scenarios++;by_mode[mode]++;
}

/* T14-F02 public-ABI async-disposer regression. */
static pangu_cleanup_ticket disposer_saved[4];
static pangu_lifecycle_graph disposer_graph;
static pangu_u32 disposer_disposed[9];
static int disposer_mode,disposer_selected,disposer_resource_count,disposer_begins;
static int disposer_groups,disposer_ticket_refusals,disposer_inline_finishes;
static pangu_i32 disposer_begin(void *opaque,const pangu_cleanup_ticket *ticket) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;
 assert((int)id==disposer_selected && disposer_begins<4);
 pangu_u32 position=positions[id];pangu_owner *owner=ticket ? (pangu_owner *)ticket->owner : 0;
 assert(owner==&owners[id] && owner->state==PANGU_OWNER_QUIESCING && owner->cleanup_waiting && owner->busy);
 disposer_saved[disposer_begins++]=*ticket;
 pangu_cleanup_ticket scope;assert(!api->cleanup_scope(&live,position,&scope));
 assert(scope.owner==ticket->owner && scope.epoch==ticket->epoch && scope.serial==ticket->serial);
 pangu_lifecycle_report report={0};assert(api->stop(&live,FUEL,&report)==5 && report.pending);
 assert(api->register_resource(&live,position,cleanup,opaque)==10);
 assert(api->register_async(&live,position,disposer_begin,opaque)==10);
 for(pangu_u32 i=0;i<current->graph_report.binding_count;i++) {
  pangu_plan_edge edge=current_output->bindings[i];
  if(current->lowered.graph.requirements[edge.requirement].consumer==id) {
   pangu_u32 provider=current->lowered.graph.provider_owners[edge.provider];
   assert(owners[provider].state==PANGU_OWNER_ACTIVE);
  }
 }
 if(disposer_mode==1) {
  assert(!api->disposer_finish(&live,position,ticket,0));
  assert(api->disposer_finish(&live,position,ticket,0)==10);disposer_inline_finishes++;
  return 5;
 }
 if(disposer_mode==2) return -88;
 if(disposer_mode==3) {
  assert(!api->disposer_finish(&live,position,ticket,0));disposer_inline_finishes++;
  return 0;
 }
 return 5;
}
static pangu_i32 disposer_setup(pangu_owner *owner,void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;setup_count++;
 assert(owner==&owners[id] && owner->state==PANGU_OWNER_LOADING && owner->inflight==1);
 if((int)id!=disposer_selected) return 0;
 for(int i=0;i<disposer_resource_count;i++)
  assert(!api->register_async(&live,positions[id],disposer_begin,opaque));
 return 0;
}
static void prepare_disposer_runtime(const pangu_prepare_result *plan,const pangu_graph_output *out,int selected_id,int resource_count) {
 if(live.phase) {
  assert(live.phase==4 && !live.started && !live.setup_waiting);
  for(pangu_u32 i=0;i<9;i++) assert(!owners[i].inflight && !owners[i].count);
 }
 memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;epoch++;assert(epoch);
 disposer_selected=selected_id;disposer_resource_count=resource_count;disposer_begins=0;
 memset(disposer_saved,0,sizeof disposer_saved);memset(disposer_disposed,0,sizeof disposer_disposed);
 assert(plan->lowered.graph.instance_count<=9);
 for(pangu_u32 i=0;i<9;i++) {
  if(!owners[i].abi_version || owners[i].state==PANGU_OWNER_LOADING) memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  source_items[i]=(pangu_lifecycle_item){&owners[i],disposer_setup,(void *)(pangu_u64)i};
 }
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};pangu_lifecycle_prepared prepared;
 assert(!api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,&storage_view,FUEL,&prepared));
 assert(prepared.item_count==plan->graph_report.order_count);
 for(pangu_u32 i=0;i<prepared.item_count;i++) positions[out->order[i]]=i;
 live.items=activation;live.count=prepared.item_count;
 disposer_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared.edge_count,disposer_disposed,9};
 live.stop_graph=&disposer_graph;current=plan;current_output=out;
 assert(!api->bind_geometry(&live,spans,64,FUEL));
}
static int disposer_start(void) {
 pangu_lifecycle_report report={0};int rc=api->start(&live,FUEL,&report);
 assert(!rc && !report.pending && report.work<=FUEL && setup_count==(int)live.count);return rc;
}
static void test_disposer_deferred_tickets(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 disposer_mode=0;prepare_disposer_runtime(plan,out,0,2);disposer_start();
 assert(api->stop(&live,FUEL,&(pangu_lifecycle_report){0})==5 && disposer_begins==1);
 pangu_lifecycle_report report={0};assert(api->stop(&live,FUEL,&report)==5 && report.pending && disposer_begins==1);
 pangu_owner pending_owner=owners[0];assert(api->owner_init(&owners[0],resources[0],2,operations[0],3,epoch+1,4)==10);assert(!memcmp(&owners[0],&pending_owner,sizeof pending_owner));
 pangu_cleanup_ticket bad=disposer_saved[0];bad.epoch++;assert(api->disposer_finish(&live,positions[0],&bad,0)==10);disposer_ticket_refusals++;
 bad=disposer_saved[0];bad.serial++;assert(api->disposer_finish(&live,positions[0],&bad,0)==10);disposer_ticket_refusals++;
 bad=disposer_saved[0];bad.owner=&owners[1];assert(api->disposer_finish(&live,positions[0],&bad,0)==10);disposer_ticket_refusals++;
 assert(api->disposer_finish(&live,positions[0],(const pangu_cleanup_ticket *)(const void *)&owners[0],0)==1);disposer_ticket_refusals++;
 assert(!api->disposer_finish(&live,positions[0],&disposer_saved[0],0));
 assert(api->disposer_finish(&live,positions[0],&disposer_saved[0],0)==10);disposer_ticket_refusals++;
 report=(pangu_lifecycle_report){0};assert(api->stop(&live,FUEL,&report)==5 && report.pending && disposer_begins==2);
 assert(disposer_saved[0].serial!=disposer_saved[1].serial);
 assert(api->disposer_finish(&live,positions[0],&disposer_saved[0],0)==10);disposer_ticket_refusals++;
 assert(!api->disposer_finish(&live,positions[0],&disposer_saved[1],-22));
 report=(pangu_lifecycle_report){0};assert(api->stop(&live,FUEL,&report)==-22 && !report.pending && live.phase==4);
 assert(owners[0].state==PANGU_OWNER_DISPOSED && owners[0].cleanup_error==-22);
 assert(api->disposer_finish(&live,positions[0],&disposer_saved[1],0)==10);disposer_ticket_refusals++;
 disposer_groups++;
}
static void test_disposer_inline_modes(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 for(int mode=1;mode<=3;mode++) {
  disposer_mode=mode;prepare_disposer_runtime(plan,out,0,1);disposer_start();
  pangu_lifecycle_report report={0};int rc=api->stop(&live,FUEL,&report);
  pangu_i32 expected=mode==1 ? 0 : mode==2 ? -88 : 10;
  assert(rc==expected && !report.pending && live.phase==4 && disposer_begins==1);
  assert(owners[0].state==PANGU_OWNER_DISPOSED && owners[0].cleanup_error==(mode==1 ? 0 : expected));
  assert(api->stop(&live,FUEL,&report)==expected && disposer_begins==1);
  disposer_groups++;
 }
 disposer_mode=0;
}
static void test_disposer_epoch_and_call_drain(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 disposer_mode=0;prepare_disposer_runtime(plan,out,0,1);disposer_start();
 assert(api->stop(&live,FUEL,&(pangu_lifecycle_report){0})==5 && disposer_begins==1);
 pangu_cleanup_ticket old=disposer_saved[0];pangu_lifecycle_report report={0};
 assert(!api->disposer_finish(&live,positions[0],&old,0));assert(api->stop(&live,FUEL,&report)==0 && !report.pending);
 prepare_disposer_runtime(plan,out,0,1);disposer_start();
 assert(api->stop(&live,FUEL,&report)==5 && disposer_begins==1);
 assert(api->disposer_finish(&live,positions[0],&old,0)==10);disposer_ticket_refusals++;
 assert(!api->disposer_finish(&live,positions[0],&disposer_saved[0],0));
 assert(api->stop(&live,FUEL,&report)==0 && !report.pending);disposer_groups++;

 /* A public dependency call keeps its consumer/provider pair alive until finish. */
 pangu_plan_edge edge=out->bindings[0];
 int consumer=(int)plan->lowered.graph.requirements[edge.requirement].consumer;
 prepare_disposer_runtime(plan,out,consumer,1);disposer_start();
 pangu_lifecycle_call_ticket call={0};report=(pangu_lifecycle_report){0};
 assert(!api->call(&live,0,FUEL,&call,&report) && report.work<=FUEL);
 assert(api->stop(&live,FUEL,&report)==5 && report.pending && disposer_begins==0);
 assert(!api->call_finish(&live,&call));
 assert(api->stop(&live,FUEL,&report)==5 && report.pending && disposer_begins==1);
 assert(!api->disposer_finish(&live,positions[consumer],&disposer_saved[0],0));
 assert(api->stop(&live,FUEL,&report)==0 && !report.pending);
 disposer_groups++;
}
static void test_disposer_provider_retention(void) {
 pangu_prepare_result plan;multi_manifest.provisions=provisions;
 assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));
 int consumer=(int)plan.lowered.graph.requirements[output.bindings[0].requirement].consumer;
 prepare_disposer_runtime(&plan,&output,consumer,1);disposer_start();
 pangu_lifecycle_report report={0};assert(api->stop(&live,FUEL,&report)==5 && report.pending && disposer_begins==1);
 for(pangu_u32 i=0;i<disposer_graph.edge_count;i++) {
  pangu_plan_edge edge=output.bindings[i];
  if(plan.lowered.graph.requirements[edge.requirement].consumer==(pangu_u32)consumer) {
   pangu_u32 provider=plan.lowered.graph.provider_owners[edge.provider];
   assert(owners[provider].state==PANGU_OWNER_ACTIVE);
  }
 }
 assert(!api->disposer_finish(&live,positions[consumer],&disposer_saved[0],0));
 assert(api->stop(&live,FUEL,&report)==0 && !report.pending);
 for(pangu_u32 i=0;i<plan.lowered.graph.instance_count;i++) if(!output.states[i]) assert(owners[i].state==PANGU_OWNER_DISPOSED);
 disposer_groups++;
}
static void async_disposer_public_core(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 test_disposer_deferred_tickets(plan,out);
 test_disposer_inline_modes(plan,out);
 test_disposer_epoch_and_call_drain(plan,out);
 test_disposer_provider_retention();
 assert(disposer_groups==7 && disposer_ticket_refusals==8 && disposer_inline_finishes==2);

}


/* T14-F03 public-ABI resumable stop budget regression. */
static int budget_mode,budget_target_id,budget_error_owner,budget_callbacks,budget_async_begins;
static int budget_call_skips,budget_cases,budget_total_ticks,budget_prefix_cases;
static pangu_i32 budget_error_status;
static pangu_u32 budget_binding;
static pangu_lifecycle_setup_ticket budget_setup_ticket;
static pangu_cleanup_ticket budget_cleanup_ticket;
static pangu_lifecycle_call_ticket budget_call_ticket;
static pangu_lifecycle_graph budget_graph;
static pangu_u32 budget_disposed[9];
static pangu_i32 budget_release(void *opaque) {
 int id=(int)(pangu_u64)opaque;budget_callbacks++;
 assert(!cleanup(opaque));assert(stop()==5);
 return id==budget_error_owner ? budget_error_status : 0;
}
static pangu_i32 budget_async_release(void *opaque,const pangu_cleanup_ticket *ticket) {
 (void)opaque;budget_callbacks++;budget_async_begins++;budget_cleanup_ticket=*ticket;
 pangu_lifecycle_report report={0};assert(api->stop(&live,FUEL,&report)==5 && report.pending);
 return 5;
}
static pangu_i32 budget_setup_entry(pangu_owner *owner,void *opaque) {
 int id=(int)(pangu_u64)opaque;setup_count++;
 assert(owner==&owners[id] && owner->inflight==1);
 assert(!api->register_resource(&live,positions[id],budget_release,opaque));
 if(id==budget_target_id && budget_mode==3) assert(!api->register_async(&live,positions[id],budget_async_release,opaque));
 else assert(!api->register_resource(&live,positions[id],budget_release,opaque));
 if(id==budget_target_id && budget_mode==4) {
  assert(!api->setup_defer(&live,&budget_setup_ticket));return 5;
 }
 return 0;
}
static void prepare_budget_runtime(const pangu_prepare_result *plan,const pangu_graph_output *out,int target,int mode,int prefix) {
 if(live.phase) {
  assert(live.phase==4 && !live.started && !live.setup_waiting);
  for(pangu_u32 i=0;i<9;i++) assert(!owners[i].inflight && !owners[i].count);
 }
 memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;epoch++;assert(epoch);
 budget_target_id=target;budget_mode=mode;budget_callbacks=budget_async_begins=0;memset(&budget_call_ticket,0,sizeof budget_call_ticket);memset(&budget_cleanup_ticket,0,sizeof budget_cleanup_ticket);memset(budget_disposed,0,sizeof budget_disposed);
 assert(plan->lowered.graph.instance_count<=9);
 for(pangu_u32 i=0;i<plan->lowered.graph.instance_count;i++) {
  if(!owners[i].abi_version || owners[i].state==PANGU_OWNER_LOADING) memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  source_items[i]=(pangu_lifecycle_item){&owners[i],budget_setup_entry,(void *)(pangu_u64)i};
 }
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};pangu_lifecycle_prepared prepared;
 assert(!api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,&storage_view,FUEL,&prepared));
 assert(prepared.item_count==plan->graph_report.order_count);prepared_edge_count=prepared.edge_count;
 for(pangu_u32 i=0;i<prepared.item_count;i++) positions[out->order[i]]=i;
 live.items=activation;live.count=prepared.item_count;
 budget_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared.edge_count,budget_disposed,9};
 live.stop_graph=prefix ? 0 : &budget_graph;current=plan;current_output=out;
 assert(!api->bind_geometry(&live,spans,64,FUEL));
}
static int budget_case_public(const pangu_prepare_result *plan,const pangu_graph_output *out,pangu_u32 position,int mode,pangu_u64 quantum,int prefix) {
 assert(quantum>=1 && quantum<=4);
 if(position>=plan->graph_report.order_count) {assert(!plan->graph_report.order_count && !position && mode==0);}
 int target=position<plan->graph_report.order_count ? (int)out->order[position] : -1;pangu_i32 codes[]={1,2,5,-94};
 budget_mode=mode;budget_target_id=target;budget_error_owner=mode==1 && plan->graph_report.order_count ? (int)out->order[0] : -1;budget_error_status=codes[quantum-1];
 if(mode==2) {
  if(prefix) {budget_call_skips++;return 0;}
  budget_binding=0xffffffffU;
  for(pangu_u32 i=0;i<plan->graph_report.binding_count;i++) {
   pangu_plan_edge edge=out->bindings[i];pangu_u32 c=plan->lowered.graph.requirements[edge.requirement].consumer,p=plan->lowered.graph.provider_owners[edge.provider];
   if(c==(pangu_u32)target || p==(pangu_u32)target) {budget_binding=i;break;}
  }
  if(budget_binding==0xffffffffU) {budget_call_skips++;return 0;}
 }
 prepare_budget_runtime(plan,out,target,mode,prefix);
 pangu_lifecycle_report report={0};int start_rc=api->start(&live,FUEL,&report);
 assert(report.work<=FUEL);
 if(mode==4) assert(start_rc==5 && setup_count==(int)position+1 && live.started==position+1);
 else assert(!start_rc && setup_count==(int)live.count);
 if(mode==2) {
  report=(pangu_lifecycle_report){0};
  assert(!api->call(&live,budget_binding,FUEL,&budget_call_ticket,&report) && report.work<=FUEL);
 }
 pangu_lifecycle_runtime before=live;pangu_owner owner_before[9];memcpy(owner_before,owners,sizeof owners);
 pangu_owned_resource resource_before[9][2];pangu_owned_operation operation_before[9][3];
 memcpy(resource_before,resources,sizeof resources);memcpy(operation_before,operations,sizeof operations);
 int callbacks_before_zero=budget_callbacks;
 report=(pangu_lifecycle_report){0};
 assert(api->stop(&live,0,&report)==2 && !report.work && report.reason==1 && report.pending && !report.reserved);
 assert(!memcmp(&before,&live,sizeof live) && !memcmp(owner_before,owners,sizeof owners) &&
        !memcmp(resource_before,resources,sizeof resources) && !memcmp(operation_before,operations,sizeof operations) &&
        budget_callbacks==callbacks_before_zero);
 int ticks=0,completed=mode<2;pangu_i32 expected=mode==1 ? budget_error_status : 0;
 for(;ticks<100000;ticks++) {
  int callbacks=budget_callbacks;pangu_i32 rc=api->stop(&live,quantum,&report);
  assert(report.work<=quantum && budget_callbacks-callbacks<=(int)report.work && !report.reserved && !live.busy);
  if(mode==4 && ticks==0) {
   assert(owners[target].state==PANGU_OWNER_QUIESCING);
   assert(api->register_resource(&live,positions[target],budget_release,(void *)(pangu_u64)target)==10);
  }
  if(!report.pending) {
   assert(report.reason==0 && report.state==4 && !live.started && rc==expected);break;
  }
  if(report.reason==1) {assert(rc==2);continue;}
  assert(report.reason==2 && rc==5 && mode>=2 && !completed);completed=1;
  if(mode==2) assert(!api->call_finish(&live,&budget_call_ticket));
  if(mode==3) assert(!api->disposer_finish(&live,positions[target],&budget_cleanup_ticket,0));
  if(mode==4) assert(!api->setup_finish(&live,&budget_setup_ticket,0));
 }
 assert(ticks<100000 && completed);
 int callbacks=budget_callbacks;report=(pangu_lifecycle_report){0};
 assert(api->stop(&live,0,&report)==expected && !report.pending && !report.work && report.reason==0 && callbacks==budget_callbacks);
 budget_cases++;budget_total_ticks+=ticks+1;if(prefix) budget_prefix_cases++;return ticks+1;
}
static pangu_owner budget_single_owner;
static pangu_owned_resource budget_single_resources[4];
static pangu_owned_operation budget_single_operations[4];
static pangu_lifecycle_runtime budget_single_runtime;
static pangu_lifecycle_item budget_single_item;
static pangu_lifecycle_span budget_single_spans[16];
static int budget_single_callbacks,budget_single_work;
static pangu_i32 budget_single_release(void *opaque) {(void)opaque;budget_single_callbacks++;return 0;}
static pangu_i32 budget_single_setup(pangu_owner *owner,void *opaque) {
 (void)opaque;assert(owner==&budget_single_owner);
 for(int i=0;i<4;i++) assert(!api->register_resource(&budget_single_runtime,0,budget_single_release,0));
 return 0;
}
static void owner_budget_public_case(void) {
 memset(&budget_single_runtime,0,sizeof budget_single_runtime);memset(&budget_single_owner,0,sizeof budget_single_owner);
 budget_single_callbacks=budget_single_work=0;
 assert(!api->owner_init(&budget_single_owner,budget_single_resources,4,budget_single_operations,4,1,FUEL));
 budget_single_item=(pangu_lifecycle_item){&budget_single_owner,budget_single_setup,0};
 budget_single_runtime.items=&budget_single_item;budget_single_runtime.count=1;
 assert(!api->bind_geometry(&budget_single_runtime,budget_single_spans,16,FUEL));
 pangu_lifecycle_report report={0};assert(!api->start(&budget_single_runtime,FUEL,&report) && !report.pending);
 pangu_lifecycle_runtime before=budget_single_runtime;pangu_owner owner_before=budget_single_owner;
 pangu_owned_resource resources_before[4];pangu_owned_operation operations_before[4];
 memcpy(resources_before,budget_single_resources,sizeof resources_before);memcpy(operations_before,budget_single_operations,sizeof operations_before);
 assert(api->stop(&budget_single_runtime,0,&report)==2 && !report.work && report.reason==1 && report.pending);
 assert(!memcmp(&before,&budget_single_runtime,sizeof before) && !memcmp(&owner_before,&budget_single_owner,sizeof owner_before));
 assert(!memcmp(resources_before,budget_single_resources,sizeof resources_before) && !memcmp(operations_before,budget_single_operations,sizeof operations_before));
 for(int tick=0;tick<32;tick++) {
  int callbacks=budget_single_callbacks;int rc=api->stop(&budget_single_runtime,1,&report);
  assert(report.work<=1 && budget_single_callbacks-callbacks<=(int)report.work && !budget_single_runtime.busy);
  budget_single_work+=(int)report.work;
  if(!report.pending) {assert(!rc && report.reason==0 && report.state==4);break;}
  assert(rc==2 && report.reason==1);
 }
 assert(budget_single_runtime.phase==4 && budget_single_owner.state==PANGU_OWNER_DISPOSED && !budget_single_owner.count && budget_single_callbacks==4);
 assert(budget_single_work>=4);
 int callbacks=budget_single_callbacks;
 assert(!api->stop(&budget_single_runtime,0,&report) && !report.pending && !report.work && report.reason==0 && callbacks==budget_single_callbacks);
}
static void budget_public_core(void) {
 owner_budget_public_case();
 pangu_prepare_result plan;int valid=0,rejected=0;
 for(pangu_u32 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {
  const host_graph_case *c=&host_graph_cases[i];
  assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,FUEL,&plan)==c->status);
  if(c->status) {rejected++;continue;}valid++;
  for(pangu_u64 q=1;q<=4;q++) {
   (void)budget_case_public(&plan,&corpus_output,0,0,q,0);
   if(plan.graph_report.order_count) (void)budget_case_public(&plan,&corpus_output,0,1,q,0);
   for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++) for(int mode=2;mode<=4;mode++) (void)budget_case_public(&plan,&corpus_output,pos,mode,q,0);
  }
 }
 assert(valid==95 && rejected==145);
 assert(!prepare(FUEL,&plan));
 for(pangu_u64 q=1;q<=4;q++) for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++) for(int mode=2;mode<=4;mode++) (void)budget_case_public(&plan,&output,pos,mode,q,0);
 for(int reverse=0;reverse<2;reverse++) {
  multi_manifest.provisions=reverse ? reverse_provisions : provisions;
  assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));
  for(pangu_u64 q=1;q<=4;q++) for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++) for(int mode=2;mode<=4;mode++) (void)budget_case_public(&plan,&output,pos,mode,q,0);
 }
 multi_manifest.provisions=provisions;
 assert(!prepare(FUEL,&plan));
 for(pangu_u64 q=1;q<=4;q++) for(int mode=0;mode<5;mode++) (void)budget_case_public(&plan,&output,2,mode,q,1);
 assert(budget_call_skips>=4 && budget_prefix_cases==16 && budget_cases>1000);
}


/* T14-F04 public-ABI call-budget regression. */
static int call_budget_scenarios,call_budget_admissions,call_budget_retries;
static int call_budget_quota_refusals,call_budget_invalid_bindings,call_budget_overlap_refusals;
static int call_budget_setup_contexts,call_budget_cleanup_contexts;
static pangu_lifecycle_graph call_budget_graph;
static pangu_u32 call_budget_disposed[9];

typedef struct {
 pangu_lifecycle_runtime runtime;
 pangu_owner owner[9];
 pangu_owned_resource resource[9][2];
 pangu_owned_operation operation[9][3];
 pangu_lifecycle_span span[64];
 pangu_lifecycle_edge edge[1000];
 pangu_u32 disposed[9];
} call_budget_snapshot;
static void call_budget_capture(call_budget_snapshot *s) {
 s->runtime=live;memcpy(s->owner,owners,sizeof owners);memcpy(s->resource,resources,sizeof resources);
 memcpy(s->operation,operations,sizeof operations);memcpy(s->span,spans,sizeof spans);
 memcpy(s->edge,lifecycle_edges,sizeof lifecycle_edges);memcpy(s->disposed,call_budget_disposed,sizeof call_budget_disposed);
}
static int call_budget_same(const call_budget_snapshot *s) {
 return !memcmp(&s->runtime,&live,sizeof live) && !memcmp(s->owner,owners,sizeof owners) &&
  !memcmp(s->resource,resources,sizeof resources) && !memcmp(s->operation,operations,sizeof operations) &&
  !memcmp(s->span,spans,sizeof spans) && !memcmp(s->edge,lifecycle_edges,sizeof lifecycle_edges) &&
  !memcmp(s->disposed,call_budget_disposed,sizeof call_budget_disposed);
}
static pangu_i32 call_budget_enter(pangu_u32 binding,int kind,const void *scope,pangu_u64 fuel,
                                   pangu_lifecycle_call_ticket *ticket,pangu_lifecycle_report *report) {
 if(kind==1) return api->setup_call(&live,binding,(const pangu_lifecycle_setup_ticket *)scope,fuel,ticket,report);
 if(kind==2) return api->cleanup_call(&live,binding,(const pangu_cleanup_ticket *)scope,fuel,ticket,report);
 return api->call(&live,binding,fuel,ticket,report);
}
static int call_budget_invoke(pangu_u32 binding) {
 pangu_u32 id=current->lowered.graph.provider_owners[current_output->bindings[binding].provider];
 assert(owners[id].state==PANGU_OWNER_ACTIVE);return (int)id;
}
static void call_budget_exercise(pangu_u32 binding,int kind,const void *scope) {
 pangu_lifecycle_call_ticket ticket,poison;memset(&poison,0xa5,sizeof poison);pangu_lifecycle_report report={0};
 call_budget_snapshot before;call_budget_capture(&before);int calls=call_budget_admissions;pangu_u64 fuel=0;
 for(;fuel<10000;fuel++) {
  ticket=poison;report=(pangu_lifecycle_report){0};
  pangu_i32 rc=call_budget_enter(binding,kind,scope,fuel,&ticket,&report);
  assert(report.work<=fuel && !report.reserved);
  if(!rc) {assert(!report.reason && !report.pending);break;}
  assert(rc==2 && report.reason==1 && report.pending && call_budget_same(&before));
  assert(!memcmp(&ticket,&poison,sizeof ticket) && call_budget_admissions==calls);call_budget_retries++;
 }
 assert(fuel<10000);
 pangu_plan_edge edge=current_output->bindings[binding];
 pangu_u32 consumer=current->lowered.graph.requirements[edge.requirement].consumer;
 pangu_u32 provider=current->lowered.graph.provider_owners[edge.provider];
 assert(owners[consumer].inflight==before.owner[consumer].inflight+1);
 assert(owners[provider].inflight==before.owner[provider].inflight+1);
 assert(call_budget_invoke(binding)==(int)provider);call_budget_admissions++;
 pangu_lifecycle_call_ticket bad=ticket;bad.provider.generation++;
 call_budget_snapshot during;call_budget_capture(&during);
 assert(api->call_finish(&live,&bad)==10 && call_budget_same(&during));
 assert(!api->call_finish(&live,&ticket));assert(api->call_finish(&live,&ticket)==10);
 assert(owners[consumer].inflight==before.owner[consumer].inflight && owners[provider].inflight==before.owner[provider].inflight);

 /* Invalid output/output aliasing is rejected before writing either object. */
 union {pangu_lifecycle_call_ticket ticket;pangu_lifecycle_report report;unsigned char bytes[64];} overlap;
 memset(&overlap,0x5a,sizeof overlap);unsigned char saved[sizeof overlap];memcpy(saved,&overlap,sizeof overlap);
 call_budget_capture(&before);
 assert(call_budget_enter(binding,kind,scope,100,(pangu_lifecycle_call_ticket *)(void *)&overlap,
                          (pangu_lifecycle_report *)(void *)&overlap)==1);
 assert(!memcmp(saved,&overlap,sizeof overlap) && call_budget_same(&before));call_budget_overlap_refusals++;
 if(kind) {
  unsigned char scope_copy[sizeof(pangu_lifecycle_setup_ticket)];
  pangu_u64 bytes=kind==1 ? sizeof(pangu_lifecycle_setup_ticket) : sizeof(pangu_cleanup_ticket);
  memcpy(scope_copy,scope,bytes);memset(&ticket,0xa5,sizeof ticket);pangu_lifecycle_call_ticket output_before=ticket;
  call_budget_capture(&before);
  assert(call_budget_enter(binding,kind,scope,100,&ticket,(pangu_lifecycle_report *)(void *)scope)==1);
  assert(!memcmp(scope_copy,scope,bytes) && !memcmp(&ticket,&output_before,sizeof ticket) && call_budget_same(&before));
  call_budget_overlap_refusals++;
 }
 call_budget_scenarios++;
}
static pangu_i32 call_budget_release(void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;call_budget_cleanup_contexts++;
 pangu_cleanup_ticket scope;assert(!api->cleanup_scope(&live,positions[id],&scope));
 for(pangu_u32 b=0;b<current->graph_report.binding_count;b++) {
  pangu_plan_edge edge=current_output->bindings[b];
  if(current->lowered.graph.requirements[edge.requirement].consumer==id) call_budget_exercise(b,2,&scope);
 }
 return 0;
}
static pangu_i32 call_budget_setup(pangu_owner *owner,void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;setup_count++;
 assert(owner==&owners[id] && owner->inflight==1);
 assert(!api->register_resource(&live,positions[id],call_budget_release,opaque));
 pangu_lifecycle_setup_ticket scope;assert(!api->setup_scope(&live,&scope));
 for(pangu_u32 b=0;b<current->graph_report.binding_count;b++) {
  pangu_plan_edge edge=current_output->bindings[b];
  if(current->lowered.graph.requirements[edge.requirement].consumer==id) call_budget_exercise(b,1,&scope);
 }
 call_budget_setup_contexts++;return 0;
}
static void call_budget_prepare(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 if(live.phase) {
  assert(live.phase==4 && !live.started && !live.setup_waiting);
  for(pangu_u32 i=0;i<9;i++) assert(!owners[i].inflight && !owners[i].count);
 }
 memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;epoch++;
 assert(epoch && plan->lowered.graph.instance_count<=9);
 for(pangu_u32 i=0;i<9;i++) {
  if(!owners[i].abi_version || owners[i].state==PANGU_OWNER_LOADING) memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  source_items[i]=(pangu_lifecycle_item){&owners[i],call_budget_setup,(void *)(pangu_u64)i};
 }
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};pangu_lifecycle_prepared prepared;
 assert(!api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,&storage_view,FUEL,&prepared));
 assert(prepared.item_count==plan->graph_report.order_count);prepared_edge_count=prepared.edge_count;
 for(pangu_u32 i=0;i<prepared.item_count;i++) positions[out->order[i]]=i;
 live.items=activation;live.count=prepared.item_count;
 call_budget_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared.edge_count,call_budget_disposed,9};live.stop_graph=&call_budget_graph;
 memset(call_budget_disposed,0,sizeof call_budget_disposed);current=plan;current_output=out;
 assert(!api->bind_geometry(&live,spans,64,FUEL));
}
static void call_budget_refusal_cases(pangu_u32 binding) {
 pangu_lifecycle_call_ticket holds[3];pangu_lifecycle_report report={0};
 for(pangu_u32 i=0;i<3;i++) {memset(&holds[i],0xa5,sizeof holds[i]);report=(pangu_lifecycle_report){0};assert(!api->call(&live,binding,FUEL,&holds[i],&report));assert(!report.reason&&!report.pending&&report.work<=FUEL);}
 call_budget_snapshot before;call_budget_capture(&before);pangu_lifecycle_call_ticket poison;memset(&poison,0xa5,sizeof poison);pangu_lifecycle_call_ticket poison_before=poison;
 report=(pangu_lifecycle_report){0};assert(api->call(&live,binding,FUEL,&poison,&report)==2);
 assert(!report.reason&&!report.pending&&report.work<=FUEL&&!report.reserved);
 assert(call_budget_same(&before) && !memcmp(&poison,&poison_before,sizeof poison));
 for(pangu_i32 i=2;i>=0;i--) assert(!api->call_finish(&live,&holds[i]));call_budget_quota_refusals++;
}
static void call_budget_generation_case(pangu_u32 binding) {
 pangu_plan_edge edge=current_output->bindings[binding];pangu_u32 consumer=current->lowered.graph.requirements[edge.requirement].consumer;
 pangu_u32 provider=current->lowered.graph.provider_owners[edge.provider];
 operations[consumer][0].generation=0xffffffffU;operations[provider][0].generation=0xffffffffU;
 pangu_lifecycle_call_ticket ticket={0};pangu_lifecycle_report report={0};
 assert(!api->call(&live,binding,FUEL,&ticket,&report)&&!report.reason&&!report.pending);
 assert(ticket.consumer.index==1 && ticket.provider.index==1);assert(!api->call_finish(&live,&ticket));
}
static void call_budget_invalid_case(pangu_u32 binding_count) {
 pangu_lifecycle_call_ticket ticket;memset(&ticket,0xa5,sizeof ticket);pangu_lifecycle_call_ticket poison=ticket;
 call_budget_snapshot before;call_budget_capture(&before);pangu_lifecycle_report report={0};
 assert(api->call(&live,binding_count,FUEL,&ticket,&report)==3 && !report.reason&&!report.pending&&!report.work);
 assert(!memcmp(&ticket,&poison,sizeof ticket) && call_budget_same(&before));call_budget_invalid_bindings++;
}
static void call_budget_run_plan(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 int setup_before=call_budget_setup_contexts,cleanup_before=call_budget_cleanup_contexts;
 call_budget_prepare(plan,out);pangu_lifecycle_report report={0};
 assert(!api->start(&live,FUEL,&report) && !report.reason&&!report.pending);
 assert(setup_count==(int)live.count && call_budget_setup_contexts-setup_before==(int)live.count);
 for(pangu_u32 b=0;b<plan->graph_report.binding_count;b++) call_budget_exercise(b,0,0);
 if(plan->graph_report.binding_count) {
  call_budget_refusal_cases(0);call_budget_generation_case(0);call_budget_invalid_case((pangu_u32)plan->graph_report.binding_count);
 }
 assert(!api->stop(&live,FUEL,&report)&&!report.pending&&report.state==4);
 assert(cleanup_count==0 && call_budget_cleanup_contexts-cleanup_before==(int)live.count);
}
static void call_budget_public_core(void) {
 pangu_prepare_result plan;int valid=0,rejected=0;
 for(pangu_u32 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {
  const host_graph_case *c=&host_graph_cases[i];
  assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,FUEL,&plan)==c->status);
  if(c->status){rejected++;continue;}valid++;call_budget_run_plan(&plan,&corpus_output);
 }
 assert(!prepare(FUEL,&plan));call_budget_run_plan(&plan,&output);
 for(int reverse=0;reverse<2;reverse++) {
  multi_manifest.provisions=reverse?reverse_provisions:provisions;
  assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));
  call_budget_run_plan(&plan,&output);
 }
 assert(valid==95&&rejected==145&&call_budget_scenarios>100);
}


/* T14-F05 public-ABI call and lease lifecycle regression. */
static pangu_u32 call_lifecycle_target;
static pangu_lifecycle_graph call_lifecycle_graph;
static pangu_u32 call_lifecycle_disposed[9];
static int call_lifecycle_mode,call_lifecycle_held,call_lifecycle_cases;
static int call_lifecycle_direct_calls,call_lifecycle_setup_calls,call_lifecycle_cleanup_calls;
static int call_lifecycle_provider_calls,call_lifecycle_rejected_calls,call_lifecycle_async_begins,call_lifecycle_async_begin_total;
static pangu_lifecycle_call_ticket call_lifecycle_retained;
static pangu_lifecycle_setup_ticket call_lifecycle_expired_setup;
static pangu_cleanup_ticket call_lifecycle_retained_scope,call_lifecycle_async_ticket;
static pangu_cleanup_ticket call_lifecycle_async_scope;
static pangu_i32 call_lifecycle_cleanup(void *opaque);
static int call_lifecycle_provider(pangu_u32 binding) {
 pangu_u32 id=current->lowered.graph.provider_owners[current_output->bindings[binding].provider];
 assert(owners[id].state==PANGU_OWNER_ACTIVE);call_lifecycle_provider_calls++;return (int)id;
}
static pangu_u32 call_lifecycle_consumer(pangu_u32 binding) {
 return current->lowered.graph.requirements[current_output->bindings[binding].requirement].consumer;
}
static pangu_u32 call_lifecycle_provider_id(pangu_u32 binding) {
 return current->lowered.graph.provider_owners[current_output->bindings[binding].provider];
}
static void call_lifecycle_rejected(pangu_u32 binding,int kind,const void *scope) {
 pangu_lifecycle_call_ticket ticket,poison;memset(&poison,0xa5,sizeof poison);ticket=poison;
 pangu_lifecycle_report report={0};pangu_i32 rc;
 if(kind==0) rc=api->call(&live,binding,FUEL,&ticket,&report);
 else if(kind==1) rc=api->setup_call(&live,binding,(const pangu_lifecycle_setup_ticket *)scope,FUEL,&ticket,&report);
 else rc=api->cleanup_call(&live,binding,(const pangu_cleanup_ticket *)scope,FUEL,&ticket,&report);
 assert(rc==10 && !memcmp(&ticket,&poison,sizeof ticket));call_lifecycle_rejected_calls++;
}
static pangu_i32 call_lifecycle_async_begin(void *opaque,const pangu_cleanup_ticket *ticket) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;call_lifecycle_async_begins++;call_lifecycle_async_begin_total++;
 call_lifecycle_async_ticket=*ticket;assert(!api->cleanup_scope(&live,positions[id],&call_lifecycle_async_scope));
 assert(!api->cleanup_call(&live,call_lifecycle_target,&call_lifecycle_async_scope,FUEL,&call_lifecycle_retained,
                           &(pangu_lifecycle_report){0}));
 assert(call_lifecycle_provider(call_lifecycle_target)==(int)call_lifecycle_provider_id(call_lifecycle_target));
 call_lifecycle_held=1;return 5;
}
static pangu_i32 call_lifecycle_cleanup(void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;cleanup_count++;
 pangu_cleanup_ticket scope;assert(!api->cleanup_scope(&live,positions[id],&scope));
 for(pangu_u32 b=0;b<current->graph_report.binding_count;b++) {
  if(call_lifecycle_consumer(b)!=id) {call_lifecycle_rejected(b,2,&scope);continue;}
  call_lifecycle_rejected(b,0,0);call_lifecycle_rejected(b,1,&call_lifecycle_expired_setup);
  pangu_lifecycle_call_ticket ticket={0};pangu_lifecycle_report report={0};
  assert(!api->cleanup_call(&live,b,&scope,FUEL,&ticket,&report)&&!report.reason&&!report.pending);
  assert(call_lifecycle_provider(b)==(int)call_lifecycle_provider_id(b));call_lifecycle_cleanup_calls++;
  if(call_lifecycle_mode==1 && b==call_lifecycle_target && !call_lifecycle_held) {
   call_lifecycle_retained=ticket;call_lifecycle_retained_scope=scope;call_lifecycle_held=1;
  } else {
   assert(!api->call_finish(&live,&ticket));assert(api->call_finish(&live,&ticket)==10);
  }
 }
 return 0;
}
static pangu_i32 call_lifecycle_setup(pangu_owner *owner,void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;setup_count++;
 assert(owner==&owners[id] && owner->inflight==1);
 assert(!api->register_resource(&live,positions[id],call_lifecycle_cleanup,opaque));
 pangu_lifecycle_setup_ticket scope;assert(!api->setup_scope(&live,&scope));call_lifecycle_expired_setup=scope;
 for(pangu_u32 b=0;b<current->graph_report.binding_count;b++) if(call_lifecycle_consumer(b)==id) {
  call_lifecycle_rejected(b,0,0);
  pangu_lifecycle_call_ticket ticket={0};pangu_lifecycle_report report={0};
  assert(!api->setup_call(&live,b,&scope,FUEL,&ticket,&report)&&!report.reason&&!report.pending);
  assert(call_lifecycle_provider(b)==(int)call_lifecycle_provider_id(b));call_lifecycle_setup_calls++;
  assert(!api->call_finish(&live,&ticket));assert(api->call_finish(&live,&ticket)==10);
 }
 if(call_lifecycle_mode==2 && current->graph_report.binding_count && call_lifecycle_consumer(call_lifecycle_target)==id)
  assert(!api->register_async(&live,positions[id],call_lifecycle_async_begin,opaque));
 return 0;
}
static void call_lifecycle_prepare(const pangu_prepare_result *plan,const pangu_graph_output *out,pangu_u32 binding,int mode) {
 if(live.phase) {assert(live.phase==4&&!live.started&&!live.setup_waiting);for(pangu_u32 i=0;i<9;i++)assert(!owners[i].inflight&&!owners[i].count);}
 memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;epoch++;
 assert(epoch&&plan->lowered.graph.instance_count<=9);
 for(pangu_u32 i=0;i<9;i++) {
  if(!owners[i].abi_version||owners[i].state==PANGU_OWNER_LOADING)memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  source_items[i]=(pangu_lifecycle_item){&owners[i],call_lifecycle_setup,(void *)(pangu_u64)i};
 }
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};pangu_lifecycle_prepared prepared;
 assert(!api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,&storage_view,FUEL,&prepared));
 assert(prepared.item_count==plan->graph_report.order_count);prepared_edge_count=prepared.edge_count;
 for(pangu_u32 i=0;i<prepared.item_count;i++)positions[out->order[i]]=i;
 live.items=activation;live.count=prepared.item_count;call_lifecycle_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared.edge_count,call_lifecycle_disposed,9};
 memset(call_lifecycle_disposed,0,sizeof call_lifecycle_disposed);live.stop_graph=&call_lifecycle_graph;
 current=plan;current_output=out;call_lifecycle_target=binding;call_lifecycle_mode=mode;call_lifecycle_held=0;
 call_lifecycle_async_begins=0;memset(&call_lifecycle_retained,0,sizeof call_lifecycle_retained);
 memset(&call_lifecycle_retained_scope,0,sizeof call_lifecycle_retained_scope);memset(&call_lifecycle_async_ticket,0,sizeof call_lifecycle_async_ticket);
 memset(&call_lifecycle_async_scope,0,sizeof call_lifecycle_async_scope);
 assert(!api->bind_geometry(&live,spans,64,FUEL));
}
static void call_lifecycle_case(const pangu_prepare_result *plan,const pangu_graph_output *out,pangu_u32 binding,int mode) {
 call_lifecycle_prepare(plan,out,binding,mode);pangu_lifecycle_report report={0};
 assert(!api->start(&live,FUEL,&report)&&!report.reason&&!report.pending&&setup_count==(int)live.count);
 pangu_u32 consumer=call_lifecycle_consumer(binding),provider=call_lifecycle_provider_id(binding);
 pangu_lifecycle_setup_ticket ended;assert(api->setup_scope(&live,&ended)==10);
 call_lifecycle_rejected(binding,1,&call_lifecycle_expired_setup);
 if(!mode) {
  assert(!api->call(&live,binding,FUEL,&call_lifecycle_retained,&report));
  assert(call_lifecycle_provider(binding)==(int)provider);call_lifecycle_direct_calls++;call_lifecycle_held=1;
 }
 report=(pangu_lifecycle_report){0};assert(api->stop(&live,FUEL,&report)==5&&report.pending);
 assert(owners[consumer].state==PANGU_OWNER_QUIESCING&&owners[provider].state==PANGU_OWNER_ACTIVE);
 call_lifecycle_rejected(binding,0,0);
 if(mode==1) {
  assert(call_lifecycle_held);pangu_cleanup_ticket expired;
  assert(api->cleanup_scope(&live,positions[consumer],&expired)==10);
  call_lifecycle_rejected(binding,2,&call_lifecycle_retained_scope);
 } else if(mode==2) {
  assert(call_lifecycle_held&&call_lifecycle_async_begins==1);
  assert(!api->disposer_finish(&live,positions[consumer],&call_lifecycle_async_ticket,0));
  call_lifecycle_rejected(binding,2,&call_lifecycle_async_scope);
  report=(pangu_lifecycle_report){0};assert(api->stop(&live,FUEL,&report)==5&&report.pending);
  assert(owners[provider].state==PANGU_OWNER_ACTIVE);
 }
 pangu_lifecycle_call_ticket forged=call_lifecycle_retained;forged.provider.generation++;
 pangu_u32 ci=owners[consumer].inflight,pi=owners[provider].inflight;
 assert(api->call_finish(&live,&forged)==10&&owners[consumer].inflight==ci&&owners[provider].inflight==pi);
 assert(!api->call_finish(&live,&call_lifecycle_retained));assert(api->call_finish(&live,&call_lifecycle_retained)==10);
 assert(!api->stop(&live,FUEL,&report)&&!report.pending&&report.state==4);
 assert(!live.started&&owners[consumer].state==PANGU_OWNER_DISPOSED&&owners[provider].state==PANGU_OWNER_DISPOSED);
 call_lifecycle_cases++;
}
static void call_lifecycle_public_core(void) {
 pangu_prepare_result plan;int valid=0,rejected=0;
 for(pangu_u32 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {
  const host_graph_case *c=&host_graph_cases[i];
  assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,FUEL,&plan)==c->status);
  if(c->status){rejected++;continue;}valid++;
  for(pangu_u32 b=0;b<plan.graph_report.binding_count;b++) for(int mode=0;mode<3;mode++) call_lifecycle_case(&plan,&corpus_output,b,mode);
 }
 assert(!prepare(FUEL,&plan));for(int mode=0;mode<3;mode++) call_lifecycle_case(&plan,&output,0,mode);
 for(int reverse=0;reverse<2;reverse++) {
  multi_manifest.provisions=reverse?reverse_provisions:provisions;
  assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));
  for(pangu_u32 b=0;b<plan.graph_report.binding_count;b++) for(int mode=0;mode<3;mode++) call_lifecycle_case(&plan,&output,b,mode);
 }
 assert(valid==95&&rejected==145&&call_lifecycle_cases>0);
 assert(call_lifecycle_direct_calls==call_lifecycle_cases/3 && call_lifecycle_async_begin_total==call_lifecycle_cases/3);
}


/* T14-F08 public-ABI protected geometry regression. */
static int geometry_refused,geometry_matrices,geometry_cases;
typedef struct {
 pangu_lifecycle_span ranges[64];pangu_u64 count,used;unsigned char bytes[32768];
} public_geometry_snapshot;
static void geometry_capture(pangu_lifecycle_runtime *r,public_geometry_snapshot *s) {
 assert(r->protected_count<=64);s->count=r->protected_count;s->used=0;
 memcpy(s->ranges,r->protected_spans,s->count*sizeof(pangu_lifecycle_span));
 for(pangu_u64 i=0;i<s->count;i++) {
  pangu_u64 n=s->ranges[i].end-s->ranges[i].begin;assert(n<=sizeof(s->bytes)-s->used);
  memcpy(s->bytes+s->used,(const void *)s->ranges[i].begin,n);s->used+=n;
 }
}
static int geometry_unchanged(const public_geometry_snapshot *s) {
 pangu_u64 offset=0;
 for(pangu_u64 i=0;i<s->count;i++) {
  pangu_u64 n=s->ranges[i].end-s->ranges[i].begin;
  if(memcmp(s->bytes+offset,(const void *)s->ranges[i].begin,n))return 0;offset+=n;
 }
 return 1;
}
static void geometry_refuse_matrix(pangu_lifecycle_runtime *r,const pangu_lifecycle_setup_ticket *setup,
                                   const pangu_cleanup_ticket *cleanup_scope_value) {
 geometry_matrices++;public_geometry_snapshot before;geometry_capture(r,&before);
 for(pangu_u64 i=0;i<r->protected_count;i++) {
  void *bad=(void *)r->protected_spans[i].begin;
  pangu_lifecycle_call_ticket call,call_before;pangu_lifecycle_report report,report_before;
  pangu_lifecycle_setup_ticket setup_out,setup_out_before;pangu_cleanup_ticket cleanup_out,cleanup_out_before;
  #define GEO_PREP() do { memset(&call,0xa5,sizeof call);call_before=call;memset(&report,0xa5,sizeof report);report_before=report;memset(&setup_out,0xa5,sizeof setup_out);setup_out_before=setup_out;memset(&cleanup_out,0xa5,sizeof cleanup_out);cleanup_out_before=cleanup_out;} while(0)
  #define GEO_REFUSE(expr) do { GEO_PREP();assert((expr)==1);geometry_refused++;assert(!memcmp(&call,&call_before,sizeof call)&&!memcmp(&report,&report_before,sizeof report)&&!memcmp(&setup_out,&setup_out_before,sizeof setup_out)&&!memcmp(&cleanup_out,&cleanup_out_before,sizeof cleanup_out));assert(geometry_unchanged(&before));} while(0)
  GEO_REFUSE(api->setup_scope(r,(pangu_lifecycle_setup_ticket *)bad));
  GEO_REFUSE(api->setup_defer(r,(pangu_lifecycle_setup_ticket *)bad));
  GEO_REFUSE(api->setup_finish(r,(const pangu_lifecycle_setup_ticket *)bad,0));
  GEO_REFUSE(api->cleanup_scope(r,0,(pangu_cleanup_ticket *)bad));
  GEO_REFUSE(api->stop(r,1,(pangu_lifecycle_report *)bad));
  GEO_REFUSE(api->start(r,1,(pangu_lifecycle_report *)bad));
  GEO_REFUSE(api->pump(r,1,(pangu_lifecycle_report *)bad));
  GEO_REFUSE(api->disposer_finish(r,0,(const pangu_cleanup_ticket *)bad,0));
  GEO_REFUSE(api->call(r,0,1,(pangu_lifecycle_call_ticket *)bad,&report));
  GEO_REFUSE(api->call(r,0,1,&call,(pangu_lifecycle_report *)bad));
  GEO_REFUSE(api->setup_call(r,0,setup,1,(pangu_lifecycle_call_ticket *)bad,&report));
  GEO_REFUSE(api->setup_call(r,0,setup,1,&call,(pangu_lifecycle_report *)bad));
  GEO_REFUSE(api->cleanup_call(r,0,cleanup_scope_value,1,(pangu_lifecycle_call_ticket *)bad,&report));
  GEO_REFUSE(api->cleanup_call(r,0,cleanup_scope_value,1,&call,(pangu_lifecycle_report *)bad));
  GEO_REFUSE(api->call_finish(r,(const pangu_lifecycle_call_ticket *)bad));
  GEO_REFUSE(api->setup_call(r,0,setup,1,(pangu_lifecycle_call_ticket *)((pangu_u64)~(pangu_u64)0-7U),&report));
  GEO_REFUSE(api->call_finish(r,(const pangu_lifecycle_call_ticket *)((const unsigned char *)&call+1)));
  #undef GEO_REFUSE
  #undef GEO_PREP
 }
 assert(geometry_unchanged(&before));
}
static pangu_i32 geometry_public_cleanup(void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;cleanup_count++;
 pangu_cleanup_ticket scope;assert(!api->cleanup_scope(&live,positions[id],&scope));
 pangu_lifecycle_setup_ticket stale={0};geometry_refuse_matrix(&live,&stale,&scope);
 union {pangu_u64 alignment;pangu_cleanup_ticket cleanup;pangu_lifecycle_call_ticket call;unsigned char bytes[96];} overlap;
 memset(&overlap,0xa5,sizeof overlap);overlap.cleanup=scope;unsigned char saved[96];memcpy(saved,overlap.bytes,sizeof saved);
 public_geometry_snapshot before;geometry_capture(&live,&before);pangu_lifecycle_report report={0};
 assert(api->cleanup_call(&live,0,&overlap.cleanup,FUEL,&overlap.call,&report)==1);
 assert(!memcmp(saved,overlap.bytes,sizeof saved)&&geometry_unchanged(&before));geometry_refused++;
 return 0;
}
static pangu_i32 geometry_public_setup(pangu_owner *owner,void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;setup_count++;
 assert(owner==&owners[id]&&owner->inflight==1);
 assert(!api->register_resource(&live,positions[id],geometry_public_cleanup,opaque));
 pangu_lifecycle_setup_ticket scope;assert(!api->setup_scope(&live,&scope));pangu_cleanup_ticket no_cleanup={0};
 geometry_refuse_matrix(&live,&scope,&no_cleanup);
 union {pangu_u64 alignment;pangu_lifecycle_setup_ticket setup;pangu_lifecycle_call_ticket call;unsigned char bytes[96];} overlap;
 memset(&overlap,0xa5,sizeof overlap);overlap.setup=scope;unsigned char saved[96];memcpy(saved,overlap.bytes,sizeof saved);
 public_geometry_snapshot before;geometry_capture(&live,&before);pangu_lifecycle_report report={0};
 assert(api->setup_call(&live,0,&overlap.setup,FUEL,&overlap.call,&report)==1);
 assert(!memcmp(saved,overlap.bytes,sizeof saved)&&geometry_unchanged(&before));geometry_refused++;
 return 0;
}
static void geometry_public_prepare(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 if(live.phase) {assert(live.phase==4&&!live.started&&!live.setup_waiting);for(pangu_u32 i=0;i<9;i++)assert(!owners[i].inflight&&!owners[i].count);}
 memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;epoch++;
 assert(epoch&&plan->lowered.graph.instance_count<=9);
 for(pangu_u32 i=0;i<9;i++) {
  if(!owners[i].abi_version||owners[i].state==PANGU_OWNER_LOADING)memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  source_items[i]=(pangu_lifecycle_item){&owners[i],geometry_public_setup,(void *)(pangu_u64)i};
 }
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};pangu_lifecycle_prepared prepared;
 assert(!api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,&storage_view,FUEL,&prepared));
 assert(prepared.item_count==plan->graph_report.order_count);prepared_edge_count=prepared.edge_count;
 for(pangu_u32 i=0;i<prepared.item_count;i++)positions[out->order[i]]=i;
 live.items=activation;live.count=prepared.item_count;budget_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared.edge_count,budget_disposed,9};
 memset(budget_disposed,0,sizeof budget_disposed);live.stop_graph=&budget_graph;current=plan;current_output=out;
}
static void geometry_public_case(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 geometry_public_prepare(plan,out);pangu_lifecycle_report report={0};
 assert(!api->bind_geometry(&live,spans,64,FUEL));
 assert(!api->start(&live,FUEL,&report)&&!report.reason&&!report.pending);
 pangu_lifecycle_setup_ticket stale={0};pangu_cleanup_ticket no_cleanup={0};
 geometry_refuse_matrix(&live,&stale,&no_cleanup);
 assert(!api->stop(&live,FUEL,&report)&&!report.pending&&!report.reason);
 geometry_refuse_matrix(&live,&stale,&no_cleanup);geometry_cases++;
}
static void geometry_public_builder_tests(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 pangu_lifecycle_runtime test={0},before; pangu_lifecycle_item items[9];pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};
 pangu_lifecycle_prepared prepared; pangu_u64 n=plan->lowered.graph.instance_count;assert(n<=9);epoch++;
 for(pangu_u32 i=0;i<n;i++) {
  if(!owners[i].abi_version||owners[i].state==PANGU_OWNER_LOADING)memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  items[i]=(pangu_lifecycle_item){&owners[i],geometry_public_setup,(void *)(pangu_u64)i};
 }
 assert(!api->prepare(plan,out,items,n,&storage_view,FUEL,&prepared));
 test.items=activation;test.count=prepared.item_count;
 pangu_lifecycle_graph graph={lifecycle_edges,prepared.edge_count,budget_disposed,9};test.stop_graph=&graph;
 before=test;pangu_lifecycle_report report={0};assert(api->start(&test,FUEL,&report)==1&&!memcmp(&test,&before,sizeof test));geometry_refused++;
 assert(api->bind_geometry(&test,(pangu_lifecycle_span *)(void *)&test,64,FUEL)==1&&!memcmp(&test,&before,sizeof test));geometry_refused++;
 assert(api->bind_geometry(&test,(pangu_lifecycle_span *)(void *)resources,64,FUEL)==1&&!memcmp(&test,&before,sizeof test));geometry_refused++;
 assert(api->bind_geometry(&test,(pangu_lifecycle_span *)((unsigned char *)spans+1),63,FUEL)==1&&!memcmp(&test,&before,sizeof test));geometry_refused++;
 if(n>1) {owners[1].resources=owners[0].resources;assert(api->bind_geometry(&test,spans,64,FUEL)==1&&!memcmp(&test,&before,sizeof test));owners[1].resources=resources[1];geometry_refused++;}
 pangu_owner *owner_ptr=activation[0].owner;activation[0].owner=(pangu_owner *)((unsigned char *)owner_ptr+1);
 assert(api->bind_geometry(&test,spans,64,FUEL)==1&&!memcmp(&test,&before,sizeof test));activation[0].owner=owner_ptr;geometry_refused++;
 pangu_u64 cap=owners[0].capacity;owners[0].capacity=(pangu_u64)~(pangu_u64)0/24+1;
 assert(api->bind_geometry(&test,spans,64,FUEL)==1&&!memcmp(&test,&before,sizeof test));owners[0].capacity=cap;geometry_refused++;
 pangu_u64 fuel=0;
 for(;fuel<100000;fuel++) {int rc=api->bind_geometry(&test,spans,64,fuel);if(!rc)break;assert(rc==2&&!memcmp(&test,&before,sizeof test));}
 assert(fuel<100000&&test.protected_count>0);
 for(pangu_u64 i=1;i<test.protected_count;i++)assert(spans[i-1].end<=spans[i].begin);
 pangu_lifecycle_runtime bound=test;pangu_lifecycle_span bound_spans[64];memcpy(bound_spans,spans,sizeof spans);
 assert(api->bind_geometry(&test,spans,64,FUEL)==10&&!memcmp(&test,&bound,sizeof test)&&!memcmp(spans,bound_spans,sizeof spans));geometry_refused++;
 /* Public owner_init must reject misaligned frames and backing arrays before mutation. */
 union {pangu_u64 alignment;unsigned char bytes[256];} raw;memset(&raw,0,sizeof raw);
 pangu_owner *aligned=(pangu_owner *)(void *)raw.bytes;pangu_owned_resource local_resources[2];pangu_owned_operation local_ops[2];
 pangu_owner copy=*aligned;assert(api->owner_init((pangu_owner *)((unsigned char *)aligned+1),local_resources,2,local_ops,2,1,FUEL)==1&&!memcmp(aligned,&copy,sizeof copy));geometry_refused++;
}
static void geometry_public_core(void) {
 pangu_prepare_result plan;int valid=0,rejected=0;
 assert(!prepare(FUEL,&plan));geometry_public_builder_tests(&plan,&output);
 for(pangu_u32 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {
  const host_graph_case *c=&host_graph_cases[i];
  assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,FUEL,&plan)==c->status);
  if(c->status){rejected++;continue;}valid++;geometry_public_case(&plan,&corpus_output);
 }
 assert(!prepare(FUEL,&plan));geometry_public_case(&plan,&output);
 for(int reverse=0;reverse<2;reverse++) {
  multi_manifest.provisions=reverse?reverse_provisions:provisions;
  assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));
  geometry_public_case(&plan,&output);
 }
 assert(valid==95&&rejected==145&&geometry_cases==98&&geometry_matrices>0&&geometry_refused>0);
}


/* T14-F09 public-ABI dependency graph teardown regression. */
static int graph_public_scenarios,graph_public_independent,graph_public_ancestors;
static int graph_public_independent_setup,graph_public_error_waiting,graph_public_edge_cases;
static int graph_public_async_begins,graph_public_call_holds,graph_public_owner_hold_skips;
static int graph_public_mode,graph_public_selected,graph_public_error_id,graph_public_held;
static pangu_u32 graph_public_binding;
static pangu_lifecycle_setup_ticket graph_public_pending_setup;
static pangu_cleanup_ticket graph_public_pending_cleanup;
static pangu_lifecycle_call_ticket graph_public_pending_call;
static pangu_lifecycle_graph graph_public_graph;
static pangu_u32 graph_public_disposed[9],graph_public_protected[9];
static pangu_i32 graph_public_cleanup(void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;pangu_i32 rc=cleanup(opaque);
 pangu_lifecycle_report report={0};assert(api->stop(&live,FUEL,&report)==5);
 return (int)id==graph_public_error_id?-93:rc;
}
static pangu_i32 graph_public_async_cleanup(void *opaque,const pangu_cleanup_ticket *ticket) {
 (void)opaque;graph_public_pending_cleanup=*ticket;graph_public_async_begins++;return 5;
}
static pangu_i32 graph_public_setup(pangu_owner *owner,void *opaque) {
 int id=(int)(pangu_u64)opaque;setup_count++;assert(owner->inflight==1);
 assert(!api->register_resource(&live,positions[id],graph_public_cleanup,opaque));
 if(id!=graph_public_selected)return 0;
 if(graph_public_mode==0||graph_public_mode==4) {
  pangu_lifecycle_setup_ticket scope;assert(!api->setup_scope(&live,&scope));
  pangu_lifecycle_report report={0};
  assert(!api->setup_call(&live,graph_public_binding,&scope,FUEL,&graph_public_pending_call,&report));
  assert(!report.reason&&!report.pending);graph_public_held=1;return 0;
 }
 if(graph_public_mode==1) assert(!api->register_async(&live,positions[id],graph_public_async_cleanup,opaque));
 if(graph_public_mode==2) {assert(!api->setup_defer(&live,&graph_public_pending_setup));return 5;}
 return graph_public_mode==3?-72:0;
}
static void graph_public_init_owners(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 pangu_u64 n=plan->lowered.graph.instance_count;assert(n<=9);epoch++;
 for(pangu_u32 i=0;i<n;i++) {
  if(!owners[i].abi_version||owners[i].state==PANGU_OWNER_LOADING)memset(&owners[i],0,sizeof owners[i]);
  assert(!api->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));
  source_items[i]=(pangu_lifecycle_item){&owners[i],graph_public_setup,(void *)(pangu_u64)i};
 }
 current=plan;current_output=out;
}
static pangu_lifecycle_prepared graph_public_prepare(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 graph_public_init_owners(plan,out);memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};
 pangu_lifecycle_prepared prepared={0};assert(!api->prepare(plan,out,source_items,plan->lowered.graph.instance_count,&storage_view,FUEL,&prepared));
 assert(prepared.item_count==plan->graph_report.order_count);prepared_edge_count=prepared.edge_count;
 for(pangu_u32 i=0;i<prepared.item_count;i++)positions[out->order[i]]=i;
 return prepared;
}
static void graph_public_edge_refusals(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 graph_public_init_owners(plan,out);pangu_u64 n=plan->lowered.graph.instance_count;
 pangu_lifecycle_storage storage_view={activation,9,lifecycle_edges,1000};pangu_lifecycle_prepared info={999,998,997};
 memset(activation,0xa5,sizeof activation);memset(lifecycle_edges,0xa5,sizeof lifecycle_edges);
 pangu_lifecycle_item item_before[9];pangu_lifecycle_edge edge_before[1000];
 memcpy(item_before,activation,sizeof activation);memcpy(edge_before,lifecycle_edges,sizeof lifecycle_edges);
 pangu_u64 fuel=1;for(;fuel<100000;fuel++) {
  pangu_i32 rc=api->prepare(plan,out,source_items,n,&storage_view,fuel,&info);
  if(!rc)break;
  assert(rc==2&&info.item_count==999&&info.edge_count==998&&info.work==997);
  assert(!memcmp(item_before,activation,sizeof activation)&&!memcmp(edge_before,lifecycle_edges,sizeof lifecycle_edges));
 }
 assert(fuel<100000&&info.item_count==plan->graph_report.order_count&&info.edge_count==plan->graph_report.binding_count&&info.work<=fuel);
 for(pangu_u32 b=0;b<info.edge_count;b++) {
  pangu_plan_edge edge=out->bindings[b];pangu_u32 consumer=plan->lowered.graph.requirements[edge.requirement].consumer;
  pangu_u32 provider=plan->lowered.graph.provider_owners[edge.provider];
  assert(out->order[lifecycle_edges[b].consumer]==consumer&&out->order[lifecycle_edges[b].provider]==provider);
 }
 if(info.edge_count) {
  pangu_lifecycle_storage short_storage={activation,9,lifecycle_edges,info.edge_count-1};
  memset(activation,0xa5,sizeof activation);memset(lifecycle_edges,0xa5,sizeof lifecycle_edges);
  memcpy(item_before,activation,sizeof activation);memcpy(edge_before,lifecycle_edges,sizeof lifecycle_edges);info=(pangu_lifecycle_prepared){999,998,997};
  assert(api->prepare(plan,out,source_items,n,&short_storage,FUEL,&info)==2);
  assert(info.item_count==999&&info.edge_count==998&&info.work==997&&!memcmp(item_before,activation,sizeof activation)&&!memcmp(edge_before,lifecycle_edges,sizeof lifecycle_edges));
 }
 graph_public_edge_cases++;
}
static pangu_u32 graph_public_find_consumer_binding(const pangu_prepare_result *plan,const pangu_graph_output *out,pangu_u32 owner) {
 for(pangu_u32 b=0;b<plan->graph_report.binding_count;b++) {
  pangu_plan_edge edge=out->bindings[b];
  if(plan->lowered.graph.requirements[edge.requirement].consumer==owner)return b;
 }
 return 0xffffffffU;
}
static void graph_public_drive(const pangu_prepare_result *plan,const pangu_graph_output *out,pangu_u32 pos,int mode) {
 pangu_u32 selected=(pangu_u32)out->order[pos];pangu_u32 binding=graph_public_find_consumer_binding(plan,out,selected);
 if((mode==0||mode==4)&&binding==0xffffffffU) {graph_public_owner_hold_skips++;return;}
 graph_public_prepare(plan,out);graph_public_selected=(int)selected;graph_public_mode=mode;graph_public_binding=binding;
 graph_public_error_id=-1;graph_public_held=0;graph_public_async_begins=0;memset(&graph_public_pending_call,0,sizeof graph_public_pending_call);
 memset(&graph_public_pending_cleanup,0,sizeof graph_public_pending_cleanup);memset(&graph_public_pending_setup,0,sizeof graph_public_pending_setup);
 memset(graph_public_disposed,0,sizeof graph_public_disposed);
 graph_public_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared_edge_count,graph_public_disposed,9};
 live.items=activation;live.count=plan->graph_report.order_count;live.stop_graph=&graph_public_graph;
 for(pangu_u64 i=0;i<live.count;i++)graph_public_protected[i]=0;
 pangu_u64 launched=mode==2||mode==3?pos+1:live.count;graph_public_protected[pos]=1;
 for(pangu_u64 pass=0;pass<launched;pass++)for(pangu_u64 i=0;i<prepared_edge_count;i++)
  if(lifecycle_edges[i].consumer<launched&&graph_public_protected[lifecycle_edges[i].consumer])graph_public_protected[lifecycle_edges[i].provider]=1;
 if(mode==4)for(pangu_u64 i=0;i<launched;i++)if(!graph_public_protected[i]){graph_public_error_id=(int)out->order[i];break;}
 assert(!api->bind_geometry(&live,spans,64,FUEL));pangu_lifecycle_report report={0};int rc=api->start(&live,FUEL,&report);
 if(mode==3) {assert(rc==-72&&!live.started&&cleanup_count==(int)launched);return;}
 assert(rc==(mode==2?5:0));
 assert(api->stop(&live,FUEL,&report)==5&&report.pending);
 pangu_u64 retained=0,finished=0;
 for(pangu_u64 i=0;i<plan->graph_report.order_count;i++) {
  pangu_u32 id=out->order[i];
  if(i>=launched){assert(owners[id].state==PANGU_OWNER_LOADING&&!graph_public_disposed[i]);continue;}
  if(graph_public_protected[i]) {retained++;assert(!graph_public_disposed[i]);assert(owners[id].state==(i==pos?PANGU_OWNER_QUIESCING:PANGU_OWNER_ACTIVE));}
  else {finished++;assert(graph_public_disposed[i]&&owners[id].state==PANGU_OWNER_DISPOSED);}
 }
 if(finished){graph_public_independent++;if(mode==2)graph_public_independent_setup++;}
 if(retained>1)graph_public_ancestors++;
 assert(live.started==retained&&live.launched==launched);int cleanup_before=cleanup_count;
 assert(api->stop(&live,FUEL,&report)==5&&cleanup_count==cleanup_before);
 if(mode==4&&graph_public_error_id>=0){assert(live.cleanup_error==-93);graph_public_error_waiting++;}
 if(mode==0||mode==4) {
  assert(graph_public_held);pangu_lifecycle_call_ticket bad=graph_public_pending_call;bad.provider.generation++;
  pangu_u32 ci=owners[selected].inflight;pangu_u32 provider=plan->lowered.graph.provider_owners[out->bindings[binding].provider];pangu_u32 pi=owners[provider].inflight;
  assert(api->call_finish(&live,&bad)==10&&owners[selected].inflight==ci&&owners[provider].inflight==pi);
  assert(!api->call_finish(&live,&graph_public_pending_call));assert(api->call_finish(&live,&graph_public_pending_call)==10);graph_public_call_holds++;
 }
 if(mode==1){assert(graph_public_async_begins==1);assert(!api->disposer_finish(&live,positions[selected],&graph_public_pending_cleanup,0));}
 if(mode==2){assert(!api->setup_finish(&live,&graph_public_pending_setup,0));assert(api->setup_finish(&live,&graph_public_pending_setup,0)==10);}
 int expected=mode==4&&graph_public_error_id>=0?-93:0;
 assert(api->stop(&live,FUEL,&report)==expected&&!report.pending);
 assert(!live.started&&cleanup_count==(int)launched);
 for(pangu_u64 i=0;i<launched;i++)assert(graph_public_disposed[i]&&owners[out->order[i]].state==PANGU_OWNER_DISPOSED);
 assert(api->stop(&live,FUEL,&report)==expected&&cleanup_count==(int)launched);graph_public_scenarios++;
}
static void graph_public_invalid_graph(const pangu_prepare_result *plan,const pangu_graph_output *out) {
 pangu_lifecycle_prepared prepared=graph_public_prepare(plan,out);assert(prepared.edge_count>0);
 graph_public_graph=(pangu_lifecycle_graph){lifecycle_edges,prepared.edge_count,graph_public_disposed,9};
 memset(graph_public_disposed,0,sizeof graph_public_disposed);live.items=activation;live.count=prepared.item_count;live.stop_graph=&graph_public_graph;
 assert(!api->bind_geometry(&live,spans,64,FUEL));pangu_lifecycle_edge saved=lifecycle_edges[0];pangu_lifecycle_report report={0};
 lifecycle_edges[0]=(pangu_lifecycle_edge){0,1};assert(api->start(&live,FUEL,&report)==16&&!setup_count);
 lifecycle_edges[0]=saved;graph_public_disposed[0]=1;assert(api->start(&live,FUEL,&report)==10&&!setup_count);
}
static void graph_public_core(void) {
 pangu_prepare_result plan;int valid=0,rejected=0;
 for(pangu_u32 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {
  const host_graph_case *c=&host_graph_cases[i];
  assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,FUEL,&plan)==c->status);
  if(c->status){rejected++;continue;}valid++;graph_public_edge_refusals(&plan,&corpus_output);
  for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++)for(int mode=0;mode<5;mode++)graph_public_drive(&plan,&corpus_output,pos,mode);
 }
 assert(!prepare(FUEL,&plan));graph_public_edge_refusals(&plan,&output);
 for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++)for(int mode=0;mode<5;mode++)graph_public_drive(&plan,&output,pos,mode);
 for(int reverse=0;reverse<2;reverse++) {
  multi_manifest.provisions=reverse?reverse_provisions:provisions;
  assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));graph_public_edge_refusals(&plan,&output);
  for(pangu_u32 pos=0;pos<plan.graph_report.order_count;pos++)for(int mode=0;mode<5;mode++)graph_public_drive(&plan,&output,pos,mode);
 }
 multi_manifest.provisions=provisions;assert(!prepare(FUEL,&plan));graph_public_invalid_graph(&plan,&output);
 assert(valid==95&&rejected==145&&graph_public_scenarios>0&&graph_public_edge_cases==98);
 assert(graph_public_independent>0&&graph_public_ancestors>0&&graph_public_independent_setup>0&&graph_public_error_waiting>0);
}

static int start_selected,start_mode,start_async_begins,start_prefix;static pangu_lifecycle_setup_ticket deferred_start;static pangu_cleanup_ticket deferred_stop;
static pangu_i32 start_async_cleanup(void *ctx,const pangu_cleanup_ticket *t) {(void)ctx;deferred_stop=*t;start_async_begins++;return 5;}
static pangu_i32 start_setup(pangu_owner *o,void *ctx) {
 int id=(int)(pangu_u64)ctx;assert(o==&owners[id]&&o->inflight==1);setup_count++;
 assert(!api->register_resource(&live,positions[id],cleanup,ctx));
 if(id!=start_selected)return 0;
 if(start_mode==9){assert(!api->register_async(&live,positions[id],start_async_cleanup,ctx));return -71;}
 if(start_mode==2||start_mode==3){assert(!api->setup_defer(&live,&deferred_start));return 5;}
 if(start_mode==8){pangu_lifecycle_report r;assert(api->stop(&live,FUEL,&r)==5);}
 return start_mode==1?-71:0;
}
static void start_prepare(const pangu_prepare_result *p,const pangu_graph_output *out) {
 pangu_lifecycle_prepared prepared=graph_public_prepare(p,out);
 for(pangu_u64 i=0;i<prepared.item_count;i++)activation[i].setup=start_setup;
}
static int callbacks(void) {return setup_count+cleanup_count+start_async_begins;}
static int start_case(const pangu_prepare_result *p,const pangu_graph_output *out,pangu_u32 pos,int mode,pangu_u64 quantum) {
 pangu_lifecycle_edge es[1000];pangu_u32 disposed[9]={0};pangu_u64 ec=0,count=0,n=p->lowered.graph.instance_count;
 start_prepare(p,out);start_mode=mode;start_selected=pos<p->graph_report.order_count ? (int)out->order[pos] : -1;start_async_begins=0;


 if(mode==4 || mode==5) for(pangu_u32 j=0;j<(mode==4 ? 2U : 3U);j++) operations[start_selected][j].generation=0xffffffffU;
 ec=prepared_edge_count;count=p->graph_report.order_count;memcpy(es,lifecycle_edges,ec*sizeof *es);pangu_lifecycle_graph graph={es,ec,disposed,9};live.items=activation;live.count=count;live.stop_graph=start_prefix ? 0 : &graph;assert(!api->bind_geometry(&live,spans,64,100000));
 pangu_lifecycle_runtime before=live;pangu_lifecycle_report report;assert(api->start(&live,0,&report)==2 && !report.work && report.pending && report.reason==1 && !memcmp(&before,&live,sizeof live));
 int ticks=0,rc=0,done=0,first=1;for(;ticks<100000;ticks++) {
  if(!first && report.pending) {before=live;int z=api->pump(&live,0,&report);assert(z==2 && !report.work && report.reason==1 && !memcmp(&before,&live,sizeof live));}
  int cb=callbacks();rc=first ? api->start(&live,quantum,&report) : api->pump(&live,quantum,&report);first=0;
  assert(report.work<=quantum && callbacks()-cb<=(int)report.work && !live.busy);
  if(!report.pending) break;
  if(report.reason==1) {assert(rc==2);continue;}
  assert(report.reason==2 && rc==5 && !done);done=1;
  if(mode==2 || mode==3) {
   assert(live.setup_waiting && live.items[live.started-1].owner->inflight==1);
   cb=callbacks();pangu_lifecycle_report waiting;assert(api->pump(&live,quantum,&waiting)==5 && waiting.reason==2 && !waiting.work && callbacks()==cb);
   if(mode==3) {int prior=callbacks();int stop_rc=api->stop(&live,quantum,&waiting);assert((stop_rc==2 || stop_rc==5) && waiting.pending);assert(waiting.work<=quantum && callbacks()-prior<=(int)waiting.work && live.stop_requested && owners[start_selected].state==3);}
   assert(!api->setup_finish(&live,&deferred_start,0));assert(api->setup_finish(&live,&deferred_start,0)==10);
  } else {assert(mode==9 && start_async_begins==1);assert(!api->disposer_finish(&live,pos,&deferred_stop,-92));}
 }
 assert(ticks<100000);int failed=mode==1 || mode==9;
 int expected=failed ? -71 : mode==3 || mode==8 ? 6 : mode==5 ? 2 : 0;
 assert(rc==expected && report.reason==0);pangu_u64 launched=failed || mode==3 || mode==8 ? pos+1 : mode==5 ? pos : count;
 assert(setup_count==(int)launched);if(mode==3 || mode==8) assert(owners[start_selected].state==4);
 if(mode==9) assert(live.cleanup_error==-92);
 if(!expected) {assert(live.phase==2 && live.started==count);assert(!stop());}
 else assert(!live.started && live.phase==4);
 assert(cleanup_count==(int)launched);
 for(pangu_u64 i=0;i<n;i++) if(out->states[i]) assert(owners[i].state==1);
 return ticks+1;
}
static void cancel_before_setup(const pangu_prepare_result *p,const pangu_graph_output *out,pangu_u32 pos,int in_validation) {
 pangu_lifecycle_edge es[1000];pangu_u32 disposed[9]={0};pangu_u64 ec,count;start_prepare(p,out);start_mode=0;start_selected=-1;
ec=prepared_edge_count;count=p->graph_report.order_count;memcpy(es,lifecycle_edges,ec*sizeof *es);pangu_lifecycle_graph graph={es,ec,disposed,9};live.items=activation;live.count=count;live.stop_graph=start_prefix ? 0 : &graph;assert(!api->bind_geometry(&live,spans,64,100000));
 pangu_lifecycle_report report;assert(api->start(&live,1,&report)==2);int ticks=0;
 if(!in_validation) while(!(live.phase==1 && live.entry_ready && live.started==pos)) {assert(api->pump(&live,1,&report)==2 && report.work<=1);assert(++ticks<100000);}
 int setups=setup_count;assert(setups==(in_validation ? 0 : (int)pos));assert(api->stop(&live,1,&report)==2 && report.work<=1);int rc;
 do {rc=api->pump(&live,1,&report);assert(report.work<=1 && ++ticks<100000);} while(report.pending);
 assert(rc==6 && !live.started && setup_count==setups && cleanup_count==setups);
 if(!in_validation) assert(owners[out->order[pos]].state==1 && !owners[out->order[pos]].inflight);
}
static void reject_before_callbacks(const pangu_prepare_result *p,const pangu_graph_output *out,int edge) {
 pangu_lifecycle_edge es[1000];pangu_u32 disposed[9]={0};pangu_u64 ec,count;start_prepare(p,out);
 ec=prepared_edge_count;count=p->graph_report.order_count;memcpy(es,lifecycle_edges,ec*sizeof *es);pangu_lifecycle_graph graph={es,ec,disposed,9};live.items=activation;live.count=count;live.stop_graph=&graph;
 if(edge) {assert(ec);es[0].provider=es[0].consumer;}else {assert(count);owners[out->order[count-1]].state=2;}
 assert(!api->bind_geometry(&live,spans,64,100000));pangu_lifecycle_report report;int rc=api->start(&live,1,&report),ticks=0;
 while(report.pending) {assert(rc==2 && report.work<=1 && !setup_count);rc=api->pump(&live,1,&report);assert(++ticks<100000);}
 assert(rc==(edge ? 16 : 1) && report.state==6 && !report.pending && !setup_count && !cleanup_count && !live.started);
 if(!edge)owners[out->order[count-1]].state=PANGU_OWNER_LOADING;
}
static int start_cases,start_cancellations,start_invalid;static long start_ticks;
static void start_public_core(void) {pangu_prepare_result p;int cases=0,cancellations=0,invalid=0;long ticks=0;
 for(pangu_u64 i=0;i<sizeof(host_graph_cases)/sizeof(host_graph_cases[0]);i++) {
  const host_graph_case *c=&host_graph_cases[i];assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,10000000,&p)==c->status);if(c->status) continue;
  cancel_before_setup(&p,&corpus_output,0,1);cancellations++;
  if(p.graph_report.order_count) {reject_before_callbacks(&p,&corpus_output,0);invalid++;}if(p.graph_report.binding_count) {reject_before_callbacks(&p,&corpus_output,1);invalid++;}
  for(pangu_u64 q=1;q<=4;q++) {ticks+=start_case(&p,&corpus_output,0,0,q);cases++;for(pangu_u32 pos=0;pos<p.graph_report.order_count;pos++) {int modes[]={1,2,3,4,5,8,9};for(int m=0;m<7;m++) {ticks+=start_case(&p,&corpus_output,pos,modes[m],q);cases++;}}}
  for(pangu_u32 pos=0;pos<p.graph_report.order_count;pos++) {cancel_before_setup(&p,&corpus_output,pos,0);cancellations++;}
 }
 assert(!prepare(1000000,&p));for(pangu_u64 q=1;q<=4;q++) for(pangu_u32 pos=0;pos<p.graph_report.order_count;pos++) for(int mode=1;mode<=5;mode++) {ticks+=start_case(&p,&output,pos,mode,q);cases++;}
 for(int rev=0;rev<2;rev++) {multi_manifest.provisions=rev ? reverse_provisions : provisions;assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,1000000,&p));for(pangu_u64 q=1;q<=4;q++) for(pangu_u32 pos=0;pos<p.graph_report.order_count;pos++) for(int mode=1;mode<=5;mode++) {ticks+=start_case(&p,&output,pos,mode,q);cases++;}}
 start_prefix=1;assert(!prepare(1000000,&p));for(pangu_u64 q=1;q<=4;q++) {ticks+=start_case(&p,&output,0,0,q);cases++;for(pangu_u32 pos=0;pos<p.graph_report.order_count;pos++) {int modes[]={1,2,3,4,5,8,9};for(int m=0;m<7;m++) {ticks+=start_case(&p,&output,pos,modes[m],q);cases++;}}}
 assert(ticks>=0&&ticks<=2147483647);start_cases=cases;start_ticks=ticks;start_cancellations=cancellations;start_invalid=invalid;assert(cases==6056&&cancellations==285&&invalid==114);
}

static pangu_i32 transaction_setup(pangu_owner *owner,void *opaque) {
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;assert(owner==&owners[id]&&owner->inflight==1);setup_count++;
 assert(!api->register_resource(&live,positions[id],cleanup,opaque));return 0;
}
static int alias_rejections,quota_rejections,transactions,refused_plans;
typedef struct {pangu_lifecycle_span spans[80];pangu_u64 count,used;unsigned char data[32768];} input_snapshot;
static void append_snapshot(input_snapshot *s,const void *p,pangu_u64 bytes) {
 if(!bytes) return;assert(s->count<80 && bytes<=sizeof(s->data)-s->used);s->spans[s->count++]=(pangu_lifecycle_span){(pangu_u64)p,(pangu_u64)p+bytes};memcpy(s->data+s->used,p,bytes);s->used+=bytes;
}
static void snapshot_inputs(input_snapshot *s,const pangu_prepare_result *p,const pangu_graph_output *g,const pangu_lifecycle_storage *storage,pangu_u64 n) {
 s->count=s->used=0;
 #define ADD(ptr,count,type) append_snapshot(s,ptr,(count)*sizeof(type))
 ADD(p,1,pangu_prepare_result);ADD(g,1,pangu_graph_output);ADD(storage,1,pangu_lifecycle_storage);ADD(source_items,n,pangu_lifecycle_item);
 const pangu_plan_registry *v=&p->lowered.registry;const pangu_graph_input *in=&p->lowered.graph;
 ADD(v->scopes,v->scope_count,pangu_plan_scope);ADD(v->services,v->service_count,pangu_plan_service);ADD(v->bindings,v->binding_count,pangu_plan_binding);ADD(v->providers,v->provider_count,pangu_plan_provider);
 ADD(in->instances,n,pangu_plan_instance);ADD(in->provider_owners,v->provider_count,pangu_u32);ADD(in->requirements,in->requirement_count,pangu_plan_requirement);
 ADD(g->order,g->order_capacity,pangu_u32);ADD(g->states,g->state_capacity,pangu_u32);ADD(g->bindings,g->binding_capacity,pangu_plan_edge);
 for(pangu_u64 i=0;i<n;i++) if(!g->states[i]) {pangu_owner *o=source_items[i].owner;ADD(o,1,pangu_owner);ADD(o->resources,o->capacity,pangu_owned_resource);ADD(o->operations,o->operation_capacity,pangu_owned_operation);}
 #undef ADD
}
static int same_inputs(const input_snapshot *s) {
 pangu_u64 offset=0;for(pangu_u64 i=0;i<s->count;i++) {pangu_u64 n=s->spans[i].end-s->spans[i].begin;if(memcmp(s->data+offset,(const void *)s->spans[i].begin,n)) return 0;offset+=n;}return 1;
}
static int poisoned(const void *p,pangu_u64 n) {const unsigned char *b=p;for(pangu_u64 i=0;i<n;i++) if(b[i]!=0xa5) return 0;return 1;}
static void reject_alias(const pangu_prepare_result *p,const pangu_graph_output *g,pangu_lifecycle_storage *s,pangu_lifecycle_prepared *bad_info,pangu_lifecycle_edge *edges,pangu_lifecycle_prepared *info,pangu_u64 n) {
 memset(activation,0xa5,sizeof activation);memset(edges,0xa5,1000*sizeof(*edges));memset(info,0xa5,sizeof(*info));input_snapshot before;snapshot_inputs(&before,p,g,s,n);
 assert(api->prepare(p,g,source_items,n,s,100000,bad_info)==1);assert(same_inputs(&before));assert(poisoned(activation,sizeof activation) && poisoned(edges,1000*sizeof(*edges)) && poisoned(info,sizeof(*info)) && !setup_count);alias_rejections++;
}
static void transaction_case(const pangu_prepare_result *p,const pangu_graph_output *g) {
 pangu_u64 n=p->lowered.graph.instance_count;graph_public_init_owners(p,g);memset(&live,0,sizeof live);setup_count=cleanup_count=cleanup_begin_count=0;
 for(pangu_u64 i=0;i<n;i++)source_items[i].setup=transaction_setup;pangu_lifecycle_edge edges[1000];pangu_lifecycle_prepared info;pangu_lifecycle_storage storage={activation,9,edges,1000};input_snapshot before;snapshot_inputs(&before,p,g,&storage,n);
 for(pangu_u64 i=0;i<before.count;i++) {
  void *target=(void *)before.spans[i].begin;
  storage.items=target;reject_alias(p,g,&storage,&info,edges,&info,n);storage.items=activation;
  storage.edges=target;reject_alias(p,g,&storage,&info,edges,&info,n);storage.edges=edges;
  reject_alias(p,g,&storage,target,edges,&info,n);
 }
 storage.edges=(pangu_lifecycle_edge *)(void *)activation;reject_alias(p,g,&storage,&info,edges,&info,n);storage.edges=edges;
 reject_alias(p,g,&storage,(pangu_lifecycle_prepared *)(void *)activation,edges,&info,n);reject_alias(p,g,&storage,(pangu_lifecycle_prepared *)(void *)edges,edges,&info,n);
 storage.item_capacity=~(pangu_u64)0/sizeof(pangu_lifecycle_item)+1;reject_alias(p,g,&storage,&info,edges,&info,n);storage.item_capacity=9;
 storage.edge_capacity=~(pangu_u64)0/sizeof(pangu_lifecycle_edge)+1;reject_alias(p,g,&storage,&info,edges,&info,n);storage.edge_capacity=1000;
 unsigned char *unaligned=(unsigned char *)&info+1;reject_alias(p,g,&storage,(pangu_lifecycle_prepared *)(void *)unaligned,edges,&info,n);
 /* Exhaust every insufficient shared budget, including after stage1 dry-run. */
 snapshot_inputs(&before,p,g,&storage,n);pangu_u64 fuel=1;
 for(;fuel<100000;fuel++) {
  memset(activation,0xa5,sizeof activation);memset(edges,0xa5,sizeof edges);memset(&info,0xa5,sizeof info);int rc=api->prepare(p,g,source_items,n,&storage,fuel,&info);
  if(!rc) break;assert(rc==2 && same_inputs(&before) && poisoned(activation,sizeof activation) && poisoned(edges,sizeof edges) && poisoned(&info,sizeof info) && !setup_count);quota_rejections++;
 }
 assert(fuel<100000 && info.work==fuel && same_inputs(&before) && info.item_count==p->graph_report.order_count && info.edge_count==p->graph_report.binding_count);
 for(pangu_u64 i=0;i<info.item_count;i++) assert(activation[i].owner==&owners[g->order[i]]);
 for(pangu_u64 i=0;i<info.edge_count;i++) {pangu_plan_edge e=g->bindings[i];assert(g->order[edges[i].consumer]==p->lowered.graph.requirements[e.requirement].consumer && g->order[edges[i].provider]==p->lowered.graph.provider_owners[e.provider]);}
 assert(poisoned(activation+info.item_count,(9-info.item_count)*sizeof(pangu_lifecycle_item)) && poisoned(edges+info.edge_count,(1000-info.edge_count)*sizeof(pangu_lifecycle_edge)));
 pangu_u32 disposed[9]={0};pangu_lifecycle_graph stop={edges,info.edge_count,disposed,9};live.items=activation;live.count=info.item_count;live.stop_graph=&stop;for(pangu_u64 i=0;i<info.item_count;i++)positions[g->order[i]]=i;assert(!api->bind_geometry(&live,spans,64,FUEL));pangu_lifecycle_report report;assert(!api->start(&live,FUEL,&report));assert(!api->stop(&live,FUEL,&report));assert(setup_count==(int)info.item_count && cleanup_count==(int)info.item_count);for(pangu_u64 i=0;i<n;i++) if(g->states[i]) assert(owners[i].state==1 && !owners[i].count);transactions++;
}
static void transaction_public_core(void) {pangu_prepare_result p;
 for(pangu_u64 i=0;i<sizeof(host_graph_cases)/sizeof(host_graph_cases[0]);i++) {const host_graph_case *c=&host_graph_cases[i];assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,10000000,&p)==c->status);if(c->status) {
 pangu_lifecycle_edge edges[1000];pangu_lifecycle_prepared info;pangu_lifecycle_storage s={activation,9,edges,1000};pangu_prepare_result copy=p;memset(activation,0xa5,sizeof activation);memset(edges,0xa5,sizeof edges);memset(&info,0xa5,sizeof info);
 assert(api->prepare(&p,&corpus_output,source_items,c->manifest->instance_count,&s,100000,&info)==10 && !memcmp(&p,&copy,sizeof p) && poisoned(activation,sizeof activation) && poisoned(edges,sizeof edges) && poisoned(&info,sizeof info));refused_plans++;continue;}
 transaction_case(&p,&corpus_output);}
 assert(!prepare(1000000,&p));transaction_case(&p,&output);
 for(int rev=0;rev<2;rev++) {multi_manifest.provisions=rev ? reverse_provisions : provisions;assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,1000000,&p));transaction_case(&p,&output);}
 assert(transactions==98&&refused_plans==145&&alias_rejections==6432&&quota_rejections==12127);
}

/* Earliest standalone-owner observable behavior through current public host ABI. */
static pangu_lifecycle_runtime early_runtime;
static pangu_owner early_owner;
static pangu_owned_resource early_resources[4];
static pangu_owned_operation early_operations[2];
static pangu_lifecycle_item early_item;
static pangu_lifecycle_span early_spans[32];
static pangu_lifecycle_setup_ticket early_ticket;
static int early_mode,early_events[4],early_event_count,early_groups;
static pangu_u64 early_epoch;
static pangu_i32 early_release(void *context) {
 int id=(int)(pangu_u64)context;assert(early_event_count<4);early_events[early_event_count++]=id;
 if(early_mode==0&&id==3){pangu_lifecycle_report report;assert(api->stop(&early_runtime,FUEL,&report)==5);assert(api->register_resource(&early_runtime,0,early_release,context)==10);}
 return early_mode==0&&id==2?-80:early_mode==1?-9:0;
}
static pangu_i32 early_setup(pangu_owner *owner,void *context) {
 (void)context;assert(owner==&early_owner&&owner->inflight==1);
 int count=early_mode==0?4:1;
 for(int i=0;i<count;i++)assert(!api->register_resource(&early_runtime,0,early_release,(void *)(pangu_u64)i));
 if(early_mode==0)assert(api->register_resource(&early_runtime,0,early_release,0)==2);
 if(early_mode==2){assert(!api->setup_defer(&early_runtime,&early_ticket));return 5;}
 return early_mode==1?-7:0;
}
static void early_prepare(int mode) {
 early_mode=mode;early_event_count=0;early_epoch++;
 assert(!api->owner_init(&early_owner,early_resources,4,early_operations,2,early_epoch,FUEL));
 memset(&early_runtime,0,sizeof early_runtime);early_item=(pangu_lifecycle_item){&early_owner,early_setup,0};early_runtime.items=&early_item;early_runtime.count=1;
 assert(!api->bind_geometry(&early_runtime,early_spans,32,FUEL));
}
static void early_owner_public_core(void) {
 pangu_lifecycle_report report;
 early_prepare(0);assert(!api->start(&early_runtime,FUEL,&report));assert(api->stop(&early_runtime,FUEL,&report)==-80);
 assert(early_event_count==4&&early_owner.state==PANGU_OWNER_DISPOSED&&!early_owner.count&&!early_owner.inflight);
 for(int i=0;i<4;i++)assert(early_events[i]==3-i);
 assert(api->stop(&early_runtime,FUEL,&report)==-80&&early_event_count==4);early_groups++;
 early_prepare(1);assert(api->start(&early_runtime,FUEL,&report)==-7);
 assert(early_owner.load_error==-7&&early_owner.cleanup_error==-9&&early_runtime.load_error==-7&&early_runtime.cleanup_error==-9&&early_event_count==1);
 assert(api->stop(&early_runtime,FUEL,&report)==-9&&early_event_count==1);early_groups++;
 early_prepare(2);assert(api->start(&early_runtime,FUEL,&report)==5);pangu_lifecycle_setup_ticket old=early_ticket;
 assert(api->stop(&early_runtime,FUEL,&report)==5&&early_owner.state==PANGU_OWNER_QUIESCING&&!early_event_count);
 assert(api->register_resource(&early_runtime,0,early_release,0)==10);
 assert(!api->setup_finish(&early_runtime,&early_ticket,0));assert(api->setup_finish(&early_runtime,&old,0)==10);
 assert(!early_event_count&&early_owner.state!=PANGU_OWNER_ACTIVE);
 assert(api->pump(&early_runtime,FUEL,&report)==6&&early_event_count==1&&early_owner.state==PANGU_OWNER_DISPOSED);early_groups++;
 pangu_owner before=early_owner;assert(api->owner_init(&early_owner,early_resources,4,early_operations,2,early_epoch,FUEL)==10&&!memcmp(&before,&early_owner,sizeof before));
 early_prepare(2);assert(api->start(&early_runtime,FUEL,&report)==5);assert(api->setup_finish(&early_runtime,&old,0)==10&&early_owner.inflight==1);
 assert(!api->setup_finish(&early_runtime,&early_ticket,0));assert(!api->pump(&early_runtime,FUEL,&report));assert(!api->stop(&early_runtime,FUEL,&report));
 before=early_owner;assert(api->owner_init(&early_owner,(pangu_owned_resource *)(void *)&early_owner,1,early_operations,2,early_epoch+1,FUEL)==1&&!memcmp(&before,&early_owner,sizeof before));
 pangu_owner invalid={0};invalid.struct_size=1;pangu_owner invalid_before=invalid;
 assert(api->owner_init(&invalid,early_resources,4,early_operations,2,1,FUEL)==1&&!memcmp(&invalid,&invalid_before,sizeof invalid));early_groups++;
 assert(early_groups==4);
}

int main(void) {
 api=pangu_lifecycle_query(1,152,1,0);assert(api);adapter_public_core();pangu_prepare_result plan;int valid=0,rejected=0;
 for(pangu_u32 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {
  const host_graph_case *c=&host_graph_cases[i];
  assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,10000000,&plan)==c->status);
  assert(!callback_calls);
  if(c->status) {rejected++;continue;}
  valid++;
  for(pangu_u32 position=0;position<plan.graph_report.order_count;position++) {
   for(int variant=0;variant<7;variant++) run_deferred(&plan,&corpus_output,position,variant);
  }
 }
 assert(!prepare(FUEL,&plan));
 for(pangu_u32 position=0;position<plan.graph_report.order_count;position++) {
  for(int variant=0;variant<7;variant++) run_deferred(&plan,&output,position,variant);
 }
 for(int reverse=0;reverse<2;reverse++) {
  multi_manifest.provisions=reverse?reverse_provisions:provisions;
  assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,FUEL,&plan));
  for(pangu_u32 position=0;position<plan.graph_report.order_count;position++) {
   for(int variant=0;variant<7;variant++) run_deferred(&plan,&output,position,variant);
  }
 }
 assert(valid==95 && rejected==145 && scenarios==1414);
 for(int i=0;i<7;i++) assert(by_mode[i]==202);
 assert(invalid_ticket_refusals==3636 && inline_finishes==202 && async_cleanup_finishes==202);
 async_disposer_public_core(&plan,&output);budget_public_core();call_budget_public_core();call_lifecycle_public_core();geometry_public_core();graph_public_core();start_public_core();transaction_public_core();early_owner_public_core();
 printf("async_setup_core_scenarios=%d valid_plans=%d rejected_plans=%d invalid_ticket_refusals=%d inline_finishes=%d async_cleanup_finishes=%d async_disposer_public_groups=%d rejected_cleanup_tickets=%d inline_cleanup_finishes=%d budget_public_cases=%d bounded_ticks=%d zero_budget_owner_work=%d public_call_skips=%d prefix_cases=%d call_budget_cases=%d admitted_calls=%d insufficient_budget_retries=%d quota_refusals=%d invalid_bindings=%d overlap_refusals=%d setup_contexts=%d cleanup_contexts=%d call_lifecycle_cases=%d direct_calls=%d setup_calls=%d cleanup_calls=%d provider_invocations=%d rejected_calls=%d async_cleanup_begins=%d geometry_cases=%d geometry_matrices=%d geometry_refusals=%d graph_cases=%d independent_cases=%d ancestor_cases=%d independent_setup_cases=%d error_while_waiting=%d edge_prepare_cases=%d call_hold_cases=%d private_owner_hold_skips=%d start_budget_cases=%d start_ticks=%d pre_setup_cancellations=%d invalid_before_callbacks=%d transactions=%d refused_plans=%d transaction_alias_rejections=%d transaction_quota_rejections=%d earlier_owner_groups=%d\n",scenarios,valid,rejected,invalid_ticket_refusals,inline_finishes,async_cleanup_finishes,disposer_groups,disposer_ticket_refusals,disposer_inline_finishes,budget_cases,budget_total_ticks,budget_single_work,budget_call_skips,budget_prefix_cases,call_budget_scenarios,call_budget_admissions,call_budget_retries,call_budget_quota_refusals,call_budget_invalid_bindings,call_budget_overlap_refusals,call_budget_setup_contexts,call_budget_cleanup_contexts,call_lifecycle_cases,call_lifecycle_direct_calls,call_lifecycle_setup_calls,call_lifecycle_cleanup_calls,call_lifecycle_provider_calls,call_lifecycle_rejected_calls,call_lifecycle_async_begin_total,geometry_cases,geometry_matrices,geometry_refused,graph_public_scenarios,graph_public_independent,graph_public_ancestors,graph_public_independent_setup,graph_public_error_waiting,graph_public_edge_cases,graph_public_call_holds,graph_public_owner_hold_skips,start_cases,(int)start_ticks,start_cancellations,start_invalid,transactions,refused_plans,alias_rejections,quota_rejections,early_groups);
 return 0;
}
