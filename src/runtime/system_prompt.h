#ifndef PANGU_SYSTEM_PROMPT_H
#define PANGU_SYSTEM_PROMPT_H
#include "tools.h"
/* Pi Durable prompt.ts. Input views/declarations are borrowed immutable JSON.
   Outputs are owned. Replay preserves section/tool insertion order; plan emits
   PanGu system Entry drafts, including post-head omission edits and tool deltas. */
int pg_prompt_replay(const struct json_object *view,struct json_object **state);
int pg_prompt_plan(const struct json_object *view,const struct json_object *sections,
 const struct json_object *tools,uint64_t timestamp,struct json_object **drafts);
int pg_prompt_configure(const struct json_object *view,struct json_object *configuration,const pg_tool_registry *,uint64_t timestamp,const struct json_object *agent_state);
int pg_prompt_tools(const pg_tool_registry *,struct json_object **);
#endif
