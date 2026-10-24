#ifndef PG_READ_TOOL_H
#define PG_READ_TOOL_H
#include "tools.h"
/* POSIX read-only backend; caller selects a trusted root and owns its lifetime.
   Relative paths only; symlinks and dot segments are refused. No writes/shell. */
typedef struct {int directory;} pg_read_tool;
int pg_read_tool_open(pg_read_tool *, const char *root);
void pg_read_tool_close(pg_read_tool *);
pg_tool_definition pg_read_tool_definition(pg_read_tool *);
#endif
