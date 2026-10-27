#define _XOPEN_SOURCE 700
#include "../runtime/mimo.h"
#include "../runtime/host.h"
#include "../runtime/native_plugin.h"
#include "../runtime/model_provider.h"
#include "../runtime/tool_provider.h"
#include "../runtime/read_tool.h"
#include "../runtime/write_tool.h"
#include "../runtime/edit_tool.h"
#include "../runtime/bash_tool.h"
#include "../runtime/profile.h"
#include "../runtime/prompt.h"
#include "../runtime/durable_client.h"
#include "../runtime/record_jsonl.h"
#include <json-c/json.h>
#include "../client/tui/runtime_bridge.h"
#include "../client/tui/tui_app.h"
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
/* Explicit static composition: neither Loop nor TUI chooses a provider. */

static const tui_runtime_ops *inputs_owner;
static tui_inputs_ops foreground_inputs;
const tui_inputs_ops *pg_tui_runtime_inputs(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_inputs:0;}
static pg_host host,agent_scope;
static pg_native_plugin extension;
static pg_model_provider default_model_provider;
static pg_tool_provider default_tool_provider;
static int native_tool_snapshot(void *,uint64_t,const struct json_object *,struct json_object **);
static int native_model_configure(void *,const struct json_object *,const pg_tool_registry *);
static void native_model_failure(void *,pg_model_failure *);
static int native_model_complete(void *,struct json_object *);
static struct {const char *path,*session_directory;pg_model model;pg_tools tools,files;pg_process_ops process;pg_client client;} application;
static struct {
 pg_record_jsonl backend;pg_record_session records;pg_document_cache cache;
 pg_document_holds holds;pg_hold_timer timer;pg_durable_session session;
 pg_task_scheduler scheduler;pg_task_runner_host tasks;
 pg_durable_tool tool;pg_durable_tool_host tool_host;
 pg_durable_generation generation;pg_durable_generation_host generation_host;
 pg_durable_inputs inputs;pg_record_queries queries;pg_record_requests requests;
 pg_durable_client client;pg_client native_client;pg_session history;
 const pg_model_provider *model_provider;
 const pg_tool_registry *declared_tools;
 const pg_tool_provider *tool_provider;
 pg_tools selected_tools;
 struct json_object *configuration;
 pg_timer_job fire,queued;void *fire_context,*queued_context;uint64_t deadline;
 unsigned armed,storage_ready,cache_ready,holds_ready,timer_ready,session_ready,client_ready,model_leased,tools_leased,tools_draining;
} durable;
static char configured_prompt[4096];
static pg_mimo provider;
static pg_tool_registry registry;
static pg_tool_registry file_registry;
static pg_read_tool reader={.directory=-1};
static pg_write_tool writer={.directory=-1};
static pg_edit_tool editor;
static pg_process command_process;
static pg_bash_tool shell_tool;
static char *const command_environment[]={"PATH=/usr/bin:/bin","LANG=C.UTF-8",0};


static int workspace_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;if(pg_read_tool_open(&reader,application.path))return 1;
 return pg_host_provide(scope,owner,"workspace",&reader);
}
static int workspace_stop(void *ctx){(void)ctx;pg_read_tool_close(&reader);return 0;}
static int process_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_process_init(&command_process);application.process=pg_process_service(&command_process);
 return pg_host_provide(scope,owner,"process",&application.process);
}
static int process_stop(void *ctx){(void)ctx;return pg_process_close(&command_process);}
static int files_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_read_tool *root=pg_host_get(scope,owner,"workspace");if(!root)return 1;writer.directory=root->directory;
 pg_tool_definition definitions[]={pg_read_snapshot_definition(root),pg_write_tool_definition(&writer)};
 if(pg_tool_registry_init(&file_registry,definitions,2))return 1;
 application.files=pg_tool_registry_service(&file_registry);return pg_host_provide(scope,owner,"files",&application.files);
}
static int no_resources(void *ctx){(void)ctx;return 0;}
static int tools_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_read_tool *root=pg_host_get(scope,owner,"workspace");pg_tools *files=pg_host_get(scope,owner,"files");pg_process_ops *process=pg_host_get(scope,owner,"process");
 if(!root||!files||!process||pg_bash_tool_init(&shell_tool,process,root->directory,command_environment))return 1;
 editor.files=*files;pg_tool_definition definitions[]={pg_read_tool_definition(root),pg_write_tool_definition(&writer),pg_edit_tool_definition(&editor),pg_bash_tool_definition(&shell_tool)};
 if(pg_tool_registry_init(&registry,definitions,4))return 1;application.tools=pg_tool_registry_service(&registry);
 default_tool_provider=(pg_tool_provider){PG_TOOL_PROVIDER_ABI,sizeof(pg_tool_provider),application.tools,&registry,0,native_tool_snapshot,"pangu.coding-tools.v1"};
 return pg_host_provide(scope,owner,"tools",&application.tools)||pg_host_provide(scope,owner,"tool.registry",&registry)||pg_host_provide(scope,owner,"tools.base",&application.tools)||pg_host_provide(scope,owner,"tool.registry.base",&registry)||pg_host_provide(scope,owner,"tools.provider",&default_tool_provider)||pg_host_provide(scope,owner,"tools.provider.base",&default_tool_provider);
}
static int tools_stop(void *ctx){(void)ctx;int result=application.tools.close?application.tools.close(application.tools.context):0;if(result!=1)pg_edit_tool_clear(&editor);return result;}
static int model_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_tool_registry *tools=pg_host_get(scope,owner,"tool.registry");if(!tools)return 1;
 application.model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,tools);
 default_model_provider=(pg_model_provider){PG_MODEL_PROVIDER_ABI,sizeof(pg_model_provider),application.model,native_model_configure,native_model_failure,native_model_complete,"pangu.openai-completions.v1",0,0};
 return pg_host_provide(scope,owner,"model",&application.model)||pg_host_provide(scope,owner,"model.provider",&default_model_provider)||pg_host_provide(scope,owner,"model.provider.base",&default_model_provider);
}
static int model_stop(void *ctx){(void)ctx;if(application.model.close)application.model.close(application.model.context);return 0;}
static uint64_t monotonic(void *ctx){(void)ctx;struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now))abort();return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;}
static double wall_time(void *ctx){(void)ctx;struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now))abort();return (double)now.tv_sec*1000+(double)now.tv_nsec/1000000;}
static int timer_arm(void *ctx,uint64_t delay,pg_timer_job fire,void *argument,void **handle){(void)ctx;if(durable.armed)return 1;durable.fire=fire;durable.fire_context=argument;durable.deadline=monotonic(0)+delay;durable.armed=1;*handle=&durable;return 0;}
static void timer_cancel(void *ctx,void *handle){(void)ctx;(void)handle;durable.armed=0;durable.fire=0;}
static int timer_enqueue(void *ctx,pg_timer_job job,void *argument){(void)ctx;if(durable.queued)return 1;durable.queued=job;durable.queued_context=argument;return 0;}
static void foreground(void){
 if(durable.armed&&durable.fire&&monotonic(0)>=durable.deadline){pg_timer_job fire=durable.fire;durable.fire=0;fire(durable.fire_context);}
 if(durable.queued&&(!durable.session_ready||pg_durable_session_idle(&durable.session))){pg_timer_job job=durable.queued;void *argument=durable.queued_context;durable.queued=0;job(argument);}
 if(durable.tools_draining){int result=durable.selected_tools.close(durable.selected_tools.context);if(result!=1){durable.tools_draining=0;durable.tools_leased=0;if(result)pg_durable_client_fault(&durable.client);}}
}
static int backend_close(void *ctx){(void)ctx;int result=pg_record_jsonl_close(&durable.backend);if(!result)durable.storage_ready=0;return result?-1:0;}
static int record_read(void *ctx,unsigned table,uint64_t id,struct json_object **out){(void)ctx;return pg_record_memory_read(&durable.backend.memory,(pg_record_table)table,id,out);}
static int record_scan(void *ctx,const pg_memory_query *query,pg_memory_page *page){(void)ctx;return pg_record_memory_scan(&durable.backend.memory,query,page);}
static int document_scan(void *ctx,const pg_document_query *query,pg_memory_page *page){(void)ctx;return pg_record_memory_scan_documents(&durable.backend.memory,query,page);}
static int mint(void *ctx,uint64_t *id){(void)ctx;return pg_record_memory_mint(&durable.backend.memory,id);}
static int resolve(void *ctx,pg_slice name,const pg_task_definition **out){
 (void)ctx;const pg_task_definition *definitions[]={&durable.generation.definition,&durable.tool.definition};*out=0;
 for(size_t i=0;i<2;i++)if(name.size==definitions[i]->name.size&&!memcmp(name.data,definitions[i]->name.data,name.size)){*out=definitions[i];return 0;}return 1;
}
static void report(void *ctx,uint64_t id,int error){(void)ctx;fprintf(stderr,"PanGu durable task %llu failed (%d; model HTTP=%ld, transport=%d, decoder=%d).\n",(unsigned long long)id,error,provider.http,provider.transport_done,provider.failed);pg_durable_client_fault(&durable.client);}
static int storage_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;if(pg_record_jsonl_open(&durable.backend,application.session_directory,1))return 1;durable.storage_ready=1;
 durable.records=(pg_record_session){.storage=pg_record_jsonl_storage(&durable.backend),.sequence=durable.backend.memory.next_sequence-1};
 struct json_object *root=0;int status=record_read(0,PG_TABLE_CONVERSATION,1,&root);json_object_put(root);
 if(status){if(status!=PG_MEMORY_MISSING)return 1;struct json_object *seed=json_tokener_parse("[{\"type\":\"conversation\",\"value\":{\"id\":1}}]");if(!seed)return 1;int result=pg_records_commit(&durable.records,seed);json_object_put(seed);if(result)return 1;}
 pg_document_storage storage;pg_record_memory_documents(&durable.backend.memory,&storage);
 pg_document_cache_hooks hooks={.clock=monotonic};if(pg_document_cache_open(&durable.cache,&storage,&hooks,0))return 1;durable.cache_ready=1;
 if(pg_document_holds_open(&durable.holds,&durable.cache,0,monotonic,0))return 1;durable.holds_ready=1;
 pg_hold_timer_host timer={0,timer_arm,timer_cancel,timer_enqueue};if(pg_hold_timer_open(&durable.timer,&durable.holds,&durable.records,&timer))return 1;durable.timer_ready=1;
 pg_durable_close close={0,backend_close};if(pg_durable_session_open(&durable.session,&durable.records,&durable.cache,&durable.holds,&durable.timer,&close))return 1;durable.session_ready=1;
 return pg_host_provide(scope,owner,"session",&durable.session);
}
static int storage_stop(void *ctx){
 (void)ctx;foreground();if(durable.session_ready){int result=pg_durable_session_close(&durable.session);if(result==1)return 1;durable.session_ready=0;return result;}
 if(durable.timer_ready)pg_hold_timer_stop(&durable.timer);if(durable.holds_ready)pg_document_holds_finish(&durable.holds,&durable.records);if(durable.cache_ready)pg_document_cache_close(&durable.cache);if(durable.storage_ready)return backend_close(0);return 0;
}
static int prepare(void *ctx,uint64_t conversation,struct json_object **out){(void)ctx;(void)conversation;*out=0;return json_object_deep_copy(durable.configuration,out,0);}
static int native_model_configure(void *ctx,const struct json_object *configuration,const pg_tool_registry *tools){pg_mimo_bind_tools(ctx,tools);return pg_mimo_configure(ctx,configuration);}
static void native_model_failure(void *ctx,pg_model_failure *out){
 pg_mimo *source=ctx;static char message[128];int n=snprintf(message,sizeof(message),"Model request failed (HTTP %ld, transport %u)",source->http,source->transport_error);
 *out=(pg_model_failure){.message={message,(size_t)n},.retryable=(unsigned)pg_model_retryable((pg_slice){source->body,source->used},source->http,source->transport_error),.http_status=(unsigned)source->http,.transport_error=source->transport_error};
}
static int native_model_complete(void *ctx,struct json_object *assistant){return pg_mimo_complete(ctx,assistant);}
static int model_acquire(void *ctx,const struct json_object *configuration,pg_model *out){
 (void)ctx;if(durable.model_leased)return 1;
 const pg_model_provider *selected=durable.model_provider;
 const char *workspace=json_object_get_string(json_object_object_get(configuration,"workspace"));if(workspace&&strcmp(workspace,application.path))return 2;
 const char *identity=json_object_get_string(json_object_object_get(configuration,"provider"));if(identity&&strcmp(identity,selected->identity))return 2;
 if(selected->configure(selected->model.context,configuration,durable.declared_tools)){if(selected->model.close)selected->model.close(selected->model.context);return 2;}
 *out=selected->model;durable.model_leased=1;return 0;
}
static void model_release(void *ctx,const pg_model *model){(void)ctx;if(model->close)model->close(model->context);durable.model_leased=0;}
static void model_failure(void *ctx,const pg_model *model,pg_model_failure *out){
 (void)ctx;if(durable.model_provider->failure)durable.model_provider->failure(model->context,out);
 else *out=(pg_model_failure){.message={"Model request failed",20}};
}
static int model_complete(void *ctx,const pg_model *model,struct json_object *assistant){(void)ctx;return durable.model_provider->complete?durable.model_provider->complete(model->context,assistant):0;}
static int model_fetch_deferred(void *ctx,const pg_model *model,const struct json_object *handle){(void)ctx;return durable.model_provider->fetch_deferred?durable.model_provider->fetch_deferred(model->context,handle):-1;}
static int model_cancel_deferred(void *ctx,const pg_model *model,const struct json_object *handle){(void)ctx;return durable.model_provider->cancel_deferred?durable.model_provider->cancel_deferred(model->context,handle):0;}
static void model_progress(void *ctx,uint64_t id,pg_slice text){(void)ctx;(void)id;pg_event event={.kind=PG_TEXT_DELTA,.text=text};pg_durable_client_emit(&durable.client,&event);}
static int tool_acquire(void *ctx,uint64_t id,pg_tools *out){(void)ctx;if(durable.tools_leased)return 1;durable.tools_leased=1;pg_edit_tool_clear(&editor);*out=durable.selected_tools;pg_durable_client_tool(&durable.client,id,0);return 0;}
static void tool_release(void *ctx,uint64_t id,const pg_tools *tools){(void)ctx;(void)id;int result=tools->close?tools->close(tools->context):0;if(result==1)durable.tools_draining=1;else {durable.tools_leased=0;if(result)pg_durable_client_fault(&durable.client);}}
static int native_tool_snapshot(void *ctx,uint64_t id,const struct json_object *call,struct json_object **out){
 (void)ctx;(void)id;*out=0;
 const char *name=json_object_get_string(json_object_object_get(call,"name"));
 if(name&&!strcmp(name,"bash")){int result=pg_bash_tool_snapshot(&shell_tool,out);return result==1?0:result;}
 if(!name||strcmp(name,"edit")||!editor.details)return 0;
 struct json_object *value=json_object_new_object();if(!value)return 5;
 struct json_object *details=json_object_get(editor.details);
 if(json_object_object_add(value,"details",details)){json_object_put(details);json_object_put(value);return 5;}
 *out=value;return 0;
}
static int tool_snapshot(void *ctx,uint64_t id,const struct json_object *call,struct json_object **out){
 (void)ctx;const pg_tool_provider *selected=durable.tool_provider;*out=0;
 return selected->snapshot?selected->snapshot(selected->context,id,call,out):0;
}
static void tool_progress(void *ctx,uint64_t id,pg_slice text){(void)ctx;(void)id;pg_durable_client_output(&durable.client,id,text);}
static int client_submit(void *ctx,pg_slice text){(void)ctx;return durable.native_client.submit(durable.native_client.context,text);}
static void client_cancel(void *ctx){(void)ctx;durable.native_client.cancel(durable.native_client.context);}
static int client_poll(void *ctx,pg_event *event){(void)ctx;foreground();return durable.native_client.poll(durable.native_client.context,event);}
static int prompt_probe(void *context,const char *path){
 pg_read_tool *root=context;struct stat metadata;
 if(fstatat(root->directory,path,&metadata,AT_SYMLINK_NOFOLLOW))return errno==ENOENT||errno==ENOTDIR?0:-1;
 return S_ISREG(metadata.st_mode)?1:0;
}
static int agent_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;const pg_model_provider *selected=pg_host_get(scope,owner,"model.provider");const pg_tool_provider *toolset=pg_host_get(scope,owner,"tools.provider");
 pg_tools *executor=toolset?(pg_tools *)&toolset->tools:0;
 if(!toolset||toolset->abi!=PG_TOOL_PROVIDER_ABI||toolset->size!=sizeof(*toolset)||!toolset->catalog||!toolset->identity||!*toolset->identity||strlen(toolset->identity)>63)return 1;
 if(!selected||!executor||!executor->execute||!executor->authorize||pg_host_get(scope,owner,"session")!=&durable.session)return 1;
 if(selected->abi!=PG_MODEL_PROVIDER_ABI||selected->size!=sizeof(*selected)||!selected->identity||!*selected->identity||strlen(selected->identity)>63||!selected->configure||!selected->model.start||!selected->model.next||!selected->model.close)return 1;
 durable.model_provider=selected;durable.tool_provider=toolset;durable.selected_tools=*executor;
 pg_tools *files=pg_host_get(scope,owner,"files");if(!files)return 1;
 pg_read_tool *root=pg_host_get(scope,owner,"workspace");if(!root)return 1;
 const pg_tool_registry *declared=toolset->catalog;if(!declared)return 1;durable.declared_tools=declared;
 pg_prompt_workspace_source source={files,root,prompt_probe,declared,{application.path,strlen(application.path)},0,0,0,{0,0},0,0,0,{0,0},{0,0},0};
 size_t prompt_size=0;const char *prompt=configured_prompt,*file=getenv("PANGU_SYSTEM_PROMPT_FILE");
 if(file){if(pg_prompt_from_file(files,(pg_slice){file,strlen(file)},configured_prompt,sizeof(configured_prompt),&prompt_size))return 1;}
 else {
  char user_path[4096],user_canonical[4096];const char *agent_dir=getenv("PANGU_AGENT_DIR"),*home=getenv("HOME");
  if(!agent_dir&&home){int n=snprintf(user_path,sizeof(user_path),"%s/.pangu/agent",home);if(n<0||(size_t)n>=sizeof(user_path))return 1;agent_dir=user_path;}
  int have_user=0;if(agent_dir){struct stat metadata;if(!stat(agent_dir,&metadata)){if(!S_ISDIR(metadata.st_mode)||!realpath(agent_dir,user_canonical))return 1;have_user=1;}else if(errno!=ENOENT&&errno!=ENOTDIR)return 1;}
  pg_read_tool resources={.directory=-1};if(pg_read_tool_open(&resources,"/"))return 1;pg_tool_definition complete=pg_read_snapshot_definition(&resources);
  pg_prompt_file_source project_source={&complete,application.path},user_source={&complete,user_canonical},ancestor_source={&complete,"/"};
  pg_tools project_files=pg_prompt_resource_files(&project_source),user_files=pg_prompt_resource_files(&user_source),ancestor_files=pg_prompt_resource_files(&ancestor_source);
  source.files=&project_files;source.context=&project_source;source.probe=pg_prompt_resource_probe;
  if(have_user){source.user_files=&user_files;source.user_context=&user_source;source.user_probe=pg_prompt_resource_probe;source.user_directory=(pg_slice){user_canonical,strlen(user_canonical)};}
  source.ancestor_files=&ancestor_files;source.ancestor_context=&ancestor_source;source.ancestor_probe=pg_prompt_resource_probe;source.canonical=pg_prompt_path_canonical;
  char git_worktree[4096],git_common[4096];if(!pg_prompt_git_paths(application.path,git_worktree,sizeof(git_worktree),git_common,sizeof(git_common))){source.git_worktree=(pg_slice){git_worktree,strlen(git_worktree)};source.git_common_directory=(pg_slice){git_common,strlen(git_common)};}
  int result=pg_prompt_workspace(&source,configured_prompt,sizeof(configured_prompt),&prompt_size);pg_read_tool_close(&resources);if(result)return 1;
 }
 const char *model=getenv("PANGU_MODEL_NAME"),*endpoint=getenv("PANGU_MODEL_ENDPOINT");if(!model)model=getenv("MIMO_MODEL");if(!model)model="mimo-v2.6-pro";if(!endpoint)endpoint="https://token-plan-cn.xiaomimimo.com/v1/chat/completions";
 durable.configuration=json_object_new_object();if(!durable.configuration)return 1;
 if(json_object_object_add(durable.configuration,"systemPrompt",json_object_new_string_len(prompt,(int)prompt_size))||json_object_object_add(durable.configuration,"model",json_object_new_string(model))||json_object_object_add(durable.configuration,"endpoint",json_object_new_string(endpoint))||json_object_object_add(durable.configuration,"workspace",json_object_new_string(application.path)))return 1;
 const char *execution=getenv("PANGU_TOOL_EXECUTION");if(execution){if(strcmp(execution,"parallel")&&strcmp(execution,"sequential"))return 1;if(json_object_object_add(durable.configuration,"toolExecution",json_object_new_string(execution)))return 1;}
 const char *options=getenv("PANGU_MODEL_OPTIONS");if(options){
  size_t size=strlen(options);if(size>=4096)return 1;struct json_tokener *parser=json_tokener_new();if(!parser)return 1;
  json_tokener_set_flags(parser,JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8);struct json_object *value=json_tokener_parse_ex(parser,options,(int)size+1);
  int invalid=json_tokener_get_error(parser)!=json_tokener_success||!json_object_is_type(value,json_type_object);json_tokener_free(parser);
  if(invalid){json_object_put(value);return 1;}if(json_object_object_add(durable.configuration,"options",value)){json_object_put(value);return 1;}
 }
 if(json_object_object_add(durable.configuration,"provider",json_object_new_string(selected->identity)))return 1;
 durable.tasks=(pg_task_runner_host){0,record_read,record_scan,document_scan,resolve,wall_time,report};
 durable.tool_host=(pg_durable_tool_host){0,tool_acquire,tool_release,mint,tool_progress,tool_snapshot};if(pg_durable_tool_init(&durable.tool,&durable.tool_host))return 1;
 durable.generation_host=(pg_durable_generation_host){0,prepare,model_acquire,model_release,mint,model_progress,&durable.tool.definition,model_failure,&durable.inputs,model_complete,model_fetch_deferred,model_cancel_deferred};if(pg_durable_generation_init(&durable.generation,&durable.generation_host))return 1;
 pg_record_memory_queries(&durable.backend.memory,&durable.queries);pg_record_memory_requests(&durable.backend.memory,&durable.requests);if(pg_durable_inputs_open(&durable.inputs,&durable.session,&durable.tasks,&durable.generation,&durable.queries,&durable.requests))return 1;
 if(pg_task_scheduler_open(&durable.scheduler,&durable.session,&durable.tasks,5))return 1;
 pg_durable_client_config config={&durable.session,&durable.scheduler,&durable.inputs,1};if(pg_durable_client_open(&durable.client,&config))return 1;durable.client_ready=1;
 durable.records.context=&durable.client;durable.records.publish=pg_durable_client_publish;
 if(pg_durable_generation_history(&durable.tasks,1,durable.configuration,&durable.history))return 1;
 durable.native_client=pg_durable_client_service(&durable.client);application.client=(pg_client){0,client_submit,client_cancel,client_poll};
 return pg_host_provide(scope,owner,"client",&application.client)||pg_host_provide(scope,owner,"inputs",&durable.inputs);
}
static int agent_suspend(void *ctx){(void)ctx;int result=pg_durable_client_suspend(&durable.client);return result==PG_TASK_PENDING?2:result;}
static int agent_stop(void *ctx){
 (void)ctx;foreground();if(durable.client_ready){int result=pg_durable_client_suspend(&durable.client);if(result)return result==PG_TASK_PENDING?1:-1;durable.client_ready=0;}else if(durable.scheduler.active){if(pg_task_scheduler_close(&durable.scheduler))return 1;}
 if(durable.tools_draining)return 1;durable.records.publish=0;json_object_put(durable.configuration);durable.configuration=0;return 0;
}
static const char *const workspace_exports[]={"workspace"},*const process_exports[]={"process"},*const files_exports[]={"files"},*const tools_exports[]={"tools","tool.registry","tools.base","tool.registry.base","tools.provider","tools.provider.base"},*const model_exports[]={"model","model.provider","model.provider.base"},*const agent_exports[]={"client","inputs"},*const storage_exports[]={"session"};
static const char *const file_imports[]={"workspace"},*const tool_imports[]={"workspace","files","process"},*const model_imports[]={"tool.registry"},*const agent_imports[]={"model.provider","tools.provider","session","files","workspace"};
static const pg_plugin agent_plugin={"agent",0,agent_imports,5,agent_exports,2,agent_start,agent_stop};
static const pg_plugin plugins[]={
 {"model",0,model_imports,1,model_exports,3,model_start,model_stop},
 {"tools",0,tool_imports,3,tools_exports,6,tools_start,tools_stop},
 {"files",0,file_imports,1,files_exports,1,files_start,no_resources},
 {"process",0,0,0,process_exports,1,process_start,process_stop},
 {"workspace",0,0,0,workspace_exports,1,workspace_start,workspace_stop},
 {"storage",0,0,0,storage_exports,1,storage_start,storage_stop},
};
int main(int argc,char **argv){
 if(argc>2||(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h")))){fputs("Usage: pangu [workspace-directory]\nSet PANGU_MODEL_ENDPOINT, PANGU_MODEL_NAME, PANGU_MODEL_API_KEY (or MIMO_API_KEY). PANGU_SESSION_DIR selects a private durable directory; default: workspace/.pangu. PANGU_SYSTEM_PROMPT_FILE selects a workspace-relative prompt. PANGU_PLUGIN loads an absolute native library into a child service scope. Ctrl-C cancels while busy and exits while idle; Ctrl-D exits and preserves pending work. Tools: read, write, edit, bash. Commands are not sandboxed.\n",stderr);return argc>2?2:0;}
 const char *endpoint=getenv("PANGU_MODEL_ENDPOINT"),*key=getenv("PANGU_MODEL_API_KEY"),*model=getenv("PANGU_MODEL_NAME");
 if(endpoint&&(!*endpoint||!model||!*model)){fputs("Custom model connection requires endpoint and model.\n",stderr);return 2;}if(!key&&!endpoint)key=getenv("MIMO_API_KEY");if(!key||!*key){fputs("Model API key is required in the environment.\n",stderr);return 2;}
 if(!isatty(0)||!isatty(1)){fputs("PanGu requires an interactive terminal.\n",stderr);return 2;}if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 int rc=2;char *workspace=realpath(argc==2?argv[1]:".",0),*directory=0;if(!workspace)goto cleanup;application.path=workspace;
 const char *selected=getenv("PANGU_SESSION_DIR");if(selected){directory=strdup(selected);}else{size_t n=strlen(workspace)+8;directory=malloc(n);if(directory)snprintf(directory,n,"%s/.pangu",workspace);}if(!directory)goto cleanup;application.session_directory=directory;
 if(pg_host_init(&host,plugins,sizeof(plugins)/sizeof(*plugins))||pg_host_start(&host)){fprintf(stderr,"Cannot activate native plugin %s (error=%u).\n",host.error_owner<host.count?plugins[host.error_owner].name:"composition",host.error);goto cleanup;}
 pg_host *parent=&host;const char *plugin=getenv("PANGU_PLUGIN");if(plugin){if(pg_native_open(&extension,plugin,&host)){fprintf(stderr,"Cannot activate native extension (error=%u).\n",extension.error);goto cleanup;}parent=&extension.host;}
 if(pg_host_init_child(&agent_scope,parent,&agent_plugin,1)||pg_host_start(&agent_scope)){fprintf(stderr,"Cannot activate Agent consumer (error=%u).\n",agent_scope.error);goto cleanup;}
 tui_tool_observer observer={&durable.client,pg_durable_client_tool_observe,pg_durable_tool_observation_release};
 pg_client *client=pg_host_find(&agent_scope,"client");if(!client)goto cleanup;pg_tui_bridge bridge;if(pg_tui_bridge_init(&bridge,*client)||pg_tui_bridge_set_history(&bridge,&durable.history)||pg_tui_bridge_set_detach(&bridge,agent_suspend,0)||pg_tui_bridge_set_tool_observer(&bridge,&observer))goto cleanup;
 if(pg_durable_client_inputs_service(&durable.client,&foreground_inputs))goto cleanup;inputs_owner=&bridge.ops;
 tui_app_config config={STDIN_FILENO,STDOUT_FILENO,1,&bridge.ops,20};int reason=tui_app_run(&config);inputs_owner=0;bridge.ops.shutdown(bridge.ops.ctx);rc=reason<0||reason==TUI_EXIT_POLL_ERROR||bridge.stop_error?1:0;if(rc)fprintf(stderr,"PanGu terminal stopped (reason=%d, bridge=%u).\n",reason,bridge.stop_error);
cleanup:
 for(;;){int closing=pg_host_close(&agent_scope);if(!closing)break;if(closing<0){rc=1;goto retain_resources;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 for(;;){int closing=pg_native_close(&extension);if(!closing)break;if(closing<0){rc=1;goto retain_resources;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 for(;;){int closing=pg_host_close(&host);if(!closing)break;if(closing<0){rc=1;goto retain_resources;}struct timespec pause={0,10000000};nanosleep(&pause,0);}free(directory);free(workspace);curl_global_cleanup();
retain_resources:
 return rc;
}
