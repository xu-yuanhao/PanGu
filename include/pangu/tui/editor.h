#ifndef PANGU_TUI_EDITOR_H
#define PANGU_TUI_EDITOR_H
#include <pangu/tui/terminal.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Single-owner UTF-8 editor. All offsets/lengths are bytes at grapheme
 * boundaries. Returned text is borrowed until the next edit/destroy. No
 * callbacks or workers. Mutations validate UTF-8 and reject terminal controls
 * except newline/tab (tabs become spaces, CRLF becomes LF). Failure preserves
 * the current text. */
typedef struct pangu_tui_editor pangu_tui_editor;
typedef enum {
  PG_EDIT_NONE,
  PG_EDIT_CHANGED,
  PG_EDIT_SUBMIT,
  PG_EDIT_CANCEL,
  PG_EDIT_COMPLETE,
  PG_EDIT_EOF,
  PG_EDIT_ERROR
} pg_edit_action;
pangu_tui_editor *pg_editor_create(void);
void pg_editor_destroy(pangu_tui_editor *);
const char *pg_editor_text(const pangu_tui_editor *, size_t *length);
size_t pg_editor_cursor(const pangu_tui_editor *);
size_t pg_editor_anchor(const pangu_tui_editor *);
pangu_tui_status pg_editor_set(pangu_tui_editor *, const char *, size_t);
pangu_tui_status pg_editor_insert(pangu_tui_editor *, const char *, size_t);
pg_edit_action pg_editor_key(pangu_tui_editor *, const uint8_t *, size_t);
/* Submitted history is copied, Unicode-trimmed, ignores empty/adjacent
 * duplicates, and retains the newest 100 entries (Pi). Older/newer navigation
 * preserves the draft text/cursor/selection; a browsing session is one undo
 * unit. Edits and undo leave browsing. Nonzero older selects older entries.
 * Move is invalid during interactive search. */
pangu_tui_status pg_editor_history_move(pangu_tui_editor *, int older);
pangu_tui_status pg_editor_history_add(pangu_tui_editor *, const char *,
                                       size_t);
/* Legacy one-shot, case-sensitive substring search. Does not enter search
 * mode; query is borrowed through return. */
pangu_tui_status pg_editor_search(pangu_tui_editor *, const char *, size_t);
/* Interactive reverse search: begin saves the complete draft and starts with
 * an empty query/no preview. Update copies UTF-8 query, case-folds via
 * utf8proc, restarts at newest, and previews a match. No match/empty query
 * shows saved draft. Step visits distinct matching text, without wrapping at
 * boundaries. Finish(accept!=0) accepts a match as an editable draft, NEVER
 * submits; with no match it keeps search open. Finish(0) cancels and restores
 * draft/cursor/ selection without changing undo. Accept is one undo unit. set()
 * discards search on success. Insert/paste edits the query while searching. All
 * calls are single-owner, synchronous, and preserve visible state on failure.
 * Query returns NULL when inactive; otherwise borrowed until next mutation.
 * matched and length are optional outputs. Keys: Ctrl+R begins/older, Up older,
 * Ctrl+S/Down newer, Backspace removes a grapheme, Ctrl+U clears query,
 * Enter accepts, Escape/Ctrl+C cancels. Other control keys are consumed. */
pangu_tui_status pg_editor_search_begin(pangu_tui_editor *);
pangu_tui_status pg_editor_search_update(pangu_tui_editor *, const char *,
                                         size_t);
pangu_tui_status pg_editor_search_step(pangu_tui_editor *, int older);
pangu_tui_status pg_editor_search_finish(pangu_tui_editor *, int accept);
const char *pg_editor_search_query(const pangu_tui_editor *, size_t *length,
                                   int *matched);
/* Replacement for current whitespace-delimited token, used by host resource,
 * slash-command and path providers. Input copied; no provider-owned data
 * retained. */
pangu_tui_status pg_editor_complete(pangu_tui_editor *, const char *, size_t);
/* Display cells across one UTF-8 grapheme, for shared renderers. */
size_t pg_editor_next(const char *, size_t, size_t);
unsigned pg_editor_width(const char *, size_t);
#ifdef __cplusplus
}
#endif
#endif
