/* tui_sink.h — 输出目的地函数表（视图与渲染模块共享）。
 *
 * write 返回 0 表示成功；非 0 由调用方按致命错误处理。ctx 指向宿主状态，
 * 所有权归宿主；data 只在调用期间有效。
 */
#ifndef PANGU_TUI_SINK_H
#define PANGU_TUI_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tui_out_sink {
	void *ctx;
	int (*write)(void *ctx, const char *data, size_t len);
} tui_out_sink;

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_SINK_H */
