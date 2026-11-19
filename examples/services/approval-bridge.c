/* Integration recipe, compiled with -fsyntax-only by the services owner.
 * App owns this context through wrapper disposal; no Agent link in services.
 * Configure wrapper ASK here: runtime commits asked/decided around answerer.
 * Install/hold aggregate -> approval wrapper -> Core. Never expose raw aggregate
 * alongside wrapper, as that would bypass approval.
 */
#include <pangu/agent/approval.h>
#include <pangu/services/approval.h>
typedef struct {
  pangu_session *session;
  pg_scope *answerer_scope; /* exact agent scope, not another agent's UI */
  pangu_core *active_core;
} pg_example_approval_bridge;
static pg_session_status answer(void *ctx,pangu_session *session,uint64_t id,
    const pg_approval_question *q,pg_approval_outcome *out) {
  (void)session; (void)id;
  pg_example_approval_bridge *b=ctx;
  pg_service_approval_question sq={q->tool_name,q->call_id,q->reason};
  pg_service_approval_outcome result=PG_SERVICE_UNAVAILABLE;
  pg_status st=pg_approval_ask(b->answerer_scope,b->active_core,&sq,&result);
  *out=PG_APPROVAL_UNAVAILABLE;
  if(st!=PG_OK) return st==PG_NOMEM?PG_SESSION_NOMEM:PG_SESSION_INVALID;
  switch(result) {
    case PG_SERVICE_ALLOWED_ONCE: *out=PG_APPROVAL_ALLOWED_ONCE; break;
    case PG_SERVICE_REJECTED: *out=PG_APPROVAL_REJECTED; break;
    case PG_SERVICE_CANCELLED: *out=PG_APPROVAL_CANCELLED; break;
    case PG_SERVICE_UNAVAILABLE: break;
    default: return PG_SESSION_INVALID;
  }
  return PG_SESSION_OK;
}
pg_status pg_example_approval_request(void *ctx,pangu_core *core,
    const pg_service_approval_question *q,pg_service_approval_outcome *out) {
  pg_example_approval_bridge *b=ctx;
  *out=PG_SERVICE_UNAVAILABLE;
  if(b->active_core) return PG_FAILED; /* no nested durable request */
  b->active_core=core;
  pg_approval_question aq={q->tool_name,q->call_id,q->reason};
  pg_approval_outcome result=PG_APPROVAL_UNAVAILABLE;
  pg_session_status st=pg_session_approval_request(b->session,&aq,answer,b,&result);
  b->active_core=NULL;
  if(st!=PG_SESSION_OK) return st==PG_SESSION_NOMEM?PG_NOMEM:PG_FAILED;
  switch(result) {
    case PG_APPROVAL_ALLOWED_ONCE: *out=PG_SERVICE_ALLOWED_ONCE; break;
    case PG_APPROVAL_REJECTED: *out=PG_SERVICE_REJECTED; break;
    case PG_APPROVAL_CANCELLED: *out=PG_SERVICE_CANCELLED; break;
    case PG_APPROVAL_UNAVAILABLE: break;
    default: return PG_FAILED;
  }
  return PG_OK; /* only after Agent decision committed */
}
