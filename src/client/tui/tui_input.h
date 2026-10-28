/* tui_input.h — 有界 UTF-8 多行输入编辑（纯状态，无 I/O）。
 *
 * 普通 Enter 请求提交，Alt+Enter 插入换行；粘贴期间不产生任何快捷键，
 * Ctrl-C/Ctrl-D/Enter 一律按内容过滤，未完成粘贴只能由结束标记收尾。
 * 编辑位置为 UTF-8 字节偏移，只停在完整 code point 边界；不实现完整字素簇编辑。
 * 有效文本最多 TUI_INPUT_MAX_BYTES-1 字节；完整字符放不下时整字符拒绝，
 * 粘贴仍继续消费到结束标记。字节解析不读时钟：普通 ESC/Alt 的下一字节必须紧跟；宿主仅对活动控件的
 * 单独 ESC 使用 10ms 取消截止时间，已进入 CSI/粘贴的分片不受该计时影响。
 * 括号粘贴与转义序列可跨任意读取边界分片。
 */
#ifndef PANGU_TUI_INPUT_H
#define PANGU_TUI_INPUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TUI_INPUT_MAX_BYTES 1024u

typedef enum {
	TUI_KEY_NONE = 0,
	TUI_KEY_CHAR,     /* 文本或逻辑光标已变化，需要重绘 */
	TUI_KEY_ENTER,    /* 请求提交（缓冲可能为空，调用方决定忽略） */
	TUI_KEY_BACKSPACE,
	TUI_KEY_CTRL_C,
	TUI_KEY_CTRL_D,   /* 空缓冲=EOF 退出请求；非空=退格（保留现有语义） */
	TUI_KEY_OVERFLOW, /* 整字符容量拒绝；同一粘贴/连续溢出期间节流 */
	TUI_KEY_IGNORED,
	TUI_KEY_CURSOR,
	TUI_KEY_PASTE,    /* 完整粘贴已插入，需要重绘 */
	TUI_KEY_CTRL_O,   /* Pi app.tools.expand：展开最近完成的工具输出 */
	TUI_KEY_HELP,     /* F1: show implemented keyboard help without changing draft */
	TUI_KEY_COMPLETIONS, /* more than one supported local command */
	TUI_KEY_SEARCH,    /* reverse search preview changed */
 TUI_KEY_PAGE_UP, TUI_KEY_PAGE_DOWN,
 TUI_KEY_WHEEL_UP, TUI_KEY_WHEEL_DOWN
} tui_key_kind;

typedef struct tui_input {
	char buf[TUI_INPUT_MAX_BYTES];
	size_t len;
	size_t cursor;
	unsigned overflow_latch;
	/* 0=普通；1=见 ESC；2=CSI；3=SS3；4=OSC/DCS/APC；5=ST 的 ESC */
	unsigned esc_state;
	unsigned esc_param;     /* CSI 数字参数（截断防溢出） */
	unsigned paste;         /* 1=正在消费 bracketed paste */
	unsigned paste_match;   /* 候选 ESC[201~ 已匹配的字节数 */
	unsigned paste_cr;      /* 粘贴内容中待折叠为 LF 的 CR */
	unsigned utf8_pending;  /* 已插入、待补齐的 UTF-8 连续字节数 */
	unsigned paste_reject;  /* 容量整字符拒绝后仍须吞掉的连续字节数 */
	unsigned enter_cr;      /* 普通 CR 后吞掉一个可选 LF，避免双提交 */
	unsigned history_count;
 int history_index; /* -1 draft, 0 newest; per-client, never persisted here */
 unsigned columns; /* view supplies terminal width; default 80 */
 size_t history_draft_len;
 size_t history_draft_cursor;
 char history_draft[TUI_INPUT_MAX_BYTES];
 unsigned history_lens[100];
 char history[100][TUI_INPUT_MAX_BYTES];
 unsigned preferred_column;
 unsigned preferred_valid;
 unsigned completion_open;
 unsigned completion_selected;
 unsigned history_search_active;
 unsigned history_search_failed;
} tui_input;

void tui_input_init(tui_input *in);
void tui_input_reset(tui_input *in);
/* Call only after Runtime accepted the prompt. Consecutive duplicates omitted. */
void tui_input_remember(tui_input *in);
/* Copy a borrowed prompt without changing draft/parser. Oversize or unsafe
 * UTF-8/control content is omitted as an editor candidate, never truncated. */
void tui_input_history_add(tui_input *in, const char *text, size_t len);
/* History: 100 owned prompts, ECMAScript edge whitespace trimmed, in-memory only.
 * Up/Down move across visual rows using the view width; at the top/bottom
 * they navigate history, restoring the draft and its cursor. Width is code-point
 * based, with the same combining/CJK approximation as the view. */
const char *tui_input_text(const tui_input *in);
size_t tui_input_len(const tui_input *in);
size_t tui_input_cursor(const tui_input *in);
int tui_input_empty(const tui_input *in);
int tui_input_incomplete(const tui_input *in); /* 括号粘贴或转义序列尚未完成 */

/* 每次调用消费恰好一个字节，所有括号标记均可分片。 */
tui_key_kind tui_input_feed(tui_input *in, unsigned char byte);
/* Host calls only after a scoped standalone-ESC deadline. CSI/paste untouched. */
tui_key_kind tui_input_escape_timeout(tui_input *in);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_INPUT_H */
