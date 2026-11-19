#ifndef PANGU_TUI_VIEW_H
#define PANGU_TUI_VIEW_H
#include <pangu/core/state.h>
#include <pangu/tui/chat.h>
#ifdef __cplusplus
extern "C" {
#endif
/* These adapters borrow all Core data only during the call and copy display
 * text synchronously; callable directly as loop_config.event or subscriber.
 * They do not retain/query Core and do not change its state. Thread-safe chat
 * render methods serialize with input. Model/tool terminal escapes are escaped.
 * Message replay includes user/system/tool labels and excludes credentials. */
void pg_tui_core_event(void *chat, pangu_core *, const pangu_event *);
void pg_tui_replay_message(pangu_tui_chat *, const pangu_message *);
#ifdef __cplusplus
}
#endif
#endif
