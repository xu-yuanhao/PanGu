#ifndef PG_EDIT_TOOL_H
#define PG_EDIT_TOOL_H
#include "tools.h"
/* Injected read/write tool service, borrowed for the editor's lifetime.
   Owner serializes calls and does not rebind the service during a run.
   Both inner operations retain validation/authorization.
   An interrupted edit is unsafe to replay; output capacity must be >=64.
   Native text/argument limits apply; structured diff display is not yet exposed. */
typedef struct { pg_tools files; } pg_edit_tool;
pg_tool_definition pg_edit_tool_definition(pg_edit_tool *);
#endif
