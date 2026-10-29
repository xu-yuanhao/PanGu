#ifndef PANGU_TOOL_LIVE_H
#define PANGU_TOOL_LIVE_H
#include "document_tx.h"
/* On the caller's mutation line, upsert sparse fields of a nested tool slot.
   A new slot supplies identity, arguments and status. remove_children removes
   the settling caller's direct children after their results have committed.
   Fields and documents are borrowed; publication follows Storage adoption. */
int pg_tool_live_stage(pg_document_tx *,uint64_t conversation,uint64_t task_id,
 const struct json_object *fields,int (*mint)(void *,uint64_t *),unsigned remove_children);
int pg_tool_live_created(pg_document_tx *,uint64_t,uint64_t,uint64_t,const struct json_object *,int (*mint)(void *,uint64_t *));
#endif
