#ifndef PANGU_DURABLE_TOOL_H
#define PANGU_DURABLE_TOOL_H
#include "task_runner.h"
#include "tools.h"
typedef struct {
 void *context;
 /* Exclusive operation lease: 0 acquired, 1 busy (poll again), 2 unavailable.
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
    details/structuredOutput are JSON, diagnostics is an array. Failure stops settlement without
    repeating the effect. Metadata stays owned by this task through commit. Diagnostics are rendered
    as a separate model text block; structuredOutput is stored in Entry.data. */
 int (*snapshot)(void *,uint64_t task_id,const struct json_object *call,struct json_object **);
} pg_durable_tool_host;
typedef struct {
 const pg_durable_tool_host *host;
 pg_task_definition definition;
 pg_task_phase phases[2];
} pg_durable_tool;
/* Borrowed host lives through all tasks and native cleanup. Resolve this
   definition as pangu.tool. Input {kind:"model",assistant,callId} references
   a committed pangu.assistant Entry's model[0].content toolCall. call stores
   intent before execution; recovered execute reruns only when both policies
   say safe. Result Entry and Task outcome commit atomically through Session.
   Tool output and model-facing identity use PanGu's own Entry namespace. */
int pg_durable_tool_init(pg_durable_tool *,const pg_durable_tool_host *);
/* Serialized read, no scheduler advancement. 0 transfers owned
   {task,call,entry?}, 1 missing; other values are read/argument errors. The
   conversation and call ID select the latest matching native Tool task. */
int pg_durable_tool_observe(const pg_task_runner_host *,uint64_t conversation,pg_slice call_id,struct json_object **);
void pg_durable_tool_observation_release(struct json_object *);
#endif
