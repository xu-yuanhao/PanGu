#ifndef PANGU_DURABLE_TOOL_H
#define PANGU_DURABLE_TOOL_H
#include "task_runner.h"
#include "tools.h"
#include "output_window.h"
typedef struct {
 void *context;
 /* Task-owned operation lease: 0 acquired, 1 busy (poll again), 2 unavailable.
    On success all service callbacks obey pg_tools, with async triple complete.
    Failed acquisition owns no resources. */
 int (*acquire)(void *,uint64_t task_id,pg_tools *);
 /* Return the service lease. On cancellation this transfers responsibility for
    pending native cleanup to the host, which drains it before Session/backend
    shutdown. Copy the descriptor before returning; its storage then expires. */
 void (*release)(void *,uint64_t task_id,const pg_tools *);
 /* mint receives runner.host->context, matching Record Tx read_context. */
 int (*mint)(void *,uint64_t *);
 void (*progress)(void *,uint64_t task_id,pg_slice);
 /* Optional final metadata snapshot, before service close/release. call borrows
    the committed toolCall. Return 0 with an owned object (or NULL): optional
    output is an optional canonical Content array, details/structuredOutput are JSON, diagnostics is an array, and usage
    is an optional Pi-shaped counter object accounted with the result transaction. Failure stops settlement without
    repeating the effect. Metadata stays owned by this task through commit. Diagnostics are rendered
    as a separate model text block; structuredOutput is stored in Entry.data. */
 int (*snapshot)(void *,uint64_t task_id,const struct json_object *call,struct json_object **);
 /* Resolve from the invocation's pinned declaration. Borrowed through final
    result validation. Contract reports are nonfatal extension diagnostics. */
 const char *(*structured_schema)(void *,uint64_t,const struct json_object *);
 void (*report_contract)(void *,uint64_t,const char *);
} pg_durable_tool_host;
/* poll returns 0 with owned final {call,result?}, 1 pending, negative error.
   state is closed before the executor lease; validate returns 1/0/-1 for
   valid arguments/invalid arguments/unavailable tool. validate receives the owning
   task ID, including before/after argument rewriting. */
typedef struct {void *context;int (*poll)(void *,uint64_t,unsigned,const struct json_object *,void **,struct json_object **);void (*close)(void *,void *);int (*validate)(void *,uint64_t,const struct json_object *);} pg_durable_tool_hooks;
typedef struct {
 const pg_durable_tool_host *host;
 pg_task_definition definition;
 pg_task_phase phases[2];
 const pg_durable_tool_hooks *hooks;
} pg_durable_tool;
/* Borrowed host lives through all tasks and native cleanup. Resolve this
   definition as pangu.tool. Input {kind:"model",assistant,callId,offeredTools?} references
   a committed pangu.assistant Entry's model[0].content toolCall. call stores
   intent before execution; a saved offeredTools array rejects names outside
   that request before lookup/hooks/effects. Recovered execute reruns only when both policies
   say safe. Result Entry and Task outcome commit atomically through Session.
   Tool output and model-facing identity use PanGu's own Entry namespace. */
int pg_durable_tool_init(pg_durable_tool *,const pg_durable_tool_host *);
/* Borrowed current call, valid through this Tool phase cleanup. NULL before open. */
const struct json_object *pg_durable_tool_call(const pg_task_runner *);
/* Effects have started and the invocation has not entered settlement. */
int pg_durable_tool_executing(const pg_task_runner *);
/* Host pins limits during acquire, before effects; buffer is owned by phase. */
int pg_durable_tool_output_limits(pg_task_runner *,const pg_tool_options *);
typedef struct {size_t max_bytes,max_lines;double min_interval_ms;uint64_t bytes_per_second;} pg_tool_output_window;
/* Executing call only. Complete decoded UTF-8 chunk is borrowed for the call;
   skipped describes omitted output directly before it, permitted for tail only.
   Retained snapshot transfers owned text; free() after use. It includes all
   accepted output, even before its next durable progress commit. */
int pg_durable_tool_output(pg_task_runner *,pg_slice,const pg_output_skip *);
int pg_durable_tool_retained_output(pg_task_runner *,pg_output_snapshot *);
/* 0 tail window copied, 1 head retention (zeroed output), other task error. */
int pg_durable_tool_output_window(pg_task_runner *,pg_tool_output_window *);
/* Internal consumer receipt: a confirmed synchronous metadata commit buys the
   same output pacing pause, measured on its native Op payload. No new write. */
void pg_durable_tool_progress_recorded(pg_task_runner *,size_t);
/* Binding borrows callback table through all phase and hook cleanup. */
void pg_durable_tool_bind_hooks(pg_durable_tool *,const pg_durable_tool_hooks *);
/* Serialized read, no scheduler advancement. 0 transfers owned
   {task,call,entry?}, 1 missing; other values are read/argument errors. The
   conversation and call ID select the latest matching model or nested Tool task.
   A nested observation exposes its input call and small Task receipt; its
   caller-owned result is read through nested_call while the caller is live. */
int pg_durable_tool_observe(const pg_task_runner_host *,uint64_t conversation,pg_slice call_id,struct json_object **);
void pg_durable_tool_observation_release(struct json_object *);
#endif
