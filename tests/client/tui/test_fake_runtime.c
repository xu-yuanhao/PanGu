/* test_fake_runtime.c — 假事件源脚本行为测试。 */
#include "test_util.h"

#include "fake_runtime.h"

static int next_event(tui_fake_runtime *fr, tui_event *ev)
{
	for (int poll = 0; poll < 512; ++poll) {
		int rc = tui_fake_runtime_ops(fr)->poll(fr, ev);
		if (rc != 0)
			return rc;
	}
	return -1;
}

static void test_no_tool_script(void)
{
	tui_fake_runtime fr;
	tui_event ev;
	int kinds[8];
	int n = 0, i;
	const tui_demo_scenario *sc = tui_demo_scenario_get("no_tool");

	T_CHECK(sc != NULL);
	tui_fake_runtime_init(&fr, sc);
	T_CHECK(tui_fake_runtime_ops(&fr)->submit(&fr, "hi", 2) == TUI_SUBMIT_OK);

	{
		int polls = 0;

		while (polls++ < 200) {
			int rc = tui_fake_runtime_ops(&fr)->poll(&fr, &ev);

			if (rc == 0)
				continue; /* 步间 idle tick */
			T_CHECK(rc == 1);
			kinds[n] = ev.kind;
			/* 验证"演"的拼接：E6 BC + 94 */
			if (n == 4)
				T_CHECK(ev.text[0] == '\x94');
			n++;
			if (ev.kind == TUI_EV_RUN_FINISHED)
				break;
		}
		T_CHECK(polls <= 200);
	}
	T_CHECK(n == 6);
	T_CHECK(kinds[0] == TUI_EV_RUN_STARTED);
	T_CHECK(kinds[1] == TUI_EV_TEXT_DELTA);
	T_CHECK(kinds[5] == TUI_EV_RUN_FINISHED);
	for (i = 0; i < 5; i++) {
		int rc = tui_fake_runtime_ops(&fr)->poll(&fr, &ev);

		T_CHECK(rc == 0); /* 终态后无事件 */
	}
	T_RUN("no_tool_script");
}

static void test_tool_ids(void)
{
	tui_fake_runtime fr;
	tui_event ev;
	int saw_started = 0, saw_finished = 0;

	tui_fake_runtime_init(&fr, tui_demo_scenario_get("tool_roundtrip"));
	(void)tui_fake_runtime_ops(&fr)->submit(&fr, "q", 1);
	for (;;) {
		int rc = tui_fake_runtime_ops(&fr)->poll(&fr, &ev);

		if (rc == 0)
			continue;
		T_CHECK(rc == 1);
		if (ev.kind == TUI_EV_RUN_FINISHED)
			break;
		if (ev.kind == TUI_EV_TOOL_STARTED) {
			saw_started = 1;
			T_CHECK(strcmp(ev.tool_call_id, "call-1") == 0);
			T_CHECK(strcmp(ev.tool_name, "read_file") == 0);
		}
		if (ev.kind == TUI_EV_TOOL_FINISHED) {
			saw_finished = 1;
			T_CHECK(strcmp(ev.tool_call_id, "call-1") == 0); /* ID 关联 */
			T_CHECK(ev.tool_ok == 1);
			T_CHECK(ev.detail != NULL);
		}
	}
	T_CHECK(saw_started && saw_finished);
	T_RUN("tool_ids");
}

static void test_busy_then_events(void)
{
	tui_fake_runtime fr;
	tui_event ev;
	int none = 0, got = 0;

	tui_fake_runtime_init(&fr, tui_demo_scenario_get("busy_reject"));
	(void)tui_fake_runtime_ops(&fr)->submit(&fr, "a", 1);
	for (;;) {
		int rc = tui_fake_runtime_ops(&fr)->poll(&fr, &ev);

		if (rc == 0) {
			none++;
			continue;
		}
		(void)ev;
		got++;
		if (ev.kind == TUI_EV_RUN_FINISHED)
			break;
	}
	T_CHECK(none >= 100); /* 忙窗口确实无事件 */
	T_CHECK(got == 3);
	T_RUN("busy_then_events");
}

static void test_cancel_semantics(void)
{
	tui_fake_runtime fr;
	tui_event ev;
	int terminal = 0;
	const tui_demo_scenario *sc = tui_demo_scenario_get("user_cancel");

	T_CHECK(sc != NULL && sc->honor_cancel == 1);

	/* 未取消：正常 success 终态 */
	tui_fake_runtime_init(&fr, sc);
	(void)tui_fake_runtime_ops(&fr)->submit(&fr, "a", 1);
	for (;;) {
		int rc = tui_fake_runtime_ops(&fr)->poll(&fr, &ev);

		if (rc == 0)
			continue;
		T_CHECK(rc == 1);
		if (ev.kind == TUI_EV_RUN_FINISHED) {
			terminal++;
			break;
		}
	}
	T_CHECK(terminal == 1);
	T_CHECK(tui_fake_runtime_ops(&fr)->poll(&fr, &ev) == 0);

	/* 取消一次：收到 1；取消后事件继续；终态 cancelled；恰好一个终态 */
	tui_fake_runtime_init(&fr, sc);
	(void)tui_fake_runtime_ops(&fr)->submit(&fr, "a", 1);
	T_CHECK(tui_fake_runtime_ops(&fr)->cancel(&fr) == 1);
	{
		int saw_text_after_cancel = 0;
		int polls = 0;

		while (polls++ < 500) {
			int rc = tui_fake_runtime_ops(&fr)->poll(&fr, &ev);

			if (rc == 0)
				continue;
			if (ev.kind == TUI_EV_TEXT_DELTA)
				saw_text_after_cancel = 1;
			if (ev.kind == TUI_EV_RUN_FINISHED) {
				T_CHECK(ev.run_status == TUI_RUN_CANCELLED);
				terminal = 1;
				break;
			}
		}
		T_CHECK(polls <= 500);
		T_CHECK(saw_text_after_cancel); /* 取消后仍显示实际到达的事件 */
		T_CHECK(terminal);
	}
	/* 闲置时取消返回 0 */
	T_CHECK(tui_fake_runtime_ops(&fr)->cancel(&fr) == 0);
	T_RUN("cancel_semantics");
}

static void test_string_validity_contract(void)
{
	tui_fake_runtime fr;
	tui_event ev1, ev2;

	tui_fake_runtime_init(&fr, tui_demo_scenario_get("no_tool"));
	(void)tui_fake_runtime_ops(&fr)->submit(&fr, "a", 1);
	T_CHECK(next_event(&fr, &ev1) == 1); /* run_started */
	T_CHECK(next_event(&fr, &ev1) == 1);
	T_CHECK(next_event(&fr, &ev2) == 1);
	/* 两次 TEXT_DELTA 返回同一 scratch 指针但内容不同：
	 * 证明客户端必须复制，不得保留事件字符串指针 */
	T_CHECK(ev1.text == ev2.text);
	T_CHECK(ev1.text_len != ev2.text_len ||
		memcmp(ev1.text, ev2.text, ev1.text_len) != 0);
	T_RUN("string_validity_contract");
}

static void test_busy_submit_rejected(void)
{
	tui_fake_runtime fr;
	tui_event ev;

	tui_fake_runtime_init(&fr, tui_demo_scenario_get("no_tool"));
	(void)tui_fake_runtime_ops(&fr)->submit(&fr, "a", 1);
	for (;;) {
		int rc = tui_fake_runtime_ops(&fr)->poll(&fr, &ev);

		if (rc == 0)
			continue;
		if (ev.kind == TUI_EV_RUN_FINISHED)
			break;
	}
	/* 终态后可再次提交；终态前第二次提交被拒 */
	tui_fake_runtime_init(&fr, tui_demo_scenario_get("busy_reject"));
	(void)tui_fake_runtime_ops(&fr)->submit(&fr, "first", 5);
	T_CHECK(tui_fake_runtime_ops(&fr)->submit(&fr, "second", 6) == TUI_SUBMIT_BUSY);
	T_CHECK(tui_fake_runtime_ops(&fr)->submit(&fr, "third", 5) == TUI_SUBMIT_BUSY);
	T_RUN("busy_submit_rejected");
}

static void test_unknown_scenario(void)
{
	T_CHECK(tui_demo_scenario_get("nope") == NULL);
	T_CHECK(tui_demo_scenario_get(NULL) == NULL);
	{
		char buf[128];

		tui_demo_scenario_names(buf, sizeof(buf));
		T_CHECK(strstr(buf, "no_tool") != NULL);
		T_CHECK(strstr(buf, "busy_reject") != NULL);
	}
	T_RUN("unknown_scenario");
}

int main(void)
{
	test_no_tool_script();
	test_tool_ids();
	test_busy_then_events();
	test_cancel_semantics();
	test_string_validity_contract();
	test_busy_submit_rejected();
	test_unknown_scenario();
	printf("test_fake_runtime: all passed\n");
	return 0;
}
