#ifndef PANGU_EXTENSIONS_H
#define PANGU_EXTENSIONS_H
#include "agent_state.h"
#include "task_provider.h"
#include "task_access.h"
/* Immutable descriptors/code are protected by their snapshot/Host lease.
   Callback output descriptors borrow immutable strings, contexts and callbacks
   from that same lease. render transfers an owned JSON string or NULL (omit).
   render returns 0 complete, 1 pending, negative error. Pending render must
   provide cancel, which releases callback resources before its snapshot closes.
   Wrappers are synchronous; callback errors retain previously shown text. */
typedef struct {const char *key;void *context;int (*render)(void *,const struct json_object *,struct json_object **);unsigned tag,reserved;void (*cancel)(void *,const struct json_object *);} pg_extension_section;
typedef struct {unsigned kind,reserved;const char *target;void *context;union {int (*tool)(void *,const pg_tool_definition *,pg_tool_definition *);int (*section)(void *,const pg_extension_section *,pg_extension_section *);} wrap;} pg_extension_wrap;
#define PG_EXTENSION_WRAP_TOOL 1u
#define PG_EXTENSION_WRAP_SECTION 2u
#define PG_EXTENSION_BEFORE_TOOL 1u
#define PG_EXTENSION_AFTER_TOOL 2u
#define PG_EXTENSION_BEFORE_REQUEST 3u
#define PG_EXTENSION_AFTER_RESPONSE 4u
#define PG_EXTENSION_ON_YIELD 5u
#define PG_EXTENSION_AFTER_TOOLS 6u
#define PG_EXTENSION_BEFORE_COMPACT 7u
/* Tool request {call,result?}; before decisions {arguments?,block?}, after decisions
   replace native {output:string|Content[],isError?,details?,diagnostics?,structuredOutput?,usage?}.
   beforeRequest uses canonical {messages,...} and returns {messages}; response
   and yield use {message}, yield may return {continue:string|content[]};
   afterTools observes {assistant,results}; beforeCompact receives
   {reason,entries,messages,firstKept,instructions?}, first {decline:true} or
   {summary:string} wins.
   Decisions transfer owned strict JSON. Request is borrowed through completion
   or cancellation. 0 completed, 1 pending with stable request, negative error. Cancel releases
   pending callback resources before its extension lease closes. */
typedef struct {unsigned event,reserved;void *context;int (*poll)(void *,const struct json_object *,struct json_object **,const pg_task_access *);void (*cancel)(void *,const struct json_object *);} pg_extension_hook;
typedef struct {const char *name;const pg_tool_definition *tools;size_t tool_count;const pg_extension_section *sections;size_t section_count;const pg_extension_wrap *wraps;size_t wrap_count;const pg_extension_hook *hooks;size_t hook_count;const pg_task_definition *const *tasks;size_t task_count;} pg_extension;
#define PG_EXTENSIONS_ABI 11u
/* Host extensions.registry. A facade acquire returns an immutable snapshot
   and lease on success (0), none on failure; release drops that lease.
   defaults is an optional stable-name JSON array;
   absent defaults select coding followed by all installed extensions. coding
   denotes the injected base coding-tool provider. */
typedef struct pg_extensions_tag {uint32_t abi,size;const pg_extension *const *entries;size_t count;const struct json_object *defaults;void *context;void (*report)(void *,const char *extension,const char *target,int);void *snapshot_context;int (*acquire)(void *,const struct pg_extensions_tag **,void **);void (*release)(void *);} pg_extensions;
typedef struct {pg_tool_registry tools;pg_extension_section *sections;size_t section_count;const pg_extension **selected;size_t selected_count;const pg_extensions *provider;unsigned tools_changed,reserved;void *snapshot_lease;void (*snapshot_release)(void *);} pg_extension_result;
/* Fresh/closed result; failure closes partial output. Native tool quota follows
   the existing registry. Name selection follows AgentState.extensions. */
int pg_extensions_validate(const pg_extensions *);
int pg_extensions_resolve(const pg_extensions *,const struct json_object *state,const pg_tool_registry *base,pg_extension_result *);
void pg_extensions_close(pg_extension_result *);
int pg_extension_hook_resolve_open(const pg_extensions *,const struct json_object *,const pg_tool_registry *,unsigned,const struct json_object *,void **);
int pg_extension_hook_open(const pg_extension_result *,unsigned,const struct json_object *,void **);
/* Completed poll transfers an owned final request. beforeTool errors block;
   other hook errors are reported and preserve the previous value. */
int pg_extension_hook_poll(void *,struct json_object **);
/* A Generation phase keeps its selection across hook events. Switching events
   requires the previous event completed; close releases the whole scope. */
int pg_extension_hook_poll_event(void *,unsigned,const struct json_object *,struct json_object **);
int pg_extension_hook_bind_access(void *,pg_task_runner *,const pg_document_history_reader *);
void pg_extension_hook_close(void *);
/* One operation lease: unchanged base definitions dispatch through the injected
   provider; extension/replaced definitions use the composed registry. */
typedef struct {pg_tools base,composed;const pg_tool_registry *base_catalog,*composed_catalog;const pg_tools *active;} pg_extension_tools;
pg_tools pg_extension_tools_service(pg_extension_tools *);
/* Same immutable descriptor routing as dispatch, without starting an operation. */
int pg_extension_tools_uses_base(pg_extension_tools *,pg_slice);
/* Detached request config: selected stable names and ordered rendered sections
   appended to resource prompt. Agent instructions are applied afterwards. */
/* Open borrows the composed result through close, owns a stable detached input.
   Poll transfers owned output on 0, returns 1 pending or negative error.
   Synchronous convenience cancels pending work and returns 3. */
int pg_extension_prepare_open(const pg_extension_result *,const struct json_object *,void **);
int pg_extension_prepare_poll(void *,struct json_object **);
void pg_extension_prepare_close(void *);
int pg_extensions_prepare(const pg_extension_result *,const struct json_object *,struct json_object **);
#endif
