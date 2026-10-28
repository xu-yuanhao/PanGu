#ifndef PANGU_DURABLE_COMPACTION_H
#define PANGU_DURABLE_COMPACTION_H
#include "durable_generation.h"
/* Select the first retained Entry from a detached Durable context view.
   first_kept=0 means there is no nonempty old prefix to summarize. */
int pg_durable_compaction_select(const struct json_object *,uint64_t keep_recent_tokens,uint64_t *first_kept);
/* Build the pinned summary request from contributions before first_kept;
   instructions is an optional UTF-8 C string. Success owns text_owner; the
   request borrows its immutable UTF-8 through model close. The output owner
   must be fresh; release it once with json_object_put. */
int pg_durable_compaction_request(const struct json_object *,uint64_t first_kept,
                                  const char *instructions,pg_session *,struct json_object **text_owner);
typedef struct {
 const pg_durable_generation_host *host;
 pg_task_definition definition;
 pg_task_phase phases[3];
} pg_durable_compaction;
/* Host uses the selected model with an empty executable tool catalog. Borrow
   it through scheduler cleanup. Start commits one conversation-owned Task;
   drive the existing Scheduler and query its durable terminal outcome. */
int pg_durable_compaction_init(pg_durable_compaction *,const pg_durable_generation_host *);
int pg_durable_compaction_start(pg_durable_compaction *,pg_durable_session *,
 const pg_task_runner_host *,const pg_compaction_request *,uint64_t *task_id);
typedef struct {
 pg_durable_compaction *module;
 pg_durable_session *session;
 const pg_task_runner_host *host;
 pg_task_scheduler *scheduler;
} pg_compaction_binding;
void pg_durable_compaction_service(pg_compaction_binding *,pg_compaction_ops *);
#endif
