#ifdef PG_TASK_FIXTURE
#include "../../src/runtime/task_provider.h"
#include "../../src/runtime/extensions.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../../src/runtime/native_plugin.h"
#include <json-c/json.h>
static int initial(void *context,const struct json_object *input,struct json_object **out){(void)context;(void)input;*out=json_tokener_parse("{\"phase\":\"work\",\"step\":0}");return *out?0:5;}
static unsigned active_tasks;
static pg_task_status stage_checkpoint(void *context,pg_record_tx *records,pg_document_tx *docs,struct json_object **state){(void)context;(void)records;(void)docs;*state=json_tokener_parse("{\"status\":\"running\",\"checkpoint\":{\"phase\":\"work\",\"step\":1}}");return *state?PG_TASK_PROGRESS:PG_TASK_ALLOCATION;}
static void cleanup(void *context,pg_task_runner *runner){(void)context;if(runner->operation){runner->operation=0;active_tasks--;}}
static int poll_task(void *context,pg_task_runner *runner,const struct json_object *record){
 (void)context;if(!runner->operation){runner->operation=&active_tasks;active_tasks++;}struct json_object *checkpoint=json_object_object_get(json_object_object_get(record,"state"),"checkpoint");
 int step=json_object_get_int(json_object_object_get(checkpoint,"step"));
 if(!step){int result=pg_task_runner_commit_with(runner,0,stage_checkpoint);return result?-(int)result:0;}
 struct json_object *next=json_tokener_parse("{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":\"EXTENSION_TASK_DONE\"}}");
 if(!next)return -5;int result=pg_task_runner_commit(runner,next);json_object_put(next);return result?-(int)result:0;
}
static const pg_task_phase phases[]={{{"work",4},0,poll_task,cleanup}};
static const pg_task_definition definition={.name={"example.work",12},.version=1,.initial=initial,.phases=phases,.phase_count=1};
static const pg_task_definition *definitions[]={&definition};
static const pg_task_provider provider={PG_TASK_PROVIDER_ABI,sizeof(pg_task_provider),definitions,1};
static int allow(void *context,pg_slice arguments){(void)context;(void)arguments;return 1;}
static int echo(void *context,pg_slice arguments,char *out,size_t capacity,size_t *size){(void)context;(void)arguments;const char *text="EXTENSION_TOOL_RESULT";*size=strlen(text);if(*size>=capacity){*size=0;return PG_TOOL_ERROR;}memcpy(out,text,*size+1);return PG_TOOL_OK;}
static const pg_tool_definition echo_tool={"native_echo","Native extension echo","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}",0,allow,allow,echo,1,0};
static int render(void *context,const struct json_object *configuration,struct json_object **out){(void)configuration;*out=json_object_new_string(context);return *out?0:5;}
static int tool_wrap(void *context,const pg_tool_definition *inner,pg_tool_definition *out){*out=*inner;out->description=context;return 0;}
static int section_wrap(void *context,const pg_extension_section *inner,pg_extension_section *out){*out=*inner;out->context=context;return 0;}
static int failed_render(void *context,const struct json_object *configuration,struct json_object **out){(void)context;(void)configuration;*out=0;return -7;}
static const pg_extension_section broken_sections[]={{"failure",0,failed_render,0,0}};
static int broken_wrap(void *context,const pg_tool_definition *inner,pg_tool_definition *out){(void)context;*out=*inner;out->name="renamed";return 0;}
static const pg_extension_section reviewer_sections[]={{"preamble","EXTENSION_REVIEW_TOKEN",render,0,0},{"shared","FIRST_SHARED",render,1,0}};
static const pg_extension_section timing_sections[]={{"shared","EXTENSION_SHARED_TOKEN",render,1,0}};
static const pg_extension_wrap timing_wraps[]={{PG_EXTENSION_WRAP_TOOL,0,"native_echo","Wrapped native echo",{.tool=tool_wrap}},{PG_EXTENSION_WRAP_SECTION,0,"shared","EXTENSION_WRAPPED_TOKEN",{.section=section_wrap}}};
static const pg_extension_wrap broken_wraps[]={{PG_EXTENSION_WRAP_TOOL,0,"native_echo",0,{.tool=broken_wrap}}};
static struct {unsigned errors,before,after,cancels,requests,responses,yields,rounds,generation_cancels,generation_pending;} hook_stats;
static const json_object *pending_before,*pending_after;
static int policy_before(void *context,const json_object *request,json_object **decision){
 (void)context;hook_stats.before++;*decision=0;json_object *call=json_object_object_get(request,"call"),*args=json_object_object_get(call,"arguments");const char *name=json_object_get_string(json_object_object_get(call,"name")),*path=json_object_get_string(json_object_object_get(args,"path"));if(!name||strcmp(name,"write")||!path)return 0;
 if(!strcmp(path,"invalid-rewrite.txt")){*decision=json_tokener_parse("{\"arguments\":{\"path\":\"no-content.txt\"}}");return 0;}
 if(!strcmp(path,"blocked.txt")){*decision=json_tokener_parse("{\"block\":\"fixture policy\"}");return 0;}if(!strcmp(path,"throw.txt"))return -11;
 if(!strcmp(path,"cancelled.txt")){if(pending_before)assert(pending_before==request);pending_before=request;return 1;}
 if(!strcmp(path,"original.txt")){if(!pending_before){pending_before=request;return 1;}assert(pending_before==request);pending_before=0;*decision=json_tokener_parse("{\"arguments\":{\"path\":\"hooked.txt\",\"content\":\"HOOK_ARGUMENTS\"}}");}return 0;
}
static int policy_after(void *context,const json_object *request,json_object **decision){
 (void)context;hook_stats.after++;*decision=0;json_object *call=json_object_object_get(request,"call"),*args=json_object_object_get(call,"arguments");const char *path=json_object_get_string(json_object_object_get(args,"path"));if(!path)return 0;
 if(!strcmp(path,"after-error.txt"))return -13;
 if(!strcmp(path,"invalid-after.txt")){*decision=json_tokener_parse("{\"output\":1}");return 0;}
 if(!strcmp(path,"after-pending.txt")){if(!pending_after){pending_after=request;return 1;}assert(pending_after==request);pending_after=0;}
 if(!strcmp(path,"hooked.txt")||!strcmp(path,"after-pending.txt"))*decision=json_tokener_parse("{\"output\":\"HOOK_AFTER_RESULT\",\"isError\":false,\"details\":{\"hook\":true}}");return 0;
}
static void policy_cancel(void *context,const json_object *request){(void)context;assert(request==pending_before||request==pending_after);if(request==pending_before)pending_before=0;if(request==pending_after)pending_after=0;hook_stats.cancels++;}
static const pg_extension_hook policy_hooks[]={{PG_EXTENSION_BEFORE_TOOL,0,0,policy_before,policy_cancel},{PG_EXTENSION_AFTER_TOOL,0,0,policy_after,policy_cancel}};
static int timing_before(void *context,const json_object *request,json_object **decision){(void)context;*decision=0;json_object *args=json_object_object_get(json_object_object_get(request,"call"),"arguments");const char *path=json_object_get_string(json_object_object_get(args,"path"));if(path&&!strcmp(path,"hooked.txt"))assert(!strcmp(json_object_get_string(json_object_object_get(args,"content")),"HOOK_ARGUMENTS"));return 0;}
static int timing_after(void *context,const json_object *request,json_object **decision){(void)context;*decision=0;json_object *args=json_object_object_get(json_object_object_get(request,"call"),"arguments");const char *path=json_object_get_string(json_object_object_get(args,"path"));if(path&&!strcmp(path,"hooked.txt")){assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(request,"result"),"output")),"HOOK_AFTER_RESULT"));assert(!json_object_deep_copy(json_object_object_get(request,"result"),decision,0));json_object_object_add(*decision,"output",json_object_new_string("HOOK_AFTER_CHAINED"));}return 0;}
static const pg_extension_hook timing_hooks[]={{PG_EXTENSION_BEFORE_TOOL,0,0,timing_before,0},{PG_EXTENSION_AFTER_TOOL,0,0,timing_after,0}};
static const pg_extension policy={"policy",0,0,0,0,0,0,policy_hooks,2};
static const json_object *pending_request,*pending_response,*pending_yield;
static int generation_before(void *context,const json_object *request,json_object **decision){
 (void)context;*decision=0;if(!pending_request){pending_request=request;hook_stats.generation_pending=1;return 1;}assert(pending_request==request);pending_request=0;hook_stats.generation_pending=0;hook_stats.requests++;json_object *messages=0;assert(!json_object_deep_copy(json_object_object_get(request,"messages"),&messages,0));json_object *message=json_tokener_parse("{\"role\":\"user\",\"content\":\"The transient request marker is BEFORE_REQUEST_MARKER. Keep following the user's task.\"}");assert(messages&&message&&!json_object_array_add(messages,message));*decision=json_object_new_object();assert(*decision&&!json_object_object_add(*decision,"messages",messages));return 0;
}
static int generation_after(void *context,const json_object *request,json_object **decision){(void)context;*decision=0;if(!pending_response){pending_response=request;return 1;}assert(pending_response==request);pending_response=0;assert(hook_stats.responses<hook_stats.requests&&json_object_is_type(json_object_object_get(request,"message"),json_type_object));hook_stats.responses++;return 0;}
static int generation_yield(void *context,const json_object *request,json_object **decision){
 (void)context;*decision=0;const char *message=json_object_to_json_string(json_object_object_get(request,"message"));if(strstr(message,"YIELD_START")){if(!pending_yield){pending_yield=request;return 1;}assert(pending_yield==request);pending_yield=0;*decision=json_tokener_parse("{\"continue\":\"Reply only YIELD_END.\"}");}hook_stats.yields++;return 0;
}
static int generation_tools(void *context,const json_object *request,json_object **decision){(void)context;*decision=0;json_object *results=json_object_object_get(request,"results");assert(json_object_is_type(results,json_type_array)&&json_object_array_length(results)>0&&hook_stats.responses>hook_stats.rounds);hook_stats.rounds++;return 0;}
static void generation_cancel(void *context,const json_object *request){(void)context;assert(request==pending_request||request==pending_response||request==pending_yield);if(request==pending_request){pending_request=0;hook_stats.generation_pending=0;}if(request==pending_response)pending_response=0;if(request==pending_yield)pending_yield=0;hook_stats.generation_cancels++;}
static const pg_extension_hook generation_hooks[]={{PG_EXTENSION_BEFORE_REQUEST,0,0,generation_before,generation_cancel},{PG_EXTENSION_AFTER_RESPONSE,0,0,generation_after,generation_cancel},{PG_EXTENSION_ON_YIELD,0,0,generation_yield,generation_cancel},{PG_EXTENSION_AFTER_TOOLS,0,0,generation_tools,0}};
static const pg_extension generation={"generation",0,0,0,0,0,0,generation_hooks,4};
static const pg_extension reviewer={"reviewer",&echo_tool,1,reviewer_sections,2,0,0,0,0},timing={"timing",0,0,timing_sections,1,timing_wraps,2,timing_hooks,2},broken={"broken",0,0,broken_sections,1,broken_wraps,1,0,0};
static const pg_extension *extensions[]={&reviewer,&timing,&broken,&policy,&generation};
static void report(void *context,const char *extension,const char *target,int error){assert(extension&&target&&(!strcmp(extension,"broken")||!strcmp(extension,"policy")||!strcmp(extension,"generation")));if(error)(*(unsigned *)context)++;}
static pg_extensions registry={PG_EXTENSIONS_ABI,sizeof(pg_extensions),extensions,5,0,&hook_stats,report,0,0,0};
static int start(void *context,pg_host *host,size_t owner){(void)context;registry.defaults=json_tokener_parse("[\"coding\",\"reviewer\",\"timing\"]");if(!registry.defaults)return 1;return pg_host_provide(host,owner,"task.definitions",(void *)&provider)||pg_host_provide(host,owner,"extensions.registry",&registry);}
static int stop(void *context){(void)context;if(active_tasks||pending_before||pending_after||pending_request||pending_response||pending_yield)return -1;json_object_put((json_object *)registry.defaults);registry.defaults=0;return 0;}
static const char *const exports[]={"task.definitions","extensions.registry"};
static const pg_plugin plugin={"product.tasks",0,0,0,exports,2,start,stop};
const pg_native_manifest pangu_plugin_v1={PG_NATIVE_ABI,sizeof(pg_native_manifest),sizeof(pg_plugin),0,&plugin,1};
#else
#include "../../src/runtime/native_plugin.h"
#include "../../src/runtime/model_provider.h"
#include "../../src/runtime/tool_provider.h"
#include <json-c/json.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(pg_model_provider)==96&&offsetof(pg_model_provider,identity)==72,"model provider ABI");
static const pg_model_provider *base;
static pg_model_provider wrapped;
static pg_tools executor;
static pg_tool_provider tool_provider;
static unsigned snapshots;
_Static_assert(sizeof(pg_tool_provider)==96&&offsetof(pg_tool_provider,snapshot)==80,"tool provider ABI");
static int snapshot(void *ctx,uint64_t id,const struct json_object *call,struct json_object **out){(void)ctx;(void)id;(void)call;snapshots++;*out=json_tokener_parse("{\"details\":{\"plugin\":\"product.fixture\"},\"diagnostics\":[{\"severity\":\"info\",\"code\":\"fixture_result\",\"message\":\"Native plugin result metadata\"}]}");return *out?0:5;}
static const pg_tools *base_tools;
static unsigned configured,executed;
static int execute(void *context,pg_slice name,pg_slice arguments,char *out,size_t capacity,size_t *size){(void)context;executed++;return base_tools->execute(base_tools->context,name,arguments,out,capacity,size);}
static int configure(void *context,const struct json_object *configuration,const pg_tool_registry *tools){configured++;return base->configure(context,configuration,tools);}
static int start(void *context,pg_host *scope,size_t owner){
 (void)context;base=pg_host_get(scope,owner,"model.provider.base");pg_tools *tools=pg_host_get(scope,owner,"tools.base");const pg_tool_registry *catalog=pg_host_get(scope,owner,"tool.registry.base");if(!base||!tools||!catalog)return 1;
 base_tools=tools;wrapped=*base;wrapped.configure=configure;wrapped.identity="pangu.fixture.openai.v1";executor=*tools;executor.execute=execute;tool_provider=(pg_tool_provider){PG_TOOL_PROVIDER_ABI,sizeof(pg_tool_provider),executor,catalog,0,snapshot,"pangu.fixture.tools.v1"};
 return pg_host_provide(scope,owner,"model.provider",&wrapped)||pg_host_provide(scope,owner,"tools",&executor)||pg_host_provide(scope,owner,"tool.registry",(void *)catalog)||pg_host_provide(scope,owner,"tools.provider",&tool_provider);
}
static int stop(void *context){(void)context;const char *path=getenv("PANGU_PLUGIN_RECEIPT");if(path){FILE *file=fopen(path,"w");if(!file)return -1;fprintf(file,"released configured=%u executed=%u snapshots=%u\n",configured,executed,snapshots);if(fclose(file))return -1;}base=0;return 0;}
static const char *const imports[]={"model.provider.base","tools.base","tool.registry.base"};
static const char *const exports[]={"model.provider","tools","tool.registry","tools.provider"};
static const pg_plugin plugin={"product.fixture",0,imports,3,exports,4,start,stop};
const pg_native_manifest pangu_plugin_v1={PG_NATIVE_ABI,sizeof(pg_native_manifest),sizeof(pg_plugin),0,&plugin,1};

#endif
