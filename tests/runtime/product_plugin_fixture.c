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
