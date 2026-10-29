#ifndef PG_READ_TOOL_H
#define PG_READ_TOOL_H
#include "tools.h"
/* POSIX read-only backend; caller selects a cwd and owns its lifetime.
   Inline images are returned through detached final metadata snapshots.
   POSIX relative/absolute paths, symlinks and dot segments. No writes/shell.
   offset/limit select bounded lines; PG_TOOL_PARTIAL means the output is not a
   complete file snapshot, so editors must not use it for a full-file overwrite. */
struct json_object;
typedef struct {size_t max_bytes;unsigned max_width,max_height;} pg_read_image_limits;
/* Borrowed processor table. prepare returns an owned {data,mimeType,resized?,
   convertedFrom?}, 1 when no representation fits, or another nonzero error. */
typedef struct {void *context;int (*prepare)(void *,pg_slice,const char *,const pg_read_image_limits *,struct json_object **);} pg_read_image_processor;
/* Final metadata is owned until the next execution or close; options are
   pinned by the serialized workspace lease before execution. */
typedef struct {int directory;struct json_object *result;const pg_read_image_processor *images;pg_read_image_limits image_limits;unsigned vision,reserved;} pg_read_tool;
int pg_read_tool_snapshot(pg_read_tool *,struct json_object **);
int pg_read_tool_configure_images(pg_read_tool *,const struct json_object *,const pg_read_image_processor *);
int pg_read_tool_open(pg_read_tool *, const char *root);
void pg_read_tool_close(pg_read_tool *);
pg_tool_definition pg_read_tool_definition(pg_read_tool *);
/* Internal complete snapshot: path only, same safety/output ceiling, never PARTIAL. */
pg_tool_definition pg_read_snapshot_definition(pg_read_tool *);
#endif
