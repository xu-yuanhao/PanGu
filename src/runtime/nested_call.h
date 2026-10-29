#ifndef PANGU_NESTED_CALL_H
#define PANGU_NESTED_CALL_H
#include "durable_tool.h"
/* Task-scoped family: the caller owns index/result until its terminal commit.
   Native call is immutable {type:"toolCall",id,name,arguments}. start stages
   index and owned ToolTask atomically, returns the existing ID for the same tool/arguments
   and key, rejects a key naming different arguments. */
int pg_nested_call_start(pg_task_runner *,const pg_task_definition *,const char *key,
 const struct json_object *call,int (*mint)(void *,uint64_t *),uint64_t *);
int pg_nested_call_read(pg_durable_session *,uint64_t,const char *,struct json_object **);
int pg_nested_call_document(pg_document_tx *,uint64_t,const char *,int (*mint)(void *,uint64_t *),pg_document_change **);
/* Store result on an already indexed member during child terminal mutation. */
int pg_nested_call_settle(pg_document_tx *,uint64_t,const char *,uint64_t,
 const struct json_object *result,int (*mint)(void *,uint64_t *));
#endif
