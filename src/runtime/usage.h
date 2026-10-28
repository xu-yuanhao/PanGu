#ifndef PANGU_USAGE_H
#define PANGU_USAGE_H
#include "task_runner.h"
/* Stage usage on the caller's open mutation. All inputs are borrowed through
   return. Runner is an active phase and usage is a finite Pi-shaped counter
   object. Mint uses the Record Tx read context. Missing usage is a no-op. */
int pg_usage_record(pg_task_runner *,pg_document_tx *,int (*mint)(void *,uint64_t *),const char *bucket,const char *key,const struct json_object *usage);
int pg_usage_model(pg_task_runner *,pg_document_tx *,int (*mint)(void *,uint64_t *),const struct json_object *configuration,const struct json_object *assistant);
/* Detached current conversation ledger; caller releases with json_object_put. */
/* Publication-only cache read: PG_DOC_MISSING without loading Storage. */
int pg_usage_peek(pg_durable_session *,uint64_t,struct json_object **);
int pg_usage_read(pg_durable_session *,uint64_t conversation,struct json_object **);
typedef struct {void *context;int (*read)(void *,uint64_t,struct json_object **);} pg_usage_ops;
/* Binding borrows Session until the Host scope finishes. */
void pg_usage_service(pg_durable_session *,pg_usage_ops *);
/* Detached committed {live,usage} from one synchronous foreground read.
   This does not pump Scheduler or advance tasks. */
int pg_agent_snapshot(pg_durable_session *,uint64_t,struct json_object **);
typedef struct {pg_durable_session *session;uint64_t conversation;struct json_object *previous;unsigned active,reserved;} pg_agent_watch;
/* Serialized pull view, no Scheduler work. First next returns the initial
   snapshot; later next coalesces committed state since the previous pull.
   1 transfers an owned changed snapshot, 0 unchanged, negative read error.
   Close drops the retained view; close watches before closing Session. */
int pg_agent_watch_open(pg_durable_session *,uint64_t,pg_agent_watch *);
int pg_agent_watch_next(pg_agent_watch *,struct json_object **);
void pg_agent_watch_close(pg_agent_watch *);
typedef struct {void *context;int (*snapshot)(void *,uint64_t,struct json_object **);void (*release)(struct json_object *);
 int (*watch)(void *,uint64_t,pg_agent_watch *);int (*next)(pg_agent_watch *,struct json_object **);void (*close)(pg_agent_watch *);} pg_observation_ops;
void pg_observation_service(pg_durable_session *,pg_observation_ops *);
#endif
