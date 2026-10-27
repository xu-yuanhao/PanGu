#ifndef PG_WRITE_TOOL_H
#define PG_WRITE_TOOL_H
#include "tools.h"
/* Borrows a trusted directory descriptor. The caller owns its lifetime.
   POSIX relative/absolute paths; parents are created and symlinks followed.
   The opened target must be a regular file before truncation.
   Calls must be serialized by the owner. Output capacity must be at least 64.
   An interrupted write may be partial: this tool is not replay-safe. */
typedef struct { int directory; } pg_write_tool;
pg_tool_definition pg_write_tool_definition(pg_write_tool *);
/* Internal ExecutionEnv write: incoming paths were tool-prepared already. */
pg_tool_definition pg_write_file_definition(pg_write_tool *);
#endif
