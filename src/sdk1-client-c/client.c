#include "src/t14-abi/sdk1-fixture.h"
/* Same source separately compiled as freestanding C17 and C++20. No owner,
 * runtime, route or entry-control declarations are imported here. */
#define REQUIRE(cond) do {if(!(cond)) {cfg->fail_line=__LINE__;return -101;}} while(0)
static pangu_sdk_i32 consume(const pangu_plugin_context *c,pangu_sdk_u32 stage) {
 sdk_fixture_config *cfg=(sdk_fixture_config *)c->userdata;
 for(pangu_sdk_u32 i=0;i<cfg->dependency_count;i++) {
  pangu_plugin_count count;
  if(cfg->flags&128) {REQUIRE(cfg->budget_unchanged(c,0));for(pangu_sdk_u64 fuel=0;;fuel++) {REQUIRE(fuel<100000);count.count=count.work=0xa5a5a5a5a5a5a5a5ULL;pangu_sdk_i32 rc=sdk1_abi_count(c->host,c,&cfg->dependencies[i].key,fuel,&count);if(!rc) {break;}REQUIRE(rc==2 && count.count==0xa5a5a5a5a5a5a5a5ULL && count.work==0xa5a5a5a5a5a5a5a5ULL && cfg->budget_unchanged(c,1));cfg->reserved++;}}
  else {REQUIRE(!sdk1_abi_count(c->host,c,&cfg->dependencies[i].key,100000,&count));}REQUIRE(count.count==cfg->dependencies[i].expected_count && count.work<=100000);
  for(pangu_sdk_u32 ordinal=0;ordinal<count.count;ordinal++) {
   pangu_plugin_request q;q.key=cfg->dependencies[i].key;q.ordinal=ordinal;q.minimum_size=24;pangu_plugin_lease lease;pangu_plugin_report report;
   if(cfg->flags&128) {REQUIRE(cfg->budget_unchanged(c,0));for(pangu_sdk_u64 fuel=0;;fuel++) {REQUIRE(fuel<100000);unsigned char *bytes=(unsigned char *)&lease;for(pangu_sdk_u32 j=0;j<sizeof lease;j++) {bytes[j]=0xa5;}report.reserved=0xa5a5a5a5U;pangu_sdk_i32 rc=sdk1_abi_acquire(c->host,c,&q,fuel,&lease,&report);if(!rc) {REQUIRE(report.work<=fuel);break;}REQUIRE(rc==2 && cfg->budget_unchanged(c,1));for(pangu_sdk_u32 j=0;j<sizeof lease;j++) {REQUIRE(bytes[j]==0xa5);}if(report.reserved==0) {REQUIRE(report.pending==1 && report.reason==1 && report.work<=fuel);}cfg->reserved++;}}
   else {REQUIRE(!sdk1_abi_acquire(c->host,c,&q,100000,&lease,&report));}REQUIRE(report.work<=100000);
   pangu_sdk_i32 result=0;REQUIRE(!sdk1_abi_invoke(c,lease.table,lease.state,q.key.version,17,&result));REQUIRE(result==cfg->expected(c,&lease,&q.key,ordinal,stage)+17);
   REQUIRE(!sdk1_abi_release(c->host,c,&lease,100000));REQUIRE(sdk1_abi_release(c->host,c,&lease,100000)==10);
  }
 }
 return 0;
}
pangu_sdk_i32 PANGU_SDK_FIXTURE_LOAD(const pangu_plugin_context *c) {
 sdk_fixture_config *cfg=(sdk_fixture_config *)c->userdata;
 for(pangu_sdk_u32 i=0;i<cfg->provision_count;i++) {sdk_fixture_provision *p=&cfg->provisions[i];if(cfg->flags&128) {REQUIRE(cfg->budget_unchanged(c,0));for(pangu_sdk_u64 fuel=0;;fuel++) {REQUIRE(fuel<100000);pangu_sdk_i32 rc=sdk1_abi_provide(c->host,c,&p->request,p->table,p->state,fuel);if(!rc) {break;}REQUIRE(rc==2 && cfg->budget_unchanged(c,1));cfg->reserved++;}}
  else {REQUIRE(!sdk1_abi_provide(c->host,c,&p->request,p->table,p->state,100000));}}
 REQUIRE(!consume(c,1));cfg->loads++;if(cfg->flags&8) {return -81;}return 0;
}
pangu_sdk_i32 PANGU_SDK_FIXTURE_ENTRY(const pangu_plugin_context *c) {
 sdk_fixture_config *cfg=(sdk_fixture_config *)c->userdata;cfg->entries++;cfg->saved_context=c;
 REQUIRE(c->abi_version==1 && c->struct_size==32 && c->host->abi_version==1 && c->host->struct_size==72 && c->host->target==1 && !c->host->reserved);
 if(cfg->flags&1) {REQUIRE(!sdk1_abi_defer(c->host,c,100000,&cfg->setup_token));if(cfg->flags&2) {REQUIRE(!PANGU_SDK_FIXTURE_LOAD(c));REQUIRE(!sdk1_abi_finish_setup(c->host,c,&cfg->setup_token,0,100000));}return 5;}
 return PANGU_SDK_FIXTURE_LOAD(c);
}
pangu_sdk_i32 PANGU_SDK_FIXTURE_DISPOSE(const pangu_plugin_context *c,const pangu_plugin_completion *token) {
 sdk_fixture_config *cfg=(sdk_fixture_config *)c->userdata;cfg->cleanups++;cfg->saved_context=c;REQUIRE(!consume(c,2));
 if(token) {cfg->cleanup_token=*token;if(cfg->flags&2) {REQUIRE(!sdk1_abi_finish_dispose(c->host,c,&cfg->cleanup_token,(cfg->flags&16) ? -82 : 0,100000));}return 5;}
 return (cfg->flags&16) ? -82 : 0;
}

pangu_sdk_i32 PANGU_SDK_FIXTURE_USE(const pangu_plugin_context *c) {return consume(c,3);}
