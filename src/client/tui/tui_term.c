/* Linux termios/winsize adapters for the terminal service. */
#define _POSIX_C_SOURCE 200809L

#include "tui_term.h"

#include <errno.h>
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

const tui_term_platform tui_term_linux_platform = {
	"linux-termios",
	linux_enter_raw,
	linux_restore,
	linux_query_size,
};
