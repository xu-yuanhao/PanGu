#ifndef PANGU_DURABLE_GENERATION_H
#define PANGU_DURABLE_GENERATION_H
#include "durable_tool.h"
#include "model_failure.h"
struct pg_durable_inputs;
typedef struct {
 void *context;
 /* Owned strict JSON {systemPrompt,model,...request options}. The provider
    must honor this saved configuration on recovery; no credentials in it. */
 int (*prepare)(void *,uint64_t conversation,struct json_object **);
 int (*acquire)(void *,const struct json_object *configuration,pg_model *);
 void (*release)(void *,const pg_model *);
 /* Same read_context as runner.host, like durable Tool's mint. */
 int (*mint)(void *,uint64_t *);
 void (*progress)(void *,uint64_t,pg_slice);
 const pg_task_definition *tool;
 /* Optional provider failure query, before close; absent means non-retryable. */
 void (*failure)(void *,const pg_model *,pg_model_failure *);
 struct pg_durable_inputs *inputs;
 /* Before provider close, complete the private assistant with terminal facts. */
 int (*complete)(void *,const pg_model *,struct json_object *assistant);
 /* Fetch begins a provider event stream; cancellation may cooperatively pend.
    Both borrow the durable handle and acquired provider until cleanup. */
 int (*fetch_deferred)(void *,const pg_model *,const struct json_object *);
 int (*cancel_deferred)(void *,const pg_model *,const struct json_object *);
} pg_durable_generation_host;
typedef struct {
 const pg_durable_generation_host *host;
 pg_task_definition definition;
 pg_task_phase phases[5];
 const pg_task_definition *compaction;
} pg_durable_generation;
/* Resolve pangu.generation alongside pangu.tool. prepare saves configuration
   and context cutoff; request streams through the native model service, then
   atomically appends assistant/create-owned-tools/waits or ends with its Entry
   receipt. A provider failure appends an error assistant and commits bounded
   retry sleep or a failed receipt; retry prepares current configuration after
   the durable deadline. tools hands off to a new generation task after results commit.
   Text progress appends to conversation pangu.live using throttled deferred
   document commits; request recovery/abort converts saved progress into an
   aborted assistant, and terminal classification clears it atomically.
   Caller holds host/model/definition leases until cleanup. Model service borrows a PG_CONTEXT_VIEW canonical request through close;
   the operation retains its complete JSON owner. Unsupported content rejects
   rather than silently losing model context. */
int pg_durable_generation_init(pg_durable_generation *,const pg_durable_generation_host *);
/* Optional definition borrowed for the Generation module lifetime. */
void pg_durable_generation_bind_compaction(pg_durable_generation *,const pg_task_definition *);
/* Read-only committed startup history through the same model projection.
   The latest head leads retained entries; last edits win. Results are ordered
   after their calls, first matching ID before the next assistant. Missing
   results are ephemeral error receipts; unmatched results are omitted.
   The current OpenAI transport collapses system content and ordered section
   patches into system_prompt. Tool deltas need an execution-registry binding
   and currently reject during projection. */
int pg_durable_generation_history(const pg_task_runner_host *,uint64_t,const struct json_object *,pg_session *);
/* Detached JSON {head?,entries,contributions,messages}, beyond the model ABI's
   row limit. cutoff is inclusive (UINT64_MAX for current). Ordered messages
   include ephemeral repaired tool results; contribution arrays retain entry
   alignment and original committed objects. Release with json_object_put. */
/* Owned canonical request with dynamic rendered systemPrompt and messages. */
int pg_durable_request_read(const pg_task_runner_host *,uint64_t,uint64_t,const struct json_object *,struct json_object **);
int pg_durable_context_read(const pg_task_runner_host *,uint64_t conversation,
                            uint64_t cutoff,struct json_object **);
int pg_durable_context_order(const struct json_object *messages,struct json_object **);
#endif
