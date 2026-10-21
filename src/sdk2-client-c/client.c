#include "src/t14-abi/sdk2-fixture.h"
/* Independently compiled C17/C++20 plugin. Method ABI includes an explicit
 * incoming capability, assigned a distinct service version by the fixture. */
#define REQUIRE(x) do {if(!(x)) {cfg->fail_line=__LINE__;return -101;}} while(0)
static pangu_sdk_i32 invoke(const pangu_plugin_context *c,const pangu_plugin_key *key,pangu_sdk_u32 ordinal,const pangu_plugin_lease *lease) {
 sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;const sdk2_test_service *table=(const sdk2_test_service *)lease->table;
 REQUIRE(table && table->version==key->version && table->struct_size>=24 && table->target==1 && !table->reserved && table->evaluate);
 pangu_sdk_i32 value=0;pangu_sdk_i32 rc=table->evaluate(lease->state,lease,17,&value);if(rc) {return rc;}REQUIRE(cfg->expected && value==cfg->expected(c,lease,key,ordinal)+17);return 0;
}
pangu_sdk_i32 SDK2_EVAL(void *opaque,const pangu_plugin_lease *incoming,pangu_sdk_i32 input,pangu_sdk_i32 *out) {
 if(!opaque || !incoming || !out) {return 1;}sdk2_service_state *state=(sdk2_service_state *)opaque;const pangu_plugin_context *c=state->context;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);if(!api || incoming->state!=state || incoming->table!=state->table) {return 10;}
 pangu_sdk_i32 rc=api->accept(c,incoming,100000);if(rc) {return rc;}sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;REQUIRE(cfg->entered);cfg->entered(c,incoming,state);
 for(pangu_sdk_u32 i=0;i<cfg->dependency_count;i++) {pangu_plugin_count n;REQUIRE(!api->base.count(c,&cfg->dependencies[i].key,100000,&n));REQUIRE(n.count==cfg->dependencies[i].expected_count);
  for(pangu_sdk_u32 j=0;j<n.count;j++) {pangu_plugin_request q;q.key=cfg->dependencies[i].key;q.ordinal=j;q.minimum_size=24;pangu_plugin_lease child;pangu_plugin_report report;
   rc=api->acquire_child(c,incoming,&q,100000,&child,&report);if(rc) {return rc;}REQUIRE(report.work<=100000);rc=invoke(c,&q.key,j,&child);pangu_sdk_i32 released=api->base.release(c,&child,100000);REQUIRE(!released);if(rc) {return rc;}
  }
 }
 long long value=(long long)state->value+input;if(value<-2147483648LL || value>2147483647LL) {return 11;}*out=(pangu_sdk_i32)value;state->calls++;return 0;
}
static pangu_sdk_i32 consume(const pangu_plugin_context *c,int hold) {
 sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);REQUIRE(api);
 for(pangu_sdk_u32 i=0;i<cfg->dependency_count;i++) {pangu_plugin_count n;REQUIRE(!api->base.count(c,&cfg->dependencies[i].key,100000,&n));REQUIRE(n.count==cfg->dependencies[i].expected_count);
  for(pangu_sdk_u32 j=0;j<n.count;j++) {pangu_plugin_request q;q.key=cfg->dependencies[i].key;q.ordinal=j;q.minimum_size=24;pangu_plugin_lease lease;pangu_plugin_report report;REQUIRE(!api->base.acquire(c,&q,100000,&lease,&report));REQUIRE(report.work<=100000);
   if(hold && !cfg->pending) {cfg->pending_root=lease;cfg->pending_dependency=i;cfg->pending_ordinal=j;cfg->pending=1;continue;}
   pangu_sdk_i32 rc=invoke(c,&q.key,j,&lease);REQUIRE(!api->base.release(c,&lease,100000));if(rc) {return rc;}
  }
 }
 return 0;
}
pangu_sdk_i32 SDK2_DRAIN(const pangu_plugin_context *c) {
 sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);REQUIRE(api && cfg->pending);pangu_plugin_key *key=&cfg->dependencies[cfg->pending_dependency].key;
 pangu_sdk_i32 rc=invoke(c,key,cfg->pending_ordinal,&cfg->pending_root);REQUIRE(!api->base.release(c,&cfg->pending_root,100000));cfg->pending=0;return rc;
}
pangu_sdk_i32 SDK2_LOAD(const pangu_plugin_context *c) {
 sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);REQUIRE(api);
 for(pangu_sdk_u32 i=0;i<cfg->provision_count;i++) {sdk2_provision *p=&cfg->provisions[i];p->state->context=c;p->state->table=p->table;REQUIRE(!api->base.provide(c,&p->request,p->table,p->state,100000));}
 REQUIRE(!consume(c,0));cfg->loads++;return (cfg->flags&8) ? -81 : 0;
}
pangu_sdk_i32 SDK2_ENTRY(const pangu_plugin_context *c) {
 sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;cfg->entries++;cfg->saved_context=c;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);REQUIRE(api);
 if(cfg->flags&1) {REQUIRE(!api->base.defer(c,100000,&cfg->setup_token));if(cfg->flags&2) {REQUIRE(!SDK2_LOAD(c));REQUIRE(!api->base.finish_setup(c,&cfg->setup_token,0,100000));}else {REQUIRE(!consume(c,1));}return 5;}return SDK2_LOAD(c);
}
pangu_sdk_i32 SDK2_DISPOSE(const pangu_plugin_context *c,const pangu_plugin_completion *completion) {
 sdk2_fixture_config *cfg=(sdk2_fixture_config *)c->userdata;const pangu_plugin_sdk2 *api=pangu_plugin_sdk2_view(c);REQUIRE(api);cfg->cleanups++;cfg->saved_context=c;
 if(completion) {cfg->cleanup_token=*completion;REQUIRE(!consume(c,1));return 5;}REQUIRE(!consume(c,0));return (cfg->flags&16) ? -82 : 0;
}
pangu_sdk_i32 SDK2_USE(const pangu_plugin_context *c) {return consume(c,0);}
