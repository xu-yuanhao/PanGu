#include <assert.h>
#include <pangu/agent/app.h>
#include <pangu/agent/recovery.h>
#include <pangu/agent/tree.h>
#include <pangu/services/model.h>
#include <pangu/services/tools.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define S(s) ((pangu_slice){s, sizeof(s) - 1})
static pangu_session *create(const char *path, pg_session_backend backend) {
  remove(path);
  pg_session_options o = {{path, strlen(path)}, backend, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  return s;
}
static pangu_message user(pangu_content *c) {
  *c = (pangu_content){.kind = PANGU_TEXT, .data = S("input")};
  return (pangu_message){.role = PANGU_USER, .content = c, .content_count = 1};
}
static void rich(void) {
  for (int b = PG_STORE_JSONL; b <= PG_STORE_SQLITE; b++) {
    const char *path = b ? "rich.sqlite" : "rich.jsonl";
    pangu_session *s = create(path, (pg_session_backend)b);
    pangu_content content[] = {
        {.kind = PANGU_THINKING, .data = S("reason"), .signature = S("signed")},
        {.kind = PANGU_IMAGE,
         .data = S("aGVsbG8="),
         .mime_type = S("image/png")},
        {.kind = PANGU_TOOL_CALL,
         .data = S("{\"x\":1}"),
         .id = S("call1"),
         .name = S("read"),
         .tool_namespace = S("fs"),
         .arguments_error = S("diagnostic")},
        {.kind = PANGU_TEXT, .data = {"a\0b", 3}}};
    pangu_system_section section = {S("system"), S("prompt"), 1};
    pangu_tool_declaration tool = {S("read"), S("Read file"),
                                   S("{\"type\":\"object\"}"), 1, 1};
    pangu_slice removed = S("write");
    char response_id[] = {'r', '\0', 's', 'p'};
    pangu_message m = {.role = PANGU_CUSTOM,
                       .custom_role = S("visible"),
                       .content = content,
                       .content_count = 4,
                       .tool_call_id = S("call0"),
                       .tool_name = S("read"),
                       .model = S("model"),
                       .provider = S("provider"),
                       .api = S("api"),
                       .error = S("safe error"),
                       .details_json = S("{\"details\":true}"),
                       .structured_json = S("[1,2]"),
                       .provider_thinking_level = S("adaptive"),
                       .response_model = S("served-variant"),
                       .response_id = {response_id, sizeof(response_id)},
                       .raw_stop_reason = S("provider/custom-stop"),
                       .diagnostics_json = S("[]"),
                       .deferred_json = S("{\"pollAfterMs\":0,\"data\":{\"zero\":0}}"),
                       .nested_calls_json =
                           S("{\"calls\":[{\"name\":\"read\",\"arguments\":{},"
                             "\"durationMs\":0}],\"complete\":false}"),
                       .has_end_turn = 1,
                       .end_turn = 0,
                       .usage = {1, 2, 3, 4, 10, .01, .02, .03, .04, .1},
                       .stop_reason = PANGU_ERROR,
                       .thinking_level = PANGU_THINKING_HIGH,
                       .is_error = 1,
                       .terminate = 1,
                       .timestamp_ms = 1234,
                       .has_duration = 1,
                       .duration_ms = UINT64_MAX,
                       .sections = &section,
                       .section_count = 1,
                       .tools_added = &tool,
                       .tools_added_count = 1,
                       .tools_removed = &removed,
                       .tools_removed_count = 1};
    content[0].has_redacted = 1;
    content[0].redacted = 0;
    content[1].has_redacted = 1;
    content[1].redacted = 1;
    m.usage.has_cache_write_1h = 1;
    m.usage.cache_write_1h = 0;
    m.usage.has_reasoning = 1;
    m.usage.reasoning = 0;
    m.usage.has_total_from_provider = 1;
    m.usage.total_from_provider = 0;
    assert(pg_session_begin(s) == PG_SESSION_OK);
    assert(pg_session_append(s, &m) == PG_SESSION_OK);
    pangu_message absent = m;
    absent.response_model = absent.response_id = absent.raw_stop_reason =
        (pangu_slice){0};
    absent.diagnostics_json = absent.deferred_json = absent.nested_calls_json =
        (pangu_slice){0};
    absent.has_end_turn = 0;
    assert(pg_session_append(s, &absent) == PG_SESSION_OK);
    pangu_message ended = m;
    ended.end_turn = 1;
    ended.stop_reason = PANGU_DEFERRED;
    ended.diagnostics_json = S("[{\"details\":{\"zero\":0,\"nil\":null}}]");
    ended.deferred_json = S("{\"pollAfterMs\":0,\"data\":null}");
    ended.nested_calls_json = S("{}");
    assert(pg_session_append(s, &ended) == PG_SESSION_OK);
    assert(pg_session_commit(s) == PG_SESSION_OK);
    pg_session_destroy(s);
    pg_session_options o = {{path, strlen(path)}, (pg_session_backend)b, 0, 1};
    assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
    const pangu_message *r = pangu_core_message_at(pg_session_messages(s), 0);
    assert(r->response_model.len == sizeof("served-variant") - 1 &&
           !memcmp(r->response_model.data, "served-variant",
                   r->response_model.len));
    assert(r->response_id.len == sizeof(response_id) &&
           !memcmp(r->response_id.data, response_id, sizeof(response_id)));
    assert(r->raw_stop_reason.len == sizeof("provider/custom-stop") - 1 &&
           !memcmp(r->raw_stop_reason.data, "provider/custom-stop",
                   r->raw_stop_reason.len));
    const pangu_message *legacy =
        pangu_core_message_at(pg_session_messages(s), 1);
    assert(legacy->model.len && !legacy->response_model.len &&
           !legacy->response_id.len && !legacy->raw_stop_reason.len);
    assert(r->has_end_turn && !r->end_turn &&
           r->diagnostics_json.len == 2 &&
           !memcmp(r->diagnostics_json.data, "[]", 2));
    assert(r->deferred_json.len == m.deferred_json.len &&
           !memcmp(r->deferred_json.data, m.deferred_json.data,
                   m.deferred_json.len));
    assert(r->nested_calls_json.len == m.nested_calls_json.len &&
           !memcmp(r->nested_calls_json.data, m.nested_calls_json.data,
                   m.nested_calls_json.len));
    assert(!legacy->has_end_turn && !legacy->diagnostics_json.len &&
           !legacy->deferred_json.len && !legacy->nested_calls_json.len);
    const pangu_message *end = pangu_core_message_at(pg_session_messages(s), 2);
    assert(end->has_end_turn && end->end_turn && end->stop_reason == PANGU_DEFERRED);
    assert(end->deferred_json.len == ended.deferred_json.len &&
           !memcmp(end->deferred_json.data, ended.deferred_json.data,
                   ended.deferred_json.len));
    assert(end->nested_calls_json.len == 2 &&
           !memcmp(end->nested_calls_json.data, "{}", 2));
    assert(end->diagnostics_json.len == ended.diagnostics_json.len &&
           !memcmp(end->diagnostics_json.data, ended.diagnostics_json.data,
                   ended.diagnostics_json.len));
    assert(r->content[0].has_redacted && !r->content[0].redacted);
    assert(r->content[1].has_redacted && r->content[1].redacted);
    assert(!r->content[2].has_redacted);
    assert(r->usage.has_cache_write_1h && r->usage.cache_write_1h == 0);
    assert(r->usage.has_reasoning && r->usage.reasoning == 0);
    assert(r->usage.has_total_from_provider && !r->usage.total_from_provider);
    assert(r->content_count == 4 && r->content[3].data.len == 3 &&
           !memcmp(r->content[3].data.data, "a\0b", 3));
    assert(r->content[0].signature.len == 6 &&
           r->content[1].mime_type.len == 9 &&
           r->content[2].tool_namespace.len == 2);
    assert(r->usage.total == 10 && r->usage.total_cost == .1 &&
           r->timestamp_ms == 1234 && r->duration_ms == UINT64_MAX);
    assert(r->section_count == 1 && r->sections[0].removed &&
           r->tools_added_count == 1 &&
           r->tools_added[0].constrained_sampling &&
           r->tools_removed_count == 1);
    assert(r->provider_thinking_level.len == 8 && r->details_json.len == 16 &&
           r->custom_role.len == 7);
    pg_session_destroy(s);
    remove(path);
  }
}
static void failure(void) {
  const char *path = "failure.jsonl";
  pangu_session *s = create(path, PG_STORE_JSONL);
  pangu_content c;
  pangu_message m = user(&c);
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &m) == PG_SESSION_OK);
  remove("failure.old");
  assert(!rename(path, "failure.old"));
  uv_fs_t request;
  assert(!uv_fs_mkdir(NULL, &request, path, 0700, NULL));
  uv_fs_req_cleanup(&request);
  assert(pg_session_commit(s) == PG_SESSION_IO);
  assert(pg_session_sequence(s) == 0);
  assert(!pangu_core_message_count(pg_session_messages(s)));
  assert(!uv_fs_rmdir(NULL, &request, path, NULL));
  uv_fs_req_cleanup(&request);
  assert(!rename("failure.old", path));
  assert(pg_session_commit(s) == PG_SESSION_OK);
  pg_session_destroy(s);
  pg_session_options o = {S("failure.jsonl"), PG_STORE_JSONL, 0, 1};
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  assert(pg_session_sequence(s) == 1);
  pg_session_destroy(s);
  remove(path);
}
typedef struct {
  pangu_session *session;
  int models, queue, cancel, events;
} run_state;
static pangu_status model(void *ctx, pangu_core *core, pangu_model_sink sink,
                          void *data) {
  run_state *state = ctx;
  assert(core);
  if (pangu_core_is_cancelled(core))
    return PANGU_CANCELLED;
  state->models++;
  pangu_content c = {.kind = PANGU_TEXT, .data = S("reply")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  return sink(data, &m, 1);
}
static void events(void *ctx, pangu_core *core, const pangu_event *e) {
  run_state *s = ctx;
  assert(core);
  s->events++;
  if (e->kind == PANGU_AGENT_START && s->queue) {
    s->queue = 0;
    pangu_content c;
    pangu_message m = user(&c);
    assert(pg_session_enqueue(s->session, PANGU_FOLLOW_UP, &m) ==
           PG_SESSION_OK);
  }
  if (e->kind == PANGU_TURN_START && s->cancel) {
    s->cancel = 0;
    pg_session_cancel(s->session);
  }
}
static void driver(void) {
  pangu_session *s = create("driver.jsonl", PG_STORE_JSONL);
  pangu_content c;
  pangu_message m = user(&c);
  run_state state = {.session = s, .queue = 1};
  pangu_loop_config cfg = {.ctx = &state, .model = model, .event = events};
  assert(pg_session_run(s, &m, 1, &cfg) == PG_SESSION_OK);
  assert(state.models == 2 && state.events > 0);
  assert(pangu_core_message_count(pg_session_messages(s)) == 4);
  state.cancel = 1;
  pg_session_status rc = pg_session_run(s, &m, 1, &cfg);
  assert(rc == PG_SESSION_OK || rc == PG_SESSION_CANCELLED);
  assert(state.models == 2);
  pg_session_destroy(s);
  pg_session_options o = {S("driver.jsonl"), PG_STORE_JSONL, 0, 0};
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) >= 4);
  pg_session_destroy(s);
  remove("driver.jsonl");
}
/* A scoped model service verifies app capability lookup and request_ctx
 * separation. */
static pangu_status request(void *ctx, pangu_core *owner,
                            const pangu_core *input,
                            pangu_model_event_sink sink, void *data) {
  int *count = ctx;
  (*count)++;
  assert(owner && pangu_core_message_count(input) > 0);
  int found = 0;
  for (size_t i = 0; i < pangu_core_message_count(input); i++)
    if (pangu_core_message_at(input, i)->tools_added_count)
      found = 1;
  assert(found);
  pangu_content c = {.kind = PANGU_TEXT, .data = S("leased provider")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  pangu_assistant_event event = {.kind = PANGU_ASSISTANT_START};
  pangu_status z = sink(data, &m, &event);
  event.kind = PANGU_ASSISTANT_DONE;
  event.reason = PANGU_STOP;
  return z == PANGU_OK ? sink(data, &m, &event) : z;
}
static pg_status install_model(pg_scope *scope, const void *config,
                               size_t size) {
  assert(size == sizeof(int));
  static const pg_model_service table = {request};
  return pg_service_register(scope, PG_NAME(PG_MODEL_SERVICE), 1, &table,
                             (void *)config);
}
static pg_status install_tools(pg_scope *scope, const void *config,
                               size_t size) {
  (void)config;
  (void)size;
  static const pangu_tool declarations[] = {
      {.name = {"probe", 5}, .schema_json = {"{}", 2}}};
  static const pg_tools_service table = {declarations, 1};
  return pg_service_register(scope, PG_NAME(PG_TOOLS_SERVICE), 1, &table, NULL);
}
typedef struct {
  pg_agent_app *app;
  int provider_calls, summaries, classifications, caller_ctx, deny_calls;
  pangu_status classify_failure;
} classifier_state;
static pangu_status classify_error(void *ctx, const pangu_message *m,
                                   uint64_t capacity, int *out) {
  classifier_state *s = ctx;
  assert(capacity == 1000000);
  s->classifications++;
  if (s->classify_failure == PANGU_CANCELLED) {
    pg_session_cancel(pg_agent_app_session(s->app));
    *out = 1;
    return PANGU_OK;
  }
  if (s->classify_failure != PANGU_OK)
    return s->classify_failure;
  *out = m->stop_reason == PANGU_ERROR;
  return PANGU_OK;
}
static int classify_length(void *ctx, const pangu_message *m,
                           uint64_t desired) {
  (void)ctx;
  (void)m;
  (void)desired;
  return 0;
}
static pangu_status deny_error(void *ctx, const pangu_message *m,
                               uint64_t capacity, int *out) {
  classifier_state *s = ctx;
  (void)m;
  (void)capacity;
  s->deny_calls++;
  *out = 0;
  return PANGU_OK;
}
static void classifier_event(void *ctx, pangu_core *core,
                             const pangu_event *e) {
  (void)core;
  (void)e;
  int *caller_ctx = ctx;
  assert(*caller_ctx == 73);
}
static pangu_status classified_request(void *ctx, pangu_core *owner,
                                       const pangu_core *input,
                                       pangu_model_sink sink, void *out) {
  classifier_state *s = ctx;
  const pangu_message *first = pangu_core_message_at(input, 0);
  int summary = first && first->role == PANGU_SYSTEM;
  pangu_slice key = pangu_core_active_api_key(owner);
  assert(key.len == 18 && !memcmp(key.data, "synthetic-fallback", 18));
  pangu_content text = {.kind = PANGU_TEXT,
                        .data = summary ? S("service summary")
                                        : S("classified reply")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &text,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP,
                     .model = S("classified"),
                     .provider = S("synthetic"),
                     .usage = {.total = 3}};
  if (summary)
    s->summaries++;
  else if (++s->provider_calls == 1) {
    m.stop_reason = PANGU_ERROR;
    m.error = S("context_length_exceeded");
  }
  return sink(out, &m, 1);
}
static void classifier_consumer(void) {
  static const pg_context_classifier api = {sizeof(pg_context_classifier),
                                            classify_error, classify_length};
  static const pg_context_classifier deny = {sizeof(pg_context_classifier),
                                             deny_error, classify_length};
  for (int backend = PG_STORE_JSONL; backend <= PG_STORE_SQLITE; backend++)
    for (int explicit = 0; explicit < 6; explicit++) {
#ifndef PANGU_AGENT_OVERFLOW
      if (explicit == 2)
        continue;
#endif
      char path[80];
      snprintf(path, sizeof(path), "classifier-%d-%d.db", backend, explicit);
      remove(path);
      classifier_state s = {.caller_ctx = 73,
                            .classify_failure = explicit == 3   ? PANGU_NOMEM
                                                : explicit == 4 ? PANGU_INVALID
                                                : explicit == 5
                                                    ? PANGU_CANCELLED
                                                    : PANGU_OK};
      pg_session_options options = {
          {path, strlen(path)}, (pg_session_backend)backend, 1, 0};
      assert(pg_agent_app_open(&options, NULL, 0, &s.app) == PG_OK);
      pangu_session *session = pg_agent_app_session(s.app);
      if (explicit != 2)
        assert(pg_service_register(pg_agent_app_scope(s.app),
                                   PG_NAME(PG_CONTEXT_CLASSIFIER_SERVICE),
                                   PG_CONTEXT_CLASSIFIER_ABI, &api,
                                   &s) == PG_OK);
      if (explicit == 1)
        assert(pg_session_set_context_classifier(session, &deny, &s) ==
               PG_SESSION_OK);
      pg_compaction_policy policy = {1, 1000000, 1, 1};
      assert(pg_session_set_compaction_policy(session, &policy) ==
             PG_SESSION_OK);
      pangu_content text = {.kind = PANGU_TEXT, .data = S("seed context")};
      pangu_message seed = {
          .role = PANGU_USER, .content = &text, .content_count = 1};
      assert(pg_session_begin(session) == PG_SESSION_OK);
      assert(pg_session_append(session, &seed) == PG_SESSION_OK);
      seed.role = PANGU_ASSISTANT;
      seed.stop_reason = PANGU_STOP;
      assert(pg_session_append(session, &seed) == PG_SESSION_OK);
      assert(pg_session_commit(session) == PG_SESSION_OK);
      pangu_loop_config cfg = {
          .ctx = &s.caller_ctx,
          .request = classified_request,
          .has_request_ctx = 1,
          .request_ctx = &s,
          .event = classifier_event,
          .api_key = S("synthetic-fallback"),
          .model_config = {.id = S("classified"), .provider = S("synthetic")}};
      pangu_content c;
      pangu_message input = user(&c);
      pg_session_status result = pg_agent_app_run(s.app, &input, 1, &cfg);
      int recovered = explicit == 0 || explicit == 2;
      assert(result == (recovered       ? PG_SESSION_OK
                        : explicit == 3 ? PG_SESSION_NOMEM
                        : explicit == 5 ? PG_SESSION_CANCELLED
                                        : PG_SESSION_CORE_ERROR));
      assert(s.provider_calls == (recovered ? 2 : 1));
      assert(s.summaries == recovered);
      if (explicit == 5)
        assert(pg_session_entry_count(session) == 4);
      assert(explicit == 1   ? (s.deny_calls > 0 && s.classifications == 0)
             : explicit == 2 ? s.classifications == 0
                             : s.classifications > 0);
      void *ctx = NULL;
      assert(pg_session_context_classifier(session, &ctx) ==
             (explicit == 1 ? &deny : NULL));
      assert(ctx == (explicit == 1 ? &s : NULL));
      uint64_t usage = pg_session_usage(session).total;
      pg_agent_app_close(s.app);
      options.create = 0;
      assert(pg_agent_app_open(&options, NULL, 0, &s.app) == PG_OK);
      session = pg_agent_app_session(s.app);
      assert(!pg_session_context_classifier(session, NULL));
      assert(pg_session_usage(session).total == usage &&
             usage == (recovered ? 9 : 3));
      pg_agent_app_close(s.app);
      remove(path);
    }
}
static void plugins(void) {
  remove("plugins.jsonl");
  int calls = 0;
  const pg_plugin plugin = {
      PANGU_SERVICES_ABI, sizeof(pg_plugin), PG_NAME("test.model"), NULL, 0,
      install_model};
  const pg_plugin tools = {
      PANGU_SERVICES_ABI, sizeof(pg_plugin), PG_NAME("test.tools"), NULL, 0,
      install_tools};
  pg_app_plugin plugins[] = {{&plugin, &calls, sizeof(calls)},
                             {&tools, NULL, 0}};
  pg_session_options o = {S("plugins.jsonl"), PG_STORE_JSONL, 1, 0};
  pg_agent_app *a = NULL;
  assert(pg_agent_app_open(&o, plugins, 2, &a) == PG_OK);
  pangu_content c;
  pangu_message m = user(&c);
  pangu_loop_config cfg = {0};
  assert(pg_agent_app_run(a, &m, 1, &cfg) == PG_SESSION_OK);
  assert(calls == 1);
  assert(pangu_core_message_count(
             pg_session_messages(pg_agent_app_session(a))) == 3);
  pg_agent_app_close(a);
  remove("plugins.jsonl");
  classifier_consumer();
}
int main(int argc, char **argv) {
  assert(argc == 2);
  if (!strcmp(argv[1], "rich"))
    rich();
  else if (!strcmp(argv[1], "failure"))
    failure();
  else if (!strcmp(argv[1], "driver"))
    driver();
  else if (!strcmp(argv[1], "plugins"))
    plugins();
  else
    return 2;
  puts(argv[1]);
  return 0;
}
