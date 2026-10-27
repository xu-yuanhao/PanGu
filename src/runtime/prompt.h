#ifndef PANGU_PROMPT_H
#define PANGU_PROMPT_H
#include "tools.h"
/* Borrowed synchronous complete UTF-8 read service. No Session mutation.
   Inputs, output and size storage must be disjoint.
   Success replaces the whole system prompt, including an intentionally empty file.
   Failure clears output/size; partial and asynchronous results are not accepted. */
int pg_prompt_from_file(const pg_tools *, pg_slice path, char *, size_t, size_t *);
/* Workdir-scoped resource source. probe: 1 regular file, 0 absent/not-file,
   negative IO failure. Complete read remains injected through files. */
typedef struct {
 const pg_tools *files;
 void *context;
 int (*probe)(void *,const char *path);
 const pg_tool_registry *tools;
 pg_slice workspace;
} pg_prompt_workspace_source;
/* Pi coding sections: default/custom prefix, declared tools/rules, addendum,
   first context candidate and cwd. Owns no source resources; output <4096.
   Failure clears output/size. Calls are serialized with the file service. */
int pg_prompt_workspace(const pg_prompt_workspace_source *,char *,size_t,size_t *);
#endif
