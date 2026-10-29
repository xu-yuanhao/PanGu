#ifndef PANGU_AGENT_STATE_H
#define PANGU_AGENT_STATE_H
#include "task_runner.h"
#include "tools.h"
/* Pi AgentDoc: conversation-scoped, rewindable, as-of fork. Changes use stable
   names; absent fields stay unchanged and JSON null removes a stored field.
   stage participates in the caller's Session transaction and borrows inputs. */
int pg_agent_state_stage(pg_document_tx *,uint64_t,const struct json_object *,int (*mint)(void *,uint64_t *));
/* Tool result addTools: append explicit selections, remove exclusions and
   extend modelTools.add in the same Doc transaction. Names are borrowed. */
int pg_agent_state_add_tools(pg_document_tx *,uint64_t,const struct json_object *,int (*mint)(void *,uint64_t *));
int pg_agent_state_read(pg_durable_session *,uint64_t,struct json_object **);
/* Mutation callbacks use adopted cache only; preload with read when opening
   each conversation, before admitting inputs or scheduling recovered work. */
int pg_agent_state_peek(pg_durable_session *,uint64_t,struct json_object **);
/* Detached request configuration; appends instructions after resource sections and preserves selection
   fields for the model/executor consumers. Other provider fields stay native. */
int pg_agent_state_prepare(const struct json_object *,const struct json_object *,struct json_object **);
/* Fresh/quiescent output. Model selection also applies modelTools; execution
   selection applies tools. Arrays preserve first occurrence and skip missing
   names; {remove} preserves installed order. Definitions/code stay borrowed. */
int pg_agent_tools_select(const pg_tool_registry *,const struct json_object *,unsigned model,pg_tool_registry *);
int pg_agent_tools_names(const pg_tool_registry *,struct json_object **);
int pg_agent_tool_allowed(const pg_tool_registry *,pg_slice);
/* Gate retains the original provider callbacks/metadata and rejects disabled
   names before execute, including recovered calls. Borrow through close. */
typedef struct {pg_tools base;const pg_tool_registry *allowed;} pg_agent_tools_gate;
pg_tools pg_agent_tools_service(pg_agent_tools_gate *);
typedef struct {void *context;int (*read)(void *,uint64_t,struct json_object **);int (*configure)(void *,uint64_t,const struct json_object *);} pg_agent_state_ops;
/* Harness create: defaults, Agent as-of/owner inheritance, explicit change and
   init share the conversation's creation transaction. Borrow all providers. */
typedef struct pg_agent_conversation_binding {
 pg_durable_session *session;void *read_context;
 int (*read)(void *,unsigned,uint64_t,struct json_object **);
 int (*mint)(void *,uint64_t *);const pg_fork_storage *fork;
} pg_agent_conversation_binding;
typedef struct pg_agent_conversation_options {
 pg_conversation_request target;const struct json_object *agent;void *context;
 int (*init)(void *,pg_record_tx *,uint64_t);
} pg_agent_conversation_options;
int pg_agent_conversation_create(pg_agent_conversation_binding *,const pg_agent_conversation_options *,struct json_object **);
int pg_provider_state_read(pg_durable_session *,uint64_t,struct json_object **);
/* Missing legacy identity is initialized once before the provider request. */
int pg_provider_state_ensure(pg_agent_conversation_binding *,uint64_t,struct json_object **);
#endif
