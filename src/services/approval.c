/* Native adaptation of Harness 22ddee3792402c3e70ec5f86a63c3e3e895a2f60
 * interaction/user-approval: per-action grant, normalized fail-closed outcomes.
 * Audit/commit remains the Agent bridge's responsibility. */
#include <pangu/services/approval.h>
#include <stdlib.h>
#include <string.h>
typedef struct question_live {
  struct question_live *previous;
  uint64_t id;
  int decided;
  pg_service_approval_outcome outcome;
} question_live;
struct pg_approval_provider {
  pg_approval_service table;
  pg_service_answer_fn answer;
  void *context;
  uint64_t next;
  int revoked;
  question_live *live;
};
static int valid(pg_service_approval_outcome o) {
  return o >= PG_SERVICE_ALLOWED_ONCE && o <= PG_SERVICE_UNAVAILABLE;
}
static int slice_valid(pangu_slice s) { return s.data || !s.len; }
static pg_status request(void *ctx, pangu_core *core,
    const pg_service_approval_question *q, pg_service_approval_outcome *out) {
  pg_approval_provider *p = ctx;
  if (!out) return PG_INVALID;
  *out = PG_SERVICE_UNAVAILABLE;
  if (!q || !q->tool_name.len || !slice_valid(q->tool_name) || !slice_valid(q->call_id) ||
      !slice_valid(q->reason)) return PG_INVALID;
  if (core && pangu_core_is_cancelled(core)) {
    *out = PG_SERVICE_CANCELLED; return PG_OK;
  }
  if (p->revoked || !p->answer || p->next == UINT64_MAX) return PG_OK;
  question_live live = {p->live, ++p->next, 0, PG_SERVICE_UNAVAILABLE};
  p->live = &live;
  pg_service_approval_outcome answer = PG_SERVICE_UNAVAILABLE;
  pg_status status = p->answer(p->context, p, live.id, core, q, &answer);
  p->live = live.previous;
  if (core && pangu_core_is_cancelled(core)) *out = PG_SERVICE_CANCELLED;
  else if (p->revoked || status != PG_OK || !valid(answer)) *out = PG_SERVICE_UNAVAILABLE;
  else *out = live.decided ? live.outcome : valid(answer) ? answer : PG_SERVICE_UNAVAILABLE;
  return PG_OK;
}
static void cleanup(void *p) { free(p); }
pg_status pg_approval_provider_register(pg_scope *scope, pg_service_answer_fn fn,
    void *ctx, pg_approval_provider **out) {
  if (!out) return PG_INVALID;
  *out = NULL;
  if (!scope || !fn) return PG_INVALID;
  pg_approval_provider *p = calloc(1, sizeof(*p));
  if (!p) return PG_NOMEM;
  p->table = (pg_approval_service){sizeof(pg_approval_service), request};
  p->answer = fn; p->context = ctx;
  pg_status st = pg_scope_defer(scope, cleanup, p);
  if (st) { free(p); return st; }
  st = pg_service_register(scope, PG_NAME(PG_APPROVAL_SERVICE), 1, &p->table, p);
  if (!st) *out = p;
  return st;
}
void pg_approval_provider_revoke(pg_approval_provider *p) {
  if (!p) return;
  p->revoked = 1;
  for (question_live *q = p->live; q; q = q->previous) {
    q->decided = 1; q->outcome = PG_SERVICE_UNAVAILABLE;
  }
}
pg_status pg_approval_provider_reply(pg_approval_provider *p, uint64_t id,
    pg_service_approval_outcome outcome) {
  if (!p || !valid(outcome)) return PG_INVALID;
  if (p->revoked) return PG_NOT_FOUND;
  for (question_live *q = p->live; q; q = q->previous) {
    if (q->id != id) continue;
    if (q->decided) return PG_NOT_FOUND;
    q->decided = 1; q->outcome = outcome;
    return PG_OK;
  }
  return PG_NOT_FOUND;
}
pg_status pg_approval_ask(pg_scope *scope, pangu_core *core,
    const pg_service_approval_question *q, pg_service_approval_outcome *out) {
  if (!out) return PG_INVALID;
  *out = PG_SERVICE_UNAVAILABLE;
  if (!scope || !q || !q->tool_name.len || !slice_valid(q->tool_name) ||
      !slice_valid(q->call_id) || !slice_valid(q->reason)) return PG_INVALID;
  if (core && pangu_core_is_cancelled(core)) {
    *out = PG_SERVICE_CANCELLED; return PG_OK;
  }
  pg_lease *lease = NULL;
  pg_status st = pg_service_acquire(scope, PG_NAME(PG_APPROVAL_SERVICE), 1, &lease);
  if (st == PG_NOT_FOUND || st == PG_CLOSED) return PG_OK;
  if (st) return st;
  const pg_approval_service *table = pg_lease_table(lease);
  pg_service_approval_outcome answer = PG_SERVICE_UNAVAILABLE;
  if (table && table->size >= sizeof(*table) && table->request) {
    st = table->request(pg_lease_context(lease), core, q, &answer);
    if (!st && valid(answer)) *out = answer;
  }
  if (core && pangu_core_is_cancelled(core)) *out = PG_SERVICE_CANCELLED;
  pg_lease_release(lease);
  return PG_OK; /* provider failure is an unavailable decision */
}

typedef struct wrapped_tools wrapped_tools;
typedef struct { wrapped_tools *owner; pangu_tool original; } wrapped_tool;
struct wrapped_tools {
  pg_tools_service service;
  pg_approval_tools_config config;
  char *reason;
  pangu_tool *tools;
  wrapped_tool *entries;
};
static pangu_status validate(void *ctx, pangu_slice schema, pangu_slice args) {
  wrapped_tool *w = ctx;
  return w->original.validate(w->original.ctx, schema, args);
}
static pangu_status prepare(void *ctx, pangu_slice args,
    pangu_arguments_sink sink, void *sc) {
  wrapped_tool *w = ctx;
  return w->original.prepare_arguments(w->original.ctx, args, sink, sc);
}
static pangu_status validate_args(void *ctx, pangu_slice schema, pangu_slice args,
    pangu_arguments_sink sink, void *sc) {
  wrapped_tool *w = ctx;
  return w->original.validate_arguments(w->original.ctx, schema, args, sink, sc);
}
static pangu_status validate_call(void *ctx,pangu_core *owner,
    const pangu_content *call,pangu_slice schema,pangu_slice args,
    pangu_arguments_sink sink,void *sc) {
  wrapped_tool *w=ctx;
  return w->original.validate_call(w->original.ctx,owner,call,schema,args,sink,sc);
}
static pangu_status authorize(wrapped_tool *w, pangu_core *core,
    const pangu_content *call, pangu_tool_sink sink, void *sc, int *allowed) {
  *allowed = 0;
  if (!call || !sink) return PANGU_INVALID;
  pg_approval_tools_config *cfg = &w->owner->config;
  pg_service_approval_outcome outcome = PG_SERVICE_UNAVAILABLE;
  pg_service_approval_question q = {w->original.name, call->id, cfg->reason};
  if (core && pangu_core_is_cancelled(core)) outcome = PG_SERVICE_CANCELLED;
  else if (cfg->policy == PG_APPROVAL_NEVER) outcome = PG_SERVICE_REJECTED;
  else if (cfg->request) {
    pg_status st = cfg->request(cfg->context, core, &q, &outcome);
    if (st || !valid(outcome)) outcome = PG_SERVICE_UNAVAILABLE;
  }
  if (core && pangu_core_is_cancelled(core)) outcome = PG_SERVICE_CANCELLED;
  if (outcome == PG_SERVICE_ALLOWED_ONCE) { *allowed = 1; return PANGU_OK; }
  const char *msg = outcome == PG_SERVICE_REJECTED ? "Tool approval rejected" :
    outcome == PG_SERVICE_CANCELLED ? "Tool approval cancelled" : "Tool approval unavailable";
  pangu_content text = {.kind = PANGU_TEXT, .data = {msg, strlen(msg)}};
  pangu_message result = {.role = PANGU_TOOL_RESULT, .content = &text,
    .content_count = 1, .tool_call_id = call->id, .tool_name = w->original.name,
    .is_error = 1};
  return sink(sc, &result, 1);
}
static pangu_status execute(void *ctx, pangu_core *core, const pangu_content *call,
    pangu_tool_sink sink, void *sc) {
  wrapped_tool *w = ctx; int allowed;
  pangu_status st = authorize(w, core, call, sink, sc, &allowed);
  return st || !allowed ? st : w->original.execute(w->original.ctx, core, call, sink, sc);
}
typedef struct { void *original; } wrapped_operation;
static pangu_status start(void *ctx, pangu_core *core, const pangu_content *call,
    pangu_tool_sink sink, void *sc, void **operation) {
  if (!operation) return PANGU_INVALID;
  *operation = NULL;
  wrapped_tool *w = ctx; int allowed;
  pangu_status st = authorize(w, core, call, sink, sc, &allowed);
  if (st || !allowed) return st;
  wrapped_operation *op = calloc(1, sizeof(*op));
  if (!op) return PANGU_NOMEM;
  st = w->original.start(w->original.ctx, core, call, sink, sc, &op->original);
  if (st && !op->original) free(op);
  else *operation = op;
  return st;
}
static void release(void *ctx, void *op) {
  wrapped_tool *w = ctx;
  if (!op) return; /* denied starts never called the source */
  wrapped_operation *held = op;
  w->original.release(w->original.ctx, held->original);
  free(held);
}
static void tools_cleanup(void *ctx) {
  wrapped_tools *w = ctx;
  free(w->reason); free(w->tools); free(w->entries); free(w);
}
typedef struct { pg_name output; const pg_approval_tools_config *config; } install_config;
static pg_status apply(pg_scope *scope, const void *data, size_t size) {
  if (!data || size != sizeof(install_config)) return PG_INVALID;
  const install_config *cfg = data;
  const pg_tools_service *source = pg_lease_table(pg_scope_dependency(scope, 0));
  if (!source || (source->count && !source->tools)) return PG_INVALID;
  wrapped_tools *w = calloc(1, sizeof(*w));
  if (!w) return PG_NOMEM;
  pg_status st = pg_scope_defer(scope, tools_cleanup, w);
  if (st) { free(w); return st; }
  w->config = *cfg->config;
  if (w->config.reason.len) {
    w->reason = malloc(w->config.reason.len);
    if (!w->reason) return PG_NOMEM;
    memcpy(w->reason, w->config.reason.data, w->config.reason.len);
    w->config.reason.data = w->reason;
  }
  if (source->count > SIZE_MAX / sizeof(*w->tools) ||
      source->count > SIZE_MAX / sizeof(*w->entries)) return PG_NOMEM;
  if (source->count) {
    w->tools = calloc(source->count, sizeof(*w->tools));
    w->entries = calloc(source->count, sizeof(*w->entries));
    if (!w->tools || !w->entries) return PG_NOMEM;
  }
  for (size_t i = 0; i < source->count; ++i) {
    pangu_tool t = source->tools[i];
    if (!t.name.len || !slice_valid(t.name) || (t.start && !t.release)) return PG_INVALID;
    for (size_t j = 0; j < i; ++j)
      if (w->tools[j].name.len == t.name.len &&
          !memcmp(w->tools[j].name.data, t.name.data, t.name.len)) return PG_INVALID;
    w->entries[i] = (wrapped_tool){w, t};
    t.ctx = &w->entries[i];
    if (t.validate) t.validate = validate;
    if (t.prepare_arguments) t.prepare_arguments = prepare;
    if (t.validate_arguments) t.validate_arguments = validate_args;
    if (t.validate_call) t.validate_call = validate_call;
    if (t.execute) t.execute = execute;
    if (t.start) t.start = start;
    if (t.release) t.release = release;
    w->tools[i] = t;
  }
  w->service = (pg_tools_service){w->tools, source->count};
  return pg_service_register(scope, cfg->output, 1, &w->service, w);
}
pg_status pg_approval_tools_install(pg_scope *parent, pg_name output, pg_name source,
    const pg_approval_tools_config *config, pg_scope **out) {
  if (!out) return PG_INVALID;
  *out = NULL;
  if (!parent || !config || !slice_valid(config->reason) ||
      (config->policy != PG_APPROVAL_ASK && config->policy != PG_APPROVAL_NEVER) ||
      !output.data || !output.size || memchr(output.data, 0, output.size) ||
      !source.data || !source.size || memchr(source.data, 0, source.size)) return PG_INVALID;
  pg_dependency dep = {source, 1};
  pg_plugin plugin = {PANGU_SERVICES_ABI, sizeof(pg_plugin),
    PG_NAME("pangu.tools.approval"), &dep, 1, apply};
  install_config cfg = {output, config};
  return pg_plugin_install(parent, &plugin, &cfg, sizeof(cfg), out);
}
