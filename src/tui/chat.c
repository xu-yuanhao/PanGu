/* Pi chat-simple / Editor integration, d5b3de22 (MIT), native command mailbox.
 */
#define _POSIX_C_SOURCE 200809L
#include <pangu/tui/chat.h>
#include "editor_internal.h"
#include <pangu/tui/keys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>
#include <uv.h>

typedef struct command {
  pg_tui_input input;
  struct command *next;
} command;
typedef struct key_input {
  struct key_input *next;
  size_t length;
  int paste, busy;
  uint8_t data[];
} key_input;
struct pangu_tui_chat {
  uv_mutex_t mutex;
  pangu_tui_terminal *terminal;
  pangu_tui_editor *editor;
  pangu_tui_submit_fn submit;
  void *submit_ctx;
  pg_tui_complete_fn complete;
  void *complete_ctx;
  command *head, *tail;
  key_input *keys_head, *keys_tail;
  char *live;
  size_t live_length;
  int started, quit, busy, cancel, defer_input, completion;
  unsigned cursor_row, drawn_rows;
  uint64_t approval_id;
  int approval_choice;
};
static void raw(pangu_tui_chat *c, const char *s, size_t n) {
  if (c->terminal)
    pangu_tui_terminal_write(c->terminal, (const uint8_t *)s, n);
}
static void str(pangu_tui_chat *c, const char *s) { raw(c, s, strlen(s)); }
static void ansi_move(pangu_tui_chat *c, unsigned n, char direction) {
  char b[40];
  if (n) {
    int k = snprintf(b, sizeof b, "\x1b[%u%c", n, direction);
    raw(c, b, (size_t)k);
  }
}
static void erase(pangu_tui_chat *c) {
  str(c, "\r");
  ansi_move(c, c->cursor_row, 'A');
  str(c, "\x1b[J");
  c->cursor_row = 0;
  c->drawn_rows = 0;
}
typedef struct {
  size_t rows, cursor_row;
  unsigned cursor_col;
} layout;
static layout measure(const char *s, size_t n, size_t cursor, unsigned cols,
                      unsigned initial) {
  layout l = {1, 0, initial};
  size_t row = 0;
  unsigned col = initial;
  for (size_t p = 0; p < n;) {
    size_t next = pg_editor_next(s, n, p);
    unsigned w = pg_editor_width(s + p, next - p);
    if (s[p] != '\n' && col + w >= cols) {
      row++;
      col = 0;
    }
    if (p == cursor) {
      l.cursor_row = row;
      l.cursor_col = col;
    }
    if (s[p] == '\n') {
      row++;
      col = 0;
    } else
      col += w;
    p = next;
  }
  if (cursor == n) {
    l.cursor_row = row;
    l.cursor_col = col;
  }
  l.rows = row + 1;
  return l;
}
/* Draw a bounded visual-row window. Scanning keeps UTF-8/graphemes intact;
 * clipped rows produce no terminal writes, including huge paste/stream tails.
 */
static void window(pangu_tui_chat *c, const char *s, size_t n, unsigned cols,
                   unsigned initial, size_t first, size_t count, size_t lo,
                   size_t hi, const char *prefix) {
  size_t row = 0, shown = first;
  unsigned col = initial;
  if (first == 0 && prefix)
    str(c, prefix);
  for (size_t p = 0; p < n;) {
    size_t next = pg_editor_next(s, n, p);
    unsigned w = pg_editor_width(s + p, next - p);
    if (s[p] != '\n' && col + w >= cols) {
      row++;
      col = 0;
    }
    if (row >= first && row < first + count) {
      while (shown < row) {
        str(c, "\r\n");
        shown++;
      }
      if (s[p] != '\n') {
        if (p >= lo && p < hi)
          str(c, "\x1b[7m");
        raw(c, s + p, next - p);
        if (p >= lo && p < hi)
          str(c, "\x1b[0m");
      }
    }
    if (s[p] == '\n') {
      row++;
      col = 0;
    } else
      col += w;
    p = next;
  }
  while (shown + 1 < first + count) {
    str(c, "\r\n");
    shown++;
  }
}
static void render(pangu_tui_chat *c) {
  if (!c->started)
    return;
  erase(c);
  size_t n;
  const char *s = pg_editor_text(c->editor, &n);
  size_t cursor = pg_editor_cursor(c->editor);
  unsigned cols = pangu_tui_terminal_columns(c->terminal),
           rows = pangu_tui_terminal_rows(c->terminal);
  if (cols < 4)
    cols = 4;
  if (rows < 4)
    rows = 4;
  size_t query_length;
  int matched;
  const char *query = pg_editor_search_query(c->editor, &query_length, &matched);
  size_t search_rows = query ? 1 : 0;
  if (query) {
    const char *prefix = matched || !query_length ? "Search: " : "No match: ";
    /* Keep multiline queries on one status line, as Codex display_query does.
     * Only display bytes change; matching uses the original newline. */
    char *display = query_length <= (SIZE_MAX - 1) / 3
                        ? malloc(query_length * 3 + 1) : NULL;
    size_t length = 0;
    if (display) {
      for (size_t i = 0; i < query_length; ++i) {
        if (query[i] == '\n') {
          memcpy(display + length, "↵", 3);
          length += 3;
        } else
          display[length++] = query[i];
      }
      display[length] = 0;
    }
    unsigned initial = (unsigned)strlen(prefix);
    layout q = measure(display ? display : "", length, length, cols, initial);
    window(c, display ? display : "", length, cols, initial, q.rows - 1, 1,
           0, 0, prefix);
    free(display);
    str(c, "\r\n");
  }
  layout input = measure(s, n, cursor, cols, 2);
  size_t budget = rows - 2;
  if (budget > 12)
    budget = 12;
  size_t first = input.cursor_row >= budget ? input.cursor_row - budget + 1 : 0;
  size_t count = input.rows - first;
  if (count > budget)
    count = budget;
  size_t live_rows = 0;
  if (c->live_length) {
    layout live = measure(c->live, c->live_length, c->live_length, cols, 0);
    live_rows = rows - 1 - count - search_rows;
    if (live_rows > 6)
      live_rows = 6;
    if (live_rows > live.rows)
      live_rows = live.rows;
    window(c, c->live, c->live_length, cols, 0, live.rows - live_rows,
           live_rows, 0, 0, NULL);
    if (live_rows)
      str(c, "\r\n");
  }
  size_t anchor = pg_editor_anchor(c->editor),
         lo = anchor < cursor ? anchor : cursor,
         hi = anchor > cursor ? anchor : cursor;
  window(c, s, n, cols, 2, first, count, lo, hi, c->busy ? "+ " : "> ");
  size_t cr = search_rows + live_rows + input.cursor_row - first,
         total = search_rows + live_rows + count;
  ansi_move(c, (unsigned)(total - 1 - cr), 'A');
  str(c, "\r");
  ansi_move(c, input.cursor_col, 'C');
  c->cursor_row = (unsigned)cr;
  c->drawn_rows = (unsigned)total;
}
static void append(pangu_tui_chat *c, const char *s, size_t n) {
  if (!c->started)
    return;
  erase(c);
  /* Never interpret model/tool output as terminal instructions. */
  for (size_t p = 0; p < n;) {
    utf8proc_int32_t cp;
    utf8proc_ssize_t k = utf8proc_iterate((const utf8proc_uint8_t *)s + p,
                                          (utf8proc_ssize_t)(n - p), &cp);
    if (k <= 0) {
      str(c, "?");
      p++;
      continue;
    }
    if (cp == '\n')
      str(c, "\r\n");
    else if (cp == '\t')
      str(c, "    ");
    else if (cp >= 32 && !(cp >= 127 && cp < 160))
      raw(c, s + p, (size_t)k);
    else if (cp != '\r')
      str(c, "?");
    p += (size_t)k;
  }

  str(c, "\r\n");
  render(c);
}
static int queue(pangu_tui_chat *c, pg_tui_input_kind kind) {
  size_t n;
  const char *s = pg_editor_text(c->editor, &n);
  if (!n)
    return 1;
  if (n == 5 && !memcmp(s, "/quit", 5)) {
    c->quit = c->cancel = 1;
    return 1;
  }
  if (n == 5 && !memcmp(s, "/help", 5)) {
    append(c,
           "Enter send/steer | Alt+Enter follow-up | Shift+Enter newline | "
           "Ctrl+C cancel | Ctrl+D quit | Ctrl+Z undo | Alt+Z redo | Ctrl+P/N "
           "history | Ctrl+R search | Tab complete",
           sizeof("Enter send/steer | Alt+Enter follow-up | Shift+Enter "
                  "newline | Ctrl+C cancel | Ctrl+D quit | Ctrl+Z undo | Alt+Z "
                  "redo | Ctrl+P/N history | Ctrl+R search | Tab complete") -
               1);
    pg_editor_set(c->editor, "", 0);
    return 1;
  }
  command *q = calloc(1, sizeof *q);
  if (!q)
    return 0;
  q->input.text = malloc(n + 1);
  if (!q->input.text) {
    free(q);
    return 0;
  }
  memcpy(q->input.text, s, n);
  q->input.text[n] = 0;
  q->input.length = n;
  q->input.kind = kind;
  if (pg_editor_history_add(c->editor, s, n) != PANGU_TUI_OK) {
    free(q->input.text);
    free(q);
    return 0;
  }
  if (c->tail)
    c->tail->next = q;
  else
    c->head = q;
  c->tail = q;
  append(c, s, n);
  pg_editor_set(c->editor, "", 0);
  return 1;
}
static void feed_locked(pangu_tui_chat *c, const uint8_t *d, size_t n,
                        int paste, int submission_busy) {
  if (!n && !paste) {
    c->quit = c->cancel = 1;
    return;
  }
  erase(c);
  pg_edit_action action;
  if (paste)
    action = pg_editor_insert(c->editor, (const char *)d, n) == PANGU_TUI_OK
                 ? PG_EDIT_CHANGED
                 : PG_EDIT_ERROR;
  else if (!pg_editor_search_query(c->editor, NULL, NULL) &&
           pangu_tui_matches_key(d, n, "alt+enter")) {
    action = queue(c, PG_TUI_FOLLOW_UP) ? PG_EDIT_CHANGED : PG_EDIT_ERROR;
  } else {
    action = pg_editor_key(c->editor, d, n);
    if (action == PG_EDIT_SUBMIT) {
      if (!queue(c, submission_busy ? PG_TUI_STEER : PG_TUI_PROMPT))
        action = PG_EDIT_ERROR;
    } else if (action == PG_EDIT_CANCEL) {
      if (c->busy)
        c->cancel = c->defer_input = 1;
      else if (!pangu_tui_matches_key(d, n, "escape"))
        c->quit = 1;
    } else if (action == PG_EDIT_EOF)
      c->quit = c->cancel = 1;
    else if (action == PG_EDIT_COMPLETE)
      c->completion = 1;
  }
  if (action == PG_EDIT_ERROR)
    append(
        c, "Input rejected (invalid UTF-8/control or allocation failure)",
        sizeof("Input rejected (invalid UTF-8/control or allocation failure)") -
            1);
  render(c);
}
pangu_tui_status pangu_tui_chat_approval_open(pangu_tui_chat *c, uint64_t id) {
  if (!c || !id) return PANGU_TUI_INVALID;
  uv_mutex_lock(&c->mutex);
  pangu_tui_status st = c->approval_id && c->approval_id != id
                           ? PANGU_TUI_INVALID : PANGU_TUI_OK;
  if (!st && !c->approval_id) {
    c->approval_id = id;
    c->approval_choice = c->quit ? 3 : 0;
  }
  uv_mutex_unlock(&c->mutex);
  return st;
}
int pangu_tui_chat_approval_choice(pangu_tui_chat *c, uint64_t id) {
  if (!c) return 0;
  uv_mutex_lock(&c->mutex);
  int choice = c->approval_id == id ? c->approval_choice : 0;
  uv_mutex_unlock(&c->mutex);
  return choice;
}
void pangu_tui_chat_approval_close(pangu_tui_chat *c, uint64_t id) {
  if (!c) return;
  uv_mutex_lock(&c->mutex);
  if (c->approval_id == id) {
    c->approval_id = 0;
    c->approval_choice = 0;
  }
  uv_mutex_unlock(&c->mutex);
}
typedef struct {
  pangu_tui_editor *editor;
  pangu_tui_status status;
} history_import;
static pangu_tui_status history_entry(void *ctx, const char *text, size_t n) {
  history_import *h = ctx;
  if (!h->status)
    h->status = pg_editor_history_add(h->editor, text, n);
  return h->status;
}
pangu_tui_status pangu_tui_chat_import_history(pangu_tui_chat *c,
                                               pg_tui_history_source source,
                                               void *ctx) {
  if (!c || !source)
    return PANGU_TUI_INVALID;
  uv_mutex_lock(&c->mutex);
  int started = c->started;
  uv_mutex_unlock(&c->mutex);
  if (started)
    return PANGU_TUI_INVALID;
  history_import h = {.editor = pg_editor_create()};
  if (!h.editor)
    return PANGU_TUI_NOMEM;
  pangu_tui_status st = source(ctx, history_entry, &h);
  if (h.status)
    st = h.status;
  if (!st) {
    uv_mutex_lock(&c->mutex);
    st = c->started ? PANGU_TUI_INVALID
                    : pg_editor_take_history(c->editor, h.editor);
    uv_mutex_unlock(&c->mutex);
  }
  pg_editor_destroy(h.editor);
  return st;
}
void pangu_tui_chat_feed(pangu_tui_chat *c, const uint8_t *d, size_t n,
                         int paste) {
  if (!c || (!d && n))
    return;
  uv_mutex_lock(&c->mutex);
  if (c->approval_id) {
    if (!paste && !n) {
      c->approval_choice = 3;
      c->quit = c->cancel = 1;
    } else if (!paste && !c->approval_choice) {
      if (pangu_tui_matches_key(d, n, "ctrl+c") ||
          pangu_tui_matches_key(d, n, "escape")) {
        c->approval_choice = 3;
        c->cancel = c->defer_input = 1;
      } else if (pangu_tui_matches_key(d, n, "y"))
        c->approval_choice = 1;
      else if (pangu_tui_matches_key(d, n, "n"))
        c->approval_choice = 2;
    }
    uv_mutex_unlock(&c->mutex);
    return;
  }
  /* Keep editing after Tab behind its owner-thread callback. Cancellation
   * bypasses the barrier so a blocked provider can still be interrupted. */
  int urgent = !paste && (!n || pangu_tui_matches_key(d, n, "ctrl+c") ||
                          pangu_tui_matches_key(d, n, "escape"));
  if ((c->completion || c->keys_head) && !urgent) {
    key_input *key =
        n > SIZE_MAX - sizeof(*key) ? NULL : malloc(sizeof(*key) + n);
    if (key) {
      key->next = NULL;
      key->length = n;
      key->paste = paste;
      key->busy = c->busy;
      if (n)
        memcpy(key->data, d, n);
      if (c->keys_tail)
        c->keys_tail->next = key;
      else
        c->keys_head = key;
      c->keys_tail = key;
    } else {
      append(c, "Input mailbox allocation failed", 31);
      /* Never submit a truncated command after losing an editing key. */
      c->quit = c->cancel = 1;
    }
  } else
    feed_locked(c, d, n, paste, c->busy);
  uv_mutex_unlock(&c->mutex);
}
static void input(void *ctx, const uint8_t *d, size_t n) {
  if (n >= 12 && !memcmp(d, "\x1b[200~", 6) &&
      !memcmp(d + n - 6, "\x1b[201~", 6))
    pangu_tui_chat_feed(ctx, d + 6, n - 12, 1);
  else
    pangu_tui_chat_feed(ctx, d, n, 0);
}
static void resize(void *ctx) {
  pangu_tui_chat *c = ctx;
  uv_mutex_lock(&c->mutex);
  erase(c);
  render(c);
  uv_mutex_unlock(&c->mutex);
}
pangu_tui_chat *pangu_tui_chat_create(void) {
  pangu_tui_chat *c = calloc(1, sizeof *c);
  if (!c)
    return NULL;
  if (uv_mutex_init(&c->mutex)) {
    free(c);
    return NULL;
  }
  c->editor = pg_editor_create();
  if (!c->editor) {
    uv_mutex_destroy(&c->mutex);
    free(c);
    return NULL;
  }
  return c;
}
void pangu_tui_input_release(pg_tui_input *i) {
  if (i) {
    free(i->text);
    memset(i, 0, sizeof *i);
  }
}
void pangu_tui_chat_destroy(pangu_tui_chat *c) {
  if (!c)
    return;
  pangu_tui_chat_stop(c);
  command *q = c->head;
  while (q) {
    command *next = q->next;
    pangu_tui_input_release(&q->input);
    free(q);
    q = next;
  }
  while (c->keys_head) {
    key_input *next = c->keys_head->next;
    free(c->keys_head);
    c->keys_head = next;
  }
  free(c->live);
  pg_editor_destroy(c->editor);
  uv_mutex_destroy(&c->mutex);
  free(c);
}
pangu_tui_status pangu_tui_chat_start(pangu_tui_chat *c, pangu_tui_terminal *t,
                                      pangu_tui_submit_fn fn, void *ctx) {
  if (!c || !t || c->started)
    return PANGU_TUI_INVALID;
  c->terminal = t;
  c->submit = fn;
  c->submit_ctx = ctx;
  c->quit = 0;
  c->started = 1;
  pangu_tui_status st = pangu_tui_terminal_start(t, input, c, resize, c);
  if (st) {
    c->started = 0;
    c->terminal = NULL;
    return st;
  }
  pangu_tui_chat_append(c, "PanGu | /help shortcuts | /quit exit",
                        sizeof("PanGu | /help shortcuts | /quit exit") - 1);
  return st;
}
void pangu_tui_chat_stop(pangu_tui_chat *c) {
  if (!c)
    return;
  if (c->terminal)
    pangu_tui_terminal_stop(c->terminal);
  uv_mutex_lock(&c->mutex);
  c->started = 0;
  c->quit = 1;
  if (c->approval_id) c->approval_choice = 3;
  c->terminal = NULL;
  c->submit = NULL;
  c->complete = NULL;
  uv_mutex_unlock(&c->mutex);
}
void pangu_tui_chat_request_quit(pangu_tui_chat *c) {
  if (!c)
    return;
  uv_mutex_lock(&c->mutex);
  c->quit = c->cancel = 1;
  if (c->approval_id) c->approval_choice = 3;
  uv_mutex_unlock(&c->mutex);
}
int pangu_tui_chat_quitting(pangu_tui_chat *c) {
  if (!c)
    return 1;
  uv_mutex_lock(&c->mutex);
  int q = c->quit;
  uv_mutex_unlock(&c->mutex);
  return q;
}
void pangu_tui_chat_busy(pangu_tui_chat *c, int busy) {
  if (!c)
    return;
  uv_mutex_lock(&c->mutex);
  c->busy = busy;
  if (!busy)
    c->defer_input = 0;
  erase(c);
  render(c);
  uv_mutex_unlock(&c->mutex);
}
void pangu_tui_chat_settling(pangu_tui_chat *c) {
  if (!c)
    return;
  uv_mutex_lock(&c->mutex);
  c->defer_input = 1;
  uv_mutex_unlock(&c->mutex);
}
int pangu_tui_chat_take_cancel(pangu_tui_chat *c) {
  if (!c)
    return 0;
  uv_mutex_lock(&c->mutex);
  int v = c->cancel;
  c->cancel = 0;
  uv_mutex_unlock(&c->mutex);
  return v;
}
int pangu_tui_chat_take_input(pangu_tui_chat *c, pg_tui_input *out) {
  if (!c || !out)
    return 0;
  uv_mutex_lock(&c->mutex);
  command *q = c->approval_id || (c->busy && (c->cancel || c->defer_input))
                   ? NULL : c->head;
  if (q) {
    c->head = q->next;
    if (!c->head)
      c->tail = NULL;
    *out = q->input;
    free(q);
  }
  uv_mutex_unlock(&c->mutex);
  return q != NULL;
}
void pangu_tui_chat_completion(pangu_tui_chat *c, pg_tui_complete_fn fn,
                               void *ctx) {
  if (!c)
    return;
  uv_mutex_lock(&c->mutex);
  c->complete = fn;
  c->complete_ctx = ctx;
  uv_mutex_unlock(&c->mutex);
}
void pangu_tui_chat_poll(pangu_tui_chat *c) {
  if (!c)
    return;
  uv_mutex_lock(&c->mutex);
  if (c->approval_id) {
    uv_mutex_unlock(&c->mutex);
    return;
  }
  for (;;) {
    int pending = c->completion;
    pg_tui_complete_fn fn = c->complete;
    void *ctx = c->complete_ctx;
    size_t n;
    const char *s = pg_editor_text(c->editor, &n);
    size_t cur = pg_editor_cursor(c->editor), a = cur;
    while (a && s[a - 1] != ' ' && s[a - 1] != '\n')
      a--;
    char *token = NULL, *before = NULL;
    if (pending && fn) {
      token = malloc(cur - a + 1);
      before = malloc(n + 1);
      if (token && before) {
        memcpy(token, s + a, cur - a);
        token[cur - a] = 0;
        memcpy(before, s, n + 1);
      } else {
        free(token);
        free(before);
        token = before = NULL;
        append(c, "Completion allocation failed", 28);
        c->quit = c->cancel = 1;
      }
    }
    uv_mutex_unlock(&c->mutex);
    if (token) {
      size_t len = 0;
      char *replacement = fn(ctx, token, cur - a, &len);
      uv_mutex_lock(&c->mutex);
      size_t now;
      const char *text = pg_editor_text(c->editor, &now);
      if (replacement && now == n && pg_editor_cursor(c->editor) == cur &&
          !memcmp(before, text, n)) {
        erase(c);
        if (pg_editor_complete(c->editor, replacement, len) != PANGU_TUI_OK) {
          append(c, "Completion rejected", 19);
          c->quit = c->cancel = 1;
        }
        render(c);
      }
      uv_mutex_unlock(&c->mutex);
      free(replacement);
      free(token);
      free(before);
    }
    uv_mutex_lock(&c->mutex);
    c->completion = 0;
    /* Replay only up to the next Tab; each callback observes exactly the
     * editor state at that key, even when an entire line arrived in one read.
     */
    while (!c->quit && !c->completion && c->keys_head) {
      key_input *key = c->keys_head;
      c->keys_head = key->next;
      if (!c->keys_head)
        c->keys_tail = NULL;
      feed_locked(c, key->data, key->length, key->paste, key->busy);
      free(key);
    }
    if (!c->completion)
      break;
  }
  int can_submit = c->submit && !c->busy && !c->quit;
  uv_mutex_unlock(&c->mutex);
  if (can_submit) {
    pg_tui_input cmd;
    if (pangu_tui_chat_take_input(c, &cmd)) {
      c->submit(c->submit_ctx, cmd.text, cmd.length);
      pangu_tui_input_release(&cmd);
    }
  }
}
pangu_tui_status pangu_tui_chat_run(pangu_tui_chat *c) {
  if (!c || !c->started)
    return PANGU_TUI_INVALID;
  while (!pangu_tui_chat_quitting(c)) {
    pangu_tui_chat_poll(c);
    uv_sleep(10);
  }
  return pangu_tui_terminal_result(c->terminal);
}
void pangu_tui_chat_append(pangu_tui_chat *c, const char *s, size_t n) {
  if (!c || (!s && n))
    return;
  uv_mutex_lock(&c->mutex);
  append(c, s, n);
  uv_mutex_unlock(&c->mutex);
}

pangu_tui_status pangu_tui_chat_live(pangu_tui_chat *c, const char *s,
                                     size_t n) {
  if (!c || (!s && n) || n == (size_t)-1)
    return PANGU_TUI_INVALID;
  char *p = malloc(n + 1);
  if (!p)
    return PANGU_TUI_NOMEM;
  size_t a = 0, b = 0;
  while (a < n) {
    utf8proc_int32_t cp;
    utf8proc_ssize_t k = utf8proc_iterate((const utf8proc_uint8_t *)s + a,
                                          (utf8proc_ssize_t)(n - a), &cp);
    if (k <= 0) {
      p[b++] = '?';
      a++;
      continue;
    }
    if (cp == '\n' || (cp >= 32 && !(cp >= 127 && cp < 160))) {
      memcpy(p + b, s + a, (size_t)k);
      b += (size_t)k;
    } else
      p[b++] = ' ';
    a += (size_t)k;
  }
  p[b] = 0;
  uv_mutex_lock(&c->mutex);
  erase(c);
  free(c->live);
  c->live = p;
  c->live_length = b;
  render(c);
  uv_mutex_unlock(&c->mutex);
  return PANGU_TUI_OK;
}

void pangu_tui_chat_clear(pangu_tui_chat *c) {
  if (!c)
    return;
  uv_mutex_lock(&c->mutex);
  erase(c);
  free(c->live);
  c->live = NULL;
  c->live_length = 0;
  str(c, "\x1b[3J\x1b[2J\x1b[H");
  render(c);
  uv_mutex_unlock(&c->mutex);
}
