#ifndef PG_RUNTIME_BRIDGE_LAYOUT_H
#define PG_RUNTIME_BRIDGE_LAYOUT_H
#define PG_B_OPS 32
#define PG_B_ACTIVE 80
#define PG_B_STOPPED 84
#define PG_B_ID 88
#define PG_B_NAME 152
#define PG_B_BYTES 216
#ifndef __ASSEMBLER__
#include "runtime_bridge.h"
#include "tui_layout.h"
_Static_assert(sizeof(pg_tui_bridge)==PG_B_BYTES,"bridge size");
_Static_assert(offsetof(pg_tui_bridge,ops)==PG_B_OPS,"bridge ops");
_Static_assert(offsetof(pg_tui_bridge,active)==PG_B_ACTIVE,"bridge active");
_Static_assert(offsetof(pg_tui_bridge,stopped)==PG_B_STOPPED,"bridge stopped");
_Static_assert(offsetof(pg_tui_bridge,id)==PG_B_ID,"bridge id");
_Static_assert(offsetof(pg_tui_bridge,name)==PG_B_NAME,"bridge name");
_Static_assert(sizeof(pg_event)==56&&sizeof(pg_client)==32,"client ABI");
_Static_assert(offsetof(pg_event,text)==8&&offsetof(pg_event,call_id)==24&&offsetof(pg_event,tool_name)==40,"event slices");
_Static_assert(PG_RUN_STARTED==0&&PG_TEXT_DELTA==1&&PG_TOOL_STARTED==2&&PG_TOOL_FINISHED==3&&PG_RUN_FINISHED==4,"event kinds");
_Static_assert(PG_SUCCESS==0&&PG_ERROR==1&&PG_CANCELLED==2,"outcomes");
#endif
#endif
