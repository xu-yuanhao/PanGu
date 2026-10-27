/* tui_view.h — 展示渲染（输出 sink 可注入：交互终端或测试内存缓冲）。
 *
 * 设计：主屏滚动回显模式（非 alt-screen），所有状态都是独立输出行，不吞状态。
 * 编辑行与事件输出互斥使用当前行：
 *  - 事件输出前先挂起编辑行（写 "\r\n" 结束该行）；
 *  - 编辑内容变化后清除其视觉行并完整重绘，最后把真实光标移到字节编辑位置；
 *    多行草稿与窄终端自动换行不改变提交字节。
 *  - text_delta 经 tui_md 流式渲染为 Markdown；工具输出仍走原文转义通道
 *    （carry 只服务该通道，最多 3 字节）。
 * 有界展示：工具/错误内容按 code point 边界截断并带明确标记；输入上限由
 * tui_input 保证；系统级上限协调由主 Agent 负责。
 */
#ifndef PANGU_TUI_VIEW_H
#define PANGU_TUI_VIEW_H

#include <stddef.h>

#include "tui_input.h"
#include "tui_md.h"
#include "tui_runtime_adapter.h"
#include "tui_sink.h"

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
	char carry[3];         /* 工具输出通道：跨事件不完整 UTF-8 序列拼接 */
	size_t carry_len;
	unsigned cols;         /* 终端总列数，最小按 1 处理 */
	unsigned editor_rows;  /* 上次绘制的编辑区视觉行数（含提示行） */
	unsigned cursor_row;   /* 上次绘制后真实光标所在视觉行（0 起） */
	unsigned cursor_col;   /* 真实光标绝对列（0 起，含提示前缀） */
	tui_md md;             /* 模型文本流式 Markdown 渲染状态 */
} tui_view;

void tui_view_init(tui_view *v, tui_out_sink sink, int ansi, tui_input *editor);
void tui_view_set_size(tui_view *v, unsigned cols);

/* 编辑区：多行/窄终端按 code point 宽度重绘并定位真实光标；降级模式无操作。 */
void tui_view_redraw_editor(tui_view *v);

/* 提交定稿：交互模式结束编辑行（文本留在滚动回显），随后清空编辑缓冲。 */
void tui_view_finalize_editor(tui_view *v);

/* 启动横幅与退出说明（两种模式都打印）。 */
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

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_VIEW_H */
