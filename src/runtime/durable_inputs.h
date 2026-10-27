#ifndef PANGU_DURABLE_INPUTS_H
#define PANGU_DURABLE_INPUTS_H
#include "durable_generation.h"
typedef struct pg_durable_inputs {
 pg_durable_session *session;
 const pg_task_runner_host *host;
 const pg_durable_generation *generation;
 const pg_record_queries *queries;
 const pg_record_requests *requests;
 unsigned busy,active;
} pg_durable_inputs;
typedef enum {PG_INPUT_OK,PG_INPUT_PENDING,PG_INPUT_MISSING,PG_INPUT_BUSY,
 PG_INPUT_INVALID,PG_INPUT_ALLOCATION,PG_INPUT_STORAGE,PG_INPUT_CLOSED} pg_input_status;
typedef struct {uint64_t id,task;unsigned status,reserved;} pg_input_receipt;
typedef struct {struct json_object *users;unsigned reset,reserved;} pg_input_boundary;
/* Borrowed services outlive all calls and scheduler phases. Serialized Session
   ownership; queued drafts live in PanGu Submission records, removed on place
   or withdrawal. Input content uses the current bounded UTF-8 text model ABI;
   Entry writes preserve the caller's typed JSON. Receipt status is queued=0/placed=1/settled=2. */
int pg_durable_inputs_open(pg_durable_inputs *,pg_durable_session *,const pg_task_runner_host *,
 const pg_durable_generation *,const pg_record_queries *,const pg_record_requests *);
int pg_durable_inputs_submit(pg_durable_inputs *,uint64_t conversation,const struct json_object *draft,pg_input_receipt *);
/* Owned detached snapshot; poll returns PENDING for queued/placed. The caller
   drives its scheduler, and may cancel its own polling without withdrawing. */
int pg_durable_inputs_status(pg_durable_inputs *,uint64_t id,struct json_object **);
int pg_durable_inputs_poll(pg_durable_inputs *,uint64_t id,struct json_object **);
/* result: 0 withdrawn,1 already placed,2 settled,3 not found/wrong scope. */
int pg_durable_inputs_withdraw(pg_durable_inputs *,uint64_t conversation,uint64_t id,unsigned *result);
/* Phase transaction helpers: prepare precedes table writes. Plan and boundary
   users are owned even on later failure. apply uses committed/staged facts only;
   kind=postTools(0),final(1),model failure(2),abort(3),withdraw queued inputs(4).
   Callers create a successor only when selected users or a continuing tool run
   require it, and commit state/Entries/Submission changes together. */
int pg_durable_inputs_prepare(pg_durable_inputs *,pg_record_tx *,uint64_t,struct json_object **plan);
int pg_durable_inputs_apply(pg_durable_inputs *,pg_record_tx *,struct json_object *plan,unsigned kind,uint64_t answer,pg_input_boundary *);
int pg_durable_inputs_start(pg_durable_inputs *,pg_record_tx *,uint64_t,const struct json_object *users,uint64_t *task);
#endif
