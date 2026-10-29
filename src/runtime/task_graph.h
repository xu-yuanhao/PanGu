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
/* Persist intent only; Scheduler polling drains the ordinary owned subtree.
   0 marked, 1 already terminal, 2 missing, 3 busy, 4 invalid, 5 allocation,
   6 storage, 7 closed. A later explicit abort replaces a restart reason. */
int pg_task_abort(pg_task_scheduler *,uint64_t);
typedef struct {void *context;int (*abort)(void *,uint64_t);} pg_task_control_ops;
void pg_task_control_service(pg_task_scheduler *,pg_task_control_ops *);
#endif
