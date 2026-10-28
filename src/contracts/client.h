#ifndef PANGU_CLIENT_H
#define PANGU_CLIENT_H
#include <stddef.h>
#include <stdint.h>
/* Internal source contract v0: borrowed event bytes expire at next poll. */
typedef struct { const char *data; size_t size; } pg_slice;
typedef enum { PG_RUN_STARTED, PG_TEXT_DELTA, PG_TOOL_STARTED, PG_TOOL_FINISHED, PG_RUN_FINISHED, PG_TOOL_OUTPUT_DELTA } pg_event_kind;
typedef enum { PG_SUCCESS, PG_ERROR, PG_CANCELLED } pg_outcome;
typedef struct {
    pg_event_kind kind;
    pg_outcome outcome;
    /* TOOL_STARTED text is the canonical JSON arguments; TOOL_FINISHED text
       is output. Slices borrow the event until next poll. */
    pg_slice text, call_id, tool_name;
} pg_event;
typedef struct {
    void *context;
    int (*submit)(void *, pg_slice); /* 0 accepted; nonzero leaves state unchanged */
    void (*cancel)(void *);
    int (*poll)(void *, pg_event *); /* 1 event, 0 no event; bounded work */
} pg_client;
struct json_object;
/* start copies instructions; reserve_tokens is 2..1000000. Serialized binding
   remains borrowed through poll/release and the owning runtime lifetime. */
typedef struct {
 uint64_t conversation,keep_recent_tokens,reserve_tokens;
 const char *instructions;
} pg_compaction_request;
typedef struct {
 void *context;
 int (*start)(void *,const pg_compaction_request *,uint64_t *);
 /* Pump one scheduler step; 0 owns a compaction Task snapshot, 1 missing. */
 int (*poll)(void *,uint64_t,struct json_object **);
 void (*release)(struct json_object *);
} pg_compaction_ops;

/* Serialized, non-pumping Submission operations. submit borrows a typed
   input/write draft and returns its stable deduplicated ID. status returns an
   owned record; list returns an owned ascending conversation-scoped array,
   limit 1..128, after=0 starts and later pages use the last ID. release once.
   Status: 0 success, 2 missing, 3 busy, 4 invalid, 5 allocation, 6 storage,
   7 closed. withdraw result: 0 withdrawn, 1 placed, 2 settled, 3 missing. */
typedef struct {
    void *context;
    int (*submit)(void *,const struct json_object *draft,uint64_t *id);
    int (*status)(void *,uint64_t id,struct json_object **);
    int (*withdraw)(void *,uint64_t id,unsigned *result);
    int (*list)(void *,uint64_t after,size_t limit,struct json_object **);
    void (*release)(struct json_object *);
} pg_inputs_ops;
/* Optional serialized resource service. reload: 0 adopted, 3 busy, 5 failed,
   7 closed. snapshot returns detached owned catalogs/diagnostics and revision;
   settings values and credentials are omitted. release once. expand processes
   foreground template/skill text; buffers are caller-owned and disjoint. */
typedef struct {
    void *context;
    int (*reload)(void *);
    int (*snapshot)(void *,struct json_object **);
    void (*release)(struct json_object *);
    int (*expand)(void *,pg_slice,char *,size_t,size_t *);
} pg_resource_ops;
#endif
