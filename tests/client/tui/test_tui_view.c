/* test_tui_view.c — 展示渲染测试：事件行序、截断标记、UTF-8 拼接、双模式。 */
#include "test_util.h"

#include "tui_view.h"

static void test_editor_redraw_ansi(void)
{
	tstr out;
	tui_view v;
	tui_input in;

	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 1, &in);

	feed_bytes(&in, "你好");
	tui_view_redraw_editor(&v);
	T_CHECK(tstr_contains(&out, "> 你好"));
	tstr_clear(&out);

	/* 再输入一次：整行重绘带 \r 清行 */
	(void)tui_input_feed(&in, 0x7F);
	tui_view_redraw_editor(&v);
	T_CHECK(tstr_contains(&out, "\r\x1b[J> 你"));
	T_RUN("editor_redraw_ansi");
}

static void test_event_lines_ansi(void)
{
	tstr out;
	tui_view v;
	tui_input in;
	tui_event ev;

	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 1, &in);
	tui_view_redraw_editor(&v); /* 编辑行先在当前行 */
	tstr_clear(&out);

	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_RUN_STARTED;
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "\r\n[run] started\r\n")); /* 挂起编辑行 + 状态行 */
	tstr_clear(&out);

	ev.kind = TUI_EV_TEXT_DELTA;
	ev.text = "第一段";
	ev.text_len = strlen(ev.text);
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "第一段"));
	tstr_clear(&out);

	ev.kind = TUI_EV_TOOL_STARTED;
	ev.tool_call_id = "call-7";
	ev.tool_name = "read_file";
	tui_view_event(&v, &ev);
    ev.kind = TUI_EV_TOOL_OUTPUT;
    ev.text = "running\033[2J";
    ev.text_len = strlen(ev.text);
    tui_view_event(&v, &ev);
    T_CHECK(tstr_contains(&out, "running\\x1b[2J"));
    ev.text = "\xe4";
    ev.text_len = 1;
    tui_view_event(&v, &ev);
	ev.kind = TUI_EV_TOOL_FINISHED;
	ev.tool_ok = 1;
	ev.detail = "20 字节内容";
	ev.detail_len = strlen(ev.detail);
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "[tool] read_file (#call-7) started\r\n"));
	T_CHECK(tstr_contains(&out, "[tool] read_file (#call-7) finished ok: 20 字节内容\r\n"));
    T_CHECK(tstr_contains(&out, "\\xe4"));
	tstr_clear(&out);

	ev.kind = TUI_EV_RUN_FINISHED;
	ev.run_status = TUI_RUN_SUCCESS;
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "[run] finished: success\r\n"));
	T_CHECK(tstr_contains(&out, "> ")); /* 终态后提示符恢复（空编辑器） */
	T_RUN("event_lines_ansi");
}

static void test_utf8_split_join(void)
{
	tstr out;
	tui_view v;
	tui_input in;
	tui_event ev;

	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 1, &in);

	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_TEXT_DELTA;
	/* "演"(E6 BC 94) 拆成 2+1 字节跨事件到达 */
	ev.text = "\xe6\xbc";
	ev.text_len = 2;
	tui_view_event(&v, &ev);
	ev.text = "\x94示";
	ev.text_len = 3;
	tui_view_event(&v, &ev);
	/* 输出中必须出现连续完整的 E6 BC 94，且没有把 E6 BC 单独放出后接换行 */
	T_CHECK(find_bytes(out.buf, out.len, "\xe6\xbc\x94", 3) != NULL);
	{
		const char *p = find_bytes(out.buf, out.len, "\xe6\xbc\x94", 3);
		T_CHECK(p != NULL && (size_t)(p - out.buf) + 3 <= out.len);
	}
	/* 终态时冲掉遗留 carry：不完整序列按原样放出 */
	tstr_clear(&out);
	ev.kind = TUI_EV_RUN_FINISHED;
	ev.run_status = TUI_RUN_SUCCESS;
	tui_view_event(&v, &ev);
	tui_view_event(&v, &ev);
	T_RUN("utf8_split_join");
}

static void test_utf8_absorb_split(void)
{
	tstr out;
	tui_view v;
	tui_input in;
	tui_event ev;

	/* 4 字节 emoji F0 9F 98 80 按 1+2+1 跨事件到达：中段整体并入 carry，
	 * 不得提前写出半个序列（被替换 C 版在此路径会把已并入的字节写两遍，
	 * 本用例固定修正后的契约）。 */
	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 0, &in);

	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_TEXT_DELTA;
	ev.text = "\xf0";
	ev.text_len = 1;
	tui_view_event(&v, &ev);
	T_CHECK(out.len == 0);
	T_CHECK(v.carry_len == 1);
	ev.text = "\x9f\x98";
	ev.text_len = 2;
	tui_view_event(&v, &ev);
	T_CHECK(out.len == 0); /* 吸收段不产生输出 */
	T_CHECK(v.carry_len == 3);
	ev.text = "\x80";
	ev.text_len = 1;
	tui_view_event(&v, &ev);
	T_CHECK(out.len == 4);
	T_CHECK(memcmp(out.buf, "\xf0\x9f\x98\x80", 4) == 0); /* 恰好一次、连续完整 */
	T_CHECK(v.carry_len == 0);
	T_RUN("utf8_absorb_split");
}

static void test_truncation_marker(void)
{
	tstr out;
	tui_view v;
	tui_input in;
	tui_event ev;
	static char big[400];
	size_t i;

	for (i = 0; i < 158; i++)
		big[i] = 'a';
	big[158] = (char)0xE4; /* "中" 从 158 开始：160 上限放不下整个字符 */
	big[159] = (char)0xB8;
	big[160] = (char)0xAD;
	for (i = 161; i < 300; i++)
		big[i] = 'b';
	big[300] = '\0';

	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 1, &in);

	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_TOOL_FINISHED;
	ev.tool_call_id = "c1";
	ev.tool_name = "t";
	ev.tool_ok = 1;
	ev.detail = big;
	ev.detail_len = 300;
	tui_view_event(&v, &ev);
	/* keep = 158（"中" 整字符放不下），截断 142 字节，带明确标记 */
	T_CHECK(tstr_contains(&out, "…[+142 字节截断]"));
	T_CHECK(find_bytes(out.buf, out.len, "aaaaaaaaaa", 10) != NULL);
	T_CHECK(!tstr_contains(&out, "bb")); /* b 段未进入输出 */
	tstr_clear(&out);

	/* 错误详情上限 512 */
	ev.kind = TUI_EV_RUN_FINISHED;
	ev.run_status = TUI_RUN_ERROR;
	ev.detail = big;
	ev.detail_len = 300;
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "[run] finished: error: "));
	T_CHECK(!tstr_contains(&out, "字节截断")); /* 300 < 512 不截断 */
	T_RUN("truncation_marker");
}

static void test_plain_mode(void)
{
	tstr out;
	tui_view v;
	tui_input in;
	tui_event ev;
	size_t i;

	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 0, &in);

	memset(&ev, 0, sizeof(ev));
	tui_view_banner(&v, "fake-runtime");
	ev.kind = TUI_EV_RUN_STARTED;
	tui_view_event(&v, &ev);
	ev.kind = TUI_EV_TEXT_DELTA;
	ev.text = "普通\n文本";
	ev.text_len = strlen(ev.text);
	tui_view_event(&v, &ev);
	ev.kind = TUI_EV_RUN_FINISHED;
	ev.run_status = TUI_RUN_CANCELLED;
	tui_view_event(&v, &ev);

	/* 降级模式：无转义序列，换行保持 '\n' 原样（管道捕获需精确字节） */
	for (i = 0; i < out.len; i++)
		T_CHECK(out.buf[i] != 0x1B);
	T_CHECK(tstr_contains(&out, "[run] started\n"));
	T_CHECK(tstr_contains(&out, "普通\n文本"));
	T_CHECK(tstr_contains(&out, "[run] finished: cancelled\n"));
	T_CHECK(!tstr_contains(&out, "> ")); /* 不绘制编辑行（内核回显） */
	T_RUN("plain_mode");
}

static void test_error_and_cancel_lines(void)
{
	tstr out;
	tui_view v;
	tui_input in;
	tui_event ev;

	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 1, &in);

	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_RUN_FINISHED;
	ev.run_status = TUI_RUN_ERROR;
	ev.detail = "boom（假）";
	ev.detail_len = strlen(ev.detail);
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "[run] finished: error: boom（假）\r\n"));
	tstr_clear(&out);

	ev.run_status = TUI_RUN_CANCELLED;
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "[run] finished: cancelled\r\n"));
	T_RUN("error_and_cancel_lines");
}

static void test_history_and_controls(void)
{
 const char content[]="A\x1b]52;c;evil\a\b\r\x7f\xc2\x9b你好🙂\n\tB";
 for(int ansi=0;ansi<2;++ansi){tstr out;tui_view view;tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),ansi,NULL);
  tui_history_entry history={.kind=0,.text=content,.text_len=sizeof(content)-1};tui_view_history(&view,&history);
  T_CHECK(tstr_contains(&out,"[history] user: A\\x1b]52;c;evil\\x07\\x08\\x0d\\x7f\\u009b你好🙂"));T_CHECK(!memchr(out.buf,27,out.len));T_CHECK(!memchr(out.buf,7,out.len));
  history=(tui_history_entry){.kind=3,.text="ok",.text_len=2,.tool_name="read",.tool_name_len=4,.call_id="one",.call_id_len=3,.tool_ok=0};tui_view_history(&view,&history);T_CHECK(tstr_contains(&out,"[history] result read (#one) error: ok"));T_CHECK(!tstr_contains(&out,"[run] started")&&!tstr_contains(&out,"finished: success"));
 }
 const char stream[]="hi你好🙂\xc2\x9b\x1b[2J\r!";
 for(size_t split=0;split<sizeof(stream);++split){tstr out;tui_view view;tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_event event={.kind=TUI_EV_TEXT_DELTA,.text=stream,.text_len=split};tui_view_event(&view,&event);event.text=stream+split;event.text_len=sizeof(stream)-1-split;tui_view_event(&view,&event);T_CHECK(!strcmp(out.buf,"hi你好🙂\\u009b\\x1b[2J\\x0d!"));T_CHECK(!view.carry_len);}
 const char malformed[]="\xe0\x1b]52;bad\a\xc0\x9b\xed\xa0\x80";tstr out;tui_view view;tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_history_entry entry={.text=malformed,.text_len=sizeof(malformed)-1};tui_view_history(&view,&entry);T_CHECK(!memchr(out.buf,27,out.len)&&!memchr(out.buf,7,out.len));T_CHECK(tstr_contains(&out,"\\xe0\\x1b]52;bad\\x07\\xc0\\x9b\\xed\\xa0\\x80"));
 tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_event partial={.kind=TUI_EV_TEXT_DELTA,.text="\xe4",.text_len=1};tui_view_event(&view,&partial);T_CHECK(!out.len&&view.carry_len==1);partial=(tui_event){.kind=TUI_EV_RUN_FINISHED,.run_status=TUI_RUN_CANCELLED};tui_view_event(&view,&partial);T_CHECK(tstr_contains(&out,"\\xe4[run] finished: cancelled")&&!view.carry_len&&!memchr(out.buf,0xe4,out.len));
 T_RUN("history_and_control_escaping_all_splits");
}

int main(void)
{
    test_history_and_controls();
	test_editor_redraw_ansi();
	test_event_lines_ansi();
	test_utf8_split_join();
	test_utf8_absorb_split();
	test_truncation_marker();
	test_plain_mode();
	test_error_and_cancel_lines();
	printf("test_tui_view: all passed\n");
	return 0;
}
