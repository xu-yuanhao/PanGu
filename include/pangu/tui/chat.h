#ifndef PANGU_TUI_CHAT_H
#define PANGU_TUI_CHAT_H
#include <pangu/tui/editor.h>
#include <pangu/tui/terminal.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct pangu_tui_chat pangu_tui_chat;
typedef void (*pangu_tui_submit_fn)(void *, const char *, size_t);
typedef enum {
  PG_TUI_PROMPT,
  PG_TUI_STEER,
  PG_TUI_FOLLOW_UP
} pg_tui_input_kind;
typedef struct {
  char *text;
  size_t length;
  pg_tui_input_kind kind;
} pg_tui_input;
/* Input and rendering are synchronized. Terminal reader only edits and queues;
 * poll/run invokes submit on its caller thread OUTSIDE all UI locks. Submit
 * text is borrowed through callback return. Callbacks may append/request quit.
 * A blocking model must periodically drain take_input/take_cancel on its OWN
 * thread (e.g. provider poll hook), never call Core from the reader thread.
 * stop joins the reader and restores terminal before releasing callback
 * context. Destroy requires no concurrent users. Terminal is borrowed through
 * stop. */
pangu_tui_chat *pangu_tui_chat_create(void);
void pangu_tui_chat_destroy(pangu_tui_chat *);
pangu_tui_status pangu_tui_chat_start(pangu_tui_chat *, pangu_tui_terminal *,
                                      pangu_tui_submit_fn, void *);
void pangu_tui_chat_stop(pangu_tui_chat *);
pangu_tui_status pangu_tui_chat_run(pangu_tui_chat *);
void pangu_tui_chat_poll(pangu_tui_chat *);
void pangu_tui_chat_request_quit(pangu_tui_chat *);
int pangu_tui_chat_quitting(pangu_tui_chat *);
/* Safe across threads. Ctrl+C/Escape while busy requests cancel; Ctrl+D on an
 * empty input or /quit requests quit and cancel. Host clears busy after run.
 * While busy Enter steers, Alt+Enter queues follow-up; Shift+Enter inserts LF.
 * During Ctrl+R search, editor owns keys: Enter accepts to edit (not submit),
 * Ctrl+C/Escape restores the draft without cancelling the model or quitting,
 * and Alt+Enter/Tab are consumed. Search query/status is rendered separately.
 */
void pangu_tui_chat_busy(pangu_tui_chat *, int);
/* Owner thread marks the end-event/return gap. Queued inputs stay owned by
 * chat until the host calls busy(0) AFTER its execution call returns. Cancel
 * also defers input, even after take_cancel consumes the request. */
void pangu_tui_chat_settling(pangu_tui_chat *);
int pangu_tui_chat_take_cancel(pangu_tui_chat *);
/* Transfer an owned command to caller; release with input_release after use.
 * Return 1 command, 0 empty or deferred during cancellation/settlement. OOM leaves edited input intact and shows an error.
 */
int pangu_tui_chat_take_input(pangu_tui_chat *, pg_tui_input *);
void pangu_tui_input_release(pg_tui_input *);
/* Modal approval input. Owner opens a nonzero request ID after durable asked;
 * display is rendered by the caller using append (untrusted fields are escaped).
 * While modal, reader consumes all keys/pastes without editing/history/submit.
 * y chooses allow-once, n rejects, Escape/Ctrl+C cancels; Enter never grants.
 * The first choice is latched until owner closes the matching ID. Reader never
 * calls session APIs. Preexisting commands/completion FIFO remain held; choices
 * never enter either queue. Safe across threads, no callbacks/data retained.
 * Open fails if another ID is active. choice returns 0 while waiting/absent,
 * else 1 allow-once, 2 reject, 3 cancel; it does NOT clear the latch. Only the
 * owner may translate it into a durable reply and then close. stop/EOF/quit
 * latch cancellation; host must settle/cancel the request before disposal.
 */
pangu_tui_status pangu_tui_chat_approval_open(pangu_tui_chat *, uint64_t);
int pangu_tui_chat_approval_choice(pangu_tui_chat *, uint64_t);
void pangu_tui_chat_approval_close(pangu_tui_chat *, uint64_t);
/* Explicit, trusted raw-input source: call sink oldest to newest with original
 * submitted USER text (not expanded prompts, assistant/tool output or pending
 * queues). Each slice is borrowed through sink return; sink copies and applies
 * Pi trim/adjacent-dedup/100-entry policy. Source stops on sink error and returns
 * that error. Source and sink are synchronous on import's owner thread; no
 * retained callbacks, no cancellation operation, no model/session mutation.
 * Import is startup-only (before start, no concurrent start/feed/import). It
 * invokes source outside UI locks, stages ownership, then atomically replaces
 * history; failure leaves history/draft/selection/undo/queues unchanged. Source
 * errors cannot be ignored even if the callback incorrectly returns OK.
 * Sources without evidence of original input must decline with UNSUPPORTED.
 */
typedef pangu_tui_status (*pg_tui_history_sink)(void *, const char *, size_t);
typedef pangu_tui_status (*pg_tui_history_source)(void *, pg_tui_history_sink,
                                                void *);
pangu_tui_status pangu_tui_chat_import_history(pangu_tui_chat *,
                                               pg_tui_history_source, void *);
/* Feed one complete terminal sequence, or raw paste payload when paste!=0.
 * Useful to alternate input adapters/tests; copied synchronously. */
void pangu_tui_chat_feed(pangu_tui_chat *, const uint8_t *, size_t, int paste);
/* Append untrusted UTF-8 transcript; terminal controls are neutralized. Safe
 * across threads. No retention of input. This renders lines, not Markdown. */
void pangu_tui_chat_append(pangu_tui_chat *, const char *, size_t);
/* Owner-thread context navigation: clear terminal history/live preview, preserve
 * editor and queued input. Synchronized with reader; no borrowed data. */
void pangu_tui_chat_clear(pangu_tui_chat *);
/* Replace provisional stream preview (copied). Empty clears. Commit finalized
 * messages with append; previews never enter scrollback as duplicate deltas. */
pangu_tui_status pangu_tui_chat_live(pangu_tui_chat *, const char *, size_t);
/* Host completion on poll caller thread. Tab is an ordering barrier: later
 * editing/paste/submission stays in a copied mailbox until poll applies the
 * completion, including input received during the callback. Cancel/EOF remain
 * immediate. Hosts must poll during provider waits before draining input.
 * Copies token before unlocking;
 * returned replacement is malloc-owned by provider and freed by UI. NULL means
 * no match. Callback lives through stop, may query plugin resources/path
 * capabilities. */
typedef char *(*pg_tui_complete_fn)(void *, const char *, size_t, size_t *);
void pangu_tui_chat_completion(pangu_tui_chat *, pg_tui_complete_fn, void *);
#ifdef __cplusplus
}
#endif
#endif
