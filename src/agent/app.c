#include <pangu/agent/app.h>
#include <pangu/agent/recovery.h>
#include <pangu/agent/approval.h>
#include <pangu/services/approval.h>
#include <pangu/services/model.h>
#include <pangu/services/tools.h>
#include <pangu/tui/plugin.h>
#include <stdlib.h>
#include <string.h>
#ifdef PANGU_AGENT_RESOURCES
#include "resources_internal.h"
#endif
#define SESSION_SLOT(a, slot) ((a)->service->size >= offsetof(pg_session_service, slot) + sizeof((a)->service->slot) && (a)->service->slot)
struct pg_agent_app {
  pg_host *host;
  pg_app_approval_policy approval;
  pg_lease *session;
  const pg_session_service *service;
  int running, presenting, actions_enabled;
#ifdef PANGU_AGENT_RESOURCES
  pg_agent_resources_run *active_resources;
#endif
  pg_lease *presentation;
  const pg_tui_service *view;
};
pg_status pg_agent_app_open(const pg_session_options *options,
                            const pg_app_plugin *plugins, size_t count,
                            pg_agent_app **out) {
  if (out)
    *out = NULL;
  if (!out || !options || (count && !plugins))
    return PG_INVALID;
  pg_agent_app *a = calloc(1, sizeof(*a));
  if (!a)
    return PG_NOMEM;
  pg_status z = pg_host_create(&a->host);
  for (size_t i = 0; z == PG_OK && i < count; i++) {
    pg_scope *scope = NULL;
    z = pg_plugin_install(pg_host_root(a->host), plugins[i].plugin,
                          plugins[i].config, plugins[i].config_size, &scope);
  }
  pg_scope *scope = NULL;
  if (z == PG_OK)
    z = pg_plugin_install(pg_host_root(a->host), pg_agent_core_plugin(),
                          options, sizeof(*options), &scope);
  if (z == PG_OK)
    z = pg_service_acquire(scope, PG_NAME(PG_SESSION_SERVICE), PG_SESSION_ABI,
                           &a->session);
  if (z != PG_OK) {
    pg_agent_app_close(a);
    return z;
  }
  a->service = pg_lease_table(a->session);
  *out = a;
  return PG_OK;
}
pangu_session *pg_agent_app_session(pg_agent_app *a) {
  return a && a->session ? pg_lease_context(a->session) : NULL;
}
pg_scope *pg_agent_app_scope(pg_agent_app *a) {
  return a ? pg_host_root(a->host) : NULL;
}
pg_status pg_agent_app_load_plugin(pg_agent_app *a, pg_name cwd, pg_name path,
                                   const void *config, size_t config_size,
                                   pg_dynamic_diagnostic *diagnostic) {
  if (diagnostic)
    memset(diagnostic, 0, sizeof(*diagnostic));
  if (!a || (config_size && !config))
    return PG_INVALID;
  if (a->running || a->presenting)
    return PG_FAILED;
  pg_scope *scope = NULL;
  return pg_dynamic_plugin_load(pg_host_root(a->host), cwd, path, config,
                                 config_size, &scope, diagnostic);
}
static pg_status resolve_tools(pg_agent_app *a, pangu_loop_config *config,
                               pg_lease **lease) {
  *lease = NULL;
  if (config->tools || config->tool_count)
    return PG_OK;
  pg_status z = pg_service_acquire(pg_host_root(a->host),
                                   PG_NAME(PG_TOOLS_SERVICE), 1, lease);
  if (z == PG_NOT_FOUND)
    return PG_OK;
  if (z != PG_OK)
    return z;
  const pg_tools_service *tools = pg_lease_table(*lease);
  if (!tools || (tools->count && !tools->tools)) {
    pg_lease_release(*lease);
    *lease = NULL;
    return PG_ABI;
  }
  config->tools = tools->tools;
  config->tool_count = tools->count;
  return PG_OK;
}
pg_status pg_agent_app_set_approval(pg_agent_app *a, pg_app_approval_policy policy) {
  if (!a || policy < PG_APP_APPROVAL_DISABLED || policy > PG_APP_APPROVAL_NEVER)
    return PG_INVALID;
  if (a->running || (a->presenting && !a->actions_enabled)) return PG_FAILED;
  a->approval = policy;
  return PG_OK;
}
typedef struct { pg_agent_app *app; pangu_core *core; } approval_answer;
static pg_session_status approval_answerer(void *ctx, pangu_session *session,
    uint64_t id, const pg_approval_question *q, pg_approval_outcome *out) {
  (void)session; (void)id;
  approval_answer *a = ctx;
  if (a->app->approval == PG_APP_APPROVAL_NEVER) {
    *out = PG_APPROVAL_REJECTED; return PG_SESSION_OK;
  }
  pg_service_approval_question question = {q->tool_name, q->call_id, q->reason};
  pg_service_approval_outcome result = PG_SERVICE_UNAVAILABLE;
  pg_status z = pg_approval_ask(pg_agent_app_scope(a->app), a->core, &question, &result);
  *out = PG_APPROVAL_UNAVAILABLE;
  if (z != PG_OK) return PG_SESSION_CORE_ERROR;
  switch (result) {
  case PG_SERVICE_ALLOWED_ONCE: *out = PG_APPROVAL_ALLOWED_ONCE; break;
  case PG_SERVICE_REJECTED: *out = PG_APPROVAL_REJECTED; break;
  case PG_SERVICE_CANCELLED: *out = PG_APPROVAL_CANCELLED; break;
  case PG_SERVICE_UNAVAILABLE: break;
  }
  return PG_SESSION_OK;
}
static pg_status approval_request(void *ctx, pangu_core *core,
    const pg_service_approval_question *q, pg_service_approval_outcome *out) {
  pg_agent_app *a = ctx;
  *out = PG_SERVICE_UNAVAILABLE;
  approval_answer answer = {a, core};
  pg_approval_question question = {q->tool_name, q->call_id, q->reason};
  pg_approval_outcome result = PG_APPROVAL_UNAVAILABLE;
  pg_session_status z = pg_session_approval_request(pg_agent_app_session(a),
      &question, approval_answerer, &answer, &result);
  if (z != PG_SESSION_OK) return PG_FAILED;
  switch (result) {
  case PG_APPROVAL_ALLOWED_ONCE: *out = PG_SERVICE_ALLOWED_ONCE; break;
  case PG_APPROVAL_REJECTED: *out = PG_SERVICE_REJECTED; break;
  case PG_APPROVAL_CANCELLED: *out = PG_SERVICE_CANCELLED; break;
  case PG_APPROVAL_UNAVAILABLE: break;
  }
  return PG_OK;
}
/* Private run scope isolates the borrowed selection, including explicit tools.
 * Its wrapper dependency pins declarations until the entire run has settled. */
static pg_status approval_wrap(pg_agent_app *a, pangu_loop_config *config,
    pg_tools_service *source, pg_scope **scope, pg_lease **lease) {
  *scope = NULL; *lease = NULL;
  if (a->approval == PG_APP_APPROVAL_DISABLED || !config->tool_count) return PG_OK;
  *source = (pg_tools_service){config->tools, config->tool_count};
  pg_status z = pg_scope_create(pg_agent_app_scope(a), scope);
  if (z != PG_OK) return z;
  z = pg_service_register(*scope, PG_NAME("pangu.agent.approval.source"), 1, source, NULL);
  pg_scope *wrapper = NULL;
  pg_approval_tools_config cfg = {.policy = PG_APPROVAL_ASK,
      .reason = {"Tool execution", 14}, .request = approval_request, .context = a};
  if (z == PG_OK) z = pg_approval_tools_install(*scope,
      PG_NAME("pangu.agent.approval.tools"), PG_NAME("pangu.agent.approval.source"), &cfg, &wrapper);
  if (z == PG_OK) z = pg_service_acquire(*scope,
      PG_NAME("pangu.agent.approval.tools"), 1, lease);
  if (z == PG_OK) {
    const pg_tools_service *tools = pg_lease_table(*lease);
    config->tools = tools->tools; config->tool_count = tools->count;
  }
  return z;
}
/* Each generation owns declarations as well as the wrapper dependency. Core
 * copies declarations at the sink boundary but borrows callback contexts until
 * run return, including failed preparation transactions and agent_end. */
typedef struct approval_generation {
  struct approval_generation *next;
  pg_tools_service source;
  pangu_tool *owned;
  pg_scope *scope;
  pg_lease *lease;
} approval_generation;
typedef struct approval_run {
  struct approval_run *previous;
  pg_agent_app *app;
  pangu_prepare_update_fn request, next;
  approval_generation *generations;
} approval_run;
/* Same owner-thread stack pattern as resources credentials: preserve every
 * caller ctx, including NULL and separate provider contexts; nested apps restore
 * the outer binding before returning to its callback. */
static _Thread_local approval_run *active_approval;
static void approval_release(approval_run *run) {
  while (run->generations) {
    approval_generation *g = run->generations;
    run->generations = g->next;
    if (g->lease) pg_lease_release(g->lease);
    if (g->scope) pg_scope_dispose(g->scope);
    for (size_t i = 0; i < g->source.count; ++i) {
      free((void *)g->owned[i].name.data);
      free((void *)g->owned[i].description.data);
      free((void *)g->owned[i].schema_json.data);
    }
    free(g->owned); free(g);
  }
}
static pangu_status approval_selection(approval_run *run, pangu_loop_config *cfg) {
  if (!cfg->tool_count) return PANGU_OK;
  if (!cfg->tools) return PANGU_INVALID;
  if (cfg->tool_count > SIZE_MAX / sizeof(pangu_tool)) return PANGU_NOMEM;
  approval_generation *g = calloc(1, sizeof(*g));
  if (!g) return PANGU_NOMEM;
  g->next = run->generations; run->generations = g;
  g->owned = calloc(cfg->tool_count, sizeof(*g->owned));
  if (!g->owned) return PANGU_NOMEM;
  g->source.tools = g->owned;
  g->source.count = cfg->tool_count;
  for (size_t i = 0; i < cfg->tool_count; ++i) {
    g->owned[i] = cfg->tools[i];
    g->owned[i].name = (pangu_slice){0};
    g->owned[i].description = (pangu_slice){0};
    g->owned[i].schema_json = (pangu_slice){0};
    pangu_slice input[] = {cfg->tools[i].name, cfg->tools[i].description, cfg->tools[i].schema_json};
    pangu_slice *output[] = {&g->owned[i].name, &g->owned[i].description, &g->owned[i].schema_json};
    for (size_t j = 0; j < 3; ++j) {
      if (!input[j].len) continue;
      if (!input[j].data) return PANGU_INVALID;
      char *copy = malloc(input[j].len);
      if (!copy) return PANGU_NOMEM;
      memcpy(copy, input[j].data, input[j].len);
      *output[j] = (pangu_slice){copy, input[j].len};
    }
  }
  cfg->tools = g->owned;
  pg_status z = approval_wrap(run->app, cfg, &g->source, &g->scope, &g->lease);
  return z == PG_OK ? PANGU_OK : z == PG_NOMEM ? PANGU_NOMEM : PANGU_INVALID;
}
typedef struct {
  approval_run *run;
  pangu_request_update_sink sink;
  void *ctx;
  pangu_status failure;
} approval_update;
static pangu_status approval_update_sink(void *ctx, const pangu_request_update *u) {
  approval_update *a = ctx;
  if (a->failure != PANGU_OK) return a->failure;
  if (!u) return a->failure = PANGU_INVALID;
  pangu_request_update copy = *u;
  if (u->fields & PANGU_UPDATE_TOOLS) {
    pangu_loop_config selection = {.tools = u->tools, .tool_count = u->tool_count};
    a->failure = approval_selection(a->run, &selection);
    if (a->failure != PANGU_OK) return a->failure;
    copy.tools = selection.tools;
  }
  return a->failure = a->sink(a->ctx, &copy);
}
static pangu_status approval_prepare(void *ctx, pangu_core *core,
    pangu_request_update_sink sink, void *out, int next) {
  approval_run *run = active_approval;
  if (!run) return PANGU_INVALID;
  approval_update update = {run, sink, out, PANGU_OK};
  pangu_status z = (next ? run->next : run->request)(ctx, core, approval_update_sink, &update);
  return update.failure != PANGU_OK ? update.failure : z;
}
static pangu_status approval_prepare_request(void *ctx, pangu_core *core,
    pangu_request_update_sink sink, void *out) {
  return approval_prepare(ctx, core, sink, out, 0);
}
static pangu_status approval_prepare_next(void *ctx, pangu_core *core,
    pangu_request_update_sink sink, void *out) {
  return approval_prepare(ctx, core, sink, out, 1);
}
static pg_status resolve_classifier(pg_agent_app *a, pg_lease **lease) {
  *lease = NULL;
  pangu_session *session = pg_agent_app_session(a);
  if (pg_session_context_classifier(session, NULL))
    return PG_OK; /* Explicit caller-owned classifier wins. */
  pg_status z = pg_service_acquire(pg_host_root(a->host),
      PG_NAME(PG_CONTEXT_CLASSIFIER_SERVICE), PG_CONTEXT_CLASSIFIER_ABI, lease);
  if (z == PG_NOT_FOUND)
    return PG_OK;
  if (z != PG_OK)
    return z;
  if (pg_session_set_context_classifier(session, pg_lease_table(*lease),
                                        pg_lease_context(*lease)) != PG_SESSION_OK) {
    pg_lease_release(*lease);
    *lease = NULL;
    return PG_ABI;
  }
  return PG_OK;
}
static void release_classifier(pg_agent_app *a, pg_lease *lease) {
  if (!lease)
    return;
  pg_session_set_context_classifier(pg_agent_app_session(a), NULL, NULL);
  pg_lease_release(lease);
}
static pg_session_status app_run(pg_agent_app *a, const pangu_message *m,
                                 size_t n, const pangu_loop_config *c,
                                 int presentation_action) {
  if (!a || !a->service)
    return PG_SESSION_INVALID;
  if (a->running || (a->presenting && !presentation_action))
    return PG_SESSION_BUSY;
  if (!c)
    return PG_SESSION_INVALID;
  const pangu_message *originals = m;
  pangu_loop_config config = *c;
  pg_lease *model = NULL;
  if (!config.model && !config.request && !config.request_events) {
    pg_status status = pg_service_acquire(pg_host_root(a->host),
                                          PG_NAME(PG_MODEL_SERVICE), 1, &model);
    if (status != PG_OK)
      return PG_SESSION_CORE_ERROR;
    const pg_model_service *provider = pg_lease_table(model);
    config.request_events = provider->request_events;
    config.has_request_ctx = 1;
    config.request_ctx = pg_lease_context(model);
  }
  pg_lease *tools = NULL;
  if (resolve_tools(a, &config, &tools) != PG_OK) {
    if (model)
      pg_lease_release(model);
    return PG_SESSION_CORE_ERROR;
  }
  pg_lease *classifier = NULL;
  if (resolve_classifier(a, &classifier) != PG_OK) {
    if (tools)
      pg_lease_release(tools);
    if (model)
      pg_lease_release(model);
    return PG_SESSION_CORE_ERROR;
  }
#ifdef PANGU_AGENT_RESOURCES
  pg_agent_resources_run *resources = NULL;
  pg_status prepared =
      pg_agent_resources_prepare(pg_agent_app_scope(a), pg_agent_app_session(a),
                                 &config, m, n, &resources, &m);
  if (prepared != PG_OK) {
    release_classifier(a, classifier);
    if (tools)
      pg_lease_release(tools);
    if (model)
      pg_lease_release(model);
    return prepared == PG_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
  }
#endif
  approval_run approval = {.app = a, .request = config.prepare_request_update,
      .next = config.prepare_next_turn_update};
  pg_status wrapped = PG_OK;
  if (a->approval != PG_APP_APPROVAL_DISABLED) {
    pangu_status status = approval_selection(&approval, &config);
    wrapped = status == PANGU_OK ? PG_OK : status == PANGU_NOMEM ? PG_NOMEM : PG_INVALID;
    if (approval.request) config.prepare_request_update = approval_prepare_request;
    if (approval.next) config.prepare_next_turn_update = approval_prepare_next;
  }
  if (wrapped != PG_OK) {
    approval_release(&approval);
#ifdef PANGU_AGENT_RESOURCES
    pg_agent_resources_release(resources);
#endif
    release_classifier(a, classifier);
    if (tools) pg_lease_release(tools);
    if (model) pg_lease_release(model);
    return wrapped == PG_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_CORE_ERROR;
  }
#ifdef PANGU_AGENT_RESOURCES
  a->active_resources = resources;
  pg_agent_resources_enter(resources);
#endif
  approval.previous = active_approval;
  active_approval = &approval;
  a->running = 1;
  pg_session_status z = SESSION_SLOT(a, run_inputs)
      ? a->service->run_inputs(pg_agent_app_session(a), m, n, &config, originals)
      : a->service->run(pg_agent_app_session(a), m, n, &config);
  a->running = 0;
  active_approval = approval.previous;
  approval_release(&approval);
#ifdef PANGU_AGENT_RESOURCES
  a->active_resources = NULL;
  pg_agent_resources_release(resources);
#endif
  if (tools)
    pg_lease_release(tools);
  if (model)
    pg_lease_release(model);
  release_classifier(a, classifier);
  return z;
}
pg_session_status pg_agent_app_run(pg_agent_app *a, const pangu_message *m,
                                   size_t n, const pangu_loop_config *c) {
  return app_run(a, m, n, c, 0);
}
static pg_session_status app_compact(pg_agent_app *a, size_t keep,
                                     const pangu_loop_config *cfg,
                                     int presentation_action) {
  if (!a || !cfg)
    return PG_SESSION_INVALID;
  if (a->running || (a->presenting && !presentation_action))
    return PG_SESSION_BUSY;
  pangu_loop_config config = *cfg;
  pg_lease *model = NULL;
  if (!config.model && !config.request && !config.request_events) {
    pg_status z = pg_service_acquire(pg_host_root(a->host),
                                     PG_NAME(PG_MODEL_SERVICE), 1, &model);
    if (z != PG_OK)
      return PG_SESSION_CORE_ERROR;
    const pg_model_service *api = pg_lease_table(model);
    config.request_events = api->request_events;
    config.has_request_ctx = 1;
    config.request_ctx = pg_lease_context(model);
  }
#ifdef PANGU_AGENT_RESOURCES
  pg_agent_resources_run *resources = NULL;
  const pangu_message *unused = NULL;
  pg_status prepared =
      pg_agent_resources_prepare(pg_agent_app_scope(a), pg_agent_app_session(a),
                                 &config, NULL, 0, &resources, &unused);
  if (prepared != PG_OK) {
    if (model)
      pg_lease_release(model);
    return prepared == PG_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
  }
#endif
#ifdef PANGU_AGENT_RESOURCES
  a->active_resources = resources;
  pg_agent_resources_enter(resources);
#endif
  a->running = 1;
  pg_session_status z =
      a->service->compact(pg_agent_app_session(a), keep, &config);
  a->running = 0;
#ifdef PANGU_AGENT_RESOURCES
  a->active_resources = NULL;
  pg_agent_resources_release(resources);
#endif
  if (model)
    pg_lease_release(model);
  return z;
}
pg_session_status pg_agent_app_compact(pg_agent_app *a, size_t keep,
                                       const pangu_loop_config *cfg) {
  return app_compact(a, keep, cfg, 0);
}
void pg_agent_app_close(pg_agent_app *a) {
  if (!a || a->running || a->presenting)
    return;
  if (a->session)
    pg_lease_release(a->session);
  if (a->host)
    pg_host_destroy(a->host);
  free(a);
}

void pg_agent_app_poll(void *ctx, pangu_core *core) {
  (void)core;
  pg_agent_app *a = ctx;
  if (a && a->view && a->presentation)
    a->view->poll(pg_lease_context(a->presentation));
}
static pg_session_status view_run(void *ctx, const pangu_message *messages,
                                  size_t count, const pangu_loop_config *loop) {
  pg_agent_app *a = ctx;
  if (!a->presenting || !a->actions_enabled)
    return PG_SESSION_BUSY;
  return app_run(a, messages, count, loop, 1);
}
static pg_session_status view_enqueue(void *ctx, pangu_queue_kind kind,
                                      const pangu_message *message) {
  pg_agent_app *a = ctx;
  if (!a->presenting || !a->actions_enabled)
    return PG_SESSION_BUSY;
  return pg_agent_app_enqueue(a, kind, message);
}
static pg_status view_reload(void *ctx) {
  pg_agent_app *a = ctx;
  if (!a->presenting || !a->actions_enabled)
    return PG_FAILED;
  return pg_agent_app_reload_resources(a);
}
static pg_session_status view_compact(void *ctx, size_t keep,
                                      const pangu_loop_config *loop) {
  pg_agent_app *a = ctx;
  if (!a->presenting || !a->actions_enabled)
    return PG_SESSION_BUSY;
  return app_compact(a, keep, loop, 1);
}
typedef struct {
  pg_tui_history_sink sink;
  void *ctx;
  pangu_tui_status status;
} app_history;
static pg_session_status history_emit(void *ctx, pangu_slice text) {
  app_history *h = ctx;
  h->status = h->sink(h->ctx, text.data, text.len);
  return h->status == PANGU_TUI_OK ? PG_SESSION_OK : PG_SESSION_INVALID;
}
static pangu_tui_status view_history(void *ctx, pg_tui_history_sink sink, void *sink_ctx) {
  pg_agent_app *a = ctx;
  if (!sink) return PANGU_TUI_INVALID;
  if (!SESSION_SLOT(a, input_history)) return PANGU_TUI_UNSUPPORTED;
  app_history h = {.sink = sink, .ctx = sink_ctx, .status = PANGU_TUI_OK};
  pg_session_status z = a->service->input_history(pg_agent_app_session(a), history_emit, &h);
  if (h.status != PANGU_TUI_OK) return h.status;
  return z == PG_SESSION_OK ? PANGU_TUI_OK : z == PG_SESSION_NOT_FOUND ? PANGU_TUI_UNSUPPORTED : z == PG_SESSION_NOMEM ? PANGU_TUI_NOMEM : PANGU_TUI_INVALID;
}
pg_status pg_agent_app_present(pg_agent_app *a, const pg_plugin *plugin,
                               const void *config, size_t size) {
  const size_t prefix = offsetof(pg_tui_config, actions);
  if (!a || !plugin || !config || size < prefix ||
      (size > prefix && size < sizeof(pg_tui_config)))
    return PG_INVALID;
  if (a->running || a->presenting)
    return PG_FAILED;
  pg_tui_config cfg = {0};
  memcpy(&cfg, config, size < sizeof(cfg) ? size : sizeof(cfg));
  static const pg_tui_actions actions = {.size = sizeof(pg_tui_actions),
      .run = view_run, .enqueue = view_enqueue, .reload_resources = view_reload,
      .compact = view_compact, .input_history = view_history};
  cfg.actions = &actions;
  cfg.actions_ctx = a;
  /* Pin only presentation here. Each action resolves providers/tools/resources
   * afresh and holds their leases through that action's synchronous return. */
  a->presenting = 1;
  pg_scope *scope = NULL;
  pg_status z = pg_plugin_install(pg_agent_app_scope(a), plugin, &cfg,
                                  sizeof(cfg), &scope);
  if (z == PG_OK)
    z = pg_service_acquire(pg_host_root(a->host), PG_NAME(PG_TUI_SERVICE),
                           PG_TUI_ABI, &a->presentation);
  if (z == PG_OK) {
    a->view = pg_lease_table(a->presentation);
    if (a->view->start(pg_lease_context(a->presentation)) != PANGU_TUI_OK)
      z = PG_FAILED;
    else {
      a->actions_enabled = 1;
      if (a->view->run(pg_lease_context(a->presentation)) != PANGU_TUI_OK)
        z = PG_FAILED;
      a->actions_enabled = 0;
    }
    a->view->stop(pg_lease_context(a->presentation));
    pg_lease_release(a->presentation);
    a->presentation = NULL;
    a->view = NULL;
  }
  if (scope)
    pg_scope_dispose(scope);
  a->presenting = 0;
  return z;
}

pg_status pg_agent_app_reload_resources(pg_agent_app *a) {
  if (!a)
    return PG_INVALID;
  if (a->running || (a->presenting && !a->actions_enabled))
    return PG_FAILED;
#ifdef PANGU_AGENT_RESOURCES
  return pg_agent_resources_reload(pg_agent_app_scope(a));
#else
  return PG_NOT_FOUND;
#endif
}
pg_status pg_agent_app_expand(pg_agent_app *a, pangu_slice input, char **out,
                              size_t *length) {
  if (!a || (a->presenting && !a->actions_enabled))
    return PG_INVALID;
#ifdef PANGU_AGENT_RESOURCES
  if (a->active_resources)
    return pg_agent_resources_expand_active(a->active_resources, input, out,
                                            length);
  return pg_agent_resources_expand(pg_agent_app_scope(a), input, out, length);
#else
  if (!out || !length || (!input.data && input.len))
    return PG_INVALID;
  *out = NULL;
  *length = 0;
  if (input.len == SIZE_MAX)
    return PG_NOMEM;
  *out = malloc(input.len + 1);
  if (!*out)
    return PG_NOMEM;
  if (input.len)
    memcpy(*out, input.data, input.len);
  (*out)[input.len] = 0;
  *length = input.len;
  return PG_OK;
#endif
}

pg_session_status pg_agent_app_enqueue(pg_agent_app *a, pangu_queue_kind kind,
                                       const pangu_message *message) {
  if (!a || !message || message->role != PANGU_USER ||
      (message->content_count && !message->content))
    return PG_SESSION_INVALID;
  if (a->presenting && !a->actions_enabled)
    return PG_SESSION_BUSY;
  size_t count = message->content_count;
  if (count > SIZE_MAX / sizeof(pangu_content) ||
      count > SIZE_MAX / sizeof(char *))
    return PG_SESSION_INVALID;
  pangu_content *content = count ? malloc(count * sizeof(*content)) : NULL;
  char **expanded = count ? calloc(count, sizeof(*expanded)) : NULL;
  if (count && (!content || !expanded)) {
    free(content);
    free(expanded);
    return PG_SESSION_NOMEM;
  }
  if (count)
    memcpy(content, message->content, count * sizeof(*content));
  pg_session_status status = PG_SESSION_OK;
  for (size_t i = 0; i < count; i++) {
    if (content[i].kind != PANGU_TEXT)
      continue;
    pg_status z = pg_agent_app_expand(a, content[i].data, &expanded[i],
                                      &content[i].data.len);
    if (z != PG_OK) {
      status = z == PG_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
      break;
    }
    content[i].data.data = expanded[i];
  }
  if (status == PG_SESSION_OK) {
    pangu_message owned = *message;
    owned.content = content;
    status = SESSION_SLOT(a, enqueue_input)
        ? a->service->enqueue_input(pg_agent_app_session(a), kind, &owned, message)
        : a->service->enqueue(pg_agent_app_session(a), kind, &owned);
  }
  for (size_t i = 0; i < count; i++)
    free(expanded[i]);
  free(expanded);
  free(content);
  return status;
}
