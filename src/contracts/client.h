#ifndef PANGU_CLIENT_H
#define PANGU_CLIENT_H
#include <stddef.h>
/* Internal source contract v0: borrowed event bytes expire at next poll. */
typedef struct { const char *data; size_t size; } pg_slice;
typedef enum { PG_RUN_STARTED, PG_TEXT_DELTA, PG_TOOL_STARTED, PG_TOOL_FINISHED, PG_RUN_FINISHED, PG_TOOL_OUTPUT_DELTA } pg_event_kind;
typedef enum { PG_SUCCESS, PG_ERROR, PG_CANCELLED } pg_outcome;
typedef struct {
    pg_event_kind kind;
    pg_outcome outcome;
    pg_slice text, call_id, tool_name;
} pg_event;
typedef struct {
    void *context;
    int (*submit)(void *, pg_slice); /* 0 accepted; nonzero leaves state unchanged */
    void (*cancel)(void *);
    int (*poll)(void *, pg_event *); /* 1 event, 0 no event; bounded work */
} pg_client;
#endif
