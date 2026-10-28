#ifndef PANGU_COMPACTION_HOOKS_H
#define PANGU_COMPACTION_HOOKS_H
#include <stdint.h>
#include <stddef.h>
struct json_object;
typedef struct {
 void *context;
 /* 0 completed (NULL decision passes), 1 pending, negative callback error.
    Borrow request until completed/cancel; decision is owned strict JSON:
    {decline:true} or {summary:string}. Repeated pending polls use the same request. */
 int (*before)(void *,const struct json_object *,struct json_object **);
 void (*cancel)(void *,const struct json_object *);
} pg_compaction_hook;
typedef struct {
 uint32_t abi,size;
 const pg_compaction_hook *entries;size_t count;
 void *report_context;
 void (*report)(void *,size_t,int);
} pg_compaction_hooks;
#define PG_COMPACTION_HOOKS_ABI 1u
typedef struct {
 const pg_compaction_hooks *hooks;
 struct json_object *request,*view,*decision;
 size_t index;
 unsigned pending,reserved;
} pg_compaction_hook_state;
int pg_compaction_hook_open(const pg_compaction_hooks *,const struct json_object *,uint64_t,const struct json_object *,pg_compaction_hook_state **);
int pg_compaction_hook_poll(pg_compaction_hook_state *);
void pg_compaction_hook_close(pg_compaction_hook_state *);
_Static_assert(sizeof(pg_compaction_hook)==24&&sizeof(pg_compaction_hooks)==40,"compaction hook ABI");
_Static_assert(sizeof(pg_compaction_hook_state)==48,"compaction hook state layout");
#endif
