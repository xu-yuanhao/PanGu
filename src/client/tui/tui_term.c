#define _POSIX_C_SOURCE 200809L

#include "tui_term.h"

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int linux_enter_raw(tui_term *t)
{
	struct termios raw;

	if (t->saved_valid)
		return 0;
	if (tcgetattr(t->in_fd, &t->saved) != 0)
		return -1;
	t->saved_valid = 1;

	raw = t->saved;
	/* 关闭：输入回显/行缓冲/信号生成/扩展处理；输入转换仅保留最基本位。
	 * 输出关 OPOST：视图层显式写 "\r\n"，行为可预期。 */
	raw.c_iflag &= ~(unsigned)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	raw.c_oflag &= ~(unsigned)OPOST;
	raw.c_lflag &= ~(unsigned)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	raw.c_cflag &= ~(unsigned)(CSIZE | PARENB);
	raw.c_cflag |= CS8;
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(t->in_fd, TCSANOW, &raw) != 0)
		return -1;
	t->raw_active = 1;
	return 0;
}

static void linux_restore(tui_term *t)
{
	if (!t->saved_valid)
		return;
	/* 先冲掉未读输入再还原：残留的 ^D/^C 不得漏给父 shell */
	(void)tcflush(t->in_fd, TCIFLUSH);
	(void)tcsetattr(t->in_fd, TCSANOW, &t->saved);
	t->saved_valid = 0;
	t->raw_active = 0;
}

static int linux_query_size(tui_term *t)
{
	struct winsize ws;

	if (ioctl(t->out_fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		t->cols = ws.ws_col;
		t->rows = ws.ws_row;
		return 0;
	}
	/* 非 TTY 或查询失败：回退缺省，明确降级而不是无限等待 */
	t->cols = 80;
	t->rows = 24;
	return -1;
}

static const tui_term_platform linux_platform = {
	.name = "linux-termios",
	.enter_raw = linux_enter_raw,
	.restore = linux_restore,
	.query_size = linux_query_size,
};

void tui_term_init(tui_term *t, int in_fd, int out_fd)
{
	memset(t, 0, sizeof(*t));
	t->in_fd = in_fd;
	t->out_fd = out_fd;
	t->in_is_tty = isatty(in_fd) == 1;
	t->out_is_tty = isatty(out_fd) == 1;
	t->platform = &linux_platform;
	t->cols = 80;
	t->rows = 24;
	(void)t->platform->query_size(t);
}

int tui_term_enter_interactive(tui_term *t)
{
	if (!t->in_is_tty || !t->out_is_tty)
		return -1;
	if (t->platform->enter_raw(t) != 0)
		return -1;
	(void)t->platform->query_size(t);
	return 0;
}

void tui_term_restore(tui_term *t)
{
	if (t->raw_active || t->saved_valid)
		t->platform->restore(t);
}

int tui_term_write(tui_term *t, const char *data, size_t len)
{
	size_t off = 0;

	while (off < len) {
		ssize_t n = write(t->out_fd, data + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			return -1;
		off += (size_t)n;
	}
	return 0;
}

int tui_term_is_interactive(const tui_term *t)
{
	return t->raw_active;
}
