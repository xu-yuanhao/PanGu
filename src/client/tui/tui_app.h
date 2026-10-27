/* tui_app.h — 客户端组合与主循环。
 *
 * 单线程事件循环：
 *  - poll() 等待 stdin / 唤醒管道；闲置时无限阻塞（不忙循环），
 *    run 活动期间以 poll_tick_ms 定时推进 Runtime（v0 接口无超时提示，
 *    定时推进为临时方案，记录于交接报告）。
 *  - 信号处理只做异步信号安全操作：置 sig_atomic_t 标志 + 自管道写一个字节，
 *    清理全部交回主循环（termios 恢复在主循环退出路径执行）。
 *  - Ctrl-C 字节：忙时请求取消并继续等待终态；闲置时退出。
 *    EOF（^D 或 stdin 读到 0）：退出。外部 SIGINT/SIGTERM/SIGHUP：优雅退出。
 *    SIGKILL 无法恢复，不宣称全部信号可恢复。
 */
#ifndef PANGU_TUI_APP_H
#define PANGU_TUI_APP_H

#include "tui_runtime_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	TUI_EXIT_EOF = 0,        /* ^D 或 stdin EOF */
	TUI_EXIT_CTRL_C_IDLE,    /* 闲置时 ^C */
	TUI_EXIT_SIGNAL,         /* 外部 SIGINT/SIGTERM/SIGHUP */
	TUI_EXIT_POLL_ERROR,     /* Runtime poll 错误（未补造终态） */
	TUI_EXIT_COMMAND        /* explicit /quit, through normal shutdown */
} tui_exit_reason;

typedef struct tui_app_config {
	int in_fd;
	int out_fd;
	int ansi;                    /* 1=交互 ANSI（要求两端都是 TTY）；0=降级纯文本 */
	const tui_runtime_ops *ops;
	unsigned poll_tick_ms;       /* run 活动期间的推进间隔；0 取默认 20ms */
} tui_app_config;

/* 运行到退出条件为止；返回退出原因；<0 为初始化失败（终端未改动）。 */
int tui_app_run(const tui_app_config *cfg);

const char *tui_exit_reason_name(int reason);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_APP_H */
