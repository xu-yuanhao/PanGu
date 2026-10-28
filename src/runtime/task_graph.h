#ifndef PANGU_TASK_GRAPH_H
#define PANGU_TASK_GRAPH_H
#include "task_runner.h"
#include "view_watch.h"
/* Detached live Session task graph; no scheduler advancement or migration.
   Serialized foreground read. Nodes omit input/checkpoint/outcome payloads.
   Caller releases JSON. Busy/error returns no graph. */
int pg_task_graph_read(pg_task_scheduler *,struct json_object **);
typedef struct {void *context;int (*read)(void *,struct json_object **);
 int (*watch)(void *,pg_view_watch *);int (*next)(pg_view_watch *,struct json_object **);void (*close)(pg_view_watch *);} pg_task_graph_ops;
void pg_task_graph_service(pg_view_binding *,pg_task_scheduler *,pg_task_graph_ops *);
#endif
