#include "src/t14-abi/lifecycle.h"
static pangu_i32 pangu_owner_init(pangu_owner *,pangu_owned_resource *,pangu_u64,pangu_owned_operation *,pangu_u64,pangu_u64);
static pangu_i32 pangu_owner_register(pangu_owner *,pangu_release_fn,void *);
static pangu_i32 pangu_owner_enter_slot(pangu_owner *,pangu_u32,pangu_u64,pangu_operation_ticket *);
static pangu_i32 pangu_owner_leave(pangu_owner *,const pangu_operation_ticket *);
static pangu_i32 pangu_owner_finish_load(pangu_owner *,pangu_u64,pangu_i32);
static pangu_i32 pangu_owner_register_async(pangu_owner *,pangu_async_release_fn,void *);
static pangu_i32 pangu_owner_cleanup_finish(pangu_owner *,const pangu_cleanup_ticket *,pangu_i32);
static pangu_i32 pangu_owner_cleanup_scope(pangu_owner *,pangu_cleanup_ticket *);
static pangu_i32 pangu_owner_stop_budget(pangu_owner *,pangu_u64,pangu_lifecycle_report *);
static pangu_i32 pangu_owner_call_ready(pangu_owner *,pangu_u32,const pangu_cleanup_ticket *);
static pangu_i32 pangu_owner_enter_cleanup_slot(pangu_owner *,const pangu_cleanup_ticket *,pangu_u64,pangu_operation_ticket *);
static pangu_i32 pangu_owner_stop(pangu_owner *);
static pangu_i32 runtime_bind_geometry(pangu_lifecycle_runtime *,pangu_lifecycle_span *,pangu_u64,pangu_u64);
static pangu_i32 runtime_start_budget(pangu_lifecycle_runtime *,pangu_u64,pangu_lifecycle_report *);
static pangu_i32 runtime_pump_budget(pangu_lifecycle_runtime *,pangu_u64,pangu_lifecycle_report *);
static pangu_i32 runtime_stop_budget(pangu_lifecycle_runtime *,pangu_u64,pangu_lifecycle_report *);
static pangu_i32 runtime_setup_scope(pangu_lifecycle_runtime *,pangu_lifecycle_setup_ticket *);
static pangu_i32 runtime_setup_defer(pangu_lifecycle_runtime *,pangu_lifecycle_setup_ticket *);
static pangu_i32 runtime_setup_finish(pangu_lifecycle_runtime *,const pangu_lifecycle_setup_ticket *,pangu_i32);
static pangu_i32 runtime_disposer_finish(pangu_lifecycle_runtime *,pangu_u32,const pangu_cleanup_ticket *,pangu_i32);
static void runtime_completion_wake(pangu_lifecycle_runtime *,const pangu_owner *);
static pangu_i32 runtime_prepare_lifecycle(const pangu_prepare_result *,const pangu_graph_output *,const pangu_lifecycle_item *,pangu_u64,const pangu_lifecycle_storage *,pangu_u64,pangu_lifecycle_prepared *);
static pangu_i32 runtime_call_finish(pangu_lifecycle_runtime *,const pangu_lifecycle_call_ticket *);
static pangu_i32 runtime_call_enter_budget(pangu_lifecycle_runtime *,pangu_u32,pangu_u64,pangu_lifecycle_call_ticket *,pangu_lifecycle_report *);
static pangu_i32 runtime_call_enter_setup_budget(pangu_lifecycle_runtime *,pangu_u32,const pangu_lifecycle_setup_ticket *,pangu_u64,pangu_lifecycle_call_ticket *,pangu_lifecycle_report *);
static pangu_i32 runtime_call_enter_cleanup_budget(pangu_lifecycle_runtime *,pangu_u32,const pangu_cleanup_ticket *,pangu_u64,pangu_lifecycle_call_ticket *,pangu_lifecycle_report *);
static pangu_i32 facade_owner_init(pangu_owner *,pangu_owned_resource *,pangu_u64,pangu_owned_operation *,pangu_u64,pangu_u64,pangu_u64);
static pangu_i32 facade_register(pangu_lifecycle_runtime *,pangu_u32,pangu_release_fn,void *);
static pangu_i32 facade_register_async(pangu_lifecycle_runtime *,pangu_u32,pangu_async_release_fn,void *);
static pangu_i32 facade_cleanup_scope(pangu_lifecycle_runtime *,pangu_u32,pangu_cleanup_ticket *);
static inline int geo_range(const void *p,pangu_u64 count,pangu_u64 stride,pangu_u64 align)
{
 pangu_u64 a=(pangu_u64)p;return count<=~(pangu_u64)0/stride && (!count || (p && !(a&(align-1)) && count*stride<=~(pangu_u64)0-a));
}
static inline int geo_overlap(const void *a,pangu_u64 n,const void *b,pangu_u64 m)
{
 pangu_u64 x=(pangu_u64)a,y=(pangu_u64)b;return n && m && (x<=y ? y-x<n : x-y<m);
}
static inline int geo_external(const pangu_lifecycle_runtime *r,const void *p,pangu_u64 bytes,pangu_u64 align)
{
 if(!geo_range(r,1,sizeof(*r),8) || !geo_range(p,bytes,1,align)) return 0;
 if(!r->protected_spans) return !geo_overlap(p,bytes,r,sizeof(*r));
 pangu_u64 lo=0,hi=r->protected_count,a=(pangu_u64)p;
 while(lo<hi) {pangu_u64 mid=lo+(hi-lo)/2;if(r->protected_spans[mid].end<=a) lo=mid+1;else hi=mid;}
 return lo==r->protected_count || !bytes || r->protected_spans[lo].begin>=a+bytes;
}
#define UMAX (~(pangu_u64)0)
static int range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
static pangu_i32 valid(const pangu_owner *o)
{
 if(!o || ((pangu_u64)o&7U) || !range_ok(o,104) || o->abi_version!=1 || o->struct_size!=104 || o->state<1 || o->state>4 || o->busy>1 || o->cleanup_waiting>1 || o->cleanup_completed>1 || o->cleanup_callback>1 || !o->epoch || o->count>o->capacity || o->inflight>o->operation_capacity || o->capacity>UMAX/24 || o->operation_capacity>0xffffffffU || (o->capacity && ((pangu_u64)o->resources&7U)) || !range_ok(o->resources,o->capacity*24) || (o->operation_capacity && ((pangu_u64)o->operations&3U)) || !range_ok(o->operations,o->operation_capacity*8)) return 1;
 if(overlap((pangu_u64)o,104,(pangu_u64)o->resources,o->capacity*24) || overlap((pangu_u64)o,104,(pangu_u64)o->operations,o->operation_capacity*8) || overlap((pangu_u64)o->resources,o->capacity*24,(pangu_u64)o->operations,o->operation_capacity*8)) return 1;return 0;
}
pangu_i32 pangu_owner_init(pangu_owner *o,pangu_owned_resource *r,pangu_u64 capacity,pangu_owned_operation *ops,pangu_u64 operation_capacity,pangu_u64 epoch)
{
 if(!o || ((pangu_u64)o&7U) || !range_ok(o,104) || capacity>UMAX/24 || operation_capacity>0xffffffffU || !epoch || (capacity && ((pangu_u64)r&7U)) || !range_ok(r,capacity*24) || (operation_capacity && ((pangu_u64)ops&3U)) || !range_ok(ops,operation_capacity*8) || overlap((pangu_u64)o,104,(pangu_u64)r,capacity*24) || overlap((pangu_u64)o,104,(pangu_u64)ops,operation_capacity*8) || overlap((pangu_u64)r,capacity*24,(pangu_u64)ops,operation_capacity*8)) return 1;
 if(o->abi_version) {if(valid(o) || o->busy || o->state!=PANGU_OWNER_DISPOSED || o->count || o->inflight || epoch<=o->epoch) return 10;}
 else {const unsigned char *p=(const unsigned char *)o;for(pangu_u32 i=0;i<104;i++) if(p[i]) return 1;}
 for(pangu_u64 i=0;i<operation_capacity;i++) {ops[i].generation=0;ops[i].active=0;}
 o->abi_version=1;o->struct_size=104;o->state=PANGU_OWNER_LOADING;o->busy=0;o->resources=r;o->capacity=capacity;o->count=0;o->operations=ops;o->operation_capacity=operation_capacity;o->inflight=0;o->epoch=epoch;o->load_error=0;o->cleanup_error=0;o->cleanup_serial=0;o->cleanup_waiting=0;o->cleanup_completed=0;o->cleanup_status=0;o->cleanup_callback=0;return 0;
}
pangu_i32 pangu_owner_register(pangu_owner *o,pangu_release_fn fn,void *context)
{
 if(valid(o) || !fn) return 1;if(o->state!=PANGU_OWNER_LOADING && o->state!=PANGU_OWNER_ACTIVE) return 10;if(o->busy) return 5;if(o->count==o->capacity) return 2;
 o->resources[o->count].release=fn;o->resources[o->count].context=context;o->resources[o->count].begin=(void *)0;o->count++;return 0;
}



static int ticket_output_valid(pangu_owner *,const void *);
pangu_i32 pangu_owner_enter_slot(pangu_owner *o,pangu_u32 setup,pangu_u64 index,pangu_operation_ticket *ticket)
{
 if(valid(o) || setup>1 || !ticket_output_valid(o,ticket)) return 1;
 if(o->busy || (o->state!=PANGU_OWNER_ACTIVE && !(setup && o->state==PANGU_OWNER_LOADING))) return 10;
 if(index>=o->operation_capacity || o->inflight==o->operation_capacity || o->operations[index].active || o->operations[index].generation==0xffffffffU) return 2;
 o->operations[index].generation++;o->operations[index].active=1;o->inflight++;ticket->owner=o;ticket->epoch=o->epoch;ticket->index=(pangu_u32)index;ticket->generation=o->operations[index].generation;return 0;
}
pangu_i32 pangu_owner_leave(pangu_owner *o,const pangu_operation_ticket *ticket)
{
 if(valid(o) || !ticket || ((pangu_u64)ticket&7U) || !range_ok(ticket,24) || overlap((pangu_u64)ticket,24,(pangu_u64)o,104) || overlap((pangu_u64)ticket,24,(pangu_u64)o->operations,o->operation_capacity*8) || overlap((pangu_u64)ticket,24,(pangu_u64)o->resources,o->capacity*24)) return 1;
 if(ticket->owner!=o || ticket->epoch!=o->epoch || ticket->index>=o->operation_capacity || !ticket->generation) return 10;pangu_owned_operation *op=&o->operations[ticket->index];
 if(!op->active || op->generation!=ticket->generation || !o->inflight) return 10;op->active=0;o->inflight--;return 0;
}
pangu_i32 pangu_owner_register_async(pangu_owner *o,pangu_async_release_fn fn,void *context)
{
 if(valid(o) || !fn) return 1;if(o->state!=PANGU_OWNER_LOADING && o->state!=PANGU_OWNER_ACTIVE) return 10;if(o->busy) return 5;if(o->count==o->capacity) return 2;
 o->resources[o->count].release=(void *)0;o->resources[o->count].context=context;o->resources[o->count].begin=fn;o->count++;return 0;
}
static int scope_valid(pangu_owner *o,const pangu_cleanup_ticket *scope)
{
 return !valid(o) && scope && scope->owner==o && scope->epoch==o->epoch && scope->serial && scope->serial==o->cleanup_serial && o->state==PANGU_OWNER_QUIESCING && !(o->cleanup_waiting && o->cleanup_completed) && (o->cleanup_callback || o->cleanup_waiting);
}
static int ticket_output_valid(pangu_owner *o,const void *ticket)
{
 return ticket && !((pangu_u64)ticket&7U) && range_ok(ticket,24) && !overlap((pangu_u64)ticket,24,(pangu_u64)o,104) && !overlap((pangu_u64)ticket,24,(pangu_u64)o->resources,o->capacity*24) && !overlap((pangu_u64)ticket,24,(pangu_u64)o->operations,o->operation_capacity*8);
}
pangu_i32 pangu_owner_cleanup_scope(pangu_owner *o,pangu_cleanup_ticket *scope)
{
 if(valid(o) || !ticket_output_valid(o,scope)) return 1;
 pangu_cleanup_ticket current={o,o->epoch,o->cleanup_serial};if(!scope_valid(o,&current)) return 10;*scope=current;return 0;
}

pangu_i32 pangu_owner_cleanup_finish(pangu_owner *o,const pangu_cleanup_ticket *ticket,pangu_i32 status)
{
 if(valid(o) || !ticket || ((pangu_u64)ticket&7U) || !range_ok(ticket,24) || overlap((pangu_u64)ticket,24,(pangu_u64)o,104) || overlap((pangu_u64)ticket,24,(pangu_u64)o->resources,o->capacity*24) || overlap((pangu_u64)ticket,24,(pangu_u64)o->operations,o->operation_capacity*8)) return 1;
 if(ticket->owner!=o || ticket->epoch!=o->epoch || !ticket->serial || ticket->serial!=o->cleanup_serial || !o->cleanup_waiting || o->cleanup_completed || o->state!=PANGU_OWNER_QUIESCING) return 10;
 o->cleanup_status=status;o->cleanup_completed=1;return 0;
}
static void completed(pangu_owner *o)
{
 if(o->cleanup_status && !o->cleanup_error) o->cleanup_error=o->cleanup_status;
 o->cleanup_waiting=0;o->cleanup_completed=0;o->cleanup_status=0;
}
static pangu_i32 owner_report(pangu_owner *o,pangu_lifecycle_report *report,pangu_u64 initial,pangu_u64 fuel,pangu_u32 reason,pangu_i32 rc)
{
 report->work=initial-fuel;report->state=o->state;report->pending=o->state!=PANGU_OWNER_DISPOSED;report->reason=reason;report->reserved=0;return rc;
}
pangu_i32 pangu_owner_stop_budget(pangu_owner *o,pangu_u64 fuel,pangu_lifecycle_report *report)
{
 if(valid(o) || !ticket_output_valid(o,report)) return 1;pangu_u64 initial=fuel;
 #define RESULT(reason,rc) return owner_report(o,report,initial,fuel,reason,rc)
 if(o->state==PANGU_OWNER_DISPOSED) {RESULT(0,o->cleanup_error);}
 if(!fuel) {RESULT(1,2);}
 if(o->state!=PANGU_OWNER_QUIESCING) {fuel--;o->state=PANGU_OWNER_QUIESCING;}
 if(o->busy || o->inflight) {RESULT(2,5);}
 if(o->cleanup_waiting && !o->cleanup_completed) {RESULT(2,5);}
 if(o->cleanup_waiting) {if(!fuel) {RESULT(1,2);}fuel--;completed(o);}
 o->busy=1;
 while(o->count) {
  if(!fuel) {o->busy=0;RESULT(1,2);}fuel--;
  o->count--;pangu_owned_resource resource=o->resources[o->count];
  o->resources[o->count].release=(void *)0;o->resources[o->count].context=(void *)0;o->resources[o->count].begin=(void *)0;o->cleanup_serial++;
  if(resource.begin) {
   o->cleanup_waiting=1;o->cleanup_completed=0;o->cleanup_status=0;pangu_cleanup_ticket ticket={o,o->epoch,o->cleanup_serial};
   o->cleanup_callback=1;pangu_i32 rc=resource.begin(resource.context,&ticket);o->cleanup_callback=0;
   if(rc!=5) {o->cleanup_status=o->cleanup_completed ? 10 : rc;o->cleanup_completed=1;}
   if(!o->cleanup_completed) {o->busy=0;RESULT(2,5);}completed(o);
  } else {
   o->cleanup_callback=1;pangu_i32 rc=resource.release ? resource.release(resource.context) : 10;o->cleanup_callback=0;
   if(rc && !o->cleanup_error) o->cleanup_error=rc;
  }
  if(o->inflight) {o->busy=0;RESULT(2,5);}
 }
 o->busy=0;if(!fuel) {RESULT(1,2);}fuel--;o->state=PANGU_OWNER_DISPOSED;RESULT(0,o->cleanup_error);
 #undef RESULT
}
pangu_i32 pangu_owner_stop(pangu_owner *o)
{
 pangu_lifecycle_report report;return pangu_owner_stop_budget(o,UMAX,&report);
}
pangu_i32 pangu_owner_finish_load(pangu_owner *o,pangu_u64 epoch,pangu_i32 status)
{
 if(valid(o)) return 1;if(epoch!=o->epoch) return 10;if(o->state==PANGU_OWNER_QUIESCING || o->state==PANGU_OWNER_DISPOSED) return 6;
 if(o->state!=PANGU_OWNER_LOADING) return 10;if(!status) {o->state=PANGU_OWNER_ACTIVE;return 0;}
 o->load_error=status;pangu_i32 rc=pangu_owner_stop(o);return rc==5 && o->state!=PANGU_OWNER_DISPOSED ? 5 : status;
}

pangu_i32 pangu_owner_call_ready(pangu_owner *o,pangu_u32 mode,const pangu_cleanup_ticket *scope)
{
 if(valid(o) || mode>2) return 1;
 if(mode==2) {if(!ticket_output_valid(o,scope)) return 1;return scope_valid(o,scope) ? 0 : 10;}
 return o->busy || (o->state!=PANGU_OWNER_ACTIVE && !(mode && o->state==PANGU_OWNER_LOADING)) ? 10 : 0;
}
pangu_i32 pangu_owner_enter_cleanup_slot(pangu_owner *o,const pangu_cleanup_ticket *scope,pangu_u64 index,pangu_operation_ticket *ticket)
{
 if(valid(o) || !ticket_output_valid(o,scope) || !ticket_output_valid(o,ticket) || overlap((pangu_u64)scope,24,(pangu_u64)ticket,24)) return 1;
 if(!scope_valid(o,scope)) return 10;
 if(index>=o->operation_capacity || o->inflight==o->operation_capacity || o->operations[index].active || o->operations[index].generation==0xffffffffU) return 2;
 o->operations[index].generation++;o->operations[index].active=1;o->inflight++;ticket->owner=o;ticket->epoch=o->epoch;ticket->index=(pangu_u32)index;ticket->generation=o->operations[index].generation;return 0;
}

#undef STEP
#undef UMAX

pangu_i32 runtime_setup_scope(pangu_lifecycle_runtime *r,pangu_lifecycle_setup_ticket *scope)
{
 if(!geo_external(r,scope,sizeof(*scope),8)) return 1;
 if(r->phase!=1 || r->stop_requested || !r->started || !(r->callback_open || (r->setup_waiting && !r->setup_completed))) return 10;
 scope->runtime=r;scope->operation=r->setup_operation;return 0;
}
pangu_i32 runtime_setup_defer(pangu_lifecycle_runtime *r,pangu_lifecycle_setup_ticket *ticket)
{
 if(!geo_external(r,ticket,sizeof(*ticket),8)) return 1;
 if(!r->busy || !r->callback_open || r->setup_waiting || !r->started || r->phase!=1 || r->stop_requested) return 10;
 r->setup_waiting=1;r->setup_completed=0;r->setup_status=0;ticket->runtime=r;ticket->operation=r->setup_operation;return 0;
}
pangu_i32 runtime_setup_finish(pangu_lifecycle_runtime *r,const pangu_lifecycle_setup_ticket *ticket,pangu_i32 status)
{
 if(!geo_external(r,ticket,sizeof(*ticket),8)) return 1;
 if(ticket->runtime!=r || !r->setup_waiting || r->setup_completed || !r->started) return 10;
 const pangu_operation_ticket *a=&ticket->operation,*b=&r->setup_operation;
 if(a->owner!=b->owner || a->epoch!=b->epoch || a->index!=b->index || a->generation!=b->generation) return 10;
 const pangu_owner *o=b->owner;
 if(o->epoch!=b->epoch || b->index>=o->operation_capacity || !o->operations[b->index].active || o->operations[b->index].generation!=b->generation) return 10;
 r->setup_status=status;r->setup_completed=1;runtime_completion_wake(r,o);return 0;
}
/* Consume tracked setup only after callback returned and explicit completion.
 * No cleanup can run while callback_open or the operation ticket remains live. */
static void complete_setup(pangu_lifecycle_runtime *r,pangu_i32 status)
{
 pangu_owner *o=(pangu_owner *)r->setup_operation.owner;
 pangu_i32 rc=pangu_owner_leave(o,&r->setup_operation);if(!status && rc) status=rc;
 r->setup_waiting=0;r->setup_completed=0;r->setup_status=0;
 if(status) {if(!o->load_error) o->load_error=status;if(!r->load_error) r->load_error=status;}
 if(!r->load_error && !r->stop_requested) {
  rc=pangu_owner_finish_load(o,r->setup_operation.epoch,0);
  if(rc) {o->load_error=rc;r->load_error=rc;}
 }
}
static pangu_i32 runtime_report(pangu_lifecycle_runtime *r,pangu_lifecycle_report *report,pangu_u64 initial,pangu_u64 fuel,pangu_u32 reason,pangu_i32 rc)
{
 report->work=initial-fuel;report->state=r->phase;report->pending=r->phase!=4;report->reason=reason;report->reserved=0;return rc;
}
pangu_i32 runtime_stop_budget(pangu_lifecycle_runtime *r,pangu_u64 fuel,pangu_lifecycle_report *report)
{
 if(!geo_external(r,report,sizeof(*report),8)) return 1;pangu_u64 initial=fuel;
 #define RESULT(reason,rc) return runtime_report(r,report,initial,fuel,reason,rc)
 if(r->phase==4) {RESULT(0,r->cleanup_error);}
 if(!fuel) {RESULT(1,2);}
 if(!r->stop_requested) {
  fuel--;r->stop_requested=1;
  /* One constant-time cancellation gate transition closes both pangu_lifecycle_runtime business
   * admission and the tracked setup owner's registration, before yielding. */
  if((r->callback_open || r->setup_waiting) && r->started) ((pangu_owner *)r->setup_operation.owner)->state=PANGU_OWNER_QUIESCING;
 }
 if((r->callback_open || r->setup_waiting) && r->started) {
  pangu_lifecycle_report owner={0};(void)pangu_owner_stop_budget((pangu_owner *)r->setup_operation.owner,fuel,&owner);fuel-=owner.work;
  if(!owner.state) {RESULT(2,1);}if(owner.reason==1) {RESULT(1,2);}
 }
 if(r->busy) {RESULT(2,5);}
 if(r->setup_waiting) {
  if(!r->setup_completed) {if(!r->stop_graph) {RESULT(2,5);}}
  else {if(!fuel) {RESULT(1,2);}fuel--;r->busy=1;complete_setup(r,r->setup_status);r->busy=0;}
 }
 if(r->phase!=3) {if(!fuel) {RESULT(1,2);}fuel--;r->phase=3;}
 if(!r->started) {if(!fuel) {RESULT(1,2);}fuel--;r->phase=4;r->stop_rescan=0;r->stop_pass_active=0;r->stop_scan_active=0;r->stop_cursor=0;r->stop_edge_cursor=0;RESULT(0,r->cleanup_error);}
 r->busy=1;
 if(r->stop_graph) {
  const pangu_lifecycle_graph *g=r->stop_graph;
  if(!r->stop_pass_active) {r->stop_cursor=r->launched;r->stop_edge_cursor=0;r->stop_scan_active=0;r->stop_pass_active=1;}
  while(r->stop_cursor || r->stop_rescan) {
   if(r->stop_rescan) {
    if(!fuel) {r->busy=0;RESULT(1,2);}fuel--;r->stop_rescan=0;r->stop_cursor=r->launched;r->stop_edge_cursor=0;r->stop_scan_active=0;
   }
   if(!r->stop_cursor) break;
   pangu_u64 i=r->stop_cursor-1;
   if(!r->stop_scan_active) {
    if(!fuel) {r->busy=0;RESULT(1,2);}fuel--;
    if(g->disposed[i]) {r->stop_cursor--;continue;}
    r->stop_scan_active=1;r->stop_edge_cursor=0;
   }
   pangu_u32 blocked=0;
   while(r->stop_edge_cursor<g->edge_count) {
    if(!fuel) {r->busy=0;RESULT(1,2);}fuel--;pangu_lifecycle_edge e=g->edges[r->stop_edge_cursor++];
    if(e.provider==i && e.consumer<r->launched && !g->disposed[e.consumer]) {blocked=1;break;}
   }
   if(blocked) {r->stop_cursor--;r->stop_scan_active=0;continue;}
   pangu_lifecycle_report owner={0};pangu_i32 rc=pangu_owner_stop_budget(r->items[i].owner,fuel,&owner);fuel-=owner.work;
   if(!owner.state) {r->busy=0;RESULT(2,1);}
   if(owner.reason==1) {r->busy=0;RESULT(1,2);}
   if(!owner.pending) {if(rc && !r->cleanup_error) r->cleanup_error=rc;g->disposed[i]=1;r->started--;}
   r->stop_cursor--;r->stop_scan_active=0;
  }
  r->stop_pass_active=0;r->busy=0;if(r->started) {RESULT(2,5);}
 } else {
  while(r->started) {
   pangu_lifecycle_report owner={0};pangu_i32 rc=pangu_owner_stop_budget(r->items[r->started-1].owner,fuel,&owner);fuel-=owner.work;
   if(!owner.state) {r->busy=0;RESULT(2,1);}if(owner.reason==1) {r->busy=0;RESULT(1,2);}
   if(owner.pending) {r->busy=0;RESULT(2,5);}if(rc && !r->cleanup_error) r->cleanup_error=rc;r->started--;
  }
  r->busy=0;
 }
 if(!fuel) {RESULT(1,2);}fuel--;r->phase=4;r->stop_rescan=0;r->stop_pass_active=0;r->stop_scan_active=0;r->stop_cursor=0;r->stop_edge_cursor=0;RESULT(0,r->cleanup_error);
 #undef RESULT
}

static pangu_i32 start_report(pangu_lifecycle_runtime *r,pangu_lifecycle_report *report,pangu_u64 initial,pangu_u64 fuel,pangu_u32 reason,pangu_i32 rc)
{
 report->work=initial-fuel;report->state=r->phase;report->pending=r->phase!=2 && r->phase!=4 && r->phase!=6;report->reason=reason;report->reserved=0;return rc;
}
static pangu_i32 settle_budget(pangu_lifecycle_runtime *r,pangu_u64 initial,pangu_u64 fuel,pangu_lifecycle_report *report)
{
 pangu_lifecycle_report stopped={0};pangu_i32 rc=runtime_stop_budget(r,fuel,&stopped);*report=stopped;report->work+=initial-fuel;
 if(report->pending) return rc;return r->load_error ? r->load_error : (rc ? rc : 6);
}
pangu_i32 runtime_pump_budget(pangu_lifecycle_runtime *r,pangu_u64 fuel,pangu_lifecycle_report *report)
{
 if(!geo_external(r,report,sizeof(*report),8) || !r->protected_spans) return 1;pangu_u64 initial=fuel;
 #define RESULT(reason,rc) return start_report(r,report,initial,fuel,reason,rc)
 #define REQUIRE() do {if(!fuel) {r->busy=0;RESULT(1,2);}fuel--;} while(0)
 #define INVALID(rc) do {r->busy=0;r->load_error=rc;r->phase=6;RESULT(0,rc);} while(0)
 if(r->busy) {RESULT(2,5);}if(r->phase==0) return 10;
 if(r->phase==3 || r->phase==4 || r->stop_requested) return settle_budget(r,initial,fuel,report);
 if(r->phase==2) {RESULT(0,0);}if(r->phase==6) {RESULT(0,r->load_error);}
 if(r->phase!=1 && r->phase!=5) return 10;if(!fuel) {RESULT(1,2);}r->busy=1;
 if(r->phase==5) {
  const pangu_lifecycle_graph *g=r->stop_graph;
  if(g && (g->capacity<r->count || (!g->disposed && r->count) || (!g->edges && g->edge_count) || g->edge_count>0xffffffffU)) {INVALID(1);}
  if(r->validate_stage==0) {
   if(g) while(r->validate_cursor<r->count) {REQUIRE();if(g->disposed[r->validate_cursor]) {INVALID(10);}r->validate_cursor++;}
   r->validate_stage=1;r->validate_cursor=0;
  }
  if(r->validate_stage==1) {
   if(g) while(r->validate_cursor<g->edge_count) {REQUIRE();pangu_lifecycle_edge e=g->edges[r->validate_cursor++];if(e.consumer>=r->count || e.provider>=e.consumer) {INVALID(16);}}
   r->validate_stage=2;r->validate_cursor=0;
  }
  if(r->validate_stage==2) {
   while(r->validate_cursor<r->count) {
    pangu_owner *o=r->items[r->validate_cursor].owner;
    if(!r->validate_owner_checked) {
     REQUIRE();if(!o || !r->items[r->validate_cursor].setup || o->abi_version!=1 || o->struct_size!=104 || o->state!=PANGU_OWNER_LOADING || o->busy || o->inflight || o->count || !o->operation_capacity) {INVALID(1);}
     r->validate_owner_checked=1;r->validate_peer=0;
    }
    while(r->validate_peer<r->validate_cursor) {REQUIRE();if(r->items[r->validate_peer++].owner==o) {INVALID(1);}}
    r->validate_cursor++;r->validate_owner_checked=0;
   }
   r->validate_stage=3;
  }
  REQUIRE();r->phase=1;
 }
 if(r->setup_waiting) {
  if(!r->setup_completed) {r->busy=0;RESULT(2,5);}REQUIRE();complete_setup(r,r->setup_status);
 }
 while(!r->load_error && !r->stop_requested && r->started<r->count) {
  pangu_lifecycle_item *item=&r->items[r->started];pangu_owner *o=item->owner;
  if(!r->entry_ready) {
   while(r->entry_cursor<o->operation_capacity) {
    REQUIRE();if(!o->operations[r->entry_cursor].active && o->operations[r->entry_cursor].generation!=0xffffffffU) {r->entry_ready=1;break;}r->entry_cursor++;
   }
   if(!r->entry_ready) {r->load_error=2;break;}
  }
  REQUIRE();pangu_i32 rc=pangu_owner_enter_slot(o,1,r->entry_cursor,&r->setup_operation);r->entry_ready=0;r->entry_cursor=0;
  if(rc) {r->load_error=rc;break;}
  r->started++;r->launched=r->started;r->callback_open=1;rc=item->setup(o,item->context);r->callback_open=0;
  if(r->setup_waiting) {
   if(rc!=5) {r->load_error=10;r->stop_requested=1;o->state=PANGU_OWNER_QUIESCING;o->load_error=10;}
   if(!r->setup_completed) {r->busy=0;if(r->stop_requested) return settle_budget(r,initial,fuel,report);RESULT(2,5);}
   complete_setup(r,r->setup_status);
  } else complete_setup(r,rc);
 }
 r->busy=0;if(r->load_error || r->stop_requested) return settle_budget(r,initial,fuel,report);
 REQUIRE();r->phase=2;RESULT(0,0);
 #undef INVALID
 #undef REQUIRE
 #undef RESULT
}
pangu_i32 runtime_start_budget(pangu_lifecycle_runtime *r,pangu_u64 fuel,pangu_lifecycle_report *report)
{
 if(!geo_external(r,report,sizeof(*report),8) || !r->protected_spans || (!r->items && r->count) || r->count>0xffffffffU) return 1;
 if(r->phase || r->busy || r->started || r->stop_requested || r->load_error || r->cleanup_error || r->setup_waiting || r->setup_completed || r->callback_open || r->reserved || r->launched || r->stop_cursor || r->stop_edge_cursor || r->stop_scan_active || r->stop_pass_active || r->validate_cursor || r->validate_peer || r->entry_cursor || r->validate_stage || r->validate_owner_checked || r->entry_ready || r->start_reserved || r->stop_rescan || r->completion_reserved) return 10;
 if(!fuel) return start_report(r,report,0,0,1,2);r->phase=5;
 pangu_i32 rc=runtime_pump_budget(r,fuel-1,report);report->work++;return rc;
}



pangu_i32 runtime_bind_geometry(pangu_lifecycle_runtime *r,pangu_lifecycle_span *spans,pangu_u64 capacity,pangu_u64 fuel)
{
 if(!geo_range(r,1,sizeof(*r),8) || !geo_range(spans,capacity,sizeof(*spans),8) || !capacity || r->count>(0xffffffffU-6U)/3U || !geo_range(r->items,r->count,sizeof(pangu_lifecycle_item),8)) return 1;
 if(r->phase || r->busy || r->started || r->stop_requested || r->protected_spans || r->protected_count) return 10;
 pangu_u64 max=r->count*3+6;if(capacity<max) return 2;pangu_u64 bytes=capacity*sizeof(*spans),used=0;
 #define STEP() do {if(!fuel) return 2;fuel--;} while(0)
 #define SAFE(ptr,n,stride,alignment) do {STEP();if(!geo_range(ptr,n,stride,alignment) || geo_overlap(spans,bytes,ptr,(n)*(stride))) return 1;} while(0)
 SAFE(r,1,sizeof(*r),8);SAFE(r->items,r->count,sizeof(pangu_lifecycle_item),8);
 for(pangu_u64 i=0;i<r->count;i++) {
  pangu_owner *o=r->items[i].owner;SAFE(o,1,sizeof(*o),8);
  if(o->abi_version!=1 || o->struct_size!=104 || o->capacity>~(pangu_u64)0/sizeof(pangu_owned_resource) || o->operation_capacity>0xffffffffU) return 1;
  SAFE(o->resources,o->capacity,sizeof(pangu_owned_resource),8);SAFE(o->operations,o->operation_capacity,sizeof(pangu_owned_operation),4);
 }
 if(r->stop_graph) {
  const pangu_lifecycle_graph *g=r->stop_graph;SAFE(g,1,sizeof(*g),8);
  SAFE(g->edges,g->edge_count,sizeof(pangu_lifecycle_edge),4);SAFE(g->disposed,g->capacity,sizeof(pangu_u32),4);
 }
 #define ADD(ptr,n,stride) do {if(n) {STEP();spans[used].begin=(pangu_u64)(ptr);spans[used++].end=(pangu_u64)(ptr)+(n)*(stride);}} while(0)
 ADD(r,1,sizeof(*r));ADD(r->items,r->count,sizeof(pangu_lifecycle_item));
 for(pangu_u64 i=0;i<r->count;i++) {pangu_owner *o=r->items[i].owner;ADD(o,1,sizeof(*o));ADD(o->resources,o->capacity,sizeof(pangu_owned_resource));ADD(o->operations,o->operation_capacity,sizeof(pangu_owned_operation));}
 if(r->stop_graph) {const pangu_lifecycle_graph *g=r->stop_graph;ADD(g,1,sizeof(*g));ADD(g->edges,g->edge_count,sizeof(pangu_lifecycle_edge));ADD(g->disposed,g->capacity,sizeof(pangu_u32));}
 ADD(spans,capacity,sizeof(*spans));
 for(pangu_u64 i=1;i<used;i++) {
  STEP();pangu_lifecycle_span x=spans[i];pangu_u64 j=i;
  while(j) {STEP();if(spans[j-1].begin<=x.begin) break;spans[j]=spans[j-1];j--;}
  spans[j]=x;
 }
 for(pangu_u64 i=1;i<used;i++) {STEP();if(spans[i-1].end>spans[i].begin) return 1;}
 STEP();r->protected_spans=spans;r->protected_count=used;return 0;
 #undef ADD
 #undef SAFE
 #undef STEP
}

void runtime_completion_wake(pangu_lifecycle_runtime *r,const pangu_owner *owner)
{
 if(!r->stop_requested || r->phase==4) return;
 /* Completion inside the currently visited cleanup is observed before advancing
  * that node. External/other-owner completions restart the pass once under fuel. */
 if(r->busy && r->stop_graph && r->stop_scan_active && r->stop_cursor && r->items[r->stop_cursor-1].owner==owner) return;
 r->stop_rescan=1;
}
pangu_i32 runtime_disposer_finish(pangu_lifecycle_runtime *r,pangu_u32 position,const pangu_cleanup_ticket *ticket,pangu_i32 status)
{
 if(!geo_external(r,ticket,sizeof(*ticket),8)) return 1;
 if(!r->stop_requested || r->phase!=3 || position>=r->launched || (r->stop_graph && r->stop_graph->disposed[position]) || ticket->owner!=r->items[position].owner) return 10;
 pangu_i32 rc=pangu_owner_cleanup_finish(r->items[position].owner,ticket,status);if(!rc) runtime_completion_wake(r,ticket->owner);return rc;
}

#undef STEP
#undef UMAX

#define STEP() do {if(!fuel) return 2;fuel--;} while(0)
static pangu_i32 items_core(const pangu_prepare_result *plan,const pangu_graph_output *graph,const pangu_lifecycle_item *source,pangu_u64 source_count,pangu_lifecycle_item *out,pangu_u64 capacity,pangu_u64 fuel,pangu_u64 *count,pangu_u32 commit,pangu_u64 *remaining)
{
 if(!plan || !graph || !count || (!source && source_count) || (!out && capacity)) return 1;
 if(plan->stage!=6 || plan->reserved || plan->lowered.report.status || plan->graph_report.status || plan->lowered.graph.registry!=&plan->lowered.registry) return 10;
 const pangu_graph_input *g=&plan->lowered.graph;
 pangu_u64 n=g->instance_count,ready=plan->graph_report.order_count,bindings=plan->graph_report.binding_count,pending=0;
 if(n>0xffffffffU || source_count!=n || ready>n || plan->graph_report.pending_count!=n-ready || graph->state_capacity<n || graph->order_capacity<ready || graph->binding_capacity<bindings || (!graph->states && n) || (!graph->order && ready) || (!graph->bindings && bindings) || (!g->instances && n) || (!g->requirements && g->requirement_count) || (!g->provider_owners && g->registry->provider_count)) return 1;
 if(capacity<ready) return 2;
 for(pangu_u64 i=0;i<n;i++) {
  STEP();if(graph->states[i]>1) return 16;pending+=graph->states[i];
  if(!graph->states[i]) {
   pangu_owner *o=source[i].owner;
   if(!o || !source[i].setup || o->abi_version!=1 || o->struct_size!=104 || o->state!=PANGU_OWNER_LOADING || o->busy || o->count || o->inflight || !o->operation_capacity) return 1;
   for(pangu_u64 j=0;j<i;j++) {STEP();if(!graph->states[j] && source[j].owner==o) return 1;}
  }
 }
 if(pending!=plan->graph_report.pending_count) return 16;
 for(pangu_u64 i=0;i<ready;i++) {
  STEP();pangu_u32 id=graph->order[i];if(id>=n || graph->states[id]) return 16;
  for(pangu_u64 j=0;j<i;j++) {STEP();if(graph->order[j]==id) return 16;}
 }
 /* Every resolved consumer/provider must be eligible and provider precedes
  * consumer. Includes optional-present and every MANY provision. */
 for(pangu_u64 i=0;i<bindings;i++) {
  STEP();pangu_plan_edge e=graph->bindings[i];
  if(e.requirement>=g->requirement_count || e.provider>=g->registry->provider_count) return 16;
  pangu_u32 consumer=g->requirements[e.requirement].consumer,provider=g->provider_owners[e.provider];
  if(consumer>=n || provider>=n || graph->states[consumer] || graph->states[provider]) return 16;
  pangu_u64 cp=n,pp=n;
  for(pangu_u64 j=0;j<ready;j++) {STEP();if(graph->order[j]==consumer) cp=j;if(graph->order[j]==provider) pp=j;}
  if(cp==n || pp>=cp) return 16;
 }
 /* Reserve the whole commit before touching caller outputs. */
 if(fuel<ready+1) return 2;fuel-=ready+1;
 if(commit) {for(pangu_u64 i=0;i<ready;i++) out[i]=source[graph->order[i]];*count=ready;}
 *remaining=fuel;return 0;
}

static pangu_i32 edges_core(const pangu_prepare_result *p,const pangu_graph_output *out,pangu_lifecycle_edge *edges,pangu_u64 capacity,pangu_u64 fuel,pangu_u64 *count,pangu_u32 commit,pangu_u64 *remaining)
{
 if(!p || !out || !count || (!edges && capacity)) return 1;
 if(p->stage!=6 || p->reserved || p->graph_report.status || p->lowered.report.status || p->lowered.graph.registry!=&p->lowered.registry) return 10;
 const pangu_graph_input *g=&p->lowered.graph;pangu_u64 n=g->instance_count,ready=p->graph_report.order_count,total=p->graph_report.binding_count;
 if(n>0xffffffffU || ready>n || p->graph_report.pending_count!=n-ready || out->order_capacity<ready || out->binding_capacity<total || out->state_capacity<n || (!out->order && ready) || (!out->states && n) || (!out->bindings && total) || (!g->requirements && g->requirement_count) || (!g->provider_owners && g->registry->provider_count)) return 1;
 if(capacity<total) return 2;
 for(pangu_u64 i=0;i<ready;i++) {
  STEP();if(out->order[i]>=n || out->states[out->order[i]]) return 16;
  for(pangu_u64 j=0;j<i;j++) {STEP();if(out->order[j]==out->order[i]) return 16;}
 }
 for(pangu_u64 i=0;i<total;i++) {
  STEP();pangu_plan_edge e=out->bindings[i];if(e.requirement>=g->requirement_count || e.provider>=g->registry->provider_count) return 16;
  pangu_u32 c=g->requirements[e.requirement].consumer,p=g->provider_owners[e.provider];if(c>=n || p>=n || out->states[c] || out->states[p]) return 16;
  pangu_u64 cp=n,pp=n;for(pangu_u64 j=0;j<ready;j++) {STEP();if(out->order[j]==c) cp=j;if(out->order[j]==p) pp=j;}
  if(cp==n || pp>=cp) return 16;
 }
 /* n and total bounded to u32 by prepared report: reserve a full commit pass. */
 pangu_u64 work=total*(ready+1)+1;if(fuel<work) return 2;fuel-=work;
 if(commit) {for(pangu_u64 i=0;i<total;i++) {
  pangu_plan_edge e=out->bindings[i];pangu_u32 c=g->requirements[e.requirement].consumer,p=g->provider_owners[e.provider],cp=0,pp=0;
  for(pangu_u64 j=0;j<ready;j++) {if(out->order[j]==c) cp=(pangu_u32)j;if(out->order[j]==p) pp=(pangu_u32)j;}
  edges[i].consumer=cp;edges[i].provider=pp;
 }
 *count=total;}*remaining=fuel;return 0;
}

typedef struct {const void *p;pangu_u64 count,stride,align;} adapter_slice;
static pangu_i32 views(const pangu_prepare_result *p,const pangu_graph_output *out)
{
 if(!geo_range(p,1,sizeof(*p),8) || !geo_range(out,1,sizeof(*out),8)) return 1;
 if(p->stage!=6 || p->reserved || p->lowered.report.status || p->graph_report.status || p->lowered.graph.registry!=&p->lowered.registry) return 10;return 0;
}
static pangu_i32 protect_slice(adapter_slice input,const adapter_slice *dest,pangu_u64 dest_count,pangu_u64 *fuel)
{
 if(!*fuel) return 2;(*fuel)--;if(!geo_range(input.p,input.count,input.stride,input.align)) return 1;
 for(pangu_u64 i=0;i<dest_count;i++) {
  if(!*fuel) return 2;(*fuel)--;
  if(geo_overlap(input.p,input.count*input.stride,dest[i].p,dest[i].count*dest[i].stride)) return 1;
 }
 return 0;
}
static pangu_i32 geometry(const pangu_prepare_result *p,const pangu_graph_output *out,const pangu_lifecycle_item *source,pangu_u64 source_count,const pangu_lifecycle_storage *storage,const adapter_slice *dest,pangu_u64 dest_count,pangu_u64 *fuel)
{
 const pangu_graph_input *g=&p->lowered.graph;const pangu_plan_registry *registry=&p->lowered.registry;
 if(g->instance_count>0xffffffffU || out->state_capacity<g->instance_count || (source && source_count!=g->instance_count)) return 1;
 for(pangu_u64 i=0;i<dest_count;i++) {
  if(!*fuel) return 2;(*fuel)--;if(!geo_range(dest[i].p,dest[i].count,dest[i].stride,dest[i].align)) return 1;
  for(pangu_u64 j=0;j<i;j++) {if(!*fuel) return 2;(*fuel)--;if(geo_overlap(dest[i].p,dest[i].count*dest[i].stride,dest[j].p,dest[j].count*dest[j].stride)) return 1;}
 }
 adapter_slice inputs[]={
  {p,1,sizeof(*p),8},{out,1,sizeof(*out),8},
  {registry->scopes,registry->scope_count,sizeof(pangu_plan_scope),4},{registry->services,registry->service_count,sizeof(pangu_plan_service),4},
  {registry->bindings,registry->binding_count,sizeof(pangu_plan_binding),4},{registry->providers,registry->provider_count,sizeof(pangu_plan_provider),4},
  {g->instances,g->instance_count,sizeof(pangu_plan_instance),4},{g->provider_owners,registry->provider_count,sizeof(pangu_u32),4},{g->requirements,g->requirement_count,sizeof(pangu_plan_requirement),4},
  {out->order,out->order_capacity,sizeof(pangu_u32),4},{out->states,out->state_capacity,sizeof(pangu_u32),4},{out->bindings,out->binding_capacity,sizeof(pangu_plan_edge),4},
  {source,source_count,sizeof(pangu_lifecycle_item),8},{storage,storage ? 1U : 0U,sizeof(pangu_lifecycle_storage),8}};
 for(pangu_u64 i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++) {pangu_i32 rc=protect_slice(inputs[i],dest,dest_count,fuel);if(rc) return rc;}
 if(source) for(pangu_u64 i=0;i<source_count;i++) {
  if(!*fuel) return 2;(*fuel)--;if(out->states[i]) continue;pangu_owner *o=source[i].owner;
  adapter_slice frame={o,1,sizeof(*o),8};pangu_i32 rc=protect_slice(frame,dest,dest_count,fuel);if(rc) return rc;
  if(o->abi_version!=1 || o->struct_size!=104 || o->operation_capacity>0xffffffffU) return 1;
  adapter_slice resources={o->resources,o->capacity,sizeof(pangu_owned_resource),8},operations={o->operations,o->operation_capacity,sizeof(pangu_owned_operation),4};
  rc=protect_slice(resources,dest,dest_count,fuel);if(rc) return rc;rc=protect_slice(operations,dest,dest_count,fuel);if(rc) return rc;
 }
 return 0;
}


pangu_i32 runtime_prepare_lifecycle(const pangu_prepare_result *p,const pangu_graph_output *out,const pangu_lifecycle_item *source,pangu_u64 source_count,const pangu_lifecycle_storage *storage,pangu_u64 fuel,pangu_lifecycle_prepared *info)
{
 pangu_i32 rc=views(p,out);if(rc) return rc;
 if(!geo_range(storage,1,sizeof(*storage),8) || !fuel || source_count!=p->lowered.graph.instance_count) return 1;
 pangu_u64 initial=fuel;adapter_slice dest[]={{storage->items,storage->item_capacity,sizeof(pangu_lifecycle_item),8},{storage->edges,storage->edge_capacity,sizeof(pangu_lifecycle_edge),4},{info,1,sizeof(*info),8}};
 rc=geometry(p,out,source,source_count,storage,dest,3,&fuel);if(rc) return rc;
 pangu_u64 ignored=0,remaining=0;
 rc=items_core(p,out,source,source_count,storage->items,storage->item_capacity,fuel,&ignored,0,&remaining);if(rc) return rc;fuel=remaining;
 rc=edges_core(p,out,storage->edges,storage->edge_capacity,fuel,&ignored,0,&remaining);if(rc) return rc;fuel=remaining;
 if(!fuel) return 2;fuel--;
 for(pangu_u64 i=0;i<p->graph_report.order_count;i++) storage->items[i]=source[out->order[i]];
 for(pangu_u64 i=0;i<p->graph_report.binding_count;i++) {
  pangu_plan_edge e=out->bindings[i];pangu_u32 c=p->lowered.graph.requirements[e.requirement].consumer,provider=p->lowered.graph.provider_owners[e.provider],cp=0,pp=0;
  for(pangu_u64 j=0;j<p->graph_report.order_count;j++) {if(out->order[j]==c) cp=(pangu_u32)j;if(out->order[j]==provider) pp=(pangu_u32)j;}
  storage->edges[i]=(pangu_lifecycle_edge){cp,pp};
 }
 info->item_count=p->graph_report.order_count;info->edge_count=p->graph_report.binding_count;info->work=initial-fuel;return 0;
}

#undef STEP
#undef UMAX

static int operation_live(pangu_owner *o,const pangu_operation_ticket *t)
{
 return t->owner==o && t->epoch==o->epoch && t->generation && t->index<o->operation_capacity && o->inflight && o->operations[t->index].active && o->operations[t->index].generation==t->generation;
}
static pangu_i32 pair(pangu_lifecycle_runtime *r,pangu_u32 binding,pangu_owner **consumer,pangu_owner **provider)
{
 if(!r) return 1;if(!r->stop_graph) return 9;
 const pangu_lifecycle_graph *g=r->stop_graph;if(binding>=g->edge_count) return 3;pangu_lifecycle_edge e=g->edges[binding];
 if(e.consumer>=r->count || e.provider>=e.consumer) return 16;
 if(e.consumer>=r->launched || g->disposed[e.consumer] || g->disposed[e.provider]) return 10;
 *consumer=r->items[e.consumer].owner;*provider=r->items[e.provider].owner;return 0;
}
static pangu_i32 enter(pangu_lifecycle_runtime *r,pangu_u32 binding,pangu_u32 kind,const void *scope,pangu_u64 fuel,pangu_lifecycle_call_ticket *out,pangu_lifecycle_report *report)
{
 pangu_u64 scope_bytes=kind==1 ? sizeof(pangu_lifecycle_setup_ticket) : sizeof(pangu_cleanup_ticket);
 if(!geo_external(r,out,sizeof(*out),8) || !geo_external(r,report,sizeof(*report),8) || geo_overlap(out,sizeof(*out),report,sizeof(*report)) || (kind && (!geo_external(r,scope,scope_bytes,8) || geo_overlap(out,sizeof(*out),scope,scope_bytes) || geo_overlap(report,sizeof(*report),scope,scope_bytes)))) return 1;
 pangu_u64 initial=fuel;
 #define RESULT(why,rc) do {report->work=initial-fuel;report->state=r->phase;report->pending=why==1;report->reason=why;report->reserved=0;return rc;} while(0)
 #define STEP() do {if(!fuel) {RESULT(1,2);}fuel--;} while(0)
 pangu_owner *consumer,*provider;pangu_i32 rc=pair(r,binding,&consumer,&provider);if(rc) {RESULT(0,rc);}
 if(kind==0 && (r->phase!=2 || r->stop_requested)) {RESULT(0,10);}
 if(kind==1) {
  const pangu_lifecycle_setup_ticket *s=scope;
  if(r->phase!=1 || r->stop_requested || s->runtime!=r || s->operation.owner!=consumer || !operation_live(consumer,&s->operation) || !(r->callback_open || (r->setup_waiting && !r->setup_completed))) {RESULT(0,10);}
  const pangu_operation_ticket *a=&s->operation,*b=&r->setup_operation;
  if(a->owner!=b->owner || a->epoch!=b->epoch || a->index!=b->index || a->generation!=b->generation) {RESULT(0,10);}
 }
 if(kind==2 && (!r->stop_requested || r->phase!=3)) {RESULT(0,10);}
 STEP();rc=pangu_owner_call_ready(consumer,kind,kind==2 ? scope : (void *)0);if(rc) {RESULT(0,rc);}
 STEP();rc=pangu_owner_call_ready(provider,0,(void *)0);if(rc) {RESULT(0,rc);}
 pangu_u64 ci=0,pi=0;
 for(;ci<consumer->operation_capacity;ci++) {STEP();if(!consumer->operations[ci].active && consumer->operations[ci].generation!=0xffffffffU) break;}
 if(ci==consumer->operation_capacity || consumer->inflight==consumer->operation_capacity) {RESULT(0,2);}
 for(;pi<provider->operation_capacity;pi++) {STEP();if(!provider->operations[pi].active && provider->operations[pi].generation!=0xffffffffU) break;}
 if(pi==provider->operation_capacity || provider->inflight==provider->operation_capacity) {RESULT(0,2);}
 STEP();pangu_lifecycle_call_ticket ticket={r,binding,0,{0},{0}};
 rc=kind==2 ? pangu_owner_enter_cleanup_slot(consumer,scope,ci,&ticket.consumer) : pangu_owner_enter_slot(consumer,kind==1,ci,&ticket.consumer);
 if(rc) {RESULT(0,rc);}rc=pangu_owner_enter_slot(provider,0,pi,&ticket.provider);
 if(rc) {(void)pangu_owner_leave(consumer,&ticket.consumer);RESULT(0,rc);}
 *out=ticket;RESULT(0,0);
 #undef STEP
 #undef RESULT
}
pangu_i32 runtime_call_enter_budget(pangu_lifecycle_runtime *r,pangu_u32 b,pangu_u64 fuel,pangu_lifecycle_call_ticket *out,pangu_lifecycle_report *report) {return enter(r,b,0,(void *)0,fuel,out,report);}
pangu_i32 runtime_call_enter_setup_budget(pangu_lifecycle_runtime *r,pangu_u32 b,const pangu_lifecycle_setup_ticket *scope,pangu_u64 fuel,pangu_lifecycle_call_ticket *out,pangu_lifecycle_report *report) {return enter(r,b,1,scope,fuel,out,report);}
pangu_i32 runtime_call_enter_cleanup_budget(pangu_lifecycle_runtime *r,pangu_u32 b,const pangu_cleanup_ticket *scope,pangu_u64 fuel,pangu_lifecycle_call_ticket *out,pangu_lifecycle_report *report) {return enter(r,b,2,scope,fuel,out,report);}



pangu_i32 runtime_call_finish(pangu_lifecycle_runtime *r,const pangu_lifecycle_call_ticket *ticket)
{
 if(!geo_external(r,ticket,sizeof(*ticket),8)) return 1;if(ticket->runtime!=r || ticket->reserved) return 10;
 pangu_owner *consumer,*provider;pangu_i32 rc=pair(r,ticket->binding,&consumer,&provider);if(rc) return rc;
 if(!operation_live(consumer,&ticket->consumer) || !operation_live(provider,&ticket->provider)) return 10;
 rc=pangu_owner_leave(provider,&ticket->provider);if(rc) return rc;rc=pangu_owner_leave(consumer,&ticket->consumer);if(!rc) runtime_completion_wake(r,consumer);return rc;
}

#undef STEP
#undef UMAX

static pangu_i32 facade_owner_init(pangu_owner *o,pangu_owned_resource *resources,pangu_u64 capacity,pangu_owned_operation *operations,pangu_u64 operation_capacity,pangu_u64 epoch,pangu_u64 fuel)
{
 if(operation_capacity>0xffffffffU) return 1;if(fuel<operation_capacity+1) return 2;
 return pangu_owner_init(o,resources,capacity,operations,operation_capacity,epoch);
}
static pangu_owner *registration_owner(pangu_lifecycle_runtime *r,pangu_u32 position)
{
 if(!geo_range(r,1,sizeof(*r),8) || !r->protected_spans || r->stop_requested || position>=r->launched) return (void *)0;
 pangu_owner *o=r->items[position].owner;
 if(r->phase==2 && o->state==PANGU_OWNER_ACTIVE) return o;
 if(r->phase==1 && (r->callback_open || (r->setup_waiting && !r->setup_completed)) && r->setup_operation.owner==o && o->state==PANGU_OWNER_LOADING) return o;
 return (void *)0;
}
static pangu_i32 facade_register(pangu_lifecycle_runtime *r,pangu_u32 position,pangu_release_fn release,void *context)
{
 pangu_owner *o=registration_owner(r,position);return o ? pangu_owner_register(o,release,context) : 10;
}
static pangu_i32 facade_register_async(pangu_lifecycle_runtime *r,pangu_u32 position,pangu_async_release_fn release,void *context)
{
 pangu_owner *o=registration_owner(r,position);return o ? pangu_owner_register_async(o,release,context) : 10;
}
static pangu_i32 facade_cleanup_scope(pangu_lifecycle_runtime *r,pangu_u32 position,pangu_cleanup_ticket *scope)
{
 if(!geo_external(r,scope,sizeof(*scope),8)) return 1;
 if(!r->stop_requested || r->phase!=3 || position>=r->launched || (r->stop_graph && r->stop_graph->disposed[position])) return 10;
 return pangu_owner_cleanup_scope(r->items[position].owner,scope);
}

static const pangu_lifecycle_api lifecycle_api={1,PANGU_LIFECYCLE_API_SIZE,PANGU_LIFECYCLE_TARGET,0,
facade_owner_init,facade_register,facade_register_async,runtime_prepare_lifecycle,runtime_bind_geometry,runtime_start_budget,runtime_pump_budget,runtime_stop_budget,runtime_setup_scope,runtime_setup_defer,runtime_setup_finish,facade_cleanup_scope,runtime_disposer_finish,runtime_call_enter_budget,runtime_call_enter_setup_budget,runtime_call_enter_cleanup_budget,runtime_call_finish};
#include "lifecycle-child-private.h"
const pangu_lifecycle_api *pangu_lifecycle_query(pangu_u32 abi,pangu_u32 size,pangu_u32 target,pangu_u32 flags)
{
 if(abi==2 && size==32 && target==1 && !flags) return (const pangu_lifecycle_api *)(const void *)&lifecycle_child_api;
 return abi==PANGU_LIFECYCLE_ABI && size==PANGU_LIFECYCLE_API_SIZE && target==PANGU_LIFECYCLE_TARGET && !flags ? &lifecycle_api : (void *)0;
}
