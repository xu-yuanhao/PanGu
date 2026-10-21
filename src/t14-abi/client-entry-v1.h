#ifndef PANGU_ENTRY_BRIDGE_H
#define PANGU_ENTRY_BRIDGE_H
#include "client-abi-v1.h"
#define PANGU_ENTRY_ABI 1
#define PANGU_ENTRY_API_SIZE 24
#ifndef __ASSEMBLER__
/* Candidate trusted static-entry context. Borrowed/read-only through setup and
 * all deferred work. Local instance index is not a stable wire identifier.
 * Same immutable manifest/installed/map as the successful T13 preparation.
 */
typedef struct {
 pangu_u32 abi_version,struct_size;
 const pangu_lifecycle_api *host;pangu_lifecycle_runtime *runtime;pangu_owner *owner;
 void *userdata;pangu_u64 owner_epoch;pangu_u32 instance_id,position;pangu_u64 reserved;
} pangu_entry_context;
/* entered/reserved are bridge-private; context and entry remain immutable. */
typedef struct {pangu_entry_context context;pangu_static_entry_fn entry;pangu_u32 entered,reserved;} pangu_entry_record;
typedef struct {pangu_owner *owner;void *userdata;} pangu_entry_instance;
typedef struct {
 const pangu_static_manifest *manifest;const pangu_installed_table *installed;
 const pangu_u32 *entry_map;pangu_u64 entry_capacity;
 const pangu_prepare_result *plan;const pangu_graph_output *graph;
 const pangu_lifecycle_api *host;pangu_lifecycle_runtime *runtime;
 const pangu_entry_instance *instances;pangu_u64 instance_count;
} pangu_entry_request;
typedef struct {
 pangu_entry_record *records;pangu_u64 record_capacity;
 pangu_lifecycle_item *items;pangu_u64 item_capacity;
 pangu_u32 *module_scratch;pangu_u64 scratch_capacity;
} pangu_entry_storage;
typedef struct {pangu_u64 instance_count,work;} pangu_entry_prepared;
typedef struct {
 pangu_u32 abi_version,struct_size,target,reserved;
 pangu_i32 (*prepare)(const pangu_entry_request *,const pangu_entry_storage *,pangu_u64,pangu_entry_prepared *);
} pangu_entry_api;
_Static_assert(sizeof(pangu_entry_context)==64,"entry context");
_Static_assert(sizeof(pangu_entry_record)==80,"entry record");
_Static_assert(sizeof(pangu_entry_instance)==16,"entry source instance");
_Static_assert(sizeof(pangu_entry_request)==80,"entry request");
_Static_assert(sizeof(pangu_entry_storage)==48,"entry storage");
_Static_assert(sizeof(pangu_entry_prepared)==16,"entry result");
_Static_assert(sizeof(pangu_entry_api)==PANGU_ENTRY_API_SIZE,"entry API");
_Static_assert(__builtin_offsetof(pangu_entry_context,host)==8,"entry host offset");
_Static_assert(__builtin_offsetof(pangu_entry_context,owner_epoch)==40,"entry epoch offset");
_Static_assert(__builtin_offsetof(pangu_entry_context,position)==52,"entry position offset");
_Static_assert(__builtin_offsetof(pangu_entry_record,entry)==64,"record code offset");
_Static_assert(__builtin_offsetof(pangu_entry_record,entered)==72,"record once offset");
_Static_assert(__builtin_offsetof(pangu_entry_api,prepare)==16,"entry method offset");
const pangu_entry_api *pangu_entry_bridge_query(pangu_u32,pangu_u32,pangu_u32,pangu_u32);
#endif
#endif
