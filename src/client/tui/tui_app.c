#define _POSIX_C_SOURCE 200809L

#include "tui_app.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "tui_input.h"
#include "tui_state.h"
#include "tui_term.h"
#include "tui_view.h"

#define TUI_DEFAULT_TICK_MS 20u

/* 信号处理路径的全局最小状态：仅 sig_atomic_t 标志与自管道写，
 * write() 为异步信号安全函数；任何复杂清理都不在这里做。 */
static volatile sig_atomic_t g_exit_signum; /* 0=无，否则收到的信号编号 */
static volatile sig_atomic_t g_winch;
static int g_wake_wfd = -1;

static void wake_main(void)
{
	if (g_wake_wfd >= 0) {
		ssize_t r = write(g_wake_wfd, "x", 1);
		(void)r;
	}
}

static void on_exit_signal(int sig)
{
	g_exit_signum = sig;
	wake_main();
}

static void on_winch(int sig)
{
	(void)sig;
	g_winch = 1;
	wake_main();
}

static int install_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_exit_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0; /* 不用 SA_RESTART：让阻塞调用返回交回主循环 */
	if (sigaction(SIGINT, &sa, NULL) != 0)
		return -1;
	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return -1;
	if (sigaction(SIGHUP, &sa, NULL) != 0)
		return -1;

	sa.sa_handler = on_winch;
	if (sigaction(SIGWINCH, &sa, NULL) != 0)
		return -1;
	return 0;
}

static int term_write_cb(void *ctx, const char *data, size_t len)
{
	return tui_term_write((tui_term *)ctx, data, len);
}

static void drain_wake(int rfd)
{
	char tmp[64];

	for (;;) {
		ssize_t n = read(rfd, tmp, sizeof(tmp));
		if (n <= 0) {
			if (n < 0 && (errno == EINTR || errno == EAGAIN))
				continue;
			break;
		}
	}
}

int tui_app_run(const tui_app_config *cfg)
{
	tui_term term;
	tui_input in;
	tui_view view;
	tui_state state;
	tui_out_sink sink;
	int wake[2] = { -1, -1 };
	int interactive = cfg->ansi;
	unsigned tick = cfg->poll_tick_ms ? cfg->poll_tick_ms : TUI_DEFAULT_TICK_MS;
	int reason = (int)TUI_EXIT_EOF;

	tui_term_init(&term, cfg->in_fd, cfg->out_fd);
	tui_input_init(&in);
	sink.ctx = &term;
	sink.write = term_write_cb;
	tui_view_init(&view, sink, interactive, interactive ? &in : NULL);
	tui_state_init(&state, cfg->ops, &view);

	if (interactive) {
		if (tui_term_enter_interactive(&term) != 0)
			return -1; /* 进入失败时未改动终端 */
	}
	if (pipe(wake) != 0)
		goto failure;
	{
		int i;
		for (i = 0; i < 2; i++) {
			int fl = fcntl(wake[i], F_GETFL, 0);
			if (fl < 0 || fcntl(wake[i], F_SETFL, fl | O_NONBLOCK) != 0)
				goto failure;
		}
	}
	if (install_handlers() != 0)
		goto failure;
	g_wake_wfd = wake[1];

	tui_view_banner(&view, cfg->ops->name ? cfg->ops->name : "runtime");
	if (interactive)
		tui_view_redraw_editor(&view);

	for (;;) {
		struct pollfd pfds[2];
		int rc;

		if (g_exit_signum) {
			reason = (int)TUI_EXIT_SIGNAL;
			break;
		}
		if (g_winch) {
			g_winch = 0;
			(void)term.platform->query_size(&term);
			if (interactive)
				tui_view_redraw_editor(&view);
		}

		pfds[0].fd = cfg->in_fd;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;
		pfds[1].fd = wake[0];
		pfds[1].events = POLLIN;
		pfds[1].revents = 0;

		/* 闲置：无限阻塞等输入/信号，不忙循环；活动：定时推进 Runtime */
		rc = poll(pfds, 2, state.running ? (int)tick : -1);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			reason = (int)TUI_EXIT_POLL_ERROR;
			break;
		}
		if (pfds[1].revents & POLLIN)
			drain_wake(wake[0]);

		if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
			unsigned char buf[256];
			ssize_t n = read(cfg->in_fd, buf, sizeof(buf));

			if (n == 0) {
				reason = (int)TUI_EXIT_EOF;
				break;
			}
			if (n < 0) {
				if (errno == EINTR || errno == EAGAIN)
					goto pump;
				reason = (int)TUI_EXIT_POLL_ERROR;
				break;
			}
			{
				ssize_t k;
				for (k = 0; k < n; k++) {
					tui_key_kind key = tui_input_feed(&in, buf[k]);

					switch (key) {
					case TUI_KEY_ENTER:
						if (!tui_input_empty(&in)) {
							tui_submit_result sr = tui_state_submit(&state, tui_input_text(&in), tui_input_len(&in));

							if (sr == TUI_SUBMIT_OK) {
								tui_view_finalize_editor(&view);
								tui_input_reset(&in);
							}
							/* BUSY/ERROR：内容保留，提示与编辑行重绘已由 view/state 完成 */
						}
						break;
					case TUI_KEY_CTRL_C:
						if (tui_state_running(&state)) {
							tui_state_cancel(&state);
						} else {
							reason = (int)TUI_EXIT_CTRL_C_IDLE;
							goto done;
						}
						break;
					case TUI_KEY_CTRL_D:
						reason = (int)TUI_EXIT_EOF;
						goto done;
					case TUI_KEY_OVERFLOW:
						tui_view_input_limit(&view);
						break;
					case TUI_KEY_CHAR:
					case TUI_KEY_BACKSPACE:
						tui_view_redraw_editor(&view);
						break;
					case TUI_KEY_NONE:
					case TUI_KEY_IGNORED:
					default:
						break;
					}
				}
			}
		}
pump:
		tui_state_poll(&state);
		if (tui_state_fatal(&state)) {
			reason = (int)TUI_EXIT_POLL_ERROR;
			break;
		}
	}
done:
	g_wake_wfd = -1;
	tui_view_exit_note(&view, tui_exit_reason_name(reason));
	tui_term_restore(&term);
	if (wake[0] >= 0)
		close(wake[0]);
	if (wake[1] >= 0)
		close(wake[1]);
	return reason;

failure:
	g_wake_wfd = -1;
	tui_term_restore(&term);
	if (wake[0] >= 0)
		close(wake[0]);
	if (wake[1] >= 0)
		close(wake[1]);
	return -1;
}

const char *tui_exit_reason_name(int reason)
{
	switch (reason) {
	case (int)TUI_EXIT_EOF:
		return "EOF";
	case (int)TUI_EXIT_CTRL_C_IDLE:
		return "Ctrl-C 闲置退出";
	case (int)TUI_EXIT_SIGNAL:
		return "收到终止信号";
	case (int)TUI_EXIT_POLL_ERROR:
		return "Runtime 错误";
	default:
		return "未知";
	}
}
