#ifndef PANGU_DURABLE_CLIENT_H
#define PANGU_DURABLE_CLIENT_H
#include "durable_generation.h"
typedef struct {
 pg_durable_session *session;
 pg_task_scheduler *scheduler;
 const pg_task_definition *generation;
 const struct json_object *configuration;
 int (*mint)(void *,uint64_t *);
 uint64_t conversation;
} pg_durable_client_config;
typedef struct {
 pg_durable_client_config config;
 void *head,*tail,*delivered;
 unsigned busy,active,cancelled,stopping,failed,fault;
 uint64_t root;
 struct json_object *tool_events,*error_message;
} pg_durable_client;
/* Single foreground client on its scheduler, borrowed providers/configuration.
   Submit atomically appends a User Entry and creates Generation. Poll pumps
   the scheduler and returns detached queued events; slices expire next poll.
   Suspend parks the scheduler and discards observations without aborting work;
   drain returned native tool leases before closing the owning Session. */
int pg_durable_client_open(pg_durable_client *,const pg_durable_client_config *);
pg_client pg_durable_client_service(pg_durable_client *);
int pg_durable_client_suspend(void *);
/* Native provider callbacks, serialized on the same foreground loop. */
int pg_durable_client_emit(pg_durable_client *,const pg_event *);
void pg_durable_client_tool(pg_durable_client *,uint64_t task_id,unsigned finished);
void pg_durable_client_output(pg_durable_client *,uint64_t task_id,pg_slice);
/* Attach as Record Session publication callback with context=client. */
void pg_durable_client_publish(void *,const struct json_object *,uint64_t);
void pg_durable_client_fault(pg_durable_client *);
#endif
