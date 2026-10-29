#ifndef PANGU_TASK_PROVIDER_H
#define PANGU_TASK_PROVIDER_H
#include "task_runner.h"
#define PG_TASK_PROVIDER_ABI 1u
/* Optional Host task.definitions service. Definitions and their code are
   immutable/borrowed until the dependent Scheduler closes. Names must be
   distinct from each other and the supplied built-ins. */
typedef struct {uint32_t abi,size;const pg_task_definition *const *definitions;size_t count;} pg_task_provider;
struct pg_extensions_tag;
/* Validate all installed task definitions and built-in name reservations. */
int pg_task_extensions_validate(const struct pg_extensions_tag *);
int pg_task_provider_validate(const pg_task_provider *,const pg_task_definition *const *,size_t);
/* 0 found, 1 missing; borrows the definition, including its plugin code. */
int pg_task_provider_find(const pg_task_provider *,pg_slice,const pg_task_definition **);
#endif
