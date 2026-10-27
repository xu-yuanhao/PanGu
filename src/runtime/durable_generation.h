#ifndef PANGU_DURABLE_GENERATION_H
#define PANGU_DURABLE_GENERATION_H
#include "durable_tool.h"
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
} pg_durable_generation_host;
typedef struct {
 const pg_durable_generation_host *host;
 pg_task_definition definition;
 pg_task_phase phases[3];
} pg_durable_generation;
/* Resolve pangu.generation alongside pangu.tool. prepare saves configuration
   and context cutoff; request streams through the native model service, then
   atomically appends assistant/create-owned-tools/waits or ends with its Entry
   receipt. tools hands off to a new generation task after results commit.
   Caller holds host/model/definition leases until cleanup. Model service uses
   the existing bounded pg_session protocol; unsupported content rejects rather
   than silently losing model context. */
int pg_durable_generation_init(pg_durable_generation *,const pg_durable_generation_host *);
/* Read-only committed startup history through the same model projection. */
int pg_durable_generation_history(const pg_task_runner_host *,uint64_t,const struct json_object *,pg_session *);
#endif
