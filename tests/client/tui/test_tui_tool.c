/* test_tui_tool.c — 工具展示状态机：头行/预览/提示/展开/有界/转义/双模式。 */
#include "test_util.h"

#include "tui_tool.h"
#include "tui_safe.h"

static void put_write(void *ctx, const char *data, size_t len)
{
	(void)tstr_write(ctx, data, len);
}

static void safe_write(void *ctx, const char *data, size_t len)
{
 tui_out_sink sink = tstr_sink(ctx);
 tui_safe_write(&sink, 0, data, len);
}

static void emit_setup(tui_tool_emit *emit, tstr *out)
{
	emit->ctx = out;
	emit->put = safe_write;
	emit->raw = put_write;
}

static void test_started_finished_plain(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_started(&t, "read_file", "call-1") == 1);
	T_CHECK(tstr_contains(&out, "[tool] read_file (#call-1) started\n"));
	tstr_clear(&out);
	T_CHECK(tui_tool_finished(&t, "call-1", 1, "20 字节内容", strlen("20 字节内容")) == 1);
	T_CHECK(tstr_contains(&out, "[tool] read_file (#call-1) ok: 20 字节内容\n"));
	T_CHECK(!tstr_contains(&out, "more lines"));
	T_RUN("started_finished_plain");
}

static void test_preview_and_expand(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;
	char text[256];
	size_t off = 0;
	int i;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_started(&t, "bash", "c1") == 1);
	for (i = 0; i < 25; i++) {
		int n = snprintf(text + off, sizeof(text) - off, "line%d\n", i);
		off += (size_t)n;
	}
	T_CHECK(tui_tool_output(&t, "c1", text, off) == 1);
	/* 前 10 行实时输出 */
	T_CHECK(tstr_contains(&out, "line0\n") && tstr_contains(&out, "line9\n"));
	T_CHECK(!tstr_contains(&out, "line10\n"));
	T_CHECK(t.slot[t.active].shown == 10 && t.slot[t.active].out_lines == 25);
	tstr_clear(&out);
	T_CHECK(tui_tool_finished(&t, "c1", 1, NULL, 0) == 1);
	T_CHECK(tstr_contains(&out, "... (15 more lines, to expand)\n"));
	T_CHECK(!tstr_contains(&out, "line12\n")); /* 提示前不泄露保留行 */
	/* 展开：输出保留尾部 15 行 */
	tstr_clear(&out);
	T_CHECK(tui_tool_expand(&t) == 1);
	T_CHECK(tstr_contains(&out, "[expand] #c1\n"));
	for (i = 10; i < 25; i++) {
		char want[32];
		snprintf(want, sizeof(want), "line%d\n", i);
		T_CHECK(tstr_contains(&out, want));
	}
	T_CHECK(!tstr_contains(&out, "line9\n")); /* 已实时输出过的不重复 */
	/* 再次展开：无操作 */
	tstr_clear(&out);
	T_CHECK(tui_tool_expand(&t) == 0);
	T_CHECK(out.len == 0);
	T_RUN("preview_and_expand");
}

static void test_expand_noop_cases(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	/* 无工具完成：无操作 */
	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_expand(&t) == 0);
	/* 输出 ≤ 预览行数：无隐藏行，展开无操作 */
	T_CHECK(tui_tool_started(&t, "t", "c1") == 1);
	T_CHECK(tui_tool_output(&t, "c1", "a\nb\n", 4) == 1);
	T_CHECK(tui_tool_finished(&t, "c1", 1, NULL, 0) == 1);
	tstr_clear(&out);
	T_CHECK(tui_tool_expand(&t) == 0);
	T_CHECK(out.len == 0);
	T_RUN("expand_noop_cases");
}

static void test_lossy_tail(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;
	char line[220];
	char text[40 * sizeof(line)];
	size_t off = 0;
	int i;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_started(&t, "big", "c1") == 1);
	for (i = 0; i < 40; i++) {
		int n = snprintf(line, sizeof(line), "row%03d-%202d\n", i, i);
		memcpy(text + off, line, (size_t)n);
		off += (size_t)n;
	}
	T_CHECK(tui_tool_output(&t, "c1", text, off) == 1);
	T_CHECK(t.slot[t.active].lossy == 1); /* 40 行 × ~212 字节 > 4096：尾部丢弃过更早输出 */
	tstr_clear(&out);
	T_CHECK(tui_tool_finished(&t, "c1", 1, NULL, 0) == 1);
	T_CHECK(tstr_contains(&out, "... (30 more lines, to expand)\n"));
	tstr_clear(&out);
	T_CHECK(tui_tool_expand(&t) == 1);
	T_CHECK(tstr_contains(&out, "(earlier output dropped)"));
	T_CHECK(tstr_contains(&out, "row039")); /* 保留最近输出 */
	T_CHECK(!tstr_contains(&out, "row000")); /* 更早的已丢弃 */
	T_RUN("lossy_tail");
}

static void test_line_split_and_utf8(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	/* 不完整 UTF-8 跨 TOOL_OUTPUT 分片：行缓冲拼合后才输出 */
	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_started(&t, "read", "c1") == 1);
	tstr_clear(&out);
	T_CHECK(tui_tool_output(&t, "c1", "\xe6\xbc", 2) == 0); /* "演" 前 2 字节 */
	T_CHECK(out.len == 0);
	T_CHECK(tui_tool_output(&t, "c1", "\x94示\n后续\n", strlen("\x94示\n后续\n")) == 1);
	T_CHECK(find_bytes(out.buf, out.len, "\xe6\xbc\x94示\n", strlen("\xe6\xbc\x94示\n")) != NULL);
	T_CHECK(t.slot[t.active].out_lines == 2);
	/* 512 字节满行按码点边界拆分：多行计数 */
	tstr_clear(&out);
	{
		char big[1200];
		memset(big, 'a', sizeof(big));
		memcpy(big + 510, "\xe4\xb8\xad", 3); /* "中" 跨 512 边界 */
		T_CHECK(tui_tool_output(&t, "c1", big, 600) == 1);
	}
	/* 510 a + 中(3B)：keep=510 → 第 1 行 510 字节；剩余 90 字节待 \n */
	T_CHECK(t.slot[t.active].out_lines == 3);
	T_RUN("line_split_and_utf8");
}

static void test_escape_safety(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_started(&t, "t", "c1") == 1);
	T_CHECK(tui_tool_output(&t, "c1", "x\033[2Jy\r\n", 8) == 1);
	T_CHECK(tstr_contains(&out, "x\\x1b[2Jy\\x0d"));
	T_CHECK(tui_tool_finished(&t, "c1", 0, "boom\x07", 5) == 1);
	T_CHECK(tstr_contains(&out, "[tool] t (#c1) error: boom\\x07"));
	T_RUN("escape_safety");
}

static void test_ansi_mode(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 1);
	T_CHECK(tui_tool_started(&t, "edit", "c9") == 1);
	T_CHECK(tstr_contains(&out, "[tool] \x1b[1medit\x1b[22m (#c9) started\n"));
	tstr_clear(&out);
	T_CHECK(tui_tool_finished(&t, "c9", 0, NULL, 0) == 1);
	T_CHECK(tstr_contains(&out, "[tool] \x1b[1medit\x1b[22m (#c9) \x1b[31merror\x1b[0m\n"));
	T_RUN("ansi_mode");
}

static void test_slots_and_interleave(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_started(&t, "a", "id-a") == 1);
	T_CHECK(tui_tool_started(&t, "b", "id-b") == 1);
	T_CHECK(tui_tool_output(&t, "id-a", "a1\n", 3) == 1);
	T_CHECK(tui_tool_output(&t, "id-b", "b1\n", 3) == 1);
	T_CHECK(tui_tool_finished(&t, "id-b", 1, NULL, 0) == 1);
	T_CHECK(tui_tool_finished(&t, "id-a", 1, NULL, 0) == 1);
	/* 最近完成的是 id-a：展开目标为 id-a（此处无隐藏行 → 无操作） */
	T_CHECK(tui_tool_expand(&t) == 0);
	/* 超过 4 个槽：轮转置换，最早的 id-a 被置换 */
	{
		int i;
		char id[16];
		for (i = 0; i < 4; i++) {
			snprintf(id, sizeof(id), "x%d", i);
			T_CHECK(tui_tool_started(&t, "t", id) == 1);
			T_CHECK(tui_tool_finished(&t, id, 1, NULL, 0) == 1);
		}
	}
	T_CHECK(tui_tool_output(&t, "id-b", "late\n", 5) == 0); /* 槽被置换后不匹配 → 丢弃 */
	T_RUN("slots_and_interleave");
}

static void test_unmatched_and_null(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_output(&t, "ghost", "x\n", 2) == 0);
	T_CHECK(tui_tool_finished(&t, "ghost", 1, NULL, 0) == 1); /* 未知完成仍出状态行 */
	T_CHECK(tstr_contains(&out, "[tool] ? (#ghost) ok"));
	T_CHECK(tui_tool_started(&t, NULL, NULL) == 1);
	T_CHECK(tstr_contains(&out, "[tool] ? (#?) started"));
	T_RUN("unmatched_and_null");
}

static void test_image_note(void)
{
	char buf[64];

	T_CHECK(tui_tool_image_note(buf, sizeof(buf), "image/png", 800, 600) > 0);
	T_CHECK(strcmp(buf, "[Image: [image/png] 800x600]") == 0);
	T_CHECK(tui_tool_image_note(buf, sizeof(buf), NULL, 1, 1) > 0);
	T_CHECK(strcmp(buf, "[Image: [image/unknown] 1x1]") == 0);
	T_CHECK(tui_tool_image_note(buf, 8, "image/png", 800, 600) == 0); /* 放不下 */
	T_RUN("image_note");
}

static void test_partial_final_line(void)
{
	tstr out;
	tui_tool_emit emit;
	tui_tool t;

	tstr_clear(&out);
	emit_setup(&emit, &out);
	tui_tool_init(&t, emit.ctx, emit.put, emit.raw, 0);
	T_CHECK(tui_tool_started(&t, "t", "c1") == 1);
	/* 12 行但最后一行无换行：完成时作为尾行计入 */
	{
		char text[64];
		size_t off = 0;
		int i;
		for (i = 0; i < 12; i++)
			off += (size_t)snprintf(text + off, sizeof(text) - off, "L%d\n", i);
		off -= 1; /* 去掉最后的 "\n"：L11 无行尾 */
		T_CHECK(tui_tool_output(&t, "c1", text, off) == 1);
	}
	tstr_clear(&out);
	T_CHECK(tui_tool_finished(&t, "c1", 1, NULL, 0) == 1);
	T_CHECK(t.slot[t.active].out_lines == 12);
	T_CHECK(!tstr_contains(&out, "L11\n")); /* 超出预览的尾行留待展开 */
	T_CHECK(tstr_contains(&out, "... (2 more lines, to expand)\n"));
	tstr_clear(&out);
	T_CHECK(tui_tool_expand(&t) == 1);
	T_CHECK(tstr_contains(&out, "L10\nL11\n"));
	T_RUN("partial_final_line");
}

static void test_empty_source_lines_and_splits(void)
{
 const char *text="\nalpha\n\nbeta\n\n";
 for(size_t split=0; split<=strlen(text); split++) {
  tstr out; tui_tool_emit emit; tui_tool t;
  tstr_clear(&out); emit_setup(&emit,&out);
  tui_tool_init(&t,emit.ctx,emit.put,emit.raw,0);
  tui_tool_started(&t,"bash","blank"); tstr_clear(&out);
  tui_tool_output(&t,"blank",text,split);
  tui_tool_output(&t,"blank",text+split,strlen(text)-split);
  T_CHECK(out.len==strlen(text) && !memcmp(out.buf,text,out.len));
  T_CHECK(t.slot[t.active].out_lines==5);
  T_CHECK(tui_tool_flush(&t)==0);
 }
 tstr out; tui_tool_emit emit; tui_tool t;
 tstr_clear(&out); emit_setup(&emit,&out);
 tui_tool_init(&t,emit.ctx,emit.put,emit.raw,0);
 tui_tool_started(&t,"bash","blank");
 const char *hidden="0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n\nlast\n\n";
 tui_tool_output(&t,"blank",hidden,strlen(hidden));
 tui_tool_finished(&t,"blank",1,NULL,0); tstr_clear(&out);
 T_CHECK(tui_tool_expand(&t)==1);
 T_CHECK(tstr_contains(&out,"[expand] #blank\n\nlast\n\n"));
 T_RUN("empty_source_lines_preserved_across_splits_and_expand");
}

static void test_expand_running_tool(void)
{
 tstr out; tui_tool_emit emit; tui_tool t;
 tstr_clear(&out); emit_setup(&emit,&out);
 tui_tool_init(&t,emit.ctx,emit.put,emit.raw,0);
 tui_tool_started(&t,"bash","live");
 const char *text="0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nhidden\n";
 tui_tool_output(&t,"live",text,strlen(text));
 tstr_clear(&out);
 T_CHECK(tui_tool_expand(&t)==1);
 T_CHECK(tstr_contains(&out,"hidden\n"));
 tstr_clear(&out);
 tui_tool_output(&t,"live","next\n",5);
 T_CHECK(!strcmp(out.buf,"next\n"));
 T_CHECK(tui_tool_expand(&t)==0);
 tui_tool_finished(&t,"live",1,NULL,0);
 T_CHECK(!tstr_contains(&out,"more lines"));
 tui_tool_started(&t,"bash","early");
 T_CHECK(tui_tool_expand(&t)==1);
 tstr_clear(&out);
 tui_tool_output(&t,"early",text,strlen(text));
 T_CHECK(!strcmp(out.buf,text));
 T_RUN("expand_running_tool_and_follow_new_output");
}

static void test_bridge_maximum_identity(void)
{
 tstr out; tui_tool_emit emit; tui_tool t;
 char id[64], name[64]; memset(id,'i',63); id[63]=0;
 memset(name,'n',63); name[63]=0;
 tstr_clear(&out); emit_setup(&emit,&out);
 tui_tool_init(&t,emit.ctx,emit.put,emit.raw,0);
 tui_tool_started(&t,name,id);
 T_CHECK(t.slot[t.active].id_len==63 && t.slot[t.active].name_len==63);
 const char *text="0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nhidden\n";
 T_CHECK(tui_tool_output(&t,id,text,strlen(text))==1);
 tui_tool_finished(&t,id,1,NULL,0);
 T_CHECK(t.slot[t.active].state==1);
 T_CHECK(tui_tool_expand(&t)==1);
 T_CHECK(tstr_contains(&out,"hidden\n"));
 /* Distinct IDs that share the previous 47-byte display prefix stay separate. */
 char other[64]; memcpy(other,id,64); other[62]='j';
 tui_tool_started(&t,name,other);
 tui_tool_output(&t,other,"second\n",7);
 tui_tool_finished(&t,other,0,NULL,0);
 T_CHECK(t.slot[t.active].state==2);
 T_CHECK(tstr_contains(&out,other) && tstr_contains(&out,id));
 T_RUN("bridge_max_identity_preserved_and_correlated");
}

static void test_expand_all_retained_tools(void)
{
 tstr out; tui_tool_emit emit; tui_tool t;
 tstr_clear(&out); emit_setup(&emit,&out);
 tui_tool_init(&t,emit.ctx,emit.put,emit.raw,0);
 const char *lines="0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nhidden\n";
 tui_tool_started(&t,"bash","done");
 tui_tool_output(&t,"done",lines,strlen(lines));
 tui_tool_finished(&t,"done",1,NULL,0);
 tui_tool_started(&t,"read","running");
 tui_tool_output(&t,"running",lines,strlen(lines));
 tstr_clear(&out);
 T_CHECK(tui_tool_expand(&t)==1);
 T_CHECK(tstr_contains(&out,"[expand] #done\nhidden\n"));
 T_CHECK(tstr_contains(&out,"[expand] #running\nhidden\n"));
 tstr_clear(&out);
 T_CHECK(tui_tool_expand(&t)==0 && out.len==0);
 tui_tool_output(&t,"running","new\n",4);
 T_CHECK(!strcmp(out.buf,"new\n"));
 T_RUN("expand_all_retained_tools");
}

int main(void)
{
 test_expand_all_retained_tools();
 test_bridge_maximum_identity();
 test_expand_running_tool();
 test_empty_source_lines_and_splits();
	test_started_finished_plain();
	test_preview_and_expand();
	test_expand_noop_cases();
	test_lossy_tail();
	test_line_split_and_utf8();
	test_escape_safety();
	test_ansi_mode();
	test_slots_and_interleave();
	test_unmatched_and_null();
	test_image_note();
	test_partial_final_line();
	return 0;
}
