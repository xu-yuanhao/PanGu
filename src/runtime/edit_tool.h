#ifndef PG_EDIT_TOOL_H
#define PG_EDIT_TOOL_H
#include "tools.h"
/* Injected read/write tool service, borrowed for the editor's lifetime.
   Owner serializes calls and does not rebind the service during a run.
   Both inner operations retain validation/authorization. The injected read must
   provide a complete file snapshot; a model-facing line pager is not that service.
   An interrupted edit is unsafe to replay; output capacity must be >=64.
   details owns the last successful {diff,patch,firstChangedLine}. It is reset
   by execution/clear; consumers retain their own JSON reference if needed. */
struct json_object;
typedef struct { pg_tools files; struct json_object *details; } pg_edit_tool;
void pg_edit_tool_clear(pg_edit_tool *);
pg_tool_definition pg_edit_tool_definition(pg_edit_tool *);
#endif
