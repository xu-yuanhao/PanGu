/* Stdin sequence reassembly ported from Pi packages/tui/src/stdin-buffer.ts,
 * snapshot d5b3de22 (based on OpenTUI, MIT). Byte-level port: input is UTF-8
 * bytes; one codepoint per non-escape character event. */
#include <pangu/tui/buffer.h>
#include <stdlib.h>
#include <string.h>

#define ESC 0x1b
#define BRACKETED_PASTE_START "\x1b[200~"
#define BRACKETED_PASTE_END "\x1b[201~"
#define DEFAULT_SEQUENCE_TIMEOUT_MS 50
#define DEFAULT_ESCAPE_TIMEOUT_MS 10

struct pangu_tui_buffer {
  uint8_t *pending;
  size_t pending_capacity;
  int failed;
  size_t pending_len;
  uint8_t *paste;
  size_t paste_capacity;
  uint64_t last_ms;
  size_t paste_len;
  int paste_mode;
  /* Pending printable codepoint from a Kitty CSI-u event, used to drop the
   * raw duplicate character some terminals send after the sequence. */
  unsigned kitty_printable;
  uint64_t deadline_ms; /* 0 = no timer */
  uint32_t sequence_timeout_ms;
  uint32_t escape_timeout_ms;
  pangu_tui_sequence_fn on_sequence;
  pangu_tui_paste_fn on_paste;
  pangu_tui_preflush_fn on_preflush;
  void *ctx;
  /* Callbacks must not re-enter process/flush/destroy on this buffer. */
  int emitting;
};

pangu_tui_buffer *pangu_tui_buffer_create(void) {
  pangu_tui_buffer *b = calloc(1, sizeof *b);
  if (b) {
    b->sequence_timeout_ms = DEFAULT_SEQUENCE_TIMEOUT_MS;
    b->escape_timeout_ms = DEFAULT_ESCAPE_TIMEOUT_MS;
  }
  return b;
}

void pangu_tui_buffer_destroy(pangu_tui_buffer *buffer) {
  if (buffer) {
    free(buffer->paste);
    free(buffer->pending);
  }
  free(buffer);
}

void pangu_tui_buffer_configure(pangu_tui_buffer *buffer,
                                uint32_t sequence_timeout_ms,
                                uint32_t escape_timeout_ms) {
  if (!buffer)
    return;
  buffer->sequence_timeout_ms =
      sequence_timeout_ms ? sequence_timeout_ms : DEFAULT_SEQUENCE_TIMEOUT_MS;
  buffer->escape_timeout_ms =
      escape_timeout_ms ? escape_timeout_ms : DEFAULT_ESCAPE_TIMEOUT_MS;
}

void pangu_tui_buffer_set_callbacks(pangu_tui_buffer *buffer,
                                    pangu_tui_sequence_fn on_sequence,
                                    pangu_tui_paste_fn on_paste,
                                    pangu_tui_preflush_fn on_preflush,
                                    void *ctx) {
  if (!buffer)
    return;
  buffer->on_sequence = on_sequence;
  buffer->on_paste = on_paste;
  buffer->on_preflush = on_preflush;
  buffer->ctx = ctx;
}

size_t pangu_tui_buffer_pending(const pangu_tui_buffer *buffer) {
  return buffer ? buffer->pending_len : 0;
}

/* ------------------------------------------------------------------ */
/* Sequence classification                                             */
/* ------------------------------------------------------------------ */

enum { NOT_ESCAPE, COMPLETE, INCOMPLETE };

/* Recognized terminal reply headers: OSC color/status, DCS, APC,OSC8-ish. */
static int is_control_string(const uint8_t *d, size_t n) {
  return n >= 2 && d[0] == ESC && (d[1] == ']' || d[1] == 'P' || d[1] == '_');
}

static int match_reply_header(const uint8_t *d, size_t n) {
  if (n < 2 || d[0] != ESC)
    return 0;
  if (d[1] == ']') {
    /* OSC 10/11 (default colors), OSC 4;<digits>; (palette),
     * OSC 7501;? (program status). */
    if (n >= 3 && d[2] == '1' && n >= 4 && (d[3] == '0' || d[3] == '1') &&
        (n < 5 || d[4] == ';'))
      return n <= 5;
    if (n >= 3 && d[2] == '4' && n >= 4 && d[3] == ';')
      return 1;
    if (n >= 6 && memcmp(d, "\x1b]7501", 6) == 0)
      return n <= 7;
    return 0;
  }
  if (d[1] == 'P') {
    /* DCS: any two payload bytes establish the header. */
    return n >= 4;
  }
  if (d[1] == '_') {
    /* APC: needs `_G` plus key=value pairs. */
    if (n < 3)
      return 0;
    if (d[2] != 'G')
      return 0;
    if (n == 3)
      return 1;
    return d[3] == 'i' || (d[3] >= 'a' && d[3] <= 'z');
  }
  return 0;
}

static int is_complete_csi(const uint8_t *d, size_t n) {
  if (n < 3)
    return INCOMPLETE;
  /* Old-style mouse: ESC[M + 3 bytes. */
  if (d[2] == 'M')
    return n >= 6 ? COMPLETE : INCOMPLETE;
  const uint8_t last = d[n - 1];
  if (last >= 0x40 && last <= 0x7e) {
    if (d[2] == '<') {
      /* SGR mouse: <digits;digits;digits>[Mm] */
      int digits = 0, semis = 0;
      for (size_t i = 3; i < n - 1; ++i) {
        uint8_t c = d[i];
        if (c >= '0' && c <= '9')
          digits = 1;
        else if (c == ';') {
          if (!digits)
            return INCOMPLETE;
          semis++;
          digits = 0;
        } else
          return INCOMPLETE;
      }
      if (semis != 2 || digits == 0)
        return INCOMPLETE;
      return (last == 'M' || last == 'm') ? COMPLETE : INCOMPLETE;
    }
    return COMPLETE;
  }
  return INCOMPLETE;
}

static int is_complete_sequence(const uint8_t *d, size_t n) {
  if (d[0] != ESC)
    return NOT_ESCAPE;
  if (n == 1)
    return INCOMPLETE;
  switch (d[1]) {
  case '[':
    return is_complete_csi(d, n);
  case ']':
    if (n >= 3 &&
        (d[n - 1] == 0x07 || (n >= 3 && d[n - 2] == ESC && d[n - 1] == '\\'))) {
      /* Terminator must be BEL or ESC \. */
      if (d[n - 1] == 0x07)
        return COMPLETE;
      return n >= 4 ? COMPLETE : INCOMPLETE;
    }
    return INCOMPLETE;
  case 'P':
  case '_':
    if (n >= 3 && d[n - 2] == ESC && d[n - 1] == '\\')
      return COMPLETE;
    return INCOMPLETE;
  case 'O':
    return n >= 3 ? COMPLETE : INCOMPLETE;
  default:
    return n == 2 ? COMPLETE : COMPLETE; /* ESC + single char */
  }
}

/* ------------------------------------------------------------------ */
/* Emission                                                            */
/* ------------------------------------------------------------------ */

static void emit_sequence(pangu_tui_buffer *b, const uint8_t *data,
                          size_t len) {
  if (b->on_sequence)
    b->on_sequence(b->ctx, data, len);
}

/* Parse a Kitty CSI-u printable codepoint for duplicate suppression:
 * ESC[<cp>[:<shifted>[:<base>]][;<mod>[:<event>]]u with no modifiers. */
static unsigned parse_kitty_printable(const uint8_t *d, size_t n) {
  if (n < 4 || d[0] != ESC || d[1] != '[' || d[n - 1] != 'u')
    return 0;
  unsigned cp = 0;
  size_t i = 2;
  int groups = 0;
  int mods = 0;
  while (i < n - 1 && d[i] >= '0' && d[i] <= '9') {
    cp = cp * 10 + (unsigned)(d[i] - '0');
    ++i;
  }
  while (i < n - 1 && (d[i] == ':' || d[i] == ';')) {
    if (d[i] == ';')
      mods = 1;
    groups++;
    ++i;
    while (i < n - 1 && d[i] >= '0' && d[i] <= '9')
      ++i;
  }
  if (i != n - 1)
    return 0;
  if (mods || groups > 2 || cp < 32)
    return 0;
  return cp;
}

static void emit_data(pangu_tui_buffer *b, const uint8_t *data, size_t len) {
  if (len == 1 && b->kitty_printable) {
    unsigned cp = 0;
    if (data[0] < 0x80)
      cp = data[0];
    if (cp == b->kitty_printable) {
      b->kitty_printable = 0;
      return;
    }
  }
  b->kitty_printable = parse_kitty_printable(data, len);
  emit_sequence(b, data, len);
}

static void emit_paste(pangu_tui_buffer *b, const uint8_t *content,
                       size_t len) {
  b->kitty_printable = 0;
  if (b->on_paste)
    b->on_paste(b->ctx, content, len);
}

/* ------------------------------------------------------------------ */
/* Extraction                                                          */
/* ------------------------------------------------------------------ */

/* Find how many bytes form the next complete unit starting at d[0].
 * Returns a length, or 0 when more data is needed. */
static size_t next_unit(const uint8_t *d, size_t n) {
  if (d[0] != ESC) {
    /* One UTF-8 codepoint. */
    uint8_t c = d[0];
    if (c < 0x80)
      return 1;
    size_t len = c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (len > n)
      return 0;
    for (size_t i = 1; i < len; ++i) {
      if ((d[i] & 0xC0) != 0x80)
        return 1; /* broken sequence: single byte */
    }
    return len;
  }
  /* Escape sequence. */
  int reply = is_control_string(d, n) && match_reply_header(d, n);
  size_t end = 1;
  while (end <= n) {
    if (reply && end > 2) {
      uint8_t c = d[end - 1];
      if (c == 0x18 || c == 0x1a)
        return end; /* CAN/SUB cancel the string */
      if (c == ESC && end < n && d[end] != '\\') {
        return end - 1; /* new escape sequence aborts the reply */
      }
    }
    int status = is_complete_sequence(d, end);
    if (status == COMPLETE) {
      /* WezTerm escape key: ESC followed by ESC + CSI/OSC/SS3/DCS/APC. */
      if (end == 2 && d[1] == ESC && end < n) {
        uint8_t next = d[end];
        if (next == '[' || next == ']' || next == 'O' || next == 'P' ||
            next == '_') {
          return 1;
        }
      }
      return end;
    }
    if (status == NOT_ESCAPE)
      return end;
    ++end;
  }
  return 0; /* needs more data */
}

/* Extract complete sequences; returns 0 on success, 1 when leftover bytes
 * remain (incomplete tail). */
static int extract_complete(pangu_tui_buffer *b, const uint8_t *data,
                            size_t n) {
  size_t pos = 0;
  while (pos < n) {
    size_t len = next_unit(data + pos, n - pos);
    if (len == 0) {
      /* Incomplete tail: keep it. */
      if (n - pos > b->pending_capacity) {
        uint8_t *p = realloc(b->pending, n - pos);
        if (!p) {
          b->failed = 1;
          return 0;
        }
        b->pending = p;
        b->pending_capacity = n - pos;
      }
      memmove(b->pending, data + pos, n - pos);
      b->pending_len = n - pos;
      return 1;
    }
    emit_data(b, data + pos, len);
    pos += len;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Paste handling                                                      */
/* ------------------------------------------------------------------ */

static int paste_find(const uint8_t *haystack, size_t n, const char *needle) {
  size_t needle_len = strlen(needle);
  if (needle_len > n)
    return -1;
  for (size_t i = 0; i + needle_len <= n; ++i) {
    if (memcmp(haystack + i, needle, needle_len) == 0)
      return (int)i;
  }
  return -1;
}

static void paste_emit(pangu_tui_buffer *b, const uint8_t *data, size_t n);

static void paste_consume(pangu_tui_buffer *b, const uint8_t *data, size_t n) {
  if (n > (size_t)-1 - b->paste_len) {
    b->failed = 1;
    return;
  }
  size_t total = b->paste_len + n;
  if (total > b->paste_capacity) {
    size_t cap = total < 1024 ? 1024 : total;
    uint8_t *p = realloc(b->paste, cap);
    if (!p) {
      b->failed = 1;
      return;
    }
    b->paste = p;
    b->paste_capacity = cap;
  }
  if (n)
    memcpy(b->paste + b->paste_len, data, n);
  b->paste_len = total;
  int end = paste_find(b->paste, total, BRACKETED_PASTE_END);
  if (end < 0)
    return;
  emit_paste(b, b->paste, (size_t)end);
  size_t rest = total - (size_t)end - 6;
  uint8_t *tail = rest ? malloc(rest) : NULL;
  if (rest && !tail) {
    b->failed = 1;
    return;
  }
  if (rest && tail)
    memcpy(tail, b->paste + end + 6, rest);
  b->paste_mode = 0;
  b->paste_len = 0;
  if (tail) {
    paste_emit(b, tail, rest);
    free(tail);
  }
}

static void paste_emit(pangu_tui_buffer *b, const uint8_t *data, size_t n) {
  if (b->paste_mode) {
    paste_consume(b, data, n);
    return;
  }
  int start = paste_find(data, n, BRACKETED_PASTE_START);
  if (start < 0) {
    int leftover = extract_complete(b, data, n);
    (void)leftover;
    return;
  }
  if (start > 0) {
    extract_complete(b, data, (size_t)start);
    b->kitty_printable = 0;
  } else {
    b->kitty_printable = 0;
  }
  size_t start_len = strlen(BRACKETED_PASTE_START);
  b->paste_mode = 1;
  b->paste_len = 0;
  paste_consume(b, data + (size_t)start + start_len,
                n - (size_t)start - start_len);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void pangu_tui_buffer_process(pangu_tui_buffer *buffer, const uint8_t *data,
                              size_t len) {
  if (!buffer || (!data && len))
    return;
  buffer->deadline_ms = 0;
  if (!data || len == 0) {
    if (buffer->pending_len == 0 && buffer->emitting) {
      /* Pi emits an empty data event for empty input; the terminal
       * layer ignores it, so we mirror only the empty-pending case. */
      emit_sequence(buffer, (const uint8_t *)"", 0);
    }
    return;
  }
  if (buffer->emitting)
    return; /* not re-entrant; drop late input */

  if (buffer->failed)
    return;
  if (len > (size_t)-1 - buffer->pending_len) {
    buffer->failed = 1;
    return;
  }
  size_t total = buffer->pending_len + len;
  uint8_t *combined = malloc(total);
  if (!combined) {
    buffer->failed = 1;
    return;
  }
  if (buffer->pending_len)
    memcpy(combined, buffer->pending, buffer->pending_len);
  memcpy(combined + buffer->pending_len, data, len);
  buffer->pending_len = 0;
  buffer->emitting = 1;
  paste_emit(buffer, combined, total);
  free(combined);

  if (buffer->pending_len > 0) {
    if (is_control_string(buffer->pending, buffer->pending_len) &&
        match_reply_header(buffer->pending, buffer->pending_len)) {
      /* Delimited reply: wait for the terminator, no timeout. */
    } else {
      uint32_t timeout = buffer->pending_len == 1 && buffer->pending[0] == ESC
                             ? buffer->escape_timeout_ms
                             : buffer->sequence_timeout_ms;
      /* Host samples monotonic time with tick before feeding each batch. */
      buffer->deadline_ms = buffer->last_ms + timeout;
    }
  }
  buffer->emitting = 0;
}

void pangu_tui_buffer_tick(pangu_tui_buffer *buffer, uint64_t now_ms) {
  if (!buffer)
    return;
  buffer->last_ms = now_ms;
  if (!buffer->deadline_ms || !buffer->pending_len || buffer->emitting ||
      now_ms < buffer->deadline_ms)
    return;
  if (buffer->on_preflush &&
      buffer->on_preflush(buffer->ctx, buffer->pending, buffer->pending_len)) {
    buffer->deadline_ms = now_ms + buffer->sequence_timeout_ms;
    return;
  }
  pangu_tui_buffer_flush(buffer);
}

void pangu_tui_buffer_flush(pangu_tui_buffer *buffer) {
  if (!buffer || buffer->emitting)
    return;
  buffer->deadline_ms = 0;
  if (buffer->pending_len == 0)
    return;
  buffer->emitting = 1;
  emit_sequence(buffer, buffer->pending, buffer->pending_len);
  buffer->emitting = 0;
  buffer->pending_len = 0;
  buffer->kitty_printable = 0;
}

void pangu_tui_buffer_clear(pangu_tui_buffer *buffer) {
  if (!buffer)
    return;
  buffer->pending_len = 0;
  buffer->paste_len = 0;
  buffer->paste_mode = 0;
  buffer->kitty_printable = 0;
  buffer->deadline_ms = 0;
  buffer->failed = 0;
}

int pangu_tui_buffer_failed(const pangu_tui_buffer *b) {
  return b ? b->failed : 1;
}
