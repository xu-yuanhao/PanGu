/* test_tui_diff.c — diff/patch 渲染：行分组、1:1 行内反色、上下文、转义、双模式。 */
#include "test_util.h"

#include "tui_diff.h"

/* A sink may destroy all caller-saved registers after copying the bytes. */
static int clobber_write(void *ctx, const char *data, size_t len)
{
 int result = tstr_write(ctx, data, len);
 __asm__ volatile("xor %%r9d, %%r9d; xor %%r10d, %%r10d; xor %%r11d, %%r11d"
                  : : : "r9", "r10", "r11", "cc");
 return result;
}

static void diff_render(tstr *out, int ansi, const char *text)
{
	tui_out_sink sink = {out, clobber_write};

	tstr_clear(out);
	tui_diff_render(&sink, ansi, text, strlen(text));
}

static void test_basic_lines_ansi(void)
{
	tstr out;

	diff_render(&out, 1, "-12 old line\n+13 new line\n  14 ctx\n");
	T_CHECK(tstr_contains(&out, "\x1b[31m-12 \x1b[7mold\x1b[27m line\x1b[0m\r\n"));
	T_CHECK(tstr_contains(&out, "\x1b[32m+13 \x1b[7mnew\x1b[27m line\x1b[0m\r\n"));
	T_CHECK(tstr_contains(&out, "\x1b[2m  14 ctx\x1b[0m\r\n"));
	T_RUN("basic_lines_ansi");
}

static void test_basic_lines_plain(void)
{
	tstr out;

	diff_render(&out, 0, "-12 old\n+13 new\n 14 ctx\n");
	T_CHECK(tstr_contains(&out, "-12 old\n"));
	T_CHECK(tstr_contains(&out, "+13 new\n"));
	T_CHECK(tstr_contains(&out, " 14 ctx\n"));
	T_CHECK(!tstr_contains(&out, "\x1b["));
	T_RUN("basic_lines_plain");
}

static void test_intra_line_pair(void)
{
	tstr out;

	/* 1:1 删除/新增：公共前后缀外的中段反色 */
	diff_render(&out, 1, "-1 return 0;\n+1 return 1;\n");
	/* 公共前缀 "return "（含中间空格差异→前缀为 "return "？"return 0;" vs "return 1;"
	 * 公共前缀 "return "，中段 "0"/"1"，公共后缀 ";" */
	T_CHECK(tstr_contains(&out, "\x1b[31m-1 return \x1b[7m0\x1b[27m;\x1b[0m"));
	T_CHECK(tstr_contains(&out, "\x1b[32m+1 return \x1b[7m1\x1b[27m;\x1b[0m"));
	T_RUN("intra_line_pair");
}

static void test_run_grouping(void)
{
	tstr out;

	/* 多行删除组 + 多行新增组：不做行内反色，删除全红、新增全绿 */
	diff_render(&out, 1, "-1 a1\n-2 a2\n+1 b1\n+2 b2\n");
	T_CHECK(tstr_contains(&out, "\x1b[31m-1 a1\x1b[0m"));
	T_CHECK(tstr_contains(&out, "\x1b[31m-2 a2\x1b[0m"));
	T_CHECK(tstr_contains(&out, "\x1b[32m+1 b1\x1b[0m"));
	T_CHECK(tstr_contains(&out, "\x1b[32m+2 b2\x1b[0m"));
	T_CHECK(!tstr_contains(&out, "\x1b[7m"));
	/* 后续上下文收束组 */
	diff_render(&out, 1, "-1 x\n+1 b\n  c\n");
	T_CHECK(tstr_contains(&out, "\x1b[31m-1 \x1b[7mx\x1b[27m\x1b[0m"));
	T_CHECK(tstr_contains(&out, "\x1b[32m+1 \x1b[7mb\x1b[27m\x1b[0m"));
	T_CHECK(tstr_contains(&out, "\x1b[2m  c\x1b[0m"));
	T_RUN("run_grouping");
}

static void test_standalone_added(void)
{
	tstr out;

	/* 无前导删除行的孤立新增行：绿色整行 */
	diff_render(&out, 1, "+1 fresh\n");
	T_CHECK(tstr_contains(&out, "\x1b[32m+1 fresh\x1b[0m"));
	T_CHECK(!tstr_contains(&out, "\x1b[7m"));
	T_RUN("standalone_added");
}

static void test_raw_context_lines(void)
{
	tstr out;

	/* 不匹配行（如 diff 头）按上下文色整行原样 */
	diff_render(&out, 1, "diff --git a/x b/x\n-1 a\n+1 b\n");
	T_CHECK(tstr_contains(&out, "\x1b[2mdiff --git a/x b/x\x1b[0m"));
	T_RUN("raw_context_lines");
}

static void test_tabs_expanded(void)
{
	tstr out;

	diff_render(&out, 0, "-1 \tone\n+1 \ttwo\n");
	T_CHECK(tstr_contains(&out, "-1    one")); /* tab → 3 空格 */
	T_RUN("tabs_expanded");
}

static void test_escape_and_utf8(void)
{
	tstr out;

	diff_render(&out, 0, "-1 a\t\033[2Jb\n+1 中文\n");
	T_CHECK(tstr_contains(&out, "\\x1b[2J"));
	T_CHECK(tstr_contains(&out, "+1 中文"));
	T_RUN("escape_and_utf8");
}

static void test_no_trailing_blank(void)
{
	tstr out;

	/* 尾部 \n 不产生多余空行；空行跳过 */
	diff_render(&out, 0, "-1 a\n+1 b\n");
	T_CHECK(out.len > 0 && out.buf[out.len - 1] == '\n');
	T_CHECK(!tstr_contains(&out, "\n\n"));
	T_RUN("no_trailing_blank");
}


int main(void)
{
	test_basic_lines_ansi();
	test_basic_lines_plain();
	test_intra_line_pair();
	test_run_grouping();
	test_standalone_added();
	test_raw_context_lines();
	test_tabs_expanded();
	test_escape_and_utf8();
	test_no_trailing_blank();
	return 0;
}
