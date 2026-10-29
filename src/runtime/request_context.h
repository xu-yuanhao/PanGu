#ifndef PANGU_REQUEST_CONTEXT_H
#define PANGU_REQUEST_CONTEXT_H
#include "../contracts/client.h"
struct json_object;
int pg_context_blank(pg_slice);
/* Append UTF-8 JSON text to an owned string, with 0/1/2 LF separator bytes.
   Inputs are borrowed; successful replacement releases the former string. */
int pg_context_append(struct json_object **,const struct json_object *,unsigned);
/* Owned text from a string or text-block array, joined with LF. */
struct json_object *pg_context_text(const struct json_object *);
/* The model encoder requests owned {messages} only. */
int pg_context_messages(const struct json_object *,struct json_object **);
/* Owned {messages,history}: OpenAI messages and flat startup display rows from
   {systemPrompt,messages} canonical context. No borrowed input survives. */
/* Provider configuration input or modelInfo.input advertises image support;
   adjacent tool results collect image parts into one following user message. */
int pg_context_messages_model(const struct json_object *,const struct json_object *,struct json_object **);
int pg_context_project(const struct json_object *,struct json_object **);
#endif
