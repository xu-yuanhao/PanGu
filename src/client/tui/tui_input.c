#include "tui_input.h"

#include "tui_utf8.h"

/* 转义序列吞掉缓冲上限：CSI 参数足够短，超过视为坏序列直接放弃 */
#define TUI_ESC_CAP 16u

/* 处于转义序列中：返回 1 表示该字节已被消费 */
static int esc_feed(tui_input *in, unsigned char b)
{
	switch (in->esc_state) {
	case 0:
		if (b == 0x1B) {
			in->esc_state = 1;
			in->esc_count = 0;
			return 1;
		}
		return 0;
	case 1:
		if (b == '[') {
			in->esc_state = 2;
		} else if (b == 'O') {
			in->esc_state = 3;
		} else {
			in->esc_state = 0; /* Meta+键：按行为吞掉 */
		}
		return 1;
	case 2:
		if (++in->esc_count > TUI_ESC_CAP) {
			in->esc_state = 0;
		} else if (b >= 0x40 && b <= 0x7E) {
			in->esc_state = 0; /* 结束字节 */
		}
		return 1;
	case 3:
		in->esc_state = 0;
		return 1;
	}
	return 1;
}

void tui_input_init(tui_input *in)
{
	in->len = 0;
	in->pending_need = 0;
	in->reject_skip = 0;
	in->overflow_latch = 0;
	in->esc_state = 0;
	in->esc_count = 0;
}

void tui_input_reset(tui_input *in)
{
	in->len = 0;
	in->pending_need = 0;
	in->reject_skip = 0;
	in->overflow_latch = 0;
}

/* 尝试追加整个序列；空间不足返回 -1（整字符拒绝，不写半截）。 */
static int append_seq(tui_input *in, unsigned char lead)
{
	size_t need = tui_utf8_seq_len(lead);

	if (in->len + need > TUI_INPUT_MAX_BYTES)
		return -1;
	in->buf[in->len++] = (char)lead;
	in->pending_need = need - 1;
	return 0;
}

tui_key_kind tui_input_feed(tui_input *in, unsigned char b)
{
	if (esc_feed(in, b))
		return TUI_KEY_IGNORED;

	if (in->reject_skip > 0) {
		if ((b & 0xC0u) == 0x80u) {
			in->reject_skip--;
			return TUI_KEY_IGNORED;
		}
		in->reject_skip = 0; /* 非连续字节：放弃跳过，按新字节处理 */
	}

	if (in->pending_need > 0) {
		in->buf[in->len++] = (char)b;
		in->pending_need--;
		return TUI_KEY_CHAR;
	}

	switch (b) {
	case 0x0D: /* CR */
	case 0x0A: /* LF（粘贴换行本轮视同提交，记录为限制） */
		return TUI_KEY_ENTER;
	case 0x08: /* BS */
	case 0x7F: /* DEL */
		if (in->len > 0) {
			size_t start = tui_utf8_last_seq_start(in->buf, in->len);
			in->len = start;
			return TUI_KEY_BACKSPACE;
		}
		return TUI_KEY_IGNORED;
	case 0x03:
		return TUI_KEY_CTRL_C;
	case 0x04: /* ^D：空行退出，非空行按退格（readline 行为） */
		if (in->len == 0)
			return TUI_KEY_CTRL_D;
		if (in->len > 0) {
			size_t start = tui_utf8_last_seq_start(in->buf, in->len);
			in->len = start;
		}
		return TUI_KEY_BACKSPACE;
	case 0x15: /* ^U 清行 */
		in->len = 0;
		in->overflow_latch = 0;
		return TUI_KEY_CHAR;
	case 0x09: /* Tab 本轮忽略 */
		return TUI_KEY_IGNORED;
	default:
		break;
	}

	if (b < 0x20u)
		return TUI_KEY_IGNORED; /* 其余控制字节忽略 */

	/* 可打印/UTF-8：先看整个序列放不放得下 */
	if (in->pending_need == 0 && (b & 0xC0u) != 0x80u) {
		if (append_seq(in, b) != 0) {
			int first = in->overflow_latch == 0;
			in->reject_skip = tui_utf8_seq_len(b) - 1;
			in->overflow_latch = 1;
			/* 仍满期间只报一次，删除内容后恢复提示 */
			return first ? TUI_KEY_OVERFLOW : TUI_KEY_IGNORED;
		}
		in->overflow_latch = 0;
		return TUI_KEY_CHAR;
	}

	/* 连续字节出现在序列外：按行为吞掉，不污染缓冲 */
	return TUI_KEY_IGNORED;
}
