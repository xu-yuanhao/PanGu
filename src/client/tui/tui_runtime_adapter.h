/* tui_runtime_adapter.h — 客户端内部 Runtime 适配接口 v0。
 *
 * 这是已集成薄 TUI 的客户端内部适配，公共契约见 src/contracts/client.h，不是稳定
 * 插件 ABI，也不是 src/contracts/client.h 的副本：最终公共头文件与桥接由
 * 主 Agent 提供，接入方式是填一张本结构的函数表（见交接报告）。
 *
 * 语义要点（v0）：
 *  - submit：完整 UTF-8 文本（指针+显式字节长度）；忙时拒绝；Runtime 需要
 *    保留输入时在返回前自行复制。
 *  - cancel：请求取消当前 run，可重复；不等于完成，不改变客户端忙闲状态。
 *  - poll：推进有限工作量并拉取一个事件；无事件返回 0，客户端据此等待
 *    输入或下次推进，不忙循环。函数签名由主 Agent 集成时固定。
 *  - 事件按序：run_started, text_delta, tool_started, tool_finished,
 *    run_finished；错误/取消通过唯一 run_finished 终态表达
 *    （success/error/cancelled）。
 *  - 事件内字符串只在本次 poll 调用期间有效；客户端保留必须复制到有界存储。
 *  - 单客户端、单线程、同一时间一个活动 run；每个 run 恰好一个终态，
 *    客户端不自行补造终态。
 */
#ifndef PANGU_TUI_RUNTIME_ADAPTER_H
#define PANGU_TUI_RUNTIME_ADAPTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	TUI_EV_RUN_STARTED = 1,
	TUI_EV_TEXT_DELTA = 2,
	TUI_EV_TOOL_STARTED = 3,
	TUI_EV_TOOL_FINISHED = 4,
	TUI_EV_RUN_FINISHED = 5
} tui_event_kind;

typedef enum {
	TUI_RUN_SUCCESS = 1,
	TUI_RUN_ERROR = 2,
	TUI_RUN_CANCELLED = 3
} tui_run_status;

typedef struct tui_event {
	tui_event_kind kind;
	/* text_delta：UTF-8 字节段（指针+字节长度，可能跨事件拆断序列） */
	const char *text;
	size_t text_len;
	/* 工具事件：稳定调用 ID 与工具名 */
	const char *tool_call_id;
	const char *tool_name;
	/* tool_finished：1 成功 / 0 失败；detail 为简短内容或错误摘要 */
	int tool_ok;
	const char *detail;
	size_t detail_len;
	/* run_finished 终态 */
	tui_run_status run_status;
} tui_event;

typedef enum {
	TUI_SUBMIT_OK = 0,
	TUI_SUBMIT_BUSY = 1, /* 忙时拒绝：输入保留在编辑器 */
	TUI_SUBMIT_ERROR = 2
} tui_submit_result;

typedef struct tui_runtime_ops {
	const char *name; /* 适配来源标识，如 "fake-runtime" */
	void *ctx;
	tui_submit_result (*submit)(void *ctx, const char *utf8, size_t byte_len);
	int (*cancel)(void *ctx); /* 1=已受理（或重复受理），0=当前无活动 run */
	int (*poll)(void *ctx, tui_event *out); /* 1=有事件 0=无事件 <0=错误 */
	void (*shutdown)(void *ctx);            /* 客户端退出前调用一次 */
} tui_runtime_ops;

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_RUNTIME_ADAPTER_H */
