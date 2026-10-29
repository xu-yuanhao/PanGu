/* tui_view.h — 展示渲染（输出 sink 可注入：交互终端或测试内存缓冲）。
 *
 * 设计：默认交互启动使用会话滚动区和底部独立输入区；banner 启用。
 * shell_active 为 0=未启用、1=idle、2=running，shell_body_end 为会话底行。
 * 非交互/独立组件测试继续使用注入 sink；小于五行暂保留旧降级路径。
 * 5–7 行使用单行会话区与按列宽收缩的提示栏。
 * PageUp 使用临时阅读屏，返回恢复会话滚动范围。长草稿在输入滚动范围内
 * 绘制并定位光标。完整消息树/原位工具块与字素簇尚未完成。
 * Markdown/tool/diff 复用同一输出和状态机，不另建演示专用视图。
 * 有界展示：工具 detail 按 code point 边界截断并带明确标记；输入上限由
 * tui_input 保证；系统级上限协调由主 Agent 负责。
 */
#ifndef PANGU_TUI_VIEW_H
#define PANGU_TUI_VIEW_H

#include <stddef.h>
#include <stdint.h>

#include "tui_input.h"
#include "tui_md.h"
#include "tui_runtime_adapter.h"
#include "tui_sink.h"
#include "tui_tool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUI_VIEW_TOOL_SNIPPET_BYTES 160u
#define TUI_VIEW_ERROR_DETAIL_BYTES 512u

typedef struct tui_view {
	tui_out_sink sink;
	int ansi;              /* 1=交互 ANSI 模式；0=降级纯文本（无转义序列） */
	tui_input *editor;     /* 可为 NULL：不绘制编辑行 */
	int editor_on_line;    /* 当前光标停在编辑行（需要挂起/重绘管理） */
	unsigned cols;         /* 终端总列数，最小按 1 处理 */
	unsigned editor_rows;  /* 上次绘制的编辑区视觉行数（含提示行） */
	unsigned cursor_row;   /* 上次绘制后真实光标所在视觉行（0 起） */
	unsigned cursor_col;   /* 真实光标绝对列（0 起，含提示前缀） */
	tui_md md;             /* 模型文本流式 Markdown 渲染状态 */
	tui_tool tool;         /* 工具执行展示状态机（事件经此渲染） */
 tui_out_sink terminal_sink; /* borrowed physical sink; sink above suppresses read-mode writes */
 unsigned reading, terminal_rows;
 size_t transcript_len;
 size_t read_anchor; /* source byte offset << 5 | printable expansion offset */
 size_t read_start; /* raw byte boundary for output received while reading */
 unsigned page_rows, transcript_dropped;
 const tui_tool_observer *tool_observer; /* borrowed for app lifetime */
 unsigned shell_active, shell_body_end; /* stable default interactive regions */
 char transcript[65536]; /* owned raw source bytes; replay always escapes controls */
 uint64_t logo_start_ms, logo_last_ms;
 unsigned logo_aspect, logo_flags, logo_fg, logo_bg, logo_phase;
 unsigned logo_width, logo_height, logo_top, logo_left, logo_dirty;
 unsigned char logo_cells[1600], logo_previous[1600];

} tui_view;

void tui_view_init(tui_view *v, tui_out_sink sink, int ansi, tui_input *editor);
void tui_view_set_size(tui_view *v, unsigned cols);
/* Recompute screen regions after app updates terminal_rows. */
void tui_view_relayout(tui_view *v);
/* Advance welcome animation; returns next deadline in ms, -1 when idle. */
int tui_view_logo_tick(tui_view *v, uint64_t now_ms);

/* 编辑区：多行/窄终端按 code point 宽度重绘并定位真实光标；降级模式无操作。 */
void tui_view_redraw_editor(tui_view *v);

/* 提交定稿：交互模式结束编辑行（文本留在滚动回显），随后清空编辑缓冲。 */
void tui_view_finalize_editor(tui_view *v);

/* 启动横幅与退出说明（两种模式都打印）。 */
void tui_view_help(tui_view *v);
void tui_view_completions(tui_view *v);
void tui_view_history_search(tui_view *v);
void tui_view_page(tui_view *v, int direction);
void tui_view_return_latest(tui_view *v);
void tui_view_user(tui_view *v, const char *text, size_t len);
void tui_view_banner(tui_view *v, const char *runtime_name);
void tui_view_exit_note(tui_view *v, const char *reason);

/* 运行态提示 */
void tui_view_busy_rejected(tui_view *v);
void tui_view_submit_error(tui_view *v);
void tui_view_input_limit(tui_view *v);
void tui_view_cancel_requested(tui_view *v);
void tui_view_poll_error(tui_view *v);
void tui_view_protocol_violation(tui_view *v);

/* 渲染一条 Runtime 事件；返回 0 成功。run_finished 后编辑行重新可用。 */
void tui_view_event(tui_view *v, const tui_event *ev);
/* Startup only: validated borrowed slices, no execution events or state changes. */
void tui_view_history(tui_view *v, const tui_history_entry *entry);

/* Ctrl-O（Pi app.tools.expand）：展开当前保留工具调用的保留输出。 */
void tui_view_tool_expand(tui_view *v);

/* 内部 emit 回调（tui_tool_emit 的实现，视图把自身作为 ctx 传入）：
 * safe=控制字节转义；raw=仅换行翻译（SGR 固定串通道）。 */
void tui_view_safe_put(void *ctx, const char *data, size_t len);
void tui_view_raw_put(void *ctx, const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_VIEW_H */
