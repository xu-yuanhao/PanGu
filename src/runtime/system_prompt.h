#ifndef PANGU_SYSTEM_PROMPT_H
#define PANGU_SYSTEM_PROMPT_H
#include "extensions.h"
/* Pi Durable prompt.ts. Input views/declarations are borrowed immutable JSON.
   Outputs are owned. Replay preserves section/tool insertion order; plan emits
   PanGu system Entry drafts, including post-head omission edits and tool deltas. */
int pg_prompt_replay(const struct json_object *view,struct json_object **state);
int pg_prompt_plan(const struct json_object *view,const struct json_object *sections,
 const struct json_object *tools,uint64_t timestamp,struct json_object **drafts);
int pg_prompt_configure(const struct json_object *view,struct json_object *configuration,const pg_tool_registry *,uint64_t timestamp,const struct json_object *agent_state);
int pg_prompt_tools(const pg_tool_registry *,struct json_object **);
/* load transfers owned partial inputs even on failure; finish adds provider
   metadata without rendering. The preparer and its providers outlive close. */
typedef struct {struct json_object *state,*configuration,*view;} pg_prompt_inputs;
typedef struct {const pg_extensions *extensions;const pg_tool_registry *base;void *context;
 int (*load)(void *,uint64_t,pg_prompt_inputs *);
 int (*finish)(void *,struct json_object *);double (*now)(void *);} pg_prompt_preparer;
int pg_prompt_preparer_poll(void *,uint64_t,void **,struct json_object **);
void pg_prompt_preparer_close(void *,void *);
#endif
