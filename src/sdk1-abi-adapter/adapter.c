extern __attribute__((visibility("hidden"))) int pangu_route_cpp_evaluate(void *,int,int *);
#include "adapter.h"
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk_cpp_fixture_entry(const pangu_plugin_context *c);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk_cpp_fixture_load(const pangu_plugin_context *c);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk_cpp_fixture_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *token);
extern __attribute__((visibility("hidden"))) pangu_sdk_i32 pangu_sdk_cpp_fixture_use(const pangu_plugin_context *c);

#include "src/t14-test-support/assert.h"
#include "src/t14-test-support/stdio.h"
pangu_sdk_i32 pangu_sdk1_abi_c_entry(const pangu_plugin_context *c){pangu_u64 a[]={(pangu_u64)c};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_fixture_entry,a,1,104);}
pangu_sdk_i32 pangu_sdk1_abi_c_load(const pangu_plugin_context *c){pangu_u64 a[]={(pangu_u64)c};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_fixture_load,a,1,105);}
pangu_sdk_i32 pangu_sdk1_abi_c_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *token){pangu_u64 a[]={(pangu_u64)c,(pangu_u64)token};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_fixture_dispose,a,2,106);}
pangu_sdk_i32 pangu_sdk1_abi_c_use(const pangu_plugin_context *c){pangu_u64 a[]={(pangu_u64)c};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_fixture_use,a,1,107);}
pangu_sdk_i32 pangu_sdk1_abi_cpp_entry(const pangu_plugin_context *c){pangu_u64 a[]={(pangu_u64)c};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_cpp_fixture_entry,a,1,108);}
pangu_sdk_i32 pangu_sdk1_abi_cpp_load(const pangu_plugin_context *c){pangu_u64 a[]={(pangu_u64)c};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_cpp_fixture_load,a,1,109);}
pangu_sdk_i32 pangu_sdk1_abi_cpp_dispose(const pangu_plugin_context *c,const pangu_plugin_completion *token){pangu_u64 a[]={(pangu_u64)c,(pangu_u64)token};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_cpp_fixture_dispose,a,2,110);}
pangu_sdk_i32 pangu_sdk1_abi_cpp_use(const pangu_plugin_context *c){pangu_u64 a[]={(pangu_u64)c};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_sdk_cpp_fixture_use,a,1,111);}
pangu_i32 pangu_sdk1_guard_eval(pangu_interop_method fn,void *state,pangu_i32 input,pangu_i32 *out){pangu_u64 a[]={(pangu_u64)state,(pangu_u64)input,(pangu_u64)out};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)fn,a,3,114);}
pangu_i32 pangu_sdk1_guard_cpp_eval(void *state,pangu_i32 input,pangu_i32 *out){pangu_u64 a[]={(pangu_u64)state,(pangu_u64)input,(pangu_u64)out};return (pangu_i32)pangu_abi_checked((pangu_abi_fn)pangu_route_cpp_evaluate,a,3,113);}
void pangu_sdk1_guard_report(void){pangu_u64 value[3],total=0;for(pangu_u32 i=0;i<128;i++){pangu_abi_counts(i,value);total+=value[0];}printf("sdk1_abi_calls=%d targets=18 corruption_cases=10 forwarding_cases=2\n",(int)total);
pangu_abi_counts(96,value);assert(value[0]);
pangu_abi_counts(97,value);assert(value[0]);
pangu_abi_counts(98,value);assert(value[0]);
pangu_abi_counts(99,value);assert(value[0]);
pangu_abi_counts(100,value);assert(value[0]);
pangu_abi_counts(101,value);assert(value[0]);
pangu_abi_counts(102,value);assert(value[0]);
pangu_abi_counts(104,value);assert(value[0]);
pangu_abi_counts(105,value);assert(value[0]);
pangu_abi_counts(106,value);assert(value[0]);
pangu_abi_counts(107,value);assert(value[0]);
pangu_abi_counts(108,value);assert(value[0]);
pangu_abi_counts(109,value);assert(value[0]);
pangu_abi_counts(110,value);assert(value[0]);
pangu_abi_counts(111,value);assert(value[0]);
pangu_abi_counts(112,value);assert(value[0]);
pangu_abi_counts(113,value);assert(value[0]);
pangu_abi_counts(114,value);assert(value[0]);
pangu_abi_counts(38,value);assert(value[1]);pangu_abi_counts(39,value);assert(value[1]);
for(pangu_u32 i=0;i<128;i++){pangu_abi_counts(i,value);if(value[0])printf("sdk1_abi_slot=%d calls=%d zero=%d other=%d\n",(int)i,(int)value[0],(int)value[1],(int)value[2]);}}
