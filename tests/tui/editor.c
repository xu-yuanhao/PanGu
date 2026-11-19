#include <assert.h>
#include <pangu/tui/chat.h>
#include <pangu/tui/completion.h>
#include <pangu/tui/editor.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
static void key(pangu_tui_editor *e, const char *s) {
  assert(pg_editor_key(e, (const uint8_t *)s, strlen(s)) != PG_EDIT_ERROR);
}
static void value(pangu_tui_editor *e, const char *s) {
  size_t n;
  const char *v = pg_editor_text(e, &n);
  if (n != strlen(s) || memcmp(v, s, n < strlen(s) ? n : strlen(s)))
    fprintf(stderr, "expected <%s> got <%.*s> (%zu)\n", s, (int)n, v, n);
  assert(n == strlen(s));
  assert(!memcmp(v, s, n));
}
static void graphemes(void) {
  pangu_tui_editor *e = pg_editor_create();
  assert(e);
  assert(!pg_editor_insert(e, "你e\xcc\x81👨‍👩‍👧‍👦",
                           strlen("你e\xcc\x81👨‍👩‍👧‍👦")));
  key(e, "\x7f");
  value(e, "你e\xcc\x81");
  key(e, "\x7f");
  value(e, "你");
  key(e, "\x7f");
  value(e, "");
  key(e, "\x1a");
  value(e, "你");
  key(e, "\x1b"
         "z");
  value(e, "");
  assert(!pg_editor_insert(e, "中\r\n文\t!", strlen("中\r\n文\t!")));
  value(e, "中\n文 !");
  assert(pg_editor_insert(e, "\x1b[2J", 4) == PANGU_TUI_INVALID);
  value(e, "中\n文 !");
  assert(pg_editor_insert(e, "\xc0\xaf", 2) == PANGU_TUI_INVALID);
  assert(pg_editor_next("e\xcc\x81x", 4, 0) == 3);
  assert(pg_editor_width("中", 3) == 2);
  assert(pg_editor_width("e\xcc\x81", 3) == 1);
  pg_editor_destroy(e);
}
static void navigation(void) {
  pangu_tui_editor *e = pg_editor_create();
  assert(e);
  assert(!pg_editor_set(e, "hello\nworld", 11));
  key(e, "\x1b[D");
  key(e, "\x1b[A");
  assert(pg_editor_cursor(e) == 4);
  key(e, "\x1b[H");
  assert(pg_editor_cursor(e) == 0);
  key(e, "\x1b[1;2C");
  key(e, "\x1b[1;2C");
  assert(pg_editor_cursor(e) == 2 && pg_editor_anchor(e) == 0);
  key(e, "X");
  value(e, "Xllo\nworld");
  key(e, "\x1a");
  value(e, "hello\nworld");
  assert(pg_editor_anchor(e) == 0 && pg_editor_cursor(e) == 2);
  key(e, "\x1b"
         "z");
  value(e, "Xllo\nworld");
  key(e, "\x05");
  key(e, "\x0b");
  value(e, "Xlloworld");
  key(e, "\x19");
  value(e, "Xllo\nworld");
  pg_editor_destroy(e);
}
static void history(void) {
  pangu_tui_editor *e = pg_editor_create();
  assert(e);
  assert(!pg_editor_history_add(e, "older", 5));
  assert(!pg_editor_history_add(e, "new\nentry", 9));
  assert(!pg_editor_set(e, "draft", 5));
  key(e, "\x1b[D");
  key(e, "\x10");
  value(e, "new\nentry");
  assert(pg_editor_cursor(e) == 0);
  key(e, "\x10");
  value(e, "older");
  key(e, "\x0e");
  value(e, "new\nentry");
  key(e, "\x0e");
  value(e, "draft");
  assert(pg_editor_cursor(e) == 4);
  assert(!pg_editor_search(e, "old", 3));
  value(e, "older");
  assert(!pg_editor_complete(e, "/help", 5));
  value(e, "/help");
  assert(pg_editor_key(e, (const uint8_t *)"\r", 1) == PG_EDIT_SUBMIT);
  key(e, "\x1b[13;2u");
  value(e, "/help\n");
  pg_editor_destroy(e);
}
/* Pi history boundaries/draft undo; Codex reverse-search query lifecycle. */
static void history_search(void) {
  pangu_tui_editor *e = pg_editor_create();
  assert(e);
  const char *entries[] = {"  old 中文\nStraße  ", "new 中文", "new 中文",
                           "old 中文\nStraße", "\xe3\x80\x80"};
  for (unsigned i = 0; i < 5; ++i)
    assert(!pg_editor_history_add(e, entries[i], strlen(entries[i])));
  assert(!pg_editor_set(e, "draft\n你é", strlen("draft\n你é")));
  key(e, "\x1b[1;2D");
  size_t cursor = pg_editor_cursor(e), anchor = pg_editor_anchor(e);
  key(e, "\x10"); value(e, "old 中文\nStraße");
  key(e, "\x10"); value(e, "new 中文"); /* adjacent duplicate skipped */
  key(e, "\x10"); value(e, "old 中文\nStraße");
  key(e, "\x10"); value(e, "old 中文\nStraße"); /* oldest boundary */
  key(e, "\x0e"); key(e, "\x0e"); key(e, "\x0e");
  value(e, "draft\n你é");
  assert(pg_editor_cursor(e) == cursor && pg_editor_anchor(e) == anchor);
  key(e, "\x12");
  int matched;
  assert(pg_editor_search_query(e, NULL, &matched) && !matched);
  assert(!pg_editor_insert(e, "STRASSE", 7));
  value(e, "old 中文\nStraße");
  key(e, "\x12"); value(e, "old 中文\nStraße"); /* unique boundary */
  key(e, "\x15");
  assert(!pg_editor_insert(e, "absent", 6));
  value(e, "draft\n你é");
  key(e, "\r"); assert(pg_editor_search_query(e, NULL, NULL));
  key(e, "\x03");
  assert(!pg_editor_search_query(e, NULL, NULL));
  assert(pg_editor_cursor(e) == cursor && pg_editor_anchor(e) == anchor);
  key(e, "\x12");
  assert(!pg_editor_insert(e, "中文", strlen("中文")));
  key(e, "\x12"); value(e, "new 中文");
  key(e, "\x13"); value(e, "old 中文\nStraße");
  key(e, "\r"); /* accept to edit, not submit */
  assert(!pg_editor_search_query(e, NULL, NULL));
  key(e, "\x1a"); value(e, "draft\n你é");
  assert(pg_editor_cursor(e) == cursor && pg_editor_anchor(e) == anchor);
  key(e, "\x12");
  assert(!pg_editor_insert(e, "é", strlen("é")));
  key(e, "\x7f");
  size_t n;
  assert(pg_editor_search_query(e, &n, NULL) && n == 0);
  key(e, "\x1b");
  for (unsigned i = 0; i < 105; ++i) {
    char item[16];
    int len = snprintf(item, sizeof item, "item%u", i);
    assert(!pg_editor_history_add(e, item, (size_t)len));
  }
  for (unsigned i = 0; i < 110; ++i) key(e, "\x10");
  value(e, "item5");
  key(e, "\x1a"); value(e, "draft\n你é");
  pg_editor_destroy(e);
}
static void chat_keys(pangu_tui_chat *c, const char *s) {
  for (; *s; ++s) pangu_tui_chat_feed(c, (const uint8_t *)s, 1, 0);
}
static void chat_search(void) {
  pangu_tui_chat *c = pangu_tui_chat_create();
  assert(c);
  chat_keys(c, "older\rnewer\r");
  pg_tui_input cmd;
  for (unsigned i = 0; i < 2; ++i) {
    assert(pangu_tui_chat_take_input(c, &cmd));
    pangu_tui_input_release(&cmd);
  }
  pangu_tui_chat_busy(c, 1);
  chat_keys(c, "draft\x12old");
  /* Search owns Alt+Enter, Tab, and Ctrl+C even with an active model. */
  pangu_tui_chat_feed(c, (const uint8_t *)"\x1b\r", 2, 0);
  chat_keys(c, "\t\x03");
  assert(!pangu_tui_chat_take_cancel(c));
  assert(!pangu_tui_chat_quitting(c));
  assert(!pangu_tui_chat_take_input(c, &cmd));
  chat_keys(c, "\r");
  assert(pangu_tui_chat_take_input(c, &cmd));
  assert(cmd.kind == PG_TUI_STEER && !strcmp(cmd.text, "draft"));
  pangu_tui_input_release(&cmd);
  chat_keys(c, "\x12old\r");
  assert(!pangu_tui_chat_take_input(c, &cmd)); /* accept only */
  chat_keys(c, "!\r");
  assert(pangu_tui_chat_take_input(c, &cmd));
  assert(!strcmp(cmd.text, "older!"));
  pangu_tui_input_release(&cmd);
  pangu_tui_chat_destroy(c);
}
static void *unused;
static void feed_thread(void *arg) {
  pangu_tui_chat *c = arg;
  for (int i = 0; i < 30; i++) {
    pangu_tui_chat_feed(c, (const uint8_t *)"你好\nmultiline",
                        strlen("你好\nmultiline"), 1);
    pangu_tui_chat_feed(c, (const uint8_t *)"\r", 1, 0);
  }
}
static void mailbox(void) {
  (void)unused;
  pangu_tui_chat *c = pangu_tui_chat_create();
  assert(c);
  uv_thread_t thread;
  assert(!uv_thread_create(&thread, feed_thread, c));
  assert(!uv_thread_join(&thread));
  pg_tui_input cmd;
  unsigned count = 0;
  while (pangu_tui_chat_take_input(c, &cmd)) {
    assert(cmd.kind == PG_TUI_PROMPT);
    assert(cmd.length == strlen("你好\nmultiline"));
    assert(!memcmp(cmd.text, "你好\nmultiline", cmd.length));
    pangu_tui_input_release(&cmd);
    count++;
  }
  assert(count == 30);
  pangu_tui_chat_busy(c, 1);
  pangu_tui_chat_feed(c, (const uint8_t *)"\x03", 1, 0);
  assert(pangu_tui_chat_take_cancel(c));
  assert(!pangu_tui_chat_take_cancel(c));
  assert(!pangu_tui_chat_quitting(c));
  pangu_tui_chat_feed(c, (const uint8_t *)"steer", 5, 1);
  pangu_tui_chat_feed(c, (const uint8_t *)"\r", 1, 0);
  /* Consuming cancel must not reopen delivery to the cancelled driver. */
  assert(!pangu_tui_chat_take_input(c, &cmd));
  pangu_tui_chat_settling(c);
  assert(!pangu_tui_chat_take_input(c, &cmd));
  pangu_tui_chat_busy(c, 0);
  assert(pangu_tui_chat_take_input(c, &cmd) && cmd.kind == PG_TUI_STEER);
  pangu_tui_input_release(&cmd);
  pangu_tui_chat_busy(c, 1);
  pangu_tui_chat_feed(c, (const uint8_t *)"follow", 6, 1);
  pangu_tui_chat_feed(c, (const uint8_t *)"\x1b\r", 2, 0);
  pangu_tui_chat_settling(c);
  assert(!pangu_tui_chat_take_input(c, &cmd));
  pangu_tui_chat_busy(c, 0);
  assert(pangu_tui_chat_take_input(c, &cmd) && cmd.kind == PG_TUI_FOLLOW_UP);
  pangu_tui_input_release(&cmd);
  pangu_tui_chat_feed(c, (const uint8_t *)"\x04", 1, 0);
  assert(pangu_tui_chat_quitting(c));
  pangu_tui_chat_destroy(c);
}
static void ring_and_completion(void) {
  pangu_tui_editor *e = pg_editor_create();
  assert(e);
  assert(!pg_editor_set(e, "one two", 7));
  key(e, "\x17");
  value(e, "one ");
  key(e, "!");
  key(e, "\x17");
  value(e, "one ");
  key(e, "\x19");
  value(e, "one !");
  key(e, "\x1b"
         "y");
  value(e, "one two");
  key(e, "\x1b"
         "y");
  value(e, "one !");
  pg_editor_destroy(e);
  pg_tui_completion_entry entries[] = {{"@readme", 7}, {"/resource", 9}};
  pg_tui_completion_catalog catalog = {entries, 2, NULL};
  size_t n;
  char *result = pg_tui_complete(&catalog, "@rea", 4, &n);
  assert(result && n == 7 && !memcmp(result, "@readme", 7));
  free(result);
  result = pg_tui_complete(NULL, "/he", 3, &n);
  assert(result && n == 5 && !memcmp(result, "/help", 5));
  free(result);
  result =
      pg_tui_complete(NULL, "include/pangu/tu", strlen("include/pangu/tu"), &n);
  assert(result && n == strlen("include/pangu/tui/") &&
         !memcmp(result, "include/pangu/tui/", n));
  free(result);
}
/* Reader batches and callbacks overlap without timing assumptions. */
typedef struct {
  pangu_tui_chat *chat;
  uv_thread_t owner;
  unsigned calls;
  const char *during;
} completion_test;
static void reader_keys(void *ctx) {
  completion_test *t = ctx;
  const char *s = t->during;
  for (; *s; ++s)
    pangu_tui_chat_feed(t->chat, (const uint8_t *)s, 1, 0);
}
static char *owner_complete(void *ctx, const char *s, size_t n, size_t *len) {
  completion_test *t = ctx;
  uv_thread_t self = uv_thread_self();
  assert(uv_thread_equal(&self, &t->owner));
  ++t->calls;
  assert((n == 4 && !memcmp(s, "/rel", n)) ||
         (n == 7 && !memcmp(s, "/reload", n)) ||
         (n == 2 && !memcmp(s, "/x", n)));
  if (t->during) {
    uv_thread_t reader;
    assert(!uv_thread_create(&reader, reader_keys, t));
    assert(!uv_thread_join(&reader));
    t->during = NULL;
  }
  if (n == 2)
    return NULL;
  char *result = malloc(8);
  assert(result);
  memcpy(result, "/reload", 8);
  *len = 7;
  return result;
}
static void completion_order(void) {
  const char *keys[] = {"/rel\t\r", "/rel\t\t\r", "/rel\t!\x7f\r",
                        "/x\t\r",   "/rel\t",     "/rel\t\r"};
  for (unsigned i = 0; i < 6; ++i) {
    completion_test t = {.chat = pangu_tui_chat_create(),
                         .owner = uv_thread_self()};
    assert(t.chat);
    pangu_tui_chat_completion(t.chat, owner_complete, &t);
    if (i == 5)
      pangu_tui_chat_busy(t.chat, 1);
    t.during = keys[i];
    uv_thread_t reader;
    assert(!uv_thread_create(&reader, reader_keys, &t));
    assert(!uv_thread_join(&reader));
    assert(!t.calls); /* reader never invokes host completion */
    t.during = i == 4 ? " extra\r" : NULL;
    if (i == 5) {
      pangu_tui_chat_feed(t.chat, (const uint8_t *)"\x03", 1, 0);
      assert(pangu_tui_chat_take_cancel(t.chat));
    }
    pangu_tui_chat_poll(t.chat);
    assert(t.calls == (i == 1 ? 2u : 1u));
    pg_tui_input cmd;
    if (i == 5) {
      assert(!pangu_tui_chat_take_input(t.chat, &cmd));
      pangu_tui_chat_busy(t.chat, 0);
    }
    assert(pangu_tui_chat_take_input(t.chat, &cmd));
    const char *want = i == 3 ? "/x" : i == 4 ? "/reload extra" : "/reload";
    assert(cmd.length == strlen(want) && !memcmp(cmd.text, want, cmd.length));
    assert(cmd.kind == (i == 5 ? PG_TUI_STEER : PG_TUI_PROMPT));
    pangu_tui_input_release(&cmd);
    assert(!pangu_tui_chat_take_input(t.chat, &cmd));
    pangu_tui_chat_destroy(t.chat);
  }
}
static void search_completion_fifo(void) {
  completion_test t = {.chat = pangu_tui_chat_create(), .owner = uv_thread_self()};
  assert(t.chat);
  pangu_tui_chat_completion(t.chat, owner_complete, &t);
  /* Everything after Tab arrives before the owner callback. Search acceptance
   * must precede the next Tab and the final Enter. */
  chat_keys(t.chat, "/rel\t\r\x12reload\r\t\r");
  assert(!t.calls);
  pangu_tui_chat_poll(t.chat);
  assert(t.calls == 2);
  pg_tui_input cmd;
  for (unsigned i = 0; i < 2; ++i) {
    assert(pangu_tui_chat_take_input(t.chat, &cmd));
    assert(!strcmp(cmd.text, "/reload"));
    pangu_tui_input_release(&cmd);
  }
  assert(!pangu_tui_chat_take_input(t.chat, &cmd));
  pangu_tui_chat_destroy(t.chat);
}
typedef struct { pangu_tui_status result; int invalid; } history_source_test;
static pangu_tui_status import_source(void *ctx, pg_tui_history_sink sink,
                                      void *target) {
  history_source_test *t = ctx;
  pangu_tui_status st = sink(target, "  imported-old  ", 16);
  if (st) return st;
  if (t->invalid) {
    /* Even a buggy provider that ignores sink failure cannot adopt a prefix. */
    (void)sink(target, "\xc0", 1);
    return PANGU_TUI_OK;
  }
  st = sink(target, "imported-new", 12);
  return st ? st : t->result;
}
static void history_import(void) {
  pangu_tui_chat *c = pangu_tui_chat_create();
  assert(c);
  chat_keys(c, "local\r");
  pangu_tui_chat_feed(c, (const uint8_t *)"draft", 5, 1);
  history_source_test t = {.result=PANGU_TUI_NOMEM};
  assert(pangu_tui_chat_import_history(c, import_source, &t) == PANGU_TUI_NOMEM);
  t.result = PANGU_TUI_UNSUPPORTED;
  assert(pangu_tui_chat_import_history(c, import_source, &t) == PANGU_TUI_UNSUPPORTED);
  t.invalid = 1;
  assert(pangu_tui_chat_import_history(c, import_source, &t) == PANGU_TUI_INVALID);
  chat_keys(c, "\x10");
  chat_keys(c, "\x05!\r"); /* original local history survived all failures */
  pg_tui_input cmd;
  assert(pangu_tui_chat_take_input(c, &cmd) && !strcmp(cmd.text, "local"));
  pangu_tui_input_release(&cmd);
  assert(pangu_tui_chat_take_input(c, &cmd) && !strcmp(cmd.text, "local!"));
  pangu_tui_input_release(&cmd);
  t.invalid = 0; t.result = PANGU_TUI_OK;
  chat_keys(c, "kept");
  assert(!pangu_tui_chat_import_history(c, import_source, &t));
  chat_keys(c, "\x10\x10\x0e\x0e\r");
  assert(pangu_tui_chat_take_input(c, &cmd) && !strcmp(cmd.text, "kept"));
  pangu_tui_input_release(&cmd);
  chat_keys(c, "\x12imported-old\r");
  assert(!pangu_tui_chat_take_input(c, &cmd));
  chat_keys(c, "\r");
  assert(pangu_tui_chat_take_input(c, &cmd) && !strcmp(cmd.text, "imported-old"));
  pangu_tui_input_release(&cmd);
  pangu_tui_chat_destroy(c);
}
static void approval_modal(void) {
  pangu_tui_chat *c = pangu_tui_chat_create();
  assert(c);
  chat_keys(c, "already-queued\r");
  pangu_tui_chat_feed(c, (const uint8_t *)"draft", 5, 1);
  pangu_tui_chat_busy(c, 1);
  assert(!pangu_tui_chat_approval_open(c, 17));
  assert(pangu_tui_chat_approval_open(c, 18) == PANGU_TUI_INVALID);
  chat_keys(c, "\r\t\x12");
  pangu_tui_chat_feed(c, (const uint8_t *)"y", 1, 1);
  assert(!pangu_tui_chat_approval_choice(c, 17));
  chat_keys(c, "yn");
  assert(pangu_tui_chat_approval_choice(c, 17) == 1);
  assert(!pangu_tui_chat_approval_choice(c, 18));
  pg_tui_input cmd;
  assert(!pangu_tui_chat_take_input(c, &cmd));
  pangu_tui_chat_approval_close(c, 18);
  assert(pangu_tui_chat_approval_choice(c, 17) == 1);
  pangu_tui_chat_approval_close(c, 17);
  assert(pangu_tui_chat_take_input(c, &cmd));
  assert(!strcmp(cmd.text, "already-queued"));
  pangu_tui_input_release(&cmd);
  assert(!pangu_tui_chat_approval_open(c, 18));
  chat_keys(c, "\x03y");
  assert(pangu_tui_chat_approval_choice(c, 18) == 3);
  assert(pangu_tui_chat_take_cancel(c));
  pangu_tui_chat_approval_close(c, 18);
  pangu_tui_chat_busy(c, 0);
  chat_keys(c, "\r");
  assert(pangu_tui_chat_take_input(c, &cmd) && !strcmp(cmd.text,"draft"));
  pangu_tui_input_release(&cmd);
  assert(!pangu_tui_chat_approval_open(c, 19));
  chat_keys(c, "y");
  pangu_tui_chat_stop(c);
  assert(pangu_tui_chat_approval_choice(c, 19) == 3);
  pangu_tui_chat_destroy(c);
}
int main(void) {
  approval_modal();
  history_import();
  search_completion_fifo();
  history_search();
  chat_search();
  completion_order();
  ring_and_completion();
  graphemes();
  navigation();
  history();
  mailbox();
  puts("editor: Unicode, selection, undo/redo, multiline, history, search, "
       "completion, mailbox passed");
  return 0;
}
