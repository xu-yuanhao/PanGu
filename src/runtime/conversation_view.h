#ifndef PANGU_CONVERSATION_VIEW_H
#define PANGU_CONVERSATION_VIEW_H
#include "task_runner.h"
typedef struct {pg_durable_session *session;const pg_task_runner_host *host;} pg_conversation_view_binding;
/* Owned {conversation,entries,docs,inbox}. entries are the active head/range;
   docs contains existing pangu.live/pangu.usage, inbox contains queued PanGu
   Submission records in ID order. Serialized readOnLine, no Task work/writes.
   Binding/providers are borrowed through Host lifetime.
   0 success, 2 missing conversation, 3 busy, other error. */
int pg_conversation_view_read(pg_conversation_view_binding *,uint64_t,struct json_object **);
typedef struct {void *context;int (*read)(void *,uint64_t,struct json_object **);void (*release)(struct json_object *);} pg_conversation_view_ops;
void pg_conversation_view_service(pg_conversation_view_binding *,pg_conversation_view_ops *);
#endif
