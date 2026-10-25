#ifndef PG_PROCESS_H
#define PG_PROCESS_H
#include "../contracts/client.h"
#include <stdint.h>
typedef enum { PG_PROCESS_WAIT, PG_PROCESS_OUTPUT, PG_PROCESS_EXIT } pg_process_kind;
typedef enum { PG_PROCESS_NORMAL, PG_PROCESS_CANCELLED, PG_PROCESS_TIMED_OUT,
               PG_PROCESS_CLOSED, PG_PROCESS_FAILED } pg_process_reason;
typedef struct {
 int directory;
 const char *shell;
 char *const *environment;
 uint64_t timeout_ms;
} pg_process_request;
typedef struct {
 pg_process_kind kind;
 unsigned stream; /* 0=stdout, 1=stderr; each stream keeps its own byte order. */
 pg_slice bytes;
 int exit_code, signal;
 pg_process_reason reason;
 int error;
} pg_process_event;
/* Caller-owned implementation storage; do not mutate an active instance. */
typedef struct {
 unsigned state;
 int pid, output[2];
 unsigned eof, next_stream, exited;
 int exit_code, signal;
 pg_process_reason reason;
 int error;
 unsigned killed;
 uint64_t deadline, idle_deadline;
 char buffer[4096];
} pg_process;
typedef struct {
 void *context;
 int (*start)(void *,const pg_process_request *,pg_slice);
 int (*next)(void *,pg_process_event *);
 void (*cancel)(void *);
 int (*close)(void *);
} pg_process_ops;
/* Replaceable provider view; owns no resources beyond the supplied instance. */
pg_process_ops pg_process_service(pg_process *);
/* Fresh storage only. Owner serializes calls; no process-wide signal changes.
   start returns errno, leaves an idle instance unchanged on failure. It waits
   for spawn/exec setup, never for command completion. Root fd is borrowed;
   shell defaults to /bin/bash, NULL environment means empty, not inherited.
   timeout_ms=0 disables timeout, otherwise <=2147483647; command <=4095 bytes,
   no NUL. stdin is /dev/null, unrelated descriptors are closed in the child.
   Requires Linux/glibc spawn extensions and default SIGCHLD without NOCLDWAIT.
   Owner must not change that disposition or reap this pid outside this API. */
void pg_process_init(pg_process *);
int pg_process_start(pg_process *, const pg_process_request *, pg_slice);
/* next returns 0=wait, 1=output/one terminal event, -1=invalid pointers.
   Raw output may contain binary/partial UTF-8; borrowed until the next call.
   Owner controls polling frequency. Separate stream decoding, retention and
   spill belong to the consumer. A leader is kept waitable until cleanup;
   quiet inherited stdio settles after 100ms, normal active output rearms it.
   After cancellation/error, draining stops 100ms after observing leader exit. */
int pg_process_next(pg_process *, pg_process_event *);
void pg_process_cancel(pg_process *);
/* Active close kills the owned process group and drains/reaps cooperatively.
   Return 1 means poll/close again; 0 means all owned resources are released.
   Do not discard/reinitialize a pending instance. Repeated closed close is OK.
   Process groups are cancellation, not a sandbox or daemon supervisor. */
int pg_process_close(pg_process *);
#endif
