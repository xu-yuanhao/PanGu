#ifndef PG_BASH_TOOL_LAYOUT_H
#define PG_BASH_TOOL_LAYOUT_H
#define PG_BH_ENVIRONMENT 0
#define PG_BH_STAGE 8
#define PG_BH_FILE 12
#define PG_BH_FLAGS 16
#define PG_BH_EXIT 20
#define PG_BH_REASON 24
#define PG_BH_CAPTURED 32
#define PG_BH_POS 40
#define PG_BH_PENDING_N 48
#define PG_BH_TAIL_N 56
#define PG_BH_DECODER 64
#define PG_BH_PENDING 120
#define PG_BH_TAIL 12416
#define PG_BH_PATH 15488
#define PG_BH_BYTES 15552
#define PG_BH_RETAIN 3072
#define PG_BH_LIMIT 16777216
#ifndef __ASSEMBLER__
#include "bash_tool.h"
#include <stddef.h>
#include <fcntl.h>
#include <json-c/json.h>
#define PG_BHO(f,n) _Static_assert(offsetof(pg_bash_tool,f)==n,"bash." #f)
PG_BHO(environment,PG_BH_ENVIRONMENT);PG_BHO(stage,PG_BH_STAGE);
PG_BHO(file,PG_BH_FILE);PG_BHO(flags,PG_BH_FLAGS);PG_BHO(exit_code,PG_BH_EXIT);
PG_BHO(reason,PG_BH_REASON);PG_BHO(captured,PG_BH_CAPTURED);
PG_BHO(pending_pos,PG_BH_POS);PG_BHO(pending_size,PG_BH_PENDING_N);PG_BHO(tail_size,PG_BH_TAIL_N);
PG_BHO(decoder,PG_BH_DECODER);PG_BHO(pending,PG_BH_PENDING);PG_BHO(tail,PG_BH_TAIL);PG_BHO(path,PG_BH_PATH);
_Static_assert(sizeof(pg_bash_tool)==PG_BH_BYTES&&sizeof(pg_stream_decoder)==28,"bash layout");
_Static_assert(PG_BASH_RETAIN==PG_BH_RETAIN&&PG_BASH_CAPTURE_LIMIT==PG_BH_LIMIT,"bash bounds");
_Static_assert(O_CLOEXEC==524288&&(JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8)==17,"library flags");
_Static_assert(json_type_object==4&&json_type_string==6&&json_type_double==2&&json_type_int==3,"JSON types");
#endif
#endif
