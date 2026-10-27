/* tui_input.h — 有界 UTF-8 多行输入编辑（纯状态，无 I/O）。
 *
 * 普通 Enter 请求提交，Alt+Enter 插入换行；粘贴期间不产生任何快捷键，
 * Ctrl-C/Ctrl-D/Enter 一律按内容过滤，未完成粘贴只能由结束标记收尾。
 * 编辑位置为 UTF-8 字节偏移，只停在完整 code point 边界；不实现完整字素簇编辑。
 * 有效文本最多 TUI_INPUT_MAX_BYTES-1 字节；完整字符放不下时整字符拒绝，
 * 粘贴仍继续消费到结束标记。输入解析无超时：普通 ESC/Alt 的下一字节必须紧跟，
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
	TUI_KEY_PASTE     /* 完整粘贴已插入，需要重绘 */
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
	unsigned reserved;
} tui_input;

void tui_input_init(tui_input *in);
void tui_input_reset(tui_input *in);
const char *tui_input_text(const tui_input *in);
size_t tui_input_len(const tui_input *in);
size_t tui_input_cursor(const tui_input *in);
int tui_input_empty(const tui_input *in);
int tui_input_incomplete(const tui_input *in); /* 括号粘贴或转义序列尚未完成 */

/* 每次调用消费恰好一个字节，所有括号标记均可分片。 */
tui_key_kind tui_input_feed(tui_input *in, unsigned char byte);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_INPUT_H */
