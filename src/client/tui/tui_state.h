/* tui_state.h — 客户端状态机：submit/cancel/poll 派发。
 *
 * 保证：
 *  - 同一时间一个活动 run；忙时 submit 返回 TUI_SUBMIT_BUSY，不改任何状态；
 *  - cancel 可重复，不改变忙闲，只有唯一 run_finished 终态才回到闲置；
 *  - 客户端绝不自行补造终态；poll 错误置 fatal 交由应用退出（如实报告）；
 *  - 每次 poll 派发最多 TUI_STATE_POLL_BATCH 条事件（推进有限工作量），
 *    无事件返回后由应用决定等待时机，不忙循环。
 */
#ifndef PANGU_TUI_STATE_H
#define PANGU_TUI_STATE_H

#include "tui_runtime_adapter.h"
#include "tui_view.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUI_STATE_POLL_BATCH 16u

typedef struct tui_state {
	const tui_runtime_ops *ops;
	tui_view *view;
	int running;              /* 1=等待终态 */
	int cancel_notice_shown;  /* 本次 run 的取消提示只显示一次 */
	int fatal;                /* poll 错误等不可恢复情况；客户端未补造终态 */
    int history_loaded;
} tui_state;

void tui_state_init(tui_state *s, const tui_runtime_ops *ops, tui_view *view);

/* 返回 TUI_SUBMIT_OK 时进入 running；BUSY/ERROR 不改变状态。 */
tui_submit_result tui_state_submit(tui_state *s, const char *utf8, size_t byte_len);

/* 请求取消；0=当前无活动 run。提示每次 run 只显示一次。 */
int tui_state_cancel(tui_state *s);

/* 推进有限工作量并渲染事件；终态后回到闲置。 */
/* Observe one initial run_started without submitting an input; 0 idle, 1 attached, -1 error. */
int tui_state_attach(tui_state *s);
/* Render optional bounded startup facts before attach; never submit/poll. */
int tui_state_history(tui_state *s);

void tui_state_poll(tui_state *s);

/* 查询 helper 为 tui_state.S 的汇编导出（与实现同源，不在头内留第二份）。 */
int tui_state_running(const tui_state *s);
int tui_state_fatal(const tui_state *s);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_STATE_H */
