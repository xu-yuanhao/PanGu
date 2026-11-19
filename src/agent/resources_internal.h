#ifndef PG_AGENT_RESOURCES_INTERNAL_H
#define PG_AGENT_RESOURCES_INTERNAL_H
#include <pangu/agent/app.h>
#include <pangu/services/resources.h>
typedef struct pg_agent_resources_run pg_agent_resources_run;
pg_status pg_agent_resources_prepare(pg_scope *, pangu_session *,
                                     pangu_loop_config *, const pangu_message *,
                                     size_t, pg_agent_resources_run **,
                                     const pangu_message **);
void pg_agent_resources_enter(pg_agent_resources_run *);
void pg_agent_resources_release(pg_agent_resources_run *);
pg_status pg_agent_resources_expand_active(const pg_agent_resources_run *,
                                           pangu_slice, char **, size_t *);
pg_status pg_agent_resources_reload(pg_scope *);
pg_status pg_agent_resources_expand(pg_scope *, pangu_slice, char **, size_t *);
#endif
