/* Linux termios/winsize and external Unicode-library ABI adapters. */
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

/* libunistring implements full, context-sensitive lowercase (including dotted I
 * and final sigma). Input/search state remains in Assembly. The library handle
 * and any replacement allocation are released before return; no global lease. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unicase.h>
#include "tui_utf8.h"
size_t tui_unicode_lower(const char *text, size_t len, char *out, size_t cap)
{
 void *handle = dlopen("libunistring.so.5", RTLD_NOW | RTLD_LOCAL);
 if (!handle) return SIZE_MAX;
 uint8_t *(*lower)(const uint8_t *, size_t, const char *, uninorm_t, uint8_t *, size_t *);
 void *symbol = dlsym(handle, "u8_tolower");
 _Static_assert(sizeof(lower) == sizeof(symbol), "POSIX function pointer ABI");
 memcpy(&lower, &symbol, sizeof(lower));
 if (!lower) { dlclose(handle); return SIZE_MAX; }
 size_t size = cap;
 uint8_t *result = lower((const uint8_t *)text, len, NULL, NULL, (uint8_t *)out, &size);
 if (!result || size > cap) size = SIZE_MAX;
 else if (result != (uint8_t *)out) memcpy(out, result, size);
 if (result != (uint8_t *)out) free(result);
 dlclose(handle);
 return size;
}
