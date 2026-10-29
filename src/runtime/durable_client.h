#ifndef PANGU_DURABLE_CLIENT_H
#define PANGU_DURABLE_CLIENT_H
#include "durable_inputs.h"
typedef struct {
 pg_durable_session *session;
 pg_task_scheduler *scheduler;
 pg_durable_inputs *inputs;
 uint64_t conversation;
} pg_durable_client_config;
typedef struct {
 pg_durable_client_config config;
 void *head,*tail,*delivered;
 unsigned busy,active,cancelled,stopping,failed,fault;
 uint64_t root;
 struct json_object *tool_events,*error_message,*usage,*live;
 uint64_t message_task;
 struct json_object *turns,*inbox;
} pg_durable_client;
/* Single foreground client on its scheduler, borrowed providers/configuration.
   Submit delegates atomic Submission/Entry/Generation admission to inputs. Poll pumps
   the scheduler and returns detached queued events; slices expire next poll.
   Suspend parks the scheduler and discards observations without aborting work;
   drain returned native tool leases before closing the owning Session. */
/* Binds the optional TUI operation table to this client. Borrow until suspend;
   no resources allocated. Idle input admission emits the same run boundary as
   submit, queued inputs preserve the active root/cancellation state. */
int pg_durable_client_inputs_service(pg_durable_client *,pg_inputs_ops *);
int pg_durable_client_open(pg_durable_client *,const pg_durable_client_config *);
int pg_durable_client_select(pg_durable_client *,uint64_t conversation);
/* Owned display rows for the selected conversation, serialized without pumping. */
int pg_durable_client_history(pg_durable_client *,struct json_object **);
int pg_durable_client_conversations_service(pg_durable_client *,pg_conversation_client_ops *);
pg_client pg_durable_client_service(pg_durable_client *);
int pg_durable_client_suspend(void *);
/* Native provider callbacks, serialized on the same foreground loop. */
int pg_durable_client_emit(pg_durable_client *,const pg_event *);
void pg_durable_client_tool(pg_durable_client *,uint64_t task_id,unsigned finished);
void pg_durable_client_output(pg_durable_client *,uint64_t task_id,pg_slice);
void pg_durable_client_text(pg_durable_client *,uint64_t task_id,pg_slice);
/* Attach as Record Session publication callback with context=client. */
void pg_durable_client_publish(void *,const struct json_object *,uint64_t);
void pg_durable_client_fault(pg_durable_client *);
/* Background Task branches do not fail the foreground run. */
void pg_durable_client_fault_task(pg_durable_client *,uint64_t);
/* tui_tool_observer read signature; owned committed snapshot, no pumping. */
int pg_durable_client_tool_observe(void *,const char *,size_t,struct json_object **);
#endif
