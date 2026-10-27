/* test_tui_md.c — 流式 Markdown 渲染快照测试：块/内联/折行/表格/流式收尾。 */
#include "test_util.h"

#include "tui_md.h"

static void md_setup(tui_md *md, tstr *out, int ansi, unsigned cols)
{
	tui_md_init(md, tstr_sink(out), ansi, cols);
	tstr_clear(out);
}

/* md_doc 每例自包含：按上例的 ansi/cols 重置渲染器并清输出。 */
static void md_doc(tui_md *md, tstr *out, const char *s)
{
	int ansi = md->ansi;
	unsigned cols = md->cols;

	md_setup(md, out, ansi, cols);
	tui_md_text(md, s, strlen(s));
	tui_md_flush(md);
}

static void test_paragraphs_and_blanks(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "a\nb\n\nc\n");
	T_CHECK(strcmp(out.buf, "a\r\nb\r\n\r\nc\r\n") == 0);

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "\n\nx\n");
	T_CHECK(strcmp(out.buf, "x\r\n") == 0); /* 流首空行抑制 */

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "x\n\n\n\ny\n");
	T_CHECK(strcmp(out.buf, "x\r\n\r\ny\r\n") == 0); /* 连续空行折叠 */

	md_setup(&md, &out, 0, 80);
	md_doc(&md, &out, "a\n");
	T_CHECK(strcmp(out.buf, "a\n") == 0); /* 降级：\n 分隔，无转义 */
	T_RUN("paragraphs_and_blanks");
}

static void test_headings(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "# 标题一\n");
	T_CHECK(strcmp(out.buf, "\x1b[0;1;4m标题一\x1b[0m\r\n") == 0);
	md_doc(&md, &out, "## T2\n");
	T_CHECK(strcmp(out.buf, "\x1b[0;1mT2\x1b[0m\r\n") == 0);
	md_doc(&md, &out, "### 深层\n");
	T_CHECK(strcmp(out.buf, "### \x1b[0;1m深层\x1b[0m\r\n") == 0);
	/* 7 个 # 不是标题；# 后无空格也不是标题 */
	md_doc(&md, &out, "####### x\n");
	T_CHECK(strcmp(out.buf, "####### x\r\n") == 0);
	md_doc(&md, &out, "#nospace\n");
	T_CHECK(strcmp(out.buf, "#nospace\r\n") == 0);
	T_RUN("headings");
}

static void test_lists(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "- a\n- b\n");
	T_CHECK(strcmp(out.buf, "- a\r\n- b\r\n") == 0);
	md_doc(&md, &out, "* star\n+ plus\n");
	T_CHECK(strcmp(out.buf, "- star\r\n- plus\r\n") == 0);
	md_doc(&md, &out, "  - 嵌套\n");
	T_CHECK(strcmp(out.buf, "  - 嵌套\r\n") == 0);
	md_doc(&md, &out, "3. 第三\n");
	T_CHECK(strcmp(out.buf, "3. 第三\r\n") == 0);
	md_doc(&md, &out, "- [x] 完成\n- [ ] 待办\n");
	T_CHECK(strcmp(out.buf, "- [x] 完成\r\n- [ ] 待办\r\n") == 0);
	/* 空项与负数非列表 */
	md_doc(&md, &out, "-\n");
	T_CHECK(strcmp(out.buf, "-\r\n") == 0);
	md_doc(&md, &out, "-5 度\n");
	T_CHECK(strcmp(out.buf, "-5 度\r\n") == 0);
	T_RUN("lists");
}

static void test_quote_and_hr(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "> 引用\n");
	T_CHECK(strcmp(out.buf, "\xe2\x94\x82 引用\r\n") == 0);
	md_doc(&md, &out, ">> 嵌套\n");
	T_CHECK(strcmp(out.buf, "\xe2\x94\x82 \xe2\x94\x82 嵌套\r\n") == 0);
	md_doc(&md, &out, ">\n");
	T_CHECK(strcmp(out.buf, "\xe2\x94\x82\r\n") == 0);

	md_setup(&md, &out, 1, 10);
	md_doc(&md, &out, "---\n");
	T_CHECK(out.len == 10 * 3 + 2 && memcmp(out.buf, "\xe2\x94\x80", 3) == 0);
	T_CHECK(find_bytes(out.buf, out.len, "\xe2\x94\x80\r\n", 5) != NULL);
	/* - 开头三个以上也是分隔线，但 - a 是列表 */
	md_doc(&md, &out, "***\n");
	T_CHECK(out.len == 10 * 3 + 2);
	T_RUN("quote_and_hr");
}

static void test_code_fence(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "```rust\nlet x = 1;\n```\n");
	T_CHECK(strcmp(out.buf, "```rust\r\n  let x = 1;\r\n```\r\n") == 0);
	/* 代码内容不渲染 Markdown，控制字节转义 */
	md_doc(&md, &out, "~~~\n**not md** \x1b[31m\n~~~\n");
	T_CHECK(strcmp(out.buf, "~~~\r\n  **not md** \\x1b[31m\r\n~~~\r\n") == 0);
	/* ~~~ 与 ``` 混用不互闭 */
	md_doc(&md, &out, "```\na\n~~~\nb\n```\n");
	T_CHECK(strcmp(out.buf, "```\r\n  a\r\n  ~~~\r\n  b\r\n```\r\n") == 0);
	T_RUN("code_fence");
}

static void test_inline_styles(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "a **bold** b\n");
	T_CHECK(strcmp(out.buf, "a \x1b[0;1mbold\x1b[0m b\r\n") == 0);
	md_doc(&md, &out, "x *it* y\n");
	T_CHECK(strcmp(out.buf, "x \x1b[0;3mit\x1b[0m y\r\n") == 0);
	md_doc(&md, &out, "u `code` v\n");
	T_CHECK(strcmp(out.buf, "u \x1b[0;7mcode\x1b[0m v\r\n") == 0);
	md_doc(&md, &out, "s ~~del~~ t\n");
	T_CHECK(strcmp(out.buf, "s \x1b[0;9mdel\x1b[0m t\r\n") == 0);
	md_doc(&md, &out, "__bi__ _it_\n");
	T_CHECK(strcmp(out.buf, "\x1b[0;1mbi\x1b[0m \x1b[0;3mit\x1b[0m\r\n") == 0);
	/* 未闭合标记按字面输出 */
	md_doc(&md, &out, "1 * 2 = 2\n");
	T_CHECK(strcmp(out.buf, "1 * 2 = 2\r\n") == 0);
	/* 反斜杠转义 */
	md_doc(&md, &out, "\\*not em\\*\n");
	T_CHECK(strcmp(out.buf, "*not em*\r\n") == 0);
	/* 嵌套：粗体内斜体；未闭合 ** 保留字面（marked：**b*i* → **b+em(i)） */
	md_doc(&md, &out, "**b*i*b**\n");
	T_CHECK(strcmp(out.buf, "\x1b[0;1mb\x1b[0;1;3mi\x1b[0;1mb\x1b[0m\r\n") == 0);
	md_doc(&md, &out, "**b*i*\n");
	T_CHECK(strcmp(out.buf, "**b\x1b[0;3mi\x1b[0m\r\n") == 0);

	/* 降级：样式标记剥除 */
	md_setup(&md, &out, 0, 80);
	md_doc(&md, &out, "a **b** `c` *d*\n");
	T_CHECK(strcmp(out.buf, "a b c d\n") == 0);
	T_RUN("inline_styles");
}

static void test_links(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "看 [文档](https://example.com) 说明\n");
	T_CHECK(strcmp(out.buf, "看 文档 (https://example.com) 说明\r\n") == 0);
	/* 文本与 URL 相同：只显示一次 */
	md_doc(&md, &out, "[https://a.b](https://a.b)\n");
	T_CHECK(strcmp(out.buf, "https://a.b\r\n") == 0);
	/* 自动链接去尖括号 */
	md_doc(&md, &out, "去 <https://pi.dev> 看\n");
	T_CHECK(strcmp(out.buf, "去 https://pi.dev 看\r\n") == 0);
	md_doc(&md, &out, "邮箱 <mailto:a@b.c>\n");
	T_CHECK(strcmp(out.buf, "邮箱 a@b.c\r\n") == 0);
	/* 非链接尖括号保持字面 */
	md_doc(&md, &out, "a<b\n");
	T_CHECK(strcmp(out.buf, "a<b\r\n") == 0);
	T_RUN("links");
}

static void test_escaping(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "a\x1b[31mb\x07" "c\n");
	T_CHECK(strcmp(out.buf, "a\\x1b[31mb\\x07c\r\n") == 0);
	md_doc(&md, &out, "\xc2\x9b C1\n");
	T_CHECK(strcmp(out.buf, "\\u009b C1\r\n") == 0);
	md_doc(&md, &out, "\xc2\xad\n");
	T_CHECK(strcmp(out.buf, "\xc2\xad\r\n") == 0); /* 完整两字节原样 */
	md_doc(&md, &out, "a\tb\n");
	T_CHECK(strcmp(out.buf, "a   b\r\n") == 0); /* TAB→3空格 */
	/* 尾部不完整序列：flush 按字节转义 */
	tstr_clear(&out);
	tui_md_text(&md, "\xe4", 1);
	tui_md_flush(&md);

	T_CHECK(strcmp(out.buf, "\\xe4\r\n") == 0);
	T_RUN("escaping");
}

static void test_wrap(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 10);
	md_doc(&md, &out, "abcdefghijklm\n");
	T_CHECK(strcmp(out.buf, "abcdefghij\r\nklm\r\n") == 0);
	/* CJK 宽度：恰好占满不折（5 字 = 10 列）；跨界宽字符折行（第 6 个起于
	 * 第 10 列需 2 格 > 剩余，与 ASCII 满行不折为同一规则） */
	md_doc(&md, &out, "一二三四五\n");
	T_CHECK(strcmp(out.buf, "\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b\xe4\xba\x94\r\n") == 0);
	md_doc(&md, &out, "一二三四五六\n");
	T_CHECK(strcmp(out.buf,
		"\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b\xe4\xba\x94\r\n\xe5\x85\xad\r\n") == 0);
	/* 列表续行缩进对齐内容 */
	md_doc(&md, &out, "- abcdefghijk\n");
	T_CHECK(strcmp(out.buf, "- abcdefgh\r\n  ijk\r\n") == 0);
	T_RUN("wrap");
}

static void test_table(void)
{
	tui_md md;
	tstr out;

	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "| a | b |\n|---|---|\n| 1 | 2 |\n");
	/* 表头整行加粗（SGR 包裹，行尾是复位码） */
	T_CHECK(tstr_contains(&out, "\xe2\x94\x82 a \xe2\x94\x82 b \xe2\x94\x82"));
	T_CHECK(tstr_contains(&out, "\xe2\x94\x82 1 \xe2\x94\x82 2 \xe2\x94\x82\r\n"));
	T_CHECK(find_bytes(out.buf, out.len, "\xe2\x94\x8c", 3) != NULL); /* ┌ */
	T_CHECK(find_bytes(out.buf, out.len, "\xe2\x94\x94", 3) != NULL); /* └ */
	/* 过窄回退原文 */
	md_setup(&md, &out, 1, 8);
	md_doc(&md, &out, "| aaa | bbb |\n|---|---|\n| 1 | 2 |\n");
	T_CHECK(tstr_contains(&out, "| aaa | bbb |\r\n"));
	T_CHECK(tstr_contains(&out, "| 1 | 2 |\r\n"));
	/* 单元格超宽截断带 …（cell_width 24 列封顶：真宽 > 24 才截断；
	 * 终端 40 列避免整表过窄回退原文） */
	md_setup(&md, &out, 1, 40);
	md_doc(&md, &out, "| 短 | 很长很长的单元格内容超出上限 |\n|---|---|\n");
	T_CHECK(find_bytes(out.buf, out.len, "\xe2\x80\xa6", 3) != NULL);
	T_RUN("table");
}

static void test_streaming_unclosed(void)
{
	tui_md md;
	tstr out;

	/* 未闭合围栏：flush 输出内容、无闭合栏 */
	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "```c\nint x;\n");
	T_CHECK(strcmp(out.buf, "```c\r\n  int x;\r\n") == 0);
	/* 部分闭栏暂扣：后续内容到达则按代码行补发 */
	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "```\ncode\n``\nmore\n```\n");
	T_CHECK(strcmp(out.buf, "```\r\n  code\r\n  ``\r\n  more\r\n```\r\n") == 0);
	/* 结束时仍是部分闭栏：丢弃（不闪烁显示半截栏） */
	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "```\ncode\n``");
	T_CHECK(strcmp(out.buf, "```\r\n  code\r\n") == 0);
	/* 未闭合表格：flush 渲染已缓冲行 */
	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "|a|b|\n|-|-|\n|1|2|");
	T_CHECK(tstr_contains(&out, "\xe2\x94\x82 1 \xe2\x94\x82 2 \xe2\x94\x82"));
	/* 尾行无换行：flush 按段落输出 */
	md_setup(&md, &out, 1, 80);
	md_doc(&md, &out, "tail");
	T_CHECK(strcmp(out.buf, "tail\r\n") == 0);
	T_RUN("streaming_unclosed");
}

static void test_splits_equivalence(void)
{
	/* 任意字节拆分喂入与一次性喂入输出一致（流式核心契约） */
	const char *doc = "# 标题\n\n- 列表 **粗体**\n- `代码`\n\n> 引用\n\n```rs\nfn f() {}\n```\n\n| a | b |\n|---|---|\n| 中文 | table |\n\n[链接](https://x.y) 尾行";
	size_t n = strlen(doc);
	size_t split;
	int ansi;

	for (ansi = 0; ansi <= 1; ansi++) {
		tstr whole;
		tui_md md;

		tstr_clear(&whole);
		md_setup(&md, &whole, ansi, 40);
		tui_md_text(&md, doc, n);
		tui_md_flush(&md);
		for (split = 0; split <= n; split++) {
			tstr piece;
			tui_md m2;

			tstr_clear(&piece);
			md_setup(&m2, &piece, ansi, 40);
			tui_md_text(&m2, doc, split);
			tui_md_text(&m2, doc + split, n - split);
			tui_md_flush(&m2);
			if (piece.len != whole.len || memcmp(piece.buf, whole.buf, whole.len) != 0) {
				fprintf(stderr, "split %zu (ansi=%d) differs\n", split, ansi);
				exit(1);
			}
		}
	}
	T_RUN("splits_equivalence");
}

static void test_line_overflow_split(void)
{
	tui_md md;
	tstr out;
	char big[600];
	size_t i;
	size_t count = 0;

	for (i = 0; i < sizeof(big); i++)
		big[i] = 'a';
	md_setup(&md, &out, 1, 200);
	tui_md_text(&md, big, sizeof(big));
	T_CHECK(out.len > 0); /* 600 > 511 上限：已触发码点边界拆分输出 */
	T_CHECK(memcmp(out.buf, "a", 1) == 0);
	tui_md_text(&md, "+\n", 2);
	tui_md_flush(&md);

	for (i = 0; i < out.len; i++)
		if (out.buf[i] == 'a')
			count++;
	T_CHECK(count == 600); /* 511 拆分 + 89 不丢字节 */
	T_CHECK(out.buf[out.len - 3] == '+'); /* 尾字符完整保留 */
	T_RUN("line_overflow_split");
}

int main(void)
{
	test_paragraphs_and_blanks();
	test_headings();
	test_lists();
	test_quote_and_hr();
	test_code_fence();
	test_inline_styles();
	test_links();
	test_escaping();
	test_wrap();
	test_table();
	test_streaming_unclosed();
	test_splits_equivalence();
	test_line_overflow_split();
	printf("test_tui_md: all passed\n");
	return 0;
}
