#include "src/bootstrap-api/host-plan.h"
#define NONE 0xffffffffU
#define UMAX (~(pangu_u64)0)
static int scope_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int scope_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
typedef struct plan {const pangu_plan_registry *r;pangu_plan_report *out;pangu_u64 fuel;} plan;
static pangu_i32 scope_fail(plan *c,pangu_i32 rc,pangu_u32 reason,pangu_u32 location) {c->out->status=rc;c->out->reason=reason;c->out->location=location;return rc;}
static pangu_i32 scope_step(plan *c) {if(c->out->work==c->fuel) return scope_fail(c,2,1,NONE);c->out->work++;return 0;}
static pangu_i32 scope_binding(plan *c,pangu_u32 scope,pangu_u32 service,pangu_u32 *mode)
{
 *mode=0;for(pangu_u64 i=0;i<c->r->binding_count;i++) {if(scope_step(c)) return 2;const pangu_plan_binding *b=&c->r->bindings[i];if(b->scope==scope && b->service==service) {*mode=b->mode;break;}}return 0;
}
static pangu_i32 scope_realm(plan *c,pangu_u32 scope,pangu_u32 service,pangu_u32 *out)
{
 while(1) {if(scope_step(c)) return 2;pangu_u32 mode;if(scope_binding(c,scope,service,&mode)) return 2;
  if(mode==1) {*out=scope;return 0;}if(mode!=2 || !scope) {*out=NONE;return 0;}scope=c->r->scopes[scope].parent;
 }
}
static pangu_i32 scope_validate(plan *c)
{
 const pangu_plan_registry *r=c->r;
 for(pangu_u64 i=0;i<r->scope_count;i++) {if(scope_step(c)) return 2;if(r->scopes[i].reserved || (i ? r->scopes[i].parent>=i : r->scopes[i].parent!=NONE)) return scope_fail(c,16,2,(pangu_u32)i);}
 for(pangu_u64 i=0;i<r->service_count;i++) {if(scope_step(c)) return 2;if(r->services[i].reserved || (r->services[i].cardinality!=1 && r->services[i].cardinality!=2)) return scope_fail(c,16,3,(pangu_u32)i);}
 for(pangu_u64 i=0;i<r->binding_count;i++) {
  if(scope_step(c)) return 2;const pangu_plan_binding *b=&r->bindings[i];if(b->reserved || b->scope>=r->scope_count || b->service>=r->service_count || (b->mode!=1 && b->mode!=2) || (!b->scope && b->mode==2)) return scope_fail(c,16,4,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {if(scope_step(c)) return 2;if(r->bindings[j].scope==b->scope && r->bindings[j].service==b->service) return scope_fail(c,4,4,(pangu_u32)i);}
 }
 for(pangu_u64 i=0;i<r->provider_count;i++) {
  if(scope_step(c)) return 2;const pangu_plan_provider *p=&r->providers[i];if(p->scope>=r->scope_count || p->service>=r->service_count || !p->version || !p->instance) return scope_fail(c,16,5,(pangu_u32)i);
  pangu_u32 mode;if(scope_binding(c,p->scope,p->service,&mode)) return 2;if(mode!=1) return scope_fail(c,16,6,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {if(scope_step(c)) return 2;const pangu_plan_provider *a=&r->providers[j];if(a->instance==p->instance || (r->services[p->service].cardinality==1 && a->scope==p->scope && a->service==p->service && a->version==p->version)) return scope_fail(c,4,5,(pangu_u32)i);}
 }
 return 0;
}
static pangu_i32 pangu_scope_plan_resolve(const pangu_plan_registry *r,const pangu_plan_query *q,pangu_u64 max_work,pangu_u32 *indices,pangu_u64 capacity,pangu_plan_report *out)
{
 if(!r || !q || !out || !scope_range_ok(r,64) || !scope_range_ok(q,16) || !scope_range_ok(out,32) || !max_work || capacity>UMAX/4 || !scope_range_ok(indices,capacity*4) || !r->scope_count || !r->service_count || r->scope_count>NONE || r->service_count>NONE || r->binding_count>NONE || r->provider_count>NONE) return 1;
 pangu_u64 read[6],rn[6];read[0]=(pangu_u64)r;rn[0]=64;read[1]=(pangu_u64)q;rn[1]=16;read[2]=(pangu_u64)r->scopes;rn[2]=r->scope_count*8;read[3]=(pangu_u64)r->services;rn[3]=r->service_count*8;read[4]=(pangu_u64)r->bindings;rn[4]=r->binding_count*16;read[5]=(pangu_u64)r->providers;rn[5]=r->provider_count*16;
 for(pangu_u32 i=0;i<6;i++) if(!scope_range_ok((const void *)read[i],rn[i]) || scope_overlap((pangu_u64)indices,capacity*4,read[i],rn[i]) || scope_overlap((pangu_u64)out,32,read[i],rn[i])) return 1;
 if(scope_overlap((pangu_u64)indices,capacity*4,(pangu_u64)out,32) || q->scope>=r->scope_count || q->service>=r->service_count || !q->version || q->required>1) return 1;
 out->status=0;out->reason=0;out->work=0;out->realm=NONE;out->count=0;out->location=NONE;out->reserved=0;plan c;c.r=r;c.out=out;c.fuel=max_work;
 pangu_i32 rc=scope_validate(&c);if(rc) return rc;if(scope_realm(&c,q->scope,q->service,&out->realm)) return 2;
 pangu_u32 count=0;
 for(pangu_u64 i=0;i<r->provider_count;i++) {if(scope_step(&c)) return 2;const pangu_plan_provider *p=&r->providers[i];if(p->scope==out->realm && p->service==q->service && p->version==q->version) count++;}
 if(!count && q->required) return scope_fail(&c,3,7,NONE);
 out->count=count;if(count>capacity) return scope_fail(&c,2,1,NONE);
 /* Reserve emission fuel in advance, preserving indices on every quota error. */
 if(r->provider_count>max_work-out->work) return scope_fail(&c,2,1,NONE);out->work+=r->provider_count;
 pangu_u32 at=0;for(pangu_u64 i=0;i<r->provider_count;i++) {const pangu_plan_provider *p=&r->providers[i];if(p->scope==out->realm && p->service==q->service && p->version==q->version) indices[at++]=(pangu_u32)i;}
 return 0;
}
static int graph_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int graph_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
typedef struct graph {const pangu_graph_input *in;const pangu_graph_scratch *s;pangu_graph_report *r;pangu_u64 fuel,edge_count;} graph;
static pangu_i32 graph_fail(graph *c,pangu_i32 rc,pangu_u32 reason,pangu_u32 location) {c->r->status=rc;c->r->reason=reason;c->r->location=location;return rc;}
static pangu_i32 graph_step(graph *c) {if(c->r->work==c->fuel) return graph_fail(c,2,1,NONE);c->r->work++;return 0;}
static pangu_i32 graph_resolve(graph *c,const pangu_plan_query *q,pangu_u32 *count)
{
 if(c->r->work==c->fuel) return graph_fail(c,2,1,NONE);pangu_plan_report report;
 pangu_i32 rc=pangu_scope_plan_resolve(c->in->registry,q,c->fuel-c->r->work,c->s->resolved,c->in->registry->provider_count,&report);
 if(rc==1) return graph_fail(c,16,2,NONE);c->r->work+=report.work;if(rc) return graph_fail(c,rc,report.reason,report.location);*count=report.count;return 0;
}
static pangu_i32 pangu_graph_plan(const pangu_graph_input *in,const pangu_graph_scratch *s,const pangu_graph_output *out,pangu_u64 max_work,pangu_graph_report *report)
{
 if(!in || !s || !out || !report || !graph_range_ok(in,48) || !graph_range_ok(s,48) || !graph_range_ok(out,48) || !graph_range_ok(report,32) || !in->registry || !graph_range_ok(in->registry,64) || !max_work || !in->instance_count || in->instance_count>NONE || in->requirement_count>NONE || s->edge_capacity>NONE || out->order_capacity>UMAX/4 || out->state_capacity>UMAX/4 || out->binding_capacity>UMAX/8) return 1;
 const pangu_plan_registry *r=in->registry;if(!r->scope_count || !r->service_count || r->scope_count>NONE || r->service_count>NONE || r->binding_count>NONE || r->provider_count>NONE) return 1;
 pangu_u64 read[11],rn[11],write[9],wn[9];
 read[0]=(pangu_u64)in;rn[0]=48;read[1]=(pangu_u64)r;rn[1]=64;read[2]=(pangu_u64)r->scopes;rn[2]=r->scope_count*8;read[3]=(pangu_u64)r->services;rn[3]=r->service_count*8;read[4]=(pangu_u64)r->bindings;rn[4]=r->binding_count*16;read[5]=(pangu_u64)r->providers;rn[5]=r->provider_count*16;read[6]=(pangu_u64)in->instances;rn[6]=in->instance_count*8;read[7]=(pangu_u64)in->provider_owners;rn[7]=r->provider_count*4;read[8]=(pangu_u64)in->requirements;rn[8]=in->requirement_count*16;read[9]=(pangu_u64)s;rn[9]=48;read[10]=(pangu_u64)out;rn[10]=48;
 write[0]=(pangu_u64)s->pending;wn[0]=in->instance_count*4;write[1]=(pangu_u64)s->degree;wn[1]=in->instance_count*4;write[2]=(pangu_u64)s->queue;wn[2]=in->instance_count*4;write[3]=(pangu_u64)s->resolved;wn[3]=r->provider_count*4;write[4]=(pangu_u64)s->edges;wn[4]=s->edge_capacity*8;write[5]=(pangu_u64)out->order;wn[5]=out->order_capacity*4;write[6]=(pangu_u64)out->states;wn[6]=out->state_capacity*4;write[7]=(pangu_u64)out->bindings;wn[7]=out->binding_capacity*8;write[8]=(pangu_u64)report;wn[8]=32;
 for(pangu_u32 i=0;i<11;i++) if(!graph_range_ok((const void *)read[i],rn[i])) return 1;
 for(pangu_u32 i=0;i<9;i++) {if(!graph_range_ok((const void *)write[i],wn[i])) return 1;for(pangu_u32 j=0;j<11;j++) if(graph_overlap(write[i],wn[i],read[j],rn[j])) return 1;for(pangu_u32 j=i+1;j<9;j++) if(graph_overlap(write[i],wn[i],write[j],wn[j])) return 1;}
 report->status=0;report->reason=0;report->work=0;report->order_count=0;report->pending_count=0;report->binding_count=0;report->location=NONE;graph c;c.in=in;c.s=s;c.r=report;c.fuel=max_work;c.edge_count=0;
 pangu_plan_query query;query.scope=0;query.service=0;query.version=1;query.required=0;pangu_u32 ignored;if(graph_resolve(&c,&query,&ignored)) return report->status;
 for(pangu_u64 i=0;i<in->instance_count;i++) {
  if(graph_step(&c)) return 2;const pangu_plan_instance *p=&in->instances[i];if(p->scope>=r->scope_count || !p->id) return graph_fail(&c,16,3,(pangu_u32)i);s->pending[i]=0;s->degree[i]=0;
  for(pangu_u64 j=0;j<i;j++) {if(graph_step(&c)) return 2;if(in->instances[j].id==p->id) return graph_fail(&c,4,3,(pangu_u32)i);}
 }
 for(pangu_u64 i=0;i<r->provider_count;i++) {if(graph_step(&c)) return 2;pangu_u32 owner=in->provider_owners[i];if(owner>=in->instance_count || in->instances[owner].scope!=r->providers[i].scope) return graph_fail(&c,16,4,(pangu_u32)i);}
 for(pangu_u64 i=0;i<in->requirement_count;i++) {
  if(graph_step(&c)) return 2;const pangu_plan_requirement *q=&in->requirements[i];if(q->consumer>=in->instance_count || q->service>=r->service_count || !q->version || q->required>1) return graph_fail(&c,16,5,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {if(graph_step(&c)) return 2;const pangu_plan_requirement *a=&in->requirements[j];if(a->consumer==q->consumer && a->service==q->service && a->version==q->version) return graph_fail(&c,4,5,(pangu_u32)i);}
  query.scope=in->instances[q->consumer].scope;query.service=q->service;query.version=q->version;query.required=0;pangu_u32 count;if(graph_resolve(&c,&query,&count)) return report->status;
  if(count>s->edge_capacity-c.edge_count) return graph_fail(&c,2,1,(pangu_u32)i);
  for(pangu_u32 j=0;j<count;j++) {if(graph_step(&c)) return 2;s->edges[c.edge_count].requirement=(pangu_u32)i;s->edges[c.edge_count].provider=s->resolved[j];c.edge_count++;}
 }
 /* Monotone Pending closure: a required MANY dependency needs >=1 eligible owner. */
 pangu_u32 changed=1;
 while(changed) {
  changed=0;for(pangu_u64 i=0;i<in->requirement_count;i++) {
   if(graph_step(&c)) return 2;const pangu_plan_requirement *q=&in->requirements[i];if(!q->required || s->pending[q->consumer]) continue;pangu_u32 any=0;
   for(pangu_u64 j=0;j<c.edge_count;j++) {if(graph_step(&c)) return 2;if(s->edges[j].requirement==i && !s->pending[in->provider_owners[s->edges[j].provider]]) any=1;}
   if(!any) {s->pending[q->consumer]=1;changed=1;}
  }
 }
 pangu_u64 final_edges=0;
 for(pangu_u64 i=0;i<c.edge_count;i++) {
  if(graph_step(&c)) return 2;pangu_plan_edge e=s->edges[i];pangu_u32 consumer=in->requirements[e.requirement].consumer,provider=in->provider_owners[e.provider];if(s->pending[consumer] || s->pending[provider]) continue;
  if(s->degree[consumer]>=NONE-1) return graph_fail(&c,2,1,consumer);s->degree[consumer]++;s->edges[final_edges].requirement=e.requirement;s->edges[final_edges].provider=e.provider;final_edges++;
 }
 pangu_u32 pending=0,ready=0;for(pangu_u64 i=0;i<in->instance_count;i++) {if(graph_step(&c)) return 2;if(s->pending[i]) pending++;}
 while((pangu_u64)ready+pending<in->instance_count) {
  pangu_u32 next=NONE;for(pangu_u64 i=0;i<in->instance_count;i++) {if(graph_step(&c)) return 2;if(!s->pending[i] && !s->degree[i]) {next=(pangu_u32)i;break;}}
  if(next==NONE) return graph_fail(&c,17,8,NONE);s->queue[ready++]=next;s->degree[next]=NONE;
  for(pangu_u64 i=0;i<final_edges;i++) {if(graph_step(&c)) return 2;pangu_plan_edge e=s->edges[i];if(in->provider_owners[e.provider]==next) s->degree[in->requirements[e.requirement].consumer]--;}
 }
 report->order_count=ready;report->pending_count=pending;report->binding_count=(pangu_u32)final_edges;
 if(ready>out->order_capacity || in->instance_count>out->state_capacity || final_edges>out->binding_capacity) return graph_fail(&c,2,1,NONE);
 pangu_u64 need=(pangu_u64)ready+in->instance_count+final_edges;
 if(need>max_work-report->work) return graph_fail(&c,2,1,NONE);report->work+=need;
 for(pangu_u32 i=0;i<ready;i++) out->order[i]=s->queue[i];for(pangu_u64 i=0;i<in->instance_count;i++) out->states[i]=s->pending[i];
 for(pangu_u64 i=0;i<final_edges;i++) {out->bindings[i].requirement=s->edges[i].requirement;out->bindings[i].provider=s->edges[i].provider;}
 return 0;
}
static int manifest_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int manifest_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
typedef struct lower {const pangu_static_manifest *m;pangu_plan_report *r;pangu_u64 fuel;} lower;
static pangu_i32 manifest_bad(lower *c,pangu_i32 rc,pangu_u32 kind,pangu_u32 at) {c->r->status=rc;c->r->reason=kind;c->r->location=at;return rc;}
static pangu_i32 manifest_charge(lower *c,pangu_u64 n) {if(n>c->fuel-c->r->work) return manifest_bad(c,2,1,NONE);c->r->work+=n;return 0;}
static pangu_i32 manifest_name(lower *c,const unsigned char *p,pangu_u32 kind,pangu_u32 at)
{
 if(manifest_charge(c,32)) return 2;pangu_u32 end=0;
 for(pangu_u32 i=0;i<32;i++) {unsigned char ch=p[i];if(!ch) {if(!i) return manifest_bad(c,16,kind,at);end=1;continue;}if(end || !((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='.' || ch=='-' || ch=='_')) return manifest_bad(c,16,kind,at);}
 return end ? 0 : manifest_bad(c,16,kind,at);
}
static pangu_i32 manifest_equal(lower *c,const unsigned char *a,const unsigned char *b,pangu_u32 *same)
{
 if(manifest_charge(c,32)) return 2;*same=1;for(pangu_u32 i=0;i<32;i++) if(a[i]!=b[i]) *same=0;return 0;
}
static pangu_i32 manifest_find(lower *c,const unsigned char *key,pangu_u32 kind,pangu_u32 version,pangu_u32 *index)
{
 *index=NONE;pangu_u64 count=kind==2 ? c->m->definition_count : kind==3 ? c->m->module_count : c->m->instance_count;
 for(pangu_u64 i=0;i<count;i++) {pangu_u32 same;const unsigned char *n=kind==2 ? c->m->definitions[i].name : kind==3 ? c->m->modules[i].name : c->m->instances[i].name;if(manifest_equal(c,key,n,&same)) return 2;if(same && (kind==2 ? c->m->definitions[i].version==version : kind==3 ? c->m->modules[i].version==version : 1)) {*index=(pangu_u32)i;return 0;}}
 return manifest_bad(c,3,kind,NONE);
}
static pangu_i32 pangu_manifest_lower(const pangu_static_manifest *m,const pangu_manifest_storage *s,pangu_u64 max_work,pangu_manifest_result *out)
{
 if(!m || !s || !out || !manifest_range_ok(m,112) || !manifest_range_ok(s,88) || !manifest_range_ok(out,144) || !max_work || !m->scope_count || !m->definition_count || !m->module_count || !m->instance_count) return 1;
 pangu_u64 counts[7],sizes[7],a[9],n[9],w[7],wn[7];counts[0]=m->scope_count;counts[1]=m->definition_count;counts[2]=m->module_count;counts[3]=m->instance_count;counts[4]=m->binding_count;counts[5]=m->provision_count;counts[6]=m->requirement_count;sizes[0]=8;sizes[1]=44;sizes[2]=96;sizes[3]=72;sizes[4]=44;sizes[5]=72;sizes[6]=72;
 a[0]=(pangu_u64)m->scopes;a[1]=(pangu_u64)m->definitions;a[2]=(pangu_u64)m->modules;a[3]=(pangu_u64)m->instances;a[4]=(pangu_u64)m->bindings;a[5]=(pangu_u64)m->provisions;a[6]=(pangu_u64)m->requirements;
 for(pangu_u32 i=0;i<7;i++) {if(counts[i]>NONE) return 1;n[i]=counts[i]*sizes[i];}a[7]=(pangu_u64)m;n[7]=112;a[8]=(pangu_u64)s;n[8]=88;
 if(s->service_capacity>UMAX/8 || s->instance_capacity>UMAX/8 || s->binding_capacity>UMAX/16 || s->provider_capacity>UMAX/16 || s->requirement_capacity>UMAX/16) return 1;
 w[0]=(pangu_u64)s->services;wn[0]=s->service_capacity*8;w[1]=(pangu_u64)s->instances;wn[1]=s->instance_capacity*8;w[2]=(pangu_u64)s->bindings;wn[2]=s->binding_capacity*16;w[3]=(pangu_u64)s->providers;wn[3]=s->provider_capacity*16;w[4]=(pangu_u64)s->owners;wn[4]=s->provider_capacity*4;w[5]=(pangu_u64)s->requirements;wn[5]=s->requirement_capacity*16;w[6]=(pangu_u64)out;wn[6]=144;
 for(pangu_u32 i=0;i<9;i++) if(!manifest_range_ok((const void *)a[i],n[i])) return 1;
 for(pangu_u32 i=0;i<7;i++) {if(!manifest_range_ok((const void *)w[i],wn[i])) return 1;for(pangu_u32 j=0;j<9;j++) if(manifest_overlap(w[i],wn[i],a[j],n[j])) return 1;for(pangu_u32 j=i+1;j<7;j++) if(manifest_overlap(w[i],wn[i],w[j],wn[j])) return 1;}
 /* All publication fields cleared individually, no implicit libc aggregate copy. */
 out->registry.scopes=(void *)0;out->registry.scope_count=0;out->registry.services=(void *)0;out->registry.service_count=0;out->registry.bindings=(void *)0;out->registry.binding_count=0;out->registry.providers=(void *)0;out->registry.provider_count=0;
 out->graph.registry=(void *)0;out->graph.instances=(void *)0;out->graph.instance_count=0;out->graph.provider_owners=(void *)0;out->graph.requirements=(void *)0;out->graph.requirement_count=0;
 out->report.status=0;out->report.reason=0;out->report.work=0;out->report.realm=NONE;out->report.count=0;out->report.location=NONE;out->report.reserved=0;lower c;c.m=m;c.r=&out->report;c.fuel=max_work;
 if(s->service_capacity<m->definition_count || s->instance_capacity<m->instance_count || s->binding_capacity<m->binding_count || s->provider_capacity<m->provision_count || s->requirement_capacity<m->requirement_count) return manifest_bad(&c,2,1,NONE);
 for(pangu_u64 i=0;i<m->scope_count;i++) {if(manifest_charge(&c,1)) return 2;if(m->scopes[i].reserved || (i ? m->scopes[i].parent>=i : m->scopes[i].parent!=NONE)) return manifest_bad(&c,16,1,(pangu_u32)i);}
 for(pangu_u64 i=0;i<m->definition_count;i++) {
  const pangu_manifest_definition *d=&m->definitions[i];if(manifest_name(&c,d->name,2,(pangu_u32)i)) return c.r->status;if(d->reserved || !d->version || (d->cardinality!=1 && d->cardinality!=2)) return manifest_bad(&c,16,2,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {pangu_u32 same;if(manifest_equal(&c,d->name,m->definitions[j].name,&same)) return 2;if(same && d->version==m->definitions[j].version) return manifest_bad(&c,4,2,(pangu_u32)i);}s->services[i].cardinality=d->cardinality;s->services[i].reserved=0;
 }
 for(pangu_u64 i=0;i<m->module_count;i++) {
  const pangu_manifest_module *v=&m->modules[i];if(manifest_name(&c,v->name,3,(pangu_u32)i) || manifest_name(&c,v->entry,3,(pangu_u32)i)) return c.r->status;
  if(v->abi_version!=1 || v->struct_size!=96 || v->target!=1 || v->flags || v->host_min>1 || v->host_max<1) return manifest_bad(&c,9,3,(pangu_u32)i);
  if(v->reserved || !v->version || !v->host_min || v->host_min>v->host_max) return manifest_bad(&c,16,3,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {pangu_u32 same;if(manifest_equal(&c,v->name,m->modules[j].name,&same)) return 2;if(same && v->version==m->modules[j].version) return manifest_bad(&c,4,3,(pangu_u32)i);}
 }
 for(pangu_u64 i=0;i<m->instance_count;i++) {
  const pangu_manifest_instance *p=&m->instances[i];if(manifest_name(&c,p->name,4,(pangu_u32)i) || manifest_name(&c,p->module,4,(pangu_u32)i)) return c.r->status;if(p->scope>=m->scope_count || !p->version) return manifest_bad(&c,16,4,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {pangu_u32 same;if(manifest_equal(&c,p->name,m->instances[j].name,&same)) return 2;if(same) return manifest_bad(&c,4,4,(pangu_u32)i);}pangu_u32 module;if(manifest_find(&c,p->module,3,p->version,&module)) return c.r->status;
  s->instances[i].scope=p->scope;s->instances[i].id=(pangu_u32)i+1;
 }
 for(pangu_u64 i=0;i<m->binding_count;i++) {
  const pangu_manifest_binding *p=&m->bindings[i];if(manifest_name(&c,p->service,5,(pangu_u32)i)) return c.r->status;pangu_u32 service;if(manifest_find(&c,p->service,2,p->version,&service)) return c.r->status;if(p->scope>=m->scope_count || (p->mode!=1 && p->mode!=2)) return manifest_bad(&c,16,5,(pangu_u32)i);
  s->bindings[i].scope=p->scope;s->bindings[i].service=service;s->bindings[i].mode=p->mode;s->bindings[i].reserved=0;
 }
 for(pangu_u64 i=0;i<m->provision_count;i++) {
  const pangu_manifest_provision *p=&m->provisions[i];if(manifest_name(&c,p->service,6,(pangu_u32)i) || manifest_name(&c,p->instance,6,(pangu_u32)i)) return c.r->status;pangu_u32 service,owner;if(manifest_find(&c,p->service,2,p->version,&service) || manifest_find(&c,p->instance,4,0,&owner)) return c.r->status;if(!p->version || !p->id) return manifest_bad(&c,16,6,(pangu_u32)i);
  s->owners[i]=owner;s->providers[i].scope=m->instances[owner].scope;s->providers[i].service=service;s->providers[i].version=p->version;s->providers[i].instance=p->id;
 }
 for(pangu_u64 i=0;i<m->requirement_count;i++) {
  const pangu_manifest_requirement *p=&m->requirements[i];if(manifest_name(&c,p->service,7,(pangu_u32)i) || manifest_name(&c,p->consumer,7,(pangu_u32)i)) return c.r->status;pangu_u32 service,consumer;if(manifest_find(&c,p->service,2,p->version,&service) || manifest_find(&c,p->consumer,4,0,&consumer)) return c.r->status;if(!p->version || p->required>1) return manifest_bad(&c,16,7,(pangu_u32)i);
  s->requirements[i].consumer=consumer;s->requirements[i].service=service;s->requirements[i].version=p->version;s->requirements[i].required=p->required;
 }
 /* Lowering publishes structural views only; graph validation remains mandatory. */
 out->registry.scopes=m->scopes;out->registry.scope_count=m->scope_count;out->registry.services=s->services;out->registry.service_count=m->definition_count;out->registry.bindings=s->bindings;out->registry.binding_count=m->binding_count;out->registry.providers=s->providers;out->registry.provider_count=m->provision_count;
 out->graph.registry=&out->registry;out->graph.instances=s->instances;out->graph.instance_count=m->instance_count;out->graph.provider_owners=s->owners;out->graph.requirements=s->requirements;out->graph.requirement_count=m->requirement_count;
 out->report.count=(pangu_u32)m->instance_count;return 0;
}
static int contracts_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int contracts_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
typedef struct check {const pangu_static_manifest *m;const pangu_module_contract *t;pangu_u64 count,fuel;pangu_plan_report *r;} check;
static pangu_i32 contracts_bad(check *c,pangu_i32 rc,pangu_u32 reason,pangu_u32 at) {c->r->status=rc;c->r->reason=reason;c->r->location=at;return rc;}
static pangu_i32 contracts_charge(check *c,pangu_u64 n) {if(n>c->fuel-c->r->work) return contracts_bad(c,2,1,NONE);c->r->work+=n;return 0;}
static pangu_i32 contracts_same(check *c,const unsigned char *a,const unsigned char *b,pangu_u32 *yes)
{
 if(contracts_charge(c,32)) return 2;*yes=1;for(pangu_u32 i=0;i<32;i++) if(a[i]!=b[i]) *yes=0;return 0;
}
static pangu_i32 contracts_name(check *c,const unsigned char *a,pangu_u32 at)
{
 if(contracts_charge(c,32)) return 2;pangu_u32 zero=0;
 for(pangu_u32 i=0;i<32;i++) {unsigned char ch=a[i];if(!ch) {if(!i) return contracts_bad(c,16,8,at);zero=1;continue;}if(zero || !((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='.' || ch=='_' || ch=='-')) return contracts_bad(c,16,8,at);}
 return zero ? 0 : contracts_bad(c,16,8,at);
}
static pangu_i32 contracts_belongs(check *c,const pangu_module_contract *t,const pangu_manifest_instance *p,pangu_u32 *yes)
{
 if(contracts_same(c,t->module,p->module,yes)) return 2;*yes=*yes && t->module_version==p->version;return 0;
}
static pangu_i32 contracts_declared(check *c,const pangu_manifest_instance *p,const unsigned char *service,pangu_u32 version,pangu_u32 role,pangu_u32 required,pangu_u32 at)
{
 for(pangu_u64 i=0;i<c->count;i++) {if(contracts_charge(c,1)) return 2;const pangu_module_contract *t=&c->t[i];pangu_u32 yes;if(contracts_belongs(c,t,p,&yes)) return 2;if(!yes || t->role!=role || t->service_version!=version) continue;if(contracts_same(c,t->service,service,&yes)) return 2;if(yes) return t->required==required ? 0 : contracts_bad(c,16,11,at);}
 return contracts_bad(c,16,10,at);
}
static pangu_i32 pangu_manifest_contracts(const pangu_static_manifest *m,const pangu_module_contract *templates,pangu_u64 template_count,pangu_u64 max_work,pangu_plan_report *report)
{
 if(!m || !report || !contracts_range_ok(m,112) || !contracts_range_ok(report,32) || template_count>NONE || !max_work || !m->module_count || !m->definition_count || !m->instance_count || m->module_count>NONE || m->definition_count>NONE || m->instance_count>NONE || m->provision_count>NONE || m->requirement_count>NONE) return 1;
 pangu_u64 a[7],n[7];a[0]=(pangu_u64)m;n[0]=112;a[1]=(pangu_u64)templates;n[1]=template_count*80;a[2]=(pangu_u64)m->definitions;n[2]=m->definition_count*44;a[3]=(pangu_u64)m->modules;n[3]=m->module_count*96;a[4]=(pangu_u64)m->instances;n[4]=m->instance_count*72;a[5]=(pangu_u64)m->provisions;n[5]=m->provision_count*72;a[6]=(pangu_u64)m->requirements;n[6]=m->requirement_count*72;
 for(pangu_u32 i=0;i<7;i++) if(!contracts_range_ok((const void *)a[i],n[i]) || contracts_overlap((pangu_u64)report,32,a[i],n[i])) return 1;
 report->status=0;report->reason=0;report->work=0;report->realm=NONE;report->count=0;report->location=NONE;report->reserved=0;check c;c.m=m;c.t=templates;c.count=template_count;c.fuel=max_work;c.r=report;
 for(pangu_u64 i=0;i<template_count;i++) {
  const pangu_module_contract *t=&templates[i];if(contracts_name(&c,t->module,(pangu_u32)i) || contracts_name(&c,t->service,(pangu_u32)i)) return report->status;
  if(!t->module_version || !t->service_version || (t->role!=1 && t->role!=2) || t->required>1 || (t->role==1 && t->required)) return contracts_bad(&c,16,8,(pangu_u32)i);
  pangu_u32 found=0;for(pangu_u64 j=0;j<m->module_count;j++) {pangu_u32 yes;if(contracts_same(&c,t->module,m->modules[j].name,&yes)) return 2;if(yes && t->module_version==m->modules[j].version) found=1;}if(!found) return contracts_bad(&c,3,8,(pangu_u32)i);
  found=0;for(pangu_u64 j=0;j<m->definition_count;j++) {pangu_u32 yes;if(contracts_same(&c,t->service,m->definitions[j].name,&yes)) return 2;if(yes && t->service_version==m->definitions[j].version) found=1;}if(!found) return contracts_bad(&c,3,8,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {const pangu_module_contract *a=&templates[j];if(contracts_charge(&c,1)) return 2;if(a->module_version!=t->module_version || a->service_version!=t->service_version || a->role!=t->role) continue;pangu_u32 yes;if(contracts_same(&c,a->module,t->module,&yes)) return 2;if(!yes) continue;if(contracts_same(&c,a->service,t->service,&yes)) return 2;if(yes) return contracts_bad(&c,4,8,(pangu_u32)i);}
 }
 for(pangu_u64 i=0;i<m->instance_count;i++) {
  const pangu_manifest_instance *p=&m->instances[i];if(contracts_charge(&c,1)) return 2;
  for(pangu_u64 j=0;j<m->provision_count;j++) {const pangu_manifest_provision *q=&m->provisions[j];pangu_u32 yes;if(contracts_same(&c,q->instance,p->name,&yes)) return 2;if(yes && contracts_declared(&c,p,q->service,q->version,1,0,(pangu_u32)j)) return report->status;}
  for(pangu_u64 j=0;j<m->requirement_count;j++) {const pangu_manifest_requirement *q=&m->requirements[j];pangu_u32 yes;if(contracts_same(&c,q->consumer,p->name,&yes)) return 2;if(yes && contracts_declared(&c,p,q->service,q->version,2,q->required,(pangu_u32)j)) return report->status;}
  for(pangu_u64 j=0;j<template_count;j++) {
   const pangu_module_contract *t=&templates[j];pangu_u32 yes;if(contracts_belongs(&c,t,p,&yes)) return 2;if(!yes) continue;pangu_u32 count=0;
   pangu_u64 end=t->role==1 ? m->provision_count : m->requirement_count;
   for(pangu_u64 k=0;k<end;k++) {
    const unsigned char *owner=t->role==1 ? m->provisions[k].instance : m->requirements[k].consumer,*service=t->role==1 ? m->provisions[k].service : m->requirements[k].service;pangu_u32 version=t->role==1 ? m->provisions[k].version : m->requirements[k].version;
    if(contracts_same(&c,owner,p->name,&yes)) return 2;if(!yes || version!=t->service_version) continue;if(contracts_same(&c,service,t->service,&yes)) return 2;if(yes) count++;
   }
   if(count!=1) return contracts_bad(&c,count ? 4 : 3,9,(pangu_u32)i);
  }
 }
 report->count=(pangu_u32)m->instance_count;return 0;
}
static int entries_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int entries_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
static int entries_equal(const unsigned char *a,const unsigned char *b,pangu_u64 n) {for(pangu_u64 i=0;i<n;i++) if(a[i]!=b[i]) return 0;return 1;}
typedef struct bind {pangu_plan_report *r;pangu_u64 fuel;} bind;
static pangu_i32 entries_bad(bind *c,pangu_i32 rc,pangu_u32 reason,pangu_u32 at) {c->r->status=rc;c->r->reason=reason;c->r->location=at;return rc;}
static pangu_i32 entries_charge(bind *c,pangu_u64 n) {if(n>c->fuel-c->r->work) return entries_bad(c,2,1,NONE);c->r->work+=n;return 0;}
static int entries_valid_name(const unsigned char *p)
{
 pangu_u32 zero=0;for(pangu_u32 i=0;i<32;i++) {unsigned char ch=p[i];if(!ch) {if(!i) return 0;zero=1;continue;}if(zero || !((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='.' || ch=='_' || ch=='-')) return 0;}return zero;
}
static pangu_i32 pangu_bind_static_entries(const pangu_static_manifest *m,const pangu_static_entry *table,pangu_u64 count,pangu_u64 max_work,pangu_u32 *indices,pangu_u64 capacity,pangu_plan_report *report)
{
 if(!m || !report || !entries_range_ok(m,112) || !entries_range_ok(report,32) || !m->module_count || m->module_count>NONE || count>NONE || capacity>UMAX/4 || !max_work || !entries_range_ok(m->modules,m->module_count*96) || !entries_range_ok(table,count*104) || !entries_range_ok(indices,capacity*4)) return 1;
 pangu_u64 a[3],n[3];a[0]=(pangu_u64)m;n[0]=112;a[1]=(pangu_u64)m->modules;n[1]=m->module_count*96;a[2]=(pangu_u64)table;n[2]=count*104;
 for(pangu_u32 i=0;i<3;i++) if(entries_overlap((pangu_u64)indices,capacity*4,a[i],n[i]) || entries_overlap((pangu_u64)report,32,a[i],n[i])) return 1;
 if(entries_overlap((pangu_u64)indices,capacity*4,(pangu_u64)report,32)) return 1;
 report->status=0;report->reason=0;report->work=0;report->realm=NONE;report->count=0;report->location=NONE;report->reserved=0;bind c;c.r=report;c.fuel=max_work;
 for(pangu_u64 i=0;i<count;i++) {
  const pangu_manifest_module *p=&table[i].module;if(entries_charge(&c,96)) return 2;
  if(!table[i].entry || !entries_valid_name(p->name) || !entries_valid_name(p->entry) || p->abi_version!=1 || p->struct_size!=96 || !p->version || p->target!=1 || p->host_min!=1 || p->host_max<1 || p->flags || p->reserved) return entries_bad(&c,16,12,(pangu_u32)i);
  for(pangu_u64 j=0;j<i;j++) {if(entries_charge(&c,32)) return 2;if(p->version==table[j].module.version && entries_equal(p->name,table[j].module.name,32)) return entries_bad(&c,4,12,(pangu_u32)i);}
 }
 for(pangu_u64 i=0;i<m->module_count;i++) {
  const pangu_manifest_module *p=&m->modules[i];pangu_u32 found=0;
  for(pangu_u64 j=0;j<count;j++) {if(entries_charge(&c,32)) return 2;if(p->version!=table[j].module.version || !entries_equal(p->name,table[j].module.name,32)) continue;found=1;if(entries_charge(&c,96)) return 2;if(!entries_equal((const unsigned char *)p,(const unsigned char *)&table[j].module,96)) return entries_bad(&c,16,13,(pangu_u32)i);}
  if(!found) return entries_bad(&c,3,12,(pangu_u32)i);
 }
 report->count=(pangu_u32)m->module_count;if(m->module_count>capacity) return entries_bad(&c,2,1,NONE);
 pangu_u64 comparisons=m->module_count*count;if(comparisons>(max_work-report->work)/32) return entries_bad(&c,2,1,NONE);report->work+=comparisons*32;
 for(pangu_u64 i=0;i<m->module_count;i++) for(pangu_u64 j=0;j<count;j++) if(m->modules[i].version==table[j].module.version && entries_equal(m->modules[i].name,table[j].module.name,32)) indices[i]=(pangu_u32)j;
 return 0;
}
static int canonical_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int canonical_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
static int canonical_bytes(const unsigned char *a,const unsigned char *b,pangu_u64 n) {for(pangu_u64 i=0;i<n;i++) if(a[i]!=b[i]) return 0;return 1;}
static int canonical_valid_name(const unsigned char *a)
{
 pangu_u32 zero=0;for(pangu_u32 i=0;i<32;i++) {unsigned char ch=a[i];if(!ch) {if(!i) return 0;zero=1;continue;}if(zero || !((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='.' || ch=='-' || ch=='_')) return 0;}return zero;
}
typedef struct canonical {pangu_plan_report *r;pangu_u64 fuel;} canonical;
static pangu_i32 canonical_bad(canonical *c,pangu_i32 rc,pangu_u32 at) {c->r->status=rc;c->r->reason=14;c->r->location=at;return rc;}
static pangu_i32 canonical_charge(canonical *c,pangu_u64 n) {if(n>c->fuel-c->r->work) return canonical_bad(c,2,NONE);c->r->work+=n;return 0;}
static pangu_i32 pangu_verify_installed_contracts(const pangu_static_manifest *m,const pangu_module_contract *source,pangu_u64 source_count,const pangu_static_entry *entries,const pangu_installed_contract_set *sets,pangu_u64 entry_count,pangu_u64 max_work,pangu_plan_report *report)
{
 if(!m || !report || !canonical_range_ok(m,112) || !canonical_range_ok(report,32) || !m->module_count || m->module_count>NONE || source_count>NONE || entry_count>NONE || !max_work) return 1;
 pangu_u64 a[5],n[5];a[0]=(pangu_u64)m;n[0]=112;a[1]=(pangu_u64)m->modules;n[1]=m->module_count*96;a[2]=(pangu_u64)source;n[2]=source_count*80;a[3]=(pangu_u64)entries;n[3]=entry_count*104;a[4]=(pangu_u64)sets;n[4]=entry_count*16;
 for(pangu_u32 i=0;i<5;i++) if(!canonical_range_ok((const void *)a[i],n[i]) || canonical_overlap((pangu_u64)report,32,a[i],n[i])) return 1;
 for(pangu_u64 i=0;i<entry_count;i++) if(sets[i].count>NONE || !canonical_range_ok(sets[i].records,sets[i].count*80) || canonical_overlap((pangu_u64)report,32,(pangu_u64)sets[i].records,sets[i].count*80)) return 1;
 report->status=0;report->reason=0;report->work=0;report->realm=NONE;report->count=0;report->location=NONE;report->reserved=0;canonical c;c.r=report;c.fuel=max_work;
 for(pangu_u64 i=0;i<entry_count;i++) {
  for(pangu_u64 j=0;j<sets[i].count;j++) {
   if(canonical_charge(&c,80)) return 2;const pangu_module_contract *t=&sets[i].records[j];
   if(!canonical_bytes(t->module,entries[i].module.name,32) || t->module_version!=entries[i].module.version || !canonical_valid_name(t->service) || !t->service_version || (t->role!=1 && t->role!=2) || t->required>1 || (t->role==1 && t->required)) return canonical_bad(&c,16,(pangu_u32)i);
   for(pangu_u64 k=0;k<j;k++) {if(canonical_charge(&c,32)) return 2;const pangu_module_contract *p=&sets[i].records[k];if(p->service_version==t->service_version && p->role==t->role && canonical_bytes(p->service,t->service,32)) return canonical_bad(&c,4,(pangu_u32)i);}
  }
 }
 for(pangu_u64 i=0;i<m->module_count;i++) {
  const pangu_manifest_module *module=&m->modules[i];pangu_u64 selected=NONE;
  for(pangu_u64 j=0;j<entry_count;j++) {if(canonical_charge(&c,32)) return 2;if(module->version==entries[j].module.version && canonical_bytes(module->name,entries[j].module.name,32)) {if(selected!=NONE) return canonical_bad(&c,4,(pangu_u32)i);selected=j;}}
  if(selected==NONE) return canonical_bad(&c,3,(pangu_u32)i);
  pangu_u64 matched=0;
  for(pangu_u64 j=0;j<source_count;j++) {
   if(canonical_charge(&c,32)) return 2;const pangu_module_contract *t=&source[j];if(t->module_version!=module->version || !canonical_bytes(t->module,module->name,32)) continue;pangu_u32 found=0;
   for(pangu_u64 k=0;k<sets[selected].count;k++) {if(canonical_charge(&c,80)) return 2;if(canonical_bytes((const unsigned char *)t,(const unsigned char *)&sets[selected].records[k],80)) found++;}
   if(found!=1) return canonical_bad(&c,16,(pangu_u32)j);matched++;
  }
  if(matched!=sets[selected].count) return canonical_bad(&c,16,(pangu_u32)i);
  /* Cardinality alone cannot detect source A,A replacing canonical A,B. */
  for(pangu_u64 j=0;j<sets[selected].count;j++) {
   pangu_u32 found=0;for(pangu_u64 k=0;k<source_count;k++) {if(canonical_charge(&c,80)) return 2;if(canonical_bytes((const unsigned char *)&sets[selected].records[j],(const unsigned char *)&source[k],80)) found++;}if(found!=1) return canonical_bad(&c,16,(pangu_u32)i);
  }
 }
 /* Extra templates for modules absent from configuration must not be ignored. */
 for(pangu_u64 i=0;i<source_count;i++) {pangu_u32 found=0;for(pangu_u64 j=0;j<m->module_count;j++) {if(canonical_charge(&c,32)) return 2;if(source[i].module_version==m->modules[j].version && canonical_bytes(source[i].module,m->modules[j].name,32)) found++;}if(found!=1) return canonical_bad(&c,16,(pangu_u32)i);}
 report->count=(pangu_u32)m->module_count;return 0;
}
static int prepare_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int prepare_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
static void prepare_unpublish(pangu_prepare_result *out)
{
 out->lowered.registry.scopes=(void *)0;out->lowered.registry.scope_count=0;out->lowered.registry.services=(void *)0;out->lowered.registry.service_count=0;out->lowered.registry.bindings=(void *)0;out->lowered.registry.binding_count=0;out->lowered.registry.providers=(void *)0;out->lowered.registry.provider_count=0;
 out->lowered.graph.registry=(void *)0;out->lowered.graph.instances=(void *)0;out->lowered.graph.instance_count=0;out->lowered.graph.provider_owners=(void *)0;out->lowered.graph.requirements=(void *)0;out->lowered.graph.requirement_count=0;
}
static pangu_i32 prepare_fail(pangu_prepare_result *out,pangu_i32 rc) {prepare_unpublish(out);return rc;}
static pangu_i32 pangu_host_prepare(const pangu_static_manifest *m,const pangu_module_contract *contracts,pangu_u64 contract_count,const pangu_manifest_storage *s,const pangu_graph_scratch *scratch,const pangu_graph_output *output,pangu_u64 max_work,pangu_prepare_result *out)
{
 if(!m || !s || !scratch || !output || !out || !prepare_range_ok(m,112) || !prepare_range_ok(s,88) || !prepare_range_ok(scratch,48) || !prepare_range_ok(output,48) || !prepare_range_ok(out,192) || !max_work || contract_count>NONE || !m->scope_count || !m->definition_count || !m->module_count || !m->instance_count) return 1;
 pangu_u64 counts[7],sizes[7],read[12],rn[12],write[15],wn[15];counts[0]=m->scope_count;counts[1]=m->definition_count;counts[2]=m->module_count;counts[3]=m->instance_count;counts[4]=m->binding_count;counts[5]=m->provision_count;counts[6]=m->requirement_count;sizes[0]=8;sizes[1]=44;sizes[2]=96;sizes[3]=72;sizes[4]=44;sizes[5]=72;sizes[6]=72;
 read[0]=(pangu_u64)m->scopes;read[1]=(pangu_u64)m->definitions;read[2]=(pangu_u64)m->modules;read[3]=(pangu_u64)m->instances;read[4]=(pangu_u64)m->bindings;read[5]=(pangu_u64)m->provisions;read[6]=(pangu_u64)m->requirements;
 for(pangu_u32 i=0;i<7;i++) {if(counts[i]>NONE) return 1;rn[i]=counts[i]*sizes[i];}
 read[7]=(pangu_u64)m;rn[7]=112;read[8]=(pangu_u64)contracts;rn[8]=contract_count*80;read[9]=(pangu_u64)s;rn[9]=88;read[10]=(pangu_u64)scratch;rn[10]=48;read[11]=(pangu_u64)output;rn[11]=48;
 if(s->service_capacity>UMAX/8 || s->instance_capacity>UMAX/8 || s->binding_capacity>UMAX/16 || s->provider_capacity>UMAX/16 || s->requirement_capacity>UMAX/16 || scratch->edge_capacity>NONE || output->order_capacity>UMAX/4 || output->state_capacity>UMAX/4 || output->binding_capacity>UMAX/8) return 1;
 write[0]=(pangu_u64)s->services;wn[0]=s->service_capacity*8;write[1]=(pangu_u64)s->instances;wn[1]=s->instance_capacity*8;write[2]=(pangu_u64)s->bindings;wn[2]=s->binding_capacity*16;write[3]=(pangu_u64)s->providers;wn[3]=s->provider_capacity*16;write[4]=(pangu_u64)s->owners;wn[4]=s->provider_capacity*4;write[5]=(pangu_u64)s->requirements;wn[5]=s->requirement_capacity*16;
 write[6]=(pangu_u64)scratch->pending;wn[6]=m->instance_count*4;write[7]=(pangu_u64)scratch->degree;wn[7]=m->instance_count*4;write[8]=(pangu_u64)scratch->queue;wn[8]=m->instance_count*4;write[9]=(pangu_u64)scratch->resolved;wn[9]=m->provision_count*4;write[10]=(pangu_u64)scratch->edges;wn[10]=scratch->edge_capacity*8;
 write[11]=(pangu_u64)output->order;wn[11]=output->order_capacity*4;write[12]=(pangu_u64)output->states;wn[12]=output->state_capacity*4;write[13]=(pangu_u64)output->bindings;wn[13]=output->binding_capacity*8;write[14]=(pangu_u64)out;wn[14]=192;
 for(pangu_u32 i=0;i<12;i++) if(!prepare_range_ok((const void *)read[i],rn[i])) return 1;
 for(pangu_u32 i=0;i<15;i++) {if(!prepare_range_ok((const void *)write[i],wn[i])) return 1;for(pangu_u32 j=0;j<12;j++) if(prepare_overlap(write[i],wn[i],read[j],rn[j])) return 1;for(pangu_u32 j=i+1;j<15;j++) if(prepare_overlap(write[i],wn[i],write[j],wn[j])) return 1;}
 prepare_unpublish(out);out->stage=1;out->reserved=0;out->work=0;out->graph_report.status=0;out->graph_report.reason=0;out->graph_report.work=0;out->graph_report.order_count=0;out->graph_report.pending_count=0;out->graph_report.binding_count=0;out->graph_report.location=NONE;
 pangu_i32 rc=pangu_manifest_lower(m,s,max_work,&out->lowered);if(out->lowered.report.work>max_work) return prepare_fail(out,10);out->work=out->lowered.report.work;if(rc) return prepare_fail(out,rc);max_work-=out->lowered.report.work;
 out->stage=2;if(!max_work) {out->lowered.report.status=2;out->lowered.report.reason=1;out->lowered.report.work=0;return prepare_fail(out,2);}
 rc=pangu_manifest_contracts(m,contracts,contract_count,max_work,&out->lowered.report);if(out->lowered.report.work>max_work) return prepare_fail(out,10);out->work+=out->lowered.report.work;max_work-=out->lowered.report.work;if(rc) return prepare_fail(out,rc);
 out->stage=3;if(!max_work) {out->graph_report.status=2;return prepare_fail(out,2);}
 rc=pangu_graph_plan(&out->lowered.graph,scratch,output,max_work,&out->graph_report);if(out->graph_report.work>max_work) return prepare_fail(out,10);out->work+=out->graph_report.work;if(rc) return prepare_fail(out,rc);
 out->stage=4;return 0;
}
static int discovery_range_ok(const void *p,pangu_u64 n) {return (!n || p) && n<=UMAX-(pangu_u64)p;}
static int discovery_overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m) {return n && m && (a<=b ? b-a<n : a-b<m);}
static void discovery_unpublish(pangu_prepare_result *out)
{
 out->lowered.registry.scopes=(void *)0;out->lowered.registry.scope_count=0;out->lowered.registry.services=(void *)0;out->lowered.registry.service_count=0;out->lowered.registry.bindings=(void *)0;out->lowered.registry.binding_count=0;out->lowered.registry.providers=(void *)0;out->lowered.registry.provider_count=0;
 out->lowered.graph.registry=(void *)0;out->lowered.graph.instances=(void *)0;out->lowered.graph.instance_count=0;out->lowered.graph.provider_owners=(void *)0;out->lowered.graph.requirements=(void *)0;out->lowered.graph.requirement_count=0;
}
static pangu_i32 discovery_fail(pangu_prepare_result *out,pangu_i32 rc) {discovery_unpublish(out);return rc;}
pangu_i32 pangu_discover_prepare(const pangu_static_manifest *m,const pangu_module_contract *contracts,pangu_u64 contract_count,const pangu_installed_table *installed,const pangu_discovery_storage *storage,pangu_u64 max_work,pangu_prepare_result *out)
{
 if(!installed || !storage || !discovery_range_ok(installed,24) || !discovery_range_ok(storage,48) || installed->count>NONE || storage->entry_capacity>UMAX/4) return 1;
 const pangu_manifest_storage *s=storage->lower;const pangu_graph_scratch *scratch=storage->scratch;const pangu_graph_output *output=storage->output;
 if(!m || !s || !scratch || !output || !out || !discovery_range_ok(m,112) || !discovery_range_ok(s,88) || !discovery_range_ok(scratch,48) || !discovery_range_ok(output,48) || !discovery_range_ok(out,192) || !max_work || contract_count>NONE || !m->scope_count || !m->definition_count || !m->module_count || !m->instance_count) return 1;
 pangu_u64 counts[7],sizes[7],read[16],rn[16],write[17],wn[17];counts[0]=m->scope_count;counts[1]=m->definition_count;counts[2]=m->module_count;counts[3]=m->instance_count;counts[4]=m->binding_count;counts[5]=m->provision_count;counts[6]=m->requirement_count;sizes[0]=8;sizes[1]=44;sizes[2]=96;sizes[3]=72;sizes[4]=44;sizes[5]=72;sizes[6]=72;
 read[0]=(pangu_u64)m->scopes;read[1]=(pangu_u64)m->definitions;read[2]=(pangu_u64)m->modules;read[3]=(pangu_u64)m->instances;read[4]=(pangu_u64)m->bindings;read[5]=(pangu_u64)m->provisions;read[6]=(pangu_u64)m->requirements;
 for(pangu_u32 i=0;i<7;i++) {if(counts[i]>NONE) return 1;rn[i]=counts[i]*sizes[i];}
 read[7]=(pangu_u64)m;rn[7]=112;read[8]=(pangu_u64)contracts;rn[8]=contract_count*80;read[9]=(pangu_u64)s;rn[9]=88;read[10]=(pangu_u64)scratch;rn[10]=48;read[11]=(pangu_u64)output;rn[11]=48;
 if(s->service_capacity>UMAX/8 || s->instance_capacity>UMAX/8 || s->binding_capacity>UMAX/16 || s->provider_capacity>UMAX/16 || s->requirement_capacity>UMAX/16 || scratch->edge_capacity>NONE || output->order_capacity>UMAX/4 || output->state_capacity>UMAX/4 || output->binding_capacity>UMAX/8) return 1;
 write[0]=(pangu_u64)s->services;wn[0]=s->service_capacity*8;write[1]=(pangu_u64)s->instances;wn[1]=s->instance_capacity*8;write[2]=(pangu_u64)s->bindings;wn[2]=s->binding_capacity*16;write[3]=(pangu_u64)s->providers;wn[3]=s->provider_capacity*16;write[4]=(pangu_u64)s->owners;wn[4]=s->provider_capacity*4;write[5]=(pangu_u64)s->requirements;wn[5]=s->requirement_capacity*16;
 write[6]=(pangu_u64)scratch->pending;wn[6]=m->instance_count*4;write[7]=(pangu_u64)scratch->degree;wn[7]=m->instance_count*4;write[8]=(pangu_u64)scratch->queue;wn[8]=m->instance_count*4;write[9]=(pangu_u64)scratch->resolved;wn[9]=m->provision_count*4;write[10]=(pangu_u64)scratch->edges;wn[10]=scratch->edge_capacity*8;
 write[11]=(pangu_u64)output->order;wn[11]=output->order_capacity*4;write[12]=(pangu_u64)output->states;wn[12]=output->state_capacity*4;write[13]=(pangu_u64)output->bindings;wn[13]=output->binding_capacity*8;write[14]=(pangu_u64)out;wn[14]=192;
 read[12]=(pangu_u64)installed;rn[12]=24;read[13]=(pangu_u64)installed->entries;rn[13]=installed->count*104;read[14]=(pangu_u64)installed->sets;rn[14]=installed->count*16;read[15]=(pangu_u64)storage;rn[15]=48;
 write[15]=(pangu_u64)storage->entry_scratch;wn[15]=storage->entry_capacity*4;write[16]=(pangu_u64)storage->entry_output;wn[16]=storage->entry_capacity*4;
 for(pangu_u32 i=0;i<16;i++) if(!discovery_range_ok((const void *)read[i],rn[i])) return 1;
 for(pangu_u32 i=0;i<17;i++) {if(!discovery_range_ok((const void *)write[i],wn[i])) return 1;for(pangu_u32 j=0;j<16;j++) if(discovery_overlap(write[i],wn[i],read[j],rn[j])) return 1;for(pangu_u32 j=i+1;j<17;j++) if(discovery_overlap(write[i],wn[i],write[j],wn[j])) return 1;}
 for(pangu_u64 i=0;i<installed->count;i++) {
  const pangu_installed_contract_set *set=&installed->sets[i];if(set->count>NONE || !discovery_range_ok(set->records,set->count*80)) return 1;
  for(pangu_u32 j=0;j<17;j++) if(discovery_overlap(write[j],wn[j],(pangu_u64)set->records,set->count*80)) return 1;
 }
 discovery_unpublish(out);out->stage=1;out->reserved=0;out->work=0;out->graph_report.status=0;out->graph_report.reason=0;out->graph_report.work=0;out->graph_report.order_count=0;out->graph_report.pending_count=0;out->graph_report.binding_count=0;out->graph_report.location=NONE;
 pangu_i32 rc=pangu_bind_static_entries(m,installed->entries,installed->count,max_work,storage->entry_scratch,storage->entry_capacity,&out->lowered.report);
 if(out->lowered.report.work>max_work) return discovery_fail(out,10);out->work=out->lowered.report.work;if(rc) return discovery_fail(out,rc);max_work-=out->lowered.report.work;
 out->stage=2;if(!max_work) {out->lowered.report.status=2;out->lowered.report.work=0;return discovery_fail(out,2);}
 rc=pangu_verify_installed_contracts(m,contracts,contract_count,installed->entries,installed->sets,installed->count,max_work,&out->lowered.report);
 if(out->lowered.report.work>max_work) return discovery_fail(out,10);out->work+=out->lowered.report.work;if(rc) return discovery_fail(out,rc);max_work-=out->lowered.report.work;
 out->stage=3;
 /* Reserve final entry-map commit before graph planner can modify final arrays. */
 if(max_work<=m->module_count) {out->lowered.report.status=2;out->lowered.report.work=0;return discovery_fail(out,2);}
 pangu_u64 before=out->work;out->work=0;out->stage=1;rc=pangu_host_prepare(m,contracts,contract_count,s,scratch,output,max_work-m->module_count,out);
 out->work+=before;out->stage+=2;if(rc) return discovery_fail(out,rc);
 for(pangu_u64 i=0;i<m->module_count;i++) storage->entry_output[i]=storage->entry_scratch[i];out->work+=m->module_count;return 0;
}
