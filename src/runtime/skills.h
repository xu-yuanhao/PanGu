#ifndef PANGU_SKILLS_H
#define PANGU_SKILLS_H
#include <stddef.h>
#include "../contracts/client.h"
struct json_object;
/* Absolute skills roots, user first; NULL omits a root. Success returns an owned
   {skills:[],diagnostics:[]} snapshot. Free with json_object_put. Synchronous;
   roots and all output storage are disjoint. Failure leaves *out NULL.
   Symlink identities are deduplicated; traversal is bounded at 64 levels. */
int pg_skills_load(const char *,const char *,struct json_object **out);
/* reader 0=read, 1=bash, 2=indirect, 3=unavailable. XML escapes all resource data.
   Caller owns disjoint output; failure clears output and size. */
int pg_skills_prompt(const struct json_object *,unsigned reader,char *,size_t,size_t *);
/* Shared Markdown reader: owned YAML scalar metadata plus normalized body.
   At most 1MiB, BOM/CRLF normalized, frontmatter removed and its body trimmed. */
int pg_resource_document(const char *,struct json_object **);
size_t pg_resource_space(const char *,size_t);
pg_slice pg_resource_trim(pg_slice);
/* POSIX directory/stat and libyaml layout bridges; owned JSON on success. */
int pg_skills_directory(const char *,struct json_object **);
int pg_skills_yaml(const char *,size_t,struct json_object **);
#endif
