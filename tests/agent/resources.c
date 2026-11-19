#include "slices.h"
#include <assert.h>
#include <pangu/agent/app.h>
#include <pangu/services/model.h>
#include <pangu/services/resources.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
typedef struct {
  pg_agent_app *app;
  int calls, hooks, summaries, check_history;
  const char *expected_model, *expected_system, *expected_input;
} state;
static state caller;
static int provider_context;
static const char *expected_key = "synthetic-key";
static void observe(void *ctx, pangu_core *owner, const pangu_event *event) {
  (void)owner;
  (void)event;
  assert(ctx == &caller);
}
static pangu_status key(void *ctx, pangu_core *owner, pangu_slice provider,
                        pangu_api_key_sink sink, void *out) {
  (void)owner;
  assert(ctx == &caller && provider.len == 8 &&
         !memcmp(provider.data, "deepseek", 8));
  caller.hooks++;
  if (caller.hooks < 0)
    return PANGU_OK;
  return sink(out, SL("synthetic-key"));
}
static int eq(pangu_slice s, const char *text) {
  return s.len == strlen(text) && !memcmp(s.data, text, s.len);
}
static pangu_status provider(void *ctx, pangu_core *owner,
                             const pangu_core *request,
                             pangu_model_event_sink sink, void *out) {
  assert(ctx == &provider_context);
  state *s = &caller;
  assert(pg_agent_app_reload_resources(s->app) == PG_FAILED);
  const pangu_model_config *m = pangu_core_active_model(owner);
  assert(m && eq(m->id, s->expected_model));
  assert(pangu_core_active_thinking(owner) == PANGU_THINKING_HIGH);
  assert(eq(pangu_core_active_api_key(owner), expected_key));
  const pangu_message *system = pangu_core_message_at(request, 0);
  assert(system && system->role == PANGU_SYSTEM);
  int summary = slice_contains(system->content[0].data,
                               "You are a context summarization assistant");
  if (summary)
    s->summaries++;
  else
    assert(slice_contains(system->content[0].data, s->expected_system));
  const pangu_message *last =
      pangu_core_message_at(request, pangu_core_message_count(request) - 1);
  if (!summary) {
    assert(last->role == PANGU_USER &&
           eq(last->content[0].data, s->expected_input));
    if (s->check_history)
      assert(pangu_core_message_count(pg_session_messages(
                 pg_agent_app_session(s->app))) == (size_t)(s->calls * 2 + 1));
    s->calls++;
  }
  pangu_content text = {.kind = PANGU_TEXT,
                        .data = SL("app resources response")};
  pangu_message answer = {.role = PANGU_ASSISTANT,
                          .content = &text,
                          .content_count = 1,
                          .stop_reason = PANGU_STOP};
  pangu_assistant_event start = {.kind = PANGU_ASSISTANT_START};
  pangu_status z = sink(out, &answer, &start);
  if (z != PANGU_OK)
    return z;
  pangu_assistant_event done = {.kind = PANGU_ASSISTANT_DONE,
                                .reason = PANGU_STOP};
  return sink(out, &answer, &done);
}
static pg_status install(pg_scope *scope, const void *config, size_t size) {
  (void)config;
  (void)size;
  static const pg_model_service service = {provider};
  return pg_service_register(scope, PG_NAME(PG_MODEL_SERVICE), 1, &service,
                             &provider_context);
}
static const pg_plugin model = {
    1, sizeof(pg_plugin), {"test.resources.model", 20}, NULL, 0, install};
static void write_path(const char *dir, const char *name, const char *text) {
  size_t n = strlen(dir) + strlen(name) + 2;
  char *path = malloc(n);
  assert(path);
  snprintf(path, n, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  assert(f);
  assert(fwrite(text, 1, strlen(text), f) == strlen(text));
  assert(!fclose(f));
  free(path);
}
typedef struct isolated isolated;
struct isolated {
  pg_agent_app *app;
  isolated *nested;
  const char *secret;
  int keys, requests, caller_ctx;
};
static pg_status isolated_key(void *ctx, pangu_slice provider, char **out,
                              size_t *length) {
  isolated *s = ctx;
  /* Deliberately non-NUL-terminated provider, passed through unchanged. */
  assert(provider.len == 3 && !memcmp(provider.data, "cut", 3));
  s->keys++;
  *length = strlen(s->secret);
  *out = malloc(*length);
  assert(*out);
  memcpy(*out, s->secret, *length);
  return PG_OK;
}
static pangu_status isolated_request(void *ctx, pangu_core *owner,
                                     const pangu_core *input,
                                     pangu_model_sink sink, void *out);
static void isolated_run(isolated *s) {
  char provider[] = {'c', 'u', 't', 'X'};
  pangu_loop_config cfg = {.ctx = &s->caller_ctx,
                           .request = isolated_request,
                           .has_request_ctx = 1,
                           .request_ctx = s,
                           .model_config = {.provider = {provider, 3}}};
  pangu_content c = {.kind = PANGU_TEXT, .data = SL("isolated")};
  pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
  assert(pg_agent_app_run(s->app, &m, 1, &cfg) == PG_SESSION_OK);
}
static pangu_status isolated_request(void *ctx, pangu_core *owner,
                                     const pangu_core *input,
                                     pangu_model_sink sink, void *out) {
  (void)input;
  isolated *s = ctx;
  assert(eq(pangu_core_active_api_key(owner), s->secret));
  s->requests++;
  if (s->nested && s->requests == 1) {
    isolated_run(s->nested);
    /* A second request in this outer run must restore the outer binding. */
    pangu_content c = {.kind = PANGU_TEXT, .data = SL("outer continuation")};
    pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
    assert(pg_agent_app_enqueue(s->app, PANGU_FOLLOW_UP, &m) == PG_SESSION_OK);
  }
  assert(eq(pangu_core_active_api_key(owner), s->secret));
  pangu_content c = {.kind = PANGU_TEXT, .data = SL("isolated reply")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  return sink(out, &m, 1);
}
static void independent_credentials(const char *path, int sqlite) {
  size_t n = strlen(path) + 16;
  char *outer_path = malloc(n), *inner_path = malloc(n);
  assert(outer_path && inner_path);
  snprintf(outer_path, n, "%s.outer", path);
  snprintf(inner_path, n, "%s.inner", path);
  isolated outer = {.secret = "synthetic-outer"},
           inner = {.secret = "synthetic-inner"};
  outer.nested = &inner;
  pg_session_options opts = {{outer_path, strlen(outer_path)},
                             sqlite ? PG_STORE_SQLITE : PG_STORE_JSONL,
                             1,
                             0};
  assert(pg_agent_app_open(&opts, NULL, 0, &outer.app) == PG_OK);
  opts.path = (pangu_slice){inner_path, strlen(inner_path)};
  assert(pg_agent_app_open(&opts, NULL, 0, &inner.app) == PG_OK);
  static const pg_credentials_service api = {isolated_key, NULL};
  assert(pg_service_register(pg_agent_app_scope(outer.app),
                             PG_NAME(PG_CREDENTIALS_SERVICE), 1, &api,
                             &outer) == PG_OK);
  assert(pg_service_register(pg_agent_app_scope(inner.app),
                             PG_NAME(PG_CREDENTIALS_SERVICE), 1, &api,
                             &inner) == PG_OK);
  isolated_run(&outer);
  assert(outer.keys == 2 && outer.requests == 2);
  assert(inner.keys == 1 && inner.requests == 1);
  pg_agent_app_close(inner.app);
  pg_agent_app_close(outer.app);
  free(inner_path);
  free(outer_path);
}
static pg_session_status raw_item(void *ctx, pangu_slice text) {
  size_t *n = ctx;
  assert(eq(text, "/hello 'one two'"));
  (*n)++;
  return PG_SESSION_OK;
}
int main(int argc, char **argv) {
  assert(argc == 5);
  independent_credentials(argv[3], atoi(argv[4]));
  pg_resources_config resources = {
      .user_directory = argv[1], .cwd = argv[2], .project_trusted = 1};
  pg_app_plugin plugins[] = {
      {&pg_resources_plugin, &resources, sizeof(resources)}, {&model, NULL, 0}};
  pg_session_options options = {{argv[3], strlen(argv[3])},
                                atoi(argv[4]) ? PG_STORE_SQLITE
                                              : PG_STORE_JSONL,
                                1,
                                0};
  assert(pg_agent_app_open(&options, plugins, 2, &caller.app) == PG_OK);
  pangu_loop_config cfg = {
      .ctx = &caller, .get_api_key = key, .event = observe};
  pangu_content content = {.kind = PANGU_TEXT, .data = SL("/hello 'one two'")};
  pangu_message prompt = {
      .role = PANGU_USER, .content = &content, .content_count = 1};
  caller.check_history = 1;
  caller.expected_model = "project-model";
  caller.expected_system = "project-system";
  caller.expected_input = "before one two";
  /* Explicit caller hook must bypass even unreadable/malformed credentials. */
  write_path(argv[1], "auth.json", "invalid synthetic-private-marker");
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  write_path(argv[2], ".pangu/settings.json", "invalid");
  assert(pg_agent_app_reload_resources(caller.app) == PG_INVALID);
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  write_path(argv[2], ".pangu/settings.json",
             "{\"defaultModel\":\"reloaded-model\"}");
  write_path(argv[2], ".pangu/SYSTEM.md", "reloaded-system");
  write_path(argv[1], "prompts/hello.md", "after $1");
  assert(pg_agent_app_reload_resources(caller.app) == PG_OK);
  caller.expected_model = "reloaded-model";
  caller.expected_system = "reloaded-system";
  caller.expected_input = "after one two";
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  assert(caller.calls == 3 && caller.hooks == 3);
  pg_agent_app_close(caller.app);
  options.create = 0;
  assert(pg_agent_app_open(&options, plugins, 2, &caller.app) == PG_OK);
  size_t raw_count = 0;
  assert(pg_session_input_history(pg_agent_app_session(caller.app), raw_item,
                                  &raw_count) == PG_SESSION_OK);
  assert(raw_count == 3);
  const pangu_core *history =
      pg_session_messages(pg_agent_app_session(caller.app));
  assert(pangu_core_message_count(history) == 6);
  assert(
      eq(pangu_core_message_at(history, 4)->content[0].data, "after one two"));
  for (size_t i = 0; i < 6; i++)
    assert(pangu_core_message_at(history, i)->role != PANGU_SYSTEM);
  caller.check_history = 0;
  pg_compaction_policy policy = {1, 2, 1, 1};
  assert(pg_session_set_compaction_policy(pg_agent_app_session(caller.app),
                                          &policy) == PG_SESSION_OK);
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  assert(caller.calls == 4 && caller.summaries == 1 && caller.hooks == 5);
  /* Implicit service refresh, then Core fallback on NOT_FOUND/empty result. */
  cfg.get_api_key = NULL;
  caller.expected_model = "reloaded-model";
  caller.expected_system = "reloaded-system";
  expected_key = "synthetic-file-key";
  write_path(
      argv[1], "auth.json",
      "{\"deepseek\":{\"type\":\"api_key\",\"key\":\"synthetic-file-key\"}}");
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  expected_key = "synthetic-fallback";
  cfg.api_key = SL("synthetic-fallback");
  write_path(argv[1], "auth.json", "{}");
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  write_path(argv[1], "auth.json",
             "{\"deepseek\":{\"type\":\"api_key\",\"key\":\"\"}}");
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  /* Both provider env fallback and auth env literals refresh per request. */
  write_path(argv[1], "auth.json", "{}");
  assert(!uv_os_setenv("DEEPSEEK_API_KEY", "synthetic-env-first"));
  expected_key = "synthetic-env-first";
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  assert(!uv_os_setenv("DEEPSEEK_API_KEY", "synthetic-env-next"));
  expected_key = "synthetic-env-next";
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  write_path(
      argv[1], "auth.json",
      "{\"deepseek\":{\"type\":\"api_key\",\"key\":\"${DEEPSEEK_API_KEY}\"}}");
  assert(!uv_os_setenv("DEEPSEEK_API_KEY", "synthetic-env-literal"));
  expected_key = "synthetic-env-literal";
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  assert(!uv_os_unsetenv("DEEPSEEK_API_KEY"));
  /* Explicit callback returning no key uses caller fallback, not the service.
   */
  cfg.get_api_key = key;
  caller.hooks = -10;
  expected_key = "synthetic-fallback";
  write_path(argv[1], "auth.json", "invalid synthetic-private-marker");
  assert(pg_agent_app_run(caller.app, &prompt, 1, &cfg) == PG_SESSION_OK);
  pg_agent_app_close(caller.app);
  puts("app resource model lease/context, reload rollback, request-only "
       "prompt, persisted expansion passed");
}
