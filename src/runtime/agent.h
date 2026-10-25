#ifndef PANGU_AGENT_H
#define PANGU_AGENT_H
#include "session.h"
#include "submissions.h"
#define PG_TURN_LIMIT PG_RUN_TURN_LIMIT
typedef enum { PG_MODEL_WAIT, PG_MODEL_TEXT, PG_MODEL_CALL, PG_MODEL_DONE, PG_MODEL_ERROR } pg_model_kind;
typedef struct { pg_model_kind kind; pg_slice text, call_id, tool_name; } pg_model_event;
/* start/next must return promptly; WAIT is cooperative, not a blocking read.
   close releases each started request on success, error or cancellation. */
typedef struct {
    void *context;
    int (*start)(void *, const pg_session *);
    int (*next)(void *, pg_model_event *);
    void (*cancel)(void *);
    void (*close)(void *);
} pg_model;
/* One sequential synchronous call per assistant turn.
   execute returns promptly; asynchronous OS tools need a later operation API. */
typedef struct {
    void *context;
    int (*authorize)(void *, pg_slice name, pg_slice arguments);
    int (*execute)(void *, pg_slice name, pg_slice arguments, char *out, size_t capacity, size_t *size);
    /* Pure metadata query: exactly 1 opts into replay; NULL defaults to unsafe. */
    int (*replay_safe)(void *, pg_slice name);
} pg_tools;
typedef struct {
    pg_session session;
    pg_model model;
    pg_tools tools;
    unsigned state, turns;
    int cancelled, request_open;
    pg_outcome outcome;
    size_t assistant_index, call_index;
    char event_text[PG_TEXT_LIMIT];
    char error_text[256];
    pg_message draft, pending;
    pg_session_control commits;
    pg_run_checkpoint next_checkpoint;
    unsigned profile_verified;
} pg_agent;
int pg_agent_init(pg_agent *, pg_model, pg_tools);
pg_client pg_agent_client(pg_agent *);
pg_submissions pg_agent_submissions(pg_agent *);
/* Fresh idle agents only: bind before the first system/input/reset commit. */
int pg_agent_bind_storage(pg_agent *, pg_storage);
/* Compare loaded identity; replacing it is allowed only when no run is active. */
int pg_agent_bind_profile(pg_agent *, pg_slice identity);
/* Explicitly resume a freshly loaded run using its original model/tool configuration.
   Interrupted tools rerun only when stored and current policies both opt in.
   retry=1 explicitly retries a parked run. Returns 2 when restart limit parks it. */
int pg_agent_resume(pg_agent *, unsigned retry);
/* Stop the local executor without aborting durable work or emitting a terminal.
   Borrowed events expire; detach observers first, then explicitly resume later. */
int pg_agent_suspend(void *agent_context);
/* Idle-only operations; reset clears transcript but preserves system and services. */
int pg_agent_set_system(pg_agent *, pg_slice);
int pg_agent_reset_session(pg_agent *);
#endif
