#include "tui_view.h"

#include <stdio.h>
#include <string.h>

#include "tui_utf8.h"

/* 统一输出：ANSI（raw 模式）下把 '\n' 翻译为 "\r\n"，避免 OPOST 关闭后的阶梯错位；
 * 降级模式按原字节写出（管道/文件捕获需要精确字节）。 */
static void vput(tui_view *v, const char *data, size_t len)
{
	if (len == 0)
		return;
	if (!v->ansi) {
		(void)v->sink.write(v->sink.ctx, data, len);
		return;
	}
	{
		size_t off = 0;
		size_t i;
		for (i = 0; i < len; i++) {
			if (data[i] == '\n') {
				if (i > off)
					(void)v->sink.write(v->sink.ctx, data + off, i - off);
				(void)v->sink.write(v->sink.ctx, "\r\n", 2);
				off = i + 1;
			}
		}
		if (len > off)
			(void)v->sink.write(v->sink.ctx, data + off, len - off);
	}
}

static void vputs(tui_view *v, const char *s)
{
	vput(v, s, strlen(s));
}

/* 事件输出统一入口：先挂起编辑行；输出后编辑缓冲非空则重绘编辑行 */
static void event_suspend(tui_view *v)
{
	if (v->ansi && v->editor_on_line) {
		vputs(v, "\r\n");
		v->editor_on_line = 0;
	}
}

static void event_resume_editor(tui_view *v)
{
	if (v->editor != NULL && !tui_input_empty(v->editor))
		tui_view_redraw_editor(v);
}

/* 有界展示：截断到 code point 边界，超长带明确标记 */
static void put_bounded(tui_view *v, const char *data, size_t len, size_t cap)
{
	size_t keep = tui_utf8_truncate_len(data, len, cap);

	vput(v, data, keep);
	if (keep < len) {
		char marker[64];
		int n = snprintf(marker, sizeof(marker), "…[+%zu 字节截断]", (unsigned long)(len - keep));
		if (n > 0)
			vput(v, marker, (size_t)n);
	}
}

/* 事件内字符串字段（工具名/调用 ID）的有界展示 */
static void put_field(tui_view *v, const char *s, size_t cap)
{
	if (s == NULL) {
		vputs(v, "?");
		return;
	}
	put_bounded(v, s, strlen(s), cap);
}

void tui_view_init(tui_view *v, tui_out_sink sink, int ansi, tui_input *editor)
{
	memset(v, 0, sizeof(*v));
	v->sink = sink;
	v->ansi = ansi;
	v->editor = editor;
}

void tui_view_redraw_editor(tui_view *v)
{
	if (v->editor == NULL || !v->ansi)
		return;
	if (v->editor_on_line)
		vputs(v, "\r\x1b[J");
	vputs(v, "> ");
	vput(v, tui_input_text(v->editor), tui_input_len(v->editor));
	v->editor_on_line = 1;
}

void tui_view_finalize_editor(tui_view *v)
{
	if (!v->ansi || !v->editor_on_line)
		return;
	vputs(v, "\r\n");
	v->editor_on_line = 0;
}

void tui_view_banner(tui_view *v, const char *runtime_name)
{
	char line[128];
	int n = snprintf(line, sizeof(line), "[tui] 盘古薄客户端（事件源: %s）\n", runtime_name);
	if (n > 0)
		vput(v, line, (size_t)n);
	v->editor_on_line = 0;
}

void tui_view_exit_note(tui_view *v, const char *reason)
{
	char line[160];
	int n = snprintf(line, sizeof(line), "[tui] 退出（%s），终端已恢复\n", reason);
	if (v->ansi && v->editor_on_line)
		vputs(v, "\r\n");
	v->editor_on_line = 0;
	if (n > 0)
		vput(v, line, (size_t)n);
}

void tui_view_busy_rejected(tui_view *v)
{
	event_suspend(v);
	vputs(v, "[tui] 忙：当前 run 进行中，提交被拒绝，输入已保留\n");
	event_resume_editor(v);
}

void tui_view_submit_error(tui_view *v)
{
	event_suspend(v);
	vputs(v, "[tui] 提交失败（Runtime 拒绝），输入已保留\n");
	event_resume_editor(v);
}

void tui_view_input_limit(tui_view *v)
{
	event_suspend(v);
	vputs(v, "[tui] 输入已达长度上限，继续输入被拒绝\n");
	event_resume_editor(v);
}

void tui_view_cancel_requested(tui_view *v)
{
	event_suspend(v);
	vputs(v, "[tui] 已请求取消，等待终态（期间继续显示实际到达的事件）\n");
	event_resume_editor(v);
}

void tui_view_poll_error(tui_view *v)
{
	event_suspend(v);
	vputs(v, "[tui] Runtime poll 错误；客户端未自行补造终态，退出\n");
}

void tui_view_protocol_violation(tui_view *v)
{
	event_suspend(v);
	vputs(v, "[tui] 协议违约：终态后仍收到事件，已丢弃该事件\n");
	event_resume_editor(v);
}

void tui_view_event(tui_view *v, const tui_event *ev)
{
	switch (ev->kind) {
	case TUI_EV_RUN_STARTED:
		event_suspend(v);
		vputs(v, "[run] started\n");
		break;
	case TUI_EV_TEXT_DELTA: {
		/* 先算后写：carry 拼接 + 本段尾部不完整序列留存，绝不向终端写出半个序列 */
		size_t off = 0;          /* 本段已并入 carry 的前缀长度 */
		int flush_carry = 0;     /* 本次是否要先冲掉 carry */
		size_t written = 0;      /* carry 之外还要写出的本段字节数 */

		if (v->carry_len > 0) {
			size_t cl0 = tui_utf8_seq_len((unsigned char)v->carry[0]);
			size_t need = (cl0 > v->carry_len) ? (cl0 - v->carry_len) : 0;
			if (need == 0) {
				flush_carry = 1; /* carry 已完整（异常路径）：直接冲掉 */
			} else if (ev->text_len >= need) {
				flush_carry = 1; /* carry + 本段前 need 字节合为完整序列 */
				off = need;
			} else {
				memcpy(v->carry + v->carry_len, ev->text, ev->text_len);
				v->carry_len += ev->text_len;
				off = ev->text_len; /* 本段全部并入 carry，本次无输出 */
			}
		}

		{
			size_t rem = ev->text_len - off;
			size_t tail = tui_utf8_incomplete_tail(ev->text + off, rem);

			written = rem - tail;
			if (flush_carry || written > 0 || off > 0) {
				event_suspend(v);
				if (flush_carry) {
					vput(v, v->carry, v->carry_len);
					v->carry_len = 0;
				}
				/* 写出 text[0..off)（补全 carry 的字节）与其后完整部分 */
				if (off + written > 0)
					vput(v, ev->text, off + written);
			}
			if (tail > 0) {
				memcpy(v->carry, ev->text + off + written, tail);
				v->carry_len = tail;
			}
			if (flush_carry || written > 0)
				event_resume_editor(v);
		}
		break;
	}
	case TUI_EV_TOOL_STARTED:
		event_suspend(v);
		vputs(v, "[tool] ");
		put_field(v, ev->tool_name, 48);
		vputs(v, " (#");
		put_field(v, ev->tool_call_id, 48);
		vputs(v, ") started\n");
		break;
	case TUI_EV_TOOL_FINISHED:
		event_suspend(v);
		vputs(v, "[tool] ");
		put_field(v, ev->tool_name, 48);
		vputs(v, " (#");
		put_field(v, ev->tool_call_id, 48);
		vputs(v, ev->tool_ok ? ") finished ok: " : ") finished error: ");
		put_bounded(v, ev->detail, ev->detail_len, TUI_VIEW_TOOL_SNIPPET_BYTES);
		vputs(v, "\n");
		break;
	case TUI_EV_RUN_FINISHED:
		/* run_finished 之前冲掉 carry（不完整序列按原样放出，明确可见） */
		if (v->carry_len > 0) {
			event_suspend(v);
			vput(v, v->carry, v->carry_len);
			v->carry_len = 0;
		}
		event_suspend(v);
		switch (ev->run_status) {
		case TUI_RUN_SUCCESS:
			vputs(v, "[run] finished: success\n");
			break;
		case TUI_RUN_ERROR:
			vputs(v, "[run] finished: error: ");
			put_bounded(v, ev->detail, ev->detail_len, TUI_VIEW_ERROR_DETAIL_BYTES);
			vputs(v, "\n");
			break;
		case TUI_RUN_CANCELLED:
			vputs(v, "[run] finished: cancelled\n");
			break;
		default:
			vputs(v, "[run] finished: unknown\n");
			break;
		}
		/* 唯一终态后编辑行重新可用：重绘提示符（含空缓冲） */
		if (v->editor != NULL && v->ansi)
			tui_view_redraw_editor(v);
		break;
	default:
		break;
	}
}
