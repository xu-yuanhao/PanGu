/* DeepSeek Harness 22ddee37 user-approval request/decide audit semantics.
 * Native owner-thread synchronous lifetime; no implicit permission policy. */
#include "internal.h"
#include <stdlib.h>
#include <string.h>
static const char *outcomes[] = {"allowed-once", "rejected", "cancelled", "unavailable"};
static int valid(pg_approval_outcome o) {return o >= PG_APPROVAL_ALLOWED_ONCE && o <= PG_APPROVAL_UNAVAILABLE;}
static pangu_slice slice(yyjson_val *v) {return (pangu_slice){yyjson_get_str(v), yyjson_get_len(v)};}
static int text_ok(pangu_slice v) {return v.data || !v.len;}
static pg_approval_event event_of(yyjson_val *v) {
  pg_approval_event e = {.id=yyjson_get_uint(yyjson_obj_get(v,"id")),
      .decided=yyjson_equals_str(yyjson_obj_get(v,"type"),"decided")};
  if (e.decided) {
    e.outcome=PG_APPROVAL_UNAVAILABLE;
    for (int i=0;i<4;i++) if(yyjson_equals_str(yyjson_obj_get(v,"outcome"),outcomes[i])) e.outcome=(pg_approval_outcome)i;
  } else {
    e.question=(pg_approval_question){slice(yyjson_obj_get(v,"toolName")),slice(yyjson_obj_get(v,"callId")),slice(yyjson_obj_get(v,"reason"))};
  }
  return e;
}
pg_session_status pg_approval_decode(yyjson_val *a, char **out) {
  *out=NULL;
  if (a && !yyjson_is_arr(a)) return PG_SESSION_CORRUPT;
  uint64_t pending=0,last=0;
  size_t i,max; yyjson_val *v;
  yyjson_arr_foreach(a,i,max,v) {
    yyjson_val *id=yyjson_obj_get(v,"id"),*type=yyjson_obj_get(v,"type");
    if (!yyjson_is_obj(v) || !yyjson_is_uint(id) || !yyjson_get_uint(id)) return PG_SESSION_CORRUPT;
    if (yyjson_equals_str(type,"asked")) {
      yyjson_val *tool=yyjson_obj_get(v,"toolName"),*call=yyjson_obj_get(v,"callId"),*reason=yyjson_obj_get(v,"reason");
      if (pending || yyjson_get_uint(id)<=last || !yyjson_is_str(tool) || !yyjson_get_len(tool) ||
          (call && !yyjson_is_str(call)) || (reason && !yyjson_is_str(reason))) return PG_SESSION_CORRUPT;
      pending=last=yyjson_get_uint(id);
    } else if (yyjson_equals_str(type,"decided")) {
      int found=0;
      for(int k=0;k<4;k++) found |= yyjson_equals_str(yyjson_obj_get(v,"outcome"),outcomes[k]);
      if(!pending || pending!=yyjson_get_uint(id) || !found) return PG_SESSION_CORRUPT;
      pending=0;
    } else return PG_SESSION_CORRUPT;
  }
  if(a) *out=yyjson_val_write(a,0,NULL);
  else { *out=malloc(3); if(*out) memcpy(*out,"[]",3); }
  return *out ? PG_SESSION_OK : PG_SESSION_NOMEM;
}
static pg_session_status append(pangu_session *s, uint64_t id,
    const pg_approval_question *q, pg_approval_outcome outcome) {
  yyjson_doc *old=yyjson_read(s->approval_events,strlen(s->approval_events),0);
  yyjson_mut_doc *d=old?yyjson_doc_mut_copy(old,NULL):NULL;
  yyjson_doc_free(old);
  if(!d) return PG_SESSION_NOMEM;
  yyjson_mut_val *v=yyjson_mut_obj(d),*root=yyjson_mut_doc_get_root(d);
  int ok=v && yyjson_mut_obj_add_uint(d,v,"id",id) &&
      yyjson_mut_obj_add_str(d,v,"type",q?"asked":"decided");
  if(q) {
    ok=ok && yyjson_mut_obj_add_strncpy(d,v,"toolName",q->tool_name.data,q->tool_name.len);
    if(q->call_id.data) ok=ok && yyjson_mut_obj_add_strncpy(d,v,"callId",q->call_id.data,q->call_id.len);
    if(q->reason.data) ok=ok && yyjson_mut_obj_add_strncpy(d,v,"reason",q->reason.data,q->reason.len);
  } else ok=ok && yyjson_mut_obj_add_str(d,v,"outcome",outcomes[outcome]);
  ok=ok && yyjson_mut_arr_add_val(root,v);
  char *encoded=ok?yyjson_mut_write(d,0,NULL):NULL;
  yyjson_mut_doc_free(d);
  if(!encoded) return PG_SESSION_NOMEM;
  pg_session_status z=pg_session_begin_internal(s);
  if(z==PG_SESSION_OK) {
    free(s->draft_approval_events); s->draft_approval_events=encoded; encoded=NULL;
    z=pg_session_commit_internal(s);
    if(z!=PG_SESSION_OK) pg_session_rollback_internal(s);
  }
  free(encoded);
  return z;
}
uint64_t pg_session_approval_pending(const pangu_session *s) {
  return s && !s->closed && !s->approval_answered && !s->approval_settling ? s->approval_active : 0;
}
pg_session_status pg_session_approval_reply(pangu_session *s,uint64_t id,pg_approval_outcome outcome) {
  if(!s || s->closed || !id || !valid(outcome)) return PG_SESSION_INVALID;
  if(s->readonly) return PG_SESSION_READ_ONLY;
  if(id!=s->approval_active || s->approval_answered || s->approval_settling) return PG_SESSION_NOT_FOUND;
  if(s->draft) return PG_SESSION_BUSY;
  if(s->cancel_requested) outcome=PG_APPROVAL_CANCELLED;
  s->approval_settling=1;
  pg_session_status z=append(s,id,NULL,outcome);
  s->approval_settling=0;
  if(z==PG_SESSION_OK) {s->approval_result=outcome; s->approval_answered=1;}
  return z;
}
pg_session_status pg_session_approval_request(pangu_session *s,const pg_approval_question *q,
    pg_approval_answerer answer,void *ctx,pg_approval_outcome *out) {
  if(!s || s->closed || !q || !out || !q->tool_name.len || !text_ok(q->tool_name) || !text_ok(q->call_id) || !text_ok(q->reason)) return PG_SESSION_INVALID;
  if(s->readonly) return PG_SESSION_READ_ONLY;
  /* prepared_hook describes the configured next-turn callback for the whole
   * run; it is not an in-flight transaction and must not reject tool approvals.
   * Actual transactions and reentrant observer/approval calls remain blocked. */
  if(!s->running_flag || s->compaction_core || s->dispatch || s->draft || s->approval_active) return PG_SESSION_BUSY;
  pg_session_status recovered=pg_approval_recover(s);
  if(recovered!=PG_SESSION_OK) return recovered;
  if(s->seq==UINT64_MAX) return PG_SESSION_INVALID;
  /* Copy before publication: observers may commit and invalidate borrowed
   * committed-state slices while an interactive answerer pumps the host. */
  yyjson_mut_doc *held=yyjson_mut_doc_new(NULL);
  if(!held) return PG_SESSION_NOMEM;
  yyjson_mut_val *tool=yyjson_mut_strncpy(held,q->tool_name.data,q->tool_name.len),
      *call=q->call_id.data?yyjson_mut_strncpy(held,q->call_id.data,q->call_id.len):NULL,
      *reason=q->reason.data?yyjson_mut_strncpy(held,q->reason.data,q->reason.len):NULL;
  if(!tool || (q->call_id.data && !call) || (q->reason.data && !reason)) {yyjson_mut_doc_free(held);return PG_SESSION_NOMEM;}
  pg_approval_question owned={{yyjson_mut_get_str(tool),yyjson_mut_get_len(tool)},
      {yyjson_mut_get_str(call),yyjson_mut_get_len(call)},{yyjson_mut_get_str(reason),yyjson_mut_get_len(reason)}};
  s->approval_active=s->seq+1; s->approval_answered=0;
  uint64_t id=s->approval_active;
  pg_session_status z=append(s,id,&owned,PG_APPROVAL_UNAVAILABLE);
  if(z==PG_SESSION_OK && !s->approval_answered) {
    pg_approval_outcome result=PG_APPROVAL_UNAVAILABLE;
    if(s->cancel_requested) result=PG_APPROVAL_CANCELLED;
    else if(answer) {
      pg_session_status answered=answer(ctx,s,id,&owned,&result);
      if(answered!=PG_SESSION_OK || !valid(result)) result=PG_APPROVAL_UNAVAILABLE;
    }
    if(!s->approval_answered) z=pg_session_approval_reply(s,id,result);
  }
  if(z==PG_SESSION_OK) *out=s->approval_result;
  else {
    /* Do not let a consumer ignore a failed audit and continue the run. */
    s->run_error=z; pangu_core_cancel(s->running);
  }
  s->approval_active=0; s->approval_answered=0;
  yyjson_mut_doc_free(held);
  return z;
}
pg_session_status pg_session_approval_events(const pangu_session *s,pg_approval_event_sink sink,void *ctx) {
  if(!s || s->closed || !sink) return PG_SESSION_INVALID;
  yyjson_doc *d=yyjson_read(s->approval_events,strlen(s->approval_events),0);
  if(!d) return PG_SESSION_NOMEM;
  size_t i,max; yyjson_val *v; pg_session_status z=PG_SESSION_OK;
  yyjson_arr_foreach(yyjson_doc_get_root(d),i,max,v) {
    pg_approval_event e=event_of(v); z=sink(ctx,&e); if(z!=PG_SESSION_OK) break;
  }
  yyjson_doc_free(d); return z;
}
pg_session_status pg_approval_recover(pangu_session *s) {
  yyjson_doc *d=yyjson_read(s->approval_events,strlen(s->approval_events),0);
  if(!d) return PG_SESSION_NOMEM;
  yyjson_val *a=yyjson_doc_get_root(d),*last=yyjson_arr_get_last(a);
  uint64_t id=last && yyjson_equals_str(yyjson_obj_get(last,"type"),"asked")?yyjson_get_uint(yyjson_obj_get(last,"id")):0;
  yyjson_doc_free(d);
  return id?append(s,id,NULL,PG_APPROVAL_CANCELLED):PG_SESSION_OK;
}
