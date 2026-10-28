#ifndef PANGU_USAGE_H
#define PANGU_USAGE_H
#include "task_runner.h"
/* Stage usage on the caller's open mutation. All inputs are borrowed through
   return. Runner is an active phase and usage is a finite Pi-shaped counter
   object. Mint uses the Record Tx read context. Missing usage is a no-op. */
int pg_usage_record(pg_task_runner *,pg_document_tx *,int (*mint)(void *,uint64_t *),const char *bucket,const char *key,const struct json_object *usage);
int pg_usage_model(pg_task_runner *,pg_document_tx *,int (*mint)(void *,uint64_t *),const struct json_object *configuration,const struct json_object *assistant);
/* Detached current conversation ledger; caller releases with json_object_put. */
int pg_usage_read(pg_durable_session *,uint64_t conversation,struct json_object **);
typedef struct {void *context;int (*read)(void *,uint64_t,struct json_object **);} pg_usage_ops;
/* Binding borrows Session until the Host scope finishes. */
void pg_usage_service(pg_durable_session *,pg_usage_ops *);
#endif
