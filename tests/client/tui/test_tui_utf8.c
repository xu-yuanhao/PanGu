/* test_tui_utf8.c — UTF-8 字节辅助测试。 */
#include "test_util.h"

#include "tui_utf8.h"

static void test_seq_len(void)
{
	T_CHECK(tui_utf8_seq_len('A') == 1);
	T_CHECK(tui_utf8_seq_len(0x7F) == 1);
	T_CHECK(tui_utf8_seq_len(0xC3) == 2);  /* é 类 */
	T_CHECK(tui_utf8_seq_len(0xE4) == 3);  /* 中 类 */
	T_CHECK(tui_utf8_seq_len(0xE6) == 3);  /* 演 类 */
	T_CHECK(tui_utf8_seq_len(0xF0) == 4);  /* emoji 类 */
	T_CHECK(tui_utf8_seq_len(0x80) == 1);  /* 孤立连续字节按 1 处理 */
	T_CHECK(tui_utf8_seq_len(0xFF) == 1);
	T_RUN("seq_len");
}

static void test_incomplete_tail(void)
{
	/* 完整序列无尾 */
	T_CHECK(tui_utf8_incomplete_tail("中", 3) == 0);
	/* "演" 的前 2 字节 */
	T_CHECK(tui_utf8_incomplete_tail("\xe6\xbc", 2) == 2);
	/* "演" 前 1 字节 */
	T_CHECK(tui_utf8_incomplete_tail("\xe6", 1) == 1);
	/* 4 字节 emoji 前 3 字节 */
	T_CHECK(tui_utf8_incomplete_tail("\xf0\x9f\x98", 3) == 3);
	/* ASCII 无尾 */
	T_CHECK(tui_utf8_incomplete_tail("abc", 3) == 0);
	T_CHECK(tui_utf8_incomplete_tail("", 0) == 0);
	/* 中间拆断后再补全 */
	{
		char buf[4];
		buf[0] = (char)0xe6;
		T_CHECK(tui_utf8_incomplete_tail(buf, 1) == 1);
		buf[1] = (char)0xbc;
		T_CHECK(tui_utf8_incomplete_tail(buf, 2) == 2);
		buf[2] = (char)0x94;
		T_CHECK(tui_utf8_incomplete_tail(buf, 3) == 0);
	}
	T_RUN("incomplete_tail");
}

static void test_last_seq_start(void)
{
	T_CHECK(tui_utf8_last_seq_start("ab", 2) == 1);
	/* "ab中"：中的起始 = 2 */
	T_CHECK(tui_utf8_last_seq_start("ab\xe4\xb8\xad", 5) == 2);
	/* 末尾不完整 "a演前2字节" → 演起始 */
	T_CHECK(tui_utf8_last_seq_start("a\xe6\xbc", 3) == 1);
	T_CHECK(tui_utf8_last_seq_start("", 0) == 0);
	T_RUN("last_seq_start");
}

static void test_truncate_len(void)
{
	/* 不超上限原样 */
	T_CHECK(tui_utf8_truncate_len("abc", 3, 10) == 3);
	/* ASCII 直接切 */
	T_CHECK(tui_utf8_truncate_len("abcdef", 6, 4) == 4);
	/* "a" + "中"(3B) 上限 3：中 需要字节 1..3，恰好放下 */
	T_CHECK(tui_utf8_truncate_len("a\xe4\xb8\xad", 4, 3) == 1);
	/* 上限 4：a + 中 = 4，可保留（不拆断，4 处于边界内） */
	T_CHECK(tui_utf8_truncate_len("a\xe4\xb8\xad", 4, 4) == 4);
	/* "中中" 上限 4：只保留第一个中 */
	T_CHECK(tui_utf8_truncate_len("\xe4\xb8\xad\xe4\xb8\xad", 6, 4) == 3);
	T_RUN("truncate_len");
}

int main(void)
{
	test_seq_len();
	test_incomplete_tail();
	test_last_seq_start();
	test_truncate_len();
	printf("test_tui_utf8: all passed\n");
	return 0;
}
