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
#define TUI_LI_INPUT_PENDING_NEED    1032
#define TUI_LI_INPUT_REJECT_SKIP     1036
#define TUI_LI_INPUT_OVERFLOW_LATCH  1040
#define TUI_LI_INPUT_ESC_STATE       1044
#define TUI_LI_INPUT_ESC_COUNT       1048
#define TUI_LI_INPUT_SIZE            1056
#define TUI_LI_INPUT_MAX_BYTES       1024

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
#define TUI_LI_VIEW_CARRY            36
#define TUI_LI_VIEW_CARRY_LEN        40
#define TUI_LI_VIEW_SIZE             48
#define TUI_LI_VIEW_TOOL_SNIPPET     160
#define TUI_LI_VIEW_ERROR_DETAIL     512

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
#define TUI_LI_EV_RUN_STARTED        1
#define TUI_LI_EV_TEXT_DELTA         2
#define TUI_LI_EV_TOOL_STARTED       3
#define TUI_LI_EV_TOOL_FINISHED      4
#define TUI_LI_EV_RUN_FINISHED       5
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
#include "tui_runtime_adapter.h"
#include "tui_state.h"
#include "tui_term.h"
#include "tui_view.h"

#define TUI_ASSERT_OFF(type, field, val) \
	_Static_assert(offsetof(type, field) == (val), #type "." #field " layout drifted")
#define TUI_ASSERT_SIZE(type, val) \
	_Static_assert(sizeof(type) == (val), #type " size drifted")
#define TUI_ASSERT_EQ(a, b) \
	_Static_assert((a) == (b), #a " != " #b)

TUI_ASSERT_OFF(tui_input, buf, TUI_LI_INPUT_BUF);
TUI_ASSERT_OFF(tui_input, len, TUI_LI_INPUT_LEN);
TUI_ASSERT_OFF(tui_input, pending_need, TUI_LI_INPUT_PENDING_NEED);
TUI_ASSERT_OFF(tui_input, reject_skip, TUI_LI_INPUT_REJECT_SKIP);
TUI_ASSERT_OFF(tui_input, overflow_latch, TUI_LI_INPUT_OVERFLOW_LATCH);
TUI_ASSERT_OFF(tui_input, esc_state, TUI_LI_INPUT_ESC_STATE);
TUI_ASSERT_OFF(tui_input, esc_count, TUI_LI_INPUT_ESC_COUNT);
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
TUI_ASSERT_OFF(tui_view, carry, TUI_LI_VIEW_CARRY);
TUI_ASSERT_OFF(tui_view, carry_len, TUI_LI_VIEW_CARRY_LEN);
TUI_ASSERT_SIZE(tui_view, TUI_LI_VIEW_SIZE);
TUI_ASSERT_EQ(TUI_VIEW_TOOL_SNIPPET_BYTES, TUI_LI_VIEW_TOOL_SNIPPET);
TUI_ASSERT_EQ(TUI_VIEW_ERROR_DETAIL_BYTES, TUI_LI_VIEW_ERROR_DETAIL);

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
TUI_ASSERT_EQ(TUI_EV_RUN_STARTED, TUI_LI_EV_RUN_STARTED);
TUI_ASSERT_EQ(TUI_EV_TEXT_DELTA, TUI_LI_EV_TEXT_DELTA);
TUI_ASSERT_EQ(TUI_EV_TOOL_STARTED, TUI_LI_EV_TOOL_STARTED);
TUI_ASSERT_EQ(TUI_EV_TOOL_FINISHED, TUI_LI_EV_TOOL_FINISHED);
TUI_ASSERT_EQ(TUI_EV_RUN_FINISHED, TUI_LI_EV_RUN_FINISHED);
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

#endif /* !__ASSEMBLER__ */
#endif /* PANGU_TUI_LAYOUT_H */
