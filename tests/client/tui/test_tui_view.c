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
	T_CHECK(out.len == 0); /* 未完行缓冲：工具/状态行打断时才输出 */

	ev.kind = TUI_EV_TOOL_STARTED;
	ev.tool_call_id = "call-7";
	ev.tool_name = "read_file";
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "第一段\r\n")); /* md_break 冲出未完行 */
	T_CHECK(tstr_contains(&out, "[tool] \x1b[1mread_file\x1b[22m (#call-7) started\r\n"));
	tstr_clear(&out);
    ev.kind = TUI_EV_TOOL_OUTPUT;
    ev.text = "running\033[2J";
    ev.text_len = strlen(ev.text);
    tui_view_event(&v, &ev);
    T_CHECK(out.len == 0); /* 工具输出行缓冲：行完整前不输出 */
    ev.text = "\xe4";
    ev.text_len = 1;
    tui_view_event(&v, &ev);
	ev.kind = TUI_EV_TOOL_FINISHED;
	ev.tool_ok = 1;
	ev.detail = "20 字节内容";
	ev.detail_len = strlen(ev.detail);
	tui_view_event(&v, &ev);
	T_CHECK(tstr_contains(&out, "running\\x1b[2J")); /* 控制字节转义 */
    T_CHECK(tstr_contains(&out, "\\xe4\r\n")); /* 尾行留存后随完成冲出 */
	T_CHECK(tstr_contains(&out, "[tool] \x1b[1mread_file\x1b[22m (#call-7) \x1b[32mok\x1b[0m: 20 字节内容\r\n"));
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
	/* "演"(E6 BC 94) 拆成 2+1 字节跨事件到达；\n 收束行触发输出
	 * （md 流式渲染：行缓冲内完成 UTF-8 拼合） */
	ev.text = "\xe6\xbc";
	ev.text_len = 2;
	tui_view_event(&v, &ev);
	ev.text = "\x94示\n";
	ev.text_len = strlen(ev.text);
	tui_view_event(&v, &ev);
	/* 输出中必须出现连续完整的 E6 BC 94，且没有把 E6 BC 单独放出后接换行 */
	T_CHECK(find_bytes(out.buf, out.len, "\xe6\xbc\x94", 3) != NULL);
	{
		const char *p = find_bytes(out.buf, out.len, "\xe6\xbc\x94", 3);
		T_CHECK(p != NULL && (size_t)(p - out.buf) + 3 <= out.len);
	}
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

	/* 4 字节 emoji F0 9F 98 80 按 1+2+1 跨事件到达：md 行缓冲拼合，
	 * 行完整前不得产生输出（含半个序列） */
	tstr_clear(&out);
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 0, &in);

	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_TEXT_DELTA;
	ev.text = "\xf0";
	ev.text_len = 1;
	tui_view_event(&v, &ev);
	T_CHECK(out.len == 0);
	T_CHECK(v.md.line_len == 1);
	ev.text = "\x9f\x98";
	ev.text_len = 2;
	tui_view_event(&v, &ev);
	T_CHECK(out.len == 0); /* 吸收段不产生输出 */
	T_CHECK(v.md.line_len == 3);
	ev.text = "\x80";
	ev.text_len = 1;
	tui_view_event(&v, &ev);
	T_CHECK(out.len == 0);
	T_CHECK(v.md.line_len == 4);
	ev.text = "\n";
	ev.text_len = 1;
	tui_view_event(&v, &ev);
	T_CHECK(out.len == 5);
	T_CHECK(memcmp(out.buf, "\xf0\x9f\x98\x80\n", 5) == 0); /* 恰好一次、连续完整 */
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
 const char stream[]="hi你好🙂\xc2\x9b\x1b[2J\r!\n";
 /* text_delta 经 tui_md 流式渲染：行完整才输出；孤儿 CR 丢弃（Pi 为 CR→LF，
 * 见 TODO 交接区差异）；C1/CSI 控制字节转义，降级模式行分隔为 \n */
 for(size_t split=0;split<sizeof(stream);++split){tstr out;tui_view view;tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_event event={.kind=TUI_EV_TEXT_DELTA,.text=stream,.text_len=split};tui_view_event(&view,&event);event.text=stream+split;event.text_len=sizeof(stream)-1-split;tui_view_event(&view,&event);T_CHECK(!strcmp(out.buf,"hi你好🙂\\u009b\\x1b[2J!\n"));}
 const char malformed[]="\xe0\x1b]52;bad\a\xc0\x9b\xed\xa0\x80";tstr out;tui_view view;tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_history_entry entry={.text=malformed,.text_len=sizeof(malformed)-1};tui_view_history(&view,&entry);T_CHECK(!memchr(out.buf,27,out.len)&&!memchr(out.buf,7,out.len));T_CHECK(tstr_contains(&out,"\\xe0\\x1b]52;bad\\x07\\xc0\\x9b\\xed\\xa0\\x80"));
 tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_event partial={.kind=TUI_EV_TEXT_DELTA,.text="\xe4",.text_len=1};tui_view_event(&view,&partial);T_CHECK(!out.len&&view.md.line_len==1);partial=(tui_event){.kind=TUI_EV_RUN_FINISHED,.run_status=TUI_RUN_CANCELLED};tui_view_event(&view,&partial);T_CHECK(tstr_contains(&out,"\\xe4\n[run] finished: cancelled")&& !memchr(out.buf,0xe4,out.len));
 T_RUN("history_and_control_escaping_all_splits");
}

static void test_cancel_keeps_pending_tool_output(void)
{
 tstr out;
 tui_view view;
 tstr_clear(&out);
 tui_view_init(&view, tstr_sink(&out), 0, NULL);
 tui_event ev = {.kind=TUI_EV_TOOL_STARTED, .tool_name="bash", .tool_call_id="pending"};
 tui_view_event(&view, &ev);
 ev.kind=TUI_EV_TOOL_OUTPUT;
 ev.text="partial\x1b[2J\xe4";
 ev.text_len=strlen(ev.text);
 tui_view_event(&view, &ev);
 tstr_clear(&out);
 ev.kind=TUI_EV_RUN_FINISHED;
 ev.run_status=TUI_RUN_CANCELLED;
 tui_view_event(&view, &ev);
 T_CHECK(tstr_contains(&out, "partial\\x1b[2J\\xe4\n[run] finished: cancelled\n"));
 T_CHECK(!tstr_contains(&out, "[tool]")); /* no invented tool outcome */
 T_CHECK(view.tool.slot[view.tool.active].state == 0);
 T_CHECK(tui_tool_flush(&view.tool) == 0); /* no duplicate partial line */
 /* Hidden output also remains reachable after the run terminal. */
 ev=(tui_event){.kind=TUI_EV_TOOL_STARTED, .tool_name="bash", .tool_call_id="hidden"};
 tui_view_event(&view, &ev);
 const char *lines="0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nhidden tail";
 ev.kind=TUI_EV_TOOL_OUTPUT; ev.text=lines; ev.text_len=strlen(lines);
 tui_view_event(&view, &ev);
 tstr_clear(&out);
 ev.kind=TUI_EV_RUN_FINISHED; ev.run_status=TUI_RUN_CANCELLED;
 tui_view_event(&view, &ev);
 T_CHECK(tstr_contains(&out, "Ctrl-O to expand"));
 tui_view_tool_expand(&view);
 T_CHECK(tstr_contains(&out, "hidden tail\n"));
 T_CHECK(tui_tool_flush(&view.tool) == 0);
 T_RUN("cancel_keeps_pending_tool_output");
}

static void test_vertical_matches_view_and_resize(void)
{
 tui_input in; tui_view view; tstr out;
 tui_input_init(&in); tstr_clear(&out);
 tui_view_init(&view,tstr_sink(&out),1,&in);
 tui_view_set_size(&view,6);
 T_CHECK(in.columns == 6);
 feed_bytes(&in,"abcdefghij");
 tui_view_redraw_editor(&view);
 T_CHECK(view.cursor_row == 1 && view.cursor_col == 6);
 feed_bytes(&in,"\x1b[A");
 tui_view_redraw_editor(&view);
 T_CHECK(in.cursor == 3 && view.cursor_row == 0 && view.cursor_col == 5);
 feed_bytes(&in,"\x1b[B");
 tui_view_redraw_editor(&view);
 T_CHECK(in.cursor == 10 && view.cursor_row == 1 && view.cursor_col == 6);
 tui_view_set_size(&view,8);
 T_CHECK(in.columns == 8 && in.preferred_valid == 0);
 feed_bytes(&in,"\x1b[A");
 tui_view_redraw_editor(&view);
 T_CHECK(in.cursor == 4 && view.cursor_row == 0 && view.cursor_col == 6);
 T_RUN("vertical_matches_view_and_resize");
}

static void test_help_preserves_editor(void)
{
 for(int ansi=0;ansi<2;ansi++) {
  tui_input in; tui_input_init(&in); feed_bytes(&in,"draft中\x1b[D");
  size_t cursor=in.cursor; tstr out; tstr_clear(&out); tui_view view;
  tui_view_init(&view,tstr_sink(&out),ansi,&in);
  tui_view_help(&view);
  T_CHECK(in.cursor==cursor && in.len==strlen("draft中"));
  T_CHECK(tstr_contains(&out,"[help] PanGu"));
  T_CHECK(tstr_contains(&out,"Ctrl-C:"));
  if(ansi) T_CHECK(view.cursor_col == 7);
  else T_CHECK(!memchr(out.buf,27,out.len));
 }
 T_RUN("help_preserves_editor");
}

static void test_stream_redraw_omits_partial_editor_codepoint(void)
{
 tui_input in; tui_view view; tstr out;
 tui_input_init(&in); tstr_clear(&out);
 tui_view_init(&view,tstr_sink(&out),1,&in);
 feed_bytes(&in,"ab\x1b[D"); tui_input_feed(&in,0xe4);
 tui_view_redraw_editor(&view);
 T_CHECK(tstr_contains(&out,"> ab") && !memchr(out.buf,0xe4,out.len));
 T_CHECK(view.cursor_col==3 && in.cursor==2 && in.utf8_pending==2);
 tstr_clear(&out);
 tui_event ev={.kind=TUI_EV_TEXT_DELTA,.text="background\n",.text_len=11};
 tui_view_event(&view,&ev);
 T_CHECK(tstr_contains(&out,"background") && !memchr(out.buf,0xe4,out.len));
 T_CHECK(in.utf8_pending==2 && in.len==3);
 tui_view_set_size(&view,4); tstr_clear(&out); tui_view_redraw_editor(&view);
 T_CHECK(!memchr(out.buf,0xe4,out.len) && in.utf8_pending==2);
 tui_input_feed(&in,0xb8); tui_input_feed(&in,0xad);
 tstr_clear(&out); tui_view_redraw_editor(&view);
 T_CHECK(tstr_contains(&out,"中") && in.len==5 && in.cursor==4);
 tui_input_reset(&in); feed_bytes(&in,"\x1b[200~a\xe4\x1b[201~");
 tstr_clear(&out); tui_view_redraw_editor(&view);
 T_CHECK(!memchr(out.buf,0xe4,out.len) && in.utf8_pending==2);
 T_RUN("stream_resize_and_paste_hide_partial_editor_codepoint");
}

static void test_transcript_read_anchor_and_backlog(void)
{
 tui_input in; tui_input_init(&in); feed_bytes(&in,"draft🙂");
 tui_view view; tstr out; tstr_clear(&out);
 tui_view_init(&view,tstr_sink(&out),1,&in);
 tui_event ev={.kind=TUI_EV_TEXT_DELTA,.text="line0\nline1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\n"};
 ev.text_len=strlen(ev.text); tui_view_event(&view,&ev);
 tui_view_page(&view,-1);
 T_CHECK(view.reading && tstr_contains(&out,"\x1b[?1049h"));
 size_t anchor=view.read_anchor; tstr_clear(&out);
 ev.text="new while reading\n\x1b]52;evil\a\n"; ev.text_len=strlen(ev.text);
 tui_view_event(&view,&ev);
 T_CHECK(view.read_anchor==anchor && out.len==0);
 T_CHECK(in.len==strlen("draft🙂"));
 tui_view_return_latest(&view);
 T_CHECK(!view.reading && tstr_contains(&out,"\x1b[?1049l"));
 T_CHECK(tstr_contains(&out,"new while reading"));
 T_CHECK(!tstr_contains(&out,"\x1b]52;evil"));
 T_CHECK(tstr_contains(&out,"> draft🙂"));
 tui_view_page(&view,-1); tstr_clear(&out);
 tui_view_exit_note(&view,"test");
 T_CHECK(!view.reading && tstr_contains(&out,"\x1b[?1049l"));
 T_RUN("transcript_anchor_backlog_and_exit_restore");
}

static void test_transcript_eviction_and_page_navigation(void)
{
 tui_input in; tui_input_init(&in); tui_view view; tstr out; tstr_clear(&out);
 tui_view_init(&view,tstr_sink(&out),1,&in);
 char line[512]; memset(line,'x',sizeof(line)); line[511]='\n';
 tui_event ev={.kind=TUI_EV_TOOL_OUTPUT,.text=line,.text_len=sizeof(line)};
 /* Unknown tool outputs still belong to the source transcript. */
 for(unsigned i=0;i<126;i++) tui_view_event(&view,&ev);
 tui_view_page(&view,-1); size_t anchor=view.read_anchor;
 tstr_clear(&out);
 for(unsigned i=0;i<10;i++) tui_view_event(&view,&ev);
 T_CHECK(view.transcript_len<=sizeof(view.transcript) && view.transcript_dropped);
 T_CHECK(view.read_anchor<anchor && out.len==0);
 T_CHECK(view.read_start<=view.transcript_len);
 tui_view_page(&view,-1);
 T_CHECK(tstr_contains(&out,"older buffered text was evicted"));
 for(unsigned i=0;i<20 && view.reading;i++) {tstr_clear(&out); tui_view_page(&view,1);}
 T_CHECK(!view.reading);
 T_RUN("transcript_eviction_adjusts_anchor_and_pages_to_latest");
}

static void test_read_visual_rows_do_not_skip_long_lines(void)
{
 tui_input in; tui_input_init(&in); tui_view view; tstr out; tstr_clear(&out);
 tui_view_init(&view,tstr_sink(&out),1,&in); tui_view_set_size(&view,8); view.page_rows=2;
 const char *text="0123456789abcdefghijklmnop中🙂XYZ\n";
 tui_event ev={.kind=TUI_EV_TEXT_DELTA,.text=text,.text_len=strlen(text)};
 tui_view_event(&view,&ev);
 tui_view_page(&view,-1); tui_view_page(&view,-1); tui_view_page(&view,-1);
 T_CHECK(view.read_anchor==0);
 tstr_clear(&out); tui_view_page(&view,0);
 T_CHECK(tstr_contains(&out,"01234567\r\n89abcdef\r\n"));
 tstr_clear(&out); tui_view_page(&view,1);
 T_CHECK(view.read_anchor==16*32 && tstr_contains(&out,"ghijklmn\r\nop中🙂XY\r\n"));
 tstr_clear(&out); tui_view_page(&view,1);
 T_CHECK(view.read_anchor==(26+strlen("中🙂XY"))*32);
 T_CHECK(tstr_contains(&out,"Z\r\n"));
 tui_view_return_latest(&view);
 tui_view_set_size(&view,7); tstr_clear(&out); tui_view_page(&view,-1);
 T_CHECK(view.reading && !tstr_contains(&out,"<8 cols"));
 tui_view_page(&view,1); T_CHECK(!view.reading);
 tui_view_init(&view,tstr_sink(&out),1,&in); tui_view_set_size(&view,8); view.page_rows=1;
 ev.text="1234567\x1bXYZ\n"; ev.text_len=strlen(ev.text); tui_view_event(&view,&ev);
 tui_view_page(&view,-1); tui_view_page(&view,-1); tui_view_page(&view,-1);
 tstr_clear(&out); tui_view_page(&view,0);
 T_CHECK(tstr_contains(&out,"1234567\\\r\n"));
 tstr_clear(&out); tui_view_page(&view,1);
 T_CHECK(view.read_anchor==7*32+1 && tstr_contains(&out,"x1bXYZ\r\n"));
 T_CHECK(!tstr_contains(&out,"\x1bXYZ"));
 tui_view_return_latest(&view);
 T_RUN("read_visual_rows_long_ascii_cjk_and_narrow_limit");
}

static void test_one_column_reader_continues_escaped_wide_character(void)
{
 tui_input in; tui_input_init(&in); tui_view view; tstr out; tstr_clear(&out);
 tui_view_init(&view,tstr_sink(&out),1,&in); tui_view_set_size(&view,1); view.page_rows=1;
 tui_event ev={.kind=TUI_EV_TEXT_DELTA,.text="中\n",.text_len=strlen("中\n")};
 tui_view_event(&view,&ev);
 tui_view_page(&view,-1);
 for(unsigned i=0;i<20;i++) tui_view_page(&view,-1);
 const char *visible="\\xe4\\xb8\\xad";
 for(size_t i=0;i<strlen(visible);i++) {
  char expected[32]; snprintf(expected,sizeof(expected),"\x1b[H\x1b[2J[\r\n%c\r\n",visible[i]);
  tstr_clear(&out); tui_view_page(&view,i?1:0);
  T_CHECK(view.reading && view.read_anchor==i);
  T_CHECK(!strcmp(out.buf,expected));
 }
 tui_view_page(&view,1); T_CHECK(!view.reading);
 T_RUN("one_column_reader_resumes_wide_character_escape_without_loss");
}

static void test_reader_tiny_height_does_not_scroll_last_row(void)
{
 for(unsigned rows=1;rows<=3;rows++) {
  tui_input in; tui_input_init(&in); tui_view view; tstr out; tstr_clear(&out);
  tui_view_init(&view,tstr_sink(&out),1,&in); tui_view_set_size(&view,1);
  view.terminal_rows=rows; view.page_rows=10;
  if(rows==3) view.transcript_dropped=1;
  tui_event ev={.kind=TUI_EV_TEXT_DELTA,.text="ab\n",.text_len=3};
  tui_view_event(&view,&ev);
  tui_view_page(&view,-1);
  for(unsigned i=0;i<5;i++) tui_view_page(&view,-1);
  tstr_clear(&out); tui_view_page(&view,0);
  const char *expected=rows==1?"\x1b[H\x1b[2Ja":rows==2?"\x1b[H\x1b[2J[\r\na":"\x1b[H\x1b[2J[\r\n[\r\na";
  T_CHECK(!strcmp(out.buf,expected));
  tstr_clear(&out); tui_view_page(&view,1);
  T_CHECK(view.reading && out.len && out.buf[out.len-1]=='b');
  tui_view_page(&view,1); T_CHECK(!view.reading);
 }
 T_RUN("reader_one_to_three_rows_keeps_last_cell_visible");
}

int main(void)
{
 test_reader_tiny_height_does_not_scroll_last_row();
 test_one_column_reader_continues_escaped_wide_character();
 test_read_visual_rows_do_not_skip_long_lines();
 test_transcript_eviction_and_page_navigation();
 test_transcript_read_anchor_and_backlog();
 test_stream_redraw_omits_partial_editor_codepoint();
 test_help_preserves_editor();
 test_vertical_matches_view_and_resize();
 test_cancel_keeps_pending_tool_output();
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
