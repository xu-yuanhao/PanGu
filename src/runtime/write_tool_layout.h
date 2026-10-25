#ifndef PG_WRITE_TOOL_LAYOUT_H
#define PG_WRITE_TOOL_LAYOUT_H
#include "read_tool_layout.h"
#define PG_WTL_STAT_LINKS 16
#define PG_WTL_OPEN_FILE 0xA0841
#define PG_WTL_OPEN_PARENT 0xB0800
#define PG_WTL_JSON_FLAGS 17
#ifndef __ASSEMBLER__
#include "write_tool.h"
#include <json-c/json.h>
_Static_assert(sizeof(pg_write_tool)==4 && offsetof(pg_write_tool,directory)==0,"write context layout");
_Static_assert(offsetof(struct stat,st_nlink)==PG_WTL_STAT_LINKS && sizeof(((struct stat *)0)->st_nlink)==8,"link count layout");
_Static_assert((O_WRONLY|O_CREAT|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK)==PG_WTL_OPEN_FILE,"write flags");
_Static_assert((O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK)==PG_WTL_OPEN_PARENT,"parent flags");
_Static_assert((JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8)==PG_WTL_JSON_FLAGS,"JSON flags");
#endif
#endif
