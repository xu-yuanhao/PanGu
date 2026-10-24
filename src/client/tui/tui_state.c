#include "tui_state.h"

#include <string.h>

void tui_state_init(tui_state *s, const tui_runtime_ops *ops, tui_view *view)
{
	memset(s, 0, sizeof(*s));
	s->ops = ops;
	s->view = view;
}

tui_submit_result tui_state_submit(tui_state *s, const char *utf8, size_t byte_len)
{
	tui_submit_result rc;

	if (s->running) {
		tui_view_busy_rejected(s->view);
		return TUI_SUBMIT_BUSY;
	}
	rc = s->ops->submit(s->ops->ctx, utf8, byte_len);
	if (rc == TUI_SUBMIT_OK) {
		s->running = 1;
		s->cancel_notice_shown = 0;
	} else if (rc == TUI_SUBMIT_ERROR) {
		tui_view_submit_error(s->view);
	}
	return rc;
}

int tui_state_cancel(tui_state *s)
{
	int rc;

	if (!s->running)
		return 0;
	rc = s->ops->cancel(s->ops->ctx);
	if (rc && !s->cancel_notice_shown) {
		tui_view_cancel_requested(s->view);
		s->cancel_notice_shown = 1;
	}
	return rc;
}

void tui_state_poll(tui_state *s)
{
	unsigned i;

	if (!s->running || s->fatal)
		return;

	for (i = 0; i < TUI_STATE_POLL_BATCH && s->running; i++) {
		tui_event ev;
		int rc = s->ops->poll(s->ops->ctx, &ev);

		if (rc < 0) {
			/* 拿不到终态也不补造：如实报错并交由应用退出 */
			s->fatal = 1;
			s->running = 0;
			tui_view_poll_error(s->view);
			return;
		}
		if (rc == 0)
			return; /* 本轮无事件：交回主循环等待，不忙循环 */
		tui_view_event(s->view, &ev);
		if (ev.kind == TUI_EV_RUN_FINISHED)
			s->running = 0; /* 唯一终态：本批立即停止拉取 */
	}
}
