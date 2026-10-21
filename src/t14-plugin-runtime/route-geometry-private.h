#ifndef PANGU_ROUTE_GEOMETRY_PRIVATE_H
#define PANGU_ROUTE_GEOMETRY_PRIVATE_H
typedef struct {const void *p;pangu_u64 n,size,align;} span;
static int range(span s) {pangu_u64 a=(pangu_u64)s.p;return s.size && s.n<=~(pangu_u64)0/s.size && (!s.n || (s.p && !(a&(s.align-1)) && s.n*s.size<=~(pangu_u64)0-a));}
static int overlap(span a,span b) {pangu_u64 x=(pangu_u64)a.p,y=(pangu_u64)b.p,n=a.n*a.size,m=b.n*b.size;return n && m && (x<=y ? y-x<n : x-y<m);}
static pangu_i32 protect(span s,const span *out,pangu_u32 count,pangu_u64 *fuel) {
 if(!*fuel) return 2;(*fuel)--;if(!range(s)) return 1;
 for(pangu_u32 i=0;i<count;i++) {if(!*fuel) return 2;(*fuel)--;if(overlap(s,out[i])) return 1;}return 0;
}
#define SP(p,n,t,a) (span){p,n,sizeof(t),a}
static int host_valid(const pangu_lifecycle_api *h) {
 return range(SP(h,1,pangu_lifecycle_api,8)) && h->abi_version==1 && h->struct_size==152 && h->target==1 && !h->reserved && h->call && h->setup_call && h->cleanup_call && h->call_finish;
}
/* Numeric geometry does not authenticate pointers or re-prove planner output. */
static pangu_i32 geometry(const pangu_service_router *v,const span *out,pangu_u32 count,int writing,pangu_u64 *fuel) {
 if(!range(SP(v,1,pangu_service_router,8))) return 1;
 for(pangu_u32 i=0;i<count;i++) {if(!range(out[i])) return 1;for(pangu_u32 j=0;j<i;j++) if(overlap(out[i],out[j])) return 1;}
 if(!host_valid(v->host) || !range(SP(v->runtime,1,pangu_lifecycle_runtime,8)) || !range(SP(v->manifest,1,pangu_static_manifest,8)) || !range(SP(v->plan,1,pangu_prepare_result,8)) || !range(SP(v->graph,1,pangu_graph_output,8))) return 1;
 const pangu_prepare_result *p=v->plan;const pangu_graph_input *g=&p->lowered.graph;const pangu_plan_registry *reg=&p->lowered.registry;const pangu_static_manifest *m=v->manifest;pangu_lifecycle_runtime *r=v->runtime;
 if(p->stage!=6 || p->reserved || p->lowered.report.status || p->graph_report.status || g->registry!=reg || m->instance_count!=g->instance_count || m->definition_count!=reg->service_count || v->slot_count!=reg->provider_count || p->graph_report.order_count!=r->count || !r->protected_spans || r->protected_count>0xffffffffU || !range(SP(r->stop_graph,1,pangu_lifecycle_graph,8)) || r->stop_graph->edge_count!=p->graph_report.binding_count || v->graph->binding_capacity<p->graph_report.binding_count || v->graph->state_capacity<g->instance_count) return 10;
 span inputs[]={SP(v,1,pangu_service_router,8),SP(v->host,1,pangu_lifecycle_api,8),SP(r,1,pangu_lifecycle_runtime,8),SP(m,1,pangu_static_manifest,8),SP(p,1,pangu_prepare_result,8),SP(v->graph,1,pangu_graph_output,8),SP(v->records,g->instance_count,pangu_entry_record,8),
 SP(m->scopes,m->scope_count,pangu_plan_scope,4),SP(m->definitions,m->definition_count,pangu_manifest_definition,4),SP(m->modules,m->module_count,pangu_manifest_module,4),SP(m->instances,m->instance_count,pangu_manifest_instance,4),SP(m->bindings,m->binding_count,pangu_manifest_binding,4),SP(m->provisions,m->provision_count,pangu_manifest_provision,4),SP(m->requirements,m->requirement_count,pangu_manifest_requirement,4),
 SP(reg->scopes,reg->scope_count,pangu_plan_scope,4),SP(reg->services,reg->service_count,pangu_plan_service,4),SP(reg->bindings,reg->binding_count,pangu_plan_binding,4),SP(reg->providers,reg->provider_count,pangu_plan_provider,4),SP(g->instances,g->instance_count,pangu_plan_instance,4),SP(g->provider_owners,reg->provider_count,pangu_u32,4),SP(g->requirements,g->requirement_count,pangu_plan_requirement,4),SP(v->graph->order,v->graph->order_capacity,pangu_u32,4),SP(v->graph->states,v->graph->state_capacity,pangu_u32,4),SP(v->graph->bindings,v->graph->binding_capacity,pangu_plan_edge,4),SP(r->protected_spans,r->protected_count,pangu_lifecycle_span,8)};
 for(pangu_u64 i=0;i<sizeof(inputs)/sizeof(inputs[0]);i++) {pangu_i32 rc=protect(inputs[i],out,count,fuel);if(rc) return rc;}
 if(!range(SP(v->slots,v->slot_count,pangu_service_slot,8))) return 1;
 if(!writing) {pangu_i32 rc=protect(SP(v->slots,v->slot_count,pangu_service_slot,8),out,count,fuel);if(rc) return rc;}
 for(pangu_u64 i=0;i<r->protected_count;i++) {pangu_i32 rc=protect(SP((const void *)r->protected_spans[i].begin,r->protected_spans[i].end-r->protected_spans[i].begin,unsigned char,1),out,count,fuel);if(rc) return rc;}
 /* Pending owner pools are outside runtime geometry but are still borrowed. */
 for(pangu_u64 i=0;i<g->instance_count;i++) {
  pangu_owner *o=v->records[i].context.owner;pangu_i32 rc=protect(SP(o,1,pangu_owner,8),out,count,fuel);if(rc) return rc;
  rc=protect(SP(o->resources,o->capacity,pangu_owned_resource,8),out,count,fuel);if(rc) return rc;rc=protect(SP(o->operations,o->operation_capacity,pangu_owned_operation,4),out,count,fuel);if(rc) return rc;
 }
 return 0;
}
#endif
