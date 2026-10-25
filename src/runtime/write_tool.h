#ifndef PG_WRITE_TOOL_H
#define PG_WRITE_TOOL_H
#include "tools.h"
/* Borrows a trusted directory descriptor. The caller owns its lifetime.
   Relative paths only; parents are created, symlinks/nonregular files refused.
   Calls must be serialized by the owner. Output capacity must be at least 64.
   An interrupted write may be partial: this tool is not replay-safe. */
typedef struct { int directory; } pg_write_tool;
pg_tool_definition pg_write_tool_definition(pg_write_tool *);
#endif
