#ifndef PG_TOOLS_H
#define PG_TOOLS_H
#include "agent.h"
#define PG_TOOL_LIMIT 16
/* Borrowed immutable strings and contexts must outlive the registry and runs.
   validate checks tool-specific schema semantics; policy is separate from visibility.
   Validators/authorization are side-effect free; callbacks return promptly. */
typedef struct {
 int (*next)(void *, char *, size_t, size_t *);
 void (*cancel)(void *);
 int (*close)(void *);
} pg_tool_async;
typedef struct {
 const char *name, *description, *schema;
 void *context;
 int (*validate)(void *, pg_slice);
 int (*authorize)(void *, pg_slice);
 int (*execute)(void *, pg_slice, char *, size_t, size_t *);
 unsigned replay_safe; /* 0=unsafe (default), 1=safe; immutable during a run. */
 const pg_tool_async *async; /* NULL means execute completes synchronously. */
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
#endif
