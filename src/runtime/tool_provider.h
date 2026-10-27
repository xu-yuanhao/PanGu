#ifndef PANGU_TOOL_PROVIDER_H
#define PANGU_TOOL_PROVIDER_H
#include "tools.h"
struct json_object;
/* Service tools.provider binds declarations, executor and result metadata as
   one immutable lease. Contexts/catalog live through all pending operations.
   snapshot runs before close, returns 0 with owned {details?,structuredOutput?,
   diagnostics?} or NULL. A replacement owns all three channels. */
typedef struct {
 uint32_t abi,size;
 pg_tools tools;
 const pg_tool_registry *catalog;
 void *context;
 int (*snapshot)(void *,uint64_t,const struct json_object *,struct json_object **);
 const char *identity;
} pg_tool_provider;
#define PG_TOOL_PROVIDER_ABI 1u
#endif
