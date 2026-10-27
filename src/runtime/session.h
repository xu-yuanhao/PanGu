#ifndef PANGU_SESSION_H
#define PANGU_SESSION_H
#include "../contracts/client.h"
#include <stdint.h>
#define PG_MESSAGE_LIMIT 64
#define PG_TEXT_LIMIT 4096
#define PG_ID_LIMIT 64
#define PG_TOOL_BATCH_LIMIT 8
#define PG_SESSION_BATCH_LIMIT (PG_TOOL_BATCH_LIMIT+1)
/* THINKING is a model-projection row; committed history exposes visible roles. */
typedef enum { PG_USER, PG_ASSISTANT, PG_TOOL_CALL, PG_TOOL_RESULT, PG_THINKING } pg_role;
typedef struct {
    pg_role role;
    size_t size;
    char text[PG_TEXT_LIMIT];
    char call_id[PG_ID_LIMIT], tool_name[PG_ID_LIMIT];
    int is_error;
} pg_message;
typedef struct { pg_message messages[PG_MESSAGE_LIMIT]; size_t count;
    char system_prompt[PG_TEXT_LIMIT]; size_t system_size;
} pg_session;
#define PG_RUN_TURN_LIMIT 16
#define PG_RUN_RESTART_LIMIT 5
typedef enum { PG_RUN_MODEL=1, PG_RUN_TOOL, PG_RUN_TERMINAL, PG_RUN_ABORTING, PG_RUN_TOOL_UNSAFE } pg_run_phase;
typedef struct {
    uint64_t id;
    pg_run_phase phase;
    unsigned turns;
    uint64_t call_index;
    pg_outcome outcome;
    unsigned restarts;
    uint64_t input_index;
} pg_run_checkpoint;
typedef enum { PG_SESSION_APPEND, PG_SESSION_SYSTEM, PG_SESSION_RESET, PG_SESSION_CHECKPOINT, PG_SESSION_PROFILE, PG_SESSION_SUBMISSION } pg_session_change_kind;
/* APPEND >2 records requires a checkpoint, atomically stored before any tool. */
typedef struct {
    pg_session_change_kind kind;
    const pg_message *messages;
    size_t count;
    pg_slice text;
    const pg_run_checkpoint *checkpoint;
} pg_session_change;
typedef enum { PG_STORE_OK, PG_STORE_REJECTED, PG_STORE_UNCERTAIN } pg_store_result;
/* Synchronous, single-owner storage seam. The borrowed detached batch expires on return.
   OK means the whole batch is stored at expected_seq+1; REJECTED means no write.
   Any ambiguous outcome must be UNCERTAIN. No reentry, retained pointers or exceptions.
   The caller adopts after return, before any publication or subsequent commit. */
typedef struct {
    void *context;
    pg_store_result (*commit)(void *, const pg_session_change *, uint64_t expected_seq, uint64_t *seq);
} pg_storage;
#define PG_SUBMISSION_LIMIT 64
typedef struct {
    uint64_t id;
    unsigned settled;
    pg_outcome outcome;
    char request_id[PG_ID_LIMIT];
} pg_submission_record;
typedef struct {
    pg_storage storage;
    uint64_t sequence;
    unsigned busy, failed;
    pg_run_checkpoint run;
    size_t profile_size;
    unsigned char profile[PG_TEXT_LIMIT];
    size_t submission_count;
    pg_submission_record submissions[PG_SUBMISSION_LIMIT];
} pg_session_control;
/* Source is borrowed until the loaded Session releases its storage. load accepts
   fresh view/control once and binds/replays its provider; 0 means success.
   The hosting provider owns partial-open cleanup even when load fails. */
typedef struct {
    void *context;
    int (*load)(void *, pg_session *, pg_session_control *);
} pg_session_source;
/* Caller owns view/control, initialized to zero with an explicit storage binding. */
int pg_session_commit(pg_session *, pg_session_control *, const pg_session_change *);
/* Explicit volatile mode: Session adoption is its sole in-memory state; no persistence. */
pg_store_result pg_session_volatile_commit(void *, const pg_session_change *, uint64_t, uint64_t *);
#endif
