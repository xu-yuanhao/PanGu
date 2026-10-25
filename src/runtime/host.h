#ifndef PANGU_HOST_H
#define PANGU_HOST_H
#include <stddef.h>
#define PG_HOST_LIMIT 32
struct pg_host;
typedef struct {
 const char *name;
 void *context;
 const char *const *requires;
 size_t require_count;
 const char *const *provides;
 size_t provide_count;
 int (*start)(void *, struct pg_host *, size_t owner);
 int (*stop)(void *); /* 0 released, 1 pending, otherwise ownership retained. */
} pg_plugin;
typedef struct { const char *name; void *value; size_t owner; } pg_host_entry;
typedef enum { PG_HOST_INVALID=1, PG_HOST_DUPLICATE, PG_HOST_MISSING,
 PG_HOST_CYCLE, PG_HOST_START_FAILED, PG_HOST_UNPUBLISHED, PG_HOST_STOP_FAILED } pg_host_error;
/* Caller owns zeroed storage; reinitialization follows completed close. Descriptor/input arrays remain immutable and disjoint from host storage.
   Contexts and services remain valid through close. Calls are serialized.
   Names are nonempty NUL-terminated byte strings, at most 63 bytes.
   Each host has at most 32 plugins/resolved services, 32 child scopes and
   32 requirements per plugin. Values are borrowed from their providers. */
typedef struct pg_host {
 const pg_plugin *plugins;
 size_t count;
 pg_host_entry entries[PG_HOST_LIMIT];
 unsigned order[PG_HOST_LIMIT],states[PG_HOST_LIMIT];
 size_t service_count,live_count;
 unsigned phase,error;
 size_t current;
 unsigned dispatching,error_owner;
 struct pg_host *parent,*first_child,*next_sibling,*previous_sibling;
 size_t child_count;
} pg_host;
/* Preflight validates the whole scope and determines dependency order. It
   invokes no callbacks. Failure leaves a closed host with an error code. */
int pg_host_init(pg_host *,const pg_plugin *,size_t);
/* Child borrows active parent services; local exports shadow inherited names.
   Parent closes children first. Hierarchy depth is at most 32 child edges.
   Use close before discarding either storage. */
int pg_host_init_child(pg_host *,pg_host *parent,const pg_plugin *,size_t);
/* Starts in dependency order. Failure initiates reverse cleanup, including
   the partially started plugin; caller must drive close until released. */
int pg_host_start(pg_host *);
/* Only a starting provider may publish its declared exports. Publication is
   visible after its start succeeds and all promised exports are present. */
int pg_host_provide(pg_host *,size_t owner,const char *,void *);
/* A consumer may request only its declared dependencies, while starting,
   active or stopping. Missing/inactive services return NULL. */
void *pg_host_get(pg_host *,size_t owner,const char *);
/* Application entry: query an active scope by exported name. */
void *pg_host_find(pg_host *,const char *);
/* Dependents stop before providers. Pending/error cleanup keeps ownership
   and services borrowed by the stopping plugin until a later close call.
   Returns 0 released, 1 pending, -1 cleanup error. Repeated close is safe. */
int pg_host_close(pg_host *);
#endif
