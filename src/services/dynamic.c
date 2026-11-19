#include <pangu/services/dynamic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
static const pg_plugin_host_api api = {
    PG_DYNAMIC_ABI,      sizeof(pg_plugin_host_api),
    pg_scope_defer,      pg_service_register,
    pg_service_acquire,  pg_lease_table,
    pg_lease_context,    pg_lease_release,
    pg_scope_dependency, pg_event_on,
    pg_event_emit};
typedef struct {
  uv_lib_t library;
} loaded;
typedef struct {
  loaded *module;
  const pg_plugin *plugin;
  const void *config;
  size_t size;
  int transferred;
} install_context;
static void close_library(void *ctx) {
  loaded *m = ctx;
  uv_dlclose(&m->library);
  free(m);
}
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  (void)size;
  install_context *c = (install_context *)config;
  pg_status st = pg_scope_defer(scope, close_library, c->module);
  if (st)
    return st;
  c->transferred = 1;
  return c->plugin->apply(scope, c->config, c->size);
}
static pg_status fail(pg_dynamic_diagnostic *d, pg_dynamic_stage stage,
                      pg_status st, const char *message) {
  if (d) {
    d->stage = stage;
    snprintf(d->message, sizeof(d->message), "%s", message);
  }
  return st;
}
static int valid(pg_name p) {
  return p.data && p.size && !memchr(p.data, 0, p.size);
}
static int absolute(pg_name p) {
#ifdef _WIN32
  return p.size > 2 &&
         ((p.data[1] == ':' && (p.data[2] == '/' || p.data[2] == '\\')) ||
          (p.data[0] == '\\' && p.data[1] == '\\'));
#else
  return p.size && p.data[0] == '/';
#endif
}
pg_status pg_dynamic_plugin_load(pg_scope *parent, pg_name cwd, pg_name path,
                                 const void *config, size_t size,
                                 pg_scope **out, pg_dynamic_diagnostic *d) {
  if (d)
    memset(d, 0, sizeof(*d));
  if (!out)
    return fail(d, PG_DYNAMIC_PATH, PG_INVALID, "output scope is required");
  *out = NULL;
  if (!parent || !valid(path) || (!config && size))
    return fail(d, PG_DYNAMIC_PATH, PG_INVALID,
                "invalid local plugin path or configuration slice");
  size_t prefix = 0;
  if (!absolute(path)) {
    if (!valid(cwd) || !absolute(cwd))
      return fail(d, PG_DYNAMIC_PATH, PG_INVALID,
                  "relative plugin path requires absolute cwd");
    if (cwd.size == SIZE_MAX)
      return fail(d, PG_DYNAMIC_PATH, PG_NOMEM,
                  "plugin path allocation failed");
    prefix = cwd.size + 1;
  }
  if (path.size > SIZE_MAX - prefix - 1)
    return fail(d, PG_DYNAMIC_PATH, PG_NOMEM, "plugin path allocation failed");
  char *name = malloc(prefix + path.size + 1);
  if (!name)
    return fail(d, PG_DYNAMIC_PATH, PG_NOMEM, "plugin path allocation failed");
  if (prefix) {
    memcpy(name, cwd.data, cwd.size);
    name[cwd.size] = '/';
  }
  memcpy(name + prefix, path.data, path.size);
  name[prefix + path.size] = 0;
  uv_fs_t req;
  int rc = uv_fs_realpath(NULL, &req, name, NULL);
  free(name);
  if (rc) {
    uv_fs_req_cleanup(&req);
    return fail(d, PG_DYNAMIC_PATH, PG_NOT_FOUND,
                "local plugin path cannot be resolved");
  }
  loaded *m = calloc(1, sizeof(*m));
  if (!m) {
    uv_fs_req_cleanup(&req);
    return fail(d, PG_DYNAMIC_OPEN, PG_NOMEM,
                "plugin handle allocation failed");
  }
  rc = uv_dlopen(req.ptr, &m->library);
  uv_fs_req_cleanup(&req);
  if (rc) {
    close_library(m);
    return fail(
        d, PG_DYNAMIC_OPEN, PG_FAILED,
        "local plugin library cannot be opened (format/platform/dependencies)");
  }
  void *symbol = NULL;
  if (uv_dlsym(&m->library, PG_DYNAMIC_SYMBOL, &symbol) || !symbol) {
    close_library(m);
    return fail(d, PG_DYNAMIC_SYMBOL_ERROR, PG_NOT_FOUND,
                "missing pangu_plugin_descriptor_v1 export");
  }
  const pg_dynamic_descriptor *exported = symbol;
  if (exported->abi != PG_DYNAMIC_ABI || exported->size < sizeof(*exported) ||
      !exported->factory) {
    close_library(m);
    return fail(d, PG_DYNAMIC_DESCRIPTOR, PG_ABI,
                "incompatible native plugin descriptor ABI/size/factory");
  }
  const pg_plugin *plugin = exported->factory(&api);
  if (!plugin || plugin->abi != PANGU_SERVICES_ABI ||
      plugin->size < sizeof(*plugin)) {
    close_library(m);
    return fail(d, PG_DYNAMIC_DESCRIPTOR, PG_ABI,
                "incompatible plugin lifecycle descriptor ABI/size");
  }
  if (!plugin->apply || !valid(plugin->name) ||
      (!plugin->dependencies && plugin->dependency_count)) {
    close_library(m);
    return fail(d, PG_DYNAMIC_DESCRIPTOR, PG_INVALID,
                "invalid native plugin lifecycle descriptor");
  }
  install_context context = {m, plugin, config, size, 0};
  pg_plugin wrapper = *plugin;
  wrapper.apply = apply;
  pg_status st =
      pg_plugin_install(parent, &wrapper, &context, sizeof(context), out);
  if (st) {
    if (!context.transferred)
      close_library(m);
    return fail(
        d, PG_DYNAMIC_INSTALL, st,
        "native plugin dependencies or apply failed; scope rolled back");
  }
  return PG_OK;
}
