#include <pangu/core/loop.h>
#include <pangu/services/resources.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <yyjson.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %d: %s\n", __LINE__, #x);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
/* Public slices may end at an allocation boundary without a trailing NUL. */
static int slice_equal(pangu_slice a, pangu_slice b) {
  return a.len == b.len && (!a.len || !memcmp(a.data, b.data, a.len));
}
static int slice_contains(pangu_slice text, pangu_slice needle) {
  if (needle.len > text.len)
    return 0;
  if (!needle.len)
    return 1;
  for (size_t i = 0; i <= text.len - needle.len; ++i)
    if (!memcmp(text.data + i, needle.data, needle.len))
      return 1;
  return 0;
}
static int json_string_equal(yyjson_val *value, pangu_slice expected) {
  return yyjson_is_str(value) &&
         slice_equal(
             (pangu_slice){yyjson_get_str(value), yyjson_get_len(value)},
             expected);
}
static pg_resource_snapshot *current;
static int turns;
static pangu_status model(void *ctx, pangu_core *c, pangu_model_sink sink,
                          void *out) {
  (void)ctx;
  const pangu_message *system = pangu_core_message_at(c, 0);
  CHECK(system && system->role == PANGU_SYSTEM);
  CHECK(system->content[0].data.len == pg_resource_system_prompt(current).len);
  CHECK(!memcmp(system->content[0].data.data,
                pg_resource_system_prompt(current).data,
                system->content[0].data.len));
  CHECK(slice_contains(system->content[0].data, SL("project marker")));
  /* Negative search must stop at Core's slice boundary, even without NUL. */
  CHECK(
      !slice_contains(system->content[0].data, SL("absent-Core-slice-marker")));
  yyjson_doc *d = yyjson_read(pg_resource_settings(current).data,
                              pg_resource_settings(current).len, 0);
  CHECK(d);
  CHECK(
      json_string_equal(yyjson_obj_get(yyjson_doc_get_root(d), "defaultModel"),
                        SL("runtime-model")));
  const pangu_model_config *active = pangu_core_active_model(c);
  CHECK(active);
  CHECK(slice_equal(active->id, SL("runtime-model")));
  const pangu_client_identity *identity = pangu_core_active_identity(c);
  CHECK(identity && slice_equal(identity->name, SL("PanGu")));
  yyjson_doc_free(d);
  ++turns;
  pangu_content b = {.kind = PANGU_TEXT, .data = SL("resources consumed")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &b,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  return sink(out, &m, 1);
}
static void write_file(const char *path, const char *text) {
  FILE *f = fopen(path, "w");
  CHECK(f);
  CHECK(fwrite(text, 1, strlen(text), f) == strlen(text));
  CHECK(!fclose(f));
}
static void path_write(const char *base, const char *suffix, const char *text) {
  char path[4096];
  snprintf(path, sizeof(path), "%s/%s", base, suffix);
  write_file(path, text);
}
int main(int argc, char **argv) {
  CHECK(argc == 3 || argc == 4 || argc == 5);
  pg_host *host;
  pg_scope *scope;
  pg_lease *lease;
  CHECK(pg_host_create(&host) == PG_OK);
  pg_resources_config config = {.user_directory = argv[1],
                                .cwd = argv[2],
                                .overrides_json =
                                    SL("{\"defaultModel\":\"runtime-model\"}"),
                                .project_trusted = 1};
  CHECK(pg_plugin_install(pg_host_root(host), &pg_resources_plugin, &config,
                          sizeof(config), &scope) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_RESOURCES_SERVICE), 1,
                           &lease) == PG_OK);
  const pg_resources_service *service = pg_lease_table(lease);
  void *ctx = pg_lease_context(lease);
  if (argc == 4) {
    pangu_slice patch = {argv[3], strlen(argv[3])};
    CHECK(service->patch_settings(ctx, 0, patch) == PG_OK);
    pg_lease_release(lease);
    pg_host_destroy(host);
    return 0;
  }
  CHECK(service->snapshot(ctx, &current) == PG_OK);
  if (argc == 5) {
    CHECK(!strcmp(argv[3], "contexts"));
    yyjson_doc *expected = yyjson_read(argv[4], strlen(argv[4]), 0);
    CHECK(expected);
    yyjson_val *array = yyjson_doc_get_root(expected);
    CHECK(yyjson_is_arr(array));
    size_t at = 0;
    for (size_t i = 0; i < pg_resource_count(current); ++i) {
      const pg_resource *r = pg_resource_at(current, i);
      if (r->kind != PG_RESOURCE_CONTEXT)
        continue;
      CHECK(at < yyjson_arr_size(array));
      CHECK(json_string_equal(yyjson_arr_get(array, at++), r->body));
    }
    CHECK(at == yyjson_arr_size(array));
    yyjson_doc_free(expected);
    pg_resource_snapshot_release(current);
    pg_lease_release(lease);
    pg_host_destroy(host);
    return 0;
  }
  int found_skill = 0;
  for (size_t i = 0; i < pg_resource_count(current); ++i) {
    const pg_resource *resource = pg_resource_at(current, i);
    CHECK(resource);
    if (resource->kind == PG_RESOURCE_SKILL &&
        slice_equal(resource->name, SL("example"))) {
      CHECK(slice_contains(resource->description, SL("Example skill")));
      CHECK(slice_equal(resource->body, SL("Do something")));
      found_skill = 1;
    }
  }
  CHECK(found_skill);
  int kept = 0, nested_kept = 0;
  for (size_t i = 0; i < pg_resource_count(current); ++i) {
    const pg_resource *r = pg_resource_at(current, i);
    if (r->kind != PG_RESOURCE_SKILL)
      continue;
    CHECK(!slice_equal(r->name, SL("blocked")));
    CHECK(!slice_equal(r->name, SL("ignored")));
    CHECK(!slice_equal(r->name, SL("FD-hidden")));
    CHECK(!slice_equal(r->name, SL("nested-drop")));
    kept |= slice_equal(r->name, SL("keep"));
    nested_kept |= slice_equal(r->name, SL("nested-take"));
  }
  CHECK(kept && nested_kept);
  CHECK(slice_contains(pg_resource_diagnostics(current),
                       SL("skill description")));
  CHECK(slice_contains(pg_resource_diagnostics(current),
                       SL("duplicate resource")));
  pangu_slice settings = pg_resource_settings(current);
  yyjson_doc *d = yyjson_read(settings.data, settings.len, 0);
  CHECK(d);
  yyjson_val *nested = yyjson_obj_get(yyjson_doc_get_root(d), "nested");
  CHECK(yyjson_get_int(yyjson_obj_get(nested, "keep")) == 1);
  CHECK(yyjson_get_int(yyjson_obj_get(nested, "override")) == 9);
  yyjson_val *tool_names =
      yyjson_obj_get(yyjson_doc_get_root(d), "defaultTools");
  CHECK(yyjson_arr_size(tool_names) == 2);
  CHECK(json_string_equal(yyjson_arr_get(tool_names, 0), SL("read")));
  CHECK(json_string_equal(yyjson_arr_get(tool_names, 1), SL("grep")));
  yyjson_doc_free(d);
  CHECK(
      slice_contains(pg_resource_system_prompt(current), SL("project marker")));
  CHECK(slice_contains(pg_resource_system_prompt(current), SL("user context")));
  CHECK(slice_contains(pg_resource_system_prompt(current),
                       SL("ancestor context")));
  char *expanded;
  size_t len;
  CHECK(pg_resource_expand(current, SL("/hello 'one two' \"\" '$1' four"),
                           &expanded, &len) == PG_OK);
  CHECK(
      slice_equal((pangu_slice){expanded, len},
                  SL("one two|fallback|$1|one two  $1 four|$1 four|one two ")));
  free(expanded);
  CHECK(pg_resource_expand(current, SL("/unknown untouched"), &expanded,
                           &len) == PG_OK);
  CHECK(slice_equal((pangu_slice){expanded, len}, SL("/unknown untouched")));
  free(expanded);
  pangu_core *core;
  CHECK(pangu_core_create(NULL, &core) == PANGU_OK);
  pangu_content b[2] = {
      {.kind = PANGU_TEXT, .data = pg_resource_system_prompt(current)},
      {.kind = PANGU_TEXT, .data = SL("hello")}};
  pangu_message m[2] = {
      {.role = PANGU_SYSTEM, .content = b, .content_count = 1},
      {.role = PANGU_USER, .content = b + 1, .content_count = 1}};
  settings = pg_resource_settings(current);
  yyjson_doc *selection = yyjson_read(settings.data, settings.len, 0);
  CHECK(selection);
  yyjson_val *selected_model =
      yyjson_obj_get(yyjson_doc_get_root(selection), "defaultModel");
  pangu_loop_config loop = {
      .model = model,
      .model_config = {.id = {yyjson_get_str(selected_model),
                              yyjson_get_len(selected_model)}}};
  CHECK(pangu_core_run(core, m, 2, &loop) == PANGU_OK);
  CHECK(turns == 1);
  yyjson_doc_free(selection);
  pangu_core_destroy(core);
  /* A reload is transactional; held snapshots retain the old bytes. */
  path_write(argv[2], ".pangu/settings.json", "broken");
  CHECK(service->reload(ctx) == PG_INVALID);
  pg_resource_snapshot *same;
  CHECK(service->snapshot(ctx, &same) == PG_OK);
  CHECK(same == current);
  pg_resource_snapshot_release(same);
  path_write(argv[2], ".pangu/settings.json",
             "{\"nested\":{\"override\":10},\"external\":true}");
  CHECK(service->patch_settings(ctx, 1, SL("{\"nested\":{\"patch\":2}}")) ==
        PG_OK);
  CHECK(service->snapshot(ctx, &same) == PG_OK);
  settings = pg_resource_settings(same);
  d = yyjson_read(settings.data, settings.len, 0);
  CHECK(d);
  CHECK(yyjson_get_bool(yyjson_obj_get(yyjson_doc_get_root(d), "external")));
  nested = yyjson_obj_get(yyjson_doc_get_root(d), "nested");
  CHECK(yyjson_get_int(yyjson_obj_get(nested, "override")) == 10);
  CHECK(yyjson_get_int(yyjson_obj_get(nested, "patch")) == 2);
  yyjson_doc_free(d);
  pg_resource_snapshot_release(same);
  CHECK(service->set_project_trusted(ctx, 0) == PG_OK);
  CHECK(service->snapshot(ctx, &same) == PG_OK);
  CHECK(!slice_contains(pg_resource_system_prompt(same), SL("project marker")));
  CHECK(slice_contains(pg_resource_system_prompt(same), SL("user marker")));
  pg_resource_snapshot_release(same);
  CHECK(service->patch_settings(ctx, 1, SL("{}")) == PG_CLOSED);
  CHECK(service->set_project_trusted(ctx, 1) == PG_OK);
  path_write(argv[2], ".pangu/prompts/new.md", "new template");
  CHECK(service->reload(ctx) == PG_OK);
  CHECK(service->snapshot(ctx, &same) == PG_OK);
  CHECK(pg_resource_expand(same, SL("/new"), &expanded, &len) == PG_OK);
  CHECK(slice_equal((pangu_slice){expanded, len}, SL("new template")));
  free(expanded);
  pg_resource_snapshot_release(same);
  pg_lease *credential_lease;
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_CREDENTIALS_SERVICE),
                           1, &credential_lease) == PG_OK);
  const pg_credentials_service *credentials = pg_lease_table(credential_lease);
  void *credential_ctx = pg_lease_context(credential_lease);
  char *key = NULL;
  size_t key_len = 0;
  CHECK(credentials->put_api_key(credential_ctx, SL("test-provider"),
                                 SL("fixture-key-one")) == PG_OK);
  CHECK(credentials->get_api_key(credential_ctx, SL("test-provider"), &key,
                                 &key_len) == PG_OK);
  CHECK(key_len == 15 && !memcmp(key, "fixture-key-one", 15));
  free(key);
  CHECK(credentials->put_api_key(credential_ctx, SL("test-provider"),
                                 SL("fixture-key-two")) == PG_OK);
  CHECK(credentials->get_api_key(credential_ctx, SL("test-provider"), &key,
                                 &key_len) == PG_OK);
  CHECK(key_len == 15 && !memcmp(key, "fixture-key-two", 15));
  free(key);
  CHECK(credentials->put_api_key(credential_ctx, SL("test-provider"),
                                 SL("!command")) == PG_INVALID);
  CHECK(credentials->put_api_key(credential_ctx, SL("test-provider"),
                                 (pangu_slice){0}) == PG_OK);
  CHECK(uv_os_setenv("TEST_PROVIDER_API_KEY", "fixture-env-key") == 0);
  CHECK(credentials->get_api_key(credential_ctx, SL("test-provider"), &key,
                                 &key_len) == PG_OK);
  CHECK(slice_equal((pangu_slice){key, key_len}, SL("fixture-env-key")));
  free(key);
  uv_os_unsetenv("TEST_PROVIDER_API_KEY");
  CHECK(credentials->put_api_key(credential_ctx, SL("test-provider"),
                                 SL("prefix-${ROTATING_KEY}-$$-$!")) == PG_OK);
  CHECK(uv_os_setenv("ROTATING_KEY", "one") == 0);
  CHECK(credentials->get_api_key(credential_ctx, SL("test-provider"), &key,
                                 &key_len) == PG_OK);
  CHECK(slice_equal((pangu_slice){key, key_len}, SL("prefix-one-$-!")));
  free(key);
  CHECK(uv_os_setenv("ROTATING_KEY", "two") == 0);
  CHECK(credentials->get_api_key(credential_ctx, SL("test-provider"), &key,
                                 &key_len) == PG_OK);
  CHECK(slice_equal((pangu_slice){key, key_len}, SL("prefix-two-$-!")));
  free(key);
  uv_os_unsetenv("ROTATING_KEY");
  CHECK(service->remember_project_trust(ctx, 0) == PG_OK);
  pg_lease_release(credential_lease);
  pg_lease_release(lease);
  pg_host_destroy(host);
  CHECK(pg_host_create(&host) == PG_OK);
  config.project_trusted = -1;
  CHECK(pg_plugin_install(pg_host_root(host), &pg_resources_plugin, &config,
                          sizeof(config), &scope) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_RESOURCES_SERVICE), 1,
                           &lease) == PG_OK);
  service = pg_lease_table(lease);
  ctx = pg_lease_context(lease);
  CHECK(service->snapshot(ctx, &same) == PG_OK);
  CHECK(!slice_contains(pg_resource_system_prompt(same), SL("project marker")));
  pg_resource_snapshot_release(same);
  CHECK(service->remember_project_trust(ctx, 1) == PG_OK);
  pg_lease_release(lease);
  pg_host_destroy(host);
  CHECK(
      slice_contains(pg_resource_system_prompt(current), SL("project marker")));
  pg_resource_snapshot_release(current);
  puts("resources: layered settings, Core prompt consumer, quoted template "
       "args, atomic patch, trust, transactional reload, snapshot unload "
       "lifetime passed");
}
