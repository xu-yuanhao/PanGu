/* tui_term.h — 终端平台后端（本轮仅 Linux x86-64 termios 实现）。
 *
 * 职责边界：termios/系统调用集中在这里，渲染器不得散落平台调用。
 * 平台通过小型函数表选择（tui_term_platform），后续平台以新表注入，不改渲染器。
 *
 * 恢复语义：enter_raw 保存原始 termios，restore 精确还原并冲掉未读输入，
 * 防止退出后残留的 ^D/^C 字节被父 shell 重新解释（Pi terminal.ts 行为，按行为重写）。
 * SIGKILL 无法恢复，不宣称全部信号可恢复；可处理信号由主循环标志路径触发 restore。
 */
#ifndef PANGU_TUI_TERM_H
#define PANGU_TUI_TERM_H

#include <stddef.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tui_term_platform tui_term_platform;

typedef struct tui_term {
	int in_fd;
	int out_fd;
	struct termios saved;
	int saved_valid;
	int raw_active;
	int in_is_tty;
	int out_is_tty;
	unsigned short cols;
	unsigned short rows;
	const tui_term_platform *platform; /* NULL = linux termios 默认表 */
} tui_term;

struct tui_term_platform {
	const char *name;
	int (*enter_raw)(tui_term *t);
	void (*restore)(tui_term *t);
	int (*query_size)(tui_term *t); /* 成功更新 t->cols/rows，返回 0 */
};

/* 探测 fds 的 TTY 属性并取默认平台表；不修改终端状态。 */
void tui_term_init(tui_term *t, int in_fd, int out_fd);

/* 交互模式进入：stdin/stdout 都必须是 TTY，否则返回非 0（不改动终端）。 */
int tui_term_enter_interactive(tui_term *t);

/* 退出交互模式；未进入过 raw 时为无害空操作。可重复调用。 */
void tui_term_restore(tui_term *t);

/* 循环写满；返回 0 成功，非 0 失败（errno 保留）。 */
int tui_term_write(tui_term *t, const char *data, size_t len);

int tui_term_is_interactive(const tui_term *t);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_TERM_H */
