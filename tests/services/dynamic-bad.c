#include <pangu/services/dynamic.h>
#include <stdlib.h>
#if defined(PG_BAD_SYMBOL)
PG_PLUGIN_EXPORT const int unrelated_symbol = 1;
#else
static pg_status apply(pg_scope *s, const void *c, size_t n) {
  (void)s;
  (void)c;
  (void)n;
  return PG_OK;
}
static const pg_dependency dependencies[] = {
    {{"missing.native.dependency", 25}, 1}};
static const pg_plugin plugin = {PANGU_SERVICES_ABI,
                                 sizeof(pg_plugin),
                                 {"bad", 3},
#ifdef PG_MISSING_DEPENDENCY
                                 dependencies,
                                 1,
#else
                                 NULL,
                                 0,
#endif
                                 apply};
static const pg_plugin *factory(const pg_plugin_host_api *api) {
  (void)api;
#ifdef PG_BAD_ABI
  abort(); /* Descriptor ABI rejection must happen before entering factory. */
#endif
  return &plugin;
}
PG_PLUGIN_EXPORT const pg_dynamic_descriptor pangu_plugin_descriptor_v1 = {
#ifdef PG_BAD_ABI
    3u, /* Pre-presence message layout: reject before the aborting factory. */
#else
    PG_DYNAMIC_ABI,
#endif
    sizeof(pg_dynamic_descriptor), factory};
#endif
