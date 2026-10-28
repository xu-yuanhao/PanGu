/* test_tui_diff.c — diff/patch 渲染：行分组、1:1 行内反色、上下文、转义、双模式。 */
#include "test_util.h"

#include "tui_diff.h"

/* Linker wrapping exercises the pair allocation failure and release boundary. */
void *__real_malloc(size_t);
void __real_free(void *);
static int fail_pair_alloc;
static size_t pair_alloc_live;
void *__wrap_malloc(size_t size)
{
 if(fail_pair_alloc) return NULL;
 void *p=__real_malloc(size);
 if(p) ++pair_alloc_live;
 return p;
}
void __wrap_free(void *p)
{
 if(p){T_CHECK(pair_alloc_live>0);--pair_alloc_live;}
 __real_free(p);
}

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
 T_CHECK(pair_alloc_live==0);
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


static void test_long_lines_are_not_truncated(void)
{
 tstr out; char text[5000];
 size_t n=0; memcpy(text+n,"-7 ",3);n+=3;
 memset(text+n,'x',1500);n+=1500;
 memcpy(text+n,"\t中TAIL\n+7 ",strlen("\t中TAIL\n+7 "));n+=strlen("\t中TAIL\n+7 ");
 memset(text+n,'y',1500);n+=1500;memcpy(text+n,"\t🙂END\n",strlen("\t🙂END\n"));n+=strlen("\t🙂END\n");text[n]=0;
 diff_render(&out,0,text);
 T_CHECK(tstr_contains(&out,"   中TAIL\n+7 "));
 T_CHECK(tstr_contains(&out,"   🙂END\n"));
 T_CHECK(out.len==n+4); /* each TAB becomes three spaces */
 T_CHECK(!tstr_contains(&out,"\x1b["));
 diff_render(&out,1,text);
 T_CHECK(tstr_contains(&out,"\x1b[31m-7 ") && tstr_contains(&out,"\x1b[32m+7 "));
 T_CHECK(tstr_contains(&out,"中TAIL\x1b[27m\x1b[0m\r\n"));
 T_CHECK(tstr_contains(&out,"🙂END\x1b[27m\x1b[0m\r\n"));
 T_RUN("long_diff_lines_full_text_and_streamed_tabs");
}

static void test_numeric_content_is_not_line_number(void)
{
 tstr out;
 diff_render(&out,1,"-7 123 old\n+7 456 new\n");
 T_CHECK(tstr_contains(&out,"-7 \x1b[7m123 old\x1b[27m"));
 T_CHECK(tstr_contains(&out,"+7 \x1b[7m456 new\x1b[27m"));
 diff_render(&out,0,"-7 123 old\n+7 456 new\n");
 T_CHECK(!strcmp(out.buf,"-7 123 old\n+7 456 new\n"));
 diff_render(&out,0,"- 12 34 value\n+ 12 35 value\n");
 T_CHECK(!strcmp(out.buf,"- 12 34 value\n+ 12 35 value\n"));
 diff_render(&out,0,"-7\t123\n+7\t456\n");
 T_CHECK(!strcmp(out.buf,"-7 123\n+7 456\n"));
 diff_render(&out,1,"- 12foo\n+ 13foo\n");
 T_CHECK(tstr_contains(&out,"\x1b[31m- \x1b[7m12foo\x1b[27m"));
 diff_render(&out,0,"-7 123\n+7 1234\n");
 T_CHECK(!strcmp(out.buf,"-7 123\n+7 1234\n"));
 diff_render(&out,1,"-7 中\n+7 中文\n");
 T_CHECK(tstr_contains(&out,"-7 中"));
 T_CHECK(tstr_contains(&out,"+7 中\x1b[7m文\x1b[27m"));
 T_RUN("numeric_content_and_tab_separator_match_upstream_regex");
}

static void test_ecmascript_whitespace_framing(void)
{
 static const char *ws[]={" ","\t","\v","\f","\r", "\xc2\xa0", "\xe1\x9a\x80",
  "\xe2\x80\x80","\xe2\x80\x81","\xe2\x80\x82","\xe2\x80\x83",
  "\xe2\x80\x84","\xe2\x80\x85","\xe2\x80\x86","\xe2\x80\x87",
  "\xe2\x80\x88","\xe2\x80\x89","\xe2\x80\x8a","\xe2\x80\xa8",
  "\xe2\x80\xa9","\xe2\x80\xaf","\xe2\x81\x9f","\xe3\x80\x80","\xef\xbb\xbf"};
 tstr out; char source[128];
 for(size_t i=0;i<sizeof(ws)/sizeof(ws[0]);++i){
  snprintf(source,sizeof(source),"-7%s123\n+7%s456\n",ws[i],ws[i]);
  diff_render(&out,0,source);
  T_CHECK(!strcmp(out.buf,"-7 123\n+7 456\n"));
  snprintf(source,sizeof(source),"%s7 context\n",ws[i]);
  diff_render(&out,0,source);
  T_CHECK(!strcmp(out.buf," 7 context\n"));
  snprintf(source,sizeof(source),"-%s12foo\n",ws[i]);
  diff_render(&out,0,source);
  T_CHECK(!strcmp(out.buf,"- 12foo\n"));
 }
 /* U+200B and U+0085 are not ECMAScript whitespace. Incomplete sequences
  * remain raw and are safely escaped; no width helper reads beyond len. */
 diff_render(&out,0,"-7\xe2\x80\x8b" "x\n");
 T_CHECK(!strcmp(out.buf,"-7\xe2\x80\x8b" "x\n"));
 diff_render(&out,0,"-7\xc2\x85" "x\n");
 T_CHECK(tstr_contains(&out,"-7\\u0085x"));
 diff_render(&out,0,"-7\xe2\x80");
 T_CHECK(tstr_contains(&out,"-7\\xe2\\x80"));
 diff_render(&out,1,"+7 x\r\n");
 T_CHECK(tstr_contains(&out,"\x1b[2m+7 x\\x0d\x1b[0m"));
 diff_render(&out,1,"+7 x\xe2\x80\xa8" "y\n");
 T_CHECK(tstr_contains(&out,"\x1b[2m+7 x\xe2\x80\xa8" "y\x1b[0m"));
 diff_render(&out,1,"+7 x\xe2\x80\xa9\n");
 T_CHECK(tstr_contains(&out,"\x1b[2m+7 x\xe2\x80\xa9\x1b[0m"));
 T_RUN("ecmascript_whitespace_scalar_framing");
}

static void test_full_source_run_grouping(void)
{
 tstr out; char source[6000]; size_t n=0;
 for(unsigned i=0;i<33;i++) n+=(size_t)snprintf(source+n,sizeof(source)-n,"-%u old%u\n",i,i);
 n+=(size_t)snprintf(source+n,sizeof(source)-n,"+1 new\n");
 diff_render(&out,1,source);
 T_CHECK(!tstr_contains(&out,"\x1b[7m"));
 T_CHECK(tstr_contains(&out,"\x1b[31m-32 old32\x1b[0m"));
 T_CHECK(tstr_contains(&out,"\x1b[32m+1 new\x1b[0m"));
 n=0; n+=(size_t)snprintf(source+n,sizeof(source)-n,"-1 old\n");
 for(unsigned i=0;i<33;i++) n+=(size_t)snprintf(source+n,sizeof(source)-n,"+%u new%u\n",i,i);
 diff_render(&out,1,source);
 T_CHECK(!tstr_contains(&out,"\x1b[7m"));
 T_CHECK(tstr_contains(&out,"\x1b[32m+32 new32\x1b[0m"));
 n+=(size_t)snprintf(source+n,sizeof(source)-n," 1 context\n-2 aa\n+2 ab\n");
 diff_render(&out,1,source);
 T_CHECK(tstr_contains(&out,"-2 \x1b[7maa\x1b[27m"));
 /* A long line in the same run must not turn its short tail into 1:1. */
 n=0; memcpy(source,"-1 ",3);n=3;memset(source+n,'x',1000);n+=1000;
 n+=(size_t)snprintf(source+n,sizeof(source)-n,"\n-2 old\n+2 new\n");
 diff_render(&out,1,source);
 T_CHECK(!tstr_contains(&out,"\x1b[7m"));
 T_CHECK(tstr_contains(&out,"\x1b[31m-2 old\x1b[0m"));
 diff_render(&out,0,source);
 T_CHECK(!strcmp(out.buf,source));
 T_RUN("full_source_run_counts_and_long_line_boundaries");
}

static void test_pair_allocation_fallback(void)
{
 tstr out;
 fail_pair_alloc=1;
 diff_render(&out,1,"-1 old\n+1 new\n");
 T_CHECK(tstr_contains(&out,"\x1b[31m-1 old\x1b[0m"));
 T_CHECK(tstr_contains(&out,"\x1b[32m+1 new\x1b[0m"));
 T_CHECK(!tstr_contains(&out,"\x1b[7m"));
 fail_pair_alloc=0;
 diff_render(&out,1,"-1 \n+1 \n");
 T_CHECK(tstr_contains(&out,"-1 "));
 T_CHECK(tstr_contains(&out,"+1 "));
 T_CHECK(!tstr_contains(&out,"\x1b[7m"));
 diff_render(&out,1,"-1 same中🙂\n+1 same中🙂\n");
 T_CHECK(!tstr_contains(&out,"\x1b[7m"));
 diff_render(&out,1,"-1 a\n+1 ab\n");
 T_CHECK(tstr_contains(&out,"-1 \x1b[7ma\x1b[27m\x1b[0m"));
 T_CHECK(tstr_contains(&out,"+1 \x1b[7mab\x1b[27m"));
 diff_render(&out,1,"-1 ab\n+1 a\n");
 T_CHECK(tstr_contains(&out,"+1 \x1b[7ma\x1b[27m\x1b[0m"));
 T_RUN("pair_allocation_failure_preserves_complete_output_and_frees");
}

static void test_word_equal_whitespace_normalization(void)
{
 const char *pairs[][2]={
  {"foo  bar"," foo bar "},{"中 文","中文"},{"🙂 , 🙂","🙂,🙂"},
  {"é  x","é x"},{"a\xc2\xa0" "b","a b"},{"   "," "},
  {"a\xe2\x80\x83" "b","a b"},{"Ȁ  Ḁ","Ȁ Ḁ"}};
 tstr out;char source[300],expected[200];
 for(size_t i=0;i<sizeof(pairs)/sizeof(pairs[0]);i++) {
  snprintf(source,sizeof(source),"-7 %s\n+7 %s\n",pairs[i][0],pairs[i][1]);
  diff_render(&out,1,source);
  T_CHECK(!tstr_contains(&out,"\x1b[7m"));
  snprintf(expected,sizeof(expected),"\x1b[31m-7 %s\x1b[0m",pairs[i][1]);
  T_CHECK(tstr_contains(&out,expected));
  snprintf(expected,sizeof(expected),"\x1b[32m+7 %s\x1b[0m",pairs[i][1]);
  T_CHECK(tstr_contains(&out,expected));
  diff_render(&out,0,source);
  snprintf(expected,sizeof(expected),"-7 %s\n+7 %s\n",pairs[i][1],pairs[i][1]);
  T_CHECK(!strcmp(out.buf,expected));
 }
 /* Dropping whitespace must not merge Latin word tokens. */
 diff_render(&out,1,"-7 a b\n+7 ab\n");
 T_CHECK(tstr_contains(&out,"\x1b[7m"));
 diff_render(&out,1,"-7 é x\n+7 éx\n");
 T_CHECK(tstr_contains(&out,"\x1b[7m"));
 /* The source contains invalid UTF-8: safely retain the fallback path. */
 diff_render(&out,1,"-7 a\xff b\n+7 a b\n");
 T_CHECK(tstr_contains(&out,"\\xff"));
 T_RUN("word_token_zero_edit_new_whitespace_and_latin_boundaries");
}

static void test_disjoint_word_replacement(void)
{
 tstr out;
 diff_render(&out,1,"-7 foobar\n+7 foobaz\n");
 T_CHECK(tstr_contains(&out,"-7 \x1b[7mfoobar\x1b[27m"));
 T_CHECK(tstr_contains(&out,"+7 \x1b[7mfoobaz\x1b[27m"));
 diff_render(&out,1,"-7   old  \n+7  new \n");
 T_CHECK(tstr_contains(&out,"-7   \x1b[7mold  \x1b[27m"));
 T_CHECK(tstr_contains(&out,"+7  \x1b[7mnew \x1b[27m"));
 diff_render(&out,1,"-7 \xc2\xa0old\n+7   new\n");
 T_CHECK(tstr_contains(&out,"-7 \xc2\xa0\x1b[7mold\x1b[27m"));
 T_CHECK(tstr_contains(&out,"+7   \x1b[7mnew\x1b[27m"));
 diff_render(&out,1,"-7 élan\n+7 éclat\n");
 T_CHECK(tstr_contains(&out,"-7 \x1b[7mélan\x1b[27m"));
 diff_render(&out,1,"-7 \n+7  new\n");
 T_CHECK(tstr_contains(&out,"-7 \x1b[0m"));
 T_CHECK(tstr_contains(&out,"+7  \x1b[7mnew\x1b[27m"));
 diff_render(&out,1,"-7 old\n+7 \n");
 T_CHECK(tstr_contains(&out,"+7 \x1b[0m"));
 T_RUN("disjoint_tokens_whole_word_and_independent_leading_whitespace");
}

static void test_shared_context_word_boundaries(void)
{
 tstr out;
 diff_render(&out,1,"-7 const foobar = 1\n+7 const foobaz = 1\n");
 T_CHECK(tstr_contains(&out,"-7 const \x1b[7mfoobar\x1b[27m = 1"));
 T_CHECK(tstr_contains(&out,"+7 const \x1b[7mfoobaz\x1b[27m = 1"));
 diff_render(&out,1,"-7 call(fooTail);\n+7 call(barTail);\n");
 T_CHECK(tstr_contains(&out,"-7 call(\x1b[7mfooTail\x1b[27m);"));
 T_CHECK(tstr_contains(&out,"+7 call(\x1b[7mbarTail\x1b[27m);"));
 diff_render(&out,1,"-7 let élan = 1\n+7 let éclat = 1\n");
 T_CHECK(tstr_contains(&out,"-7 let \x1b[7mélan\x1b[27m = 1"));
 T_CHECK(tstr_contains(&out,"+7 let \x1b[7méclat\x1b[27m = 1"));
 diff_render(&out,1,"-7 abc 中尾\n+7 abd 中尾\n");
 T_CHECK(tstr_contains(&out,"-7 \x1b[7mabc\x1b[27m 中尾"));
 diff_render(&out,0,"-7 const foobar = 1\n+7 const foobaz = 1\n");
 T_CHECK(!strcmp(out.buf,"-7 const foobar = 1\n+7 const foobaz = 1\n"));
 T_RUN("shared_context_changed_word_edges_preserve_punctuation_and_cjk");
}

int main(void)
{
 test_shared_context_word_boundaries();
 test_disjoint_word_replacement();
 test_word_equal_whitespace_normalization();
 test_pair_allocation_fallback();
 test_full_source_run_grouping();
 test_ecmascript_whitespace_framing();
 test_numeric_content_is_not_line_number();
 test_long_lines_are_not_truncated();
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
