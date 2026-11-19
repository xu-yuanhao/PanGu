#ifndef PANGU_TUI_EDITOR_INTERNAL_H
#define PANGU_TUI_EDITOR_INTERNAL_H
#include <pangu/tui/editor.h>
/* Transfer only history nodes; reject active browsing/search. Both editors
 * remain owned by the caller. Text/selection/undo and queues are untouched. */
pangu_tui_status pg_editor_take_history(pangu_tui_editor *to,
                                        pangu_tui_editor *from);
#endif
