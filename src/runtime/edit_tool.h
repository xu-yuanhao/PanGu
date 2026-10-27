#ifndef PG_EDIT_TOOL_H
#define PG_EDIT_TOOL_H
#include "tools.h"
/* Injected read/write tool service, borrowed for the editor's lifetime.
   Owner serializes calls and does not rebind the service during a run.
   Both inner operations retain validation/authorization. The injected read must
   provide a complete file snapshot; a model-facing line pager is not that service.
   An interrupted edit is unsafe to replay; output capacity must be >=64.
   Success reports replacement count and the first differing byte's 1-based line
   in the preserved file content. first_changed_line is zero until successful
   completion and is reset on every execution. Native text/argument limits apply. */
typedef struct { pg_tools files; uint64_t first_changed_line; } pg_edit_tool;
pg_tool_definition pg_edit_tool_definition(pg_edit_tool *);
#endif
