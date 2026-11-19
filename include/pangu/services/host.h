#ifndef PANGU_SERVICES_HOST_H
#define PANGU_SERVICES_HOST_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PANGU_SERVICES_ABI 1u
typedef struct pg_host pg_host;
typedef struct pg_scope pg_scope;
typedef struct pg_lease pg_lease;
typedef struct {
  const char *data;
  size_t size;
} pg_name;
#ifdef __cplusplus
#define PG_NAME(s) pg_name{(s), sizeof(s) - 1}
#else
#define PG_NAME(s) ((pg_name){(s), sizeof(s) - 1})
#endif
typedef enum {
  PG_OK,
  PG_INVALID,
  PG_NOMEM,
  PG_NOT_FOUND,
  PG_CLOSED,
  PG_ABI,
  PG_FAILED
} pg_status;
typedef void (*pg_cleanup_fn)(void *);
typedef void (*pg_event_fn)(void *, pg_name, const void *, size_t);
typedef struct {
  pg_name name;
  uint32_t abi;
} pg_dependency;
typedef struct {
  uint32_t abi;
  size_t size;
  pg_name name;
  const pg_dependency *dependencies;
  size_t dependency_count;
  pg_status (*apply)(pg_scope *, const void *, size_t);
} pg_plugin;
/* Administration (create/install/register/acquire/emit/dispose) is serialized
 * by the host caller. release may run on any thread, including during disposal.
 * All callbacks run synchronously on the administration caller, without a host
 * lock. Callbacks must not destroy the host. Callbacks may register/query/emit,
 * but must not dispose a scope from an event callback or dispose any scope
 * whose lease they hold. No cancellation is implicit: owners cancel their work,
 * release leases, then dispose. A dispose blocks until all in-flight leases
 * leave; calling it with one's own lease would deadlock. Host destruction
 * requires exclusive caller access. Names are length-delimited and copied.
 * Tables and contexts are borrowed until scope cleanup; a lease keeps them
 * valid even after replacement/withdrawal. Scope handles remain valid until
 * host destruction; disposal is idempotent. On failure output handles are NULL.
 * Successful defer transfers cleanup to scope; failed defer leaves ownership
 * with caller. Cleanup runs in reverse registration order, after children and
 * in-flight users, before dependencies are released. Registration replacement
 * shadows ancestors/older bindings; unloading reveals the previous live
 * binding. Plugin registrations publish in the installing parent namespace;
 * plain scope registrations remain local. Missing dependencies fail
 * installation (no implicit asynchronous restart); withdrawing a provider
 * disposes its injected consumers before cleanup. No function table is copied.
 */
pg_status pg_host_create(pg_host **);
pg_scope *pg_host_root(pg_host *);
void pg_host_destroy(pg_host *);
pg_status pg_scope_create(pg_scope *, pg_scope **);
pg_status pg_scope_dispose(pg_scope *);
pg_status pg_scope_defer(pg_scope *, pg_cleanup_fn, void *);
pg_status pg_service_register(pg_scope *, pg_name, uint32_t abi,
                              const void *table, void *context);
pg_status pg_service_acquire(pg_scope *, pg_name, uint32_t abi, pg_lease **);
const void *pg_lease_table(const pg_lease *);
void *pg_lease_context(const pg_lease *);
void pg_lease_release(pg_lease *);
pg_status pg_plugin_install(pg_scope *, const pg_plugin *, const void *config,
                            size_t config_size, pg_scope **);
/* Dependency leases are borrowed through scope cleanup; index is descriptor
 * order. */
const pg_lease *pg_scope_dependency(const pg_scope *, size_t index);
pg_status pg_event_on(pg_scope *, pg_name, pg_event_fn, void *);
/* Ancestor listeners receive descendant events in registration order. Payload
 * is borrowed only for callbacks. New listeners do not join an active dispatch.
 */
pg_status pg_event_emit(pg_scope *, pg_name, const void *, size_t);
#ifdef __cplusplus
}
#endif
#endif
