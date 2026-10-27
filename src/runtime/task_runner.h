#ifndef PANGU_TASK_RUNNER_H
#define PANGU_TASK_RUNNER_H
#include "durable_session.h"
typedef struct pg_task_runner pg_task_runner;
typedef struct {
 pg_slice name;
 void *context;
 /* 0 phase returned, 1 pending native operation, negative uncaught error.
    poll may call runner_commit. The phase record stays immutable through
    pending calls. cleanup releases operation/watch/timer resources once. */
 int (*poll)(void *,pg_task_runner *,const struct json_object *);
 void (*cleanup)(void *,pg_task_runner *);
} pg_task_phase;
typedef struct {
 pg_slice name;
 uint64_t version;
 void *context;
 int (*initial)(void *,const struct json_object *,struct json_object **);
 const pg_task_phase *phases;
 size_t phase_count;
 int (*abort)(void *,pg_task_runner *,const struct json_object *);
 void (*abort_cleanup)(void *,pg_task_runner *);
 /* Owned {input,checkpoint}; borrowed arguments are immutable. */
 int (*migrate)(void *,const struct json_object *,const struct json_object *,uint64_t,struct json_object **);
} pg_task_definition;
typedef struct {
 void *context;
 int (*read)(void *,unsigned,uint64_t,struct json_object **);
 int (*scan)(void *,const pg_memory_query *,pg_memory_page *);
 int (*documents)(void *,const pg_document_query *,pg_memory_page *);
 int (*resolve)(void *,pg_slice,const pg_task_definition **);
 double (*now)(void *);
 void (*report)(void *,uint64_t,int);
} pg_task_runner_host;
struct pg_task_runner {
 pg_durable_session *session;
 const pg_task_runner_host *host;
 uint64_t id;
 struct json_object *record,*phase_record;
 const pg_task_definition *definition;
 const pg_task_phase *phase;
 void *operation;
 uint64_t restart_limit;
 unsigned busy,active,pending,closing,recovered,committing;
 pg_slice error;
 pg_record_tx *mutation; /* Private synchronous phase transaction, otherwise NULL. */
};
typedef enum {PG_TASK_PROGRESS,PG_TASK_PENDING,PG_TASK_DONE,PG_TASK_BLOCKED,
 PG_TASK_INVALID,PG_TASK_ALLOCATION,PG_TASK_STORAGE,PG_TASK_CLOSED} pg_task_status;
/* Per-task execution owner, selected by the scheduler. Caller serializes each
   runner and assigns an ID to only one live runner. Host/definition leases
   remain valid until cleanup/close. Ordinary callbacks run outside Storage
   adoption and must return normally. Open borrows a healthy writable Session.
   Poll executes at most one phase callback, so multiple runners can interleave
   async operations on the host loop. error borrows handler text until return;
   operation belongs to the current phase and its cleanup callback.
   Close runners before Session close; healthy running work parks as pending
   without incrementing restarts. Process interruption is counted on recovery. */
pg_task_status pg_task_runner_open(pg_task_runner *,pg_durable_session *,uint64_t,
 const pg_task_runner_host *,uint64_t restart_limit);
pg_task_status pg_task_runner_poll(pg_task_runner *);
/* Commit the running phase's next running/waiting/sleeping/terminal state through the
   Durable mutation line. Terminal becomes completing while ordinary owned
   tasks/conversations are live. Decisions use fresh committed state. State is
   borrowed during call; successful return has stored a detached snapshot.
   Waiting requires existing task IDs and allSettled or failFast; failFast
   members must be directly owned. Admission rejects cycles through waits and
   ordinary ownership, including owned conversations; abort handlers cannot
   wait. Rejected waits leave durable state unchanged and expose borrowed error
   text until the handler returns. Multi-record phase transactions compose on
   the same Session mutation line. */
pg_task_status pg_task_runner_commit(pg_task_runner *,const struct json_object *);
/* stage may read then stage other records/documents, creates owned next state
   (NULL keeps state), and returns OK or an error. The callback transfers partial
   next state on failure too. State and staged writes commit atomically or are
   discarded together. Current task state belongs to this runner; stage must not
   replace it directly. Callback performs no external effects or async work.
   Public Tx reads keep their read-before-write barrier; wait/owner checks use
   final staged candidates after the callback. */
pg_task_status pg_task_runner_commit_with(pg_task_runner *,void *,
 pg_task_status (*stage)(void *,pg_record_tx *,pg_document_tx *,struct json_object **));
pg_task_status pg_task_runner_close(pg_task_runner *);
typedef struct {
 pg_durable_session *session;
 const pg_task_runner_host *host;
 pg_task_runner **runners;
 size_t count,capacity;
 uint64_t restart_limit;
 double next_wake;
 unsigned busy,active,closing,reserved;
} pg_task_scheduler;
/* Zeroed caller storage, one scheduler per writable Session. Open discovers
   and recovers live tasks before any phase executes. Even failed open must be
   closed. Each poll discovers newly committed tasks and gives each live runner
   at most one callback, in ascending task-ID order. Host/definition leases and
   serialization follow runner rules; close scheduler before Session. No other
   runner may execute its IDs. next_wake is the earliest sleeping deadline from
   the last successful poll, infinity when absent. Registry/service changes and
   native-operation completions request another poll on the injected host loop.
   Before handlers, persisted cancellation intent and failed failFast members
   derive ordinary descendant/sibling marks in one Session commit. Aborts drain
   children first; background task branches remain independent.
   A close requested from a callback returns PENDING; call close after unwinding. */
pg_task_status pg_task_scheduler_open(pg_task_scheduler *,pg_durable_session *,
 const pg_task_runner_host *,uint64_t restart_limit);
pg_task_status pg_task_scheduler_poll(pg_task_scheduler *);
pg_task_status pg_task_scheduler_close(pg_task_scheduler *);
#endif
