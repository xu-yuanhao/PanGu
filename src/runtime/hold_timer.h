#ifndef PANGU_HOLD_TIMER_H
#define PANGU_HOLD_TIMER_H
#include "holds.h"
typedef void (*pg_timer_job)(void *);
typedef struct {
 void *context;
 /* Mandatory foreground timer; delay is monotonic milliseconds. Return 0
    and an owned handle, or nonzero with any partial handle for cancellation.
    Delivery is once, on the host thread, after foreground returns. */
 int (*foreground)(void *,uint64_t delay_ms,pg_timer_job,void *,void **handle);
 /* Release the handle, including after delivery. No future deliveries may
    reference its callback/context after return. Must not invoke callbacks. */
 void (*cancel)(void *,void *handle);
 /* Admit one job to the Session mutation line; no inline execution. Success
    runs it exactly once with records/cache/holds idle. Nonzero admits nothing. */
 int (*enqueue)(void *,pg_timer_job,void *);
} pg_hold_timer_host;
typedef struct {
 pg_document_holds *holds;
 pg_record_session *session;
 pg_hold_timer_host host;
 void *ticket;
 unsigned busy,phase,error,reserved;
} pg_hold_timer;
/* Zeroed/closed controller; providers, records and holds remain borrowed until
   stop returns 0. Calls and timer deliveries share the host event thread.
   sync follows held changes/releases on the Session line: arm the earliest
   finite deadline, retain an already fired ticket until its queued job runs,
   and cancel/rearm only when the deadline changes. UINT64_MAX is infinity.
   Delivery only enqueues; the admitted job stores all holds, without another
   memory observation. Internal arm/queue/store failure preserves the first
   Session failure and prevents further timer work.
   stop seals timer admission and cancels the foreground handle. It returns 1
   while an already queued job must drain, then 0; that job skips Storage.
   Stop completes before holds_finish and cache/Storage close. -1 means invalid
   or reentrant stop. State is private. */
pg_records_status pg_hold_timer_open(pg_hold_timer *,pg_document_holds *,pg_record_session *,const pg_hold_timer_host *);
pg_records_status pg_hold_timer_sync(pg_hold_timer *);
int pg_hold_timer_stop(pg_hold_timer *);
#endif
