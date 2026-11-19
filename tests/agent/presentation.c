#include "slices.h"
#include <assert.h>
#include <pangu/agent/app.h>
#include <pangu/services/model.h>
#include <pangu/services/resources.h>
#include <pangu/tui/plugin.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define S(x) ((pangu_slice){x, sizeof(x) - 1})
static struct {
  pg_agent_app *app;
  uv_thread_t run_thread;
  pg_tui_actions actions;
  void *ctx;
  pangu_loop_config loop;
  const char *user, *project;
  int phase, fail_start, fail_run, fail_install, starts, stops, cleanups, calls,
      polls;
  int provider_context, caller_context, in_poll, summaries, cancel_once,
      cancelled;
} state;
static const pg_plugin view_plugin;
static pangu_message input(pangu_content *content, pangu_slice text) {
  *content = (pangu_content){.kind = PANGU_TEXT, .data = text};
  return (pangu_message){
      .role = PANGU_USER, .content = content, .content_count = 1};
}
static void write_path(const char *dir, const char *suffix, const char *text) {
  size_t n = strlen(dir) + strlen(suffix) + 2;
  char *path = malloc(n);
  assert(path);
  snprintf(path, n, "%s/%s", dir, suffix);
  FILE *f = fopen(path, "wb");
  assert(f);
  assert(fwrite(text, 1, strlen(text), f) == strlen(text));
  assert(!fclose(f));
  free(path);
}
static void reject_reentry(void) {
  pg_tui_config cfg = {0};
  pg_agent_app_close(state.app); /* Must remain alive. */
  assert(pg_agent_app_scope(state.app));
  assert(pg_agent_app_present(state.app, &view_plugin, &cfg, sizeof(cfg)) ==
         PG_FAILED);
  assert(pg_agent_app_run(state.app, NULL, 0, &state.loop) == PG_SESSION_BUSY);
  assert(pg_agent_app_compact(state.app, 1, &state.loop) == PG_SESSION_BUSY);
}
static void lifecycle_guard(void) {
  reject_reentry();
  assert(state.actions.run(state.ctx, NULL, 0, &state.loop) == PG_SESSION_BUSY);
  assert(state.actions.reload_resources(state.ctx) == PG_FAILED);
}
static pangu_tui_status start(void *ctx) {
  assert(ctx == &state);
  state.starts++;
  lifecycle_guard();
  return state.fail_start ? PANGU_TUI_IO : PANGU_TUI_OK;
}
static void stop(void *ctx) {
  assert(ctx == &state);
  state.stops++;
  lifecycle_guard();
}
static void cleanup(void *ctx) {
  assert(ctx == &state);
  state.cleanups++;
  lifecycle_guard();
}
static pangu_tui_status action_history_item(void *, const char *, size_t);
static void poll(void *ctx) {
  assert(ctx == &state);
  state.polls++;
  if (state.in_poll || state.calls != 0)
    return;
  state.in_poll = 1;
  write_path(
      state.user, "auth.json",
      "{\"deepseek\":{\"type\":\"api_key\",\"key\":\"synthetic-ui-next\"}}");
  assert(state.actions.reload_resources(state.ctx) == PG_FAILED);
  assert(state.actions.run(state.ctx, NULL, 0, &state.loop) == PG_SESSION_BUSY);
  pangu_content c;
  pangu_message m = input(&c, S("/hello 'queued $1'"));
  assert(state.actions.enqueue(state.ctx, PANGU_STEERING, &m) == PG_SESSION_OK);
  m = input(&c, S("/skill:example busy argument"));
  assert(state.actions.enqueue(state.ctx, PANGU_FOLLOW_UP, &m) ==
         PG_SESSION_OK);
  size_t committed_raw = 0;
  assert(state.actions.input_history(state.ctx, action_history_item, &committed_raw) == PANGU_TUI_OK);
  assert(committed_raw == 1); /* Both accepted inputs are still pending. */
  /* Disk changes do not affect accepted busy input or later turns in this run.
   */
  write_path(state.user, "prompts/hello.md", "after $1");
  state.in_poll = 0;
}
static pangu_status model(void *ctx, pangu_core *owner,
                          const pangu_core *request,
                          pangu_model_event_sink sink, void *out) {
  assert(ctx == &state.provider_context);
  uv_thread_t current_thread = uv_thread_self();
  assert(uv_thread_equal(&current_thread, &state.run_thread));
  assert(pg_agent_app_reload_resources(state.app) == PG_FAILED);
  reject_reentry();
  const pangu_model_config *m = pangu_core_active_model(owner);
  assert(m);
  const pangu_message *system = pangu_core_message_at(request, 0);
  assert(system && system->role == PANGU_SYSTEM);
  int summary = slice_contains(system->content[0].data,
                               "context summarization assistant");
  if (!summary)
    assert(slice_contains(system->content[0].data,
                          state.phase ? "reloaded-system" : "project-system"));
  assert(
      slice_contains(m->id, state.phase ? "reloaded-model" : "project-model"));
  if (state.cancel_once) {
    state.cancel_once = 0;
    state.cancelled++;
    pg_session_cancel(pg_agent_app_session(state.app));
    return PANGU_CANCELLED;
  }
  const pangu_message *last =
      pangu_core_message_at(request, pangu_core_message_count(request) - 1);
  assert(last && last->role == PANGU_USER);
  if (summary)
    state.summaries++;
  else if (state.calls == 0)
    assert(slice_contains(last->content[0].data, "before first"));
  else if (state.calls == 1)
    assert(slice_contains(last->content[0].data, "before queued $1"));
  else if (state.calls == 2) {
    assert(slice_contains(last->content[0].data, "<skill name=\"example\""));
    assert(slice_contains(last->content[0].data, "busy argument"));
  } else
    assert(slice_contains(last->content[0].data, "after next"));
  assert(slice_contains(pangu_core_active_api_key(owner),
                        state.calls == 0 ? "synthetic-ui-first"
                                         : "synthetic-ui-next"));
  pg_agent_app_poll(state.app, owner);
  if (!summary)
    state.calls++;
  pangu_content text = {.kind = PANGU_TEXT, .data = S("presentation reply")};
  pangu_message answer = {.role = PANGU_ASSISTANT,
                          .content = &text,
                          .content_count = 1,
                          .stop_reason = PANGU_STOP};
  pangu_assistant_event start_event = {.kind = PANGU_ASSISTANT_START};
  pangu_status z = sink(out, &answer, &start_event);
  if (z != PANGU_OK)
    return z;
  pangu_assistant_event done = {.kind = PANGU_ASSISTANT_DONE,
                                .reason = PANGU_STOP};
  return sink(out, &answer, &done);
}
static pg_status install_model(pg_scope *scope, const void *config,
                               size_t size) {
  (void)config;
  (void)size;
  static const pg_model_service api = {model};
  return pg_service_register(scope, PG_NAME(PG_MODEL_SERVICE), 1, &api,
                             &state.provider_context);
}
static pangu_tui_status action_history_item(void *ctx, const char *text, size_t len) {
  size_t *count = ctx;
  static const char *expected[] = {"/hello first", "/hello 'queued $1'", "/skill:example busy argument"};
  assert(*count < 3);
  assert(len == strlen(expected[*count]) && !memcmp(text, expected[*count], len));
  (*count)++;
  return PANGU_TUI_OK;
}
static pangu_tui_status run(void *ctx) {
  state.run_thread = uv_thread_self();
  assert(ctx == &state);
  reject_reentry();
  if (state.fail_run)
    return PANGU_TUI_IO;
  pangu_content c;
  pangu_message m = input(&c, S("/hello first"));
  assert(state.actions.run(state.ctx, &m, 1, &state.loop) == PG_SESSION_OK);
  assert(state.calls == 3);
  size_t raw_count = 0;
  assert(state.actions.input_history(state.ctx, action_history_item, &raw_count) == PANGU_TUI_OK);
  assert(raw_count == 3);
  assert(pg_session_queue_count(pg_agent_app_session(state.app),
                                PANGU_STEERING) == 0);
  assert(pg_session_queue_count(pg_agent_app_session(state.app),
                                PANGU_FOLLOW_UP) == 0);
  write_path(state.project, ".pangu/settings.json", "invalid");
  assert(state.actions.reload_resources(state.ctx) == PG_INVALID);
  write_path(state.project, ".pangu/settings.json",
             "{\"defaultModel\":\"reloaded-model\"}");
  write_path(state.project, ".pangu/SYSTEM.md", "reloaded-system");
  assert(state.actions.reload_resources(state.ctx) == PG_OK);
  state.phase = 1;
  m = input(&c, S("/hello next"));
  assert(state.actions.run(state.ctx, &m, 1, &state.loop) == PG_SESSION_OK);
  assert(state.actions.compact(state.ctx, 1, &state.loop) == PG_SESSION_OK);
  assert(state.summaries == 1);
  state.cancel_once = 1;
  m = input(&c, S("/hello cancelled"));
  assert(state.actions.run(state.ctx, &m, 1, &state.loop) ==
         PG_SESSION_CANCELLED);
  assert(state.actions.reload_resources(state.ctx) == PG_OK);
  m = input(&c, S("/hello next"));
  assert(state.actions.run(state.ctx, &m, 1, &state.loop) == PG_SESSION_OK);
  assert(state.cancelled == 1);
  return PANGU_TUI_OK;
}
static pg_status install_view(pg_scope *scope, const void *config,
                              size_t size) {
  assert(size == sizeof(pg_tui_config));
  const pg_tui_config *cfg = config;
  assert(cfg->actions && cfg->actions->size == sizeof(pg_tui_actions));
  state.actions = *cfg->actions;
  state.ctx = cfg->actions_ctx;
  state.loop = cfg->loop;
  lifecycle_guard();
  if (state.fail_install)
    return PG_FAILED;
  assert(pg_scope_defer(scope, cleanup, &state) == PG_OK);
  static const pg_tui_service api = {
      PG_TUI_ABI, sizeof(pg_tui_service), start, run, poll, stop, NULL};
  return pg_service_register(scope, PG_NAME(PG_TUI_SERVICE), PG_TUI_ABI, &api,
                             &state);
}
static const pg_plugin view_plugin = {
    1, sizeof(pg_plugin), {"test.presentation", 17}, NULL, 0, install_view};
static const pg_plugin model_plugin = {
    1, sizeof(pg_plugin), {"test.model", 10}, NULL, 0, install_model};
int main(int argc, char **argv) {
  assert(argc == 5);
  state.user = argv[1];
  state.project = argv[2];
  pg_resources_config resources = {
      .user_directory = argv[1], .cwd = argv[2], .project_trusted = 1};
  pg_app_plugin plugins[] = {
      {&pg_resources_plugin, &resources, sizeof(resources)},
      {&model_plugin, NULL, 0}};
  pg_session_options options = {{argv[3], strlen(argv[3])},
                                atoi(argv[4]) ? PG_STORE_SQLITE
                                              : PG_STORE_JSONL,
                                1,
                                0};
  assert(pg_agent_app_open(&options, plugins, 2, &state.app) == PG_OK);
  /* Allocate exactly the old ABI prefix, so ASan catches any tail read. */
  size_t old_size = offsetof(pg_tui_config, actions);
  unsigned char *old = calloc(1, old_size);
  assert(old);
  write_path(
      state.user, "auth.json",
      "{\"deepseek\":{\"type\":\"api_key\",\"key\":\"synthetic-ui-first\"}}");
  pangu_loop_config loop = {.ctx = &state.caller_context};
  memcpy(old, &loop, sizeof(loop));
  assert(pg_agent_app_present(state.app, &view_plugin, old, old_size - 1) ==
         PG_INVALID);
  assert(pg_agent_app_present(state.app, &view_plugin, old, old_size + 1) ==
         PG_INVALID);
  assert(pg_agent_app_present(state.app, &view_plugin, old, old_size) == PG_OK);
  free(old);
  assert(state.calls == 5 && state.starts == 1 && state.stops == 1 &&
         state.cleanups == 1);
  pg_tui_config cfg = {.loop = loop};
  state.fail_start = 1;
  assert(pg_agent_app_present(state.app, &view_plugin, &cfg, sizeof(cfg)) ==
         PG_FAILED);
  assert(state.stops == 2 && state.cleanups == 2);
  state.fail_start = 0;
  state.fail_run = 1;
  assert(pg_agent_app_present(state.app, &view_plugin, &cfg, sizeof(cfg)) ==
         PG_FAILED);
  assert(state.stops == 3 && state.cleanups == 3);
  state.fail_run = 0;
  state.fail_install = 1;
  assert(pg_agent_app_present(state.app, &view_plugin, &cfg, sizeof(cfg)) ==
         PG_FAILED);
  assert(state.stops == 3 && state.cleanups == 3);
  size_t visible = pangu_core_message_count(
      pg_session_messages(pg_agent_app_session(state.app)));
  pg_agent_app_close(state.app);
  options.create = 0;
  assert(pg_agent_app_open(&options, NULL, 0, &state.app) == PG_OK);
  assert(pangu_core_message_count(
             pg_session_messages(pg_agent_app_session(state.app))) == visible);
  pg_agent_app_close(state.app);
  puts("presentation: legacy prefix, per-run snapshots, busy queues, idle "
       "reload, ctx and lifecycle passed");
}
