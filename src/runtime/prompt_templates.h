#ifndef PANGU_PROMPT_TEMPLATES_H
#define PANGU_PROMPT_TEMPLATES_H
#include "skills.h"
/* Owned {templates:[],diagnostics:[]} snapshot. Absolute optional roots, user
   before project. Markdown children only, preserving source order/duplicates. */
int pg_prompt_templates_load(const char *,const char *,struct json_object **);
/* Extra absolute strings or owning-scope source descriptors. */
int pg_prompt_templates_load_config(const char *,const char *,const struct json_object *,struct json_object **);
/* Quotes group arguments; values/defaults are substituted once. All output
   buffers must be disjoint from borrowed inputs; failure clears output/size. */
int pg_prompt_args(pg_slice,struct json_object **);
int pg_prompt_substitute(pg_slice,const struct json_object *,char *,size_t,size_t *);
/* Expand /template or /skill:name, otherwise copy unchanged. Catalogs are
   borrowed load snapshots; skills may include disabled automatic invocations.
   Explicit skill bodies are loaded at invocation time. */
int pg_resource_expand(const struct json_object *,const struct json_object *,pg_slice,char *,size_t,size_t *);
#endif
