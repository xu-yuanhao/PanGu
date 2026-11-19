/* Actual host/session/presentation fixture; model owns its poll on this thread. */
#include <pangu/agent/app.h>
#include <pangu/tui/plugin.h>
#include <pangu/services/model.h>
#ifdef PANGU_TUI_RESOURCES_TEST
#include <pangu/services/resources.h>
#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <uv.h>
typedef struct {
  pg_agent_app *app;
  const pg_tui_service *view;
  void *view_ctx;
  int marker;
  int resources;
  int history;
  unsigned requests;
  int approval, stop_approval;
  unsigned effects;
  char *effect_path;
} state;
#ifdef PANGU_TUI_RESOURCES_TEST
static int contains(pangu_slice text, const char *part) {
  size_t n = strlen(part);
  for (size_t i = 0; n <= text.len && i <= text.len - n; i++)
    if (!memcmp(text.data + i, part, n)) return 1;
  return 0;
}
static pangu_status resource_key(void *ctx, pangu_core *owner,
 pangu_slice provider, pangu_api_key_sink sink, void *out) {
  (void)owner;
  assert(*(int *)ctx == 12 && provider.len);
  return sink(out, (pangu_slice){"synthetic-tui-secret", 20});
}
static pangu_status resource_provider(state *s, pangu_core *owner,
 const pangu_core *request, pangu_model_event_sink sink, void *sink_ctx) {
  assert(pg_agent_app_reload_resources(s->app) == PG_FAILED);
  const pangu_model_config *model = pangu_core_active_model(owner);
  assert(model);
  assert(contains(pangu_core_active_api_key(owner), "synthetic-tui-secret"));
  const pangu_message *system = pangu_core_message_at(request, 0);
  const pangu_message *last = pangu_core_message_at(request, pangu_core_message_count(request)-1);
  assert(system && system->role == PANGU_SYSTEM && system->content_count);
  assert(last && last->role == PANGU_USER && last->content_count);
  pangu_slice input = last->content[0].data;
  const char *sys = contains(system->content[0].data, "project-system-v2") ? "project-system-v2" :
                    contains(system->content[0].data, "project-system-v1") ? "project-system-v1" :
                    contains(system->content[0].data, "user-system-v1") ? "user-system-v1" : "missing-system";
  if (contains(input, "slow")) {
    pg_lease *lease = NULL;
    assert(pg_service_acquire(pg_agent_app_scope(s->app), PG_NAME(PG_TUI_SERVICE), 1, &lease) == PG_OK);
    const pg_tui_service *view = pg_lease_table(lease);
    pangu_tui_chat_append(view->chat(pg_lease_context(lease)), "Resource request waiting", 24);
    pg_lease_release(lease);
    for (int i = 0; i < 200; i++) {
      pg_agent_app_poll(s->app, owner);
      if (pangu_core_is_cancelled(owner)) return PANGU_CANCELLED;
      /* Tests queue exactly one steer and one follow-up; observe committed
       * acceptance, then release this request without a timing guess. */
      pangu_session *session = pg_agent_app_session(s->app);
      if (pg_session_queue_count(session, PANGU_STEERING) &&
          pg_session_queue_count(session, PANGU_FOLLOW_UP)) break;
      uv_sleep(10);
    }
  }
  assert(model->id.len < 1024 && input.len < 8192);
  int n = snprintf(NULL, 0, "Model=%.*s\nThinking=%d\nSystem=%s\nInput:\n%.*s",
                   (int)model->id.len, model->id.data,
                   (int)pangu_core_active_thinking(owner), sys,
                   (int)input.len, input.data);
  assert(n >= 0);
  char *answer = malloc((size_t)n + 1);
  assert(answer);
  snprintf(answer, (size_t)n + 1, "Model=%.*s\nThinking=%d\nSystem=%s\nInput:\n%.*s",
           (int)model->id.len, model->id.data,
           (int)pangu_core_active_thinking(owner), sys, (int)input.len, input.data);
  pangu_content c = {.kind=PANGU_TEXT, .data={answer,(size_t)n}};
  pangu_message m = {.role=PANGU_ASSISTANT, .content=&c, .content_count=1,
    .stop_reason=PANGU_STOP, .usage={.input=3,.output=2,.total=5}};
  pangu_assistant_event e = {.kind=PANGU_ASSISTANT_START};
  pangu_status z = sink(sink_ctx, &m, &e);
  if (!z) { e.kind=PANGU_ASSISTANT_DONE; e.reason=PANGU_STOP; z=sink(sink_ctx,&m,&e); }
  free(answer);
  return z;
}
#endif
static void approval_stop(void *ctx, pangu_session *session, const pg_session_event *e) {
  state *s = ctx;
  if (!s->stop_approval || e->kind != PG_SESSION_COMMITTED ||
      !pg_session_approval_pending(session)) return;
  s->stop_approval = 0;
  pg_lease *lease = NULL;
  assert(pg_service_acquire(pg_agent_app_scope(s->app), PG_NAME(PG_TUI_SERVICE), 1, &lease) == PG_OK);
  const pg_tui_service *view = pg_lease_table(lease);
  view->stop(pg_lease_context(lease));
  pg_lease_release(lease);
}
static pangu_status approval_validate(void *ctx, pangu_slice schema, pangu_slice args) {
  (void)ctx; (void)schema;
  return args.len == 2 && !memcmp(args.data, "{}", 2) ? PANGU_OK : PANGU_INVALID;
}
static pangu_status approval_tool(void *ctx, pangu_core *core,
 const pangu_content *call, pangu_tool_sink sink, void *target) {
  (void)core; (void)call;
  state *s = ctx;
  /* An ordinary tool: policy and approval live entirely in app/services/TUI. */
  FILE *effect = fopen(s->effect_path, "ab");
  assert(effect);
  assert(fwrite("effect\n", 1, 7, effect) == 7);
  assert(fclose(effect) == 0);
  ++s->effects;
  const char *text = "approval effect executed";
  pangu_content content = {.kind=PANGU_TEXT,.data={text,strlen(text)}};
  pangu_message result = {.role=PANGU_TOOL_RESULT,.content=&content,.content_count=1,
                          .is_error=0};
  return sink(target, &result, 1);
}
static pangu_status provider(void *ctx, pangu_core *owner,
 const pangu_core *request, pangu_model_event_sink sink, void *sink_ctx) {
  state *s = ctx;
  assert(s->marker == 314);
#ifdef PANGU_TUI_RESOURCES_TEST
  if (s->resources) return resource_provider(s, owner, request, sink, sink_ctx);
#endif
  if (s->history) {
    ++s->requests;
    char marker[80];
    int n = snprintf(marker, sizeof marker, "Model request %u", s->requests);
    pangu_tui_chat_append(s->view->chat(s->view_ctx), marker, (size_t)n);
  }
  if (s->approval) {
    const pangu_message *last = pangu_core_message_at(request, pangu_core_message_count(request)-1);
    if (last && last->role == PANGU_USER) {
      s->stop_approval = last->content_count && last->content[0].data.len == 4 &&
                        !memcmp(last->content[0].data.data, "stop", 4);
      pangu_content call = {.kind=PANGU_TOOL_CALL,.id={"approval-call",13},
        .name={"approval-fixture",16},.data={"{}",2}};
      pangu_message m = {.role=PANGU_ASSISTANT,.content=&call,.content_count=1,
                         .stop_reason=PANGU_TOOL_USE};
      pangu_assistant_event e = {.kind=PANGU_ASSISTANT_START};
      pangu_status z = sink(sink_ctx, &m, &e);
      if (!z) { e.kind=PANGU_ASSISTANT_DONE; e.reason=PANGU_TOOL_USE; z=sink(sink_ctx,&m,&e); }
      return z;
    }
  }
  int summary = 0;
  pangu_slice answer = {"reply", 5};
  for (size_t i = 0; i < pangu_core_message_count(request); i++) {
    const pangu_message *m = pangu_core_message_at(request, i);
    for (size_t j = 0; j < m->content_count; j++) {
      pangu_slice t = m->content[j].data;
      if (m->role == PANGU_SYSTEM && t.data) {
        const char needle[] = "Do NOT continue";
        for (size_t k = 0; k + sizeof needle - 1 <= t.len; k++)
          if (!memcmp(t.data + k, needle, sizeof needle - 1)) summary = 1;
      }
    }
    if (m->role == PANGU_USER && m->content_count && !summary)
      answer = m->content[0].data;
  }
  if (summary || (answer.len == 4 && !memcmp(answer.data, "slow", 4))) {
    for (int i = 0; i < 80; i++) {
      if (s->view) s->view->poll(s->view_ctx);
      else pg_agent_app_poll(s->app, owner);
      if (pangu_core_is_cancelled(owner)) return PANGU_CANCELLED;
      uv_sleep(10);
    }
    if (summary) answer = (pangu_slice){"fixture model summary", 21};
  }
  if (s->history) answer = (pangu_slice){"assistant-only-sentinel", 23};
  pangu_content c = {.kind=PANGU_TEXT, .data=answer};
  pangu_message m = {.role=PANGU_ASSISTANT, .content=&c, .content_count=1,
    .stop_reason=PANGU_STOP, .usage={.input=3,.output=2,.total=5}};
  pangu_assistant_event e = {.kind=PANGU_ASSISTANT_START};
  pangu_status z = sink(sink_ctx, &m, &e);
  if (z) return z;
  e.kind=PANGU_ASSISTANT_DONE; e.reason=PANGU_STOP;
  return sink(sink_ctx, &m, &e);
}
static pg_status install(pg_scope *scope, const void *cfg, size_t n) {
  if (n != sizeof(state) || !cfg) return PG_INVALID;
  static const pg_model_service api = {provider};
  return pg_service_register(scope, PG_NAME(PG_MODEL_SERVICE), 1, &api, (void *)cfg);
}
static pangu_status live_poll(void *ctx, pangu_core *owner) {
  state *s = ctx;
  pg_agent_app_poll(s->app, owner);
  return pangu_core_is_cancelled(owner) ? PANGU_CANCELLED : PANGU_OK;
}
static pangu_slice string(const char *s) {
  return (pangu_slice){s, s ? strlen(s) : 0};
}
static void caller_event(void *ctx, pangu_core *core, const pangu_event *e) {
  (void)core; (void)e;
  assert(*(int *)ctx == 12);
}
/* This fixture's history-* files are exclusively direct-session stores: no
 * resource driver or prompt preparation is ever used. That construction is
 * evidence of raw USER provenance; arbitrary imported app files are excluded. */
static pangu_tui_status raw_history(void *ctx, pg_tui_history_sink sink,
                                    void *target) {
  state *s = ctx;
  assert(s->history);
  return pg_tui_history_user_messages(
      pg_session_messages(pg_agent_app_session(s->app)), sink, target);
}
static void history_baseline(pangu_session *session) {
  pangu_content user[] = {
    {.kind=PANGU_TEXT, .data={"  seeded-", 9}},
    {.kind=PANGU_TEXT, .data={"中文  ", sizeof("中文  ")-1}}};
  pangu_content system = {.kind=PANGU_TEXT, .data={"system-only-sentinel",20}};
  pangu_content tool = {.kind=PANGU_TEXT, .data={"tool-only-sentinel",18}};
  pangu_content call = {.kind=PANGU_TOOL_CALL, .id={"history-call",12},
    .name={"history-fixture",15}, .data={"{}",2}};
  pangu_message messages[] = {
    {.role=PANGU_SYSTEM, .content=&system, .content_count=1},
    {.role=PANGU_USER, .content=user, .content_count=2},
    {.role=PANGU_ASSISTANT, .content=&call, .content_count=1,
      .stop_reason=PANGU_TOOL_USE},
    {.role=PANGU_TOOL_RESULT, .content=&tool, .content_count=1,
      .tool_call_id={"history-call",12}, .tool_name={"history-fixture",15}}};
  assert(pg_session_begin(session) == PG_SESSION_OK);
  for (size_t i = 0; i < sizeof messages/sizeof *messages; ++i)
    assert(pg_session_append(session, &messages[i]) == PG_SESSION_OK);
  assert(pg_session_commit(session) == PG_SESSION_OK);
}
int main(int argc, char **argv) {
  assert(argc == 3 || argc == 4 || argc == 8);
  state s = {.marker=314};
  const pg_plugin model = {PANGU_SERVICES_ABI,sizeof(pg_plugin),PG_NAME("fixture.model"),NULL,0,install};
  pg_app_plugin plugin = {&model,&s,sizeof s};
  pg_session_options opt = {{argv[1],strlen(argv[1])},PG_STORE_JSONL,!strcmp(argv[2],"create"),0};
  s.history = argc == 4 && (!strcmp(argv[3], "history-jsonl") ||
                             !strcmp(argv[3], "history-sqlite"));
  if (s.history && !strcmp(argv[3], "history-sqlite")) opt.backend = PG_STORE_SQLITE;
  s.approval = argc == 4 && (!strcmp(argv[3], "approval-jsonl") ||
                              !strcmp(argv[3], "approval-sqlite"));
  if (s.approval && !strcmp(argv[3], "approval-sqlite")) opt.backend=PG_STORE_SQLITE;
  int app_legacy = argc == 4 && (!strcmp(argv[3], "app-legacy-jsonl") ||
                                  !strcmp(argv[3], "app-legacy-sqlite"));
  if (app_legacy && !strcmp(argv[3], "app-legacy-sqlite")) opt.backend = PG_STORE_SQLITE;
  int missing = argc == 4 && !strcmp(argv[3], "missing");
#ifdef PANGU_TUI_RESOURCES_TEST
  if (argc == 8) {
    assert(!strcmp(argv[3], "resources"));
    s.resources = 1;
    opt.backend = !strcmp(argv[7], "sqlite") ? PG_STORE_SQLITE : PG_STORE_JSONL;
    pg_resources_config resources = {.user_directory=argv[4], .cwd=argv[5], .project_trusted=atoi(argv[6])};
    pg_app_plugin plugins[] = {plugin, {&pg_resources_plugin, &resources, sizeof resources}};
    assert(pg_agent_app_open(&opt, plugins, 2, &s.app) == PG_OK);
  } else
#endif
  assert(pg_agent_app_open(&opt,missing ? NULL : &plugin,missing ? 0 : 1,&s.app)==PG_OK);
  if (s.history && opt.create) history_baseline(pg_agent_app_session(s.app));
  /* caller ctx intentionally differs from provider ctx: preserve both. */
  int caller = 12;
  pg_tui_config cfg = {.loop={.ctx=&caller,.event=caller_event}};
#ifdef PANGU_TUI_RESOURCES_TEST
  if (s.resources) cfg.loop.get_api_key = resource_key;
#endif
  if (s.history) {
    cfg.loop.request_events = provider;
    cfg.loop.has_request_ctx = 1;
    cfg.loop.request_ctx = &s;
  }
  pangu_tool approval = {.name={"approval-fixture",16}, .schema_json={"{}",2},
                         .ctx=&s, .validate=approval_validate, .execute=approval_tool};
  if (s.approval) {
    cfg.loop.tools=&approval; cfg.loop.tool_count=1;
    assert(pg_agent_app_set_approval(s.app, PG_APP_APPROVAL_ASK) == PG_OK);
    uint64_t subscription;
    assert(pg_session_subscribe(pg_agent_app_session(s.app), approval_stop, &s,
                                &subscription) == PG_SESSION_OK);
    s.effect_path = malloc(strlen(argv[1]) + 9);
    assert(s.effect_path);
    sprintf(s.effect_path, "%s.effects", argv[1]);
  }
  int live = argc == 4 && !strcmp(argv[3], "live");
  pg_deepseek_config deepseek = {.poll=live_poll,.poll_ctx=&s};
  if (live) {
    const char *key = getenv("DEEPSEEK_API_KEY");
    if (!key || !*key) { pg_agent_app_close(s.app); return 77; }
    deepseek.endpoint = string(getenv("PANGU_DEEPSEEK_ENDPOINT"));
    pg_scope *scope = NULL;
    assert(pg_plugin_install(pg_agent_app_scope(s.app), &pg_deepseek_plugin,
                            &deepseek, sizeof deepseek, &scope) == PG_OK);
    cfg.loop.api_key = string(key);
    const char *model_id = getenv("PANGU_MODEL");
    cfg.loop.model_config.id = string(model_id && *model_id ? model_id : "deepseek-chat");
    cfg.loop.model_config.provider = string("deepseek");
    cfg.loop.model_config.api = string("openai-completions");
  }
  pg_status z;
  if (argc == 4 && !live && !app_legacy && !s.approval) {
    pg_scope *root = pg_agent_app_scope(s.app), *scope = NULL;
    pg_session_service old;
    if (!strcmp(argv[3], "legacy")) {
      pg_lease *session = NULL;
      assert(pg_service_acquire(root, PG_NAME(PG_SESSION_SERVICE), 1, &session) == PG_OK);
      old = *(const pg_session_service *)pg_lease_table(session);
      old.size = offsetof(pg_session_service, entry_count);
      assert(pg_service_register(root, PG_NAME(PG_SESSION_SERVICE), 1, &old,
                                 pg_lease_context(session)) == PG_OK);
      pg_lease_release(session);
    }
    /* ABI1 actions prefix predating reload/compact: exact allocation makes
     * accidental tail reads observable under ASan. NULL slots use session. */
    pg_tui_actions history_actions = {.size=sizeof(pg_tui_actions),
                                       .input_history=raw_history};
    if (s.history) {
      cfg.actions = &history_actions;
      cfg.actions_ctx = &s;
    }
    pg_tui_actions *old_actions = NULL;
    if (!strcmp(argv[3], "oldprefix") || !strcmp(argv[3], "history-prefix")) {
      size_t n = !strcmp(argv[3], "history-prefix")
        ? offsetof(pg_tui_actions, input_history)
        : offsetof(pg_tui_actions, reload_resources);
      old_actions = calloc(1, n);
      assert(old_actions);
      old_actions->size = n;
      cfg.actions = old_actions;
    }
    size_t config_size = !strcmp(argv[3], "legacy-config")
      ? offsetof(pg_tui_config, actions) : sizeof cfg;
    /* Exact allocation catches direct-plugin reads past the old config prefix
     * under ASan, independently of app_present's bounded-copy adapter. */
    void *exact = malloc(config_size);
    assert(exact);
    memcpy(exact, &cfg, config_size);
    assert(pg_plugin_install(root, pg_tui_plugin(), exact, config_size, &scope) == PG_OK);
    free(exact);
    free(old_actions);
    pg_lease *lease = NULL;
    assert(pg_service_acquire(root, PG_NAME(PG_TUI_SERVICE), 1, &lease) == PG_OK);
    s.view = pg_lease_table(lease);
    s.view_ctx = pg_lease_context(lease);
    assert(s.view->start(s.view_ctx) == PANGU_TUI_OK);
    z = s.view->run(s.view_ctx) == PANGU_TUI_OK ? PG_OK : PG_FAILED;
    s.view->stop(s.view_ctx);
    s.view = NULL;
    pg_lease_release(lease);
    pg_scope_dispose(scope);
  } else z = pg_agent_app_present(s.app,pg_tui_plugin(),&cfg,sizeof cfg);
#ifdef PANGU_TUI_RESOURCES_TEST
  if (s.resources && opt.create && !z) {
    /* Acceptance persists an original, but no USER placement has happened.
     * Reopen must exclude this pending raw command from editor history. */
    const char raw[] = "/hello pending-history-only";
    pangu_content text = {.kind=PANGU_TEXT, .data={raw,sizeof(raw)-1}};
    pangu_message input = {.role=PANGU_USER,.content=&text,.content_count=1};
    assert(pg_agent_app_enqueue(s.app, PANGU_FOLLOW_UP, &input) == PG_SESSION_OK);
  }
#endif
  pg_agent_app_close(s.app);
  free(s.effect_path);
  return z ? 1 : 0;
}
