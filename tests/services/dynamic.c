#include "../../examples/services/native-plugin/fixture.h"
#include <pangu/services/dynamic.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %d: %s\n", __LINE__, #x);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static int equals(pangu_slice a, pangu_slice b) {
  return a.len == b.len && (!a.len || !memcmp(a.data, b.data, a.len));
}
static pangu_status model(void *ctx, pangu_core *core, pangu_model_sink sink,
                          void *out) {
  int *step = ctx;
  pangu_content b;
  pangu_message m = {
      .role = PANGU_ASSISTANT, .content = &b, .content_count = 1};
  if ((*step)++ == 0) {
    b = (pangu_content){.kind = PANGU_TOOL_CALL,
                        .id = SL("external-call"),
                        .name = SL("native_echo"),
                        .data = SL("{}")};
    m.stop_reason = PANGU_TOOL_USE;
  } else {
    const pangu_message *last =
        pangu_core_message_at(core, pangu_core_message_count(core) - 1);
    CHECK(last->role == PANGU_TOOL_RESULT && !last->is_error);
    CHECK(equals(last->content[0].data, SL("external native result")));
    b = (pangu_content){.kind = PANGU_TEXT,
                        .data = SL("external consumer complete")};
    m.stop_reason = PANGU_STOP;
  }
  return sink(out, &m, 1);
}
static void run_core(const pg_tools_service *tools) {
  pangu_core *c;
  CHECK(pangu_core_create(NULL, &c) == PANGU_OK);
  int step = 0;
  pangu_loop_config cfg = {.ctx = &step,
                           .model = model,
                           .tools = tools->tools,
                           .tool_count = tools->count};
  pangu_content b = {.kind = PANGU_TEXT, .data = SL("invoke external")};
  pangu_message m = {.role = PANGU_USER, .content = &b, .content_count = 1};
  CHECK(pangu_core_run(c, &m, 1, &cfg) == PANGU_OK);
  CHECK(step == 2);
  pangu_core_destroy(c);
}
typedef struct {
  pg_lease *lease;
  uv_sem_t started;
  int *cleaned;
} pending;
static pangu_status capture(void *ctx, const pangu_message *m, int final) {
  (void)ctx;
  CHECK(final);
  CHECK(equals(m->content[0].data, SL("external native result")));
  return PANGU_OK;
}
static void use_lease(void *ctx) {
  pending *p = ctx;
  uv_sem_post(&p->started);
  uv_sleep(60);
  CHECK(*p->cleaned == 0);
  const pg_tools_service *tools = pg_lease_table(p->lease);
  pangu_content call = {
      .kind = PANGU_TOOL_CALL, .name = SL("native_echo"), .data = SL("{}")};
  CHECK(tools->tools[0].execute(tools->tools[0].ctx, NULL, &call, capture,
                                NULL) == PANGU_OK);
  pg_lease_release(p->lease);
}
static pg_status static_apply(pg_scope *s, const void *cfg, size_t n) {
  if (!cfg || n != sizeof(pg_tools_service))
    return PG_INVALID;
  return pg_service_register(s, PG_NAME(PG_NATIVE_FIXTURE_SERVICE), 1, cfg,
                             NULL);
}
int main(int argc, char **argv) {
  CHECK(argc == 6);
  pg_host *h;
  CHECK(pg_host_create(&h) == PG_OK);
  pg_scope *scope = NULL;
  pg_dynamic_diagnostic diagnostic;
  int cleaned = 0;
  pg_native_fixture_config config = {pg_schema_validate, NULL, &cleaned, 0};
  pg_name empty = {0}, path = {argv[1], strlen(argv[1])};
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty, path, &config,
                               sizeof(config), &scope, &diagnostic) == PG_OK);
  CHECK(scope && diagnostic.stage == PG_DYNAMIC_NONE);
  pg_lease *lease;
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_NATIVE_FIXTURE_SERVICE),
                           1, &lease) == PG_OK);
  run_core(pg_lease_table(lease));
  pending p = {.lease = lease, .cleaned = &cleaned};
  CHECK(uv_sem_init(&p.started, 0) == 0);
  uv_thread_t thread;
  CHECK(uv_thread_create(&thread, use_lease, &p) == 0);
  uv_sem_wait(&p.started);
  CHECK(pg_scope_dispose(scope) == PG_OK);
  CHECK(cleaned == 1);
  CHECK(uv_thread_join(&thread) == 0);
  uv_sem_destroy(&p.started);
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_NATIVE_FIXTURE_SERVICE),
                           1, &lease) == PG_NOT_FOUND);
  CHECK(pg_scope_dispose(scope) == PG_OK); /* idempotent, no double dlclose */
  /* Failed dynamic replacement restores the static provider and rolls back. */
  pg_tools_service base = {0};
  pg_plugin static_plugin = {1, sizeof(pg_plugin), PG_NAME("static.base"), NULL,
                             0, static_apply};
  pg_scope *base_scope;
  CHECK(pg_plugin_install(pg_host_root(h), &static_plugin, &base, sizeof(base),
                          &base_scope) == PG_OK);
  config.fail_after_registration = 1;
  scope = (pg_scope *)1;
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty, path, &config,
                               sizeof(config), &scope,
                               &diagnostic) == PG_FAILED);
  CHECK(!scope && diagnostic.stage == PG_DYNAMIC_INSTALL && cleaned == 2);
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_NATIVE_FIXTURE_SERVICE),
                           1, &lease) == PG_OK);
  CHECK(pg_lease_table(lease) == &base);
  pg_lease_release(lease);
  config.fail_after_registration = 0;
  for (int i = 2; i < 5; ++i) {
    scope = (pg_scope *)1;
    pg_name bad = {argv[i], strlen(argv[i])};
    pg_status st = pg_dynamic_plugin_load(pg_host_root(h), empty, bad, &config,
                                          sizeof(config), &scope, &diagnostic);
    CHECK(st == (i == 2 ? PG_NOT_FOUND : i == 3 ? PG_ABI : PG_NOT_FOUND));
    CHECK(!scope);
    if (i == 3) {
      /* The ABI3 fixture aborts if its factory runs. No scope effects may occur. */
      CHECK(cleaned == 2);
      CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_NATIVE_FIXTURE_SERVICE),
                               1, &lease) == PG_OK);
      CHECK(pg_lease_table(lease) == &base);
      pg_lease_release(lease);
    }
    CHECK(diagnostic.stage == (i == 2   ? PG_DYNAMIC_SYMBOL_ERROR
                               : i == 3 ? PG_DYNAMIC_DESCRIPTOR
                                        : PG_DYNAMIC_INSTALL));
  }
  pg_name malformed = {argv[5], strlen(argv[5])};
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty, malformed, NULL, 0,
                               &scope, &diagnostic) == PG_FAILED);
  CHECK(!scope && diagnostic.stage == PG_DYNAMIC_OPEN);
  char *directory = malloc(path.size + 1);
  CHECK(directory);
  memcpy(directory, path.data, path.size);
  directory[path.size] = 0;
  char *separator = strrchr(directory, '/');
#ifdef _WIN32
  char *backslash = strrchr(directory, '\\');
  if (backslash && (!separator || backslash > separator))
    separator = backslash;
#endif
  CHECK(separator);
  *separator = 0;
  pg_name cwd = {directory, strlen(directory)},
          relative_module = {separator + 1, strlen(separator + 1)};
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), cwd, relative_module, &config,
                               sizeof(config), &scope, &diagnostic) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_NATIVE_FIXTURE_SERVICE),
                           1, &lease) == PG_OK);
  run_core(pg_lease_table(lease));
  pg_lease_release(lease);
  CHECK(pg_scope_dispose(scope) == PG_OK);
  CHECK(cleaned == 3);
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_NATIVE_FIXTURE_SERVICE),
                           1, &lease) == PG_OK);
  CHECK(pg_lease_table(lease) == &base);
  pg_lease_release(lease);
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), cwd, PG_NAME("absent-plugin"),
                               NULL, 0, &scope, &diagnostic) == PG_NOT_FOUND);
  CHECK(!scope && diagnostic.stage == PG_DYNAMIC_PATH);
  free(directory);
  char relative[] = "absent-plugin";
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty,
                               (pg_name){relative, sizeof(relative) - 1}, NULL,
                               0, &scope, &diagnostic) == PG_INVALID);
  CHECK(diagnostic.stage == PG_DYNAMIC_PATH);
  char invalid[] = {'a', 0, 'b'};
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty, (pg_name){invalid, 3},
                               NULL, 0, &scope, &diagnostic) == PG_INVALID);
  /* Repeated installs/unloads reuse one shared module and independent scopes.
   */
  for (int i = 0; i < 3; ++i) {
    CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty, path, &config,
                                 sizeof(config), &scope, &diagnostic) == PG_OK);
    CHECK(pg_service_acquire(pg_host_root(h),
                             PG_NAME(PG_NATIVE_FIXTURE_SERVICE), 1,
                             &lease) == PG_OK);
    run_core(pg_lease_table(lease));
    pg_lease_release(lease);
    CHECK(pg_scope_dispose(scope) == PG_OK);
  }
  CHECK(cleaned == 6);
  /* Host destruction also closes multiple independent references after cleanup.
   */
  pg_scope *other;
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty, path, &config,
                               sizeof(config), &scope, &diagnostic) == PG_OK);
  CHECK(pg_dynamic_plugin_load(pg_host_root(h), empty, path, &config,
                               sizeof(config), &other, &diagnostic) == PG_OK);
  pg_host_destroy(h);
  CHECK(cleaned == 8);
  puts("dynamic: external Core tool, lease barrier, unload, rollback/static "
       "restore, ABI3 rejected before factory, symbol/dependency/path diagnostics passed");
  return 0;
}
