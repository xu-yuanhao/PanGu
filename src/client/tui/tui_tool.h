/* tui_tool.h — 工具执行/输出/状态展示状态机（纯状态，无终端 I/O，经 emit 输出）。
 *
 * 移植 Pi ToolExecutionComponent/render-utils 的可落库行为（快照 d5b3de2）：
 * 调用头行（工具名+调用 ID）、输出预览（前 10 行实时，其余计数并保留尾部）、
 * 完成/错误状态、"... (N more lines, to expand)" 提示、Ctrl-O 展开保留尾部、
 * 图片附件可访问信息（imageFallback 文本）。与 Pi 的有意差异（滚动回显不可
 * 重写，均记录于 TODO 交接区）：
 *  - Pi 整块组件可原位改写并双向展开/收起；本实现 started 行一次性输出，
 *    展开是把隐藏行重新打印一次，再次展开为无操作。
 *  - Pi 状态用整块背景色；本实现用行内状态标记，ANSI 模式加前景色/加粗。
 *  - Pi 预览按视觉行（折行后）计；本实现按源行计，超 512 字节的源行按
 *    码点边界拆分为多行（与 tui_md 同语义）。
 *  - Pi 展开是全局双向切换；本实现 Ctrl-O 展开当前四个保留槽的所有调用，
 *    新调用继承展开偏好，不重写滚动回显中的已显示内容，暂不收起。
 * 有界性：尾部保留最近 4096 字节（按整行丢弃最旧行，单行超限整行丢弃并
 * 记 lossy）；行缓冲 512 字节；name/id 保留桥接契约内的最多 63 字节。slot 4 个按调用 ID
 * 匹配（并行工具调用），满后轮转置换。
 */
#ifndef PANGU_TUI_TOOL_H
#define PANGU_TUI_TOOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TUI_TOOL_NAME_MAX       64u
#define TUI_TOOL_ID_MAX         64u
#define TUI_TOOL_LINE_MAX       512u
#define TUI_TOOL_TAIL_MAX       4096u
#define TUI_TOOL_SLOTS          4u
#define TUI_TOOL_PREVIEW_LINES  10u

typedef struct tui_tool_emit {
	void *ctx;
	/* 内容段：控制字节转义后输出（工具原文、detail、展开内容） */
	void (*put)(void *ctx, const char *data, size_t len);
	/* 固定串/SGR：仅换行翻译，不转义（SGR 序列含 ESC 必须走此通道） */
	void (*raw)(void *ctx, const char *data, size_t len);
} tui_tool_emit;

typedef struct tui_tool_rec {
	char name[TUI_TOOL_NAME_MAX];
	unsigned name_len;
	char id[TUI_TOOL_ID_MAX];
	unsigned id_len;
	unsigned state;     /* 0 pending (also no reported tool outcome at run end), 1 ok, 2 error */
	unsigned expanded;  /* 已执行过展开 */
	unsigned out_lines; /* 累计输出源行数（含完成时的尾行） */
	unsigned shown;     /* 已实时输出的行数 */
	unsigned hinted;    /* 已输出展开提示 */
	unsigned tail_len;  /* 保留尾部字节数 */
	unsigned lossy;     /* 尾部丢弃过更早输出 */
	char line[TUI_TOOL_LINE_MAX]; /* 行累积缓冲（跨 TOOL_OUTPUT 分片） */
	unsigned line_len;
	char tail[TUI_TOOL_TAIL_MAX]; /* 隐藏行保留尾部（供展开） */
} tui_tool_rec;

typedef struct tui_tool {
	tui_tool_emit emit;
	int ansi;
	unsigned active;        /* 最近使用的 slot 下标 */
	unsigned last_finished; /* 最近完成的 slot；TUI_TOOL_SLOTS=无 */
	tui_tool_rec slot[TUI_TOOL_SLOTS];
 unsigned expanded_preference; /* inherited by every subsequent tool call */
} tui_tool;

void tui_tool_init(tui_tool *t, void *ctx,
                   void (*put)(void *, const char *, size_t),
                   void (*raw)(void *, const char *, size_t), int ansi);

/* 调用头行。返回 1=有输出（视图据此恢复编辑行）。 */
int tui_tool_started(tui_tool *t, const char *name, const char *call_id);

/* 流式输出（可任意分片，行按 '\n' 收束；不完整 UTF-8 留行缓冲拼合）。 */
int tui_tool_output(tui_tool *t, const char *call_id, const char *text, size_t len);

/* 完成状态行：ok 决定 ok/error 标记与颜色；detail 有界 160 字节；
 * 隐藏行 >0 时输出展开提示。 */
int tui_tool_finished(tui_tool *t, const char *call_id, int ok,
		      const char *detail, size_t detail_len);

/* Ctrl-O expands every retained call. Pending calls may be enabled before
 * output arrives; future lines bypass preview. Returns 1 if any call changed.
 * Enables future new calls too; does not collapse prior terminal scrollback. */
int tui_tool_expand(tui_tool *t);

/* Restore a saved result component without output or execution. name/id are
 * borrowed NUL-terminated strings; ok is the saved authoritative outcome. */
void tui_tool_restore(tui_tool *t, const char *name, const char *call_id, int ok);

/* Replace retained canonical-result preview for a known call, then feed chunks
 * through output and end. Preserves outcome/expansion; 0 if ID not retained.
 * End flushes only this call and emits an expansion hint when needed. */
int tui_tool_result_begin(tui_tool *t, const char *call_id);
int tui_tool_result_end(tui_tool *t, const char *call_id);

/* Flush buffered output at an authoritative run terminal. This does not
 * fabricate tool outcomes; only tool_finished carries success/failure.
 * Returns 1 when a buffered line or an expansion hint was emitted. */
int tui_tool_flush(tui_tool *t);

/* 图片附件可访问信息（Pi imageFallback）："[Image: [mime] WxH]"，
 * mime 为 NULL 时用 "image/unknown"。返回写入长度（不含 NUL），放不下返回 0。 */
size_t tui_tool_image_note(char *buf, size_t cap,
			   const char *mime, unsigned w, unsigned h);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_TOOL_H */
