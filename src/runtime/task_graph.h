#ifndef PANGU_TASK_GRAPH_H
#define PANGU_TASK_GRAPH_H
#include "task_runner.h"
/* Detached live Session task graph; no scheduler advancement or migration.
   Serialized foreground read. Nodes omit input/checkpoint/outcome payloads.
   Caller releases JSON. Busy/error returns no graph. */
int pg_task_graph_read(pg_task_scheduler *,struct json_object **);
typedef struct pg_task_graph_watch pg_task_graph_watch;
typedef struct {pg_task_scheduler *scheduler;pg_task_graph_watch *head;struct json_object *current;unsigned closed,reserved;} pg_task_graph_binding;
struct pg_task_graph_watch {pg_task_graph_binding *binding;pg_task_graph_watch *next;struct json_object *previous;unsigned active,initial;};
/* Watch storage starts zeroed/released. Each observer owns its delivery baseline. First next delivers the attach
   snapshot; subsequent pulls fold intervening changes and refresh registry
   metadata. 1 owned frame, 0 unchanged, negative error/closed. No Task work.
   Watch storage stays alive until close or owning binding shutdown. */
int pg_task_graph_watch_open(pg_task_graph_binding *,pg_task_graph_watch *);
int pg_task_graph_watch_next(pg_task_graph_watch *,struct json_object **);
void pg_task_graph_watch_close(pg_task_graph_watch *);
void pg_task_graph_binding_close(pg_task_graph_binding *);
typedef struct {void *context;int (*read)(void *,struct json_object **);
 int (*watch)(void *,pg_task_graph_watch *);int (*next)(pg_task_graph_watch *,struct json_object **);void (*close)(pg_task_graph_watch *);} pg_task_graph_ops;
void pg_task_graph_service(pg_task_graph_binding *,pg_task_scheduler *,pg_task_graph_ops *);
#endif
