/* Real terminal reader -> chat -> owner completion/submit, no model timing. */
#include <pangu/tui/chat.h>
#include <pangu/tui/completion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { pangu_tui_chat *chat; unsigned count; } fixture;
static void submitted(void *ctx, const char *text, size_t length) {
  fixture *f = ctx;
  char *line = malloc(length + 64);
  if (!line) { pangu_tui_chat_request_quit(f->chat); return; }
  int prefix = snprintf(line, 64, "Accepted[%u]: ", ++f->count);
  memcpy(line + prefix, text, length);
  for (size_t i = 0; i < length; ++i)
    if (line[prefix + i] == '\n') line[prefix + i] = '|';
  pangu_tui_chat_append(f->chat, line, (size_t)prefix + length);
  free(line);
}
int main(void) {
  fixture f = {.chat = pangu_tui_chat_create()};
  pangu_tui_terminal *terminal = pangu_tui_terminal_create();
  if (!f.chat || !terminal) return 2;
  pg_tui_completion_entry entry = {"/reload", 7};
  pg_tui_completion_catalog catalog = {&entry, 1, NULL};
  pangu_tui_chat_completion(f.chat, pg_tui_complete, &catalog);
  pangu_tui_status st = pangu_tui_chat_start(f.chat, terminal, submitted, &f);
  if (!st) st = pangu_tui_chat_run(f.chat);
  pangu_tui_chat_destroy(f.chat);
  pangu_tui_terminal_destroy(terminal);
  return st ? 2 : 0;
}
