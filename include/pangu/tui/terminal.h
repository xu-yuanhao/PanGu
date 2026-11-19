#ifndef PANGU_TUI_TERMINAL_H
#define PANGU_TUI_TERMINAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Process terminal ported from Pi packages/tui/src/terminal.ts, snapshot
 * d5b3de22: raw mode, bracketed paste, Kitty keyboard protocol
 * negotiation with modifyOtherKeys fallback, and SIGWINCH resize. The
 * terminal is single-owner. POSIX installs temporary SIGWINCH/INT/TERM/HUP
 * handlers that set flags; reader dispatches resize/EOF on its thread. stop
 * joins the reader, restores saved modes and handlers. No API is signal-safe.
 * Handle fatal process termination externally; it cannot run normal cleanup. */

typedef struct pangu_tui_terminal pangu_tui_terminal;

typedef enum {
    PANGU_TUI_OK = 0,
    PANGU_TUI_INVALID,
    PANGU_TUI_NOMEM,
    PANGU_TUI_UNSUPPORTED, /* no TTY or missing platform capability */
    PANGU_TUI_IO
} pangu_tui_status;

/* Input callbacks borrow bytes through return. len=0 reports input closure
 * or reader failure; query result after stopping for a diagnostic status. */
typedef void (*pangu_tui_input_fn)(void *ctx, const uint8_t *data, size_t len);
typedef void (*pangu_tui_resize_fn)(void *ctx);

pangu_tui_terminal *pangu_tui_terminal_create(void);
void pangu_tui_terminal_destroy(pangu_tui_terminal *terminal);

/* Enter raw mode on stdin and stdout, enable bracketed paste, and start
 * keyboard protocol negotiation. Call not before both are attached to a
 * terminal; input callback fires from the reader thread. */
pangu_tui_status pangu_tui_terminal_start(pangu_tui_terminal *terminal,
                                          pangu_tui_input_fn on_input, void *input_ctx,
                                          pangu_tui_resize_fn on_resize, void *resize_ctx);

/* Restore the saved modes. Safe to call multiple times; also used from
 * normal owner-thread error/exit paths, never from callbacks or signals. */
void pangu_tui_terminal_stop(pangu_tui_terminal *terminal);

/* Reset string written by stop(); pass to restore from a raw atexit hook
 * when stop() could not run. Returns the length, 0 when a full stop already
 * wrote it. */
size_t pangu_tui_terminal_exit_sequence(const pangu_tui_terminal *terminal, char *out, size_t capacity);

/* Esc timeout in ms: PANGU_TUI_ESC_TIMEOUT, then 100 over SSH, else 10. */
uint32_t pangu_tui_terminal_escape_timeout_ms(void);

int pangu_tui_terminal_kitty_active(const pangu_tui_terminal *terminal);
unsigned pangu_tui_terminal_columns(const pangu_tui_terminal *terminal);
unsigned pangu_tui_terminal_rows(const pangu_tui_terminal *terminal);

/* Thread-safe snapshot of reader failure, initially OK. */
pangu_tui_status pangu_tui_terminal_result(const pangu_tui_terminal *);

void pangu_tui_terminal_write(pangu_tui_terminal *terminal, const uint8_t *data, size_t len);
void pangu_tui_terminal_set_title(pangu_tui_terminal *terminal, const char *title);

#ifdef __cplusplus
}
#endif
#endif
