/* test_tui_input.c — 一行 UTF-8 编辑器测试：退格/长度上限/控制键/转义序列。 */
#include "test_util.h"

#include "tui_input.h"

static void feed_str(tui_input *in, const char *s)
{
	while (*s)
		tui_input_feed(in, (unsigned char)*s++);
}

static void test_basic_ascii(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "hello");
	T_CHECK(in.len == 5 && memcmp(in.buf, "hello", 5) == 0);
	T_CHECK(tui_input_feed(&in, '\r') == TUI_KEY_ENTER);
	T_CHECK(in.len == 5); /* 提交后由调用方清空，编辑器不改内容 */

	tui_input_reset(&in);
	T_CHECK(tui_input_feed(&in, '\r') == TUI_KEY_ENTER); /* 空提交仍报 ENTER，由上层忽略 */
	T_CHECK(tui_input_feed(&in, 'a') == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 0);
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_IGNORED); /* 空行退格无效 */
	T_RUN("basic_ascii");
}

static void test_utf8_backspace(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "a中b");
	T_CHECK(in.len == 5); /* 1 + 3(中) + 1(b) */
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 4); /* 删掉 'b' */
	T_CHECK(tui_input_feed(&in, 0x08) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 1); /* 整个 "中" 一次删除，不拆坏 */
	T_CHECK(in.buf[0] == 'a');
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 0);

	/* 分批喂入多字节序列 */
	tui_input_reset(&in);
	T_CHECK(tui_input_feed(&in, 0xE4) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xB8) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xAD) == TUI_KEY_CHAR);
	T_CHECK(in.len == 3);
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 0);
	T_RUN("utf8_backspace");
}

static void test_ctrl_keys(void)
{
	tui_input in;

	tui_input_init(&in);
	T_CHECK(tui_input_feed(&in, 0x04) == TUI_KEY_CTRL_D); /* 空行 ^D = 退出 */
	feed_str(&in, "hi");
	T_CHECK(tui_input_feed(&in, 0x04) == TUI_KEY_BACKSPACE); /* 非空 ^D = 退格 */
	T_CHECK(in.len == 1);
	feed_str(&in, "i");
	T_CHECK(tui_input_feed(&in, 0x15) == TUI_KEY_CHAR); /* ^U 清行 */
	T_CHECK(in.len == 0);
	T_CHECK(tui_input_feed(&in, 0x03) == TUI_KEY_CTRL_C);
	T_CHECK(tui_input_feed(&in, '\n') == TUI_KEY_ENTER); /* LF 也算提交 */
	T_CHECK(tui_input_feed(&in, 0x09) == TUI_KEY_IGNORED); /* Tab 忽略 */
	T_CHECK(tui_input_feed(&in, 0x01) == TUI_KEY_IGNORED); /* 其他控制字节忽略 */
	T_RUN("ctrl_keys");
}

static void test_escape_sequences(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "ab");
	/* 方向键 ESC [ A：整序列吞掉 */
	feed_str(&in, "\x1b[A");
	T_CHECK(in.len == 2);
	/* SS3 ESC O P */
	feed_str(&in, "\x1bOP");
	T_CHECK(in.len == 2);
	/* 孤立 ESC：吞掉；其后一个键按 Meta 组合吞掉（Pi stdin-buffer 行为，
	 * 单行编辑器不使用 Meta，整序列无操作） */
	T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 'c') == TUI_KEY_IGNORED);
	T_CHECK(in.len == 2);
	/* CSI 超长坏序列：吞掉 16 字节上限内的参数，随后放弃该序列；
	 * 放弃触发字节与其后的可打印字节按普通输入进入缓冲（有界、可预期） */
	{
		int i;
		T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
		T_CHECK(tui_input_feed(&in, '[') == TUI_KEY_IGNORED);
		for (i = 0; i < 17; i++)
			T_CHECK(tui_input_feed(&in, '9') == TUI_KEY_IGNORED); /* 16 参数 + 放弃触发字节 */
		T_CHECK(tui_input_feed(&in, 'x') == TUI_KEY_CHAR);
		T_CHECK(tui_input_feed(&in, 'y') == TUI_KEY_CHAR);
		T_CHECK(tui_input_feed(&in, 'z') == TUI_KEY_CHAR);
		T_CHECK(in.len == 5); /* "ab" + xyz */
	}
	T_RUN("escape_sequences");
}

static void test_max_length(void)
{
	tui_input in;
	unsigned i;

	tui_input_init(&in);
	for (i = 0; i < TUI_INPUT_MAX_BYTES; i++)
		T_CHECK(tui_input_feed(&in, 'a') == TUI_KEY_CHAR);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES);
	/* 满：继续输入被拒且节流（第二次不再报 OVERFLOW） */
	T_CHECK(tui_input_feed(&in, 'b') == TUI_KEY_OVERFLOW);
	T_CHECK(tui_input_feed(&in, 'c') == TUI_KEY_IGNORED);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES);
	/* 删一个再放一个 */
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(tui_input_feed(&in, 'd') == TUI_KEY_CHAR);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES);
	/* ^U 清空后解除节流 */
	T_CHECK(tui_input_feed(&in, 0x15) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 'e') == TUI_KEY_CHAR);
	T_CHECK(in.len == 1);
	T_RUN("max_length");
}

static void test_whole_char_reject_at_boundary(void)
{
	tui_input in;
	unsigned i;

	tui_input_init(&in);
	for (i = 0; i < TUI_INPUT_MAX_BYTES - 1; i++)
		(void)tui_input_feed(&in, 'a');
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
	/* "中" 需要 3 字节，只剩 2：整字符拒绝，连续字节被吞，不留半截 */
	T_CHECK(tui_input_feed(&in, 0xE4) == TUI_KEY_OVERFLOW);
	T_CHECK(tui_input_feed(&in, 0xB8) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 0xAD) == TUI_KEY_IGNORED);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
	/* 腾出 3 字节后可放入 */
	(void)tui_input_feed(&in, 0x7F);
	(void)tui_input_feed(&in, 0x7F);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 3);
	T_CHECK(tui_input_feed(&in, 0xE4) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xB8) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xAD) == TUI_KEY_CHAR);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES);
	T_RUN("whole_char_reject_at_boundary");
}

int main(void)
{
	test_basic_ascii();
	test_utf8_backspace();
	test_ctrl_keys();
	test_escape_sequences();
	test_max_length();
	test_whole_char_reject_at_boundary();
	printf("test_tui_input: all passed\n");
	return 0;
}
