#ifdef PG_TASK_FIXTURE
#include "../../src/runtime/task_provider.h"
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
static int start(void *context,pg_host *host,size_t owner){(void)context;return pg_host_provide(host,owner,"task.definitions",(void *)&provider);}
static int stop(void *context){(void)context;return active_tasks?-1:0;}
static const char *const exports[]={"task.definitions"};
static const pg_plugin plugin={"product.tasks",0,0,0,exports,1,start,stop};
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
