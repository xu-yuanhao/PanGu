#ifndef PG_READ_TOOL_H
#define PG_READ_TOOL_H
#include "tools.h"
/* POSIX read-only backend; caller selects a cwd and owns its lifetime.
   POSIX relative/absolute paths, symlinks and dot segments. No writes/shell.
   offset/limit select bounded lines; PG_TOOL_PARTIAL means the output is not a
   complete file snapshot, so editors must not use it for a full-file overwrite. */
typedef struct {int directory;} pg_read_tool;
int pg_read_tool_open(pg_read_tool *, const char *root);
void pg_read_tool_close(pg_read_tool *);
pg_tool_definition pg_read_tool_definition(pg_read_tool *);
/* Internal complete snapshot: path only, same safety/output ceiling, never PARTIAL. */
pg_tool_definition pg_read_snapshot_definition(pg_read_tool *);
#endif
