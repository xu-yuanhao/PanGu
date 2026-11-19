/* Deterministic Core + real read-only file tool. Product DeepSeek/session
 * startup is src/agent/app.c through pg_tui_plugin; no echo model here. */
#include <pangu/tui/completion.h>
#include <pangu/tui/view.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define S(x) (pangu_slice){x, sizeof(x) - 1}
typedef struct {
  pangu_tui_chat *chat;
  pangu_core *core;
  unsigned tools;
} app;
static void pump(app *a) {
  pangu_tui_chat_poll(a->chat);
  if (pangu_tui_chat_take_cancel(a->chat))
    pangu_core_cancel(a->core);
  pg_tui_input cmd;
  while (pangu_tui_chat_take_input(a->chat, &cmd)) {
    pangu_content c = {.kind = PANGU_TEXT, .data = {cmd.text, cmd.length}};
    pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
    if (pangu_core_enqueue(a->core,
                           cmd.kind == PG_TUI_FOLLOW_UP ? PANGU_FOLLOW_UP
                                                        : PANGU_STEERING,
                           &m) != PANGU_OK)
      pangu_tui_chat_append(a->chat, "Queue failed", 12);
    pangu_tui_input_release(&cmd);
  }
}
static pangu_status model(void *ctx, pangu_core *owner,
                          const pangu_core *request, pangu_model_sink sink,
                          void *target) {
  app *a = ctx;
  const pangu_message *last =
      pangu_core_message_at(request, pangu_core_message_count(request) - 1);
  if (last && last->role == PANGU_USER && last->content_count &&
      last->content[0].data.len >= 5 &&
      !memcmp(last->content[0].data.data, "/tool", 5)) {
    pangu_content call = {.kind = PANGU_TOOL_CALL,
                          .id = S("read-1"),
                          .name = S("read_readme"),
                          .data = S("{}")};
    pangu_message m = {.role = PANGU_ASSISTANT,
                       .content = &call,
                       .content_count = 1,
                       .stop_reason = PANGU_TOOL_USE};
    return sink(target, &m, 1);
  }
  const char *answer =
      last && last->role == PANGU_TOOL_RESULT
          ? "The README was read by the file tool. Its result is shown above."
          : "Deterministic Core stream. Use /tool to read README.md; type "
            "while streaming, or Ctrl+C to cancel.";
  size_t n = strlen(answer);
  for (size_t i = 0; i <= n; i += 1) {
    pump(a);
    if (pangu_core_is_cancelled(owner))
      return PANGU_CANCELLED;
    pangu_content c = {.kind = PANGU_TEXT, .data = {answer, i}};
    pangu_message m = {.role = PANGU_ASSISTANT,
                       .content = &c,
                       .content_count = 1,
                       .stop_reason = i == n ? PANGU_STOP : PANGU_PENDING,
                       .usage = {.output = i, .total = i}};
    pangu_status st = sink(target, &m, i == n);
    if (st)
      return st;
    uv_sleep(8);
  }
  return PANGU_OK;
}
static pangu_status validate(void *ctx, pangu_slice schema, pangu_slice args) {
  (void)ctx;
  (void)schema;
  return args.len == 2 && !memcmp(args.data, "{}", 2) ? PANGU_OK
                                                      : PANGU_INVALID;
}
static pangu_status readme(void *ctx, pangu_core *core,
                           const pangu_content *call, pangu_tool_sink sink,
                           void *target) {
  (void)core;
  (void)call;
  app *a = ctx;
  char text[2048];
  FILE *f = fopen("README.md", "rb");
  size_t n = 0;
  int failed = !f;
  if (f) {
    n = fread(text, 1, sizeof text, f);
    failed = ferror(f);
    fclose(f);
  } else {
    memcpy(text, "README.md unavailable", 21);
    n = 21;
  }
  pangu_content c = {.kind = PANGU_TEXT, .data = {text, n}};
  pangu_message m = {.role = PANGU_TOOL_RESULT,
                     .content = &c,
                     .content_count = 1,
                     .is_error = failed};
  a->tools++;
  return sink(target, &m, 1);
}
static void event(void *ctx, pangu_core *core, const pangu_event *e) {
  app *a = ctx;
  pg_tui_core_event(a->chat, core, e);
  pump(a);
}
static void submit(void *ctx, const char *text, size_t n) {
  app *a = ctx;
  pangu_tool tool = {
      .name = S("read_readme"),
      .description = S("Read the project README"),
      .schema_json = S("{\"type\":\"object\",\"additionalProperties\":false}"),
      .ctx = a,
      .validate = validate,
      .execute = readme};
  pangu_loop_config cfg = {.ctx = a,
                           .request = model,
                           .event = event,
                           .tools = &tool,
                           .tool_count = 1};
  pangu_content c = {.kind = PANGU_TEXT, .data = {text, n}};
  pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
  pangu_tui_chat_busy(a->chat, 1);
  pangu_status st = pangu_core_run(a->core, &m, 1, &cfg);
  pangu_tui_chat_busy(a->chat, 0);
  if (st && st != PANGU_CANCELLED)
    pangu_tui_chat_append(a->chat, "Core run failed", 15);
}
int main(void) {
  pangu_tui_terminal *terminal = pangu_tui_terminal_create();
  app a = {.chat = pangu_tui_chat_create()};
  if (!terminal || !a.chat || pangu_core_create(NULL, &a.core) != PANGU_OK) {
    fprintf(stderr, "pangu-tui: allocation failed\n");
    pangu_tui_chat_destroy(a.chat);
    pangu_tui_terminal_destroy(terminal);
    return 2;
  }
  pangu_tui_chat_completion(a.chat, pg_tui_complete, NULL);
  pangu_tui_status status = pangu_tui_chat_start(a.chat, terminal, submit, &a);
  if (status != PANGU_TUI_OK) {
    fprintf(stderr,
            "pangu-tui: interactive terminal required or terminal start failed "
            "(%d)\n",
            status);
  } else {
    pangu_tui_terminal_set_title(terminal, "PanGu");
    pangu_tui_chat_append(
        a.chat, "Deterministic demo | /tool reads README.md | product: pangu",
        sizeof("Deterministic demo | /tool reads README.md | product: pangu") -
            1);
    status = pangu_tui_chat_run(a.chat);
  }
  pangu_tui_chat_stop(a.chat);
  pangu_core_destroy(a.core);
  pangu_tui_chat_destroy(a.chat);
  pangu_tui_terminal_destroy(terminal);
  return status == PANGU_TUI_OK ? 0 : 2;
}
