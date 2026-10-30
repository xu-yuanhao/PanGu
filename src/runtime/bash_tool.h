#ifndef PG_BASH_TOOL_H
#define PG_BASH_TOOL_H
#include "tools.h"
#include "process.h"
#include "execution_env.h"
#include "stream_decode.h"
#define PG_BASH_RETAIN 51200
#define PG_BASH_CAPTURE_LIMIT (16u*1024u*1024u)
/* Serialized owner; ExecutionEnv and its services remain borrowed for the
   whole operation. Commands are not sandboxed by this tool.
   Retains a UTF-8 tail <=50 KiB/2000 lines, spills raw combined output to
   a private temporary file. 16 MiB capture limit cancels with a partial-file
   error. The result snapshot owns the complete retained output; callback previews
   fit the caller-provided buffer.
   Regular-file spill IO is bounded per poll, but is not asynchronous disk IO. */
typedef struct {
 const pg_execution_env *environment;
 unsigned stage;
 int file;
 unsigned flags;
 int exit_code;
 unsigned reason;
 uint64_t captured;
 size_t pending_pos,pending_size,tail_size;
 pg_stream_decoder decoder[2];
 char pending[12296],tail[PG_BASH_RETAIN],path[64];
} pg_bash_tool;
int pg_bash_tool_init(pg_bash_tool *,const pg_execution_env *);
pg_tool_definition pg_bash_tool_definition(pg_bash_tool *);
struct json_object;
/* Before close, at final stage only: 0 owned {structuredOutput,diagnostics},
   1 no completed result, 5 allocation error. Reads typed state, never output text. */
int pg_bash_tool_snapshot(const pg_bash_tool *,struct json_object **);
#endif
