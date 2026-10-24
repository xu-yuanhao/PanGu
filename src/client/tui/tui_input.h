/* tui_input.h — 一行 UTF-8 输入编辑（纯状态，无 I/O）。
 *
 * 本轮范围：行尾编辑、Enter 提交、退格删除一个 code point（不拆坏 UTF-8）、
 * ^U 清行、^C/^D 控制键、显式字节长度上限（不足容纳整个字符则整字符拒绝）、
 * ESC 序列吞掉（同一字节流内的 CSI/SS3；孤立 ESC 视为无操作）。
 * 不做：多行编辑、行中光标移动、宽字符字素编辑、粘贴括号模式（记录为限制）。
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
	TUI_KEY_CHAR,     /* 已追加/删除内容，需要重绘 */
	TUI_KEY_ENTER,    /* 请求提交（缓冲可能为空，调用方决定忽略） */
	TUI_KEY_BACKSPACE,
	TUI_KEY_CTRL_C,
	TUI_KEY_CTRL_D,   /* EOF/退出请求（缓冲为空时才产生） */
	TUI_KEY_OVERFLOW, /* 超长拒绝（调用方据此提示一次；节流见 feed 说明） */
	TUI_KEY_IGNORED   /* 转义序列/Tab/其他控制字节 */
} tui_key_kind;

typedef struct tui_input {
	char buf[TUI_INPUT_MAX_BYTES];
	size_t len;
	/* 内部状态：多字节序列等待与被拒序列的剩余连续字节 */
	unsigned pending_need;   /* 等待中的连续字节数 */
	unsigned reject_skip;    /* 拒绝后需吞掉的连续字节数 */
	unsigned overflow_latch; /* 上限提示节流：0=可提示 1=已提示 */
	/* ESC 序列吞掉状态机：0=无 1=见ESC 2=CSI中 3=SS3中 */
	unsigned esc_state;
	unsigned esc_count;
} tui_input;

/* 查询 helper 为 tui_input.S 的汇编导出（不再以 static inline 留在头文件，
 * 避免同一模块出现第二份实现）。 */
void tui_input_init(tui_input *in);
void tui_input_reset(tui_input *in);
const char *tui_input_text(const tui_input *in);
size_t tui_input_len(const tui_input *in);
int tui_input_empty(const tui_input *in);

/* 喂入一个输入字节，返回一个按键事件。
 * TUI_KEY_OVERFLOW 带节流：内容未变（仍满）期间只报一次，删除后恢复。 */
tui_key_kind tui_input_feed(tui_input *in, unsigned char byte);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_INPUT_H */
