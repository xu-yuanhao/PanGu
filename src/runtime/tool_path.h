#ifndef PANGU_TOOL_PATH_H
#define PANGU_TOOL_PATH_H
#ifndef __ASSEMBLER__
#include "../contracts/client.h"
#include <curl/urlapi.h>
_Static_assert(CURLUPART_URL==0&&CURLUPART_HOST==5&&CURLUPART_PATH==7&&CURLU_URLDECODE==64,"curl URL ABI");
/* Pi normalizeToolPath/ExecutionEnv path adaptation. Bounded UTF-8 input,
   disjoint output; maps pasted spaces/@, home and valid file URLs, then POSIX
   lexical separators/dot segments. Relative leading parents remain cwd-relative.
   0 NUL-terminated result; 1 failure with cleared output. */
int pg_tool_path_prepare(pg_slice,char *,size_t);
/* ExecutionEnv stage: resolve paths without tool-specific pasted @/spaces. */
int pg_tool_path_resolve(pg_slice,char *,size_t);
struct json_object;
/* Private parsed argument object only; replace its path without touching the
   original borrowed request. Failure preserves the object's original path. */
int pg_tool_path_arguments(struct json_object *);
const char *pg_tool_path_home(void);
#endif
#endif
