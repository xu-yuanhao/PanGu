extern __attribute__((visibility("hidden"))) int pangu_route_cpp_evaluate(void *,int,int *);
#include "src/sdk1-abi-adapter/adapter.h"
#include "src/t14-abi/sdk1-fixture.h"
#include "src/t14-abi/client-plugin-host-v1.h"
#define PANGU_ROUTE_TEST_MAIN sdk_route_regression_main
#include "route-fixture.h"
#undef PANGU_ROUTE_TEST_MAIN
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_entry(const pangu_plugin_context *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_dispose(const pangu_plugin_context *,const pangu_plugin_completion *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_load(const pangu_plugin_context *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk1_abi_cpp_use(const pangu_plugin_context *);
static const pangu_plugin_host_api *sdk_host;static int nested_setup_denials,nested_cleanup_denials,nested_business_calls,sdk_backend;
static pangu_plugin_bank bank;static pangu_plugin_binding sdk_bindings[9];static pangu_plugin_definition sdk_definitions[9];static sdk_fixture_config configs[9];static void *sdk_userdata[9];
static int sdk_cases,sdk_calls[3],sdk_refusals,sdk_aliases,sdk_budget_checks,sdk_operation_budget_refusals,sdk_bind_budgets,sdk_deferred,sdk_async_cleanup,sdk_cpp_calls,sdk_c_calls,current_language,sdk_operation_budget_refusals;
static pangu_plugin_lease saved_lease;static pangu_plugin_completion saved_setup,saved_dispose;static pangu_u32 saved_lease_id,saved_setup_id,saved_dispose_id;static int have_lease,have_setup,have_dispose;
static unsigned char snapshot[65536];static pangu_u64 snapshot_size;static pangu_u32 snapshot_owner;
static pangu_u64 image(unsigned char *out) {
 pangu_u64 used=0;
 #define SAVE(p,n) do {pangu_u64 bytes=(n);assert(used+bytes<=sizeof snapshot);if(out) memcpy(out+used,p,bytes);used+=bytes;} while(0)
 SAVE(&run,sizeof run);SAVE(owners,sizeof owners);SAVE(operations,sizeof operations);SAVE(resources,sizeof resources);SAVE(records,sizeof records);SAVE(service_slots,router.slot_count*sizeof service_slots[0]);SAVE(tables,router.slot_count*sizeof tables[0]);SAVE(service_states,router.slot_count*sizeof service_states[0]);SAVE(sdk_bindings,sizeof sdk_bindings);SAVE(&bank,sizeof bank);SAVE(items,sizeof items);SAVE(stop_edges,sizeof stop_edges);SAVE(disposed,sizeof disposed);SAVE(protected_regions,sizeof protected_regions);
 #undef SAVE
 return used;
}
pangu_sdk_i32 pangu_sdk_fixture_budget_unchanged(const pangu_plugin_context *c,pangu_sdk_u32 mode) {
 pangu_u32 id=(pangu_u32)((const sdk_fixture_config *)c->userdata-configs);assert(id<9 && c==&sdk_bindings[id].context);
 if(!mode) {snapshot_size=image(snapshot);snapshot_owner=id;return 1;}assert(snapshot_owner==id);unsigned char now[65536];pangu_u64 n=image(now);assert(n==snapshot_size && !memcmp(now,snapshot,n));sdk_budget_checks++;return 1;
}
pangu_sdk_i32 pangu_sdk_fixture_expected(const pangu_plugin_context *c,const pangu_plugin_lease *lease,const pangu_plugin_key *key,pangu_sdk_u32 ordinal,pangu_sdk_u32 stage) {
 pangu_u32 id=(pangu_u32)((const sdk_fixture_config *)c->userdata-configs);assert(id<9 && c==&sdk_bindings[id].context && stage>=1 && stage<=3);pangu_u32 seen=0;
 for(pangu_u32 b=0;b<current_plan->graph_report.binding_count;b++) {pangu_plan_edge e=current_graph->bindings[b];const pangu_plan_requirement *req=&current_plan->lowered.graph.requirements[e.requirement];if(req->consumer!=id || req->version!=key->version || memcmp(router.manifest->definitions[req->service].name,key->name,32)) continue;if(seen++!=ordinal) continue;pangu_u32 p=e.provider;assert(lease->table==&tables[p].header && lease->state==&service_states[p]);sdk_calls[stage-1]++;if(current_language) sdk_cpp_calls++;else sdk_c_calls++;return service_states[p].value;}
 assert(0);return -999;
}
/* Known gap probe: a called provider's own SDK context lacks delegated parent
 * scope during another plugin's setup/cleanup. Record refusal, do not call it
 * support. Business nesting is exercised with actual nested service invocation. */
static pangu_i32 nested_eval(void *opaque,pangu_i32 input,pangu_i32 *out) {
 service_state *state=opaque;pangu_u32 p=(pangu_u32)(state-service_states);assert(p<router.slot_count);pangu_u32 id=current_plan->lowered.graph.provider_owners[p];const pangu_plugin_context *c=&sdk_bindings[id].context;sdk_fixture_config *cfg=&configs[id];
 for(pangu_u32 d=0;d<cfg->dependency_count;d++) if(cfg->dependencies[d].expected_count) {
  pangu_plugin_request q;q.key=cfg->dependencies[d].key;q.ordinal=0;q.minimum_size=24;pangu_plugin_lease lease;pangu_plugin_report report;memset(&lease,0xa5,sizeof lease);memset(&report,0xa5,sizeof report);int rc=sdk1_abi_acquire(c->host,c,&q,100000,&lease,&report);
  if(run.phase==1 || run.phase==3) {assert(rc==10 && entry_poison(&lease,sizeof lease) && entry_poison(&report,sizeof report));if(run.phase==1) nested_setup_denials++;else nested_cleanup_denials++;}
  else {assert(run.phase==2 && !rc);const test_service *table=lease.table;pangu_i32 value=0;assert(!pangu_sdk1_guard_eval(table->evaluate,lease.state,5,&value) && value==((service_state *)lease.state)->value+5);assert(!sdk1_abi_release(c->host,c,&lease,100000));nested_business_calls++;}
  break;
 }
 return sdk_backend==0 ? eval_a(opaque,input,out) : sdk_backend==1 ? eval_b(opaque,input,out) : pangu_sdk1_guard_cpp_eval(opaque,input,out);
}
static pangu_i32 sdk_trampoline(void *opaque) {return sdk_host->enter(opaque,100000);}
static void config_for(const pangu_static_manifest *m,const pangu_prepare_result *p,const pangu_graph_output *g,int profile,int selected,int budget) {
 memset(configs,0,sizeof configs);memset(sdk_definitions,0,sizeof sdk_definitions);
 for(pangu_u32 i=0;i<m->instance_count;i++) {sdk_userdata[i]=&configs[i];configs[i].abi_call=pangu_abi_checked;configs[i].budget_unchanged=pangu_sdk_fixture_budget_unchanged;configs[i].expected=pangu_sdk_fixture_expected;sdk_definitions[i].entry=current_language ? pangu_sdk1_abi_cpp_entry : pangu_sdk1_abi_c_entry;sdk_definitions[i].dispose=current_language ? pangu_sdk1_abi_cpp_dispose : pangu_sdk1_abi_c_dispose;sdk_definitions[i].cleanup_mode=profile==3 || profile==5 || profile==6;if(profile==6) configs[i].flags=2;if(budget) configs[i].flags|=128;}
 if(selected>=0) {sdk_fixture_config *cfg=&configs[selected];if(profile==1 || profile==5 || profile==7) cfg->flags|=1;if(profile==2) cfg->flags|=3;if(profile==4) cfg->flags|=8|16;}
 for(pangu_u32 i=0;i<p->lowered.registry.provider_count;i++) {pangu_u32 id=p->lowered.graph.provider_owners[i];sdk_fixture_config *cfg=&configs[id];assert(cfg->provision_count<16);sdk_fixture_provision *s=&cfg->provisions[cfg->provision_count++];pangu_u32 service=p->lowered.registry.providers[i].service;memcpy(s->request.key.name,m->definitions[service].name,32);s->request.key.version=p->lowered.registry.providers[i].version;s->request.minimum_size=16;s->table=&tables[i].header;s->state=&service_states[i];for(pangu_u32 j=0;j+1<cfg->provision_count;j++) if(!memcmp(&cfg->provisions[j].request.key,&s->request.key,sizeof s->request.key)) s->request.ordinal++;}
 for(pangu_u32 i=0;i<p->lowered.graph.requirement_count;i++) {const pangu_plan_requirement *req=&p->lowered.graph.requirements[i];sdk_fixture_config *cfg=&configs[req->consumer];assert(cfg->dependency_count<16);sdk_fixture_dependency *dep=&cfg->dependencies[cfg->dependency_count++];memcpy(dep->key.name,m->definitions[req->service].name,32);dep->key.version=req->version;for(pangu_u32 b=0;b<p->graph_report.binding_count;b++) if(g->bindings[b].requirement==i) dep->expected_count++;}
}
static void bad_outputs(void) {
 for(pangu_u32 i=0;i<router.manifest->instance_count;i++) if(configs[i].entries && configs[i].dependency_count) {
  const pangu_plugin_context *c=&sdk_bindings[i].context;const pangu_plugin_sdk *api=c->host;pangu_plugin_request q;q.key=configs[i].dependencies[0].key;q.ordinal=0;q.minimum_size=24;
  pangu_plugin_count count;if(sdk1_abi_count(api,c,&q.key,100000,&count) || !count.count) continue;
  pangu_u32 table_index=0;while(table_index<router.slot_count && !service_slots[table_index].published) table_index++;assert(table_index<router.slot_count);
  struct {void *p;pangu_u64 n;} targets[]={{&bank,sizeof bank},{sdk_bindings,sizeof sdk_bindings},{(void *)pangu_abi_host_original()->base,sizeof(*sdk_host)},{(void *)c->host,sizeof(*c->host)},{&run,sizeof run},{records,sizeof records},{owners,sizeof owners},{operations,sizeof operations},{resources,sizeof resources},{service_slots,sizeof service_slots},{&tables[table_index],sizeof tables[table_index]},{protected_regions,sizeof protected_regions}};
  for(pangu_u32 j=0;j<sizeof targets/sizeof targets[0];j++) {unsigned char before[80];pangu_u64 n=targets[j].n<80 ? targets[j].n : 80;memcpy(before,targets[j].p,n);pangu_plugin_report report;memset(&report,0xa5,sizeof report);assert(sdk1_abi_acquire(api,c,&q,100000,targets[j].p,&report)==1 && !memcmp(before,targets[j].p,n) && entry_poison(&report,sizeof report));assert(sdk1_abi_count(api,c,&q.key,100000,targets[j].p)==1 && !memcmp(before,targets[j].p,n));assert(sdk1_abi_defer(api,c,100000,targets[j].p)==1 && !memcmp(before,targets[j].p,n));sdk_aliases+=3;}
  pangu_plugin_lease lease;memset(&lease,0xa5,sizeof lease);pangu_plugin_report report;memset(&report,0xa5,sizeof report);pangu_plugin_context fake=*c;assert(sdk1_abi_acquire(api,&fake,&q,100000,&lease,&report)==10 && entry_poison(&lease,sizeof lease) && entry_poison(&report,sizeof report));sdk_refusals++;
  assert(sdk1_abi_acquire(api,c,&q,100000,&lease,(void *)&lease)==1 && entry_poison(&lease,sizeof lease));sdk_aliases++;
  pangu_plugin_request bad=q;bad.ordinal=(pangu_u32)count.count;assert(sdk1_abi_acquire(api,c,&bad,100000,&lease,&report)==3 && entry_poison(&lease,sizeof lease) && entry_poison(&report,sizeof report));sdk_refusals++;
  bad=q;bad.key.name[0]^=1;assert(sdk1_abi_acquire(api,c,&bad,100000,&lease,&report)==3 && entry_poison(&lease,sizeof lease) && entry_poison(&report,sizeof report));pangu_plugin_count missing;memset(&missing,0xa5,sizeof missing);assert(sdk1_abi_count(api,c,&bad.key,100000,&missing)==3 && entry_poison(&missing,sizeof missing));sdk_refusals+=2;
  _Alignas(8) struct {pangu_u32 pad;pangu_plugin_request q;} aligned4;aligned4.q=q;assert(((pangu_u64)&aligned4.q&7)==4);assert(!sdk1_abi_count(api,c,&aligned4.q.key,100000,&missing));assert(!sdk1_abi_acquire(api,c,&aligned4.q,100000,&lease,&report));
  saved_lease=lease;saved_lease_id=i;have_lease=1;for(pangu_u32 other=0;other<router.manifest->instance_count;other++) if(other!=i && configs[other].entries) {pangu_sdk_fixture_budget_unchanged(c,0);assert(sdk1_abi_release(api,&sdk_bindings[other].context,&lease,100000)==10 && pangu_sdk_fixture_budget_unchanged(c,1));sdk_refusals++;break;}assert(!sdk1_abi_release(api,c,&lease,100000));
  pangu_plugin_completion token;memset(&token,0xa5,sizeof token);assert(sdk1_abi_defer(api,c,100000,&token)==10 && entry_poison(&token,sizeof token));sdk_refusals++;

 }
}
static int total_entries(void) {int n=0;for(int i=0;i<9;i++) n+=(int)configs[i].entries;return n;}
static int total_cleanups(void) {int n=0;for(int i=0;i<9;i++) n+=(int)configs[i].cleanups;return n;}
static void sdk_drive(const pangu_static_manifest *m,const pangu_prepare_result *p,const pangu_graph_output *g,const pangu_u32 *map,int language,int backend,int profile,int budget) {
 current_language=language;sdk_backend=backend;memset(&run,0,sizeof run);memset(disposed,0,sizeof disposed);memset(service_slots,0,sizeof service_slots);memset(service_states,0,sizeof service_states);current_plan=p;current_graph=g;epoch++;
 for(pangu_u32 i=0;i<m->instance_count;i++) {if(!owners[i].abi_version || owners[i].state==1) memset(&owners[i],0,sizeof owners[i]);assert(!life->owner_init(&owners[i],resources[i],2,operations[i],3,epoch,4));instances_in[i]=(pangu_entry_instance){&owners[i],&sdk_bindings[i]};}
 for(pangu_u32 i=0;i<p->lowered.registry.provider_count;i++) {tables[i]=(test_service){{p->lowered.registry.providers[i].version,24,1,0},nested_eval};service_states[i].value=(int)p->lowered.registry.providers[i].instance*11+3;}
 pangu_entry_request req={m,&compiled,map,m->module_count,p,g,life,&run,instances_in,m->instance_count};pangu_entry_storage st={records,9,sources,9,module_scratch,9};pangu_entry_prepared ep;assert(!bridge->prepare(&req,&st,100000,&ep));pangu_lifecycle_storage ls={items,9,stop_edges,1000};pangu_lifecycle_prepared lp;assert(!life->prepare(p,g,sources,m->instance_count,&ls,100000,&lp));pangu_lifecycle_graph sg={stop_edges,lp.edge_count,disposed,9};run.items=items;run.count=lp.item_count;run.stop_graph=&sg;assert(!life->bind_geometry(&run,protected_regions,64,100000));router=(pangu_service_router){life,&run,m,p,g,records,service_slots,p->lowered.registry.provider_count};
 int selected=p->graph_report.order_count ? (int)g->order[(profile==4 || profile==5) ? p->graph_report.order_count/2 : 0] : -1;config_for(m,p,g,profile,selected,budget);
 if(budget) {for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);memset(&bank,0xa5,sizeof bank);memset(sdk_bindings,0xa5,sizeof sdk_bindings);pangu_owner before[9];memcpy(before,owners,sizeof owners);int rc=sdk_host->bind(&bank,&router,route,sdk_bindings,m->instance_count,sdk_definitions,sdk_userdata,fuel);if(!rc) break;assert(rc==2 && entry_poison(&bank,sizeof bank) && entry_poison(sdk_bindings,sizeof sdk_bindings) && !memcmp(before,owners,sizeof owners));sdk_bind_budgets++;}}
 else assert(!sdk_host->bind(&bank,&router,route,sdk_bindings,m->instance_count,sdk_definitions,sdk_userdata,100000));
 pangu_lifecycle_report report;int rc=life->start(&run,1,&report),ticks=0;
 while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2) {int finished=0;
  for(pangu_u32 i=0;i<m->instance_count;i++) {sdk_fixture_config *cfg=&configs[i];if(run.setup_waiting && !run.setup_completed && cfg->entries && !cfg->loads && (cfg->flags&1)) {
   const pangu_plugin_context *c=&sdk_bindings[i].context;if(profile==5) {pangu_lifecycle_report stopped;int sr=life->stop(&run,1,&stopped);assert(sr==2 || sr==5);}else {assert(!(language ? pangu_sdk1_abi_cpp_load(c) : pangu_sdk1_abi_c_load(c)));}
   if(profile==7) {for(int word=0;word<4;word++) {pangu_plugin_completion bad=cfg->setup_token;bad.opaque[word]^=1;pangu_sdk_fixture_budget_unchanged(c,0);assert(sdk1_abi_finish_setup(c->host,c,&bad,0,100000)==10 && pangu_sdk_fixture_budget_unchanged(c,1));sdk_refusals++;}}
   assert(!sdk1_abi_finish_setup(c->host,c,&cfg->setup_token,0,100000));assert(sdk1_abi_finish_setup(c->host,c,&cfg->setup_token,0,100000)==10);sdk_refusals++;saved_setup=cfg->setup_token;saved_setup_id=i;have_setup=1;sdk_deferred++;finished=1;break;
  }}
  if(!finished) {for(pangu_u32 i=0;i<m->instance_count;i++) if(owners[i].cleanup_waiting && !owners[i].cleanup_completed) {sdk_fixture_config *cfg=&configs[i];const pangu_plugin_context *c=&sdk_bindings[i].context;pangu_plugin_completion bad=cfg->cleanup_token;bad.opaque[3]^=1;assert(sdk1_abi_finish_dispose(c->host,c,&bad,0,100000)==10);sdk_refusals++;assert(!sdk1_abi_finish_dispose(c->host,c,&cfg->cleanup_token,0,100000));assert(sdk1_abi_finish_dispose(c->host,c,&cfg->cleanup_token,0,100000)==10);sdk_refusals++;saved_dispose=cfg->cleanup_token;saved_dispose_id=i;have_dispose=1;sdk_async_cleanup++;finished=1;break;}}
  assert(finished);
 }else assert(report.reason==1);rc=life->pump(&run,1,&report);}
 if((profile==4 || profile==5) && run.count) {assert(rc==(profile==4 ? -81 : 6) && run.phase==4);if(profile==4) assert(run.cleanup_error==-82);}
 else {assert(!rc && run.phase==2);
  if(have_lease && saved_lease_id<m->instance_count && configs[saved_lease_id].entries) {const pangu_plugin_context *c=&sdk_bindings[saved_lease_id].context;pangu_sdk_fixture_budget_unchanged(c,0);assert(sdk1_abi_release(c->host,c,&saved_lease,100000)==10 && pangu_sdk_fixture_budget_unchanged(c,1));sdk_refusals++;}
  if(have_setup && saved_setup_id<m->instance_count && configs[saved_setup_id].entries) {const pangu_plugin_context *c=&sdk_bindings[saved_setup_id].context;pangu_sdk_fixture_budget_unchanged(c,0);assert(sdk1_abi_finish_setup(c->host,c,&saved_setup,0,100000)==10 && pangu_sdk_fixture_budget_unchanged(c,1));sdk_refusals++;}
  if(have_dispose && saved_dispose_id<m->instance_count && configs[saved_dispose_id].entries) {const pangu_plugin_context *c=&sdk_bindings[saved_dispose_id].context;pangu_sdk_fixture_budget_unchanged(c,0);assert(sdk1_abi_finish_dispose(c->host,c,&saved_dispose,0,100000)==10 && pangu_sdk_fixture_budget_unchanged(c,1));sdk_refusals++;}
  bad_outputs();for(pangu_u32 i=0;i<m->instance_count;i++) if(configs[i].entries) {assert(!(language ? pangu_sdk1_abi_cpp_use(&sdk_bindings[i].context) : pangu_sdk1_abi_c_use(&sdk_bindings[i].context)));}
  rc=life->stop(&run,1,&report);ticks=0;while(report.pending) {assert(report.work<=1 && ++ticks<100000);if(report.reason==2) {int found=0;for(pangu_u32 i=0;i<m->instance_count;i++) if(owners[i].cleanup_waiting && !owners[i].cleanup_completed) {const pangu_plugin_context *c=&sdk_bindings[i].context;assert(!sdk1_abi_finish_dispose(c->host,c,&configs[i].cleanup_token,0,100000));saved_dispose=configs[i].cleanup_token;saved_dispose_id=i;have_dispose=1;sdk_async_cleanup++;found=1;break;}assert(found);}else assert(report.reason==1);rc=life->stop(&run,1,&report);}assert(!rc);
 }
 assert(total_entries()==total_cleanups());for(pangu_u32 i=0;i<m->instance_count;i++) {assert(!configs[i].fail_line);if(configs[i].entries) {assert(sdk_host->enter(&records[i].context,100000)==10);sdk_refusals++;}else assert(!sdk_bindings[i].entered);}for(pangu_u32 i=0;i<m->instance_count;i++) sdk_operation_budget_refusals+=(int)configs[i].reserved;sdk_cases++;
}
int main(void) {
 pangu_abi_init();pangu_route_cpp_set_guard(pangu_abi_checked);life=real_life=pangu_abi_life();bridge=pangu_abi_entry();route=pangu_abi_route();sdk_host=pangu_abi_host()->base;assert(sdk_host && sdk_host->client->struct_size==72);assert(!pangu_plugin_host_query(2,40,1,0) && !pangu_plugin_host_query(1,39,1,0) && !pangu_plugin_host_query(1,40,2,0) && !pangu_plugin_host_query(1,40,1,1));pangu_prepare_result p;
 for(pangu_u64 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {const host_graph_case *c=&host_graph_cases[i];for(int language=0;language<2;language++) for(int profile=0;profile<4;profile++) {install(c->installed,language);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=sdk_trampoline;assert(pangu_discover_prepare(c->manifest,c->contracts,c->count,&compiled,&corpus_storage,10000000,&p)==c->status);if(c->status) continue;sdk_drive(c->manifest,&p,&corpus_output,corpus_entry_output,language,profile%3,profile,0);}}
 for(int variant=0;variant<4;variant++) for(int language=0;language<2;language++) for(int profile=0;profile<8;profile++) {install(&installed,language);for(pangu_u64 j=0;j<compiled.count;j++) compiled_entries[j].entry=sdk_trampoline;const pangu_static_manifest *m=variant>=2 ? &multi_manifest : &manifest;if(variant==1) instances[2].scope=2;if(variant>=2) multi_manifest.provisions=variant==3 ? reverse_provisions : provisions;assert(!pangu_discover_prepare(m,source_contracts,4,&compiled,&storage,1000000,&p));sdk_drive(m,&p,&output,entry_output,language,profile%3,profile,variant==0 && profile==0);if(variant==1) instances[2].scope=1;}
 assert(sdk_calls[0] && sdk_calls[1] && sdk_calls[2] && sdk_cpp_calls && sdk_c_calls && nested_setup_denials && nested_cleanup_denials && nested_business_calls);printf("sdk_cases=%d setup_calls=%d cleanup_calls=%d business_calls=%d c_plugin_calls=%d cpp_plugin_calls=%d refusals=%d alias_refusals=%d state_snapshot_checks=%d operation_budget_refusals=%d bind_budget_refusals=%d deferred_setups=%d async_cleanups=%d nested_setup_gap=%d nested_cleanup_gap=%d nested_business_calls=%d\n",sdk_cases,sdk_calls[0],sdk_calls[1],sdk_calls[2],sdk_c_calls,sdk_cpp_calls,sdk_refusals,sdk_aliases,sdk_budget_checks,sdk_operation_budget_refusals,sdk_bind_budgets,sdk_deferred,sdk_async_cleanup,nested_setup_denials,nested_cleanup_denials,nested_business_calls);pangu_sdk1_guard_report();return 0;
}
