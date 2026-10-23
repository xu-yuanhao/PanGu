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
 pangu_u32 id=(pangu_u32)(pangu_u64)opaque;cleanup_count++;
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
 assert(prepared.item_count==plan->graph_report.order_count);
 for(pangu_u32 i=0;i<prepared.item_count;i++) positions[out->order[i]]=i;
 live.items=activation;live.count=prepared.item_count;
 /* Match the historical prefix teardown semantics; branch scheduling has its own corpus. */
 assert(!api->bind_geometry(&live,spans,64,FUEL));current=plan;current_output=out;
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
int main(void) {
 api=pangu_lifecycle_query(1,152,1,0);assert(api);pangu_prepare_result plan;int valid=0,rejected=0;
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
 printf("async_setup_core_scenarios=%d valid_plans=%d rejected_plans=%d invalid_ticket_refusals=%d inline_finishes=%d async_cleanup_finishes=%d\n",scenarios,valid,rejected,invalid_ticket_refusals,inline_finishes,async_cleanup_finishes);
 return 0;
}
