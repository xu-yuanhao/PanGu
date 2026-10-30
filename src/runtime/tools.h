#ifndef PG_TOOLS_H
#define PG_TOOLS_H
#include "agent.h"
#define PG_TOOL_LIMIT 16
/* Durable final OK/ERROR with this size selects the invocation's reported output.
   Zero size is an explicit empty result; WAIT/OUTPUT always carry real sizes. */
#define PG_TOOL_OUTPUT_REPORTED ((size_t)-1)
/* Borrowed immutable strings and contexts must outlive the registry and runs.
   validate checks tool-specific schema semantics; policy is separate from visibility.
   Validators/authorization are side-effect free; callbacks return promptly. */
typedef struct {size_t max_bytes,max_lines;unsigned tail,reserved;const char *structured_output_schema;const char *constrained_sampling;} pg_tool_options;
/* constrained_sampling is optional Pi constrainedSampling JSON; the descriptor
   and both schema strings outlive every registry/snapshot borrowing them. */
struct pg_tool_execution;
typedef struct {
 int (*next)(void *, char *, size_t, size_t *);
 void (*cancel)(void *);
 int (*close)(void *);
 /* Optional paired factory: open allocates an invocation context for all tool
    callbacks, performs no effects, and borrows api until dispose. The Durable
    Host binds factories before dispatch; an unbound factory cannot execute. Failure owns
    no context. close drains native work; dispose releases the context once. */
 int (*open)(void *definition_context,struct pg_tool_execution *api,void **instance);
 void (*dispose)(void *instance);
} pg_tool_async;
typedef struct {
 const char *name, *description, *schema;
 void *context;
 int (*validate)(void *, pg_slice);
 int (*authorize)(void *, pg_slice);
 int (*execute)(void *, pg_slice, char *, size_t, size_t *);
 unsigned replay_safe; /* 0=unsafe (default), 1=safe; immutable during a run. */
 const pg_tool_async *async; /* NULL means execute completes synchronously. */
 const pg_tool_options *options; /* Borrowed immutable; NULL: default output budget and ordinary sampling. */
} pg_tool_definition;
typedef struct {
 pg_tool_definition entries[PG_TOOL_LIMIT]; size_t count;
 const pg_tool_definition *active;
 unsigned operation_state, cancel_sent, dispatch_depth;
} pg_tool_registry;
/* Fresh/quiescent storage only. Initialization is transactional; no reinit,
   modification or unregistration during a run. One async operation per registry.
   Inline nested sync calls remain allowed; async start reserves ownership. */
int pg_tool_registry_init(pg_tool_registry *, const pg_tool_definition *, size_t);
pg_tools pg_tool_registry_service(pg_tool_registry *);
/* Read-only pre-intent validation of {name,arguments}: 1 valid, 0 bad
   arguments, -1 unavailable. Uses the executor's same lookup/JSON/validator
   path; authorization remains immediately before the effect. */
int pg_tool_registry_check_json(const pg_tool_registry *,const struct json_object *);
#endif
