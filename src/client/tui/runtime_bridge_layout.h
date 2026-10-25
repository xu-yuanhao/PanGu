#ifndef PG_RUNTIME_BRIDGE_LAYOUT_H
#define PG_RUNTIME_BRIDGE_LAYOUT_H
#include "../../runtime/session_layout.h"
#define PG_B_OPS 32
#define PG_B_ACTIVE 88
#define PG_B_STOPPED 92
#define PG_B_ID 96
#define PG_B_NAME 160
#define PG_B_DETACH 224
#define PG_B_DETACH_CONTEXT 232
#define PG_B_STOP_ERROR 240
#define PG_B_HISTORY 248
#define PG_B_HISTORY_COUNT 256
#define PG_B_BYTES 264
#ifndef __ASSEMBLER__
#include "runtime_bridge.h"
#include "tui_layout.h"
_Static_assert(sizeof(pg_tui_bridge)==PG_B_BYTES,"bridge size");
_Static_assert(offsetof(pg_tui_bridge,ops)==PG_B_OPS,"bridge ops");
_Static_assert(offsetof(pg_tui_bridge,active)==PG_B_ACTIVE,"bridge active");
_Static_assert(offsetof(pg_tui_bridge,stopped)==PG_B_STOPPED,"bridge stopped");
_Static_assert(offsetof(pg_tui_bridge,id)==PG_B_ID,"bridge id");
_Static_assert(offsetof(pg_tui_bridge,name)==PG_B_NAME,"bridge name");
_Static_assert(offsetof(pg_tui_bridge,detach)==PG_B_DETACH&&offsetof(pg_tui_bridge,detach_context)==PG_B_DETACH_CONTEXT,"bridge detach");
_Static_assert(offsetof(pg_tui_bridge,stop_error)==PG_B_STOP_ERROR,"bridge stop error");
_Static_assert(offsetof(pg_tui_bridge,history_session)==PG_B_HISTORY&&offsetof(pg_tui_bridge,history_count)==PG_B_HISTORY_COUNT,"history binding");
_Static_assert(sizeof(pg_session)==PG_S_BYTES&&sizeof(pg_message)==PG_MSG_BYTES,"history view layout");
_Static_assert(offsetof(pg_message,text)==PG_MSG_TEXT&&offsetof(pg_message,size)==PG_MSG_SIZE,"history text offsets");
_Static_assert(offsetof(pg_message,call_id)==PG_MSG_ID&&offsetof(pg_message,tool_name)==PG_MSG_NAME&&offsetof(pg_message,is_error)==PG_MSG_ERROR,"history tool offsets");
_Static_assert(offsetof(pg_session,count)==PG_S_COUNT,"history count offset");
_Static_assert(PG_USER==0&&PG_ASSISTANT==1&&PG_TOOL_CALL==2&&PG_TOOL_RESULT==3,"history roles");
_Static_assert(sizeof(pg_event)==56&&sizeof(pg_client)==32,"client ABI");
_Static_assert(offsetof(pg_event,text)==8&&offsetof(pg_event,call_id)==24&&offsetof(pg_event,tool_name)==40,"event slices");
_Static_assert(PG_RUN_STARTED==0&&PG_TEXT_DELTA==1&&PG_TOOL_STARTED==2&&PG_TOOL_FINISHED==3&&PG_RUN_FINISHED==4,"event kinds");
_Static_assert(PG_SUCCESS==0&&PG_ERROR==1&&PG_CANCELLED==2,"outcomes");
#endif
#endif
