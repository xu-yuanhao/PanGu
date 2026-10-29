#ifndef PANGU_TASK_ACCESS_H
#define PANGU_TASK_ACCESS_H
#include "task_runner.h"
#include "document_reader.h"
#include "durable_inputs.h"
/* Borrowed phase capability; valid until phase cleanup. History is optional
   for current reads/memos and required for snapshot_as_of. Outputs are detached
   and owned; status follows pg_task_status except document reads, which retain
   document-reader statuses. Memo names are explicit byte slices, including embedded NUL;
   PanGu stores their hex byte spelling as Task.memos keys. found distinguishes
   JSON null from absence. Candidate is borrowed; first durable winner remains. */
typedef struct {pg_task_runner *runner;const struct json_object *phase;
 const pg_document_history_reader *history;uint64_t task_id,conversation;} pg_task_access;
int pg_task_access_open(pg_task_access *,pg_task_runner *,const pg_document_history_reader *);
int pg_task_access_memo(const pg_task_access *,pg_slice,const struct json_object *candidate,
 unsigned has_candidate,struct json_object **value,unsigned *found);
int pg_task_access_get_task(const pg_task_access *,uint64_t,struct json_object **);
int pg_task_access_snapshot(const pg_task_access *,const pg_document_definition *,const pg_document_address *,struct json_object **);
int pg_task_access_snapshot_as_of(const pg_task_access *,const pg_document_definition *,const pg_document_address *,uint64_t,struct json_object **);
/* Commit child creation on the parent's guarded mutation line. Creation's
   conversation defaults to this capability; ownership options remain explicit.
   Success transfers the durable ID; failed admission clears it. */
int pg_task_access_create_task(const pg_task_access *,const pg_task_definition *,
 const struct json_object *input,const struct json_object *options,int (*mint)(void *,uint64_t *),uint64_t *);
struct pg_agent_conversation_binding;
struct pg_agent_conversation_options;
/* Uses the active task mutation guard and preserves explicit ownership.
   Creation options, init and providers are borrowed for this synchronous call. */
int pg_task_access_create_conversation(const pg_task_access *,struct pg_agent_conversation_binding *,const struct pg_agent_conversation_options *,struct json_object **);
/* Admit into a conversation on this task's guarded transaction. Receipt is
   transferred only after settlement; requestId preserves Submission dedup. */
int pg_task_access_submit(const pg_task_access *,pg_durable_inputs *,uint64_t,const struct json_object *,pg_input_receipt *);
pg_task_runner *pg_task_scheduler_find(pg_task_scheduler *,uint64_t);
#endif
