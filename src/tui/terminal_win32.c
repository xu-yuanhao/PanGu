/* Win32 console adapter. Shared key/editor/chat logic remains C17.
 * Uses console records (no POSIX signals/termios), UTF-16→UTF-8 via Windows,
 * VT output, bounded waits for stop and window-size notifications. */
#include <pangu/tui/keys.h>
#include <pangu/tui/terminal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <windows.h>
struct pangu_tui_terminal {
  HANDLE input, output;
  DWORD input_mode, output_mode;
  UINT input_cp, output_cp;
  uv_thread_t thread;
  atomic_int running;
  atomic_int result;
  atomic_uint columns, rows;
  int saved, reader_started;
  pangu_tui_input_fn on_input;
  void *input_ctx;
  pangu_tui_resize_fn on_resize;
  void *resize_ctx;
};
static volatile LONG interrupted;
static pangu_tui_terminal *active_terminal;
static BOOL WINAPI control(DWORD event) {
  if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT ||
      event == CTRL_CLOSE_EVENT || event == CTRL_LOGOFF_EVENT ||
      event == CTRL_SHUTDOWN_EVENT) {
    InterlockedExchange(&interrupted, 1);
    return TRUE;
  }
  return FALSE;
}
static void dimensions(pangu_tui_terminal *t) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(t->output, &info)) {
    t->columns = (unsigned)(info.srWindow.Right - info.srWindow.Left + 1);
    t->rows = (unsigned)(info.srWindow.Bottom - info.srWindow.Top + 1);
  }
}
static void emit(pangu_tui_terminal *t, const char *s, size_t n) {
  if (t->on_input)
    t->on_input(t->input_ctx, (const uint8_t *)s, n);
}
static void reader(void *ctx) {
  pangu_tui_terminal *t = ctx;
  WCHAR high = 0;
  while (t->running && !InterlockedCompareExchange(&interrupted, 0, 0)) {
    DWORD ready = WaitForSingleObject(t->input, 10);
    if (ready == WAIT_TIMEOUT)
      continue;
    if (ready != WAIT_OBJECT_0)
      break;
    INPUT_RECORD record;
    DWORD count;
    if (!ReadConsoleInputW(t->input, &record, 1, &count) || !count)
      break;
    if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
      dimensions(t);
      if (t->on_resize)
        t->on_resize(t->resize_ctx);
      continue;
    }
    if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
      continue;
    KEY_EVENT_RECORD k = record.Event.KeyEvent;
    unsigned mods = 0;
    if (k.dwControlKeyState & SHIFT_PRESSED)
      mods |= PANGU_KMOD_SHIFT;
    if (k.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
      mods |= PANGU_KMOD_CTRL;
    if (k.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
      mods |= PANGU_KMOD_ALT;
    unsigned cp = 0;
    char final = 0;
    switch (k.wVirtualKeyCode) {
    case VK_LEFT:
      final = 'D';
      break;
    case VK_RIGHT:
      final = 'C';
      break;
    case VK_UP:
      final = 'A';
      break;
    case VK_DOWN:
      final = 'B';
      break;
    case VK_HOME:
      final = 'H';
      break;
    case VK_END:
      final = 'F';
      break;
    case VK_DELETE:
      cp = 57426;
      break;
    case VK_INSERT:
      cp = 57425;
      break;
    case VK_PRIOR:
      cp = 57421;
      break;
    case VK_NEXT:
      cp = 57422;
      break;
    case VK_RETURN:
      cp = 13;
      break;
    case VK_BACK:
      cp = 127;
      break;
    case VK_TAB:
      cp = 9;
      break;
    case VK_ESCAPE:
      cp = 27;
      break;
    default:
      break;
    }
    char bytes[64];
    int n = 0;
    if (final)
      n = snprintf(bytes, sizeof bytes, "\x1b[1;%u%c", mods + 1, final);
    else if (cp)
      n = snprintf(bytes, sizeof bytes, "\x1b[%u;%uu", cp, mods + 1);
    else {
      WCHAR ch = k.uChar.UnicodeChar;
      if (!ch)
        continue;
      if (ch >= 0xD800 && ch <= 0xDBFF) {
        high = ch;
        continue;
      }
      WCHAR text[2];
      int chars = 1;
      text[0] = ch;
      if (ch >= 0xDC00 && ch <= 0xDFFF && high) {
        text[0] = high;
        text[1] = ch;
        chars = 2;
      }
      high = 0;
      /* AltGr produces text, not a Ctrl+Alt shortcut. */
      if (ch < 32 && (mods & PANGU_KMOD_CTRL)) {
        cp = (ch >= 1 && ch <= 26) ? ch + 96 : ch == 0 ? 32 : ch + 64;
        n = snprintf(bytes, sizeof bytes, "\x1b[%u;%uu", cp, mods + 1);
      } else if ((mods & PANGU_KMOD_ALT) && !(mods & PANGU_KMOD_CTRL)) {
        bytes[0] = 27;
        n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, chars,
                                bytes + 1, (int)sizeof bytes - 1, NULL, NULL);
        if (n)
          n++;
      } else
        n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, chars,
                                bytes, (int)sizeof bytes, NULL, NULL);
    }
    if (n > 0 && (size_t)n < sizeof bytes)
      for (WORD i = 0; i < k.wRepeatCount; i++)
        emit(t, bytes, (size_t)n);
  }
  t->running = 0;
  emit(t, NULL, 0);
}
pangu_tui_terminal *pangu_tui_terminal_create(void) {
  pangu_tui_terminal *t = calloc(1, sizeof *t);
  if (t) {
    t->columns = 80;
    t->rows = 24;
  }
  return t;
}
void pangu_tui_terminal_destroy(pangu_tui_terminal *t) {
  if (t) {
    pangu_tui_terminal_stop(t);
    free(t);
  }
}
pangu_tui_status pangu_tui_terminal_start(pangu_tui_terminal *t,
                                          pangu_tui_input_fn input, void *ictx,
                                          pangu_tui_resize_fn resize,
                                          void *rctx) {
  if (!t || !input || t->saved || active_terminal)
    return PANGU_TUI_INVALID;
  t->input = GetStdHandle(STD_INPUT_HANDLE);
  t->output = GetStdHandle(STD_OUTPUT_HANDLE);
  if (!GetConsoleMode(t->input, &t->input_mode) ||
      !GetConsoleMode(t->output, &t->output_mode))
    return PANGU_TUI_UNSUPPORTED;
  t->input_cp = GetConsoleCP();
  t->output_cp = GetConsoleOutputCP();
  t->saved = 1;
  DWORD mode = (t->input_mode | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) &
               ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                 ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
  if (!SetConsoleMode(t->input, mode) ||
      !SetConsoleMode(t->output,
                      t->output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
    pangu_tui_terminal_stop(t);
    return PANGU_TUI_UNSUPPORTED;
  }
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
  t->on_input = input;
  t->input_ctx = ictx;
  t->on_resize = resize;
  t->resize_ctx = rctx;
  dimensions(t);
  InterlockedExchange(&interrupted, 0);
  SetConsoleCtrlHandler(control, TRUE);
  active_terminal = t;
  t->running = 1;
  if (uv_thread_create(&t->thread, reader, t)) {
    t->running = 0;
    pangu_tui_terminal_stop(t);
    return PANGU_TUI_UNSUPPORTED;
  }
  t->reader_started = 1;
  return PANGU_TUI_OK;
}
void pangu_tui_terminal_stop(pangu_tui_terminal *t) {
  if (!t)
    return;
  t->running = 0;
  if (t->reader_started) {
    uv_thread_join(&t->thread);
    t->reader_started = 0;
  }
  if (t->saved) {
    SetConsoleMode(t->input, t->input_mode);
    SetConsoleMode(t->output, t->output_mode);
    SetConsoleCP(t->input_cp);
    SetConsoleOutputCP(t->output_cp);
    t->saved = 0;
  }
  if (active_terminal == t) {
    SetConsoleCtrlHandler(control, FALSE);
    active_terminal = NULL;
  }
  pangu_tui_keys_set_kitty(0);
  t->on_input = NULL;
  t->on_resize = NULL;
}
size_t pangu_tui_terminal_exit_sequence(const pangu_tui_terminal *t, char *out,
                                        size_t n) {
  (void)t;
  (void)out;
  (void)n;
  return 0;
}
uint32_t pangu_tui_terminal_escape_timeout_ms(void) { return 10; }
int pangu_tui_terminal_kitty_active(const pangu_tui_terminal *t) {
  (void)t;
  return 0;
}
unsigned pangu_tui_terminal_columns(const pangu_tui_terminal *t) {
  return t ? t->columns : 80;
}
unsigned pangu_tui_terminal_rows(const pangu_tui_terminal *t) {
  return t ? t->rows : 24;
}
void pangu_tui_terminal_write(pangu_tui_terminal *t, const uint8_t *s,
                              size_t n) {
  if (!t || !t->saved || (!s && n))
    return;
  while (n) {
    DWORD written = 0;
    DWORD take = n > 32768 ? 32768 : (DWORD)n;
    if (!WriteFile(t->output, s, take, &written, NULL) || !written)
      break;
    s += written;
    n -= written;
  }
}
void pangu_tui_terminal_set_title(pangu_tui_terminal *t, const char *title) {
  if (!t || !title)
    return;
  int n =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title, -1, NULL, 0);
  if (!n)
    return;
  WCHAR *w = malloc((size_t)n * sizeof *w);
  if (!w)
    return;
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title, -1, w, n);
  SetConsoleTitleW(w);
  free(w);
}

pangu_tui_status pangu_tui_terminal_result(const pangu_tui_terminal *t) {
  return t ? (pangu_tui_status)atomic_load(&t->result) : PANGU_TUI_INVALID;
}
