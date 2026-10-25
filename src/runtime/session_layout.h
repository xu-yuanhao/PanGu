#ifndef PG_SESSION_LAYOUT_H
#define PG_SESSION_LAYOUT_H
#define PG_MSG_BYTES 4248
#define PG_MSG_SIZE 8
#define PG_MSG_TEXT 16
#define PG_MSG_ID 4112
#define PG_MSG_NAME 4176
#define PG_MSG_ERROR 4240
#define PG_S_COUNT 271872
#define PG_S_SYSTEM 271880
#define PG_S_SYSTEM_SIZE 275976
#define PG_S_BYTES 275984
#define PG_SC_SEQ 16
#define PG_SC_BUSY 24
#define PG_SC_FAILED 28
#define PG_SC_RUN 32
#define PG_SC_PROFILE_SIZE 72
#define PG_SC_PROFILE 80
#define PG_SC_SUB_COUNT 4176
#define PG_SC_SUBS 4184
#define PG_SUB_BYTES 80
#define PG_SUB_KEY 16
#define PG_SC_BYTES 9304
#define PG_CP_BYTES 40
#define PG_CP_PHASE 8
#define PG_CP_TURNS 12
#define PG_CP_CALL 16
#define PG_CP_OUTCOME 24
#define PG_CP_RESTARTS 28
#define PG_CP_INPUT 32
#define PG_CHANGE_BYTES 48
#define PG_SESSION_APPEND_MAX 9
#define PG_SESSION_DATA 56
#define PG_SESSION_CP (PG_SESSION_DATA+PG_SESSION_APPEND_MAX*PG_MSG_BYTES)
#define PG_SESSION_REQUEST (PG_SESSION_CP+PG_CP_BYTES)
#define PG_SESSION_STACK ((PG_SESSION_REQUEST+64+7)/16*16+8)
#ifdef __ASSEMBLER__
.macro PG_NATIVE_FRAME_ALLOC amount
.rept (\amount / 4096)
    sub rsp,4096
    test byte ptr [rsp],0
.endr
.if (\amount % 4096)
    sub rsp,(\amount % 4096)
    test byte ptr [rsp],0
.endif
.endm
#else
#include "session.h"
_Static_assert(PG_SESSION_APPEND_MAX==PG_SESSION_BATCH_LIMIT,"session append bound");
_Static_assert(sizeof(pg_message)==PG_MSG_BYTES&&sizeof(pg_run_checkpoint)==PG_CP_BYTES,"session transaction layout");
_Static_assert(sizeof(pg_session_control)==PG_SC_BYTES&&sizeof(pg_session_change)==PG_CHANGE_BYTES,"session scratch layout");
#endif
#endif
