#include <pangu/services/tools.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  pg_tools_service service;
  pangu_tool *tools;
} aggregate;
typedef struct {
  pg_name output;
  size_t count;
} configuration;
static void cleanup(void *ctx) {
  aggregate *a = ctx;
  free(a->tools);
  free(a);
}
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  if (!config || size != sizeof(configuration))
    return PG_INVALID;
  const configuration *cfg = config;
  aggregate *a = calloc(1, sizeof(*a));
  if (!a)
    return PG_NOMEM;
  pg_status st = pg_scope_defer(scope, cleanup, a);
  if (st) {
    free(a);
    return st;
  }
  size_t count = 0;
  for (size_t i = 0; i < cfg->count; ++i) {
    const pg_tools_service *s = pg_lease_table(pg_scope_dependency(scope, i));
    if (!s || (s->count && !s->tools))
      return PG_INVALID;
    if (s->count > SIZE_MAX - count)
      return PG_NOMEM;
    count += s->count;
  }
  if (count > SIZE_MAX / sizeof(*a->tools))
    return PG_NOMEM;
  if (count) {
    a->tools = malloc(count * sizeof(*a->tools));
    if (!a->tools)
      return PG_NOMEM;
  }
  for (size_t i = 0; i < cfg->count; ++i) {
    const pg_tools_service *s = pg_lease_table(pg_scope_dependency(scope, i));
    for (size_t j = 0; j < s->count; ++j) {
      pangu_tool t = s->tools[j];
      if (!t.name.data || !t.name.len)
        return PG_INVALID;
      for (size_t k = 0; k < a->service.count; ++k) {
        pangu_slice n = a->tools[k].name;
        if (n.len == t.name.len && !memcmp(n.data, t.name.data, n.len))
          return PG_INVALID;
      }
      a->tools[a->service.count++] = t;
    }
  }
  a->service.tools = a->tools;
  return pg_service_register(scope, cfg->output, 1, &a->service, a);
}
pg_status pg_tools_aggregate_install(pg_scope *parent, pg_name output,
                                     const pg_dependency *sources, size_t count,
                                     pg_scope **out) {
  if (!out)
    return PG_INVALID;
  *out = NULL;
  if (!parent || !output.data || !output.size ||
      memchr(output.data, 0, output.size) || (count && !sources))
    return PG_INVALID;
  for (size_t i = 0; i < count; ++i)
    if (sources[i].abi != 1)
      return PG_ABI;
  configuration cfg = {output, count};
  pg_plugin plugin = {PANGU_SERVICES_ABI,
                      sizeof(pg_plugin),
                      {"pangu.tools.aggregate", 21},
                      sources,
                      count,
                      apply};
  return pg_plugin_install(parent, &plugin, &cfg, sizeof(cfg), out);
}
