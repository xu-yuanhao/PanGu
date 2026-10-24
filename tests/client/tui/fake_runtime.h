/* fake_runtime.h — 假 Runtime 事件源（客户端测试替身，临时适配声明）。
 *
 * 只按脚本输出事件：不调用网络、Shell，不读写用户项目；不能承载另一个
 * Agent Loop。事件字符串全部写入单个 scratch 缓冲并在下次 poll 时覆盖，
 * 以真实暴露"事件字符串只在当前处理调用期间有效"的契约。
 *
 * 这是 tests/ 下的临时适配实现；最终由主 Agent 用 src/contracts/client.h
 * 桥接真实 Runtime（见交接报告）。
 */
#ifndef PANGU_TUI_FAKE_RUNTIME_H
#define PANGU_TUI_FAKE_RUNTIME_H

#include "tui_runtime_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tui_demo_step {
	unsigned idle_polls;    /* 本步之前返回无事件的 poll 次数 */
	tui_event_kind kind;
	const char *text;       /* TEXT_DELTA 的 UTF-8 字节（可为拆断序列） */
	const char *tool_call_id;
	const char *tool_name;
	int tool_ok;
	const char *detail;     /* TOOL_FINISHED / RUN_FINISHED(error) 内容 */
	tui_run_status run_status;
} tui_demo_step;

typedef struct tui_demo_scenario {
	const char *name;
	int honor_cancel; /* 1=收到取消后终态改为 cancelled；0=忽略取消照常完成 */
	size_t step_count;
	const tui_demo_step *steps;
} tui_demo_scenario;

/* 按名称取场景；未知返回 NULL。可用场景：no_tool, tool_roundtrip,
 * model_error, user_cancel, busy_reject。 */
const tui_demo_scenario *tui_demo_scenario_get(const char *name);

/* 把场景名列表写入 buf（逗号分隔）。 */
void tui_demo_scenario_names(char *buf, size_t cap);

typedef struct tui_fake_runtime {
	const tui_demo_scenario *sc;
	size_t step;
	unsigned pending_idle;
	int run_active;   /* submit 受理后到终态之前 */
	int cancelled;
	unsigned long poll_calls;
	unsigned long cancel_calls;
	unsigned long submit_calls;
} tui_fake_runtime;

void tui_fake_runtime_init(tui_fake_runtime *fr, const tui_demo_scenario *sc);
const tui_runtime_ops *tui_fake_runtime_ops(tui_fake_runtime *fr);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_FAKE_RUNTIME_H */
