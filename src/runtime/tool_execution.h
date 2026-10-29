#ifndef PANGU_TOOL_EXECUTION_H
#define PANGU_TOOL_EXECUTION_H
#include "task_access.h"
/* Call-owned capability, borrowed until the tool instance is disposed. Sequence
   restarts with an invocation; omitted keys replay by their original call order.
   Host sets resumes from the pinned tool replay policy before opening it. */
typedef struct pg_tool_execution {
 pg_task_access access;
 const pg_task_definition *tool;
 int (*mint)(void *,uint64_t *);
 const char *call_id;
 uint64_t sequence;
 unsigned memoized,resumes;
} pg_tool_execution;
/* 0 admitted, positive task status on failure; key NULL selects the next
   sequence key. Explicit keys are nonempty path segments, excluding __proto__
   and positive integer strings. Arguments are copied on admission. */
int pg_tool_execution_start(pg_tool_execution *,const char *name,const struct json_object *arguments,const char *key,uint64_t *task_id);
/* 0 transfers detached {taskId,isError,structuredOutput?,details?,diagnostics,
   usage?}, 1 pending, other positive task/document status on failure. */
int pg_tool_execution_poll(pg_tool_execution *,uint64_t task_id,struct json_object **);
#endif
