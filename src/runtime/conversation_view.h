#ifndef PANGU_CONVERSATION_VIEW_H
#define PANGU_CONVERSATION_VIEW_H
#include "task_runner.h"
#include "view_watch.h"
typedef struct pg_conversation_watch pg_conversation_watch;
typedef struct {pg_durable_session *session;const pg_task_runner_host *host;pg_conversation_watch *head;unsigned closed,reserved;} pg_conversation_view_binding;
struct pg_conversation_watch {pg_view_binding source;pg_view_watch observer;pg_conversation_view_binding *owner;uint64_t conversation;pg_conversation_watch *next;unsigned active,reserved;};
/* Owned {conversation,entries,docs,inbox}. entries are the active head/range;
   docs contains existing pangu.live/pangu.usage, inbox contains queued PanGu
   Submission records in ID order. Serialized readOnLine, no Task work/writes.
   Binding/providers are borrowed through Host lifetime.
   0 success, 2 missing conversation, 3 busy, other error. */
/* Detached ascending queued Submission records; caller already owns readOnLine. */
int pg_conversation_queue_read(const pg_task_runner_host *,uint64_t,struct json_object **);
int pg_conversation_view_read(pg_conversation_view_binding *,uint64_t,struct json_object **);
/* Zeroed/released watch storage lives until close or owning Host shutdown.
   next follows pg_view_watch_next: initial frame, then merged state changes. */
int pg_conversation_watch_open(pg_conversation_view_binding *,uint64_t,pg_conversation_watch *);
int pg_conversation_watch_next(pg_conversation_watch *,struct json_object **);
void pg_conversation_watch_close(pg_conversation_watch *);
void pg_conversation_view_close(pg_conversation_view_binding *);
typedef struct {void *context;int (*read)(void *,uint64_t,struct json_object **);void (*release)(struct json_object *);
 int (*watch)(void *,uint64_t,pg_conversation_watch *);int (*next)(pg_conversation_watch *,struct json_object **);void (*close)(pg_conversation_watch *);} pg_conversation_view_ops;
void pg_conversation_view_service(pg_conversation_view_binding *,pg_conversation_view_ops *);
void pg_conversation_history_service(pg_conversation_view_binding *,pg_conversation_history_ops *);
int pg_conversation_entries(pg_conversation_view_binding *,uint64_t,const pg_conversation_entry_query *,struct json_object **);
int pg_conversation_list(pg_conversation_view_binding *,const pg_conversation_query *,struct json_object **);
int pg_conversation_info(pg_conversation_view_binding *,uint64_t,struct json_object **);
struct pg_agent_conversation_binding;
int pg_conversation_rename(struct pg_agent_conversation_binding *,uint64_t,pg_slice);
#endif
