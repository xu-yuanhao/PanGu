#ifndef PANGU_EXTENSION_REGISTRY_H
#define PANGU_EXTENSION_REGISTRY_H
#include "extensions.h"
/* Owner retains immutable descriptor/data/code on install and releases it only
   after all published snapshots using it close. Callbacks cannot reenter
   mutation; serialized calls return busy instead. NULL owner borrows from the
   enclosing Host provider, which must outlive registry close. */
typedef struct {void *context;int (*retain)(void *);void (*release)(void *);} pg_extension_owner;
typedef struct {pg_extensions facade;void *current;uint64_t revision,epochs;unsigned closed,busy;} pg_extension_registry;
/* Zeroed storage, immutable seed (NULL is empty). 0 success, 3 busy, 5 invalid/
   allocation/owner failure, 7 closed. Failed publication leaves current intact. */
int pg_extension_registry_open(pg_extension_registry *,const pg_extensions *);
int pg_extension_registry_install(void *,const pg_extension *,const pg_extension_owner *);
int pg_extension_registry_remove(void *,const char *);
int pg_extension_registry_describe(void *,struct json_object **);
/* Seals publication, retires current, then 1 while snapshots remain, 0 closed.
   Keep registry/Host storage alive while pending; releases require no polling. */
int pg_extension_registry_close(pg_extension_registry *);
typedef struct {void *context;int (*install)(void *,const pg_extension *,const pg_extension_owner *);int (*remove)(void *,const char *);int (*describe)(void *,struct json_object **);} pg_extension_control;
/* Lookup all installed task definitions independently of Agent selection.
   Success transfers an epoch lease; missing/failure return no lease. */
void pg_extension_registry_release(void *);
int pg_extension_registry_task(void *,pg_slice,const pg_task_definition **,void **);
void pg_extension_registry_service(pg_extension_registry *,pg_extension_control *);
#endif
