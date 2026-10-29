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
 *  - 事件内字符串从本次 poll 返回后有效，至下一次 poll 失效；客户端保留必须复制到有界存储。
 *  - 单客户端、单线程、同一时间一个活动 run；每个 run 恰好一个终态，
 *    客户端不自行补造终态。
 */
#ifndef PANGU_TUI_RUNTIME_ADAPTER_H
#define PANGU_TUI_RUNTIME_ADAPTER_H

#include <stddef.h>
#include "../../contracts/client.h"

#include "../../runtime/usage.h"
#include "../../runtime/task_graph.h"
#include "../../runtime/conversation_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	TUI_EV_RUN_STARTED = 1,
	TUI_EV_TEXT_DELTA = 2,
	TUI_EV_TOOL_STARTED = 3,
	TUI_EV_TOOL_FINISHED = 4,
	TUI_EV_RUN_FINISHED = 5,
    TUI_EV_TOOL_OUTPUT = 6
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
	/* tool_started：detail 为真实参数的 JSON；tool_finished：1 成功 / 0 失败，
       detail 为工具输出或错误摘要。 */
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

/* Read-only startup snapshot, separate from execution events. Borrowed slices
   expire on next call. history(index): 1 entry, 0 end, <0 error; no execution.
   Dynamic startup rows use explicit lengths and end at the first 0 result.
   Consumers maintain their own display/scroll budgets. */
typedef struct tui_history_entry {
    unsigned kind; /* 0 user, 1 assistant, 2 tool call, 3 tool result */
    const char *text;
    size_t text_len;
    const char *tool_name;
    size_t tool_name_len;
    const char *call_id;
    size_t call_id_len;
    int tool_ok;
} tui_history_entry;

/* Independent committed tool view. read does not advance execution. 0 returns
   owned {task,call,entry?}; 1 missing, other values error. call has real arguments,
   entry contains model[0].details and data.{status,arguments,diagnostics}.
   release is valid after subsequent reads; all strings are JSON-owned. */
struct json_object;
typedef struct {
    void *context;
    int (*read)(void *,const char *call_id,size_t,struct json_object **);
    void (*release)(struct json_object *);
} tui_tool_observer;

/* Optional durable Submission operations. Draft is Pi-style input/write JSON,
   with PanGu data/identity; whenBusy selects reject/steer/followUp. All calls
   are serialized and non-pumping. Results: 0 success, 2 missing, 3 busy,
   4 invalid, 5 allocation, 6 storage, 7 closed. submit returns a stable ID;
   status returns an owned record, including queued/placed/settled facts.
   list returns an owned ascending array, scoped to the active conversation;
   limit=1..128, after=0 starts, subsequent pages use the last record ID.
   withdraw result: 0 withdrawn, 1 placed, 2 settled, 3 missing. Owned values
   survive later calls and are released exactly once with release. */
typedef pg_inputs_ops tui_inputs_ops;
typedef pg_resource_ops tui_resource_ops;
typedef pg_compaction_ops tui_compaction_ops;
typedef pg_compaction_request tui_compaction_request;

typedef struct tui_runtime_ops {
	const char *name; /* 适配来源标识，如 "fake-runtime" */
	void *ctx;
	tui_submit_result (*submit)(void *ctx, const char *utf8, size_t byte_len);
	int (*cancel)(void *ctx); /* 1=已受理（或重复受理），0=当前无活动 run */
	int (*poll)(void *ctx, tui_event *out); /* 1=有事件 0=无事件 <0=错误 */
	int (*shutdown)(void *ctx); /* 0=released, 1=pending; repeat after waiting. */
    int (*history)(void *ctx, size_t index, tui_history_entry *out); /* optional */
} tui_runtime_ops;
/* Optional native extension lookup; NULL for other adapters or no binding. */
const tui_tool_observer *pg_tui_runtime_tool_observer(const tui_runtime_ops *);
/* Optional product binding; clients weak-link this lookup. An absent symbol
   or NULL means this adapter has no durable input operations. */
const tui_inputs_ops *pg_tui_runtime_inputs(const tui_runtime_ops *);
/* Optional product resource service; same binding lifetime as input ops.
   Clients may weak-link this lookup. */
const tui_resource_ops *pg_tui_runtime_resources(const tui_runtime_ops *);
/* Optional manual compaction; poll owns a persistent Task snapshot. */
const tui_compaction_ops *pg_tui_runtime_compaction(const tui_runtime_ops *);

/* Optional committed Live/usage snapshot; ownership follows pg_observation_ops.
   The binding expires when the bridge detaches. */
const pg_observation_ops *pg_tui_runtime_observation(const tui_runtime_ops *);
const pg_task_graph_ops *pg_tui_runtime_task_graph(const tui_runtime_ops *);
const pg_conversation_view_ops *pg_tui_runtime_conversation_view(const tui_runtime_ops *);
const pg_conversation_client_ops *pg_tui_runtime_conversations(const tui_runtime_ops *);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_RUNTIME_ADAPTER_H */
