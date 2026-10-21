#include "src/t14-abi/client-abi-v1.h"
#include "src/focused-test-support/assert.h"
#include "src/focused-test-support/stdio.h"
#include "src/focused-test-support/string.h"
#include "plan-fixture.h"
extern const pangu_lifecycle_api *facade_asm_query(void);
static const pangu_lifecycle_api *api;
static pangu_lifecycle_runtime life_runtime;static pangu_owner owner_frames[9];static pangu_owned_resource resource_pool[9][2];static pangu_owned_operation operation_pool[9][3];
static pangu_lifecycle_item source_items[9],items[9];static pangu_lifecycle_edge life_edges[1000];static pangu_lifecycle_span ranges[64];static pangu_u32 disposed[9];
typedef struct {pangu_u32 id,position;} plugin_context;static plugin_context contexts[9];
static const pangu_prepare_result *plan;static const pangu_graph_output *plan_output;static pangu_lifecycle_setup_ticket setup_ticket;static pangu_cleanup_ticket cleanup_ticket;static pangu_u32 cleanup_position;
static int mode,selected,setups,cleanups,provider_calls,total_cases,registration_refusals;
static pangu_u32 consumer(pangu_u32 b) {return plan->lowered.graph.requirements[plan_output->bindings[b].requirement].consumer;}
static pangu_u32 provider(pangu_u32 b) {return plan->lowered.graph.provider_owners[plan_output->bindings[b].provider];}
static pangu_i32 release_resource(void *p);
static pangu_i32 async_release(void *p,const pangu_cleanup_ticket *ticket) {plugin_context *c=p;cleanup_ticket=*ticket;cleanup_position=c->position;return 5;}
static void use_provider(pangu_u32 b,int cleanup,const void *scope) {
 pangu_lifecycle_call_ticket ticket;pangu_lifecycle_report report;pangu_i32 rc=cleanup ? api->cleanup_call(&life_runtime,b,scope,100,&ticket,&report) : api->setup_call(&life_runtime,b,scope,100,&ticket,&report);
 assert(!rc && report.work<=100 && !report.pending);pangu_u32 id=provider(b);assert(owner_frames[id].state==PANGU_OWNER_ACTIVE);provider_calls++;assert(!api->call_finish(&life_runtime,&ticket));assert(api->call_finish(&life_runtime,&ticket)==10);
}
static pangu_i32 release_resource(void *p) {
 plugin_context *c=p;cleanups++;pangu_cleanup_ticket scope;assert(!api->cleanup_scope(&life_runtime,c->position,&scope));
 for(pangu_u32 b=0;b<plan->graph_report.binding_count;b++) if(consumer(b)==c->id) {
  use_provider(b,1,&scope);pangu_u32 pos=contexts[provider(b)].position;assert(owner_frames[provider(b)].state==2);
  assert(api->register_resource(&life_runtime,pos,release_resource,p)==10);assert(api->register_async(&life_runtime,pos,async_release,p)==10);registration_refusals+=2;
 }
 assert(api->register_resource(&life_runtime,c->position,release_resource,p)==10);registration_refusals++;return 0;
}
static pangu_i32 plugin_setup(pangu_owner *owner,void *p) {
 plugin_context *c=p;c->position=(pangu_u32)life_runtime.launched-1;assert(owner==&owner_frames[c->id]);setups++;
 assert(!api->register_resource(&life_runtime,c->position,release_resource,p));pangu_lifecycle_setup_ticket scope;assert(!api->setup_scope(&life_runtime,&scope));
 for(pangu_u32 b=0;b<plan->graph_report.binding_count;b++) if(consumer(b)==c->id) use_provider(b,0,&scope);
 if((int)c->id==selected && mode==1) {assert(!api->setup_defer(&life_runtime,&setup_ticket));return 5;}
 if((int)c->id==selected && mode==2) {assert(!api->register_async(&life_runtime,c->position,async_release,p));return -41;}return 0;
}
static void finish_pending(pangu_lifecycle_report *report,pangu_u64 q) {
 assert(report->reason==2 && report->pending);
 if(life_runtime.setup_waiting) {
  pangu_lifecycle_report stop;int rc=api->stop(&life_runtime,q,&stop);assert((rc==2 || rc==5) && stop.work<=q);
  assert(api->register_resource(&life_runtime,contexts[selected].position,release_resource,&contexts[selected])==10);registration_refusals++;
  assert(!api->setup_finish(&life_runtime,&setup_ticket,0));assert(api->setup_finish(&life_runtime,&setup_ticket,0)==10);
 } else {assert(!api->disposer_finish(&life_runtime,cleanup_position,&cleanup_ticket,mode==2 ? -52 : 0));assert(api->disposer_finish(&life_runtime,cleanup_position,&cleanup_ticket,0)==10);}
}
static void run_case(const pangu_prepare_result *p,const pangu_graph_output *out,int position,int kind,pangu_u64 q) {
 plan=p;plan_output=out;mode=kind;selected=position<0 ? -1 : (int)out->order[position];setups=cleanups=0;memset(&life_runtime,0,sizeof life_runtime);memset(disposed,0,sizeof disposed);
 pangu_u64 n=p->lowered.graph.instance_count;
 for(pangu_u64 i=0;i<n;i++) {memset(&owner_frames[i],0,sizeof(pangu_owner));assert(!api->owner_init(&owner_frames[i],resource_pool[i],2,operation_pool[i],3,1,4));contexts[i]=(plugin_context){(pangu_u32)i,0};source_items[i]=(pangu_lifecycle_item){&owner_frames[i],plugin_setup,&contexts[i]};}
 pangu_lifecycle_storage storage={items,9,life_edges,1000};pangu_lifecycle_prepared prepared;assert(!api->prepare(p,out,source_items,n,&storage,100000,&prepared));pangu_lifecycle_graph graph={life_edges,prepared.edge_count,disposed,9};life_runtime.items=items;life_runtime.count=prepared.item_count;life_runtime.stop_graph=&graph;
 assert(api->register_resource(&life_runtime,0,release_resource,0)==10);registration_refusals++;assert(!api->bind_geometry(&life_runtime,ranges,64,100000));
 pangu_lifecycle_report report;int rc=api->start(&life_runtime,q,&report),ticks=0;
 for(;;) {assert(report.work<=q);if(!report.pending) break;if(report.reason==2) finish_pending(&report,q);else assert(report.reason==1 && rc==2);rc=api->pump(&life_runtime,q,&report);assert(++ticks<100000);}
 if(kind) {assert(rc==(kind==1 ? 6 : -41) && life_runtime.phase==4 && !life_runtime.started);assert(setups==position+1 && cleanups==position+1);if(kind==2) assert(life_runtime.cleanup_error==-52);}
 else {
  assert(!rc && life_runtime.phase==2 && setups==(int)prepared.item_count);
  if(prepared.item_count) {
   plugin_context *c=&contexts[out->order[0]];
   assert(!(q<=2 ? api->register_resource(&life_runtime,0,release_resource,c) : api->register_async(&life_runtime,0,async_release,c)));
   for(pangu_u32 b=0;b<prepared.edge_count;b++) {pangu_lifecycle_call_ticket ticket;assert(!api->call(&life_runtime,b,100,&ticket,&report));provider_calls++;assert(!api->call_finish(&life_runtime,&ticket));}
  }
  do {rc=api->stop(&life_runtime,q,&report);assert(report.work<=q && ++ticks<100000);if(report.pending && report.reason==2) finish_pending(&report,q);} while(report.pending);
  assert(!rc && !life_runtime.started && cleanups==(int)prepared.item_count+(prepared.item_count && q<=2));
 }
 for(pangu_u64 i=0;i<n;i++) if(out->states[i]) assert(owner_frames[i].state==1 && !owner_frames[i].count);total_cases++;
}
int main(void) {
 api=pangu_lifecycle_query(1,152,1,0);assert(api && api==facade_asm_query() && api->abi_version==1 && api->struct_size==152 && api->target==1 && !api->reserved);
 pangu_u32 bad[]={0,2,0xffffffffU};for(int i=0;i<3;i++) assert(!pangu_lifecycle_query(bad[i],152,1,0));assert(!pangu_lifecycle_query(1,151,1,0) && !pangu_lifecycle_query(1,153,1,0) && !pangu_lifecycle_query(1,152,0,0) && !pangu_lifecycle_query(1,152,2,0) && !pangu_lifecycle_query(1,152,1,1));
 pangu_owner o={0},copy=o;pangu_owned_resource resources[2];pangu_owned_operation operations[3],before[3];memset(operations,0xa5,sizeof operations);memcpy(before,operations,sizeof before);assert(api->owner_init(&o,resources,2,operations,3,1,3)==2 && !memcmp(&o,&copy,sizeof o) && !memcmp(operations,before,sizeof before));
 pangu_prepare_result p;
 for(pangu_u64 i=0;i<sizeof(host_graph_cases)/sizeof(host_graph_cases[0]);i++) {const host_graph_case *c=&host_graph_cases[i];assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,10000000,&p)==c->status);if(c->status) continue;
  for(pangu_u64 q=1;q<=4;q++) run_case(&p,&corpus_output,-1,0,q);
  for(pangu_u32 pos=0;pos<p.graph_report.order_count;pos++) for(int kind=1;kind<=2;kind++) for(pangu_u64 q=1;q<=4;q+=3) run_case(&p,&corpus_output,(int)pos,kind,q);
 }
 assert(!prepare(1000000,&p));for(pangu_u64 q=1;q<=4;q++) run_case(&p,&output,-1,0,q);
 for(int rev=0;rev<2;rev++) {multi_manifest.provisions=rev ? reverse_provisions : provisions;assert(!pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,1000000,&p));for(pangu_u64 q=1;q<=4;q++) run_case(&p,&output,-1,0,q);}
 printf("facade_cases=%d provider_calls=%d registration_refusals=%d\n",total_cases,provider_calls,registration_refusals);return 0;
}
