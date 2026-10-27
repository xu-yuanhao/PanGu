#include "fake_runtime.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * 场景脚本。文本包含刻意拆断的 UTF-8 序列（"演" = E6 BC 94 在 no_tool 中
 * 跨两个 text_delta），用于验证客户端拼接。
 * ------------------------------------------------------------------------- */

static const tui_demo_step steps_no_tool[] = {
	{ 0, TUI_EV_RUN_STARTED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 2, TUI_EV_TEXT_DELTA, "你好，", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 2, TUI_EV_TEXT_DELTA, "这是盘古薄客户端的", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 2, TUI_EV_TEXT_DELTA, "\xe6\xbc", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS }, /* "演" 前 2 字节 */
	{ 2, TUI_EV_TEXT_DELTA, "\x94逐段输出演示。\n", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 2, TUI_EV_RUN_FINISHED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
};

static const tui_demo_step steps_tool[] = {
	{ 0, TUI_EV_RUN_STARTED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 2, TUI_EV_TEXT_DELTA, "我来读取示例文件。\n", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 1, TUI_EV_TOOL_STARTED, NULL, "call-1", "read_file", 0, NULL, TUI_RUN_SUCCESS },
	{ 6, TUI_EV_TOOL_FINISHED, NULL, "call-1", "read_file", 1, "示例文件内容：盘古演示数据 20 字节", TUI_RUN_SUCCESS },
	{ 2, TUI_EV_TEXT_DELTA, "读取完成，一次工具往返结束。", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 2, TUI_EV_RUN_FINISHED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
};

static const tui_demo_step steps_error[] = {
	{ 0, TUI_EV_RUN_STARTED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 2, TUI_EV_TEXT_DELTA, "开始生成…", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 8, TUI_EV_RUN_FINISHED, NULL, NULL, NULL, 0, "provider timeout after 30s（假错误演示）", TUI_RUN_ERROR },
};

static const tui_demo_step steps_cancel[] = {
	{ 0, TUI_EV_RUN_STARTED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 1, TUI_EV_TEXT_DELTA, "长任务已开始，", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 200, TUI_EV_TEXT_DELTA, "…", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS }, /* 留出取消窗口 */
	{ 1, TUI_EV_TEXT_DELTA, "取消前的最后输出。", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 1, TUI_EV_RUN_FINISHED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS }, /* honor_cancel 时改 cancelled */
};

static const tui_demo_step steps_busy[] = {
	{ 0, TUI_EV_RUN_STARTED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 100, TUI_EV_TEXT_DELTA, "终于完成了。", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS }, /* 忙窗口 */
	{ 1, TUI_EV_RUN_FINISHED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
};

static const tui_demo_step steps_poll_error[] = {
	{ 0, TUI_EV_RUN_STARTED, NULL, NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
	{ 1, TUI_EV_TEXT_DELTA, "出错前的一段输出。", NULL, NULL, 0, NULL, TUI_RUN_SUCCESS },
};

static const tui_demo_scenario scenarios[] = {
	{ "no_tool", 0, 0, 6, steps_no_tool },
	{ "tool_roundtrip", 0, 0, 6, steps_tool },
	{ "model_error", 0, 0, 3, steps_error },
	{ "user_cancel", 1, 0, 5, steps_cancel },
	{ "busy_reject", 0, 0, 3, steps_busy },
	{ "poll_error", 0, 1, 2, steps_poll_error },
};

const tui_demo_scenario *tui_demo_scenario_get(const char *name)
{
	size_t i;

	if (name == NULL)
		return NULL;
	for (i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++) {
		if (strcmp(scenarios[i].name, name) == 0)
			return &scenarios[i];
	}
	return NULL;
}

void tui_demo_scenario_names(char *buf, size_t cap)
{
	size_t i;
	size_t off = 0;

	if (cap == 0)
		return;
	buf[0] = '\0';
	for (i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++) {
		int n = snprintf(buf + off, cap - off, "%s%s", i ? "," : "", scenarios[i].name);
		if (n < 0 || (size_t)n >= cap - off)
			break;
		off += (size_t)n;
	}
}

/* ------------------------------------------------------------------------- */

/* 单一 scratch：事件字符串只在当前 poll 调用期间有效（下次 emit 覆盖） */
static char g_scratch[256];

static const char *scratch_set(const char *s)
{
	size_t n = strlen(s);

	if (n >= sizeof(g_scratch))
		n = sizeof(g_scratch) - 1;
	memcpy(g_scratch, s, n);
	g_scratch[n] = '\0';
	return g_scratch;
}

static tui_submit_result fake_submit(void *ctx, const char *utf8, size_t byte_len)
{
	tui_fake_runtime *fr = ctx;

	(void)utf8;
	(void)byte_len; /* Runtime 需要保留时在此复制；假源不保留 */
	fr->submit_calls++;
	if (fr->run_active)
		return TUI_SUBMIT_BUSY;
	fr->run_active = 1;
	fr->cancelled = 0;
	fr->step = 0;
	fr->pending_idle = fr->sc->steps[0].idle_polls;
	return TUI_SUBMIT_OK;
}

static int fake_cancel(void *ctx)
{
	tui_fake_runtime *fr = ctx;

	fr->cancel_calls++;
	if (!fr->run_active)
		return 0;
	fr->cancelled = 1;
	return 1;
}

static int fake_poll(void *ctx, tui_event *out)
{
	tui_fake_runtime *fr = ctx;
	const tui_demo_step *st;

	fr->poll_calls++;
	if (!fr->run_active || fr->step >= fr->sc->step_count) {
		if (fr->run_active && fr->sc->poll_fails)
			return -1; /* 脚本播完且仍在运行：Runtime 错误路径 */
		return 0;
	}
	if (fr->pending_idle > 0) {
		fr->pending_idle--;
		return 0;
	}

	st = &fr->sc->steps[fr->step];
	fr->step++;
	if (fr->step < fr->sc->step_count)
		fr->pending_idle = fr->sc->steps[fr->step].idle_polls;

	memset(out, 0, sizeof(*out));
	out->kind = st->kind;
	if (st->text != NULL)
		out->text = scratch_set(st->text);
	out->text_len = st->text ? strlen(st->text) : 0;
	out->tool_call_id = st->tool_call_id;
	out->tool_name = st->tool_name;
	out->tool_ok = st->tool_ok;
	if (st->detail != NULL)
		out->detail = scratch_set(st->detail);
	out->detail_len = st->detail ? strlen(st->detail) : 0;
	out->run_status = st->run_status;

	/* 终态：honor_cancel 且已请求取消 → cancelled；run 回到空闲 */
	if (st->kind == TUI_EV_RUN_FINISHED) {
		if (fr->sc->honor_cancel && fr->cancelled)
			out->run_status = TUI_RUN_CANCELLED;
		fr->run_active = 0;
	}
	return 1;
}

static int fake_shutdown(void *ctx)
{
	(void)ctx;
	return 0;
}

void tui_fake_runtime_init(tui_fake_runtime *fr, const tui_demo_scenario *sc)
{
	memset(fr, 0, sizeof(*fr));
	fr->sc = sc;
}

static tui_runtime_ops fake_ops = {
	"fake-runtime",
	NULL,
	fake_submit,
	fake_cancel,
	fake_poll,
	fake_shutdown,
    NULL,
};

const tui_runtime_ops *tui_fake_runtime_ops(tui_fake_runtime *fr)
{
	fake_ops.ctx = fr;
	return &fake_ops;
}
