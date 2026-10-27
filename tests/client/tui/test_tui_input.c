/* test_tui_input.c — UTF-8 多行编辑测试：光标/编辑/容量/转义/括号粘贴。 */
#include "test_util.h"

#include "tui_input.h"

static void feed_str(tui_input *in, const char *s)
{
	while (*s)
		tui_input_feed(in, (unsigned char)*s++);
}

/* 喂入并返回最后一个键值 */
static int feed_last(tui_input *in, const char *s)
{
	int k = TUI_KEY_NONE;

	while (*s)
		k = tui_input_feed(in, (unsigned char)*s++);
	return k;
}

static void test_basic_ascii(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "hello");
	T_CHECK(in.len == 5 && in.cursor == 5);
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

static void test_crlf_single_submit(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "hi");
	T_CHECK(tui_input_feed(&in, '\r') == TUI_KEY_ENTER);
	/* CRLF 的 LF 被吞：不产生第二次提交 */
	T_CHECK(tui_input_feed(&in, '\n') == TUI_KEY_IGNORED);
	/* 换行后 LF 单独仍是合法提交 */
	T_CHECK(tui_input_feed(&in, '\n') == TUI_KEY_ENTER);
	T_RUN("crlf_single_submit");
}

static void test_utf8_backspace(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "a\xe4\xb8\xad" "b");
	T_CHECK(in.len == 5); /* 1 + 3(中) + 1(b) */
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 4); /* 删掉 'b' */
	T_CHECK(tui_input_feed(&in, 0x08) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 1); /* 整个 "中" 一次删除，不拆坏 */
	T_CHECK(in.buf[0] == 'a');
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 0);

	/* 分批喂入多字节序列：字节序保真，光标为字节偏移 */
	tui_input_reset(&in);
	T_CHECK(tui_input_feed(&in, 0xE4) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xB8) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xAD) == TUI_KEY_CHAR);
	T_CHECK(in.len == 3);
	T_CHECK(memcmp(in.buf, "\xe4\xb8\xad", 3) == 0);
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 0);

	/* 半字符直接退格：pending 一并清除，后续 ASCII 正常插入 */
	tui_input_reset(&in);
	T_CHECK(tui_input_feed(&in, 0xE4) == TUI_KEY_CHAR);
	T_CHECK(in.utf8_pending == 2);
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(in.len == 0 && in.utf8_pending == 0);
	T_CHECK(tui_input_feed(&in, 'a') == TUI_KEY_CHAR);
	T_CHECK(in.len == 1);
	T_RUN("utf8_backspace");
}

static void test_cursor_movement(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "a\xe4\xb8\xad" "b"); /* cursor=5 */
	/* Left/Right 按 code point 边界移动（CSI 与 SS3 两种形态） */
	T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '[') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 'D') == TUI_KEY_CURSOR);
	T_CHECK(in.cursor == 4);
	feed_str(&in, "\x1b[D");
	T_CHECK(in.cursor == 1);
	feed_str(&in, "\x1bOD"); /* SS3 左：删掉 'a' 前的码点 */
	T_CHECK(in.cursor == 0);
	T_CHECK(feed_last(&in, "\x1bOD") == TUI_KEY_IGNORED); /* 真行首 */
	feed_str(&in, "\x1bOC"); /* SS3 右：越过 'a' */
	T_CHECK(in.cursor == 1);
	feed_str(&in, "\x1bOC"); /* SS3 右：越过 "中" */
	T_CHECK(in.cursor == 4);
	feed_str(&in, "\x1bOC");
	T_CHECK(in.cursor == 5);
	T_CHECK(feed_last(&in, "\x1bOC") == TUI_KEY_IGNORED); /* 行尾右移无效 */
	feed_str(&in, "\x1b[H"); /* 单行 Home → 0 */
	T_CHECK(in.cursor == 0);
	feed_str(&in, "\x1b[F"); /* End → 末尾 */
	T_CHECK(in.cursor == 5);
	T_CHECK(feed_last(&in, "\x1b[H") == TUI_KEY_CURSOR);
	T_CHECK(in.cursor == 0);
	T_RUN("cursor_movement");
}

static void test_home_end_delete_multiline(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "ab");
	T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '\r') == TUI_KEY_CHAR); /* Alt+Enter 插入换行 */
	T_CHECK(in.buf[2] == '\n' && in.len == 3);
	feed_str(&in, "cde"); /* "ab\ncde" cursor=6 */
	T_CHECK(in.len == 6);
	feed_str(&in, "\x1b[H");
	T_CHECK(in.cursor == 3); /* 行首 */
	feed_str(&in, "\x1b[H");
	T_CHECK(in.cursor == 3); /* 已在行首，不再上跳 */
	feed_str(&in, "\x1b[F");
	T_CHECK(in.cursor == 6); /* 行尾 */
	feed_str(&in, "\x1b[F");
	T_CHECK(in.cursor == 6);
	feed_str(&in, "\x1b[D");
	T_CHECK(in.cursor == 5); /* 'e' */
	/* Delete 删除光标后一个字符（ESC[3~ 与 SS3 变体键位差异各自验证） */
	T_CHECK(feed_last(&in, "\x1b[3~") == TUI_KEY_CHAR);
	T_CHECK(in.len == 5 && memcmp(in.buf, "ab\ncd", 5) == 0);
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE); /* Backspace 删前一个 */
	T_CHECK(in.len == 4 && memcmp(in.buf, "ab\nc", 4) == 0);
	/* Home 变体：ESC[1~ / ESC[7~；End 变体：ESC[4~ / ESC[8~ / SS3 F */
	feed_str(&in, "\x1b[4~");
	T_CHECK(in.cursor == 4);
	feed_str(&in, "\x1b[7~");
	T_CHECK(in.cursor == 3);
	feed_str(&in, "\x1b[8~");
	T_CHECK(in.cursor == 4);
	T_CHECK(feed_last(&in, "\x1bOF") == TUI_KEY_CURSOR);
	T_CHECK(in.cursor == 4);
	T_CHECK(feed_last(&in, "\x1bOH") == TUI_KEY_CURSOR);
	T_CHECK(in.cursor == 3);
	T_RUN("home_end_delete_multiline");
}

static void test_alt_enter_variants(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "ab");
	T_CHECK(feed_last(&in, "\x1b\r") == TUI_KEY_CHAR); /* Alt+Enter (CR) */
	T_CHECK(in.buf[2] == '\n');
	T_CHECK(feed_last(&in, "\x1b\n") == TUI_KEY_CHAR); /* Alt+Enter (LF) */
	T_CHECK(in.buf[3] == '\n');
	T_CHECK(in.len == 4);
	/* 小键盘回车 SS3 M 仍是提交 */
	T_CHECK(feed_last(&in, "\x1bOM") == TUI_KEY_ENTER);
	T_RUN("alt_enter_variants");
}

static void test_ctrl_keys(void)
{
	tui_input in;

	tui_input_init(&in);
	T_CHECK(tui_input_feed(&in, 0x04) == TUI_KEY_CTRL_D); /* 空行 ^D = 退出 */
	feed_str(&in, "hi");
	T_CHECK(tui_input_feed(&in, 0x04) == TUI_KEY_BACKSPACE); /* 非空 ^D = 退格（保留语义） */
	T_CHECK(in.len == 1);
	feed_str(&in, "i");
	T_CHECK(tui_input_feed(&in, 0x15) == TUI_KEY_CHAR); /* ^U 清行 */
	T_CHECK(in.len == 0 && in.cursor == 0);
	T_CHECK(tui_input_feed(&in, 0x03) == TUI_KEY_CTRL_C);
	T_CHECK(tui_input_feed(&in, '\n') == TUI_KEY_ENTER); /* LF 也算提交 */
	T_CHECK(tui_input_feed(&in, 0x09) == TUI_KEY_IGNORED); /* Tab 忽略 */
	T_CHECK(tui_input_feed(&in, 0x01) == TUI_KEY_IGNORED); /* 其他控制字节忽略 */
	T_RUN("ctrl_keys");
}

static void test_escape_safety(void)
{
	tui_input in;

	tui_input_init(&in);
	feed_str(&in, "ab");
	/* 未知方向键 ESC[A：整序列吞掉 */
	T_CHECK(feed_last(&in, "\x1b[A") == TUI_KEY_IGNORED);
	T_CHECK(in.len == 2);
	/* SS3 未知键 */
	T_CHECK(feed_last(&in, "\x1bOP") == TUI_KEY_IGNORED);
	T_CHECK(in.len == 2);
	/* 带修饰的方向键 ESC[1;5D：参数后吞掉，不产生光标动作 */
	T_CHECK(feed_last(&in, "\x1b[1;5D") == TUI_KEY_IGNORED);
	T_CHECK(in.len == 2);
	/* 孤立 ESC：其后一个键按 Meta 组合吞掉（Pi stdin-buffer 行为） */
	T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 'c') == TUI_KEY_IGNORED);
	T_CHECK(in.len == 2);
	/* 超长 CSI：所有参数字节消费到终结字节为止，其后的正常文本不受影响 */
	{
		int i;

		T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
		T_CHECK(tui_input_feed(&in, '[') == TUI_KEY_IGNORED);
		for (i = 0; i < 40; i++)
			T_CHECK(tui_input_feed(&in, '7') == TUI_KEY_IGNORED);
		T_CHECK(tui_input_feed(&in, 'x') == TUI_KEY_IGNORED); /* 终结字节 */
		T_CHECK(tui_input_feed(&in, 'y') == TUI_KEY_CHAR);
		T_CHECK(in.len == 3 && memcmp(in.buf, "aby", 3) == 0);
	}
	/* OSC 以 BEL 结束；其后正常输入 */
	T_CHECK(feed_last(&in, "\x1b]0;title\x07") == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 'z') == TUI_KEY_CHAR);
	/* OSC 以 ESC \ (ST) 结束 */
	T_CHECK(feed_last(&in, "\x1b]8;;http://x\x1b\\") == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 'w') == TUI_KEY_CHAR);
	/* DCS 无上限消费到 ST；期间 Ctrl-C 可中止序列并产生取消 */
	T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 'P') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '1') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 0x03) == TUI_KEY_CTRL_C);
	T_CHECK(tui_input_feed(&in, 'v') == TUI_KEY_CHAR);
	/* CSI 中 Ctrl-C 同样中止并取消 */
	T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '[') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 0x03) == TUI_KEY_CTRL_C);
	/* CAN 中止 CSI，其后正常输入 */
	T_CHECK(feed_last(&in, "\x1b[1;\x18") == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 'u') == TUI_KEY_CHAR);
	T_CHECK(in.len == 7);
	/* 游离粘贴结束标记：忽略不崩溃 */
	T_CHECK(feed_last(&in, "\x1b[201~") == TUI_KEY_IGNORED);
	T_CHECK(in.len == 7);
	T_RUN("escape_safety");
}

static void test_capacity(void)
{
	tui_input in;
	unsigned i;

	tui_input_init(&in);
	for (i = 0; i < TUI_INPUT_MAX_BYTES - 1; i++)
		T_CHECK(tui_input_feed(&in, 'a') == TUI_KEY_CHAR);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1); /* 有效文本 ≤ 1023 */
	/* 满：继续输入被拒且节流（第二次不再报 OVERFLOW） */
	T_CHECK(tui_input_feed(&in, 'b') == TUI_KEY_OVERFLOW);
	T_CHECK(tui_input_feed(&in, 'c') == TUI_KEY_IGNORED);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
	/* 删一个再放一个 */
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(tui_input_feed(&in, 'd') == TUI_KEY_CHAR);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
	/* ^U 清空后解除节流 */
	T_CHECK(tui_input_feed(&in, 0x15) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 'e') == TUI_KEY_CHAR);
	T_CHECK(in.len == 1);
	T_RUN("capacity");
}

static void test_whole_char_reject_at_boundary(void)
{
	tui_input in;
	unsigned i;

	tui_input_init(&in);
	for (i = 0; i < TUI_INPUT_MAX_BYTES - 4; i++)
		(void)tui_input_feed(&in, 'a');
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 4); /* 1020：只差 3 字节 */
	/* "中" 需要 3 字节：恰好放满 1023 */
	T_CHECK(tui_input_feed(&in, 0xE4) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xB8) == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, 0xAD) == TUI_KEY_CHAR);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
	/* 再放不下任何整字符 */
	T_CHECK(tui_input_feed(&in, 'x') == TUI_KEY_OVERFLOW);
	T_CHECK(tui_input_feed(&in, 0xE4) == TUI_KEY_IGNORED); /* 节流 */
	T_CHECK(tui_input_feed(&in, 0xB8) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 0xAD) == TUI_KEY_IGNORED);
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
	T_CHECK(memcmp(in.buf + TUI_INPUT_MAX_BYTES - 4, "\xe4\xb8\xad", 3) == 0);
	T_RUN("whole_char_reject_at_boundary");
}

/* 括号粘贴：所有读取边界拆分点都验证一次（标记/内容任意拆分） */
static void paste_all_splits(const char *stream, size_t stream_len,
			     const char *expect, size_t expect_len)
{
	size_t split;

	for (split = 0; split <= stream_len; split++) {
		tui_input in;
		int last = TUI_KEY_NONE;
		size_t i;

		tui_input_init(&in);
		for (i = 0; i < split; i++)
			last = tui_input_feed(&in, (unsigned char)stream[i]);
		if (split != stream_len)
			T_CHECK(tui_input_incomplete(&in) == 1 ||
				split == 0); /* 标记中途必为未完成 */
		for (i = split; i < stream_len; i++)
			last = tui_input_feed(&in, (unsigned char)stream[i]);
		if (!(last == TUI_KEY_PASTE && in.len == expect_len &&
		      memcmp(in.buf, expect, expect_len) == 0 && in.paste == 0)) {
			fprintf(stderr, "split %zu failed: last=%d len=%zu\n",
				split, last, in.len);
			exit(1);
		}
	}
}

static void test_paste_basic_and_splits(void)
{
	tui_input in;

	tui_input_init(&in);
	T_CHECK(feed_last(&in, "\x1b[200~hello\x1b[201~") == TUI_KEY_PASTE);
	T_CHECK(in.len == 5 && memcmp(in.buf, "hello", 5) == 0);
	T_CHECK(in.cursor == 5);
	T_CHECK(tui_input_incomplete(&in) == 0);

	/* 所有拆分点：开始/结束标记可在任意字节边界分片到达 */
	paste_all_splits("\x1b[200~hi\x1b[201~", 14, "hi", 2);
	/* 内容含 CRLF/CR/LF 三种换行形态 */
	paste_all_splits("\x1b[200~L1\r\nL2\x1b[201~", 18, "L1\nL2", 5);
	paste_all_splits("\x1b[200~a\rb\x1b[201~", 15, "a\nb", 3);
	paste_all_splits("\x1b[200~a\nb\x1b[201~", 15, "a\nb", 3);
	/* 空粘贴（立即结束） */
	tui_input_init(&in);
	T_CHECK(feed_last(&in, "\x1b[200~\x1b[201~") == TUI_KEY_PASTE);
	T_CHECK(in.len == 0);
	/* 重复开始标记：嵌套标记按未匹配前缀处理，不进入二次粘贴 */
	tui_input_init(&in);
	(void)feed_last(&in, "\x1b[200~\x1b[201~");
	T_CHECK(in.paste == 0);
	T_RUN("paste_basic_and_splits");
}

static void test_paste_content_safety(void)
{
	tui_input in;

	/* 粘贴中的 Enter/Ctrl-C/Ctrl-D/孤立 ESC：无提交、无取消、无退出 */
	tui_input_init(&in);
	T_CHECK(feed_last(&in, "\x1b[200~x\r\x03\x04\x1b\x1by\x1b[201~") == TUI_KEY_PASTE);
	T_CHECK(in.len == 3); /* x + 折叠 LF + y */
	T_CHECK(memcmp(in.buf, "x\ny", 3) == 0);
	/* 未完成粘贴：Enter 字节不提交（feed 层不产生 ENTER），incomplete 为真 */
	tui_input_init(&in);
	(void)feed_last(&in, "\x1b[200~abc");
	T_CHECK(tui_input_incomplete(&in) == 1);
	T_CHECK(tui_input_feed(&in, '\r') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '\n') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 0x03) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, 0x04) == TUI_KEY_IGNORED);
	/* 结束标记收尾：CR 折叠为 LF 落定，粘贴结束 */
	T_CHECK(tui_input_feed(&in, 0x1B) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '[') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '2') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '0') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '1') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(&in, '~') == TUI_KEY_PASTE);
	T_CHECK(in.len == 4 && memcmp(in.buf, "abc\n", 4) == 0);
	T_CHECK(in.paste == 0 && tui_input_incomplete(&in) == 0);
	T_RUN("paste_content_safety");
}

static void test_paste_overflow_recovery(void)
{
	tui_input in;
	unsigned i;

	/* 距上限 3 字节：粘贴放得下一个 "中"，第二个整字符起全部拒绝 */
	tui_input_init(&in);
	for (i = 0; i < TUI_INPUT_MAX_BYTES - 4; i++)
		(void)tui_input_feed(&in, 'a');
	{
		int saw_overflow = 0;
		const char *s = "\x1b[200~\xe4\xb8\xad\xe4\xb8\xad extra\x1b[201~";

		while (*s) {
			int k = tui_input_feed(&in, (unsigned char)*s++);

			if (k == TUI_KEY_OVERFLOW)
				saw_overflow++;
		}
		T_CHECK(saw_overflow == 1); /* 整串只报一次，其余节流 */
	}
	T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
	T_CHECK(in.buf[TUI_INPUT_MAX_BYTES - 4] == (char)0xE4);
	T_CHECK(in.cursor == TUI_INPUT_MAX_BYTES - 1);
	/* 粘贴结束后：继续正常编辑与提交 */
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(tui_input_feed(&in, 'z') == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, '\r') == TUI_KEY_ENTER);
	tui_input_reset(&in);

	/* 大段溢出粘贴：仅报一次 OVERFLOW，仍消费到结束标记 */
	tui_input_init(&in);
	(void)feed_last(&in, "\x1b[200~");
	{
		int overflow_seen = 0;

		for (i = 0; i < 400; i++) { /* 341 个 "中" 恰满 1023，其余整字符拒绝 */
			int k1 = tui_input_feed(&in, 0xE4);
			int k2 = tui_input_feed(&in, 0xB8);
			int k3 = tui_input_feed(&in, 0xAD);

			if (k1 == TUI_KEY_OVERFLOW || k2 == TUI_KEY_OVERFLOW ||
			    k3 == TUI_KEY_OVERFLOW)
				overflow_seen++;
			if (overflow_seen > 1) {
				fprintf(stderr, "overflow not throttled\n");
				exit(1);
			}
		}
		T_CHECK(overflow_seen == 1);
		T_CHECK(in.len == TUI_INPUT_MAX_BYTES - 1);
		T_CHECK(feed_last(&in, "\x1b[201~") == TUI_KEY_PASTE);
		T_CHECK(in.paste == 0);
	}
	T_CHECK(tui_input_feed(&in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(tui_input_feed(&in, 'q') == TUI_KEY_CHAR);
	T_CHECK(tui_input_feed(&in, '\r') == TUI_KEY_ENTER);
	T_RUN("paste_overflow_recovery");
}

int main(void)
{
	test_basic_ascii();
	test_crlf_single_submit();
	test_utf8_backspace();
	test_cursor_movement();
	test_home_end_delete_multiline();
	test_alt_enter_variants();
	test_ctrl_keys();
	test_escape_safety();
	test_capacity();
	test_whole_char_reject_at_boundary();
	test_paste_basic_and_splits();
	test_paste_content_safety();
	test_paste_overflow_recovery();
	printf("test_tui_input: all passed\n");
	return 0;
}
