#ifndef PANGU_EXTENSIONS_H
#define PANGU_EXTENSIONS_H
#include "agent_state.h"
/* Immutable descriptors/code live until every dependent Agent lease closes.
   Callback output descriptors borrow immutable strings, contexts and callbacks
   from that same lease. render transfers an owned JSON string or NULL (omit).
   Callbacks are synchronous; nonzero errors are reported and skip the target. */
typedef struct {const char *key;void *context;int (*render)(void *,const struct json_object *,struct json_object **);unsigned tag,reserved;} pg_extension_section;
typedef struct {unsigned kind,reserved;const char *target;void *context;union {int (*tool)(void *,const pg_tool_definition *,pg_tool_definition *);int (*section)(void *,const pg_extension_section *,pg_extension_section *);} wrap;} pg_extension_wrap;
#define PG_EXTENSION_WRAP_TOOL 1u
#define PG_EXTENSION_WRAP_SECTION 2u
typedef struct {const char *name;const pg_tool_definition *tools;size_t tool_count;const pg_extension_section *sections;size_t section_count;const pg_extension_wrap *wraps;size_t wrap_count;} pg_extension;
#define PG_EXTENSIONS_ABI 1u
/* Host extensions.registry. defaults is an optional stable-name JSON array;
   absent defaults select coding followed by all installed extensions. coding
   denotes the injected base coding-tool provider. */
typedef struct {uint32_t abi,size;const pg_extension *const *entries;size_t count;const struct json_object *defaults;void *context;void (*report)(void *,const char *extension,const char *target,int);} pg_extensions;
typedef struct {pg_tool_registry tools;pg_extension_section *sections;size_t section_count;const pg_extension **selected;size_t selected_count;const pg_extensions *provider;unsigned tools_changed,reserved;} pg_extension_result;
/* Fresh/closed result; failure closes partial output. Native tool quota follows
   the existing registry. Name selection follows AgentState.extensions. */
int pg_extensions_resolve(const pg_extensions *,const struct json_object *state,const pg_tool_registry *base,pg_extension_result *);
void pg_extensions_close(pg_extension_result *);
/* One operation lease: unchanged base definitions dispatch through the injected
   provider; extension/replaced definitions use the composed registry. */
typedef struct {pg_tools base,composed;const pg_tool_registry *base_catalog,*composed_catalog;const pg_tools *active;} pg_extension_tools;
pg_tools pg_extension_tools_service(pg_extension_tools *);
/* Detached request config: selected stable names and ordered rendered sections
   appended to resource prompt. Agent instructions are applied afterwards. */
int pg_extensions_prepare(const pg_extension_result *,const struct json_object *,struct json_object **);
#endif
