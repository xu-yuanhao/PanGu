#ifndef PANGU_SUBMISSIONS_H
#define PANGU_SUBMISSIONS_H
#include "session.h"
/* Input-only admission; no queue. Duplicate keys return their original record
   before checking busy/input. A new key atomically stores input plus run checkpoint.
   Keys are 1..63 non-NUL bytes, retained across reset; capacity is explicit.
   Output is caller-owned/disjoint, unchanged on rejection; outcome is valid only
   when settled. submit: 0 success, nonzero rejected; find: 0 found, 1 absent,
   2 invalid arguments. No callbacks or IO occur on duplicate admission. */
typedef struct {
    void *context;
    int (*submit)(void *, pg_slice request_id, pg_slice input, pg_submission_record *out);
    int (*find)(void *, pg_slice request_id, pg_submission_record *out);
} pg_submissions;
/* Borrowed until the next mutation. NULL means absent/invalid; no IO. */
const pg_submission_record *pg_submission_find(const pg_session_control *, pg_slice request_id);
#endif
