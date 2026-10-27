/* tui_layout.h — TUI 手写汇编与 C 测试共享的结构布局/枚举/平台常量（客户端内部）。
 *
 * 数值只有这一份（TUI_LI_* 宏对汇编与 C 同时可见）；C 编译时用 offsetof/
 * sizeof/枚举值逐项断言它们与各公开头文件的真实布局一致，漂移即编译失败。
 * TUI_LI_ 前缀避免与头文件自己的宏重名；断言是比较两套独立来源的宏。
 */
#ifndef PANGU_TUI_LAYOUT_H
#define PANGU_TUI_LAYOUT_H

/* tui_input（tui_input.h） */
#define TUI_LI_INPUT_BUF             0
#define TUI_LI_INPUT_LEN             1024
#define TUI_LI_INPUT_CURSOR          1032
#define TUI_LI_INPUT_OVERFLOW_LATCH  1040
#define TUI_LI_INPUT_ESC_STATE       1044
#define TUI_LI_INPUT_ESC_PARAM       1048
#define TUI_LI_INPUT_PASTE           1052
#define TUI_LI_INPUT_PASTE_MATCH     1056
#define TUI_LI_INPUT_PASTE_CR        1060
#define TUI_LI_INPUT_UTF8_PENDING    1064
#define TUI_LI_INPUT_PASTE_REJECT    1068
#define TUI_LI_INPUT_ENTER_CR        1072
#define TUI_LI_INPUT_HISTORY_COUNT   1076
#define TUI_LI_INPUT_HISTORY_INDEX   1080
#define TUI_LI_INPUT_COLUMNS         1084
#define TUI_LI_INPUT_PREF_COL        104928
#define TUI_LI_INPUT_PREF_VALID      104932
#define TUI_LI_INPUT_DRAFT_LEN       1088
#define TUI_LI_INPUT_DRAFT_CURSOR    1096
#define TUI_LI_INPUT_DRAFT           1104
#define TUI_LI_INPUT_HISTORY_LENS    2128
#define TUI_LI_INPUT_HISTORY         2528
#define TUI_LI_INPUT_COMPLETION_OPEN 104936
#define TUI_LI_INPUT_COMPLETION_SEL  104940
#define TUI_LI_INPUT_SEARCH_ACTIVE   104944
#define TUI_LI_INPUT_SEARCH_FAILED   104948
#define TUI_LI_INPUT_SIZE            104952
#define TUI_LI_INPUT_MAX_BYTES       1024

/* tui_out_sink（tui_sink.h） */
#define TUI_LI_SINK_CTX              0
#define TUI_LI_SINK_WRITE            8

/* tui_state（tui_state.h） */
#define TUI_LI_STATE_OPS             0
#define TUI_LI_STATE_VIEW            8
#define TUI_LI_STATE_RUNNING         16
#define TUI_LI_STATE_CANCEL_SHOWN    20
#define TUI_LI_STATE_FATAL           24
#define TUI_LI_STATE_HISTORY_LOADED  28
#define TUI_LI_STATE_SIZE            32
#define TUI_LI_STATE_POLL_BATCH      16

/* tui_view（tui_view.h） */
#define TUI_LI_VIEW_SINK_CTX         0
#define TUI_LI_VIEW_SINK_WRITE       8
#define TUI_LI_VIEW_ANSI             16
#define TUI_LI_VIEW_EDITOR           24
#define TUI_LI_VIEW_EDITOR_ON_LINE   32
#define TUI_LI_VIEW_COLS             36
#define TUI_LI_VIEW_EDITOR_ROWS      40
#define TUI_LI_VIEW_CURSOR_ROW       44
#define TUI_LI_VIEW_CURSOR_COL       48
#define TUI_LI_VIEW_MD               56
#define TUI_LI_VIEW_TOOL             5816
#define TUI_LI_VIEW_TERMINAL         24960
#define TUI_LI_VIEW_READING          24976
#define TUI_LI_VIEW_TERMINAL_ROWS    24980
#define TUI_LI_VIEW_TRANSCRIPT_LEN   24984
#define TUI_LI_VIEW_READ_ANCHOR      24992
#define TUI_LI_VIEW_READ_START       25000
#define TUI_LI_VIEW_PAGE_ROWS        25008
#define TUI_LI_VIEW_DROPPED          25012
#define TUI_LI_VIEW_TRANSCRIPT       25016
#define TUI_LI_VIEW_SIZE             90552
#define TUI_LI_VIEW_TOOL_SNIPPET     160
#define TUI_LI_VIEW_ERROR_DETAIL     512

/* tui_md（tui_md.h） */
#define TUI_LI_MD_SINK_CTX           0
#define TUI_LI_MD_SINK_WRITE         8
#define TUI_LI_MD_ANSI               16
#define TUI_LI_MD_COLS               20
#define TUI_LI_MD_LINE               24
#define TUI_LI_MD_LINE_LEN           536
#define TUI_LI_MD_FENCE              540
#define TUI_LI_MD_FENCE_CHAR         544
#define TUI_LI_MD_FENCE_LEN          548
#define TUI_LI_MD_HOLD               552
#define TUI_LI_MD_HOLD_LEN           1064
#define TUI_LI_MD_CAND               1068
#define TUI_LI_MD_CAND_LEN           1580
#define TUI_LI_MD_TABLE_ON           1584
#define TUI_LI_MD_LAST_BLANK         1588
#define TUI_LI_MD_WROTE_ANY          1592
#define TUI_LI_MD_TABLE_ROWS_N       1596
#define TUI_LI_MD_TABLE_LENS         1600
#define TUI_LI_MD_TABLE              1664
#define TUI_LI_MD_SIZE               5760
#define TUI_LI_MD_LINE_MAX           512
#define TUI_LI_MD_TABLE_ROWS         16
#define TUI_LI_MD_ROW_MAX            256

/* tui_tool（tui_tool.h） */
#define TUI_LI_TOOL_EMIT_CTX         0
#define TUI_LI_TOOL_EMIT_PUT         8
#define TUI_LI_TOOL_EMIT_RAW         16
#define TUI_LI_TOOL_ANSI             24
#define TUI_LI_TOOL_ACTIVE           28
#define TUI_LI_TOOL_LAST_FIN         32
#define TUI_LI_TOOL_SLOT0            36
#define TUI_LI_TOOL_SIZE             19144
#define TUI_LI_REC_NAME              0
#define TUI_LI_REC_NAME_LEN          64
#define TUI_LI_REC_ID                68
#define TUI_LI_REC_ID_LEN            132
#define TUI_LI_REC_STATE             136
#define TUI_LI_REC_EXPANDED          140
#define TUI_LI_REC_OUTLINES          144
#define TUI_LI_REC_SHOWN             148
#define TUI_LI_REC_HINTED            152
#define TUI_LI_REC_TAIL_LEN          156
#define TUI_LI_REC_LOSSY             160
#define TUI_LI_REC_LINE              164
#define TUI_LI_REC_LINE_LEN          676
#define TUI_LI_REC_TAIL              680
#define TUI_LI_REC_SIZE              4776
#define TUI_LI_TOOL_NAME_MAX         64
#define TUI_LI_TOOL_ID_MAX           64
#define TUI_LI_TOOL_LINE_MAX         512
#define TUI_LI_TOOL_TAIL_MAX         4096
#define TUI_LI_TOOL_SLOTS            4
#define TUI_LI_TOOL_PREVIEW          10

/* tui_event / tui_runtime_ops（tui_runtime_adapter.h，主 Agent 维护，只读） */
#define TUI_LI_EVENT_KIND            0
#define TUI_LI_EVENT_TEXT            8
#define TUI_LI_EVENT_TEXT_LEN        16
#define TUI_LI_EVENT_CALL_ID         24
#define TUI_LI_EVENT_TOOL_NAME       32
#define TUI_LI_EVENT_TOOL_OK         40
#define TUI_LI_EVENT_DETAIL          48
#define TUI_LI_EVENT_DETAIL_LEN      56
#define TUI_LI_EVENT_RUN_STATUS      64
#define TUI_LI_EVENT_SIZE            72
#define TUI_LI_OPS_NAME              0
#define TUI_LI_OPS_CTX               8
#define TUI_LI_OPS_SUBMIT            16
#define TUI_LI_OPS_CANCEL            24
#define TUI_LI_OPS_POLL              32
#define TUI_LI_OPS_SHUTDOWN          40
#define TUI_LI_OPS_HISTORY           48
#define TUI_LI_OPS_SIZE              56
#define TUI_LI_HISTORY_KIND          0
#define TUI_LI_HISTORY_TEXT          8
#define TUI_LI_HISTORY_TEXT_LEN      16
#define TUI_LI_HISTORY_NAME          24
#define TUI_LI_HISTORY_NAME_LEN      32
#define TUI_LI_HISTORY_ID            40
#define TUI_LI_HISTORY_ID_LEN        48
#define TUI_LI_HISTORY_OK            56
#define TUI_LI_HISTORY_SIZE          64

/* tui_app_config（tui_app.h） */
#define TUI_LI_APP_IN_FD             0
#define TUI_LI_APP_OUT_FD            4
#define TUI_LI_APP_ANSI              8
#define TUI_LI_APP_OPS               16
#define TUI_LI_APP_TICK_MS           24
#define TUI_LI_APP_CFG_SIZE          32

/* tui_term / tui_term_platform（tui_term.h） */
#define TUI_LI_TERM_IN_FD            0
#define TUI_LI_TERM_OUT_FD           4
#define TUI_LI_TERM_SAVED            8
#define TUI_LI_TERM_SAVED_VALID      68
#define TUI_LI_TERM_RAW_ACTIVE       72
#define TUI_LI_TERM_IN_IS_TTY        76
#define TUI_LI_TERM_OUT_IS_TTY       80
#define TUI_LI_TERM_COLS             84
#define TUI_LI_TERM_ROWS             86
#define TUI_LI_TERM_PLATFORM         88
#define TUI_LI_TERM_SIZE             96
#define TUI_LI_PLATFORM_NAME         0
#define TUI_LI_PLATFORM_ENTER_RAW    8
#define TUI_LI_PLATFORM_RESTORE      16
#define TUI_LI_PLATFORM_QUERY_SIZE   24
#define TUI_LI_PLATFORM_SIZE         32

/* 枚举数值（与各头文件 typedef enum 一致，C 侧断言） */
#define TUI_LI_KEY_NONE              0
#define TUI_LI_KEY_CHAR              1
#define TUI_LI_KEY_ENTER             2
#define TUI_LI_KEY_BACKSPACE         3
#define TUI_LI_KEY_CTRL_C            4
#define TUI_LI_KEY_CTRL_D            5
#define TUI_LI_KEY_OVERFLOW          6
#define TUI_LI_KEY_IGNORED           7
#define TUI_LI_KEY_CURSOR            8
#define TUI_LI_KEY_PASTE             9
#define TUI_LI_KEY_CTRL_O            10
#define TUI_LI_KEY_HELP              11
#define TUI_LI_KEY_COMPLETIONS       12
#define TUI_LI_KEY_SEARCH            13
#define TUI_LI_KEY_PAGE_UP           14
#define TUI_LI_KEY_PAGE_DOWN         15
#define TUI_LI_EV_RUN_STARTED        1
#define TUI_LI_EV_TEXT_DELTA         2
#define TUI_LI_EV_TOOL_STARTED       3
#define TUI_LI_EV_TOOL_FINISHED      4
#define TUI_LI_EV_RUN_FINISHED       5
#define TUI_LI_EV_TOOL_OUTPUT        6
#define TUI_LI_RUN_SUCCESS           1
#define TUI_LI_RUN_ERROR             2
#define TUI_LI_RUN_CANCELLED         3
#define TUI_LI_SUBMIT_OK             0
#define TUI_LI_SUBMIT_BUSY           1
#define TUI_LI_SUBMIT_ERROR          2
#define TUI_LI_EXIT_EOF              0
#define TUI_LI_EXIT_CTRL_C_IDLE      1
#define TUI_LI_EXIT_SIGNAL           2
#define TUI_LI_EXIT_POLL_ERROR       3
#define TUI_LI_EXIT_COMMAND          4

/* Linux x86-64 平台常量（glibc 公开接口约定；sigaction 偏移在 ABI 测试中
 * 另有 offsetof 断言，见 tests/client/tui/test_tui_abi.c） */
#define TUI_LI_SA_HANDLER            0
#define TUI_LI_SA_MASK               8
#define TUI_LI_SA_FLAGS              136
#define TUI_LI_SA_RESTORER           144
#define TUI_LI_SIGACTION_SIZE        152
#define TUI_LI_SIGINT                2
#define TUI_LI_SIGHUP                1
#define TUI_LI_SIGTERM               15
#define TUI_LI_SIGWINCH              28
#define TUI_LI_POLLIN                1
#define TUI_LI_POLLPRI               2
#define TUI_LI_POLLERR               8
#define TUI_LI_POLLHUP               16
#define TUI_LI_EINTR                 4
#define TUI_LI_EAGAIN                11

#ifndef __ASSEMBLER__

#include <stddef.h>

#include "tui_app.h"
#include "tui_input.h"
#include "tui_md.h"
#include "tui_runtime_adapter.h"
#include "tui_state.h"
#include "tui_term.h"
#include "tui_tool.h"
#include "tui_view.h"

#define TUI_ASSERT_OFF(type, field, val) \
	_Static_assert(offsetof(type, field) == (val), #type "." #field " layout drifted")
#define TUI_ASSERT_SIZE(type, val) \
	_Static_assert(sizeof(type) == (val), #type " size drifted")
#define TUI_ASSERT_EQ(a, b) \
	_Static_assert((a) == (b), #a " != " #b)

TUI_ASSERT_OFF(tui_input, buf, TUI_LI_INPUT_BUF);
TUI_ASSERT_OFF(tui_input, len, TUI_LI_INPUT_LEN);
TUI_ASSERT_OFF(tui_input, cursor, TUI_LI_INPUT_CURSOR);
TUI_ASSERT_OFF(tui_input, overflow_latch, TUI_LI_INPUT_OVERFLOW_LATCH);
TUI_ASSERT_OFF(tui_input, esc_state, TUI_LI_INPUT_ESC_STATE);
TUI_ASSERT_OFF(tui_input, esc_param, TUI_LI_INPUT_ESC_PARAM);
TUI_ASSERT_OFF(tui_input, paste, TUI_LI_INPUT_PASTE);
TUI_ASSERT_OFF(tui_input, paste_match, TUI_LI_INPUT_PASTE_MATCH);
TUI_ASSERT_OFF(tui_input, paste_cr, TUI_LI_INPUT_PASTE_CR);
TUI_ASSERT_OFF(tui_input, utf8_pending, TUI_LI_INPUT_UTF8_PENDING);
TUI_ASSERT_OFF(tui_input, paste_reject, TUI_LI_INPUT_PASTE_REJECT);
TUI_ASSERT_OFF(tui_input, enter_cr, TUI_LI_INPUT_ENTER_CR);
TUI_ASSERT_OFF(tui_input, history_count, TUI_LI_INPUT_HISTORY_COUNT);
TUI_ASSERT_OFF(tui_input, history_index, TUI_LI_INPUT_HISTORY_INDEX);
TUI_ASSERT_OFF(tui_input, history_draft_len, TUI_LI_INPUT_DRAFT_LEN);
TUI_ASSERT_OFF(tui_input, history_draft_cursor, TUI_LI_INPUT_DRAFT_CURSOR);
TUI_ASSERT_OFF(tui_input, history_draft, TUI_LI_INPUT_DRAFT);
TUI_ASSERT_OFF(tui_input, history_lens, TUI_LI_INPUT_HISTORY_LENS);
TUI_ASSERT_OFF(tui_input, history, TUI_LI_INPUT_HISTORY);
TUI_ASSERT_OFF(tui_input, columns, TUI_LI_INPUT_COLUMNS);
TUI_ASSERT_OFF(tui_input, preferred_column, TUI_LI_INPUT_PREF_COL);
TUI_ASSERT_OFF(tui_input, preferred_valid, TUI_LI_INPUT_PREF_VALID);
TUI_ASSERT_OFF(tui_input, completion_open, TUI_LI_INPUT_COMPLETION_OPEN);
TUI_ASSERT_OFF(tui_input, completion_selected, TUI_LI_INPUT_COMPLETION_SEL);
TUI_ASSERT_OFF(tui_input, history_search_active, TUI_LI_INPUT_SEARCH_ACTIVE);
TUI_ASSERT_OFF(tui_input, history_search_failed, TUI_LI_INPUT_SEARCH_FAILED);
TUI_ASSERT_SIZE(tui_input, TUI_LI_INPUT_SIZE);
TUI_ASSERT_EQ(TUI_INPUT_MAX_BYTES, TUI_LI_INPUT_MAX_BYTES);

TUI_ASSERT_OFF(tui_state, ops, TUI_LI_STATE_OPS);
TUI_ASSERT_OFF(tui_state, view, TUI_LI_STATE_VIEW);
TUI_ASSERT_OFF(tui_state, running, TUI_LI_STATE_RUNNING);
TUI_ASSERT_OFF(tui_state, cancel_notice_shown, TUI_LI_STATE_CANCEL_SHOWN);
TUI_ASSERT_OFF(tui_state, fatal, TUI_LI_STATE_FATAL);
TUI_ASSERT_OFF(tui_state, history_loaded, TUI_LI_STATE_HISTORY_LOADED);
TUI_ASSERT_SIZE(tui_state, TUI_LI_STATE_SIZE);
TUI_ASSERT_EQ(TUI_STATE_POLL_BATCH, TUI_LI_STATE_POLL_BATCH);

TUI_ASSERT_OFF(tui_view, sink.ctx, TUI_LI_VIEW_SINK_CTX);
TUI_ASSERT_OFF(tui_view, sink.write, TUI_LI_VIEW_SINK_WRITE);
TUI_ASSERT_OFF(tui_view, ansi, TUI_LI_VIEW_ANSI);
TUI_ASSERT_OFF(tui_view, editor, TUI_LI_VIEW_EDITOR);
TUI_ASSERT_OFF(tui_view, editor_on_line, TUI_LI_VIEW_EDITOR_ON_LINE);
TUI_ASSERT_OFF(tui_view, cols, TUI_LI_VIEW_COLS);
TUI_ASSERT_OFF(tui_view, editor_rows, TUI_LI_VIEW_EDITOR_ROWS);
TUI_ASSERT_OFF(tui_view, cursor_row, TUI_LI_VIEW_CURSOR_ROW);
TUI_ASSERT_OFF(tui_view, cursor_col, TUI_LI_VIEW_CURSOR_COL);
TUI_ASSERT_OFF(tui_view, md, TUI_LI_VIEW_MD);
TUI_ASSERT_OFF(tui_view, tool, TUI_LI_VIEW_TOOL);
TUI_ASSERT_OFF(tui_view, terminal_sink, TUI_LI_VIEW_TERMINAL);
TUI_ASSERT_OFF(tui_view, reading, TUI_LI_VIEW_READING);
TUI_ASSERT_OFF(tui_view, terminal_rows, TUI_LI_VIEW_TERMINAL_ROWS);
TUI_ASSERT_OFF(tui_view, transcript_len, TUI_LI_VIEW_TRANSCRIPT_LEN);
TUI_ASSERT_OFF(tui_view, read_anchor, TUI_LI_VIEW_READ_ANCHOR);
TUI_ASSERT_OFF(tui_view, read_start, TUI_LI_VIEW_READ_START);
TUI_ASSERT_OFF(tui_view, page_rows, TUI_LI_VIEW_PAGE_ROWS);
TUI_ASSERT_OFF(tui_view, transcript_dropped, TUI_LI_VIEW_DROPPED);
TUI_ASSERT_OFF(tui_view, transcript, TUI_LI_VIEW_TRANSCRIPT);
TUI_ASSERT_SIZE(tui_view, TUI_LI_VIEW_SIZE);
TUI_ASSERT_EQ(TUI_VIEW_TOOL_SNIPPET_BYTES, TUI_LI_VIEW_TOOL_SNIPPET);
TUI_ASSERT_EQ(TUI_VIEW_ERROR_DETAIL_BYTES, TUI_LI_VIEW_ERROR_DETAIL);

TUI_ASSERT_OFF(tui_tool, emit.ctx, TUI_LI_TOOL_EMIT_CTX);
TUI_ASSERT_OFF(tui_tool, emit.put, TUI_LI_TOOL_EMIT_PUT);
TUI_ASSERT_OFF(tui_tool, emit.raw, TUI_LI_TOOL_EMIT_RAW);
TUI_ASSERT_OFF(tui_tool, ansi, TUI_LI_TOOL_ANSI);
TUI_ASSERT_OFF(tui_tool, active, TUI_LI_TOOL_ACTIVE);
TUI_ASSERT_OFF(tui_tool, last_finished, TUI_LI_TOOL_LAST_FIN);
TUI_ASSERT_OFF(tui_tool, slot, TUI_LI_TOOL_SLOT0);
TUI_ASSERT_SIZE(tui_tool, TUI_LI_TOOL_SIZE);
TUI_ASSERT_OFF(tui_tool_rec, name, TUI_LI_REC_NAME);
TUI_ASSERT_OFF(tui_tool_rec, name_len, TUI_LI_REC_NAME_LEN);
TUI_ASSERT_OFF(tui_tool_rec, id, TUI_LI_REC_ID);
TUI_ASSERT_OFF(tui_tool_rec, id_len, TUI_LI_REC_ID_LEN);
TUI_ASSERT_OFF(tui_tool_rec, state, TUI_LI_REC_STATE);
TUI_ASSERT_OFF(tui_tool_rec, expanded, TUI_LI_REC_EXPANDED);
TUI_ASSERT_OFF(tui_tool_rec, out_lines, TUI_LI_REC_OUTLINES);
TUI_ASSERT_OFF(tui_tool_rec, shown, TUI_LI_REC_SHOWN);
TUI_ASSERT_OFF(tui_tool_rec, hinted, TUI_LI_REC_HINTED);
TUI_ASSERT_OFF(tui_tool_rec, tail_len, TUI_LI_REC_TAIL_LEN);
TUI_ASSERT_OFF(tui_tool_rec, lossy, TUI_LI_REC_LOSSY);
TUI_ASSERT_OFF(tui_tool_rec, line, TUI_LI_REC_LINE);
TUI_ASSERT_OFF(tui_tool_rec, line_len, TUI_LI_REC_LINE_LEN);
TUI_ASSERT_OFF(tui_tool_rec, tail, TUI_LI_REC_TAIL);
TUI_ASSERT_SIZE(tui_tool_rec, TUI_LI_REC_SIZE);
TUI_ASSERT_EQ(TUI_TOOL_NAME_MAX, TUI_LI_TOOL_NAME_MAX);
TUI_ASSERT_EQ(TUI_TOOL_ID_MAX, TUI_LI_TOOL_ID_MAX);
TUI_ASSERT_EQ(TUI_TOOL_LINE_MAX, TUI_LI_TOOL_LINE_MAX);
TUI_ASSERT_EQ(TUI_TOOL_TAIL_MAX, TUI_LI_TOOL_TAIL_MAX);
TUI_ASSERT_EQ(TUI_TOOL_SLOTS, TUI_LI_TOOL_SLOTS);
TUI_ASSERT_EQ(TUI_TOOL_PREVIEW_LINES, TUI_LI_TOOL_PREVIEW);

TUI_ASSERT_OFF(tui_md, sink.ctx, TUI_LI_MD_SINK_CTX);
TUI_ASSERT_OFF(tui_md, sink.write, TUI_LI_MD_SINK_WRITE);
TUI_ASSERT_OFF(tui_md, ansi, TUI_LI_MD_ANSI);
TUI_ASSERT_OFF(tui_md, cols, TUI_LI_MD_COLS);
TUI_ASSERT_OFF(tui_md, line, TUI_LI_MD_LINE);
TUI_ASSERT_OFF(tui_md, line_len, TUI_LI_MD_LINE_LEN);
TUI_ASSERT_OFF(tui_md, fence, TUI_LI_MD_FENCE);
TUI_ASSERT_OFF(tui_md, fence_char, TUI_LI_MD_FENCE_CHAR);
TUI_ASSERT_OFF(tui_md, fence_len, TUI_LI_MD_FENCE_LEN);
TUI_ASSERT_OFF(tui_md, hold, TUI_LI_MD_HOLD);
TUI_ASSERT_OFF(tui_md, hold_len, TUI_LI_MD_HOLD_LEN);
TUI_ASSERT_OFF(tui_md, cand, TUI_LI_MD_CAND);
TUI_ASSERT_OFF(tui_md, cand_len, TUI_LI_MD_CAND_LEN);
TUI_ASSERT_OFF(tui_md, table_on, TUI_LI_MD_TABLE_ON);
TUI_ASSERT_OFF(tui_md, last_blank, TUI_LI_MD_LAST_BLANK);
TUI_ASSERT_OFF(tui_md, wrote_any, TUI_LI_MD_WROTE_ANY);
TUI_ASSERT_OFF(tui_md, table_rows, TUI_LI_MD_TABLE_ROWS_N);
TUI_ASSERT_OFF(tui_md, table_lens, TUI_LI_MD_TABLE_LENS);
TUI_ASSERT_OFF(tui_md, table, TUI_LI_MD_TABLE);
TUI_ASSERT_SIZE(tui_md, TUI_LI_MD_SIZE);
TUI_ASSERT_EQ(TUI_MD_LINE_MAX, TUI_LI_MD_LINE_MAX);
TUI_ASSERT_EQ(TUI_MD_TABLE_ROWS, TUI_LI_MD_TABLE_ROWS);
TUI_ASSERT_EQ(TUI_MD_ROW_MAX, TUI_LI_MD_ROW_MAX);

TUI_ASSERT_OFF(tui_event, kind, TUI_LI_EVENT_KIND);
TUI_ASSERT_OFF(tui_event, text, TUI_LI_EVENT_TEXT);
TUI_ASSERT_OFF(tui_event, text_len, TUI_LI_EVENT_TEXT_LEN);
TUI_ASSERT_OFF(tui_event, tool_call_id, TUI_LI_EVENT_CALL_ID);
TUI_ASSERT_OFF(tui_event, tool_name, TUI_LI_EVENT_TOOL_NAME);
TUI_ASSERT_OFF(tui_event, tool_ok, TUI_LI_EVENT_TOOL_OK);
TUI_ASSERT_OFF(tui_event, detail, TUI_LI_EVENT_DETAIL);
TUI_ASSERT_OFF(tui_event, detail_len, TUI_LI_EVENT_DETAIL_LEN);
TUI_ASSERT_OFF(tui_event, run_status, TUI_LI_EVENT_RUN_STATUS);
TUI_ASSERT_SIZE(tui_event, TUI_LI_EVENT_SIZE);

TUI_ASSERT_OFF(tui_runtime_ops, name, TUI_LI_OPS_NAME);
TUI_ASSERT_OFF(tui_runtime_ops, ctx, TUI_LI_OPS_CTX);
TUI_ASSERT_OFF(tui_runtime_ops, submit, TUI_LI_OPS_SUBMIT);
TUI_ASSERT_OFF(tui_runtime_ops, cancel, TUI_LI_OPS_CANCEL);
TUI_ASSERT_OFF(tui_runtime_ops, poll, TUI_LI_OPS_POLL);
TUI_ASSERT_OFF(tui_runtime_ops, shutdown, TUI_LI_OPS_SHUTDOWN);
TUI_ASSERT_OFF(tui_runtime_ops, history, TUI_LI_OPS_HISTORY);
TUI_ASSERT_SIZE(tui_runtime_ops, TUI_LI_OPS_SIZE);
TUI_ASSERT_OFF(tui_history_entry, kind, TUI_LI_HISTORY_KIND);
TUI_ASSERT_OFF(tui_history_entry, text, TUI_LI_HISTORY_TEXT);
TUI_ASSERT_OFF(tui_history_entry, text_len, TUI_LI_HISTORY_TEXT_LEN);
TUI_ASSERT_OFF(tui_history_entry, tool_name, TUI_LI_HISTORY_NAME);
TUI_ASSERT_OFF(tui_history_entry, tool_name_len, TUI_LI_HISTORY_NAME_LEN);
TUI_ASSERT_OFF(tui_history_entry, call_id, TUI_LI_HISTORY_ID);
TUI_ASSERT_OFF(tui_history_entry, call_id_len, TUI_LI_HISTORY_ID_LEN);
TUI_ASSERT_OFF(tui_history_entry, tool_ok, TUI_LI_HISTORY_OK);
TUI_ASSERT_SIZE(tui_history_entry, TUI_LI_HISTORY_SIZE);

TUI_ASSERT_OFF(tui_app_config, in_fd, TUI_LI_APP_IN_FD);
TUI_ASSERT_OFF(tui_app_config, out_fd, TUI_LI_APP_OUT_FD);
TUI_ASSERT_OFF(tui_app_config, ansi, TUI_LI_APP_ANSI);
TUI_ASSERT_OFF(tui_app_config, ops, TUI_LI_APP_OPS);
TUI_ASSERT_OFF(tui_app_config, poll_tick_ms, TUI_LI_APP_TICK_MS);
TUI_ASSERT_SIZE(tui_app_config, TUI_LI_APP_CFG_SIZE);

TUI_ASSERT_OFF(tui_term, in_fd, TUI_LI_TERM_IN_FD);
TUI_ASSERT_OFF(tui_term, out_fd, TUI_LI_TERM_OUT_FD);
TUI_ASSERT_OFF(tui_term, saved, TUI_LI_TERM_SAVED);
TUI_ASSERT_OFF(tui_term, saved_valid, TUI_LI_TERM_SAVED_VALID);
TUI_ASSERT_OFF(tui_term, raw_active, TUI_LI_TERM_RAW_ACTIVE);
TUI_ASSERT_OFF(tui_term, in_is_tty, TUI_LI_TERM_IN_IS_TTY);
TUI_ASSERT_OFF(tui_term, out_is_tty, TUI_LI_TERM_OUT_IS_TTY);
TUI_ASSERT_OFF(tui_term, cols, TUI_LI_TERM_COLS);
TUI_ASSERT_OFF(tui_term, rows, TUI_LI_TERM_ROWS);
TUI_ASSERT_OFF(tui_term, platform, TUI_LI_TERM_PLATFORM);
TUI_ASSERT_SIZE(tui_term, TUI_LI_TERM_SIZE);

TUI_ASSERT_OFF(tui_term_platform, name, TUI_LI_PLATFORM_NAME);
TUI_ASSERT_OFF(tui_term_platform, enter_raw, TUI_LI_PLATFORM_ENTER_RAW);
TUI_ASSERT_OFF(tui_term_platform, restore, TUI_LI_PLATFORM_RESTORE);
TUI_ASSERT_OFF(tui_term_platform, query_size, TUI_LI_PLATFORM_QUERY_SIZE);
TUI_ASSERT_SIZE(tui_term_platform, TUI_LI_PLATFORM_SIZE);

TUI_ASSERT_EQ(TUI_KEY_NONE, TUI_LI_KEY_NONE);
TUI_ASSERT_EQ(TUI_KEY_CHAR, TUI_LI_KEY_CHAR);
TUI_ASSERT_EQ(TUI_KEY_ENTER, TUI_LI_KEY_ENTER);
TUI_ASSERT_EQ(TUI_KEY_BACKSPACE, TUI_LI_KEY_BACKSPACE);
TUI_ASSERT_EQ(TUI_KEY_CTRL_C, TUI_LI_KEY_CTRL_C);
TUI_ASSERT_EQ(TUI_KEY_CTRL_D, TUI_LI_KEY_CTRL_D);
TUI_ASSERT_EQ(TUI_KEY_OVERFLOW, TUI_LI_KEY_OVERFLOW);
TUI_ASSERT_EQ(TUI_KEY_IGNORED, TUI_LI_KEY_IGNORED);
TUI_ASSERT_EQ(TUI_KEY_CURSOR, TUI_LI_KEY_CURSOR);
TUI_ASSERT_EQ(TUI_KEY_PASTE, TUI_LI_KEY_PASTE);
TUI_ASSERT_EQ(TUI_KEY_CTRL_O, TUI_LI_KEY_CTRL_O);
TUI_ASSERT_EQ(TUI_KEY_HELP, TUI_LI_KEY_HELP);
TUI_ASSERT_EQ(TUI_KEY_COMPLETIONS, TUI_LI_KEY_COMPLETIONS);
TUI_ASSERT_EQ(TUI_KEY_SEARCH, TUI_LI_KEY_SEARCH);
TUI_ASSERT_EQ(TUI_KEY_PAGE_UP, TUI_LI_KEY_PAGE_UP);
TUI_ASSERT_EQ(TUI_KEY_PAGE_DOWN, TUI_LI_KEY_PAGE_DOWN);
TUI_ASSERT_EQ(TUI_EV_RUN_STARTED, TUI_LI_EV_RUN_STARTED);
TUI_ASSERT_EQ(TUI_EV_TEXT_DELTA, TUI_LI_EV_TEXT_DELTA);
TUI_ASSERT_EQ(TUI_EV_TOOL_STARTED, TUI_LI_EV_TOOL_STARTED);
TUI_ASSERT_EQ(TUI_EV_TOOL_FINISHED, TUI_LI_EV_TOOL_FINISHED);
TUI_ASSERT_EQ(TUI_EV_RUN_FINISHED, TUI_LI_EV_RUN_FINISHED);
TUI_ASSERT_EQ(TUI_EV_TOOL_OUTPUT, TUI_LI_EV_TOOL_OUTPUT);
TUI_ASSERT_EQ(TUI_RUN_SUCCESS, TUI_LI_RUN_SUCCESS);
TUI_ASSERT_EQ(TUI_RUN_ERROR, TUI_LI_RUN_ERROR);
TUI_ASSERT_EQ(TUI_RUN_CANCELLED, TUI_LI_RUN_CANCELLED);
TUI_ASSERT_EQ(TUI_SUBMIT_OK, TUI_LI_SUBMIT_OK);
TUI_ASSERT_EQ(TUI_SUBMIT_BUSY, TUI_LI_SUBMIT_BUSY);
TUI_ASSERT_EQ(TUI_SUBMIT_ERROR, TUI_LI_SUBMIT_ERROR);
TUI_ASSERT_EQ(TUI_EXIT_EOF, TUI_LI_EXIT_EOF);
TUI_ASSERT_EQ(TUI_EXIT_CTRL_C_IDLE, TUI_LI_EXIT_CTRL_C_IDLE);
TUI_ASSERT_EQ(TUI_EXIT_SIGNAL, TUI_LI_EXIT_SIGNAL);
TUI_ASSERT_EQ(TUI_EXIT_POLL_ERROR, TUI_LI_EXIT_POLL_ERROR);
TUI_ASSERT_EQ(TUI_EXIT_COMMAND, TUI_LI_EXIT_COMMAND);

#endif /* !__ASSEMBLER__ */
#endif /* PANGU_TUI_LAYOUT_H */
