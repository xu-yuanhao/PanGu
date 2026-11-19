/* Native adaptation of Pi Editor/Input, UndoStack, KillRing, word-navigation,
 * d5b3de22 (MIT). Unicode segmentation uses utf8proc rather than
 * Intl.Segmenter. Selection/redo are native additions; history preserves the
 * unsent draft. */
#include "editor_internal.h"
#include <ctype.h>
#include <pangu/tui/editor.h>
#include <pangu/tui/keys.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>

typedef struct snapshot {
  char *text;
  size_t length, cursor, anchor;
  struct snapshot *next;
} snapshot;
struct pangu_tui_editor {
  char *text;
  size_t length, cursor, anchor;
  snapshot *undo, *redo, *history, *kills;
  size_t history_index;
  char *draft;
  size_t draft_cursor, draft_anchor;
  char *search_query;
  size_t search_length, search_index;
  int search_matched;
  snapshot *search_saved;
  char *kill;
  int killing, yanked;
  size_t yank_start, yank_length;
};
static char *copy(const char *s, size_t n) {
  if (n == SIZE_MAX)
    return NULL;
  char *p = malloc(n + 1);
  if (p) {
    if (n)
      memcpy(p, s, n);
    p[n] = 0;
  }
  return p;
}
static void clear(snapshot **s) {
  while (*s) {
    snapshot *p = *s;
    *s = p->next;
    free(p->text);
    free(p);
  }
}
static int push(snapshot **s, const pangu_tui_editor *e) {
  snapshot *p = malloc(sizeof *p);
  if (!p)
    return 0;
  p->text = copy(e->text, e->length);
  if (!p->text) {
    free(p);
    return 0;
  }
  p->length = e->length;
  p->cursor = e->cursor;
  p->anchor = e->anchor;
  p->next = *s;
  *s = p;
  return 1;
}
static void exit_history(pangu_tui_editor *e) {
  e->history_index = 0;
  free(e->draft);
  e->draft = NULL;
}
static void discard_search(pangu_tui_editor *e) {
  free(e->search_query);
  e->search_query = NULL;
  e->search_length = e->search_index = 0;
  e->search_matched = 0;
  clear(&e->search_saved);
}
pangu_tui_editor *pg_editor_create(void) {
  pangu_tui_editor *e = calloc(1, sizeof *e);
  if (e) {
    e->text = copy("", 0);
    if (!e->text) {
      free(e);
      return NULL;
    }
  }
  return e;
}
void pg_editor_destroy(pangu_tui_editor *e) {
  if (!e)
    return;
  free(e->text);
  free(e->draft);
  free(e->kill);
  discard_search(e);
  clear(&e->undo);
  clear(&e->redo);
  clear(&e->history);
  clear(&e->kills);
  free(e);
}
const char *pg_editor_text(const pangu_tui_editor *e, size_t *n) {
  if (n)
    *n = e ? e->length : 0;
  return e ? e->text : NULL;
}
size_t pg_editor_cursor(const pangu_tui_editor *e) { return e ? e->cursor : 0; }
size_t pg_editor_anchor(const pangu_tui_editor *e) { return e ? e->anchor : 0; }
size_t pg_editor_next(const char *s, size_t n, size_t at) {
  if (at >= n)
    return n;
  utf8proc_int32_t prev, cp, state = 0;
  utf8proc_ssize_t k = utf8proc_iterate((const utf8proc_uint8_t *)s + at,
                                        (utf8proc_ssize_t)(n - at), &prev);
  if (k <= 0)
    return at + 1;
  size_t p = at + (size_t)k;
  while (p < n) {
    k = utf8proc_iterate((const utf8proc_uint8_t *)s + p,
                         (utf8proc_ssize_t)(n - p), &cp);
    if (k <= 0)
      break;
    if (utf8proc_grapheme_break_stateful(prev, cp, &state))
      break;
    prev = cp;
    p += (size_t)k;
  }
  return p;
}
unsigned pg_editor_width(const char *s, size_t n) {
  unsigned width = 0;
  size_t p = 0;
  while (p < n) {
    utf8proc_int32_t cp;
    utf8proc_ssize_t k = utf8proc_iterate((const utf8proc_uint8_t *)s + p,
                                          (utf8proc_ssize_t)(n - p), &cp);
    if (k <= 0)
      break;
    int w = utf8proc_charwidth(cp);
    if (w > 0 && (unsigned)w > width)
      width = (unsigned)w;
    p += (size_t)k;
  }
  return width;
}
static size_t previous(const pangu_tui_editor *e, size_t p) {
  size_t a = 0, b = 0;
  while (a < p) {
    b = a;
    a = pg_editor_next(e->text, e->length, a);
  }
  return b;
}
static pangu_tui_status replace(pangu_tui_editor *e, size_t a, size_t b,
                                const char *s, size_t n, int record) {
  if (n > SIZE_MAX - (e->length - (b - a)) - 1)
    return PANGU_TUI_NOMEM;
  size_t total = e->length - (b - a) + n;
  char *p = malloc(total + 1);
  if (!p)
    return PANGU_TUI_NOMEM;
  memcpy(p, e->text, a);
  if (n)
    memcpy(p + a, s, n);
  memcpy(p + a + n, e->text + b, e->length - b);
  p[total] = 0;
  if (record && !push(&e->undo, e)) {
    free(p);
    return PANGU_TUI_NOMEM;
  }
  if (record)
    clear(&e->redo);
  free(e->text);
  e->text = p;
  e->length = total;
  e->cursor = e->anchor = a + n;
  return PANGU_TUI_OK;
}
static pangu_tui_status clean(const char *s, size_t n, char **out,
                              size_t *len) {
  if ((!s && n) || n > PTRDIFF_MAX)
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
      free(p);
      return PANGU_TUI_INVALID;
    }
    if (cp == '\r') {
      p[b++] = '\n';
      a++;
      if (a < n && s[a] == '\n')
        a++;
      continue;
    }
    if (cp == '\t') {
      p[b++] = ' ';
      a++;
      continue;
    }
    if (cp < 32 && cp != '\n') {
      free(p);
      return PANGU_TUI_INVALID;
    }
    if ((cp >= 127 && cp < 160)) {
      free(p);
      return PANGU_TUI_INVALID;
    }
    memcpy(p + b, s + a, (size_t)k);
    b += (size_t)k;
    a += (size_t)k;
  }
  p[b] = 0;
  *out = p;
  *len = b;
  return PANGU_TUI_OK;
}
pangu_tui_status pg_editor_set(pangu_tui_editor *e, const char *s, size_t n) {
  if (!e)
    return PANGU_TUI_INVALID;
  if (!n) {
    e->text[0] = 0;
    e->length = e->cursor = e->anchor = 0;
    exit_history(e);
    discard_search(e);
    e->killing = e->yanked = 0;
    clear(&e->undo);
    clear(&e->redo);
    return PANGU_TUI_OK;
  }
  char *p;
  size_t len;
  pangu_tui_status st = clean(s, n, &p, &len);
  if (st)
    return st;
  st = replace(e, 0, e->length, p, len, 0);
  free(p);
  if (!st) {
    clear(&e->undo);
    clear(&e->redo);
    exit_history(e);
    discard_search(e);
  }
  return st;
}
pangu_tui_status pg_editor_insert(pangu_tui_editor *e, const char *s,
                                  size_t n) {
  if (!e)
    return PANGU_TUI_INVALID;
  if (e->search_query) {
    if ((!s && n) || n > SIZE_MAX - e->search_length - 1)
      return PANGU_TUI_INVALID;
    char *query = malloc(e->search_length + n + 1);
    if (!query)
      return PANGU_TUI_NOMEM;
    memcpy(query, e->search_query, e->search_length);
    if (n)
      memcpy(query + e->search_length, s, n);
    pangu_tui_status st =
        pg_editor_search_update(e, query, e->search_length + n);
    free(query);
    return st;
  }
  e->killing = e->yanked = 0;
  char *p;
  size_t len;
  pangu_tui_status st = clean(s, n, &p, &len);
  if (st)
    return st;
  size_t a = e->cursor < e->anchor ? e->cursor : e->anchor,
         b = e->cursor > e->anchor ? e->cursor : e->anchor;
  st = replace(e, a, b, p, len, 1);
  if (!st)
    exit_history(e);
  free(p);
  return st;
}
static size_t line_start(const pangu_tui_editor *e, size_t p) {
  while (p && e->text[p - 1] != '\n')
    p--;
  return p;
}
static size_t line_end(const pangu_tui_editor *e, size_t p) {
  while (p < e->length && e->text[p] != '\n')
    p++;
  return p;
}
static int cls(const pangu_tui_editor *e, size_t p) {
  unsigned char c = (unsigned char)e->text[p];
  return isspace(c) ? 0 : (c >= 128 || isalnum(c) || c == '_') ? 1 : 2;
}
static size_t word(const pangu_tui_editor *e, int back) {
  size_t p = e->cursor;
  if (back) {
    while (p && cls(e, previous(e, p)) == 0)
      p = previous(e, p);
    if (!p)
      return 0;
    int c = cls(e, previous(e, p));
    while (p && cls(e, previous(e, p)) == c)
      p = previous(e, p);
  } else {
    while (p < e->length && cls(e, p) == 0)
      p = pg_editor_next(e->text, e->length, p);
    if (p == e->length)
      return p;
    int c = cls(e, p);
    while (p < e->length && cls(e, p) == c)
      p = pg_editor_next(e->text, e->length, p);
  }
  return p;
}
static pangu_tui_status restore(pangu_tui_editor *e, snapshot **from,
                                snapshot **to) {
  if (!*from)
    return PANGU_TUI_OK;
  if (!push(to, e))
    return PANGU_TUI_NOMEM;
  snapshot *p = *from;
  *from = p->next;
  free(e->text);
  e->text = p->text;
  e->length = p->length;
  e->cursor = p->cursor;
  e->anchor = p->anchor;
  free(p);
  exit_history(e);
  return PANGU_TUI_OK;
}
/* ECMAScript trim whitespace used by Pi addToHistory, after clean(). */
static int history_space(utf8proc_int32_t cp) {
  utf8proc_category_t category = utf8proc_category(cp);
  return cp == '\n' || cp == '\r' || cp == '\t' || cp == 0xFEFF ||
         category == UTF8PROC_CATEGORY_ZS || category == UTF8PROC_CATEGORY_ZL ||
         category == UTF8PROC_CATEGORY_ZP;
}
pangu_tui_status pg_editor_history_add(pangu_tui_editor *e, const char *s,
                                       size_t n) {
  if (!e)
    return PANGU_TUI_INVALID;
  char *p;
  size_t len;
  pangu_tui_status st = clean(s, n, &p, &len);
  if (st)
    return st;
  size_t first = len, last = 0;
  for (size_t i = 0; i < len;) {
    utf8proc_int32_t cp;
    size_t k = (size_t)utf8proc_iterate((const utf8proc_uint8_t *)p + i,
                                        (utf8proc_ssize_t)(len - i), &cp);
    if (!history_space(cp)) {
      if (first == len)
        first = i;
      last = i + k;
    }
    i += k;
  }
  if (!last || (e->history && e->history->length == last - first &&
                !memcmp(e->history->text, p + first, last - first))) {
    free(p);
    return PANGU_TUI_OK;
  }
  pangu_tui_editor temp = {.text = p + first, .length = last - first};
  int ok = push(&e->history, &temp);
  free(p);
  if (!ok)
    return PANGU_TUI_NOMEM;
  exit_history(e);
  if (e->search_index)
    ++e->search_index;
  snapshot *h = e->history;
  for (size_t count = 1; h && count < 100; ++count)
    h = h->next;
  if (h)
    clear(&h->next);
  return PANGU_TUI_OK;
}
pangu_tui_status pg_editor_take_history(pangu_tui_editor *to,
                                        pangu_tui_editor *from) {
  if (!to || !from || to == from || to->history_index || to->search_query)
    return PANGU_TUI_INVALID;
  clear(&to->history);
  to->history = from->history;
  from->history = NULL;
  return PANGU_TUI_OK;
}
pangu_tui_status pg_editor_history_move(pangu_tui_editor *e, int back) {
  if (!e || e->search_query)
    return PANGU_TUI_INVALID;
  size_t idx = e->history_index;
  if ((!back && !idx) || !e->history)
    return PANGU_TUI_OK;
  size_t want = back ? idx + 1 : idx - 1;
  snapshot *h = e->history;
  for (size_t i = 1; h && i < want; i++)
    h = h->next;
  if (want && !h)
    return PANGU_TUI_OK;
  char *draft = NULL;
  size_t cursor = e->cursor, anchor = e->anchor;
  if (!idx) {
    draft = copy(e->text, e->length);
    if (!draft)
      return PANGU_TUI_NOMEM;
  }
  const char *text = want ? h->text : e->draft;
  size_t n = want ? h->length : strlen(text);
  pangu_tui_status st = replace(e, 0, e->length, text, n, !idx);
  if (st) {
    free(draft);
    return st;
  }
  if (!idx) {
    free(e->draft);
    e->draft = draft;
    e->draft_cursor = cursor;
    e->draft_anchor = anchor;
  }
  e->history_index = want;
  e->cursor = want ? (back ? 0 : n) : e->draft_cursor;
  e->anchor = want ? e->cursor : e->draft_anchor;
  if (!want)
    exit_history(e);
  return PANGU_TUI_OK;
}
pangu_tui_status pg_editor_search(pangu_tui_editor *e, const char *q,
                                  size_t n) {
  if (!e || e->search_query || (!q && n) || n > PTRDIFF_MAX)
    return PANGU_TUI_INVALID;
  for (snapshot *h = e->history; h; h = h->next)
    for (size_t i = 0; n <= h->length && i <= h->length - n; i++)
      if (!n || !memcmp(h->text + i, q, n))
        return replace(e, 0, e->length, h->text, h->length, 1);
  return PANGU_TUI_OK;
}
/* Codex bottom_pane/chat_composer/history_search.rs and
 * bottom_pane/chat_composer_history.rs, 8dd6099e (Apache-2.0):
 * query/preview separation, unique matches, accept-to-edit and cancel-to-draft.
 * Native in-memory store is bounded by Pi's 100-entry history policy. */
const char *pg_editor_search_query(const pangu_tui_editor *e, size_t *n,
                                   int *matched) {
  if (n)
    *n = e && e->search_query ? e->search_length : 0;
  if (matched)
    *matched = e && e->search_query ? e->search_matched : 0;
  return e ? e->search_query : NULL;
}
pangu_tui_status pg_editor_search_begin(pangu_tui_editor *e) {
  if (!e)
    return PANGU_TUI_INVALID;
  if (e->search_query)
    return PANGU_TUI_OK;
  char *q = copy("", 0);
  snapshot *saved = NULL;
  if (!q || !push(&saved, e)) {
    free(q);
    return PANGU_TUI_NOMEM;
  }
  e->search_query = q;
  e->search_saved = saved;
  e->search_length = e->search_index = 0;
  e->search_matched = 0;
  exit_history(e);
  return PANGU_TUI_OK;
}
static pangu_tui_status search_find(pangu_tui_editor *e, const char *query,
                                    size_t n, size_t start, int older,
                                    snapshot **result, size_t *index) {
  *result = NULL;
  *index = 0;
  if (!n)
    return PANGU_TUI_OK;
  utf8proc_uint8_t *folded = NULL;
  utf8proc_ssize_t len = utf8proc_map(
      (const utf8proc_uint8_t *)query, (utf8proc_ssize_t)n, &folded,
      UTF8PROC_STABLE | UTF8PROC_CASEFOLD | UTF8PROC_COMPOSE);
  if (len < 0)
    return PANGU_TUI_NOMEM;
  size_t at = 0;
  for (snapshot *h = e->history; h; h = h->next) {
    ++at;
    if ((older && at < start) || (!older && at > start))
      continue;
    int duplicate = 0;
    for (snapshot *p = e->history; p != h; p = p->next)
      if (p->length == h->length && !memcmp(p->text, h->text, h->length)) {
        duplicate = 1;
        break;
      }
    if (duplicate)
      continue;
    utf8proc_uint8_t *text = NULL;
    utf8proc_ssize_t length = utf8proc_map(
        (const utf8proc_uint8_t *)h->text, (utf8proc_ssize_t)h->length, &text,
        UTF8PROC_STABLE | UTF8PROC_CASEFOLD | UTF8PROC_COMPOSE);
    if (length < 0) {
      free(folded);
      return PANGU_TUI_NOMEM;
    }
    int match = strstr((const char *)text, (const char *)folded) != NULL;
    free(text);
    if (match) {
      *result = h;
      *index = at;
      if (older)
        break;
    }
  }
  free(folded);
  return PANGU_TUI_OK;
}
pangu_tui_status pg_editor_search_update(pangu_tui_editor *e, const char *q,
                                         size_t n) {
  if (!e || !e->search_query)
    return PANGU_TUI_INVALID;
  char *query;
  size_t length;
  pangu_tui_status st = clean(q, n, &query, &length);
  if (st)
    return st;
  snapshot *match;
  size_t index;
  st = search_find(e, query, length, 1, 1, &match, &index);
  if (st) {
    free(query);
    return st;
  }
  snapshot *preview = match ? match : e->search_saved;
  st = replace(e, 0, e->length, preview->text, preview->length, 0);
  if (st) {
    free(query);
    return st;
  }
  if (!match) {
    e->cursor = preview->cursor;
    e->anchor = preview->anchor;
  }
  free(e->search_query);
  e->search_query = query;
  e->search_length = length;
  e->search_index = index;
  e->search_matched = match != NULL;
  return PANGU_TUI_OK;
}
pangu_tui_status pg_editor_search_step(pangu_tui_editor *e, int older) {
  if (!e || !e->search_query)
    return PANGU_TUI_INVALID;
  if (!e->search_length || (!older && !e->search_index))
    return PANGU_TUI_OK;
  snapshot *match;
  size_t index, start = older ? e->search_index + 1 : e->search_index - 1;
  pangu_tui_status st = search_find(e, e->search_query, e->search_length, start,
                                    older, &match, &index);
  if (st || !match)
    return st;
  st = replace(e, 0, e->length, match->text, match->length, 0);
  if (!st) {
    e->search_index = index;
    e->search_matched = 1;
  }
  return st;
}
pangu_tui_status pg_editor_search_finish(pangu_tui_editor *e, int accept) {
  if (!e)
    return PANGU_TUI_INVALID;
  if (!e->search_query || (accept && !e->search_matched))
    return PANGU_TUI_OK;
  snapshot *saved = e->search_saved;
  e->search_saved = NULL;
  if (accept) {
    saved->next = e->undo;
    e->undo = saved;
    clear(&e->redo);
    e->cursor = e->anchor = e->length;
  } else {
    free(e->text);
    e->text = saved->text;
    e->length = saved->length;
    e->cursor = saved->cursor;
    e->anchor = saved->anchor;
    free(saved);
  }
  discard_search(e);
  exit_history(e);
  return PANGU_TUI_OK;
}
pangu_tui_status pg_editor_complete(pangu_tui_editor *e, const char *s,
                                    size_t n) {
  if (!e || e->search_query)
    return PANGU_TUI_INVALID;
  char *p;
  size_t len;
  pangu_tui_status st = clean(s, n, &p, &len);
  if (st)
    return st;
  size_t a = e->cursor;
  while (a && !isspace((unsigned char)e->text[a - 1]))
    a--;
  st = replace(e, a, e->cursor, p, len, 1);
  if (!st)
    exit_history(e);
  free(p);
  return st;
}
#define MATCH(k) pangu_tui_matches_key(d, n, k)
static pg_edit_action search_key(pangu_tui_editor *e, const uint8_t *d,
                                 size_t n, const pangu_tui_key *k) {
  pangu_tui_status st = PANGU_TUI_OK;
  if (MATCH("ctrl+r") || MATCH("up"))
    st = pg_editor_search_step(e, 1);
  else if (MATCH("ctrl+s") || MATCH("down"))
    st = pg_editor_search_step(e, 0);
  else if (MATCH("escape") || MATCH("ctrl+c"))
    st = pg_editor_search_finish(e, 0);
  else if (MATCH("enter") || MATCH("ctrl+m"))
    st = pg_editor_search_finish(e, 1);
  else if (MATCH("ctrl+u"))
    st = pg_editor_search_update(e, "", 0);
  else if (MATCH("backspace") || MATCH("ctrl+h")) {
    size_t last = 0;
    for (size_t p = 0; p < e->search_length;) {
      last = p;
      p = pg_editor_next(e->search_query, e->search_length, p);
    }
    st = pg_editor_search_update(e, e->search_query, last);
  } else {
    char text[4];
    size_t used = pangu_tui_decode_printable(d, n, text);
    if (!used && !(k->modifiers & ~PANGU_KMOD_SHIFT) && k->codepoint >= 32 &&
        k->codepoint <= 0x10ffff)
      used = (size_t)utf8proc_encode_char((utf8proc_int32_t)k->codepoint,
                                          (utf8proc_uint8_t *)text);
    if (used)
      st = pg_editor_insert(e, text, used);
  }
  return st ? PG_EDIT_ERROR : PG_EDIT_CHANGED;
}
pg_edit_action pg_editor_key(pangu_tui_editor *e, const uint8_t *d, size_t n) {
  if (!e || !d || !n)
    return PG_EDIT_NONE;
  pangu_tui_key k;
  if (!pangu_tui_parse_key(d, n, &k) || k.event_type == PANGU_KEY_RELEASE)
    return PG_EDIT_NONE;
  if (e->search_query)
    return search_key(e, d, n, &k);
  if (MATCH("ctrl+c") || MATCH("escape"))
    return PG_EDIT_CANCEL;
  if (MATCH("ctrl+d") && !e->length)
    return PG_EDIT_EOF;
  if (MATCH("enter") || MATCH("ctrl+m"))
    return PG_EDIT_SUBMIT;
  if (MATCH("tab"))
    return PG_EDIT_COMPLETE;
  int yank_pop = MATCH("alt+y"), yank = MATCH("ctrl+y");
  if (!yank_pop && !yank)
    e->yanked = 0;
  if (!(MATCH("ctrl+w") || MATCH("alt+backspace") || MATCH("alt+d") ||
        MATCH("alt+delete") || MATCH("ctrl+u") || MATCH("ctrl+k")))
    e->killing = 0;
  pangu_tui_status st = PANGU_TUI_OK;
  if (MATCH("shift+enter") || MATCH("alt+enter") || MATCH("ctrl+j"))
    st = pg_editor_insert(e, "\n", 1);
  else if (MATCH("ctrl+z") || MATCH("ctrl+-"))
    st = restore(e, &e->undo, &e->redo);
  else if (MATCH("ctrl+shift+z") || MATCH("alt+z"))
    st = restore(e, &e->redo, &e->undo);
  else if (MATCH("ctrl+p"))
    return pg_editor_history_move(e, 1) ? PG_EDIT_ERROR : PG_EDIT_CHANGED;
  else if (MATCH("ctrl+n"))
    return pg_editor_history_move(e, 0) ? PG_EDIT_ERROR : PG_EDIT_CHANGED;
  else if (MATCH("ctrl+r"))
    return pg_editor_search_begin(e) ? PG_EDIT_ERROR : PG_EDIT_CHANGED;
  else if (yank_pop) {
    if (e->yanked && e->kills) {
      snapshot *h = e->kills;
      size_t length = h->length;
      st = replace(e, e->yank_start, e->yank_start + e->yank_length, h->text,
                   length, 1);
      if (!st) {
        e->kills = h->next;
        h->next = NULL;
        char *previous = e->kill;
        e->kill = h->text;
        h->text = previous;
        h->length = strlen(previous);
        snapshot **tail = &e->kills;
        while (*tail)
          tail = &(*tail)->next;
        *tail = h;
        e->yank_length = length;
      }
    }
  } else if (yank) {
    if (e->kill) {
      size_t a = e->cursor < e->anchor ? e->cursor : e->anchor;
      size_t length = strlen(e->kill);
      st = pg_editor_insert(e, e->kill, length);
      if (!st) {
        e->yank_start = a;
        e->yank_length = length;
        e->yanked = 1;
      }
    }
  } else {
    size_t target = e->cursor;
    int move = 1;
    int shift = (k.modifiers & PANGU_KMOD_SHIFT) != 0;
    /* Movement ignores shift while preserving selection anchor. */
    pangu_tui_key plain = k;
    plain.modifiers &= ~PANGU_KMOD_SHIFT;
    if (k.codepoint == 0xFFFFF004u || MATCH("ctrl+b"))
      target = (plain.modifiers & (PANGU_KMOD_ALT | PANGU_KMOD_CTRL))
                   ? word(e, 1)
                   : previous(e, e->cursor);
    else if (k.codepoint == 0xFFFFF003u || MATCH("ctrl+f"))
      target = (plain.modifiers & (PANGU_KMOD_ALT | PANGU_KMOD_CTRL))
                   ? word(e, 0)
                   : pg_editor_next(e->text, e->length, e->cursor);
    else if (MATCH("alt+b"))
      target = word(e, 1);
    else if (MATCH("alt+f"))
      target = word(e, 0);
    else if (k.codepoint == 0xFFFFF00Eu || MATCH("ctrl+a"))
      target = line_start(e, e->cursor);
    else if (k.codepoint == 0xFFFFF00Fu || MATCH("ctrl+e"))
      target = line_end(e, e->cursor);
    else if (k.codepoint == 0xFFFFF001u || k.codepoint == 0xFFFFF002u) {
      size_t start = line_start(e, e->cursor), end = line_end(e, e->cursor),
             col = 0;
      for (size_t p = start; p < e->cursor;
           p = pg_editor_next(e->text, e->length, p))
        col++;
      int up = k.codepoint == 0xFFFFF001u;
      if ((up && !start) || (!up && end == e->length)) {
        if (up && !e->history_index && e->length && e->cursor) {
          e->cursor = 0;
          if (!shift)
            e->anchor = 0;
        } else if (up || e->history_index)
          st = pg_editor_history_move(e, up);
        else {
          e->cursor = end;
          if (!shift)
            e->anchor = end;
        }
        move = 0;
      } else {
        target = up ? line_start(e, start - 1) : end + 1;
        while (col-- && target < e->length && e->text[target] != '\n')
          target = pg_editor_next(e->text, e->length, target);
      }
    } else
      move = 0;
    if (move) {
      e->cursor = target;
      if (!shift)
        e->anchor = target;
      e->killing = 0;
      return PG_EDIT_CHANGED;
    }
    if (k.codepoint == 0xFFFFF001u || k.codepoint == 0xFFFFF002u)
      return st ? PG_EDIT_ERROR : PG_EDIT_CHANGED;
    int kill = 0;
    size_t a = e->cursor < e->anchor ? e->cursor : e->anchor,
           b = e->cursor > e->anchor ? e->cursor : e->anchor;
    if (MATCH("backspace")) {
      if (a == b)
        a = previous(e, a);
    } else if (MATCH("delete") || MATCH("ctrl+d")) {
      if (a == b)
        b = pg_editor_next(e->text, e->length, b);
    } else if (MATCH("ctrl+w") || MATCH("alt+backspace")) {
      a = word(e, 1);
      kill = 1;
    } else if (MATCH("alt+d") || MATCH("alt+delete")) {
      b = word(e, 0);
      kill = 1;
    } else if (MATCH("ctrl+u")) {
      a = line_start(e, a);
      kill = 1;
    } else if (MATCH("ctrl+k")) {
      b = line_end(e, b);
      if (a == b && b < e->length)
        b++;
      kill = 1;
    } else {
      char out[4];
      size_t used = pangu_tui_decode_printable(d, n, out);
      if (!used && !(k.modifiers & ~PANGU_KMOD_SHIFT) && k.codepoint >= 32 &&
          k.codepoint <= 0x10ffff)
        used = (size_t)utf8proc_encode_char((utf8proc_int32_t)k.codepoint,
                                            (utf8proc_uint8_t *)out);
      if (used) {
        st = pg_editor_insert(e, out, used);
      }
      e->killing = 0;
      return st ? PG_EDIT_ERROR : used ? PG_EDIT_CHANGED : PG_EDIT_NONE;
    }
    char *newkill = NULL;
    if (kill && b > a) {
      size_t old = e->killing && e->kill ? strlen(e->kill) : 0;
      newkill = malloc(old + b - a + 1);
      if (!newkill)
        return PG_EDIT_ERROR;
      if (a < e->cursor) {
        memcpy(newkill, e->text + a, b - a);
        if (old)
          memcpy(newkill + b - a, e->kill, old);
      } else {
        if (old)
          memcpy(newkill, e->kill, old);
        memcpy(newkill + old, e->text + a, b - a);
      }
      newkill[old + b - a] = 0;
    }
    snapshot *older = NULL;
    if (newkill && !e->killing && e->kill) {
      older = calloc(1, sizeof *older);
      if (!older) {
        free(newkill);
        return PG_EDIT_ERROR;
      }
      older->text = e->kill;
      older->length = strlen(e->kill);
    }
    st = replace(e, a, b, "", 0, 1);
    if (!st)
      exit_history(e);
    if (!st && newkill) {
      if (older) {
        older->next = e->kills;
        e->kills = older;
        older = NULL;
      } else
        free(e->kill);
      e->kill = newkill;
    } else
      free(newkill);
    free(older);
    e->killing = kill;
  }
  return st ? PG_EDIT_ERROR : PG_EDIT_CHANGED;
}
