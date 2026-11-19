/* Independent shared module: no host or Core binary linkage. */
#include "fixture.h"
#include <pangu/services/dynamic.h>
#include <stdlib.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
static const pg_plugin_host_api *host;
typedef struct {
  pg_native_fixture_config config;
  pangu_tool tool;
  pg_tools_service service;
} instance;
static void cleanup(void *ctx) {
  instance *i = ctx;
  if (i->config.cleaned)
    ++*i->config.cleaned;
  free(i);
}
static pangu_status validate(void *ctx, pangu_slice schema, pangu_slice args) {
  instance *i = ctx;
  return i->config.validate(i->config.validator_ctx, schema, args);
}
static pangu_status execute(void *ctx, pangu_core *core,
                            const pangu_content *call, pangu_tool_sink sink,
                            void *out) {
  (void)ctx;
  (void)core;
  (void)call;
  pangu_content block = {.kind = PANGU_TEXT,
                         .data = SL("external native result")};
  pangu_message result = {
      .role = PANGU_TOOL_RESULT, .content = &block, .content_count = 1};
  return sink(out, &result, 1);
}
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  if (!config || size != sizeof(pg_native_fixture_config))
    return PG_INVALID;
  const pg_native_fixture_config *c = config;
  if (!c->validate)
    return PG_INVALID;
  instance *i = calloc(1, sizeof(*i));
  if (!i)
    return PG_NOMEM;
  i->config = *c;
  pg_status st = host->defer(scope, cleanup, i);
  if (st) {
    free(i);
    return st;
  }
  i->tool = (pangu_tool){
      .name = SL("native_echo"),
      .description = SL("External native tool"),
      .schema_json = SL("{\"type\":\"object\",\"additionalProperties\":false}"),
      .ctx = i,
      .validate = validate,
      .execute = execute};
  i->service = (pg_tools_service){&i->tool, 1};
  st = host->register_service(scope, PG_NAME(PG_NATIVE_FIXTURE_SERVICE), 1,
                              &i->service, i);
  if (st)
    return st;
  return c->fail_after_registration ? PG_FAILED : PG_OK;
}
static const pg_plugin plugin = {
    PANGU_SERVICES_ABI,
    sizeof(pg_plugin),
    {"example.native", sizeof("example.native") - 1},
    NULL,
    0,
    apply};
static const pg_plugin *factory(const pg_plugin_host_api *api) {
  if (!api || api->abi != PG_DYNAMIC_ABI || api->size < sizeof(*api))
    return NULL;
  host = api;
  return &plugin;
}
PG_PLUGIN_EXPORT const pg_dynamic_descriptor pangu_plugin_descriptor_v1 = {
    PG_DYNAMIC_ABI, sizeof(pg_dynamic_descriptor), factory};
