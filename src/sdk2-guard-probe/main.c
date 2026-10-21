#include "src/t14-guard/guard.h"
#include "src/t14-abi/sdk2-fixture.h"
#include "src/t14-abi/client-plugin-host2-v2.h"
#define PANGU_ROUTE_TEST_MAIN sdk2_base_regression_main
#include "route-fixture.h"
#undef PANGU_ROUTE_TEST_MAIN
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_entry(const pangu_plugin_context *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_load(const pangu_plugin_context *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_dispose(const pangu_plugin_context *,const pangu_plugin_completion *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_drain(const pangu_plugin_context *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_abi_cpp_use(const pangu_plugin_context *);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 sdk2_cpp_evaluate(void *,const pangu_plugin_lease *,pangu_sdk_i32,pangu_sdk_i32 *);
static const pangu_plugin_host_api2 *host2;static const pangu_invocation_api2 *inv2;static const pangu_lifecycle_child_api *kernel2;
static pangu_manifest_definition vdefs[16];static pangu_manifest_binding vbind[32];static pangu_manifest_provision vprov[16];static pangu_manifest_requirement vreq[128];static pangu_module_contract vsource[128],vcanonical[9][128];static pangu_static_entry ventries[9];static pangu_installed_contract_set vsets[9];static pangu_installed_table vtable;static pangu_static_manifest vmanifest;
static const pangu_prepare_result *plan_view;static const pangu_graph_output *graph_view;static const pangu_u32 *entry_view;
typedef struct {
 pangu_lifecycle_runtime runtime;pangu_owner own[9];pangu_owned_resource resource[9][2];pangu_owned_operation operation[9][3];
 pangu_entry_record rec[9];pangu_entry_instance input[9];pangu_lifecycle_item source[9],item[9];pangu_u32 scratch[9],disposed[9];pangu_lifecycle_edge edges[1000];pangu_lifecycle_graph graph;pangu_lifecycle_span spans[64];
 pangu_service_slot slots[16];pangu_service_router route;pangu_plugin_bank2 bank;pangu_invocation_slot caps[32];pangu_plugin_binding binding[9];pangu_plugin_definition defs[9];void *userdata[9];sdk2_fixture_config config[9];sdk2_test_service tables[16];sdk2_service_state states[16];
 pangu_lifecycle_report report;pangu_u64 epoch;int language,profile,chosen,status,early_completed;pangu_plugin_lease observed_root;
} model;
static int language_nested[2][3];
static model models[2];static int active_models,cases2,calls2,nested_calls[3],root_calls2,max_depth2,cross_runtime_refusals,negative_parent_refusals,aliases2,no_parent_refusals,parent_busy2,root_budget2,child_budget2,release_budget2,accept_budget_refusals,completed_refusals,constructor_budget_refusals,loader_version_refusals,constructor_invalid_refusals,cancel_refusals,async_resumes,cpp_entries,c_entries;
static pangu_u32 provider_depth(model *m,pangu_u64 index) {pangu_u32 depth=0;for(;m->caps[index].parent!=~(pangu_u64)0;index=m->caps[index].parent) {assert(++depth<32);}return depth;}
static model *owner_of(const pangu_plugin_context *c,pangu_u32 *id) {
 for(int k=0;k<active_models;k++) for(pangu_u32 i=0;i<vmanifest.instance_count;i++) if(c==&models[k].binding[i].context) {*id=i;assert(c->userdata==&models[k].config[i]);return &models[k];}assert(0);return 0;
}
static void save_models(void);
static void models_same(void);
static int parent_request(model *,const pangu_plugin_lease *,const pangu_plugin_context **,pangu_plugin_request *);
static int accept_budget_done;
void sdk2_fixture_entered(const pangu_plugin_context *c,const pangu_plugin_lease *cap,void *state) {
 pangu_u32 id;model *m=owner_of(c,&id);assert(cap->opaque[0]==(pangu_u64)&m->bank.calls && cap->opaque[2]<32);pangu_invocation_slot *slot=&m->caps[cap->opaque[2]];assert(slot->active && slot->lease.ticket.provider.owner==&m->own[id] && cap->state==state);pangu_u32 depth=provider_depth(m,cap->opaque[2]);
 if(!depth) m->observed_root=*cap;
 const pangu_plugin_sdk2 *sdk=pangu_plugin_sdk2_view(c);assert(sdk);
 if(!accept_budget_done) {save_models();for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);int rc=sdk->accept(c,cap,fuel);if(!rc) break;assert(rc==2);models_same();accept_budget_refusals++;}accept_budget_done=1;}
 if(depth) {
  pangu_invocation_slot *root=&m->caps[slot->root];pangu_u32 caller=(pangu_u32)(root->lease.ticket.consumer.owner-m->own);const pangu_plugin_context *caller_ctx=&m->binding[caller].context;const pangu_plugin_sdk2 *caller_api=pangu_plugin_sdk2_view(caller_ctx);save_models();assert(sdk2_abi_release(caller_api,caller_ctx,&m->observed_root,100000)==2);models_same();parent_busy2++;
  if(m->profile==6 && m->runtime.phase==3 && !m->early_completed && m->own[caller].cleanup_waiting && !m->own[caller].cleanup_callback && !m->own[caller].cleanup_completed) {
   assert(!sdk2_abi_finish_dispose(caller_api,caller_ctx,&m->config[caller].cleanup_token,0,100000));m->early_completed=1;
   const pangu_plugin_context *receiver;pangu_plugin_request q;assert(parent_request(m,&m->observed_root,&receiver,&q));const pangu_plugin_sdk2 *receiver_api=pangu_plugin_sdk2_view(receiver);pangu_plugin_lease out;pangu_plugin_report report;memset(&out,0xa5,sizeof out);save_models();assert(sdk2_abi_acquire_child(receiver_api,receiver,&m->observed_root,&q,100000,&out,&report)==10 && entry_poison(&out,sizeof out));models_same();completed_refusals++;
  }
 }
 if((int)depth>max_depth2) max_depth2=(int)depth;calls2++;assert(((const sdk2_test_service *)cap->table)->evaluate==(m->language ? sdk2_cpp_evaluate : sdk2_c_evaluate));if(depth) {assert(m->runtime.phase>=1 && m->runtime.phase<=3);int phase=m->runtime.phase==1 ? 0 : m->runtime.phase==3 ? 1 : 2;nested_calls[phase]++;language_nested[m->language][phase]++;}else root_calls2++;
}
pangu_sdk_i32 sdk2_fixture_expected(const pangu_plugin_context *c,const pangu_plugin_lease *lease,const pangu_plugin_key *key,pangu_sdk_u32 ordinal) {
 pangu_u32 id;model *m=owner_of(c,&id);pangu_u32 count=0;for(pangu_u32 b=0;b<plan_view->graph_report.binding_count;b++) {pangu_plan_edge edge=graph_view->bindings[b];const pangu_plan_requirement *req=&plan_view->lowered.graph.requirements[edge.requirement];if(req->consumer!=id || req->version!=key->version || memcmp(vmanifest.definitions[req->service].name,key->name,32)) continue;if(count++!=ordinal) continue;assert(lease->state==&m->states[edge.provider] && lease->table==&m->tables[edge.provider]);return m->states[edge.provider].value;}assert(0);return -999;
}
static void save_models(void);
static void models_same(void);
static pangu_i32 trampoline2(void *opaque) {
 const pangu_entry_context *e=opaque;pangu_plugin_binding *binding=e->userdata;pangu_plugin_context original=binding->context;
 binding->context.abi_version=1;binding->context.host=host2->base->client;save_models();assert(host2->enter(e,100000)==10);models_same();binding->context=original;loader_version_refusals++;
 save_models();assert(host2->base->enter(e,100000)==1);models_same();loader_version_refusals++;return host2->enter(e,100000);
}
static pangu_u32 version2(pangu_u32 v) {assert(v && v<0xffffff9bU);return v+100;}
static void copy_array(void *out,const void *in,pangu_u64 bytes) {if(bytes) memcpy(out,in,bytes);}
static void clone_versioned(const pangu_static_manifest *m,const pangu_module_contract *contracts,pangu_u64 count,const pangu_installed_table *installed) {
 assert(m->definition_count<=16 && m->binding_count<=32 && m->provision_count<=16 && m->requirement_count<=128 && installed->count<=9 && count<=128);vmanifest=*m;copy_array(vdefs,m->definitions,m->definition_count*sizeof vdefs[0]);for(pangu_u32 i=0;i<m->definition_count;i++) vdefs[i].version=version2(vdefs[i].version);vmanifest.definitions=vdefs;copy_array(vbind,m->bindings,m->binding_count*sizeof vbind[0]);for(pangu_u32 i=0;i<m->binding_count;i++) vbind[i].version=version2(vbind[i].version);vmanifest.bindings=vbind;copy_array(vprov,m->provisions,m->provision_count*sizeof vprov[0]);for(pangu_u32 i=0;i<m->provision_count;i++) vprov[i].version=version2(vprov[i].version);vmanifest.provisions=vprov;copy_array(vreq,m->requirements,m->requirement_count*sizeof vreq[0]);for(pangu_u32 i=0;i<m->requirement_count;i++) vreq[i].version=version2(vreq[i].version);vmanifest.requirements=vreq;
 copy_array(vsource,contracts,count*sizeof vsource[0]);for(pangu_u32 i=0;i<count;i++) vsource[i].service_version=version2(vsource[i].service_version);
 for(pangu_u32 i=0;i<installed->count;i++) {ventries[i]=installed->entries[i];ventries[i].entry=trampoline2;assert(installed->sets[i].count<=128);copy_array(vcanonical[i],installed->sets[i].records,installed->sets[i].count*sizeof vcanonical[i][0]);for(pangu_u32 j=0;j<installed->sets[i].count;j++) vcanonical[i][j].service_version=version2(vcanonical[i][j].service_version);vsets[i]=(pangu_installed_contract_set){vcanonical[i],installed->sets[i].count};}vtable=(pangu_installed_table){ventries,vsets,installed->count};
}
static pangu_u64 model_bytes(void) {return sizeof models;}
static unsigned char snapshot[sizeof models];
static void save_models(void) {memcpy(snapshot,models,model_bytes());}
static void models_same(void) {assert(!memcmp(snapshot,models,model_bytes()));}
static void populate(model *m) {
 memset(m->config,0,sizeof m->config);memset(m->defs,0,sizeof m->defs);for(pangu_u32 i=0;i<vmanifest.instance_count;i++) {m->userdata[i]=&m->config[i];m->config[i].abi_call=pangu_abi_checked;m->config[i].expected=sdk2_fixture_expected;m->config[i].entered=sdk2_fixture_entered;m->defs[i].entry=m->language ? pangu_abi_cpp_entry : pangu_abi_c_entry;m->defs[i].dispose=m->language ? pangu_abi_cpp_dispose : pangu_abi_c_dispose;}
 if(m->chosen>=0) {sdk2_fixture_config *cfg=&m->config[m->chosen];if(m->profile==1 || m->profile==2 || m->profile==5) cfg->flags=1;if(m->profile==3) cfg->flags=3;if(m->profile==4) cfg->flags=8|16;if(m->profile==1 || m->profile==2 || m->profile==6) m->defs[m->chosen].cleanup_mode=1;}
 for(pangu_u32 i=0;i<plan_view->lowered.registry.provider_count;i++) {pangu_u32 id=plan_view->lowered.graph.provider_owners[i];sdk2_fixture_config *cfg=&m->config[id];assert(cfg->provision_count<16);sdk2_provision *p=&cfg->provisions[cfg->provision_count++];pangu_u32 service=plan_view->lowered.registry.providers[i].service;memcpy(p->request.key.name,vdefs[service].name,32);p->request.key.version=plan_view->lowered.registry.providers[i].version;p->request.minimum_size=16;for(pangu_u32 j=0;j+1<cfg->provision_count;j++) if(!memcmp(&cfg->provisions[j].request.key,&p->request.key,sizeof p->request.key)) p->request.ordinal++;p->table=&m->tables[i];p->state=&m->states[i];m->tables[i]=(sdk2_test_service){p->request.key.version,24,1,0,m->language ? sdk2_cpp_evaluate : sdk2_c_evaluate};m->states[i].value=(int)plan_view->lowered.registry.providers[i].instance*11+3;}
 for(pangu_u32 i=0;i<plan_view->lowered.graph.requirement_count;i++) {const pangu_plan_requirement *req=&plan_view->lowered.graph.requirements[i];sdk2_fixture_config *cfg=&m->config[req->consumer];assert(cfg->dependency_count<16);sdk2_dependency *d=&cfg->dependencies[cfg->dependency_count++];memcpy(d->key.name,vdefs[req->service].name,32);d->key.version=req->version;for(pangu_u32 b=0;b<plan_view->graph_report.binding_count;b++) if(graph_view->bindings[b].requirement==i) d->expected_count++;}
}
static pangu_i32 understate_bind(pangu_invocation_bank *b,const pangu_service_router *v,const pangu_service_api *api,const pangu_lifecycle_child_api *child,pangu_invocation_slot *slots,pangu_u64 n,pangu_u64 epoch,pangu_u64 fuel,pangu_invocation_bind_plan *out) {
 int rc=inv2->prepare_bind(b,v,api,child,slots,n,epoch,fuel,out);if(!rc) {assert(out->bind_fuel);out->bind_fuel--;}return rc;
}
static void bad_constructors(model *m) {
 struct {pangu_plugin_bank2 *bank;pangu_plugin_binding *binding;pangu_invocation_slot *caps;} bad[]={{(void *)m->binding,m->binding,m->caps},{&m->bank,(void *)m->caps,m->caps},{&m->bank,m->binding,(void *)m->binding},{(void *)&m->runtime,m->binding,m->caps},{&m->bank,m->binding,(void *)m->defs},{&m->bank,(void *)m->userdata,m->caps},{(void *)inv2,m->binding,m->caps}};
 for(pangu_u32 i=0;i<sizeof bad/sizeof bad[0];i++) {save_models();assert(host2->bind(bad[i].bank,&m->route,route,inv2,kernel2,bad[i].binding,vmanifest.instance_count,m->defs,m->userdata,bad[i].caps,32,m->epoch,100000)==1);models_same();aliases2++;}
 if(vmanifest.instance_count) {pangu_plugin_definition original=m->defs[0];m->defs[0].cleanup_mode=2;save_models();assert(host2->bind(&m->bank,&m->route,route,inv2,kernel2,m->binding,vmanifest.instance_count,m->defs,m->userdata,m->caps,32,m->epoch,100000)==1);models_same();m->defs[0]=original;constructor_invalid_refusals++;}
 pangu_invocation_api2 false_plan=*inv2;false_plan.prepare_bind=understate_bind;save_models();assert(host2->bind(&m->bank,&m->route,route,&false_plan,kernel2,m->binding,vmanifest.instance_count,m->defs,m->userdata,m->caps,32,m->epoch,100000)==2);models_same();constructor_invalid_refusals++;
}
static void construct(model *m,int language,int profile,int budget) {
 m->language=language;m->profile=profile;m->early_completed=0;m->epoch++;m->chosen=plan_view->graph_report.order_count ? (int)graph_view->order[plan_view->graph_report.order_count-1] : -1;memset(&m->runtime,0,sizeof m->runtime);memset(m->disposed,0,sizeof m->disposed);memset(m->slots,0,sizeof m->slots);memset(m->states,0,sizeof m->states);
 for(pangu_u32 i=0;i<vmanifest.instance_count;i++) {if(!m->own[i].abi_version || m->own[i].state==1) memset(&m->own[i],0,sizeof m->own[i]);assert(!life->owner_init(&m->own[i],m->resource[i],2,m->operation[i],3,m->epoch,4));m->input[i]=(pangu_entry_instance){&m->own[i],&m->binding[i]};}
 pangu_entry_request req={&vmanifest,&vtable,entry_view,vmanifest.module_count,plan_view,graph_view,life,&m->runtime,m->input,vmanifest.instance_count};pangu_entry_storage es={m->rec,9,m->source,9,m->scratch,9};pangu_entry_prepared ep;assert(!bridge->prepare(&req,&es,100000,&ep));pangu_lifecycle_storage ls={m->item,9,m->edges,1000};pangu_lifecycle_prepared lp;assert(!life->prepare(plan_view,graph_view,m->source,vmanifest.instance_count,&ls,100000,&lp));m->graph=(pangu_lifecycle_graph){m->edges,lp.edge_count,m->disposed,9};m->runtime.items=m->item;m->runtime.count=lp.item_count;m->runtime.stop_graph=&m->graph;assert(!life->bind_geometry(&m->runtime,m->spans,64,100000));m->route=(pangu_service_router){life,&m->runtime,&vmanifest,plan_view,graph_view,m->rec,m->slots,plan_view->lowered.registry.provider_count};populate(m);
 if(budget) bad_constructors(m);
 if(budget) {save_models();for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);int rc=host2->bind(&m->bank,&m->route,route,inv2,kernel2,m->binding,vmanifest.instance_count,m->defs,m->userdata,m->caps,32,m->epoch,fuel);if(!rc) break;assert(rc==2);models_same();constructor_budget_refusals++;}}
 else assert(!host2->bind(&m->bank,&m->route,route,inv2,kernel2,m->binding,vmanifest.instance_count,m->defs,m->userdata,m->caps,32,m->epoch,100000));
 save_models();assert(host2->bind(&m->bank,&m->route,route,inv2,kernel2,m->binding,vmanifest.instance_count,m->defs,m->userdata,m->caps,32,m->epoch,100000)==10);models_same();constructor_invalid_refusals++;
}
static int model_pending(model *m) {return m->runtime.setup_waiting && !m->runtime.setup_completed;}
static void start(model *m) {m->status=life->start(&m->runtime,1,&m->report);int ticks=0;while(m->report.pending && m->report.reason==1) {assert(m->report.work<=1 && ++ticks<100000);m->status=life->pump(&m->runtime,1,&m->report);}}
static int parent_request(model *m,const pangu_plugin_lease *parent,const pangu_plugin_context **context,pangu_plugin_request *q) {
 pangu_u32 p=(pangu_u32)((const sdk2_service_state *)parent->state-m->states);assert(p<plan_view->lowered.registry.provider_count);pangu_u32 id=plan_view->lowered.graph.provider_owners[p];sdk2_fixture_config *cfg=&m->config[id];
 for(pangu_u32 i=0;i<cfg->dependency_count;i++) if(cfg->dependencies[i].expected_count) {*context=&m->binding[id].context;q->key=cfg->dependencies[i].key;q->ordinal=0;q->minimum_size=24;return 1;}return 0;
}
static void no_parent_grant(model *m,const pangu_plugin_lease *parent) {
 const pangu_plugin_context *c;pangu_plugin_request q;if(!parent_request(m,parent,&c,&q)) return;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);pangu_plugin_count count;assert(!sdk2_abi_count(api,c,&q.key,100000,&count) && count.count);
 pangu_plugin_lease out;pangu_plugin_report report;memset(&out,0xa5,sizeof out);memset(&report,0xa5,sizeof report);save_models();assert(sdk2_abi_acquire(api,c,&q,100000,&out,&report)==10 && entry_poison(&out,sizeof out) && entry_poison(&report,sizeof report));models_same();no_parent_refusals++;
}
static void bad_outputs(model *m) {
 for(pangu_u32 i=0;i<vmanifest.instance_count;i++) if(m->config[i].entries && m->config[i].dependency_count && m->config[i].dependencies[0].expected_count) {
  const pangu_plugin_context *c=&m->binding[i].context;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);pangu_plugin_request q;q.key=m->config[i].dependencies[0].key;q.ordinal=0;q.minimum_size=24;
  pangu_plugin_lease cap;pangu_plugin_report report;assert(!sdk2_abi_acquire(api,c,&q,100000,&cap,&report));
  struct {void *p;pangu_u64 n;} targets[]={{&m->bank,sizeof m->bank},{m->caps,sizeof m->caps},{m->binding,sizeof m->binding},{(void *)api,sizeof(*api)},{(void *)pangu_abi_host_original(),sizeof(*host2)},{(void *)inv2,sizeof(*inv2)},{(void *)inv2->base,sizeof(*inv2->base)},{(void *)kernel2,sizeof(*kernel2)},{&m->runtime,sizeof m->runtime},{m->own,sizeof m->own},{m->operation,sizeof m->operation},{m->rec,sizeof m->rec},{(void *)cap.table,24}};
  for(pangu_u32 j=0;j<sizeof targets/sizeof targets[0];j++) {unsigned char before[80];pangu_u64 bytes=targets[j].n<80 ? targets[j].n : 80;memcpy(before,targets[j].p,bytes);memset(&report,0xa5,sizeof report);save_models();assert(sdk2_abi_acquire(api,c,&q,100000,targets[j].p,&report)==1 && entry_poison(&report,sizeof report));models_same();assert(!memcmp(before,targets[j].p,bytes));pangu_plugin_count *out=targets[j].p;assert(sdk2_abi_count(api,c,&q.key,100000,out)==1);models_same();aliases2+=2;}
  pangu_plugin_context copied=*c;pangu_plugin_lease out;memset(&out,0xa5,sizeof out);save_models();assert(sdk2_abi_acquire(api,&copied,&q,100000,&out,&report)==10 && entry_poison(&out,sizeof out));models_same();negative_parent_refusals++;
  assert(!sdk2_abi_release(api,c,&cap,100000));break;
 }
}
static void finish_waits(model *m) {
 if(model_pending(m)) {pangu_u32 id=(pangu_u32)m->chosen;sdk2_fixture_config *cfg=&m->config[id];const pangu_plugin_context *c=&m->binding[id].context;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);assert(api);
  if(m->profile==2) {pangu_lifecycle_report stopped;int rc=life->stop(&m->runtime,1,&stopped);assert(rc==2 || rc==5);}
  if(cfg->pending) {if(m->profile!=2) no_parent_grant(m,&cfg->pending_root);int rc=m->language ? pangu_abi_cpp_drain(c) : pangu_abi_c_drain(c);if(m->profile==2) {assert(!rc || rc==10);if(rc==10) cancel_refusals++;}else assert(!rc);}
  if(m->profile!=2) assert(!(m->language ? pangu_abi_cpp_load(c) : pangu_abi_c_load(c)));
  assert(!sdk2_abi_finish_setup(api,c,&cfg->setup_token,0,100000));
 }else {int found=0;for(pangu_u32 i=0;i<vmanifest.instance_count;i++) if(m->own[i].cleanup_waiting && (!m->own[i].cleanup_completed || m->config[i].pending)) {const pangu_plugin_context *c=&m->binding[i].context;sdk2_fixture_config *cfg=&m->config[i];if(cfg->pending) {no_parent_grant(m,&cfg->pending_root);int rc=m->language ? pangu_abi_cpp_drain(c) : pangu_abi_c_drain(c);if(m->profile==6 && m->early_completed) assert(!rc || rc==10);else assert(!rc);async_resumes++;}if(!m->own[i].cleanup_completed) assert(!c->host->finish_dispose(c,&cfg->cleanup_token,0,100000));found=1;break;}assert(found);}
}
static void startup_done(model *m) {int ticks=0;while(m->report.pending) {assert(m->report.work<=1 && ++ticks<100000);if(m->report.reason==2) finish_waits(m);else assert(m->report.reason==1);m->status=life->pump(&m->runtime,1,&m->report);}if((m->profile==2 || m->profile==4) && m->runtime.count) {assert(m->status==(m->profile==2 ? 6 : -81) && m->runtime.phase==4);if(m->profile==4) assert(m->runtime.cleanup_error==-82);}else assert(!m->status && m->runtime.phase==2);}
static void stop_done(model *m) {if(m->runtime.phase==4) return;m->status=life->stop(&m->runtime,1,&m->report);int ticks=0;while(m->report.pending) {assert(m->report.work<=1 && ++ticks<100000);if(m->report.reason==2) finish_waits(m);else assert(m->report.reason==1);m->status=life->stop(&m->runtime,1,&m->report);}assert(!m->status);}
static void check_final(model *m) {int entries=0,cleanups=0;for(pangu_u32 i=0;i<vmanifest.instance_count;i++) {assert(!m->config[i].fail_line && !m->config[i].pending);entries+=(int)m->config[i].entries;cleanups+=(int)m->config[i].cleanups;if(m->language) cpp_entries+=(int)m->config[i].entries;else c_entries+=(int)m->config[i].entries;}assert(entries==cleanups);for(int i=0;i<32;i++) assert(!m->caps[i].active && !m->caps[i].children);cases2++;}
static void cross_runtime(void) {
 for(int a=0;a<2;a++) {model *m=&models[a],*foreign=&models[1-a];if(m->chosen<0 || !m->config[m->chosen].pending) continue;pangu_plugin_lease *cap=&m->config[m->chosen].pending_root;pangu_u32 p=(pangu_u32)((sdk2_service_state *)cap->state-m->states),id=plan_view->lowered.graph.provider_owners[p];const pangu_plugin_context *good=&m->binding[id].context,*bad=&foreign->binding[id].context;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(good),*wrong=pangu_plugin_sdk2_view(bad);assert(api && wrong);
  save_models();assert(wrong->accept(bad,cap,100000)==10);models_same();cross_runtime_refusals++;save_models();assert(wrong->base.release(&foreign->binding[m->chosen].context,cap,100000)==10);models_same();cross_runtime_refusals++;assert(!sdk2_abi_accept(api,good,cap,100000));
  for(pangu_u32 i=0;i<vmanifest.instance_count;i++) if(foreign->config[i].entries && foreign->config[i].dependency_count && foreign->config[i].dependencies[0].expected_count) {pangu_plugin_request q;q.key=foreign->config[i].dependencies[0].key;q.ordinal=0;q.minimum_size=24;pangu_plugin_lease out;pangu_plugin_report report;memset(&out,0xa5,sizeof out);memset(&report,0xa5,sizeof report);const pangu_plugin_context *c=&foreign->binding[i].context;const pangu_plugin_sdk2 *v=pangu_plugin_sdk2_view(c);save_models();assert(sdk2_abi_acquire_child(v,c,cap,&q,100000,&out,&report)==10 && entry_poison(&out,sizeof out) && entry_poison(&report,sizeof report));models_same();cross_runtime_refusals++;break;}
  for(int word=0;word<10;word++) {pangu_plugin_lease forged=*cap;if(word<8) forged.opaque[word]^=1;else if(word==8) forged.table=(const void *)((pangu_u64)forged.table^8);else forged.state=(void *)((pangu_u64)forged.state^8);save_models();assert(sdk2_abi_accept(api,good,&forged,100000)==10);models_same();negative_parent_refusals++;}
 }
}
static int root_budget2_done,child_budget2_done,release_budget2_done;
static void operation_budgets(model *m) {
 if(root_budget2_done && child_budget2_done && release_budget2_done) return;
 for(pangu_u32 i=0;i<vmanifest.instance_count;i++) if(m->config[i].entries) for(pangu_u32 d=0;d<m->config[i].dependency_count;d++) if(m->config[i].dependencies[d].expected_count) {
  const pangu_plugin_context *caller=&m->binding[i].context;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(caller);pangu_plugin_request q;q.key=m->config[i].dependencies[d].key;q.ordinal=0;q.minimum_size=24;pangu_plugin_lease root;pangu_plugin_report report;
  if(!root_budget2_done) {save_models();for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);memset(&root,0xa5,sizeof root);memset(&report,0xa5,sizeof report);int rc=sdk2_abi_acquire(api,caller,&q,fuel,&root,&report);if(!rc) {assert(report.work<=fuel);break;}assert(rc==2 && entry_poison(&root,sizeof root));models_same();if(report.reserved==0) assert(report.work<=fuel && report.pending && report.reason==1);root_budget2++;}root_budget2_done=1;}else assert(!sdk2_abi_acquire(api,caller,&q,100000,&root,&report));
  const pangu_plugin_context *receiver;pangu_plugin_request childq;
  if(!child_budget2_done && parent_request(m,&root,&receiver,&childq)) {const pangu_plugin_sdk2 *callee=pangu_plugin_sdk2_view(receiver);pangu_plugin_lease child;save_models();for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);memset(&child,0xa5,sizeof child);memset(&report,0xa5,sizeof report);int rc=sdk2_abi_acquire_child(callee,receiver,&root,&childq,fuel,&child,&report);if(!rc) {assert(report.work<=fuel);break;}assert(rc==2 && entry_poison(&child,sizeof child));models_same();if(report.reserved==0) assert(report.work<=fuel && report.pending && report.reason==1);child_budget2++;}assert(!sdk2_abi_release(callee,receiver,&child,100000));child_budget2_done=1;}
  if(!release_budget2_done) {save_models();for(pangu_u64 fuel=0;;fuel++) {assert(fuel<100000);int rc=sdk2_abi_release(api,caller,&root,fuel);if(!rc) break;assert(rc==2);models_same();release_budget2++;}release_budget2_done=1;}else assert(!sdk2_abi_release(api,caller,&root,100000));return;
 }
}
static void run_single(int language,int profile,int budget) {active_models=1;model *m=&models[0];construct(m,language,profile,budget);start(m);startup_done(m);if(m->runtime.phase==2) {operation_budgets(m);bad_outputs(m);for(pangu_u32 i=0;i<vmanifest.instance_count;i++) if(m->config[i].entries) assert(!(language ? pangu_abi_cpp_use(&m->binding[i].context) : pangu_abi_c_use(&m->binding[i].context)));}stop_done(m);check_final(m);}
int main(void) {
 pangu_abi_init();life=real_life=pangu_abi_life();kernel2=pangu_abi_child();bridge=pangu_abi_entry();route=pangu_abi_route();host2=pangu_abi_host();inv2=pangu_abi_inv();assert(host2 && host2->base && host2->client->base.abi_version==2 && host2->client->base.struct_size==88 && inv2 && inv2->base);pangu_prepare_result plan;
 for(pangu_u64 i=0;i<sizeof host_graph_cases/sizeof host_graph_cases[0];i++) {const host_graph_case *c=&host_graph_cases[i];clone_versioned(c->manifest,c->contracts,c->count,c->installed);assert(pangu_discover_prepare(&vmanifest,vsource,c->count,&vtable,&corpus_storage,10000000,&plan)==c->status);if(c->status) continue;plan_view=&plan;graph_view=&corpus_output;entry_view=corpus_entry_output;for(int language=0;language<2;language++) for(int profile=0;profile<7;profile++) if(profile<4 || profile==6) run_single(language,profile,0);
  active_models=2;construct(&models[0],0,1,0);construct(&models[1],1,1,0);start(&models[0]);start(&models[1]);cross_runtime();startup_done(&models[0]);startup_done(&models[1]);stop_done(&models[1]);stop_done(&models[0]);check_final(&models[0]);check_final(&models[1]);
 }
 for(int variant=0;variant<4;variant++) {const pangu_static_manifest *m=variant>=2 ? &multi_manifest : &manifest;if(variant==1) instances[2].scope=2;if(variant>=2) multi_manifest.provisions=variant==3 ? reverse_provisions : provisions;clone_versioned(m,source_contracts,4,&installed);assert(!pangu_discover_prepare(&vmanifest,vsource,4,&vtable,&storage,1000000,&plan));plan_view=&plan;graph_view=&output;entry_view=entry_output;for(int language=0;language<2;language++) for(int profile=0;profile<7;profile++) run_single(language,profile,variant==0 && profile==0);if(variant==1) instances[2].scope=1;}
 assert(nested_calls[0] && nested_calls[1] && nested_calls[2] && cross_runtime_refusals && negative_parent_refusals && max_depth2>=2 && cpp_entries && c_entries && constructor_budget_refusals && parent_busy2 && accept_budget_refusals && completed_refusals && root_budget2_done && child_budget2_done && release_budget2_done);
 assert(loader_version_refusals==2*(c_entries+cpp_entries));
 for(int language=0;language<2;language++) for(int phase=0;phase<3;phase++) assert(language_nested[language][phase]);
 printf("sdk2_cases=%d actual_method_calls=%d nested_setup_calls=%d nested_cleanup_calls=%d nested_business_calls=%d root_calls=%d max_depth=%d cross_runtime_refusals=%d invalid_parent_refusals=%d alias_refusals=%d no_parent_refusals=%d parent_busy=%d root_budget_refusals=%d child_budget_refusals=%d release_budget_refusals=%d accept_budget_refusals=%d completed_scope_refusals=%d constructor_budget_refusals=%d constructor_invalid_refusals=%d loader_version_refusals=%d cancelled_child_refusals=%d async_root_resumes=%d c_entries=%d cpp_entries=%d c_nested_setup=%d c_nested_cleanup=%d c_nested_business=%d cpp_nested_setup=%d cpp_nested_cleanup=%d cpp_nested_business=%d\n",cases2,calls2,nested_calls[0],nested_calls[1],nested_calls[2],root_calls2,max_depth2,cross_runtime_refusals,negative_parent_refusals,aliases2,no_parent_refusals,parent_busy2,root_budget2,child_budget2,release_budget2,accept_budget_refusals,completed_refusals,constructor_budget_refusals,constructor_invalid_refusals,loader_version_refusals,cancel_refusals,async_resumes,c_entries,cpp_entries,language_nested[0][0],language_nested[0][1],language_nested[0][2],language_nested[1][0],language_nested[1][1],language_nested[1][2]);pangu_abi_report();return 0;
}
