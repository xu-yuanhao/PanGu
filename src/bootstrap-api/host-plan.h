#ifndef PANGU_HOST_PLAN_H
#define PANGU_HOST_PLAN_H
#include "src/bootstrap-api/bootstrap.h"
#ifndef __ASSEMBLER__
/* WIP planning kernel only. Local catalogue indices are not wire/ABI identities.
 * No plugin execution, activation, resource ownership or runtime access grant.
 * All arrays immutable for call, caller-owned. Output indices and report disjoint
 * from inputs and each other. Precondition1 preserves outputs/report; runtime
 * failures preserve indices, report disposable. Fuel charged before each scan.
 * mode1 isolate, mode2 explicit parent inheritance; absent binding denies lookup.
 * Providers must be declared in their own isolated realm; no child override of
 * inherited service. Cardinality1 unique per realm/service/exact version;
 * cardinality2 returns all matches in manifest order, never chooses first.
 */
typedef struct pangu_plan_scope {pangu_u32 parent,reserved;} pangu_plan_scope;
typedef struct pangu_plan_service {pangu_u32 cardinality,reserved;} pangu_plan_service;
typedef struct pangu_plan_binding {pangu_u32 scope,service,mode,reserved;} pangu_plan_binding;
typedef struct pangu_plan_provider {pangu_u32 scope,service,version,instance;} pangu_plan_provider;
typedef struct pangu_plan_registry {
 const pangu_plan_scope *scopes;pangu_u64 scope_count;
 const pangu_plan_service *services;pangu_u64 service_count;
 const pangu_plan_binding *bindings;pangu_u64 binding_count;
 const pangu_plan_provider *providers;pangu_u64 provider_count;
} pangu_plan_registry;
typedef struct pangu_plan_query {pangu_u32 scope,service,version,required;} pangu_plan_query;
typedef struct pangu_plan_report {pangu_i32 status;pangu_u32 reason;pangu_u64 work;pangu_u32 realm,count,location,reserved;} pangu_plan_report;
_Static_assert(sizeof(pangu_plan_registry)==64,"registry view");
_Static_assert(sizeof(pangu_plan_report)==32,"plan report");
/* Static planning WIP, no activation/callbacks/ownership/auth grant. All indices
 * local to immutable plan; plugin IDs distinct from provision IDs. Optional
 * present providers form real ordering edges; cycles return17, never silently
 * drop optional dependencies. Missing required dependencies mark Pending and
 * propagate; independent components still get ordered. Multi required means at
 * least one eligible provision; bind all eligible records in manifest order.
 * Scratch disposable. Final order/states/bindings unchanged on every failure.
 */
typedef struct pangu_plan_instance {pangu_u32 scope,id;} pangu_plan_instance;
typedef struct pangu_plan_requirement {pangu_u32 consumer,service,version,required;} pangu_plan_requirement;
typedef struct pangu_plan_edge {pangu_u32 requirement,provider;} pangu_plan_edge;
typedef struct pangu_graph_input {const pangu_plan_registry *registry;const pangu_plan_instance *instances;pangu_u64 instance_count;const pangu_u32 *provider_owners;const pangu_plan_requirement *requirements;pangu_u64 requirement_count;} pangu_graph_input;
typedef struct pangu_graph_scratch {pangu_u32 *pending,*degree,*queue,*resolved;pangu_plan_edge *edges;pangu_u64 edge_capacity;} pangu_graph_scratch;
typedef struct pangu_graph_output {pangu_u32 *order;pangu_u64 order_capacity;pangu_u32 *states;pangu_u64 state_capacity;pangu_plan_edge *bindings;pangu_u64 binding_capacity;} pangu_graph_output;
typedef struct pangu_graph_report {pangu_i32 status;pangu_u32 reason;pangu_u64 work;pangu_u32 order_count,pending_count,binding_count,location;} pangu_graph_report;
_Static_assert(sizeof(pangu_graph_input)==48,"graph input");
_Static_assert(sizeof(pangu_graph_scratch)==48,"graph scratch");
_Static_assert(sizeof(pangu_graph_output)==48,"graph output");
_Static_assert(sizeof(pangu_graph_report)==32,"graph report");
/* WIP static POD manifest, not runtime plugin loading. Canonical nonempty ASCII
 * IDs occupy32 bytes with zero padding, exact versions. Module descriptor ABI1,
 * target1 Linux-x86_64, host ABI interval must contain1, flags0 only. Entry is a
 * symbol identity only: no code executes. No config/permission/dynamic loader
 * support yet. Workspaces disposable; publish lowered views only on success.
 * Result contains an internal pointer: must stay at its original address and
 * must not be byte-copied/serialized. Lowered is NOT graph-valid/active/authorized.
 */
typedef struct pangu_manifest_definition {unsigned char name[32];pangu_u32 version,cardinality,reserved;} pangu_manifest_definition;
typedef struct pangu_manifest_module {pangu_u32 abi_version,struct_size;unsigned char name[32],entry[32];pangu_u32 version,target,host_min,host_max,flags,reserved;} pangu_manifest_module;
typedef struct pangu_manifest_instance {unsigned char name[32],module[32];pangu_u32 version,scope;} pangu_manifest_instance;
typedef struct pangu_manifest_binding {unsigned char service[32];pangu_u32 scope,mode,version;} pangu_manifest_binding;
typedef struct pangu_manifest_provision {unsigned char service[32],instance[32];pangu_u32 version,id;} pangu_manifest_provision;
typedef struct pangu_manifest_requirement {unsigned char service[32],consumer[32];pangu_u32 version,required;} pangu_manifest_requirement;
typedef struct pangu_static_manifest {
 const pangu_plan_scope *scopes;pangu_u64 scope_count;
 const pangu_manifest_definition *definitions;pangu_u64 definition_count;
 const pangu_manifest_module *modules;pangu_u64 module_count;
 const pangu_manifest_instance *instances;pangu_u64 instance_count;
 const pangu_manifest_binding *bindings;pangu_u64 binding_count;
 const pangu_manifest_provision *provisions;pangu_u64 provision_count;
 const pangu_manifest_requirement *requirements;pangu_u64 requirement_count;
} pangu_static_manifest;
typedef struct pangu_manifest_storage {
 pangu_plan_service *services;pangu_u64 service_capacity;
 pangu_plan_instance *instances;pangu_u64 instance_capacity;
 pangu_plan_binding *bindings;pangu_u64 binding_capacity;
 pangu_plan_provider *providers;pangu_u32 *owners;pangu_u64 provider_capacity;
 pangu_plan_requirement *requirements;pangu_u64 requirement_capacity;
} pangu_manifest_storage;
typedef struct pangu_manifest_result {pangu_plan_registry registry;pangu_graph_input graph;pangu_plan_report report;} pangu_manifest_result;
_Static_assert(sizeof(pangu_manifest_definition)==44,"versioned definition");
_Static_assert(sizeof(pangu_manifest_binding)==44,"versioned binding");
_Static_assert(sizeof(pangu_manifest_module)==96,"module descriptor");
_Static_assert(sizeof(pangu_static_manifest)==112,"static manifest");
_Static_assert(sizeof(pangu_manifest_storage)==88,"manifest storage");
_Static_assert(sizeof(pangu_manifest_result)==144,"manifest result");
/* WIP manifest consistency stage, after structural lowering and before graph
 * planning. Immutable module/service/version templates: role1 provides (required0),
 * role2 consumes(required0/1). Exactly one instance declaration per template;
 * no extra claims, omitted dependencies or changed required flags allowed.
 * Multi-provider multiplicity comes from multiple plugin instances, not duplicate
 * claims by one instance. Does not activate/load modules or authenticate metadata.
 * Caller charges report.work against shared pipeline budget. Preconditions1
 * preserve report; runtime failures report status/reason/location, no other writes.
 */
typedef struct pangu_module_contract {unsigned char module[32],service[32];pangu_u32 module_version,service_version,role,required;} pangu_module_contract;
_Static_assert(sizeof(pangu_module_contract)==80,"module contract");
/* Unified planning WIP. Global geometry precedes all writes. Structural lowering,
 * module contracts and graph planning share remaining work. Only stage4 publishes
 * lowered graph views; runtime failures clear those pointers/counts. Graph output
 * arrays remain unchanged on every failure; lowering/scratch disposable.
 * Preconditions1 preserve entire result. Result self-references cannot be moved.
 * Success may include Pending instances and is not activation or authorization.
 */
typedef struct pangu_prepare_result {pangu_manifest_result lowered;pangu_graph_report graph_report;pangu_u32 stage,reserved;pangu_u64 work;} pangu_prepare_result;
_Static_assert(sizeof(pangu_prepare_result)==192,"prepare result");
typedef pangu_i32 (*pangu_static_entry_fn)(void *);
typedef struct pangu_static_entry {pangu_manifest_module module;pangu_static_entry_fn entry;} pangu_static_entry;
_Static_assert(sizeof(pangu_static_entry)==104,"static entry");
/* Static symbol binding WIP. Table supplied by trusted compiled composition;
 * validates exact96-byte metadata, maps selected modules to nonnull functions,
 * never invokes them. Does not validate canonical module-contract ownership yet.
 * Preconditions preserve report/map; runtime errors preserve map. Caller fuel
 * bounds comparisons including commit; report disposable on runtime failure.
 */
typedef struct pangu_installed_contract_set {const pangu_module_contract *records;pangu_u64 count;} pangu_installed_contract_set;
_Static_assert(sizeof(pangu_installed_contract_set)==16,"installed contracts");
/* WIP: sets[i] belongs to the trusted compiled entries[i]. Config/source templates
 * must be exact canonical sets for selected modules, any ordering allowed.
 * Validates metadata correspondence, not cryptographic provenance; native trusted
 * callers can still forge inputs. No callbacks. Follow entry metadata validation
 * and precede instance-template/graph checks. Preconditions preserve report.
 */
typedef struct pangu_installed_table {const pangu_static_entry *entries;const pangu_installed_contract_set *sets;pangu_u64 count;} pangu_installed_table;
typedef struct pangu_discovery_storage {const pangu_manifest_storage *lower;const pangu_graph_scratch *scratch;const pangu_graph_output *output;pangu_u32 *entry_scratch,*entry_output;pangu_u64 entry_capacity;} pangu_discovery_storage;
_Static_assert(sizeof(pangu_installed_table)==24,"installed table view");
_Static_assert(sizeof(pangu_discovery_storage)==48,"discovery storage");
/* WIP full preparation path: bind -> canonical -> lower -> contracts -> graph.
 * Stage6 only publishes plan and entry_output; every runtime failure clears plan
 * and preserves graph outputs/entry_output. Scratch disposable; precondition1
 * preserves result. Success may contain Pending and never invokes callbacks.
 */
pangu_i32 pangu_discover_prepare(const pangu_static_manifest *,const pangu_module_contract *,pangu_u64,const pangu_installed_table *,const pangu_discovery_storage *,pangu_u64,pangu_prepare_result *);
#endif
#endif
