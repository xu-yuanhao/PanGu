/* test_tui_input.c — UTF-8 多行编辑测试：光标/编辑/容量/转义/括号粘贴。 */
#include "test_util.h"

#include "tui_input.h"
#include "tui_utf8.h"

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
	/* 未知键 ESC[Z：整序列吞掉 */
	T_CHECK(feed_last(&in, "\x1b[Z") == TUI_KEY_IGNORED);
	T_CHECK(in.len == 2);
	/* SS3 未知键 */
	T_CHECK(feed_last(&in, "\x1bOQ") == TUI_KEY_IGNORED);
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

static void test_prompt_history(void)
{
 tui_input in;
 tui_input_init(&in);
 feed_str(&in, "  first  ");
 tui_input_remember(&in);
 tui_input_reset(&in);
 feed_str(&in, "second中");
 tui_input_remember(&in);
 tui_input_remember(&in);
 T_CHECK(in.history_count == 2);
 tui_input_reset(&in);
 feed_str(&in, "草稿🙂");
 feed_str(&in, "\x1b[H");
 T_CHECK(feed_last(&in, "\x1b[A") == TUI_KEY_CURSOR);
 T_CHECK(in.len == strlen("second中") && !memcmp(in.buf,"second中",in.len));
 T_CHECK(in.cursor == 0);
 feed_str(&in, "\x1b[A");
 T_CHECK(in.len == 5 && !memcmp(in.buf,"first",5));
 T_CHECK(feed_last(&in,"\x1b[A") == TUI_KEY_IGNORED);
 feed_str(&in,"\x1b[F\x1b[B");
 T_CHECK(in.history_index == 0);
 feed_str(&in,"\x1b[F\x1b[B");
 T_CHECK(in.history_index == -1 && in.cursor == 0);
 T_CHECK(in.len == strlen("草稿🙂") && !memcmp(in.buf,"草稿🙂",in.len));
 /* Edit a recalled prompt, then browse again: edited bytes become the draft. */
 feed_str(&in,"\x1b[A!");
 T_CHECK(in.history_index == -1);
 T_CHECK(in.buf[0] == '!');
 tui_input_reset(&in);
 for(unsigned i=0;i<105;i++) {
  char n[20]; snprintf(n,sizeof(n),"prompt-%u",i);
  feed_str(&in,n); tui_input_remember(&in); tui_input_reset(&in);
 }
 T_CHECK(in.history_count == 100);
 feed_str(&in,"\x1b[A");
 T_CHECK(in.len == 10 && !memcmp(in.buf,"prompt-104",10));
 for(unsigned i=0;i<99;i++) feed_str(&in,"\x1b[A");
 T_CHECK(in.len == 8 && !memcmp(in.buf,"prompt-5",8));
 T_RUN("prompt_history_draft_dedup_and_capacity");
}

static void test_unicode_history_trim(void)
{
 const char *spaces[]={" ","\t","\n","\r","\v","\f","\xc2\xa0","\xe1\x9a\x80",
 "\xe2\x80\x80","\xe2\x80\x81","\xe2\x80\x82","\xe2\x80\x83",
 "\xe2\x80\x84","\xe2\x80\x85","\xe2\x80\x86","\xe2\x80\x87",
 "\xe2\x80\x88","\xe2\x80\x89","\xe2\x80\x8a","\xe2\x80\xa8",
 "\xe2\x80\xa9","\xe2\x80\xaf","\xe2\x81\x9f","\xe3\x80\x80","\xef\xbb\xbf"};
 tui_input in; tui_input_init(&in);
 for(size_t i=0;i<sizeof(spaces)/sizeof(spaces[0]);i++) {
  in.len=(size_t)snprintf(in.buf,sizeof(in.buf),"%s中🙂%s",spaces[i],spaces[i]);
  in.cursor=in.len;
  tui_input_remember(&in);
  T_CHECK(in.history_count == 1 && in.history_lens[0] == strlen("中🙂"));
  T_CHECK(!memcmp(in.history[0],"中🙂",strlen("中🙂")));
  memcpy(in.buf,spaces[i],strlen(spaces[i])); in.len=strlen(spaces[i]);
  tui_input_remember(&in); T_CHECK(in.history_count == 1);
 }
 const char *nonspaces[]={"\xe1\xa0\x8e","\xe2\x80\x8b"};
 for(size_t i=0;i<2;i++) {
  in.len=(size_t)snprintf(in.buf,sizeof(in.buf),"%sX%s",nonspaces[i],nonspaces[i]);
  tui_input_remember(&in);
  T_CHECK(in.history_lens[0] == in.len);
 }
 T_RUN("ecmascript_history_trim");
}

static void test_visual_vertical(void)
{
 tui_input in; tui_input_init(&in);
 feed_str(&in,"abcdef\x1b\rx\x1b\rabcdef");
 /* Last row content column 6 -> short middle row -> first row content column 6. */
 T_CHECK(in.cursor == 15);
 feed_str(&in,"\x1b[A"); T_CHECK(in.cursor == 8);
 feed_str(&in,"\x1b[A"); T_CHECK(in.cursor == 6);
 feed_str(&in,"\x1b[B"); T_CHECK(in.cursor == 8);
 feed_str(&in,"\x1b[B"); T_CHECK(in.cursor == 15);
 tui_input_reset(&in); in.columns=6;
 feed_str(&in,"abcdefghij");
 feed_str(&in,"\x1b[A"); T_CHECK(in.cursor == 3);
 feed_str(&in,"\x1b[B"); T_CHECK(in.cursor == 10);
 /* Wide codepoints and combining marks never split during row selection. */
 tui_input_reset(&in); in.columns=6;
 feed_str(&in,"中🙂a中🙂");
 feed_str(&in,"\x1b[A"); T_CHECK(in.cursor == strlen("中"));
 feed_str(&in,"\x1b[B"); T_CHECK(in.cursor == strlen("中🙂a中🙂"));
 tui_input_reset(&in); in.columns=5;
 feed_str(&in,"a\xcc\x81" "bcdef");
 feed_str(&in,"\x1b[A");
 T_CHECK(in.cursor == 4); /* after b; prompt cells are excluded */
 /* Up selects multiline history at start; Down moves within it first. */
 tui_input_reset(&in); in.columns=80;
 feed_str(&in,"line1\x1b\rline2\x1b\rline3");
 tui_input_remember(&in); tui_input_reset(&in);
 feed_str(&in,"\x1b[A"); T_CHECK(in.cursor == 0);
 feed_str(&in,"\x1b[B"); T_CHECK(in.cursor == 6 && in.history_index == 0);
 feed_str(&in,"\x1b[A"); T_CHECK(in.cursor == 0 && in.history_index == 0);
 T_RUN("visual_vertical_wrap_width_and_preferred_column");
}

static void test_help_keys_preserve_draft(void)
{
 tui_input in; tui_input_init(&in); feed_str(&in,"中draft");
 feed_str(&in,"\x1b[D"); size_t cursor=in.cursor;
 T_CHECK(feed_last(&in,"\x1bOP") == TUI_KEY_HELP);
 T_CHECK(feed_last(&in,"\x1b[11~") == TUI_KEY_HELP);
 T_CHECK(in.cursor == cursor && in.len == strlen("中draft"));
 feed_str(&in,"\x1b[200~");
 T_CHECK(feed_last(&in,"\x1bOP") != TUI_KEY_HELP);
 feed_str(&in,"\x1b[201~");
 T_RUN("help_keys_preserve_draft_and_paste_safety");
}

static void test_local_command_completion(void)
{
 tui_input in; tui_input_init(&in);
 feed_str(&in,"/"); T_CHECK(tui_input_feed(&in,9)==TUI_KEY_COMPLETIONS);
 T_CHECK(in.len==1 && in.cursor==1);
 feed_str(&in,"h"); T_CHECK(tui_input_feed(&in,9)==TUI_KEY_CHAR);
 T_CHECK(in.len==8 && in.cursor==8 && !memcmp(in.buf,"/hotkeys",8));
 T_CHECK(tui_input_feed(&in,9)==TUI_KEY_IGNORED);
 tui_input_reset(&in); feed_str(&in,"/q中\x1b[D");
 T_CHECK(tui_input_feed(&in,9)==TUI_KEY_CHAR);
 T_CHECK(in.cursor==5 && in.len==8 && !memcmp(in.buf,"/quit中",8));
 tui_input_reset(&in); feed_str(&in,"/unknown");
 T_CHECK(tui_input_feed(&in,9)==TUI_KEY_IGNORED);
 tui_input_reset(&in); feed_str(&in,"\x1b[200~/h\t\x1b[201~");
 T_CHECK(in.len==2 && !memcmp(in.buf,"/h",2));
 tui_input_reset(&in); feed_str(&in,"/h");
 memset(in.buf+2,'x',1021); in.len=1023; in.cursor=2;
 T_CHECK(tui_input_feed(&in,9)==TUI_KEY_OVERFLOW);
 T_CHECK(in.len==1023 && in.cursor==2 && in.buf[2]=='x');
 T_RUN("command_completion_suffix_capacity_and_paste");
}

static void test_borrowed_prompt_history(void)
{
 tui_input in; tui_input_init(&in); feed_str(&in,"draft🙂");
 size_t cursor=in.cursor;
 tui_input_history_add(&in,"earlier中",strlen("earlier中"));
 tui_input_history_add(&in,"recent🙂",strlen("recent🙂"));
 T_CHECK(in.len==strlen("draft🙂") && in.cursor==cursor);
 T_CHECK(in.history_count==2);
 const char *bad[]={"\x1b[2J","bad\rtext","\xc2\x85","\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe4"};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) tui_input_history_add(&in,bad[i],strlen(bad[i]));
 tui_input_history_add(&in,NULL,0); tui_input_history_add(&in,NULL,10);
 T_CHECK(in.history_count==2);
 feed_str(&in,"\x1b[H\x1b[A");
 T_CHECK(in.len==strlen("recent🙂") && !memcmp(in.buf,"recent🙂",in.len));
 feed_str(&in,"\x1b[F\x1b[B");
 T_CHECK(in.len==strlen("draft🙂") && !memcmp(in.buf,"draft🙂",in.len));
 T_RUN("borrowed_history_copy_and_unsafe_rejection");
}

static void test_command_choice_focus(void)
{
 tui_input in; tui_input_init(&in);
 feed_str(&in,"/中\x1b[D");
 T_CHECK(tui_input_feed(&in,9)==TUI_KEY_COMPLETIONS);
 T_CHECK(in.completion_open==1 && in.cursor==1);
 T_CHECK(feed_last(&in,"\x1b[B")==TUI_KEY_COMPLETIONS);
 T_CHECK(in.completion_selected==1 && in.cursor==1);
 T_CHECK(tui_input_feed(&in,13)==TUI_KEY_CHAR);
 T_CHECK(in.completion_open==0 && in.cursor==5 && in.len==8);
 T_CHECK(!memcmp(in.buf,"/quit中",8));
 T_CHECK(tui_input_feed(&in,10)==TUI_KEY_IGNORED); /* CRLF never submits selection */
 tui_input_reset(&in); feed_str(&in,"/"); tui_input_feed(&in,9);
 T_CHECK(tui_input_feed(&in,3)==TUI_KEY_CURSOR);
 T_CHECK(in.completion_open==0 && in.len==1 && in.cursor==1);
 T_CHECK(tui_input_feed(&in,3)==TUI_KEY_CTRL_C); /* focus released */
 tui_input_feed(&in,9);
 T_CHECK(feed_last(&in,"\x1b[A")==TUI_KEY_COMPLETIONS);
 T_CHECK(in.completion_selected==1);
 tui_input_feed(&in,'h');
 T_CHECK(in.completion_open==0 && in.len==2);
 T_RUN("command_choice_accept_cancel_and_focus");
}

static void test_reverse_history_recall(void)
{
 tui_input in; tui_input_init(&in);
 tui_input_history_add(&in,"older中 prompt",strlen("older中 prompt"));
 tui_input_history_add(&in,"unrelated",9);
 tui_input_history_add(&in,"newer中 prompt",strlen("newer中 prompt"));
 feed_str(&in,"中"); size_t cursor=in.cursor;
 T_CHECK(tui_input_feed(&in,18)==TUI_KEY_SEARCH);
 T_CHECK(in.history_search_active==1 && in.history_index==-1 && in.len==0);
 feed_str(&in,"中");
 T_CHECK(in.history_index==0 && in.len==strlen("中"));
 tui_input_feed(&in,18);
 T_CHECK(in.history_index==2 && in.len==strlen("中"));
 tui_input_feed(&in,18); T_CHECK(in.history_index==2);
 T_CHECK(tui_input_feed(&in,19)==TUI_KEY_SEARCH && in.history_index==0);
 feed_str(&in,"\x1b[A"); T_CHECK(in.history_index==2);
 feed_str(&in,"\x1b[B"); T_CHECK(in.history_index==0);
 T_CHECK(tui_input_feed(&in,3)==TUI_KEY_CURSOR);
 T_CHECK(in.history_search_active==0 && in.cursor==cursor && in.len==strlen("中"));
 tui_input_feed(&in,18); feed_str(&in,"中");
 T_CHECK(tui_input_feed(&in,13)==TUI_KEY_CURSOR);
 T_CHECK(in.history_search_active==0 && in.history_index==-1);
 T_CHECK(tui_input_feed(&in,10)==TUI_KEY_IGNORED);
 T_CHECK(tui_input_feed(&in,13)==TUI_KEY_ENTER);
 tui_input_reset(&in); feed_str(&in,"original"); tui_input_feed(&in,18);
 feed_str(&in,"no match");
 T_CHECK(in.history_index==-1 && in.history_search_active==1);
 T_CHECK(tui_input_feed(&in,13)==TUI_KEY_SEARCH); /* never submits no-match query */
 T_CHECK(tui_input_feed(&in,4)==TUI_KEY_CURSOR);
 T_CHECK(in.len==8 && !memcmp(in.buf,"original",8));
 tui_input_feed(&in,18);
 feed_str(&in,"\x1b[200~中\x1b[201~");
 T_CHECK(in.history_index==0 && in.history_search_active==1);
 tui_input_feed(&in,3); T_CHECK(in.len==8 && !memcmp(in.buf,"original",8));
 T_RUN("reverse_history_recall_accept_and_restore");
}

static void test_search_unique_results(void)
{
 tui_input in; tui_input_init(&in);
 tui_input_history_add(&in,"match A",7);
 tui_input_history_add(&in,"match B",7);
 tui_input_history_add(&in,"match A",7);
 tui_input_history_add(&in,"match C",7);
 tui_input_history_add(&in,"match B",7);
 feed_str(&in,"original🙂\x1b[D"); size_t saved=in.cursor;
 tui_input_feed(&in,18); feed_str(&in,"match");
 T_CHECK(in.history_index==0); /* B */
 tui_input_feed(&in,18); T_CHECK(in.history_index==1); /* C */
 tui_input_feed(&in,18); T_CHECK(in.history_index==2); /* A */
 tui_input_feed(&in,18); T_CHECK(in.history_index==2); /* older B/A skipped */
 tui_input_feed(&in,19); T_CHECK(in.history_index==1);
 tui_input_feed(&in,19); T_CHECK(in.history_index==0);
 feed_str(&in,"x\x7f"); /* editing query restarts from newest */
 T_CHECK(in.history_index==0 && in.history_search_active);
 tui_input_feed(&in,3);
 T_CHECK(in.cursor==saved && in.len==strlen("original🙂"));
 T_CHECK(!memcmp(in.buf,"original🙂",in.len));
 tui_input_reset(&in); tui_input_feed(&in,0xe4);
 T_CHECK(tui_input_feed(&in,18)==TUI_KEY_IGNORED);
 T_CHECK(in.utf8_pending==2 && !in.history_search_active);
 tui_input_feed(&in,0xb8); tui_input_feed(&in,0xad);
 T_CHECK(tui_input_feed(&in,18)==TUI_KEY_SEARCH);
 T_RUN("search_unique_results_and_draft_cursor");
}

static void test_unicode_lowercase_search(void)
{
 const char *entries[]={"ÉCOLE中", "ΟΣ", "İSTANBUL", "Straße"};
 const char *queries[]={"école", "ος", "i\xcc\x87stanbul", "straße"};
 for(size_t i=0;i<4;i++) {
  tui_input in; tui_input_init(&in);
  tui_input_history_add(&in,entries[i],strlen(entries[i]));
  tui_input_feed(&in,18); feed_str(&in,queries[i]);
  T_CHECK(!in.history_search_failed && in.history_index==0);
  T_CHECK(tui_input_feed(&in,13)==TUI_KEY_CURSOR);
  T_CHECK(in.len==strlen(entries[i]) && !memcmp(in.buf,entries[i],in.len));
 }
 /* Lowercase is deliberately not casefold: ß is not expanded into ss. */
 tui_input in; tui_input_init(&in);
 tui_input_history_add(&in,"Straße",strlen("Straße"));
 tui_input_feed(&in,18); feed_str(&in,"STRASSE");
 T_CHECK(in.history_index==-1 && !in.history_search_failed);
 char tiny[2];
 T_CHECK(tui_unicode_lower("İ",strlen("İ"),tiny,sizeof(tiny))==(size_t)-1);
 T_CHECK(tui_unicode_lower("\xff",1,tiny,sizeof(tiny))==(size_t)-1);
 T_RUN("unicode_full_lowercase_search_without_casefold");
}

static void test_scoped_escape_timeout(void)
{
 tui_input in; tui_input_init(&in); feed_str(&in,"draft🙂"); size_t cursor=in.cursor;
 tui_input_feed(&in,18); feed_str(&in,"query"); tui_input_feed(&in,27);
 T_CHECK(tui_input_escape_timeout(&in)==TUI_KEY_CURSOR);
 T_CHECK(!in.history_search_active && in.esc_state==0);
 T_CHECK(in.cursor==cursor && in.len==strlen("draft🙂"));
 tui_input_reset(&in); feed_str(&in,"/\t\x1b[");
 T_CHECK(tui_input_escape_timeout(&in)==TUI_KEY_IGNORED && in.completion_open);
 T_CHECK(tui_input_feed(&in,'B')==TUI_KEY_COMPLETIONS && in.completion_selected==1);
 tui_input_feed(&in,27);
 T_CHECK(tui_input_escape_timeout(&in)==TUI_KEY_CURSOR && !in.completion_open);
 T_CHECK(in.len==1 && in.cursor==1);
 feed_str(&in,"\x1b[200~\x1b");
 T_CHECK(tui_input_escape_timeout(&in)==TUI_KEY_IGNORED && in.paste);
 feed_str(&in,"\x1b[201~");
 tui_input_reset(&in); tui_input_feed(&in,27);
 T_CHECK(tui_input_escape_timeout(&in)==TUI_KEY_IGNORED && in.esc_state==1);
 T_RUN("scoped_escape_timeout_preserves_csi_and_paste");
}

static void test_invalid_utf8_recovery(void)
{
 const char *invalid[]={"\xc0\xaf","\xc1\xbf","\xe0\x80\xaf","\xed\xa0\x80",
 "\xf0\x80\x80\xaf","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xff","\xc2\x9b"};
 for(size_t j=0;j<sizeof(invalid)/sizeof(invalid[0]);j++) {
  for(int paste=0;paste<2;paste++) {
   tui_input in; tui_input_init(&in);
   feed_str(&in,"ab\x1b[D");
   if(paste) feed_str(&in,"\x1b[200~");
   feed_str(&in,invalid[j]);
   if(paste) feed_str(&in,"\x1b[201~");
   T_CHECK(in.len==2 && in.cursor==1 && !memcmp(in.buf,"ab",2));
   feed_str(&in,"中"); T_CHECK(in.len==5 && !memcmp(in.buf,"a中b",5));
  }
 }
 tui_input in; tui_input_init(&in); feed_str(&in,"ab\x1b[D");
 tui_input_feed(&in,0xe4);
 T_CHECK(tui_input_incomplete(&in));
 feed_str(&in,"X"); T_CHECK(!tui_input_incomplete(&in));
 T_CHECK(in.len==3 && !memcmp(in.buf,"aXb",3));
 tui_input_feed(&in,0xe4); feed_str(&in,"\x1b[F");
 T_CHECK(!tui_input_incomplete(&in) && in.len==3 && in.cursor==3);
 T_RUN("utf8_scalar_rejection_and_partial_recovery");
}

int main(void)
{
 test_invalid_utf8_recovery();
 test_scoped_escape_timeout();
 test_unicode_lowercase_search();
 test_search_unique_results();
 test_reverse_history_recall();
 test_command_choice_focus();
 test_borrowed_prompt_history();
 test_local_command_completion();
 test_help_keys_preserve_draft();
 test_unicode_history_trim();
 test_visual_vertical();
 test_prompt_history();
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
