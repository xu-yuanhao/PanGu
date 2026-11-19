/* Process terminal ported from Pi packages/tui/src/terminal.ts, snapshot
 * d5b3de22. POSIX implementation: raw mode via termios, SIGWINCH resize,
 * Kitty keyboard protocol negotiation with modifyOtherKeys fallback. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pangu/tui/buffer.h>
#include <pangu/tui/keys.h>
#include <pangu/tui/terminal.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define KITTY_QUERY "\x1b[>7u\x1b[?u\x1b[c"
#define KITTY_POP "\x1b[<u"
#define MODIFY_OTHER_KEYS_ON "\x1b[>4;2m"
#define MODIFY_OTHER_KEYS_OFF "\x1b[>4;0m"
#define PASTE_ON "\x1b[?2004h"
#define PASTE_OFF "\x1b[?2004l"
#define KEYBOARD_RESPONSE_FRAGMENT_TIMEOUT_MS 150

struct pangu_tui_terminal {
  pthread_mutex_t mutex;
  pthread_t reader_thread;
  atomic_int running;
  atomic_int result;
  int reader_started;
  struct sigaction saved_winch, saved_int, saved_term, saved_hup;
  int signal_saved;
  int is_tty;
  int was_raw;
  struct termios saved_modes;
  int modes_saved;
  int kitty_pushed;
  atomic_int kitty_active;
  int modify_other_keys_active;
  int pending_da;
  /* Reassembly of split Kitty negotiation replies. */
  uint8_t negotiation[32];
  size_t negotiation_len;
  uint64_t negotiation_deadline_ms;
  pangu_tui_input_fn on_input;
  void *input_ctx;
  pangu_tui_resize_fn on_resize;
  void *resize_ctx;
  pangu_tui_buffer *buffer;
  atomic_uint columns;
  atomic_uint rows;
  int exit_sequence_written; /* stop() already restored */
};

#define READ_CHUNK 4096
#define MAX_PASTE_FORWARD 65536

/* ------------------------------------------------------------------ */
/* Static resize fan-out (one terminal per process for now)            */
/* ------------------------------------------------------------------ */

static pangu_tui_terminal *g_terminal;

static volatile sig_atomic_t resize_pending;
static volatile sig_atomic_t stop_pending;
static void handle_stop(int sig) {
  (void)sig;
  stop_pending = 1;
}
static void handle_winch(int sig) {
  (void)sig;
  resize_pending = 1;
}
static void refresh_dimensions(pangu_tui_terminal *t);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void write_all(int fd, const char *data, size_t len) {
  while (len > 0) {
    ssize_t written = write(fd, data, len);
    if (written <= 0) {
      if (written < 0 && errno == EINTR)
        continue;
      return;
    }
    data += written;
    len -= (size_t)written;
  }
}

static void mutex_lock(pangu_tui_terminal *t) { pthread_mutex_lock(&t->mutex); }
static void mutex_unlock(pangu_tui_terminal *t) {
  pthread_mutex_unlock(&t->mutex);
}

static uint64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
}

uint32_t pangu_tui_terminal_escape_timeout_ms(void) {
  const char *configured = getenv("PANGU_TUI_ESC_TIMEOUT");
  if (configured && *configured) {
    char *end = NULL;
    long value = strtol(configured, &end, 10);
    if (end && *end == 0 && value > 0 && value <= 10000)
      return (uint32_t)value;
  }
  if (getenv("SSH_CONNECTION") || getenv("SSH_TTY"))
    return 100;
  return 10;
}

/* ------------------------------------------------------------------ */
/* Kitty keyboard protocol negotiation                                 */
/* ------------------------------------------------------------------ */

typedef enum {
  NEGOTIATION_NONE,
  NEGOTIATION_KITTY_FLAGS,
  NEGOTIATION_DA
} negotiation_kind;

static negotiation_kind parse_negotiation(const uint8_t *d, size_t n) {
  if (n < 3 || d[0] != 0x1b || d[1] != '[')
    return NEGOTIATION_NONE;
  if (n >= 3 && d[2] == '?') {
    /* \x1b[?<flags>u or \x1b[?...c (device attributes) */
    int is_u = d[n - 1] == 'u';
    int is_c = d[n - 1] == 'c';
    if (!is_u && !is_c)
      return NEGOTIATION_NONE;
    int semis = 0, other = 0;
    for (size_t i = 3; i < n - 1; ++i) {
      uint8_t c = d[i];
      if (c >= '0' && c <= '9')
        continue;
      else if (c == ';')
        semis++;
      else
        other = 1;
    }
    if (other || (is_u && semis))
      return NEGOTIATION_NONE;
    return is_u ? NEGOTIATION_KITTY_FLAGS : NEGOTIATION_DA;
  }
  return NEGOTIATION_NONE;
}

static int is_negotiation_prefix(const uint8_t *d, size_t n) {
  if (n == 2 && d[0] == 0x1b && d[1] == '[')
    return 1;
  if (n < 3 || d[0] != 0x1b || d[1] != '[' || d[2] != '?')
    return 0;
  for (size_t i = 3; i < n; ++i) {
    uint8_t c = d[i];
    if (!((c >= '0' && c <= '9') || c == ';'))
      return 0;
  }
  return 1;
}

static void set_kitty(pangu_tui_terminal *t, int active) {
  t->kitty_active = active;
  pangu_tui_keys_set_kitty(active);
}

static void enable_modify_other_keys(pangu_tui_terminal *t) {
  if (t->kitty_active || t->modify_other_keys_active)
    return;
  write_all(STDOUT_FILENO, MODIFY_OTHER_KEYS_ON, strlen(MODIFY_OTHER_KEYS_ON));
  t->modify_other_keys_active = 1;
}

static void disable_modify_other_keys(pangu_tui_terminal *t) {
  if (!t->modify_other_keys_active)
    return;
  write_all(STDOUT_FILENO, MODIFY_OTHER_KEYS_OFF,
            strlen(MODIFY_OTHER_KEYS_OFF));
  t->modify_other_keys_active = 0;
}

static void handle_negotiation(pangu_tui_terminal *t, negotiation_kind kind,
                               const uint8_t *data, size_t len) {
  if (kind == NEGOTIATION_DA) {
    if (t->pending_da == 0) {
      /* A late reply that answers another query: forward as input. */
      if (t->on_input)
        t->on_input(t->input_ctx, data, len);
      return;
    }
    t->pending_da--;
  }
  if (kind == NEGOTIATION_KITTY_FLAGS) {
    unsigned flags = 0;
    for (size_t i = 3; i < len - 1; ++i) {
      flags = flags * 10 + (unsigned)(data[i] - '0');
    }
    if (flags != 0) {
      disable_modify_other_keys(t);
      if (!t->kitty_active)
        set_kitty(t, 1);
    } else {
      enable_modify_other_keys(t);
    }
    return;
  }
  if (!t->kitty_active)
    enable_modify_other_keys(t);
}

static void flush_negotiation_buffer(pangu_tui_terminal *t) {
  if (t->negotiation_len == 0)
    return;
  if (t->on_input)
    t->on_input(t->input_ctx, t->negotiation, t->negotiation_len);
  t->negotiation_len = 0;
}

/* Negotiation reply handling: returns 1 when the sequence was consumed. */
static int consume_negotiation(pangu_tui_terminal *t, const uint8_t *data,
                               size_t len) {
  if (t->negotiation_len > 0) {
    size_t total = t->negotiation_len + len;
    if (total <= sizeof t->negotiation) {
      memcpy(t->negotiation + t->negotiation_len, data, len);
      negotiation_kind kind = parse_negotiation(t->negotiation, total);
      if (kind != NEGOTIATION_NONE) {
        t->negotiation_len = 0;
        t->negotiation_deadline_ms = 0;
        handle_negotiation(t, kind, t->negotiation, total);
        return 1;
      }
      if (is_negotiation_prefix(t->negotiation, total)) {
        t->negotiation_len = total;
        return 1;
      }
      flush_negotiation_buffer(t);
    } else {
      flush_negotiation_buffer(t);
    }
  }
  negotiation_kind kind = parse_negotiation(data, len);
  if (kind != NEGOTIATION_NONE) {
    handle_negotiation(t, kind, data, len);
    return 1;
  }
  if (is_negotiation_prefix(data, len)) {
    memcpy(t->negotiation, data, len);
    t->negotiation_len = len;
    t->negotiation_deadline_ms =
        now_ms() + KEYBOARD_RESPONSE_FRAGMENT_TIMEOUT_MS;
    return 1;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Input pipeline                                                      */
/* ------------------------------------------------------------------ */

static void forward_sequence(void *ctx, const uint8_t *data, size_t len) {
  pangu_tui_terminal *t = ctx;
  mutex_lock(t);
  int consumed = t->running ? consume_negotiation(t, data, len) : 1;
  mutex_unlock(t);
  if (consumed || !t->running || !t->on_input)
    return;
  t->on_input(t->input_ctx, data, len);
}

static void forward_paste(void *ctx, const uint8_t *data, size_t len) {
  pangu_tui_terminal *t = ctx;
  if (!t->running || !t->on_input)
    return;
  /* Re-wrap paste content with bracketed paste markers, as Pi does. */
  static const char start[] = "\x1b[200~";
  static const char end[] = "\x1b[201~";
  if (len > SIZE_MAX - 12)
    return;
  uint8_t *wrapped = malloc(len + 12);
  if (!wrapped) {
    t->result = PANGU_TUI_NOMEM;
    t->running = 0;
    return;
  }
  memcpy(wrapped, start, 6);
  if (len)
    memcpy(wrapped + 6, data, len);
  memcpy(wrapped + 6 + len, end, 6);
  t->on_input(t->input_ctx, wrapped, len + 12);
  free(wrapped);
}

/* On preflush we try to consume split negotiation replies; otherwise let
 * the flush run so ambiguous Alt-key input recovers within the timeout. */
static int try_preflush(void *ctx, const uint8_t *data, size_t len) {
  pangu_tui_terminal *t = ctx;
  if (t->negotiation_len > 0 && is_negotiation_prefix(data, len)) {
    /* Still waiting for the rest of a split reply; do not flush yet. */
    return 1;
  }
  return 0;
}

#define READ_CHUNK 4096

static void *reader_main(void *arg) {
  pangu_tui_terminal *t = arg;
  uint8_t chunk[READ_CHUNK];
  while (t->running) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval timeout = {0, 10000};
    int ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout);
    if (!t->running || stop_pending)
      break;
    if (ready < 0 && errno != EINTR) {
      t->result = PANGU_TUI_IO;
      break;
    }
    if (ready > 0) {
      ssize_t got = read(STDIN_FILENO, chunk, sizeof chunk);
      if (got < 0 && errno == EINTR)
        continue;
      if (got < 0) {
        t->result = PANGU_TUI_IO;
        break;
      }
      if (got == 0)
        break;
      pangu_tui_buffer_tick(t->buffer, now_ms());
      pangu_tui_buffer_process(t->buffer, chunk, (size_t)got);
    }
    pangu_tui_buffer_tick(t->buffer, now_ms());
    if (pangu_tui_buffer_failed(t->buffer)) {
      t->result = PANGU_TUI_NOMEM;
      break;
    }
    if (resize_pending) {
      resize_pending = 0;
      refresh_dimensions(t);
      if (t->on_resize)
        t->on_resize(t->resize_ctx);
    }
    mutex_lock(t);
    if (t->negotiation_deadline_ms && now_ms() >= t->negotiation_deadline_ms) {
      flush_negotiation_buffer(t);
      t->negotiation_deadline_ms = 0;
    }
    mutex_unlock(t);
  }
  t->running = 0;
  /* Zero-length callback reports EOF/error; bytes are never retained. */
  if (t->on_input)
    t->on_input(t->input_ctx, NULL, 0);
  return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

pangu_tui_terminal *pangu_tui_terminal_create(void) {
  pangu_tui_terminal *t = calloc(1, sizeof *t);
  if (!t)
    return NULL;
  pthread_mutex_init(&t->mutex, NULL);
  t->buffer = pangu_tui_buffer_create();
  if (!t->buffer) {
    pthread_mutex_destroy(&t->mutex);
    free(t);
    return NULL;
  }
  pangu_tui_buffer_configure(t->buffer, 0,
                             pangu_tui_terminal_escape_timeout_ms());
  pangu_tui_buffer_set_callbacks(t->buffer, forward_sequence, forward_paste,
                                 try_preflush, t);
  return t;
}

void pangu_tui_terminal_destroy(pangu_tui_terminal *terminal) {
  if (!terminal)
    return;
  pangu_tui_terminal_stop(terminal);
  pangu_tui_buffer_destroy(terminal->buffer);
  pthread_mutex_destroy(&terminal->mutex);
  if (g_terminal == terminal)
    g_terminal = NULL;
  free(terminal);
}

static void refresh_dimensions(pangu_tui_terminal *t) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 &&
      ws.ws_row > 0) {
    t->columns = ws.ws_col;
    t->rows = ws.ws_row;
    return;
  }
  const char *columns = getenv("COLUMNS");
  const char *rows = getenv("LINES");
  t->columns = columns ? (unsigned)atoi(columns) : 80;
  t->rows = rows ? (unsigned)atoi(rows) : 24;
  if (!columns)
    t->columns = 80;
  if (!rows)
    t->rows = 24;
}

pangu_tui_status pangu_tui_terminal_start(pangu_tui_terminal *terminal,
                                          pangu_tui_input_fn on_input,
                                          void *input_ctx,
                                          pangu_tui_resize_fn on_resize,
                                          void *resize_ctx) {
  if (!terminal || !on_input)
    return PANGU_TUI_INVALID;
  mutex_lock(terminal);
  if (terminal->running) {
    mutex_unlock(terminal);
    return PANGU_TUI_INVALID;
  }
  mutex_unlock(terminal);

  terminal->is_tty = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
  if (!terminal->is_tty ||
      (getenv("TERM") && strcmp(getenv("TERM"), "dumb") == 0))
    return PANGU_TUI_UNSUPPORTED;
  if (g_terminal && g_terminal != terminal)
    return PANGU_TUI_INVALID;

  /* Save modes and enter raw mode. */
  if (tcgetattr(STDIN_FILENO, &terminal->saved_modes) != 0)
    return PANGU_TUI_UNSUPPORTED;
  terminal->modes_saved = 1;
  struct termios raw = terminal->saved_modes;
  terminal->was_raw = !(raw.c_lflag & ICANON) && !(raw.c_lflag & ECHO);
  raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN);
  raw.c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(tcflag_t)(OPOST);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
    return PANGU_TUI_UNSUPPORTED;

  terminal->on_input = on_input;
  terminal->input_ctx = input_ctx;
  terminal->on_resize = on_resize;
  terminal->resize_ctx = resize_ctx;
  terminal->exit_sequence_written = 0;
  refresh_dimensions(terminal);

  /* SIGWINCH handling: one terminal per process. */
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = handle_winch;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGWINCH, &sa, &terminal->saved_winch) == 0)
    terminal->signal_saved = 1;
  sa.sa_handler = handle_stop;
  sigaction(SIGINT, &sa, &terminal->saved_int);
  sigaction(SIGTERM, &sa, &terminal->saved_term);
  sigaction(SIGHUP, &sa, &terminal->saved_hup);
  stop_pending = 0;
  g_terminal = terminal;

  /* Best-effort dimension refresh after suspend/resume (Pi behavior). */
  kill(getpid(), SIGWINCH);

  write_all(STDOUT_FILENO, PASTE_ON, strlen(PASTE_ON));

  /* Kitty protocol negotiation. */
  pangu_tui_buffer_clear(terminal->buffer);
  terminal->kitty_pushed = 1;
  terminal->pending_da = 1;
  terminal->negotiation_len = 0;
  write_all(STDOUT_FILENO, KITTY_QUERY, strlen(KITTY_QUERY));

  terminal->result = PANGU_TUI_OK;
  terminal->running = 1;
  if (pthread_create(&terminal->reader_thread, NULL, reader_main, terminal) !=
      0) {
    terminal->running = 0;
    pangu_tui_terminal_stop(terminal);
    return PANGU_TUI_UNSUPPORTED;
  }
  terminal->reader_started = 1;
  return PANGU_TUI_OK;
}

void pangu_tui_terminal_stop(pangu_tui_terminal *terminal) {
  if (!terminal)
    return;
  mutex_lock(terminal);
  int was_running = terminal->reader_started;
  terminal->reader_started = 0;
  terminal->running = 0;
  mutex_unlock(terminal);
  if (was_running) {
    pthread_join(terminal->reader_thread, NULL);
  }

  if (!terminal->modes_saved)
    return;
  if (terminal->signal_saved) {
    sigaction(SIGWINCH, &terminal->saved_winch, NULL);
    sigaction(SIGINT, &terminal->saved_int, NULL);
    sigaction(SIGTERM, &terminal->saved_term, NULL);
    sigaction(SIGHUP, &terminal->saved_hup, NULL);
    terminal->signal_saved = 0;
  }
  if (g_terminal == terminal)
    g_terminal = NULL;
  write_all(STDOUT_FILENO, PASTE_OFF, strlen(PASTE_OFF));
  if (terminal->kitty_pushed || terminal->kitty_active) {
    write_all(STDOUT_FILENO, KITTY_POP, strlen(KITTY_POP));
  }
  terminal->kitty_pushed = 0;
  set_kitty(terminal, 0);
  disable_modify_other_keys(terminal);

  pangu_tui_buffer_clear(terminal->buffer);
  terminal->negotiation_len = 0;
  terminal->negotiation_deadline_ms = 0;
  terminal->pending_da = 0;
  terminal->on_input = NULL;
  terminal->on_resize = NULL;

  /* Drain late input so stray Kitty key releases do not leak to the shell. */
  struct timespec pause = {0, 20 * 1000000};
  fd_set set;
  struct timeval tv = {0, 50 * 1000};
  for (;;) {
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) != 1)
      break;
    uint8_t sink[256];
    ssize_t got = read(STDIN_FILENO, sink, sizeof sink);
    if (got <= 0)
      break;
    nanosleep(&pause, NULL);
    tv.tv_sec = 0;
    tv.tv_usec = 50 * 1000;
  }

  if (terminal->modes_saved) {
    struct termios restore = terminal->saved_modes;
    if (!terminal->was_raw) {
      /* nothing extra */
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &restore);
    terminal->modes_saved = 0;
  }
  terminal->exit_sequence_written = 1;
}

size_t pangu_tui_terminal_exit_sequence(const pangu_tui_terminal *terminal,
                                        char *out, size_t capacity) {
  if (!terminal)
    return 0;
  if (terminal->exit_sequence_written)
    return 0;
  size_t used = 0;
  const char *parts[3];
  size_t count = 0;
  if (terminal->kitty_pushed || terminal->kitty_active)
    parts[count++] = KITTY_POP;
  if (terminal->modify_other_keys_active)
    parts[count++] = MODIFY_OTHER_KEYS_OFF;
  parts[count++] = PASTE_OFF;
  for (size_t i = 0; i < count; ++i) {
    size_t len = strlen(parts[i]);
    if (out && used + len <= capacity)
      memcpy(out + used, parts[i], len);
    used += len;
  }
  return used;
}

int pangu_tui_terminal_kitty_active(const pangu_tui_terminal *terminal) {
  return terminal ? terminal->kitty_active : 0;
}

unsigned pangu_tui_terminal_columns(const pangu_tui_terminal *terminal) {
  return terminal ? terminal->columns : 80;
}

unsigned pangu_tui_terminal_rows(const pangu_tui_terminal *terminal) {
  return terminal ? terminal->rows : 24;
}

void pangu_tui_terminal_write(pangu_tui_terminal *terminal, const uint8_t *data,
                              size_t len) {
  if (!terminal || !data)
    return;
  write_all(STDOUT_FILENO, (const char *)data, len);
}

void pangu_tui_terminal_set_title(pangu_tui_terminal *terminal,
                                  const char *title) {
  if (!terminal || !title)
    return;
  char buf[256];
  int used = snprintf(buf, sizeof buf, "\x1b]0;%s\x07", title);
  if (used > 0)
    write_all(STDOUT_FILENO, buf,
              (size_t)used < sizeof buf ? (size_t)used : sizeof buf - 1);
}

pangu_tui_status pangu_tui_terminal_result(const pangu_tui_terminal *t) {
  return t ? (pangu_tui_status)atomic_load(&t->result) : PANGU_TUI_INVALID;
}
