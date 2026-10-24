#ifndef PG_TUI_RUNTIME_BRIDGE_H
#define PG_TUI_RUNTIME_BRIDGE_H
#include "tui_runtime_adapter.h"
#include "../../contracts/client.h"
typedef struct {
 pg_client client;
 tui_runtime_ops ops;
 int active, stopped;
 char id[64], name[64];
} pg_tui_bridge;
int pg_tui_bridge_init(pg_tui_bridge *, pg_client);
#endif
