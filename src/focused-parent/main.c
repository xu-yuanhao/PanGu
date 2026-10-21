#include "src/t14-abi/client-invocation-v1.h"
#define PANGU_ROUTE_TEST_MAIN parent_route_regression_main
#include "route-fixture.h"
#undef PANGU_ROUTE_TEST_MAIN
static const pangu_invocation_api *calls;static const pangu_lifecycle_child_api *child_api;
static pangu_invocation_bank cap_bank;static pangu_invocation_slot cap_slots[32];
static pangu_plugin_lease waiting_roots[9],stale_cap;static int root_waiting[9],have_stale;static pangu_u32 stale_consumer;
static pangu_cleanup_ticket cleanup_tokens[9];
static int parent_cases,parent_actual_calls,parent_setup_calls,parent_cleanup_calls,parent_business_calls,invalid_parents,parent_aliases,root_budgets,child_budgets,parent_busy,async_roots,cancellation_denials,completed_scope_denials,quota_denials,retired_slots;
static int run_profile,chosen,root_budget_done,child_budget_done,quota_probe_done,sidecar_quota_done,metadata_checked,max_depth;static pangu_u32 node_capacity;static pangu_u64 bank_epoch;
static unsigned char saved[32768];static pangu_u64 saved_size;
static pangu_u64 capture(unsigned char *out) {
 pangu_u64 n=0;
 #define SAVE(p,bytes) do {pangu_u64 size=(bytes);assert(n+size<=sizeof saved);if(out) memcpy(out+n,p,size);n+=size;} while(0)
 SAVE(&run,sizeof run);SAVE(owners,sizeof owners);SAVE(operations,sizeof operations);SAVE(resources,sizeof resources);SAVE(records,sizeof records);SAVE(items,sizeof items);SAVE(disposed,sizeof disposed);SAVE(stop_edges,sizeof stop_edges);SAVE(protected_regions,sizeof protected_regions);SAVE(&cap_bank,sizeof cap_bank);SAVE(cap_slots,sizeof cap_slots);SAVE(service_slots,router.slot_count*sizeof service_slots[0]);SAVE(tables,router.slot_count*sizeof tables[0]);SAVE(service_states,router.slot_count*sizeof service_states[0]);
 #undef SAVE
 return n;
}
static void begin_snapshot(void) {saved_size=capture(saved);}
static void parent_unchanged(void) {unsigned char now[32768];assert(capture(now)==saved_size && !memcmp(now,saved,saved_size));}
static void public_query(pangu_u32 b,pangu_u32 mode,const void *scope,pangu_service_query *q) {query_for(b,mode,scope,q);}
static pangu_u32 callee(pangu_u32 b) {return current_plan->lowered.graph.provider_owners[current_graph->bindings[b].provider];}
static void metadata_alias_tests(const pangu_service_query *q) {
 pangu_u32 selected=current_graph->bindings[q->binding].provider;
 struct {void *p;pangu_u64 n;} targets[]={{&cap_bank,sizeof cap_bank},{cap_slots,sizeof cap_slots},{(void *)calls,sizeof(*calls)},{(void *)child_api,sizeof(*child_api)},{(void *)life,sizeof(*life)},{(void *)route,sizeof(*route)},{&router,sizeof router},{&run,sizeof run},{records,sizeof records},{owners,sizeof owners},{operations,sizeof operations},{resources,sizeof resources},{protected_regions,sizeof protected_regions},{&tables[selected],sizeof tables[selected]},{(void *)q,sizeof(*q)}};
 for(pangu_u32 i=0;i<sizeof targets/sizeof targets[0];i++) {unsigned char before[80];pangu_u64 n=targets[i].n<80 ? targets[i].n : 80;memcpy(before,targets[i].p,n);pangu_lifecycle_report report;memset(&report,0xa5,sizeof report);begin_snapshot();assert(calls->open_root(&cap_bank,q,100000,targets[i].p,&report)==1 && !memcmp(before,targets[i].p,n) && entry_poison(&report,sizeof report));parent_unchanged();parent_aliases++;}
}
static pangu_i32 open_root_test(pangu_u32 b,pangu_u32 mode,const void *scope,pangu_plugin_lease *out) {
 pangu_service_query q;public_query(b,mode,scope,&q);pangu_lifecycle_report report;
 if(!metadata_checked) {metadata_alias_tests(&q);metadata_checked=1;}
 if(!root_budget_done) {begin_snapshot();for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);memset(out,0xa5,sizeof(*out));memset(&report,0xa5,sizeof report);int rc=calls->open_root(&cap_bank,&q,fuel,out,&report);if(!rc) {assert(report.work<=fuel);root_budget_done=1;break;}assert(rc==2 && entry_poison(out,sizeof(*out)));parent_unchanged();if(!entry_poison(&report,sizeof report)) assert(report.work<=fuel && report.pending && report.reason==1);root_budgets++;}return 0;}
 return calls->open_root(&cap_bank,&q,100000,out,&report);
}
static void invalid_parent_tests(const pangu_plugin_lease *parent,pangu_u32 receiver,const pangu_service_query *q) {
 for(int word=0;word<10;word++) {pangu_plugin_lease bad=*parent;if(word<8) bad.opaque[word]^=1;else if(word==8) bad.table=(const void *)((pangu_u64)bad.table^8);else bad.state=(void *)((pangu_u64)bad.state^8);pangu_plugin_lease out;pangu_lifecycle_report report;memset(&out,0xa5,sizeof out);begin_snapshot();assert(calls->open_child(&cap_bank,&bad,receiver,q,100000,&out,&report)==10 && entry_poison(&out,sizeof out));parent_unchanged();invalid_parents++;}
 pangu_invocation_slot *node=&cap_slots[parent->opaque[2]];pangu_plugin_lease out;pangu_lifecycle_report report;memset(&out,0xa5,sizeof out);
 /* Correct handle, but no live kernel operation: manager cache alone is not authority. */
 for(int which=0;which<3;which++) {pangu_lifecycle_call_ticket original=node->lease.ticket;if(which==0) node->lease.ticket.consumer.generation^=1;if(which==1) node->lease.ticket.provider.generation^=1;if(which==2) node->lease.ticket.runtime=(void *)&bank_epoch;begin_snapshot();int rc=calls->open_child(&cap_bank,parent,receiver,q,100000,&out,&report);assert(rc && entry_poison(&out,sizeof out));parent_unchanged();node->lease.ticket=original;invalid_parents++;}
 pangu_service_query bad=*q;bad.mode=2;begin_snapshot();assert(calls->open_child(&cap_bank,parent,receiver,&bad,100000,&out,&report)==1);parent_unchanged();invalid_parents++;
 begin_snapshot();assert(calls->open_child(&cap_bank,parent,0xffffffffU,q,100000,&out,&report)==1);parent_unchanged();invalid_parents++;
 pangu_plugin_lease same=*parent;begin_snapshot();assert(calls->open_child(&cap_bank,&same,receiver,q,100000,&same,&report)==1 && !memcmp(&same,parent,sizeof same));parent_unchanged();parent_aliases++;
}
static void invoke_tree(const pangu_plugin_lease *incoming,pangu_u32 owner,pangu_u32 stage,pangu_u32 depth) {
 assert(depth<=9);if((int)depth>max_depth) max_depth=(int)depth;const test_service *table=incoming->table;service_state *state=incoming->state;pangu_i32 value=0;assert(!table->evaluate(state,17,&value) && value==state->value+17);parent_actual_calls++;
 for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==owner) {
  pangu_service_query q;public_query(b,0,0,&q);invalid_parent_tests(incoming,owner,&q);pangu_plugin_lease child;pangu_lifecycle_report report;
  if(!child_budget_done) {begin_snapshot();for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);memset(&child,0xa5,sizeof child);memset(&report,0xa5,sizeof report);int rc=calls->open_child(&cap_bank,incoming,owner,&q,fuel,&child,&report);if(!rc) {assert(report.work<=fuel);child_budget_done=1;break;}assert(rc==2 && entry_poison(&child,sizeof child));parent_unchanged();if(!entry_poison(&report,sizeof report)) assert(report.pending && report.reason==1 && report.work<=fuel);child_budgets++;}}
  else {memset(&child,0xa5,sizeof child);begin_snapshot();int rc=calls->open_child(&cap_bank,incoming,owner,&q,100000,&child,&report);if(node_capacity==1) {assert(rc==2 && entry_poison(&child,sizeof child));parent_unchanged();quota_denials++;continue;}assert(!rc);}
  if(stage==1) parent_setup_calls++;else if(stage==2) parent_cleanup_calls++;else parent_business_calls++;
  pangu_u32 root_owner=(pangu_u32)(cap_slots[incoming->opaque[2]].lease.ticket.consumer.owner-owners);begin_snapshot();assert(calls->close(&cap_bank,root_owner,incoming,100000)==2);parent_unchanged();parent_busy++;
  /* Same parent can legitimately fan out; it is only stale replay that is forbidden. */
  if(depth==0 && node_capacity>2) {pangu_plugin_lease sibling;int full=owners[owner].inflight==owners[owner].operation_capacity || owners[callee(b)].inflight==owners[callee(b)].operation_capacity;begin_snapshot();int rc=calls->open_child(&cap_bank,incoming,owner,&q,100000,&sibling,&report);if(full) {assert(rc==2);parent_unchanged();quota_denials++;}else {assert(!rc);if(!quota_probe_done) {pangu_plugin_lease full;memset(&full,0xa5,sizeof full);begin_snapshot();assert(calls->open_child(&cap_bank,incoming,owner,&q,100000,&full,&report)==2 && entry_poison(&full,sizeof full) && report.reason==0 && !report.pending);parent_unchanged();quota_probe_done=1;quota_denials++;}assert(!calls->close(&cap_bank,owner,&sibling,100000));assert(calls->close(&cap_bank,owner,&sibling,100000)==10);invalid_parents++;}}

  invoke_tree(&child,callee(b),stage,depth+1);assert(!calls->close(&cap_bank,owner,&child,100000));begin_snapshot();assert(calls->close(&cap_bank,owner,&child,100000)==10);parent_unchanged();invalid_parents++;
  pangu_plugin_lease poison;memset(&poison,0xa5,sizeof poison);begin_snapshot();assert(calls->open_child(&cap_bank,&child,callee(b),&q,100000,&poison,&report)!=0 && entry_poison(&poison,sizeof poison));parent_unchanged();invalid_parents++;
 }
}
static int any_waiting(void) {for(int i=0;i<9;i++) if(root_waiting[i]) return 1;return 0;}
static void roots_for(const pangu_entry_context *c,pangu_u32 mode,const void *scope,int hold_first) {
 for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==c->instance_id) {
  pangu_plugin_lease root;assert(!open_root_test(b,mode,scope,&root));
  if(hold_first && !any_waiting()) {waiting_roots[c->instance_id]=root;root_waiting[c->instance_id]=1;continue;}
  invoke_tree(&root,callee(b),mode==1 ? 1 : mode==2 ? 2 : 3,0);assert(!calls->close(&cap_bank,c->instance_id,&root,100000));stale_cap=root;stale_consumer=c->instance_id;have_stale=1;
 }
}
static pangu_i32 manager_cleanup(void *opaque) {const pangu_entry_context *c=opaque;pangu_cleanup_ticket scope;assert(!life->cleanup_scope(&run,c->position,&scope));roots_for(c,2,&scope,0);cleanups++;return 0;}
static pangu_i32 manager_cleanup_async(void *opaque,const pangu_cleanup_ticket *ticket) {const pangu_entry_context *c=opaque;cleanup_tokens[c->instance_id]=*ticket;roots_for(c,2,ticket,1);cleanups++;return 5;}
static pangu_i32 manager_load(const pangu_entry_context *c) {for(pangu_u32 i=0;i<router.slot_count;i++) if(current_plan->lowered.graph.provider_owners[i]==c->instance_id) assert(!route->publish(&router,c,i,&tables[i].header,&service_states[i],100000));pangu_lifecycle_setup_ticket scope;assert(!life->setup_scope(&run,&scope));roots_for(c,1,&scope,0);return run_profile==4 && (int)c->instance_id==chosen ? -61 : 0;}
static pangu_i32 manager_entry(void *opaque) {
 const pangu_entry_context *c=opaque;entries_called++;if(run_profile==1 || run_profile==2 || run_profile==3) assert(!life->register_async(&run,c->position,manager_cleanup_async,(void *)c));else assert(!life->register_resource(&run,c->position,manager_cleanup,(void *)c));
 if((run_profile==1 || run_profile==2 || run_profile==3) && (int)c->instance_id==chosen) {pangu_lifecycle_setup_ticket scope;assert(!life->setup_scope(&run,&scope));roots_for(c,1,&scope,1);assert(!life->setup_defer(&run,&pending_setup));return 5;}return manager_load(c);
}
static pangu_u32 incoming_binding(pangu_u32 owner) {for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) if(consumer(b)==owner) return b;return 0xffffffffU;}
static void handle_waiting_setup(void) {
 pangu_u32 id=(pangu_u32)chosen;const pangu_entry_context *c=&records[id].context;
 if(run_profile==2) {pangu_lifecycle_report stopped;int rc=life->stop(&run,1,&stopped);assert(rc==2 || rc==5);}
 if(root_waiting[id]) {pangu_u32 b=incoming_binding(id);assert(b!=0xffffffffU);pangu_plugin_lease *root=&waiting_roots[id];pangu_u32 owner=callee(b);
  if(run_profile==2) {for(pangu_u32 edge=0;edge<current_plan->graph_report.binding_count;edge++) if(consumer(edge)==owner) {pangu_service_query q;public_query(edge,0,0,&q);pangu_plugin_lease child;pangu_lifecycle_report report;memset(&child,0xa5,sizeof child);begin_snapshot();assert(calls->open_child(&cap_bank,root,owner,&q,100000,&child,&report)==10 && entry_poison(&child,sizeof child));parent_unchanged();cancellation_denials++;}}
  else invoke_tree(root,owner,1,0);
  if(run_profile==3) {assert(!life->setup_finish(&run,&pending_setup,0));for(pangu_u32 edge=0;edge<current_plan->graph_report.binding_count;edge++) if(consumer(edge)==owner) {pangu_service_query q;public_query(edge,0,0,&q);pangu_plugin_lease child;pangu_lifecycle_report report;begin_snapshot();assert(calls->open_child(&cap_bank,root,owner,&q,100000,&child,&report)==10);parent_unchanged();completed_scope_denials++;}}
  assert(!calls->close(&cap_bank,id,root,100000));root_waiting[id]=0;
 }
 if(run_profile!=2 && run_profile!=3) assert(!manager_load(c));
 if(run_profile==3) { /* Completion already issued above, but entry effects can no longer publish. */
  if(!run.setup_completed) assert(!life->setup_finish(&run,&pending_setup,0));
 }else assert(!life->setup_finish(&run,&pending_setup,0));
}
static void finish_cleanups(void) {
 int found=0;
 for(pangu_u32 i=0;i<router.manifest->instance_count;i++) if(owners[i].cleanup_waiting && !owners[i].cleanup_completed) {
  int finished_early=0;
  if(root_waiting[i]) {pangu_u32 b=incoming_binding(i);assert(b!=0xffffffffU);pangu_plugin_lease *root=&waiting_roots[i];pangu_u32 owner=callee(b);invoke_tree(root,owner,2,0);
   for(pangu_u32 edge=0;edge<current_plan->graph_report.binding_count;edge++) if(consumer(edge)==owner && node_capacity>1) {
    pangu_service_query q;public_query(edge,0,0,&q);pangu_plugin_lease admitted;pangu_lifecycle_report report;assert(!calls->open_child(&cap_bank,root,owner,&q,100000,&admitted,&report));assert(!life->disposer_finish(&run,records[i].context.position,&cleanup_tokens[i],0));finished_early=1;
    pangu_plugin_lease poison;memset(&poison,0xa5,sizeof poison);begin_snapshot();assert(calls->open_child(&cap_bank,root,owner,&q,100000,&poison,&report)==10 && entry_poison(&poison,sizeof poison));parent_unchanged();completed_scope_denials++;assert(calls->close(&cap_bank,i,root,100000)==2);parent_busy++;
    const test_service *table=admitted.table;service_state *state=admitted.state;pangu_i32 value;assert(!table->evaluate(state,17,&value) && value==state->value+17);parent_actual_calls++;assert(!calls->close(&cap_bank,owner,&admitted,100000));break;
   }
   assert(!calls->close(&cap_bank,i,root,100000));root_waiting[i]=0;async_roots++;
  }
  if(!finished_early) assert(!life->disposer_finish(&run,records[i].context.position,&cleanup_tokens[i],0));found=1;break;
 }
 assert(found);
}
static void drive_manager(const pangu_static_manifest *m,const pangu_prepare_result *p,const pangu_graph_output *g,const pangu_u32 *map,int profile,int capacity) {
 run_profile=profile;node_capacity=(pangu_u32)capacity;bank_epoch++;memset(&run,0,sizeof run);memset(disposed,0,sizeof disposed);memset(root_waiting,0,sizeof root_waiting);memset(service_slots,0,sizeof service_slots);memset(service_states,0,sizeof service_states);entries_called=cleanups=0;current_plan=p;current_graph=g;chosen=p->graph_report.order_count ? (int)g->order[p->graph_report.order_count-1] : -1;
 for(pangu_u32 i=0;i<m->instance_count;i++) {if(!owners[i].abi_version || owners[i].state==1) memset(&owners[i],0,sizeof owners[i]);assert(!life->owner_init(&owners[i],resources[i],2,operations[i],3,bank_epoch,4));instances_in[i]=(pangu_entry_instance){&owners[i],0};}
 for(pangu_u32 i=0;i<p->lowered.registry.provider_count;i++) {tables[i]=(test_service){{p->lowered.registry.providers[i].version,24,1,0},eval_a};service_states[i].value=(int)p->lowered.registry.providers[i].instance*11+3;}
 pangu_entry_request req={m,&compiled,map,m->module_count,p,g,life,&run,instances_in,m->instance_count};pangu_entry_storage st={records,9,sources,9,module_scratch,9};pangu_entry_prepared ep;assert(!bridge->prepare(&req,&st,100000,&ep));pangu_lifecycle_storage ls={items,9,stop_edges,1000};pangu_lifecycle_prepared lp;assert(!life->prepare(p,g,sources,m->instance_count,&ls,100000,&lp));pangu_lifecycle_graph graph={stop_edges,lp.edge_count,disposed,9};run.items=items;run.count=lp.item_count;run.stop_graph=&graph;assert(!life->bind_geometry(&run,protected_regions,64,100000));router=(pangu_service_router){life,&run,m,p,g,records,service_slots,p->lowered.registry.provider_count};assert(!calls->bind(&cap_bank,&router,route,child_api,cap_slots,capacity,bank_epoch,100000));begin_snapshot();assert(calls->bind(&cap_bank,&router,route,child_api,cap_slots,capacity,bank_epoch,100000)==10);parent_unchanged();invalid_parents++;
 pangu_lifecycle_child_api wrong=*child_api;wrong.base=(void *)0;begin_snapshot();assert(calls->bind(&cap_bank,&router,route,&wrong,cap_slots,capacity,bank_epoch+1,100000)==1);parent_unchanged();invalid_parents++;
 if(profile==5 && capacity>1) {cap_slots[0].generation=~(pangu_u64)0;retired_slots++;}
 pangu_lifecycle_report report;int rc=life->start(&run,1,&report),ticks=0;
 while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2) {if(run.setup_waiting) handle_waiting_setup();else finish_cleanups();}else assert(report.reason==1);rc=life->pump(&run,1,&report);}
 if((profile==2 || profile==4) && run.count) assert(rc==(profile==2 ? 6 : -61) && run.phase==4);
 else {assert(!rc && run.phase==2);if(have_stale && stale_consumer<m->instance_count && !g->states[stale_consumer]) {begin_snapshot();assert(calls->close(&cap_bank,stale_consumer,&stale_cap,100000)==10);parent_unchanged();invalid_parents++;}
  for(pangu_u32 i=0;i<m->instance_count;i++) if(!g->states[i]) roots_for(&records[i].context,0,0,0);
  rc=life->stop(&run,1,&report);ticks=0;while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2) finish_cleanups();else assert(report.reason==1);rc=life->stop(&run,1,&report);}assert(!rc);
 }
 assert(cleanups==entries_called);for(pangu_u32 i=0;i<node_capacity;i++) assert(!cap_slots[i].active && !cap_slots[i].children);for(pangu_u32 i=0;i<m->instance_count;i++) assert(!root_waiting[i]);parent_cases++;
}
int main(void) {
 life=real_life=pangu_lifecycle_query(1,152,1,0);child_api=(const pangu_lifecycle_child_api *)(const void *)pangu_lifecycle_query(2,32,1,0);route=pangu_service_route_query(1,40,1,0);bridge=pangu_entry_bridge_query(1,24,1,0);calls=pangu_invocation_query(1,48,1,0);assert(child_api && child_api->abi_version==2 && child_api->struct_size==32 && child_api->base==life && calls);assert(!pangu_lifecycle_query(2,31,1,0) && !pangu_lifecycle_query(2,32,2,0) && !pangu_lifecycle_query(2,32,1,1));assert(!pangu_invocation_query(2,48,1,0) && !pangu_invocation_query(1,47,1,0) && !pangu_invocation_query(1,48,2,0) && !pangu_invocation_query(1,48,1,1));pangu_prepare_result p;
 for(pangu_u64 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {const host_graph_case *c=&host_graph_cases[i];for(int reverse=0;reverse<2;reverse++) {install(c->installed,reverse);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=manager_entry;assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,&compiled,&corpus_storage,10000000,&p)==c->status);if(c->status) continue;for(int profile=0;profile<4;profile++) drive_manager(c->manifest,&p,&corpus_output,corpus_entry_output,profile,32);
 if(!sidecar_quota_done) {int chain=0;for(pangu_u32 a=0;a<p.graph_report.binding_count;a++) for(pangu_u32 b=0;b<p.graph_report.binding_count;b++) if(p.lowered.graph.provider_owners[corpus_output.bindings[a].provider]==p.lowered.graph.requirements[corpus_output.bindings[b].requirement].consumer) chain=1;if(chain) {drive_manager(c->manifest,&p,&corpus_output,corpus_entry_output,0,1);sidecar_quota_done=1;}}}}
 for(int variant=0;variant<4;variant++) for(int reverse=0;reverse<2;reverse++) {install(&installed,reverse);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=manager_entry;const pangu_static_manifest *m=variant>=2 ? &multi_manifest : &manifest;if(variant==1) instances[2].scope=2;if(variant>=2) multi_manifest.provisions=variant==3 ? reverse_provisions : provisions;assert(!pangu_discover_prepare(m,source_contracts,4,&compiled,&storage,1000000,&p));for(int profile=0;profile<6;profile++) drive_manager(m,&p,&output,entry_output,profile,32);if(variant==1) instances[2].scope=1;}
 assert(parent_setup_calls && parent_cleanup_calls && parent_business_calls && root_budgets && child_budgets && invalid_parents && cancellation_denials && quota_probe_done && sidecar_quota_done && max_depth>=2);
 printf("parent_cases=%d actual_service_calls=%d nested_setup_calls=%d nested_cleanup_calls=%d nested_business_calls=%d invalid_parent_refusals=%d alias_refusals=%d root_budget_refusals=%d child_budget_refusals=%d parent_busy=%d deferred_cleanup_roots=%d cancellation_refusals=%d completed_scope_refusals=%d retired_slots=%d quota_refusals=%d max_depth=%d\n",parent_cases,parent_actual_calls,parent_setup_calls,parent_cleanup_calls,parent_business_calls,invalid_parents,parent_aliases,root_budgets,child_budgets,parent_busy,async_roots,cancellation_denials,completed_scope_denials,retired_slots,quota_denials,max_depth);return 0;
}
