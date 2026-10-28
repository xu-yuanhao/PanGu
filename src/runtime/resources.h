#ifndef PANGU_RESOURCES_H
#define PANGU_RESOURCES_H
#include "prompt_templates.h"
/* Four independently owned JSON references; builders fill zeroed output and
   leave all produced references there even on failure. Prompt is a string,
   other fields are the existing settings/skills/template catalog objects. */
typedef struct {struct json_object *skills,*templates,*settings,*prompt;} pg_resource_candidate;
typedef struct {
 void *context;
 int (*build)(void *,pg_resource_candidate *);
 int (*busy)(void *);
} pg_resource_host;
typedef struct {
 const pg_resource_host *host;
 struct json_object *configuration; /* borrowed mutable idle model configuration */
 pg_resource_candidate current;
 uint64_t revision;
 unsigned refreshing,closed;
} pg_resources;
/* Zeroed storage. Configuration must outlive close; its systemPrompt is updated
   only after a complete candidate succeeds. Serialized on the foreground loop.
   Reload returns 0 adopted, 3 busy, 5 build/allocation/shape failure, 7 closed.
   Failed reload preserves current catalogs, revision and configured prompt. */
int pg_resources_open(pg_resources *,const pg_resource_host *,struct json_object *);
int pg_resources_reload(void *);
int pg_resources_close(pg_resources *);
/* Owned detached {revision,skills,templates,systemPrompt,diagnostics}; settings
   values are omitted. Caller releases once with json_object_put. */
int pg_resources_snapshot(void *,struct json_object **);
int pg_resources_expand(void *,pg_slice,char *,size_t,size_t *);
#endif
