#ifndef PG_TUI_RUNTIME_BRIDGE_H
#define PG_TUI_RUNTIME_BRIDGE_H
#include "tui_runtime_adapter.h"
#include "../../runtime/session.h"
typedef struct {
 pg_client client;
 tui_runtime_ops ops;
 int active, stopped;
 char id[64], name[64];
 int (*detach)(void *);
 void *detach_context;
 int stop_error;
 const pg_session *history_session;
 size_t history_count;
 tui_tool_observer tool_observer;
} pg_tui_bridge;
int pg_tui_bridge_init(pg_tui_bridge *, pg_client);
/* Bind a stable committed view before startup attach/poll. No storage ownership. */
int pg_tui_bridge_set_history(pg_tui_bridge *, const pg_session *);
/* Optional owner lifecycle binding: stop observing without requesting abort. */
int pg_tui_bridge_set_detach(pg_tui_bridge *, int (*detach)(void *), void *context);
int pg_tui_bridge_set_tool_observer(pg_tui_bridge *,const tui_tool_observer *);
const tui_tool_observer *pg_tui_bridge_tool_observer(const pg_tui_bridge *);
#endif
