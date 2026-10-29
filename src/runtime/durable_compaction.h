#ifndef PANGU_DURABLE_COMPACTION_H
#define PANGU_DURABLE_COMPACTION_H
#include "durable_generation.h"
#include "compaction_hooks.h"
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
 pg_task_phase phases[4];
 const pg_compaction_hooks *hooks;
 const pg_durable_generation_hooks *extension_hooks;
} pg_durable_compaction;
/* Host uses the selected model with an empty executable tool catalog. Borrow
   it through scheduler cleanup. Start commits one conversation-owned Task;
   drive the existing Scheduler and query its durable terminal outcome. */
int pg_durable_compaction_init(pg_durable_compaction *,const pg_durable_generation_host *);
void pg_durable_compaction_bind_hooks(pg_durable_compaction *,const pg_compaction_hooks *);
void pg_durable_compaction_bind_extensions(pg_durable_compaction *,const pg_durable_generation_hooks *);
int pg_durable_compaction_start(pg_durable_compaction *,pg_durable_session *,
 const pg_task_runner_host *,const pg_compaction_request *,uint64_t *task_id);
typedef struct {
 pg_durable_compaction *module;
 pg_durable_session *session;
 const pg_task_runner_host *host;
 pg_task_scheduler *scheduler;
} pg_compaction_binding;
void pg_durable_compaction_service(pg_compaction_binding *,pg_compaction_ops *);
typedef struct {
 const pg_task_definition *definition;
 const pg_task_runner_host *host;
 int (*mint)(void *,uint64_t *);
 uint64_t conversation,owner;
 const struct json_object *input;
} pg_compaction_creation;
/* Stage a conversation-owned or Generation-owned compaction on the caller's
   transaction with its Live status. Definition/host/input are borrowed only
   through this call; documents belongs to the same active Session mutation. */
int pg_durable_compaction_create(const pg_compaction_creation *,pg_record_tx *,uint64_t *,pg_document_tx *);
typedef struct {uint64_t keep_recent_tokens,reserve_tokens,background_tokens;} pg_compaction_policy;
/* 0 no cut, 1 blocking threshold, 2 background threshold. */
int pg_durable_compaction_threshold(const struct json_object *view,
 const struct json_object *configuration,pg_compaction_policy *);
#endif
