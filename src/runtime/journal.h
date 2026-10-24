#ifndef PANGU_JOURNAL_H
#define PANGU_JOURNAL_H
#include "session.h"
#define PG_JOURNAL_LIMIT (16u * 1024u * 1024u)
#define PG_JOURNAL_RECORD_MAX 8544
typedef struct {
    int file, directory;
    unsigned failed, busy;
    uint64_t sequence, end;
} pg_journal;
/* Initialize descriptors to -1. directory is borrowed; name is one basename.
   Open requires a fresh zero Session and fresh control. Success binds Storage;
   failure leaves the view empty and control unchanged. Single owner, no reentry.
   A torn final frame is truncated; complete corrupt/unknown frames are refused. */
int pg_journal_open(pg_journal *, int directory, const char *name, pg_session *, pg_session_control *);
void pg_journal_close(pg_journal *);
pg_store_result pg_journal_commit(void *, const pg_session_change *, uint64_t expected, uint64_t *sequence);
#endif
