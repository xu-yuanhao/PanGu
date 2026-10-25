#ifndef PG_TOOLS_H
#define PG_TOOLS_H
#include "agent.h"
#define PG_TOOL_LIMIT 16
/* Borrowed immutable strings and contexts must outlive the registry and runs.
   validate checks tool-specific schema semantics; policy is separate from visibility.
   All callbacks are synchronous and must return promptly in this initial subset. */
typedef struct {
 const char *name, *description, *schema;
 void *context;
 int (*validate)(void *, pg_slice);
 int (*authorize)(void *, pg_slice);
 int (*execute)(void *, pg_slice, char *, size_t, size_t *);
 unsigned replay_safe; /* 0=unsafe (default), 1=safe; immutable during a run. */
} pg_tool_definition;
typedef struct { pg_tool_definition entries[PG_TOOL_LIMIT]; size_t count; } pg_tool_registry;
/* Initialization is transactional; no modification/unregistration during a run. */
int pg_tool_registry_init(pg_tool_registry *, const pg_tool_definition *, size_t);
pg_tools pg_tool_registry_service(pg_tool_registry *);
#endif
