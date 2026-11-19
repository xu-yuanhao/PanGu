#ifndef PANGU_SERVICES_DYNAMIC_H
#define PANGU_SERVICES_DYNAMIC_H
#include <pangu/services/host.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native boundary version covers BOTH the injected host function table and
 * the current public Core message/content/tool/callback layouts used by modules.
 * ABI 4 appends uint32_t presence (six optional-field bits) to pangu_message;
 * pangu_tool is unchanged from ABI 3 (owner-aware validate_call).
 * ABI 1/2/3 modules must be rebuilt: their shorter messages cannot be passed
 * to current Core consumers safely (ABI 1/2 also have older tool layouts).
 * Loader checks this version before invoking factory/apply. Constructors still
 * run at dlopen. The export name below is a stable descriptor bootstrap symbol;
 * its historical v1 suffix does not override descriptor.abi compatibility. */
#define PG_DYNAMIC_ABI 4u
#define PG_DYNAMIC_SYMBOL "pangu_plugin_descriptor_v1"
#if defined(_WIN32)
#define PG_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define PG_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define PG_PLUGIN_EXPORT
#endif
/* Injected host ABI avoids executable symbol export / duplicate host instances.
 * Immutable table lives for the process. Extensions must not link their own
 * host implementation; all scope effects use this table. */
typedef struct {
  uint32_t abi;
  size_t size;
  pg_status (*defer)(pg_scope *, pg_cleanup_fn, void *);
  pg_status (*register_service)(pg_scope *, pg_name, uint32_t, const void *,
                                void *);
  pg_status (*acquire)(pg_scope *, pg_name, uint32_t, pg_lease **);
  const void *(*table)(const pg_lease *);
  void *(*context)(const pg_lease *);
  void (*release)(pg_lease *);
  const pg_lease *(*dependency)(const pg_scope *, size_t);
  pg_status (*event_on)(pg_scope *, pg_name, pg_event_fn, void *);
  pg_status (*event_emit)(pg_scope *, pg_name, const void *, size_t);
} pg_plugin_host_api;
/* Export a const data symbol named pangu_plugin_descriptor_v1. Its factory
 * returns a process/library-lifetime pg_plugin, with no registration or other
 * side effects. apply uses normal pg_plugin lifecycle, injected host functions,
 * and must defer every effect before exposing it. No C++ exceptions cross ABI.
 * ABI/size are checked before calling factory or apply. Config is borrowed only
 * through apply; retained data must be copied or explicitly caller-owned. */
typedef struct {
  uint32_t abi;
  size_t size;
  const pg_plugin *(*factory)(const pg_plugin_host_api *);
} pg_dynamic_descriptor;
typedef enum {
  PG_DYNAMIC_NONE,
  PG_DYNAMIC_PATH,
  PG_DYNAMIC_OPEN,
  PG_DYNAMIC_SYMBOL_ERROR,
  PG_DYNAMIC_DESCRIPTOR,
  PG_DYNAMIC_INSTALL
} pg_dynamic_stage;
typedef struct {
  pg_dynamic_stage stage;
  char
      message[256]; /* safe bounded stage diagnostic; excludes config/OS text */
} pg_dynamic_diagnostic;
/* Explicit local native-code execution, never package/network installation or
 * a sandbox. path/cwd are UTF-8 length-delimited (pg_name.size), NUL forbidden.
 * Relative path requires absolute cwd; canonical absolute path is resolved
 * before uv_dlopen, never searched via PATH/loader basename lookup. Caller's
 * trust/approval policy runs before this function. Library constructors run at
 * dlopen; only cooperative apply effects are transactionally rolled back.
 * out is NULL on failure. diagnostics optional, copied, survives unload.
 * Success returns normal scope; pg_scope_dispose withdraws services, waits for
 * outstanding leases, executes plugin cleanups, then closes the library last.
 * Same administration/callback threading rules as host.h. Do not dispose while
 * holding your own lease. No function/table pointer survives its lease/scope.
 * Missing dependencies and apply failure close the library and roll back scope.
 * Loader cannot hot-replace an on-disk loaded binary; unload before replacing.
 */
pg_status pg_dynamic_plugin_load(pg_scope *parent, pg_name cwd, pg_name path,
                                 const void *config, size_t config_size,
                                 pg_scope **out, pg_dynamic_diagnostic *);
#ifdef __cplusplus
}
#endif
#endif
