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
 const pg_tools *user_files;
 void *user_context;
 int (*user_probe)(void *,const char *path);
 pg_slice user_directory; /* Canonical absolute directory for deduplication. */
 /* Optional complete read/probe rooted at filesystem '/', receiving paths
    relative to that root. Only ancestor context candidates are requested. */
 const pg_tools *ancestor_files;
 void *ancestor_context;
 int (*ancestor_probe)(void *,const char *);
} pg_prompt_workspace_source;
/* Pi coding sections: default/custom prefix, declared tools/rules, addendum,
   user/ancestor/project context candidates and cwd. Project SYSTEM/APPEND overrides
   the corresponding user resource; user instructions precede project ones. Owns no source resources; output <4096.
   Failure clears output/size. Calls are serialized with the file service. */
int pg_prompt_workspace(const pg_prompt_workspace_source *,char *,size_t,size_t *);
#endif
