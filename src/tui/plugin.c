#include <pangu/tui/completion.h>
#include <pangu/tui/plugin.h>
#include <pangu/agent/approval.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <pangu/services/model.h>
#include <pangu/services/approval.h>
#include <stdlib.h>
#include <uv.h>
/* Pi interactive-mode.ts getUserMessageText/renderInitialMessages,
 * d5b3de22 (MIT). Kept in presentation plugin to preserve the base TUI's
 * independent terminal/editor link closure. Raw provenance is a host contract. */
pangu_tui_status pg_tui_history_user_messages(const pangu_core *messages,
                                              pg_tui_history_sink sink,
                                              void *ctx) {
  if (!messages || !sink)
    return PANGU_TUI_INVALID;
  for (size_t i = 0; i < pangu_core_message_count(messages); ++i) {
    const pangu_message *m = pangu_core_message_at(messages, i);
    if (m->role != PANGU_USER)
      continue;
    size_t length = 0;
    for (size_t j = 0; j < m->content_count; ++j) {
      if (m->content[j].kind != PANGU_TEXT)
        continue;
      if (m->content[j].data.len > SIZE_MAX - length)
        return PANGU_TUI_NOMEM;
      length += m->content[j].data.len;
    }
    if (!length)
      continue;
    char *text = malloc(length);
    if (!text)
      return PANGU_TUI_NOMEM;
    size_t at = 0;
    for (size_t j = 0; j < m->content_count; ++j) {
      const pangu_content *c = &m->content[j];
      if (c->kind == PANGU_TEXT && c->data.len) {
        memcpy(text + at, c->data.data, c->data.len);
        at += c->data.len;
      }
    }
    pangu_tui_status st = sink(ctx, text, length);
    free(text);
    if (st)
      return st;
  }
  return PANGU_TUI_OK;
}
typedef struct {
  pangu_tui_chat *chat;
  pangu_tui_terminal *terminal;
  pangu_session *session;
  const pg_session_service *api;
  pangu_loop_config loop;
  pg_tui_actions actions;
  void *actions_ctx;
  uint64_t subscription;
  int running;
  pg_scope *scope;
  int context_dirty;
  uint64_t approval_id;
  pg_approval_provider *answer_provider;
  int answering, stopped;
} presentation;
#define HAS(p, slot) ((p)->api->size >= offsetof(pg_session_service, slot) + sizeof((p)->api->slot) && (p)->api->slot)
static int approval_capable(const presentation *p) {
  return HAS(p, approval_pending) && HAS(p, approval_reply) && HAS(p, approval_events);
}
/* Harness user-approval question/first-decision UI. No policy or grants live
 * in presentation; all decisions are committed by the run owner. */
static pg_session_status approval_event(void *ctx, const pg_approval_event *e) {
  presentation *p = ctx;
  if (e->id != p->approval_id || e->decided) return PG_SESSION_OK;
  pangu_tui_chat_append(p->chat, "Approval requested", 18);
  pangu_tui_chat_append(p->chat, "Tool:", 5);
  pangu_tui_chat_append(p->chat, e->question.tool_name.data, e->question.tool_name.len);
  pangu_tui_chat_append(p->chat, "Call ID:", 8);
  pangu_tui_chat_append(p->chat, e->question.call_id.data, e->question.call_id.len);
  pangu_tui_chat_append(p->chat, "Reason:", 7);
  pangu_tui_chat_append(p->chat, e->question.reason.data, e->question.reason.len);
  pangu_tui_chat_append(p->chat,
      "Approval: y allow once | n reject | Esc/Ctrl+C cancel (Enter does not allow)",
      sizeof("Approval: y allow once | n reject | Esc/Ctrl+C cancel (Enter does not allow)")-1);
  return PG_SESSION_OK;
}
static void approval_sync(presentation *p) {
  if (p->stopped || !approval_capable(p)) return;
  uint64_t id = p->api->approval_pending(p->session);
  if (p->approval_id && p->approval_id != id) {
    pangu_tui_chat_approval_close(p->chat, p->approval_id);
    p->approval_id = 0;
  }
  if (!id || p->approval_id == id) return;
  if (pangu_tui_chat_approval_open(p->chat, id) != PANGU_TUI_OK) {
    p->api->cancel(p->session);
    return;
  }
  p->approval_id = id;
  if (p->api->approval_events(p->session, approval_event, p) != PG_SESSION_OK)
    p->api->cancel(p->session);
}
static void session_event(void *ctx, pangu_session *session,
                          const pg_session_event *e) {
  (void)session;
  presentation *p = ctx;
  if (e->kind == PG_SESSION_COMMITTED) approval_sync(p);
  if (e->kind == PG_SESSION_LIVE && e->live)
    pg_tui_core_event(p->chat, NULL, e->live);
  if (e->kind == PG_SESSION_CONTEXT_CHANGED)
    p->context_dirty = 1;
  if (e->kind == PG_SESSION_CLOSED)
    pangu_tui_chat_request_quit(p->chat);
}
static void stop(void *ctx) {
  presentation *p = ctx;
  p->stopped = 1;
  pg_approval_provider_revoke(p->answer_provider);
  /* Stop never grants a latched y. Settle the live request on this owner
   * before joining the reader and releasing its context. */
  uint64_t id = approval_capable(p) ? p->api->approval_pending(p->session) : 0;
  if (id) {
    p->api->cancel(p->session);
    (void)p->api->approval_reply(p->session, id, PG_APPROVAL_CANCELLED);
  }
  if (p->approval_id) {
    pangu_tui_chat_approval_close(p->chat, p->approval_id);
    p->approval_id = 0;
  }
  pangu_tui_chat_stop(p->chat);
}
static void cleanup(void *ctx) {
  presentation *p = ctx;
  stop(p);
  if (p->subscription)
    p->api->unsubscribe(p->session, p->subscription);
  pangu_tui_chat_destroy(p->chat);
  pangu_tui_terminal_destroy(p->terminal);
  free(p);
}
static pangu_tui_chat *chat(void *ctx) { return ((presentation *)ctx)->chat; }
static void rebuild(presentation *p) {
  pangu_tui_chat_clear(p->chat);
  if (HAS(p, leaf)) {
    char b[96];
    int n = snprintf(b, sizeof b, "Current leaf: %llu", (unsigned long long)p->api->leaf(p->session));
    pangu_tui_chat_append(p->chat, b, (size_t)n);
  }
  const pangu_core *messages = p->api->messages(p->session);
  for (size_t i = 0; i < pangu_core_message_count(messages); i++)
    pg_tui_replay_message(p->chat, pangu_core_message_at(messages, i));
  p->context_dirty = 0;
}
static pangu_tui_status start(void *ctx) {
  presentation *p = ctx;
  /* Pi renderInitialMessages populates history once; ordinary transcript
   * rebuilds do not repopulate it. Session USER bodies alone cannot certify
   * original input after an app resource driver has persisted expansions. */
  pangu_tui_status st = PANGU_TUI_OK;
  if (p->actions.input_history) {
    st = pangu_tui_chat_import_history(p->chat, p->actions.input_history,
                                       p->actions_ctx);
    if (st != PANGU_TUI_OK && st != PANGU_TUI_UNSUPPORTED)
      return st;
  }
  st = pangu_tui_chat_start(p->chat, p->terminal, NULL, NULL);
  if (st)
    return st;
  rebuild(p);
  pangu_tui_chat_append(p->chat, "/tree /switch ID /branch ID /reset /handoff TEXT /compact [keep tokens] /usage",
                        sizeof("/tree /switch ID /branch ID /reset /handoff TEXT /compact [keep tokens] /usage") - 1);
  if (p->actions.reload_resources)
    pangu_tui_chat_append(p->chat,
      "/reload refreshes settings, templates and skills; /NAME or /skill:NAME submits a resource",
      sizeof("/reload refreshes settings, templates and skills; /NAME or /skill:NAME submits a resource") - 1);
  return PANGU_TUI_OK;
}
static void diagnostic(presentation *p, const char *prefix, int code) {
  char s[128];
  int n = snprintf(s, sizeof s, "%s (%d)", prefix, code);
  if (n > 0)
    pangu_tui_chat_append(p->chat, s, (size_t)n);
}
static void notice(presentation *p, const char *s) {
  pangu_tui_chat_append(p->chat, s, strlen(s));
}
static int number(const char *s, size_t n, uint64_t *out) {
  if (!n) return 0;
  uint64_t v = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9' || v > (UINT64_MAX - (unsigned)(s[i]-'0')) / 10) return 0;
    v = v * 10 + (unsigned)(s[i]-'0');
  }
  *out = v;
  return 1;
}
/* Recognized commands never become model input, including malformed/busy ones.
 * Session mutations require idle; summary polling only drains cancellation. */
static int command(presentation *p, const pg_tui_input *cmd) {
  size_t n = 0;
  while (n < cmd->length && cmd->text[n] != ' ' && cmd->text[n] != '\t' && cmd->text[n] != '\n') n++;
  const char *arg = cmd->text + n;
  size_t len = cmd->length - n;
  while (len && (*arg == ' ' || *arg == '\t')) { arg++; len--; }
#define IS(s) (n == sizeof(s)-1 && !memcmp(cmd->text, s, n))
  if (IS("/reload")) {
    if (p->running) {
      notice(p, "Reload requires idle; cancel or wait, then retry");
    } else if (len) {
      notice(p, "Usage: /reload");
    } else if (!p->actions.reload_resources) {
      notice(p, "Resource reload is unavailable");
    } else {
      pg_status z = p->actions.reload_resources(p->actions_ctx);
      if (z == PG_OK)
        notice(p, "Resources reloaded; next input uses refreshed settings, templates and skills");
      else if (z == PG_NOT_FOUND)
        notice(p, "Resource reload is unavailable");
      else
        diagnostic(p, "Resource reload failed; previous snapshot retained", z);
    }
    return 1;
  }
  int tree = IS("/tree"), sw = IS("/switch") || IS("/branch");
  int reset = IS("/reset") || IS("/handoff"), compact = IS("/compact"), usage = IS("/usage");
  if (!(tree || sw || reset || compact || usage)) return 0;
  if (p->running) {
    notice(p, "Session command requires idle; cancel or wait, then retry");
    return 1;
  }
  pg_session_status st = PG_SESSION_OK;
  uint64_t value = compact ? 20000 : 0;
  if (tree) {
    if (!HAS(p, entry_count) || !HAS(p, entry_at) || !HAS(p, leaf)) goto unavailable;
    if (len) goto syntax;
    notice(p, "Session tree: /switch ID (0 = root); /branch ID then send to fork");
    for (size_t i = 0; i < p->api->entry_count(p->session); i++) {
      pg_session_entry e;
      st = p->api->entry_at(p->session, i, &e);
      if (st) break;
      char b[160];
      int k = snprintf(b, sizeof b, "%c id=%llu parent=%llu %s", e.id == p->api->leaf(p->session) ? '*' : ' ',
        (unsigned long long)e.id, (unsigned long long)e.parent,
        e.kind == PG_ENTRY_RESET ? "reset" : e.kind == PG_ENTRY_COMPACTION ? "compaction" :
        e.message && e.message->role == PANGU_USER ? "user" : "message");
      pangu_tui_chat_append(p->chat, b, (size_t)k);
    }
  } else if (sw) {
    if (!HAS(p, switch_to)) goto unavailable;
    if (!number(arg, len, &value)) goto syntax;
    st = p->api->switch_to(p->session, value);
  } else if (reset) {
    if (!HAS(p, reset)) goto unavailable;
    st = p->api->reset(p->session, (pangu_slice){arg, len});
  } else if (usage) {
    if (!HAS(p, usage)) goto unavailable;
    if (len) goto syntax;
    pangu_usage u = p->api->usage(p->session);
    char b[180];
    int k = snprintf(b, sizeof b, "Archived usage: input=%llu output=%llu total=%llu",
      (unsigned long long)u.input, (unsigned long long)u.output, (unsigned long long)u.total);
    pangu_tui_chat_append(p->chat, b, (size_t)k);
  } else if (compact) {
    if (!p->actions.compact && !HAS(p, compact)) goto unavailable;
    /* Pi default retention budget; explicit zero is useful for small contexts. */
    if (len && (!number(arg, len, &value) || value > SIZE_MAX)) goto syntax;
    pangu_loop_config loop = p->loop;
    pg_lease *lease = NULL;
    if (!p->actions.compact && !loop.model && !loop.request && !loop.request_events) {
      pg_status z = pg_service_acquire(p->scope, PG_NAME(PG_MODEL_SERVICE), 1, &lease);
      if (z != PG_OK) {
        diagnostic(p, "Compaction requires an available pangu.model provider", z);
        return 1;
      }
      const pg_model_service *model = pg_lease_table(lease);
      if (!model || !model->request_events) {
        pg_lease_release(lease);
        notice(p, "Compaction model has no request_events capability");
        return 1;
      }
      loop.request_events = model->request_events;
      loop.has_request_ctx = 1;
      loop.request_ctx = pg_lease_context(lease);
    }
    p->running = 1;
    pangu_tui_chat_busy(p->chat, 1);
    pangu_tui_chat_settling(p->chat);
    notice(p, "Compacting context; Ctrl+C cancels, new input waits");
    st = p->actions.compact
      ? p->actions.compact(p->actions_ctx, (size_t)value, &loop)
      : p->api->compact(p->session, (size_t)value, &loop);
    p->running = 0;
    pangu_tui_chat_busy(p->chat, 0);
    if (lease) pg_lease_release(lease);
    notice(p, st == PG_SESSION_CANCELLED ? "Compaction cancelled; context unchanged" :
      st == PG_SESSION_OK ? "Compaction complete" : "Compaction failed; context unchanged");
  }
  if (p->context_dirty) rebuild(p);
  if (st != PG_SESSION_OK) {
    const char *why = st == PG_SESSION_INVALID ? "Invalid arguments or no compressible prefix" :
      st == PG_SESSION_READ_ONLY ? "Session is read-only" :
      st == PG_SESSION_BUSY ? "Session is busy or has an outstanding transaction" :
      st == PG_SESSION_NOT_FOUND ? "Session entry not found" :
      st == PG_SESSION_CANCELLED ? "Session operation cancelled" :
      st == PG_SESSION_IO ? "Session persistence failed" : "Session operation failed";
    diagnostic(p, why, st);
  }
  return 1;
unavailable:
  notice(p, "Session service does not provide this command capability");
  return 1;
syntax:
  notice(p, "Usage: /tree | /switch ID | /branch ID | /reset [handoff] | /handoff [text] | /compact [keep tokens] | /usage");
  return 1;
#undef IS
}
static void poll(void *ctx) {
  presentation *p = ctx;
  approval_sync(p);
  if (p->answering) return;
  if (p->approval_id) {
    uint64_t id = p->approval_id;
    int choice = pangu_tui_chat_approval_choice(p->chat, id);
    if (choice) {
      pg_approval_outcome outcome = choice == 1 ? PG_APPROVAL_ALLOWED_ONCE
                                 : choice == 2 ? PG_APPROVAL_REJECTED
                                               : PG_APPROVAL_CANCELLED;
      if (choice == 3) p->api->cancel(p->session);
      pg_session_status z = p->api->approval_reply(p->session, id, outcome);
      if (z != PG_SESSION_OK && z != PG_SESSION_NOT_FOUND) {
        p->api->cancel(p->session);
        diagnostic(p, "Approval reply failed", z);
      }
      approval_sync(p);
    }
    /* Do not transfer pre-approval commands to the waiting driver. */
    return;
  }
  /* chat retains post-cancel/end inputs until run returns and busy clears. */
  if (pangu_tui_chat_take_cancel(p->chat))
    p->api->cancel(p->session);
  pangu_tui_chat_poll(p->chat);
  if (p->context_dirty && !p->running) rebuild(p);
  pg_tui_input cmd;
  /* One idle run per poll prevents recursive follow-up runs. Nested provider
   * polls see running=1 and transfer queue commands to the session driver. */
  while (pangu_tui_chat_take_input(p->chat, &cmd)) {
    if (command(p, &cmd)) {
      pangu_tui_input_release(&cmd);
      if (!p->running) break;
      continue;
    }
    pangu_content text = {.kind = PANGU_TEXT, .data = {cmd.text, cmd.length}};
    pangu_message m = {
        .role = PANGU_USER, .content = &text, .content_count = 1};
    pg_session_status st;
    if (p->running) {
      pangu_queue_kind kind =
          cmd.kind == PG_TUI_FOLLOW_UP ? PANGU_FOLLOW_UP : PANGU_STEERING;
      st = p->actions.enqueue
        ? p->actions.enqueue(p->actions_ctx, kind, &m)
        : p->api->enqueue(p->session, kind, &m);
    } else {
      if (pangu_tui_chat_quitting(p->chat)) {
        pangu_tui_input_release(&cmd);
        break;
      }
      p->running = 1;
      pangu_tui_chat_busy(p->chat, 1);
      st = p->actions.run
        ? p->actions.run(p->actions_ctx, &m, 1, &p->loop)
        : p->api->run(p->session, &m, 1, &p->loop);
      p->running = 0;
      pangu_tui_chat_busy(p->chat, 0);
    }
    pangu_tui_input_release(&cmd);
    if (st != PG_SESSION_OK && st != PG_SESSION_CANCELLED)
      diagnostic(p, "Session operation failed", st);
    if (!p->running)
      break;
  }
}
static pangu_tui_status run(void *ctx) {
  presentation *p = ctx;
  while (!pangu_tui_chat_quitting(p->chat)) {
    poll(p);
    uv_sleep(10);
  }
  return pangu_tui_terminal_result(p->terminal);
}
/* services callback owns no retained question/core/provider data. The durable
 * asked event opened the modal before dispatch; only this callback translates
 * its keyboard latch to the separate, ephemeral provider reply namespace. */
static pg_status answer_service(void *ctx, pg_approval_provider *provider,
    uint64_t id, pangu_core *core, const pg_service_approval_question *q,
    pg_service_approval_outcome *out) {
  presentation *p = ctx;
  (void)q; /* readonly fields already rendered from committed asked */
  *out = PG_SERVICE_UNAVAILABLE;
  if (p->stopped || p->answering || !p->approval_id) return PG_OK;
  uint64_t modal = p->approval_id;
  p->answering = 1;
  for (;;) {
    int choice = pangu_tui_chat_approval_choice(p->chat, modal);
    if (p->stopped || pangu_tui_chat_quitting(p->chat) ||
        (core && pangu_core_is_cancelled(core))) choice = 3;
    if (choice) {
      if (choice == 3) p->api->cancel(p->session);
      pg_service_approval_outcome decision = choice == 1 ? PG_SERVICE_ALLOWED_ONCE
        : choice == 2 ? PG_SERVICE_REJECTED : PG_SERVICE_CANCELLED;
      pg_status z = pg_approval_provider_reply(provider, id, decision);
      if (z == PG_OK) *out = decision;
      break;
    }
    uv_sleep(5);
  }
  /* Quiesce before the request lease can be released or scope disposed.
   * Closing consumes any latched key; a late key cannot answer another id. */
  pangu_tui_chat_approval_close(p->chat, modal);
  p->approval_id = 0;
  p->answering = 0;
  return PG_OK;
}
static const pg_tui_service service;
pg_session_status pg_tui_approval_answer(void *ctx, pangu_session *session,
                                        uint64_t id,
                                        const pg_approval_question *question,
                                        pg_approval_outcome *out) {
  (void)question;
  if (!ctx || !session || !id || !out) return PG_SESSION_INVALID;
  *out = PG_APPROVAL_UNAVAILABLE;
  pg_lease *lease = NULL;
  if (pg_service_acquire(ctx, PG_NAME(PG_TUI_SERVICE), PG_TUI_ABI, &lease) != PG_OK)
    return PG_SESSION_OK;
  const pg_tui_service *view = pg_lease_table(lease);
  void *owner = pg_lease_context(lease);
  /* This implementation owns that context; never route another session. */
  presentation *p = owner;
  if (view == &service && p->session == session && approval_capable(p)) {
    while (p->api->approval_pending(session) == id) {
      if (pangu_tui_chat_quitting(p->chat)) {
        p->api->cancel(session);
        (void)p->api->approval_reply(session, id, PG_APPROVAL_CANCELLED);
        break;
      }
      view->poll(owner);
      if (p->api->approval_pending(session) == id) uv_sleep(5);
    }
  }
  pg_lease_release(lease);
  return PG_SESSION_OK;
}
static const pg_tui_service service = {
    PG_TUI_ABI, sizeof(pg_tui_service), start, run, poll, stop, chat};
/* Command completion is presentation-owned. Resource bodies/settings are never
 * expanded here: host completion may replace this catalog, while the driver
 * remains the sole consumer of raw submitted text. */
static char *complete(void *ctx, const char *token, size_t length, size_t *out) {
  presentation *p = ctx;
#define ENTRY(s) {s, sizeof(s) - 1}
  static const pg_tui_completion_entry commands[] = {
    ENTRY("/tree"), ENTRY("/switch"), ENTRY("/branch"), ENTRY("/reset"),
    ENTRY("/handoff"), ENTRY("/compact"), ENTRY("/usage"), ENTRY("/reload")};
#undef ENTRY
  pg_tui_completion_catalog catalog = {
    commands, sizeof(commands) / sizeof(commands[0]) - !p->actions.reload_resources, NULL};
  return pg_tui_complete(&catalog, token, length, out);
}
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  if (!config || (size != offsetof(pg_tui_config, actions) &&
                  size < sizeof(pg_tui_config)))
    return PG_INVALID;
  pg_tui_config copied = {0};
  memcpy(&copied, config, size < sizeof(copied) ? size : sizeof(copied));
  const pg_tui_config *cfg = &copied;
  pg_tui_actions actions = {0};
  if (cfg->actions) {
    size_t n = cfg->actions->size;
    if (n < offsetof(pg_tui_actions, enqueue) + sizeof(actions.enqueue))
      return PG_ABI;
    actions.run = cfg->actions->run;
    actions.enqueue = cfg->actions->enqueue;
    if (!!actions.run != !!actions.enqueue)
      return PG_INVALID;
    if (n >= offsetof(pg_tui_actions, reload_resources) + sizeof(actions.reload_resources))
      actions.reload_resources = cfg->actions->reload_resources;
    if (n >= offsetof(pg_tui_actions, compact) + sizeof(actions.compact))
      actions.compact = cfg->actions->compact;
    if (n >= offsetof(pg_tui_actions, input_history) + sizeof(actions.input_history))
      actions.input_history = cfg->actions->input_history;
  }
  const pg_lease *dependency = pg_scope_dependency(scope, 0);
  const pg_session_service *api = pg_lease_table(dependency);
  if (!api || api->abi != PG_SESSION_ABI || api->size < offsetof(pg_session_service, entry_count))
    return PG_ABI;
  presentation *p = calloc(1, sizeof *p);
  if (!p)
    return PG_NOMEM;
  p->scope = scope;
  p->session = pg_lease_context(dependency);
  p->api = api;
  p->loop = cfg->loop;
  p->actions = actions;
  p->actions_ctx = cfg->actions_ctx;
  p->chat = pangu_tui_chat_create();
  p->terminal = pangu_tui_terminal_create();
  if (!p->chat || !p->terminal) {
    pangu_tui_chat_destroy(p->chat);
    pangu_tui_terminal_destroy(p->terminal);
    free(p);
    return PG_NOMEM;
  }
  pg_session_status s =
      api->subscribe(p->session, session_event, p, &p->subscription);
  if (s) {
    cleanup(p);
    return s == PG_SESSION_NOMEM ? PG_NOMEM : PG_FAILED;
  }
  pangu_tui_chat_completion(p->chat,
                            cfg->complete ? cfg->complete : complete,
                            cfg->complete ? cfg->complete_ctx : p);
  pg_status status = pg_approval_provider_register(scope, answer_service, p,
                                                   &p->answer_provider);
  if (status) {
    cleanup(p);
    return status;
  }
  /* Deferred cleanup runs before the scope-owned provider is freed. */
  status = pg_scope_defer(scope, cleanup, p);
  if (status) {
    cleanup(p);
    return status;
  }
  return pg_service_register(scope, PG_NAME(PG_TUI_SERVICE), PG_TUI_ABI,
                             &service, p);
}
const pg_plugin *pg_tui_plugin(void) {
  static const pg_dependency dependencies[] = {
      {PG_NAME(PG_SESSION_SERVICE), PG_SESSION_ABI}};
  static const pg_plugin plugin = {PANGU_SERVICES_ABI,
                                   sizeof(pg_plugin),
                                   PG_NAME("pangu.tui.default"),
                                   dependencies,
                                   1,
                                   apply};
  return &plugin;
}
