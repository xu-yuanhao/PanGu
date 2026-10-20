#include "src/bootstrap-api/host-plan.h"
#include "src/bootstrap-api/memory.h"
#define MODULE(n) {1,96,n,n,1,1,1,1,0,0}
static pangu_u32 callback_calls;
static pangu_i32 alpha(void *p) {callback_calls++;if(p) (*(pangu_u32 *)p)++;return 101;}
static pangu_i32 beta(void *p) {callback_calls++;if(p) (*(pangu_u32 *)p)++;return 202;}
static pangu_i32 gamma(void *p) {callback_calls++;if(p) (*(pangu_u32 *)p)++;return 303;}
static const pangu_plan_scope scopes[]={{0xffffffffU,0},{0,0},{0,0}};
#include "src/host-plan-probe/graph-cases.h"
static const pangu_manifest_definition definitions[]={ {"agent",1,1,0},{"other",1,1,0} };
static pangu_manifest_module modules[]={MODULE("provider"),MODULE("required"),MODULE("optional")};
static pangu_manifest_instance instances[]={ {"app","provider",1,0},{"session","provider",1,1},{"client","required",1,1},{"optional","optional",1,2} };
static pangu_manifest_binding bindings[]={ {"agent",0,1,1},{"agent",1,1,1},{"agent",2,1,1} };
static const pangu_manifest_provision provisions[]={ {"agent","app",1,1},{"agent","session",1,2} };
static pangu_manifest_requirement requirements[]={ {"agent","client",1,1},{"agent","optional",1,0},{"other","client",1,0} };
static pangu_module_contract source_contracts[]={ {"provider","agent",1,1,1,0},{"required","agent",1,1,2,1},{"required","other",1,1,2,0},{"optional","agent",1,1,2,0} };
static const pangu_module_contract canonical[]={ {"provider","agent",1,1,1,0},{"required","agent",1,1,2,1},{"required","other",1,1,2,0},{"optional","agent",1,1,2,0} };
static const pangu_static_entry entries[]={ {MODULE("provider"),alpha},{MODULE("required"),beta},{MODULE("optional"),gamma} };
static const pangu_installed_contract_set sets[]={ {canonical,1},{canonical+1,2},{canonical+3,1} };
static const pangu_installed_table installed={entries,sets,3};
static pangu_static_manifest manifest={scopes,3,definitions,2,modules,3,instances,4,bindings,3,provisions,2,requirements,3};
static pangu_plan_service lowered_services[2];static pangu_plan_instance lowered_instances[4];static pangu_plan_binding lowered_bindings[3];static pangu_plan_provider lowered_providers[2];static pangu_u32 lowered_owners[2];static pangu_plan_requirement lowered_requirements[3];
static const pangu_manifest_storage lower={lowered_services,2,lowered_instances,4,lowered_bindings,3,lowered_providers,lowered_owners,2,lowered_requirements,3};
static pangu_u32 pending[4],degree[4],queue[4],resolved[2],output_order[4],output_states[4],entry_work[3],entry_output[3];static pangu_plan_edge edges[16],output_bindings[16];
static pangu_graph_scratch scratch={pending,degree,queue,resolved,edges,16};
static pangu_graph_output output={output_order,4,output_states,4,output_bindings,16};
static pangu_discovery_storage storage={&lower,&scratch,&output,entry_work,entry_output,3};
static volatile pangu_u64 test_fuel;
static void fill(void *p,pangu_u64 n,unsigned char ch) {unsigned char *b=p;for(pangu_u64 i=0;i<n;i++) b[i]=ch;}
static int bytes(const void *p,pangu_u64 n,unsigned char ch) {const unsigned char *b=p;for(pangu_u64 i=0;i<n;i++) if(b[i]!=ch) return 0;return 1;}
static void poison(void) {fill(output_order,sizeof(output_order),0xa5);fill(output_states,sizeof(output_states),0xa5);fill(output_bindings,sizeof(output_bindings),0xa5);fill(entry_output,sizeof(entry_output),0xa5);}
static int unchanged(void) {return bytes(output_order,sizeof(output_order),0xa5) && bytes(output_states,sizeof(output_states),0xa5) && bytes(output_bindings,sizeof(output_bindings),0xa5) && bytes(entry_output,sizeof(entry_output),0xa5);}
static int empty(const pangu_prepare_result *r) {return !r->lowered.registry.scopes && !r->lowered.registry.services && !r->lowered.registry.providers && !r->lowered.graph.registry && !r->lowered.graph.instances && !r->lowered.graph.instance_count;}
static void name(unsigned char *p,const char *s) {pangu_u32 i=0;while(s[i]) {p[i]=(unsigned char)s[i];i++;}for(;i<32;i++) p[i]=0;}
static pangu_i32 prepare(pangu_u64 fuel,pangu_prepare_result *out) {return pangu_discover_prepare(&manifest,source_contracts,4,&installed,&storage,fuel,out);}
static int good(pangu_prepare_result *out)
{
 if(prepare(1000000,out) || out->stage!=6 || out->reserved || callback_calls || out->graph_report.order_count!=4 || out->graph_report.pending_count || out->graph_report.binding_count!=1) return 1;
 for(pangu_u32 i=0;i<4;i++) if(output_order[i]!=i || output_states[i]) return 2;
 for(pangu_u32 i=0;i<3;i++) if(entry_output[i]!=i) return 3;
 return output_bindings[0].requirement!=0 || output_bindings[0].provider!=1;
}
static int semantics(void)
{
 pangu_prepare_result out;if(good(&out)) return 20;pangu_u64 total=out.work;pangu_u32 seen=0;
 for(pangu_u64 fuel=1;fuel<total;fuel++) {test_fuel=fuel;poison();if(prepare(fuel,&out)!=2 || out.work>fuel || !empty(&out) || !unchanged()) return 21;seen|=1U<<out.stage;}
 if(seen!=62 || prepare(total,&out) || out.work!=total) return 22;
 for(pangu_u32 kind=0;kind<6;kind++) {
  poison();if(kind==0) name(modules[0].entry,"forged");if(kind==1) source_contracts[1].required=0;if(kind==2) name(instances[0].module,"absent");if(kind==3) requirements[0].required=0;if(kind==4) bindings[1].scope=0;if(kind==5) output.state_capacity=3;
  pangu_i32 expected=kind==2 ? 3 : kind==4 ? 4 : kind==5 ? 2 : 16;
  if(prepare(1000000,&out)!=expected || out.stage!=(kind==5 ? 5 : kind+1) || !empty(&out) || !unchanged()) return 23;
  if(kind==0) name(modules[0].entry,"provider");if(kind==1) source_contracts[1].required=1;if(kind==2) name(instances[0].module,"provider");if(kind==3) requirements[0].required=1;if(kind==4) bindings[1].scope=1;if(kind==5) output.state_capacity=4;
 }
 poison();source_contracts[1].required=0;requirements[0].required=0;if(prepare(1000000,&out)!=16 || out.stage!=2 || !empty(&out) || !unchanged()) return 24;source_contracts[1].required=1;requirements[0].required=1;
 fill(&out,192,0xa5);storage.entry_output=output_order;if(prepare(1000000,&out)!=1 || !bytes(&out,192,0xa5)) return 25;storage.entry_output=entry_output;
 storage.entry_scratch=(pangu_u32 *)(void *)canonical;if(prepare(1000000,&out)!=1 || !bytes(&out,192,0xa5)) return 26;storage.entry_scratch=entry_work;
 instances[2].scope=2;if(prepare(1000000,&out) || out.stage!=6 || out.graph_report.pending_count!=1 || out.graph_report.order_count!=3 || output_order[0]!=0 || output_order[1]!=1 || output_order[2]!=3 || output_states[2]!=1) return 27;instances[2].scope=1;
 if(good(&out)) return 28;return 0;
}
static int pages(const pangu_memory_api *memory)
{
 pangu_mapping maps[3];for(pangu_u32 i=0;i<3;i++) {maps[i].address=(void *)0;maps[i].length=0;if(memory->map(memory->context,4096,&maps[i])) return 30;}
 pangu_prepare_result *out=(pangu_prepare_result *)((unsigned char *)maps[0].address+4096-192);pangu_u32 *mapping=(pangu_u32 *)((unsigned char *)maps[1].address+4096-12);pangu_discovery_storage *desc=(pangu_discovery_storage *)((unsigned char *)maps[2].address+4096-48);
 desc->lower=&lower;desc->scratch=&scratch;desc->output=&output;desc->entry_scratch=entry_work;desc->entry_output=mapping;desc->entry_capacity=3;
 if(pangu_discover_prepare(&manifest,source_contracts,4,&installed,desc,1000000,out) || out->stage!=6 || mapping[0]!=0 || mapping[1]!=1 || mapping[2]!=2) return 31;
 fill(mapping,12,0xa5);desc->entry_capacity=2;if(pangu_discover_prepare(&manifest,source_contracts,4,&installed,desc,1000000,out)!=2 || !empty(out) || !bytes(mapping,12,0xa5)) return 32;
 for(pangu_u32 i=0;i<3;i++) if(memory->unmap(memory->context,&maps[i])) return 33;return 0;
}

static const pangu_plan_scope multi_scopes[]={{0xffffffffU,0}};
static const pangu_manifest_definition multi_definitions[]={{"agent",1,2,0},{"other",1,1,0}};
static const pangu_manifest_instance multi_instances[]={{"app","provider",1,0},{"session","provider",1,0},{"client","required",1,0},{"optional","optional",1,0}};
static const pangu_manifest_binding multi_bindings[]={{"agent",0,1,1}};
static const pangu_manifest_provision reverse_provisions[]={{"agent","session",1,2},{"agent","app",1,1}};
static pangu_static_manifest multi_manifest={multi_scopes,1,multi_definitions,2,modules,3,multi_instances,4,multi_bindings,1,provisions,2,requirements,3};
#define CYCLE_CONTRACTS(flag) {{"provider","agent",1,1,1,0},{"provider","agent",1,1,2,flag},{"required","agent",1,1,2,1},{"required","other",1,1,2,0},{"optional","agent",1,1,2,0}}
static const pangu_module_contract cycle_contracts[]=CYCLE_CONTRACTS(1),optional_cycle_contracts[]=CYCLE_CONTRACTS(0);
static const pangu_installed_contract_set cycle_sets[]={{cycle_contracts,2},{cycle_contracts+2,2},{cycle_contracts+4,1}};
static const pangu_installed_contract_set optional_cycle_sets[]={{optional_cycle_contracts,2},{optional_cycle_contracts+2,2},{optional_cycle_contracts+4,1}};
static const pangu_installed_table cycle_installed={entries,cycle_sets,3},optional_cycle_installed={entries,optional_cycle_sets,3};
static const pangu_manifest_requirement cycle_declarations[]={{"agent","client",1,1},{"agent","optional",1,0},{"other","client",1,0},{"agent","app",1,1},{"agent","session",1,1}};
static const pangu_manifest_requirement optional_cycle_declarations[]={{"agent","client",1,1},{"agent","optional",1,0},{"other","client",1,0},{"agent","app",1,0},{"agent","session",1,0}};
static pangu_plan_requirement lowered_cycle_requirements[5];
static const pangu_manifest_storage cycle_lower={lowered_services,2,lowered_instances,4,lowered_bindings,3,lowered_providers,lowered_owners,2,lowered_cycle_requirements,5};
static const pangu_discovery_storage cycle_storage={&cycle_lower,&scratch,&output,entry_work,entry_output,3};
static pangu_static_manifest cycle_manifest={scopes,3,definitions,2,modules,3,instances,4,bindings,3,provisions,2,cycle_declarations,5};
static int combinations(void)
{
 pangu_prepare_result out;
 for(pangu_u32 reverse=0;reverse<2;reverse++) {
  multi_manifest.provisions=reverse ? reverse_provisions : provisions;
  if(pangu_discover_prepare(&multi_manifest,source_contracts,4,&installed,&storage,1000000,&out) || out.stage!=6 || out.graph_report.order_count!=4 || out.graph_report.binding_count!=4 || callback_calls) return 50;
  for(pangu_u32 i=0;i<4;i++) if(output_order[i]!=i || output_states[i]) return 51;
  if(output_bindings[0].requirement!=0 || output_bindings[0].provider!=0 || output_bindings[1].requirement!=0 || output_bindings[1].provider!=1 || output_bindings[2].requirement!=1 || output_bindings[2].provider!=0 || output_bindings[3].requirement!=1 || output_bindings[3].provider!=1) return 52;
  if(out.lowered.registry.providers[0].instance!=(reverse ? 2U : 1U) || out.lowered.graph.provider_owners[0]!=(reverse ? 1U : 0U)) return 53;
 }
 for(pangu_u32 optional=0;optional<2;optional++) {
  poison();cycle_manifest.requirements=optional ? optional_cycle_declarations : cycle_declarations;
  if(pangu_discover_prepare(&cycle_manifest,optional ? optional_cycle_contracts : cycle_contracts,5,optional ? &optional_cycle_installed : &cycle_installed,&cycle_storage,1000000,&out)!=17 || out.stage!=5 || out.graph_report.reason!=8 || !empty(&out) || !unchanged() || callback_calls) return 54;
 }
 if(good(&out)) return 55;return 0;
}

static pangu_plan_service corpus_services[10];static pangu_plan_instance corpus_instances[9];static pangu_plan_binding corpus_scope_bindings[10];static pangu_plan_provider corpus_providers[10];static pangu_u32 corpus_owners[10];static pangu_plan_requirement corpus_requirements[90];
static const pangu_manifest_storage corpus_lower={corpus_services,10,corpus_instances,9,corpus_scope_bindings,10,corpus_providers,corpus_owners,10,corpus_requirements,90};
static pangu_u32 corpus_pending[9],corpus_degree[9],corpus_queue[9],corpus_resolved[10],corpus_order[9],corpus_states[9],corpus_entry_work[9],corpus_entry_output[9];static pangu_plan_edge corpus_edges[1000],corpus_output_edges[1000];
static const pangu_graph_scratch corpus_scratch={corpus_pending,corpus_degree,corpus_queue,corpus_resolved,corpus_edges,1000};
static const pangu_graph_output corpus_output={corpus_order,9,corpus_states,9,corpus_output_edges,1000};
static const pangu_discovery_storage corpus_storage={&corpus_lower,&corpus_scratch,&corpus_output,corpus_entry_work,corpus_entry_output,9};
static volatile pangu_u64 graph_case;
static int graph_corpus(void)
{
 pangu_u32 accepted=0,rejected=0;
 for(pangu_u64 i=0;i<sizeof(host_graph_cases)/sizeof(host_graph_cases[0]);i++) {
  graph_case=i;const host_graph_case *c=&host_graph_cases[i];pangu_prepare_result out;
  fill(corpus_order,sizeof(corpus_order),0xa5);fill(corpus_states,sizeof(corpus_states),0xa5);fill(corpus_output_edges,sizeof(corpus_output_edges),0xa5);fill(corpus_entry_output,sizeof(corpus_entry_output),0xa5);
  pangu_i32 rc=pangu_discover_prepare(c->manifest,c->contracts,c->count,c->installed,&corpus_storage,10000000,&out);
  if(rc!=c->status || callback_calls) return 60;
  if(rc) {rejected++;if(rc!=17 || out.stage!=5 || out.graph_report.reason!=8 || !empty(&out) || !bytes(corpus_order,sizeof(corpus_order),0xa5) || !bytes(corpus_states,sizeof(corpus_states),0xa5) || !bytes(corpus_output_edges,sizeof(corpus_output_edges),0xa5) || !bytes(corpus_entry_output,sizeof(corpus_entry_output),0xa5)) return 61;continue;}
  accepted++;if(out.stage!=6 || out.graph_report.order_count!=c->order_count || out.graph_report.binding_count!=c->binding_count) return 62;
  pangu_u32 pending_count=0;
  for(pangu_u64 j=0;j<c->manifest->instance_count;j++) {if(corpus_states[j]!=c->states[j]) return 63;pending_count+=c->states[j];}
  if(out.graph_report.pending_count!=pending_count) return 64;
  for(pangu_u32 j=0;j<c->order_count;j++) if(corpus_order[j]!=c->order[j]) return 65;
  for(pangu_u32 j=0;j<c->binding_count;j++) if(corpus_output_edges[j].requirement!=c->bindings[j].requirement || corpus_output_edges[j].provider!=c->bindings[j].provider) return 66;
  for(pangu_u64 j=0;j<c->manifest->module_count;j++) if(corpus_entry_output[j]!=j) return 67;
 }
 return accepted==95 && rejected==145 ? 0 : 68;
}
int host_plan_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;int rc=semantics();if(rc) return rc;rc=pages(memory);if(rc) return rc;rc=combinations();if(rc) return rc;rc=graph_corpus();if(rc) return rc;
 if(callback_calls) return 40;pangu_u32 count=0;pangu_i32 a=entries[entry_output[0]].entry(&count),b=entries[entry_output[1]].entry(&count),c=entries[entry_output[2]].entry(&count);
 return a==101 && b==202 && c==303 && count==3 && callback_calls==3 ? 0 : 41;
}
