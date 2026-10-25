#ifndef PG_BASH_TOOL_H
#define PG_BASH_TOOL_H
#include "tools.h"
#include "process.h"
#include "stream_decode.h"
#define PG_BASH_RETAIN 3072
#define PG_BASH_CAPTURE_LIMIT (16u*1024u*1024u)
/* Serialized owner; process service, workspace fd and explicit environment are
   borrowed for the whole operation; init source must not overlap destination. Commands are not sandboxed by this tool.
   Retains a UTF-8 tail <=3072 bytes/2000 lines, spills raw combined output to
   a private temporary file. 16 MiB capture limit cancels with a partial-file
   error. Final result fits the current 4096-byte transcript slot.
   Regular-file spill IO is bounded per poll, but is not asynchronous disk IO. */
typedef struct {
 pg_process_ops process;
 int directory;
 unsigned stage;
 char *const *environment;
 int file;
 unsigned flags;
 int exit_code;
 unsigned reason;
 uint64_t captured;
 size_t pending_pos,pending_size,tail_size;
 pg_stream_decoder decoder[2];
 char pending[12296],tail[PG_BASH_RETAIN],path[64];
} pg_bash_tool;
int pg_bash_tool_init(pg_bash_tool *,const pg_process_ops *,int directory,char *const *environment);
pg_tool_definition pg_bash_tool_definition(pg_bash_tool *);
#endif
