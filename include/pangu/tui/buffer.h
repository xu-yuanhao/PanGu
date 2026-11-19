#ifndef PANGU_TUI_BUFFER_H
#define PANGU_TUI_BUFFER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Stdin sequence reassembly ported from Pi packages/tui/src/stdin-buffer.ts,
 * snapshot d5b3de22. Splits batched input into complete escape sequences and
 * extracts bracketed-paste content. Timers are explicit ticks so tests and
 * event loops can drive time deterministically. Single-owner, callbacks run
 * synchronously on the feeding thread and must not re-enter or destroy it.
 * The owner must call
 * pangu_tui_buffer_tick() regularly (e.g. every few milliseconds). */

typedef struct pangu_tui_buffer pangu_tui_buffer;

/* Called once per complete input sequence (never empty; paste content is
 * routed to on_paste instead). data need not be NUL-terminated. */
typedef void (*pangu_tui_sequence_fn)(void *ctx, const uint8_t *data, size_t len);

/* Called once per bracketed paste with the pasted content (may be empty
 * only for an empty paste payload). */
typedef void (*pangu_tui_paste_fn)(void *ctx, const uint8_t *data, size_t len);

/* Called when buffered bytes should be flushed as input before the timeout
 * expires. Returning non-zero cancels the flush; zero lets it run. This is
 * how the terminal layer consumes negotiation replies. */
typedef int (*pangu_tui_preflush_fn)(void *ctx, const uint8_t *data, size_t len);

pangu_tui_buffer *pangu_tui_buffer_create(void);
void pangu_tui_buffer_destroy(pangu_tui_buffer *buffer);

/* Sequence timeout 50 ms, lone-ESC timeout 10 ms by default (100 ms over
 * SSH is resolved by the terminal layer). Zero resets to defaults. */
void pangu_tui_buffer_configure(pangu_tui_buffer *buffer, uint32_t sequence_timeout_ms,
                                uint32_t escape_timeout_ms);

void pangu_tui_buffer_set_callbacks(pangu_tui_buffer *buffer, pangu_tui_sequence_fn on_sequence,
                                    pangu_tui_paste_fn on_paste, pangu_tui_preflush_fn on_preflush,
                                    void *ctx);

/* Feed raw stdin bytes (may be partial). Emits sequences and paste events
 * through the callbacks. */
void pangu_tui_buffer_process(pangu_tui_buffer *buffer, const uint8_t *data, size_t len);

/* Advance time. Emits timeout flushes due at or before now_ms. now_ms must
 * be non-decreasing across calls. Call once before each process batch to
 * establish the arrival time, then periodically while waiting for input. */
void pangu_tui_buffer_tick(pangu_tui_buffer *buffer, uint64_t now_ms);

/* Flush everything buffered now, bypassing on_preflush. */
void pangu_tui_buffer_flush(pangu_tui_buffer *buffer);

/* Drop all buffered state without emitting. */
void pangu_tui_buffer_clear(pangu_tui_buffer *buffer);

/* Sticky allocation/overflow failure. Owner stops input and diagnoses.
 * clear resets it; never silently submit a truncated paste. */
int pangu_tui_buffer_failed(const pangu_tui_buffer *);
size_t pangu_tui_buffer_pending(const pangu_tui_buffer *buffer);

#ifdef __cplusplus
}
#endif
#endif
