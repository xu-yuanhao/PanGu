#include <pangu/tui/view.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  char *data;
  size_t length;
} text;
static int add(text *b, const char *p, size_t n) {
  if (!n)
    return 1;
  if (!p || n > SIZE_MAX - b->length - 1)
    return 0;
  char *s = realloc(b->data, b->length + n + 1);
  if (!s)
    return 0;
  memcpy(s + b->length, p, n);
  b->data = s;
  b->length += n;
  s[b->length] = 0;
  return 1;
}
static int str(text *b, const char *s) { return add(b, s, strlen(s)); }
static int message(text *b, const pangu_message *m) {
  const char *role = m->role == PANGU_USER          ? "You: "
                     : m->role == PANGU_ASSISTANT   ? "Assistant: "
                     : m->role == PANGU_TOOL_RESULT ? "Tool result: "
                     : m->role == PANGU_SYSTEM      ? "System: "
                                                    : "Message: ";
  if (!str(b, role))
    return 0;
  for (size_t i = 0; i < m->content_count; i++) {
    const pangu_content *c = m->content + i;
    if (i && !str(b, "\n"))
      return 0;
    if (c->kind == PANGU_THINKING && !str(b, "[thinking] "))
      return 0;
    if (c->kind == PANGU_TOOL_CALL) {
      if (!str(b, "[tool ") || !add(b, c->name.data, c->name.len) ||
          !str(b, "] "))
        return 0;
    }
    if (c->kind == PANGU_THINKING && c->has_redacted && c->redacted) {
      if (!str(b, "[redacted]"))
        return 0;
    } else if (c->kind == PANGU_IMAGE) {
      if (!str(b, "[image]"))
        return 0;
    } else if (!add(b, c->data.data, c->data.len))
      return 0;
  }
  if (m->is_error || m->stop_reason == PANGU_ERROR) {
    if (!str(b, "\nError: ") || !add(b, m->error.data, m->error.len))
      return 0;
  }
  if (m->stop_reason == PANGU_ABORTED && !str(b, "\n[cancelled]"))
    return 0;
  return 1;
}
static void failed(pangu_tui_chat *c) {
  static const char s[] = "Display allocation failed";
  pangu_tui_chat_append(c, s, sizeof s - 1);
}
void pg_tui_replay_message(pangu_tui_chat *c, const pangu_message *m) {
  if (!c || !m)
    return;
  text b = {0};
  if (message(&b, m))
    pangu_tui_chat_append(c, b.data, b.length);
  else
    failed(c);
  free(b.data);
}
void pg_tui_core_event(void *ctx, pangu_core *core, const pangu_event *e) {
  (void)core;
  pangu_tui_chat *chat = ctx;
  if (!chat || !e)
    return;
  text b = {0};
  int ok = 1;
  switch (e->kind) {
  case PANGU_AGENT_START:
    pangu_tui_chat_busy(chat, 1);
    return;
  case PANGU_AGENT_END:
    pangu_tui_chat_settling(chat);
    pangu_tui_chat_live(chat, "", 0);
    return;
  case PANGU_MESSAGE_START:
  case PANGU_MESSAGE_UPDATE:
    if (e->message && e->message->role == PANGU_ASSISTANT) {
      ok = message(&b, e->message);
      if (ok)
        ok = pangu_tui_chat_live(chat, b.data, b.length) == PANGU_TUI_OK;
    }
    break;
  case PANGU_MESSAGE_END:
    if (e->message) {
      if (e->message->role == PANGU_ASSISTANT)
        pangu_tui_chat_live(chat, "", 0);
      /* User submissions are shown by chat; replay is explicit on reopen. */
      if (e->message->role != PANGU_USER)
        pg_tui_replay_message(chat, e->message);
      if (e->message->role == PANGU_ASSISTANT) {
        char s[160];
        int n =
            snprintf(s, sizeof s, "Tokens: input=%llu output=%llu total=%llu",
                     (unsigned long long)e->message->usage.input,
                     (unsigned long long)e->message->usage.output,
                     (unsigned long long)e->message->usage.total);
        if (n > 0)
          pangu_tui_chat_append(chat, s, (size_t)n);
      }
    }
    break;
  case PANGU_TOOL_START:
  case PANGU_TOOL_END:
  case PANGU_TOOL_UPDATE:
    ok = str(&b, e->kind == PANGU_TOOL_START ? "Tool started: "
                 : e->kind == PANGU_TOOL_END ? "Tool finished: "
                                             : "Tool update: ");
    if (ok && e->tool_call)
      ok = add(&b, e->tool_call->name.data, e->tool_call->name.len);
    if (e->kind == PANGU_TOOL_UPDATE && e->message) {
      if (ok)
        ok = str(&b, "\n") && message(&b, e->message);
      if (ok)
        ok = pangu_tui_chat_live(chat, b.data, b.length) == PANGU_TUI_OK;
    } else {
      if (e->kind == PANGU_TOOL_END)
        pangu_tui_chat_live(chat, "", 0);
      if (ok)
        pangu_tui_chat_append(chat, b.data, b.length);
    }
    break;
  default:
    break;
  }
  if (!ok) {
    failed(chat);
  }
  free(b.data);
}
