/* test_tui_state.c — 状态机测试：忙拒绝/唯一终态/取消不提前闲置/不补造终态。 */
#include "test_util.h"

#include "tui_state.h"

typedef struct mock_runtime {
	/* poll 队列：依次返回 */
	tui_event q[32];
	int qn;
	int qi;
	/* 计数 */
	int submit_calls, cancel_calls, poll_calls, shutdown_calls;
	/* 行为配置 */
	tui_submit_result submit_result;
	int cancel_result;
	int poll_error; /* 1=poll 返回 -1 */
} mock_runtime;

static tui_submit_result m_submit(void *ctx, const char *utf8, size_t len)
{
	mock_runtime *m = ctx;

	(void)utf8;
	(void)len;
	m->submit_calls++;
	return m->submit_result;
}

static int m_cancel(void *ctx)
{
	mock_runtime *m = ctx;

	m->cancel_calls++;
	return m->cancel_result;
}

static int m_poll(void *ctx, tui_event *out)
{
	mock_runtime *m = ctx;

	m->poll_calls++;
	if (m->poll_error)
		return -1;
	if (m->qi >= m->qn)
		return 0;
	*out = m->q[m->qi++];
	return 1;
}

static int m_shutdown(void *ctx)
{
	((mock_runtime *)ctx)->shutdown_calls++;
	return 0;
}

static void m_add(mock_runtime *m, tui_event ev)
{
	m->q[m->qn++] = ev;
}

static tui_event mk(tui_event_kind kind)
{
	tui_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.kind = kind;
	if (kind == TUI_EV_TEXT_DELTA) {
		ev.text = "x";
		ev.text_len = 1;
	}
	if (kind == TUI_EV_TOOL_STARTED || kind == TUI_EV_TOOL_FINISHED) {
		ev.tool_call_id = "c1";
		ev.tool_name = "t";
	}
	return ev;
}

static tui_runtime_ops m_ops = {
	"mock", NULL, m_submit, m_cancel, m_poll, m_shutdown, NULL,
};

static void test_submit_and_busy(void)
{
	tstr out;
	tui_view v;
	tui_state s;
	mock_runtime m;

	memset(&m, 0, sizeof(m));
	tstr_clear(&out);
	tui_view_init(&v, tstr_sink(&out), 1, NULL);
	m_ops.ctx = &m;
	tui_state_init(&s, &m_ops, &v);

	T_CHECK(tui_state_submit(&s, "hi", 2) == TUI_SUBMIT_OK);
	T_CHECK(m.submit_calls == 1);
	T_CHECK(tui_state_running(&s) == 1);

	tstr_clear(&out);
	T_CHECK(tui_state_submit(&s, "second", 6) == TUI_SUBMIT_BUSY);
	T_CHECK(m.submit_calls == 1); /* 忙时不该打到 ops.submit */
	T_CHECK(tstr_contains(&out, "忙"));
	T_CHECK(tui_state_running(&s) == 1); /* 仍忙：不因拒绝改变状态 */

	m.cancel_result = 1;
	T_CHECK(tui_state_cancel(&s) == 1);
	T_CHECK(m.cancel_calls == 1);
	T_CHECK(tui_state_running(&s) == 1); /* 取消不等于完成 */
	T_RUN("submit_and_busy");
}

static void test_cancel_notice_once(void)
{
	tstr out;
	tui_view v;
	tui_state s;
	mock_runtime m;

	memset(&m, 0, sizeof(m));
	m.cancel_result = 1;
	tstr_clear(&out);
	tui_view_init(&v, tstr_sink(&out), 1, NULL);
	m_ops.ctx = &m;
	tui_state_init(&s, &m_ops, &v);

	(void)tui_state_submit(&s, "x", 1);
	T_CHECK(tui_state_cancel(&s) == 1);
	T_CHECK(tui_state_cancel(&s) == 1);
	T_CHECK(tui_state_cancel(&s) == 1);
	T_CHECK(m.cancel_calls == 3);
	T_CHECK(tstr_contains(&out, "已请求取消")); /* 提示只出现一次 */
	T_CHECK(strstr(out.buf, "已请求取消") == strstr(out.buf, "已请求取消"));
	{
		const char *first = strstr(out.buf, "已请求取消");
		T_CHECK(strstr(first + 1, "已请求取消") == NULL);
	}
	T_RUN("cancel_notice_once");
}

static void test_full_round_trip_unique_terminal(void)
{
	tstr out;
	tui_view v;
	tui_state s;
	mock_runtime m;

	memset(&m, 0, sizeof(m));
	tstr_clear(&out);
	tui_view_init(&v, tstr_sink(&out), 1, NULL);
	m_ops.ctx = &m;
	tui_state_init(&s, &m_ops, &v);

	m_add(&m, mk(TUI_EV_RUN_STARTED));
	m_add(&m, mk(TUI_EV_TEXT_DELTA));
	m_add(&m, mk(TUI_EV_TOOL_STARTED));
	m_add(&m, mk(TUI_EV_TOOL_FINISHED));
	m_add(&m, mk(TUI_EV_RUN_FINISHED));
	/* 队列里再塞一个终态：状态机必须在第一个终态后停止拉取 */
	m_add(&m, mk(TUI_EV_RUN_FINISHED));
	m_add(&m, mk(TUI_EV_TEXT_DELTA));

	(void)tui_state_submit(&s, "go", 2);
	tui_state_poll(&s);

	T_CHECK(m.poll_calls == 5); /* 16 上限内遇到终态即停 */
	T_CHECK(tui_state_running(&s) == 0);
	{
		int finished = 0;
		const char *p = out.buf;
		while ((p = strstr(p, "[run] finished")) != NULL) {
			finished++;
			p++;
		}
		T_CHECK(finished == 1); /* 唯一终态 */
	}
	T_CHECK(tstr_contains(&out, "[run] started"));
	T_CHECK(tstr_contains(&out, "[tool] t (#c1) started"));
	T_CHECK(tstr_contains(&out, "[tool] t (#c1) finished"));

	/* 闲置后再 poll：无操作 */
	tui_state_poll(&s);
	T_CHECK(m.poll_calls == 5);
	T_RUN("full_round_trip_unique_terminal");
}

static void test_no_fabricated_terminal(void)
{
	tstr out;
	tui_view v;
	tui_state s;
	mock_runtime m;

	memset(&m, 0, sizeof(m)); /* poll 永远返回 0：Runtime 挂起 */
	tstr_clear(&out);
	tui_view_init(&v, tstr_sink(&out), 1, NULL);
	m_ops.ctx = &m;
	tui_state_init(&s, &m_ops, &v);

	(void)tui_state_submit(&s, "go", 2);
	tui_state_poll(&s);
	tui_state_poll(&s);
	T_CHECK(tui_state_running(&s) == 1);
	T_CHECK(!tstr_contains(&out, "finished")); /* 客户端没有补造终态 */
	T_RUN("no_fabricated_terminal");
}

static void test_poll_error_fatal(void)
{
	tstr out;
	tui_view v;
	tui_state s;
	mock_runtime m;

	memset(&m, 0, sizeof(m));
	m.poll_error = 1;
	tstr_clear(&out);
	tui_view_init(&v, tstr_sink(&out), 1, NULL);
	m_ops.ctx = &m;
	tui_state_init(&s, &m_ops, &v);

	(void)tui_state_submit(&s, "go", 2);
	tui_state_poll(&s);
	T_CHECK(tui_state_fatal(&s) == 1);
	T_CHECK(tui_state_running(&s) == 0);
	T_CHECK(tstr_contains(&out, "poll 错误"));
	T_CHECK(!tstr_contains(&out, "finished: success")); /* 未补造成功终态 */
	T_RUN("poll_error_fatal");
}

static void test_cancelled_terminal_stops(void)
{
	tstr out;
	tui_view v;
	tui_state s;
	mock_runtime m;

	memset(&m, 0, sizeof(m));
	tstr_clear(&out);
	tui_view_init(&v, tstr_sink(&out), 1, NULL);
	m_ops.ctx = &m;
	tui_state_init(&s, &m_ops, &v);

	m_add(&m, mk(TUI_EV_RUN_STARTED));
	m_add(&m, mk(TUI_EV_TEXT_DELTA));
	{
		tui_event fin = mk(TUI_EV_RUN_FINISHED);

		fin.run_status = TUI_RUN_CANCELLED;
		m_add(&m, fin);
	}

	(void)tui_state_submit(&s, "go", 2);
	m.cancel_result = 1;
	(void)tui_state_cancel(&s); /* 终态前请求取消：期间事件照常显示 */
	tui_state_poll(&s);
	T_CHECK(tui_state_running(&s) == 0);
	T_CHECK(tstr_contains(&out, "第一段") || tstr_contains(&out, "x"));
	T_CHECK(tstr_contains(&out, "cancelled"));
	T_RUN("cancelled_terminal_stops");
}

static void test_attach_existing_run(void)
{
 tstr out;tui_view view;tui_state state;mock_runtime mock={0};
 tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),1,NULL);m_ops.ctx=&mock;tui_state_init(&state,&m_ops,&view);
 T_CHECK(tui_state_attach(&state)==0&&!tui_state_running(&state));
 m_add(&mock,mk(TUI_EV_RUN_STARTED));m_add(&mock,mk(TUI_EV_TEXT_DELTA));m_add(&mock,mk(TUI_EV_RUN_FINISHED));
 T_CHECK(tui_state_attach(&state)==1&&tui_state_running(&state));T_CHECK(mock.submit_calls==0&&tstr_contains(&out,"[run] started"));
 tui_state_poll(&state);T_CHECK(!tui_state_running(&state)&&!tui_state_fatal(&state));
 mock=(mock_runtime){0};tui_state_init(&state,&m_ops,&view);m_add(&mock,mk(TUI_EV_RUN_FINISHED));
 T_CHECK(tui_state_attach(&state)==-1&&tui_state_fatal(&state));
 T_RUN("attach_existing_run");
}

static unsigned history_calls,history_mode;
static int m_history(void *ctx,size_t index,tui_history_entry *out){(void)ctx;++history_calls;*out=(tui_history_entry){.kind=index?1u:0u,.text=index?"answer":"question",.text_len=index?6u:8u};if(history_mode==1)return -1;if(history_mode==2){out->text=0;return 1;}if(history_mode==3){out->kind=4;return 1;}if(history_mode==4)return 1;if(history_mode==5){out->kind=2;out->tool_name="t";out->tool_name_len=64;out->call_id="c";out->call_id_len=1;return 1;}return index<2?1:0;}
static void test_startup_history(void){for(history_mode=0;history_mode<6;++history_mode){tstr out;tui_view view;tui_state state;mock_runtime runtime={0};tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_runtime_ops ops=m_ops;ops.ctx=&runtime;ops.history=m_history;history_calls=0;tui_state_init(&state,&ops,&view);int result=tui_state_history(&state);T_CHECK(!runtime.poll_calls&&!runtime.submit_calls&&!runtime.cancel_calls&&!state.running);if(!history_mode){T_CHECK(result==2&&history_calls==3&&!state.fatal);T_CHECK(tstr_contains(&out,"[history] user: question")&&tstr_contains(&out,"[history] assistant: answer"));T_CHECK(tui_state_history(&state)==0&&history_calls==3);}else{T_CHECK(result<0&&state.fatal);T_CHECK(history_calls<=(history_mode==4?65u:1u));}T_CHECK(!tstr_contains(&out,"[run] started")&&!tstr_contains(&out,"finished: success"));}T_RUN("startup_history_no_execution_and_bounded_errors");}

int main(void)
{
    test_startup_history();
	test_attach_existing_run();
	test_submit_and_busy();
	test_cancel_notice_once();
	test_full_round_trip_unique_terminal();
	test_no_fabricated_terminal();
	test_poll_error_fatal();
	test_cancelled_terminal_stops();
	printf("test_tui_state: all passed\n");
	return 0;
}
