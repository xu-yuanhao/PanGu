#define _XOPEN_SOURCE 700
#include "../runtime/mimo.h"
#include "../runtime/host.h"
#include "../runtime/native_plugin.h"
#include "../runtime/model_provider.h"
#include "../runtime/models.h"
#include "../runtime/usage.h"
#include "../runtime/task_graph.h"
#include "../runtime/task_provider.h"
#include "../runtime/agent_state.h"
#include "../runtime/extensions.h"
#include "../runtime/system_prompt.h"
#include "../runtime/extension_registry.h"
#include "../runtime/conversation_view.h"
#include "../runtime/request_context.h"
#include "../runtime/tool_provider.h"
#include "../runtime/tool_execution.h"
#include "../runtime/read_tool.h"
#include "../runtime/execution_env.h"
#include "../runtime/image_processor.h"
#include "../runtime/write_tool.h"
#include "../runtime/edit_tool.h"
#include "../runtime/bash_tool.h"
#include "../runtime/profile.h"
#include "../runtime/prompt.h"
#include "../runtime/skills.h"
#include "../runtime/prompt_templates.h"
#include "../runtime/settings.h"
#include "../runtime/resources.h"
#include "../runtime/durable_client.h"
#include "../runtime/durable_compaction.h"
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
static tui_resource_ops foreground_resources;
static tui_compaction_ops foreground_compaction;
static pg_usage_ops foreground_usage;
static pg_usage_total_ops foreground_usage_total;
static pg_usage_total_binding usage_total_binding;
static pg_observation_ops foreground_observation;
static pg_task_graph_ops foreground_task_graph;
static pg_task_control_ops foreground_task_control;
static const pg_task_provider *extension_tasks;
static pg_agent_state_ops foreground_agent_state;
static pg_view_binding task_graph_binding;
static pg_conversation_view_binding conversation_view_binding;
static pg_conversation_view_ops foreground_conversation_view;
static pg_conversation_client_ops foreground_conversations;
static pg_conversation_catalog_ops foreground_conversation_catalog;
static pg_conversation_actions_ops foreground_conversation_actions;
static pg_conversation_history_ops foreground_conversation_history;
static pg_model_catalog_ops foreground_model_catalog;
static uint64_t startup_conversation=1;
static pg_agent_conversation_binding conversation_creation;
static pg_fork_storage conversation_forks;
const tui_inputs_ops *pg_tui_runtime_inputs(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_inputs:0;}
const tui_resource_ops *pg_tui_runtime_resources(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_resources:0;}
const tui_compaction_ops *pg_tui_runtime_compaction(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_compaction:0;}
const pg_observation_ops *pg_tui_runtime_observation(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_observation:0;}
const pg_usage_total_ops *pg_tui_runtime_usage_total(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_usage_total:0;}
const pg_task_graph_ops *pg_tui_runtime_task_graph(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_task_graph:0;}
const pg_task_control_ops *pg_tui_runtime_task_control(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_task_control:0;}
const pg_conversation_view_ops *pg_tui_runtime_conversation_view(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_conversation_view:0;}
const pg_conversation_client_ops *pg_tui_runtime_conversations(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_conversations:0;}
const pg_conversation_catalog_ops *pg_tui_runtime_conversation_catalog(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_conversation_catalog:0;}
const pg_conversation_actions_ops *pg_tui_runtime_conversation_actions(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_conversation_actions:0;}
const pg_conversation_history_ops *pg_tui_runtime_conversation_history(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_conversation_history:0;}
const pg_model_catalog_ops *pg_tui_runtime_models(const tui_runtime_ops *ops){return ops&&ops==inputs_owner&&!((pg_tui_bridge *)ops->ctx)->stopped?&foreground_model_catalog:0;}
static pg_host host,agent_scope;
static pg_native_plugin extension;
static pg_model_provider default_model_provider;
static pg_tool_provider default_tool_provider;
static int native_tool_snapshot(void *,uint64_t,const struct json_object *,struct json_object **);
static int native_model_configure(void *,const struct json_object *,const pg_tool_registry *);
static void native_model_failure(void *,pg_model_failure *);
static int native_model_complete(void *,struct json_object *);
static struct {const char *path,*session_directory;pg_model model;pg_tools tools,files;pg_process_ops process;pg_execution_env execution_env;pg_client client;} application;
typedef struct tool_scope {
 struct tool_scope *next;
 uint64_t task_id;
 pg_extension_result extensions;
 pg_tool_registry allowed;
 pg_extension_tools dispatch;
 pg_tools executor;
 pg_agent_tools_gate gate;
 pg_tool_definition *definition;
 pg_tool_execution execution;
 const pg_tool_async *factory;pg_tool_async instance_async;void *instance;
 unsigned base,shared_base,draining;
} tool_scope;
static struct {
 pg_record_jsonl backend;pg_record_session records;pg_document_cache cache;
 pg_document_holds holds;pg_hold_timer timer;pg_durable_session session;
 pg_task_scheduler scheduler;pg_task_runner_host tasks;
 pg_durable_tool tool;pg_durable_tool_host tool_host;
 pg_durable_generation generation;pg_durable_generation_host generation_host,compaction_host;
 pg_durable_compaction compaction;pg_compaction_binding compaction_binding;pg_compaction_ops compaction_ops;
 pg_durable_inputs inputs;pg_record_queries queries;pg_record_requests requests;
 pg_durable_client client;pg_client native_client;struct json_object *history;
 const pg_model_provider *model_provider;
 const pg_model_metadata *model_metadata;
 const pg_tool_registry *declared_tools;
 const pg_tool_provider *tool_provider;
 pg_tools selected_tools;
 tool_scope *tool_scopes;
 pg_extension_registry extension_registry;pg_extension_control extension_control;pg_task_resolver task_resolver;pg_document_history_reader task_history;pg_prompt_preparer prompt_preparer;pg_durable_prepare async_prepare;
 const pg_extensions *extensions;pg_extension_result model_extensions;
 pg_tool_registry offered_tools;
 struct json_object *configuration;
 pg_resources resources;pg_resource_host resource_host;
 pg_timer_job fire,queued;void *fire_context,*queued_context;uint64_t deadline;
 unsigned armed,storage_ready,cache_ready,holds_ready,timer_ready,session_ready,client_ready,model_leased;
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
static int execution_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_read_tool *workspace=pg_host_get(scope,owner,"workspace");pg_tools *files=pg_host_get(scope,owner,"files");pg_process_ops *process=pg_host_get(scope,owner,"process");
 if(!workspace||!files||!process)return 1;
 application.execution_env=(pg_execution_env){.abi=PG_EXECUTION_ENV_ABI,.size=sizeof(pg_execution_env),.workspace=workspace,.files=*files,.process=*process,.directory=workspace->directory,.environment=command_environment};
 return pg_host_provide(scope,owner,"execution.env",&application.execution_env);
}
static int files_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_read_tool *root=pg_host_get(scope,owner,"workspace");if(!root)return 1;writer.directory=root->directory;
 pg_tool_definition definitions[]={pg_read_snapshot_definition(root),pg_write_file_definition(&writer)};
 if(pg_tool_registry_init(&file_registry,definitions,2))return 1;
 application.files=pg_tool_registry_service(&file_registry);return pg_host_provide(scope,owner,"files",&application.files);
}
static int no_resources(void *ctx){(void)ctx;return 0;}
static int tools_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_execution_env *execution=pg_host_get(scope,owner,"execution.env");
 if(execution&&execution->workspace&&pg_read_tool_configure_images(execution->workspace,0,pg_host_get(scope,owner,"image.processor")))return 1;
 if(!execution||!execution->workspace||pg_bash_tool_init(&shell_tool,execution))return 1;
 editor.files=execution->files;pg_tool_definition definitions[]={pg_read_tool_definition(execution->workspace),pg_write_tool_definition(&writer),pg_edit_tool_definition(&editor),pg_bash_tool_definition(&shell_tool)};
 if(pg_tool_registry_init(&registry,definitions,4))return 1;application.tools=pg_tool_registry_service(&registry);
 default_tool_provider=(pg_tool_provider){PG_TOOL_PROVIDER_ABI,sizeof(pg_tool_provider),application.tools,&registry,0,native_tool_snapshot,"pangu.coding-tools.v1"};
 return pg_host_provide(scope,owner,"tools",&application.tools)||pg_host_provide(scope,owner,"tool.registry",&registry)||pg_host_provide(scope,owner,"tools.base",&application.tools)||pg_host_provide(scope,owner,"tool.registry.base",&registry)||pg_host_provide(scope,owner,"tools.provider",&default_tool_provider)||pg_host_provide(scope,owner,"tools.provider.base",&default_tool_provider);
}
static int tools_stop(void *ctx){(void)ctx;int result=application.tools.close?application.tools.close(application.tools.context):0;if(result!=1)pg_edit_tool_clear(&editor);return result;}
static void catalog_release(json_object *value){json_object_put(value);}
static int catalog_list(void *ctx,const char *provider_name,json_object **out){(void)ctx;*out=0;if(durable.resources.closed||!durable.resources.current.settings)return 7;return pg_models_list(json_object_object_get(durable.resources.current.settings,"modelCatalog"),provider_name,out);}
static int native_model_describe(void *ctx,const struct json_object *configuration,struct json_object **out){(void)ctx;return pg_models_describe(json_object_object_get(durable.resources.current.settings,"modelCatalog"),configuration,out);}
static pg_model_metadata default_model_metadata={PG_MODEL_METADATA_ABI,sizeof(pg_model_metadata),0,native_model_describe};
static int model_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_tool_registry *tools=pg_host_get(scope,owner,"tool.registry");if(!tools)return 1;
 application.model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,tools);
 default_model_provider=(pg_model_provider){PG_MODEL_PROVIDER_ABI,sizeof(pg_model_provider),application.model,native_model_configure,native_model_failure,native_model_complete,"pangu.openai-completions.v1",0,0};
 return pg_host_provide(scope,owner,"model",&application.model)||pg_host_provide(scope,owner,"model.provider",&default_model_provider)||pg_host_provide(scope,owner,"model.provider.base",&default_model_provider)||pg_host_provide(scope,owner,"model.metadata",&default_model_metadata);
}
static int model_stop(void *ctx){(void)ctx;if(application.model.close)application.model.close(application.model.context);return 0;}
static uint64_t monotonic(void *ctx){(void)ctx;struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now))abort();return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;}
static double wall_time(void *ctx){(void)ctx;struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now))abort();return (double)now.tv_sec*1000+(double)now.tv_nsec/1000000;}
static int timer_arm(void *ctx,uint64_t delay,pg_timer_job fire,void *argument,void **handle){(void)ctx;if(durable.armed)return 1;durable.fire=fire;durable.fire_context=argument;durable.deadline=monotonic(0)+delay;durable.armed=1;*handle=&durable;return 0;}
static void timer_cancel(void *ctx,void *handle){(void)ctx;(void)handle;durable.armed=0;durable.fire=0;}
static int timer_enqueue(void *ctx,pg_timer_job job,void *argument){(void)ctx;if(durable.queued)return 1;durable.queued=job;durable.queued_context=argument;return 0;}
static tool_scope *tool_scope_find(uint64_t id){for(tool_scope *scope=durable.tool_scopes;scope;scope=scope->next)if(scope->task_id==id)return scope;return 0;}
static void tool_scope_drop(tool_scope *scope){
 tool_scope **link=&durable.tool_scopes;while(*link&&*link!=scope)link=&(*link)->next;
 if(*link)*link=scope->next;if(scope->instance)scope->factory->dispose(scope->instance);pg_extensions_close(&scope->extensions);free(scope);
}
static void foreground(void){
 if(durable.armed&&durable.fire&&monotonic(0)>=durable.deadline){pg_timer_job fire=durable.fire;durable.fire=0;fire(durable.fire_context);}
 if(durable.queued&&(!durable.session_ready||pg_durable_session_idle(&durable.session))){pg_timer_job job=durable.queued;void *argument=durable.queued_context;durable.queued=0;job(argument);}
 for(tool_scope *scope=durable.tool_scopes,*next;scope;scope=next){next=scope->next;if(!scope->draining)continue;int result=scope->executor.close?scope->executor.close(scope->executor.context):0;if(result!=1){tool_scope_drop(scope);if(result)pg_durable_client_fault(&durable.client);}}
}
static int backend_close(void *ctx){(void)ctx;int result=pg_record_jsonl_close(&durable.backend);if(!result)durable.storage_ready=0;return result?-1:0;}
static int record_read(void *ctx,unsigned table,uint64_t id,struct json_object **out){(void)ctx;return pg_record_memory_read(&durable.backend.memory,(pg_record_table)table,id,out);}
static int record_scan(void *ctx,const pg_memory_query *query,pg_memory_page *page){(void)ctx;return pg_record_memory_scan(&durable.backend.memory,query,page);}
static int task_history_entry(void *ctx,uint64_t conversation,uint64_t id,json_object **out,uint64_t *sequence){return pg_record_memory_entry(ctx,id,conversation,out,sequence);}
static int document_scan(void *ctx,const pg_document_query *query,pg_memory_page *page){(void)ctx;return pg_record_memory_scan_documents(&durable.backend.memory,query,page);}
static int mint(void *ctx,uint64_t *id){(void)ctx;return pg_record_memory_mint(&durable.backend.memory,id);}
static int fork_entry(void *ctx,uint64_t conversation,uint64_t id,json_object **out,uint64_t *sequence){(void)ctx;return pg_record_memory_entry(&durable.backend.memory,id,conversation,out,sequence);}
static int fork_documents(void *ctx,const pg_fork_query *query,pg_fork_page *out){
 (void)ctx;pg_document_query request={.scope=PG_DOC_CONVERSATION,.owner=query->conversation_id,.at=query->at,.limit=256,.after=query->after,.has_cursor=query->has_cursor};pg_memory_page page={0};int status=pg_record_memory_scan_documents(&durable.backend.memory,&request,&page);*out=(pg_fork_page){.items=page.items,.after=(int64_t)page.after,.has_next=page.has_next};return status;
}
static int conversation_create(void *ctx,uint64_t parent,uint64_t at,const json_object *agent,json_object **out){
 pg_agent_conversation_options options={.target={.parent=parent,.entry=at},.agent=agent};return pg_durable_client_create(ctx,&conversation_creation,&options,out);
}
static int conversation_rename(void *ctx,uint64_t conversation,pg_slice name){return pg_durable_client_rename(ctx,&conversation_creation,conversation,name);}
static int resolve(void *ctx,pg_slice name,const pg_task_definition **out){
 (void)ctx;const pg_task_definition *definitions[]={&durable.generation.definition,&durable.tool.definition,&durable.compaction.definition};*out=0;
 for(size_t i=0;i<3;i++)if(name.size==definitions[i]->name.size&&!memcmp(name.data,definitions[i]->name.data,name.size)){*out=definitions[i];return 0;}return pg_task_provider_find(extension_tasks,name,out);
}
static int task_acquire(void *ctx,pg_slice name,const pg_task_definition **out,void **lease){
 (void)ctx;*out=0;*lease=0;int result=pg_extension_registry_task(&durable.extension_registry,name,out,lease);if(result!=1)return result;return resolve(0,name,out);
}
static void report(void *ctx,uint64_t id,int error){(void)ctx;fprintf(stderr,"PanGu durable task %llu failed (%d; model HTTP=%ld, transport=%d, decoder=%d).\n",(unsigned long long)id,error,provider.http,provider.transport_done,provider.failed);pg_durable_client_fault_task(&durable.client,id);}
static int storage_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;if(pg_record_jsonl_open(&durable.backend,application.session_directory,1))return 1;durable.storage_ready=1;
 durable.records=(pg_record_session){.storage=pg_record_jsonl_storage(&durable.backend),.sequence=durable.backend.memory.next_sequence-1};
 pg_document_storage storage;pg_record_memory_documents(&durable.backend.memory,&storage);
 pg_document_cache_hooks hooks={.clock=monotonic};if(pg_document_cache_open(&durable.cache,&storage,&hooks,0))return 1;durable.cache_ready=1;
 if(pg_document_holds_open(&durable.holds,&durable.cache,0,monotonic,0))return 1;durable.holds_ready=1;
 pg_hold_timer_host timer={0,timer_arm,timer_cancel,timer_enqueue};if(pg_hold_timer_open(&durable.timer,&durable.holds,&durable.records,&timer))return 1;durable.timer_ready=1;
 pg_durable_close close={0,backend_close};if(pg_durable_session_open(&durable.session,&durable.records,&durable.cache,&durable.holds,&durable.timer,&close))return 1;durable.session_ready=1;
 conversation_forks=(pg_fork_storage){0,fork_entry,fork_documents,mint};conversation_creation=(pg_agent_conversation_binding){&durable.session,0,record_read,mint,&conversation_forks};pg_agent_conversation_options root_request={.target={.root=1}};struct json_object *root=0;int status=pg_agent_conversation_create(&conversation_creation,&root_request,&root);json_object_put(root);if(status)return 1;
 return pg_host_provide(scope,owner,"session",&durable.session);
}
static int storage_stop(void *ctx){
 (void)ctx;foreground();if(durable.session_ready){int result=pg_durable_session_close(&durable.session);if(result==1)return 1;durable.session_ready=0;return result;}
 if(durable.timer_ready)pg_hold_timer_stop(&durable.timer);if(durable.holds_ready)pg_document_holds_finish(&durable.holds,&durable.records);if(durable.cache_ready)pg_document_cache_close(&durable.cache);if(durable.storage_ready)return backend_close(0);return 0;
}
static int prepare_load(void *ctx,uint64_t conversation,pg_prompt_inputs *inputs){
 (void)ctx;json_object *replayed=0,*identity=0;int result=pg_provider_state_ensure(&conversation_creation,conversation,&identity);if(!result)result=pg_agent_state_read(&durable.session,conversation,&inputs->state);if(!result)result=pg_durable_context_read(&durable.tasks,conversation,UINT64_MAX,&inputs->view);if(!result)result=pg_prompt_replay(inputs->view,&replayed);if(!result)result=json_object_deep_copy(durable.configuration,&inputs->configuration,0);json_object *shown=0;if(!result){shown=json_object_get(json_object_object_get(replayed,"sections"));result=json_object_object_add(inputs->configuration,"shown",shown);if(!result)shown=0;}json_object_put(shown);if(!result){json_object *id=json_object_get(json_object_object_get(identity,"sessionId"));result=json_object_object_add(inputs->configuration,"sessionId",id);if(result)json_object_put(id);}json_object_put(identity);json_object_put(replayed);return result;
}
static int prepare_finish(void *ctx,json_object *configuration){
 (void)ctx;
 struct json_object *settings=json_object_object_get(durable.resources.current.settings,"settings"),*policy=json_object_object_get(settings,"compaction"),*copy=0;
 if(policy){if(json_object_deep_copy(policy,&copy,0)||json_object_object_add(configuration,"compaction",copy))goto failed;copy=0;}
 if(durable.model_metadata){if(durable.model_metadata->describe(durable.model_metadata->context,configuration,&copy)||!copy||!json_object_is_type(copy,json_type_object)||!pg_document_value_valid(copy))goto failed;if(json_object_object_add(configuration,"modelInfo",copy))goto failed;copy=0;}
 if(pg_models_compaction_policy(configuration,&copy)||json_object_object_add(configuration,"compaction",copy))goto failed;copy=0;
 return 0;
failed:json_object_put(copy);return 1;
}
static int prepare(void *ctx,uint64_t conversation,json_object **out){
 (void)ctx;*out=0;pg_prompt_inputs inputs={0};int result=prepare_load(0,conversation,&inputs);if(!result)result=pg_agent_state_prepare(inputs.configuration,inputs.state,out);if(!result)result=prepare_finish(0,*out);json_object_put(inputs.state);json_object_put(inputs.configuration);json_object_put(inputs.view);if(result){json_object_put(*out);*out=0;}return result;
}
static int native_model_configure(void *ctx,const struct json_object *configuration,const pg_tool_registry *tools){pg_mimo_bind_tools(ctx,tools);return pg_mimo_configure(ctx,configuration);}
static void native_model_failure(void *ctx,pg_model_failure *out){
 pg_mimo *source=ctx;static char message[128];int n=snprintf(message,sizeof(message),"Model request failed (HTTP %ld, transport %u)",source->http,source->transport_error);
 pg_slice detail={message,(size_t)n},body={source->body,source->used},structured_detail={0},structured_body={0};if(!pg_mimo_failure_data(source,&structured_detail,&structured_body)){if(structured_detail.size)detail=structured_detail;if(structured_body.size)body=structured_body;}
 *out=(pg_model_failure){.message=detail,.retryable=source->retry_blocked?0:(unsigned)pg_model_retryable(body,source->http,source->transport_error),.http_status=(unsigned)source->http,.transport_error=source->transport_error,.flags=PG_MODEL_FAILURE_CONTEXT_CHECKED|((unsigned)pg_model_context_overflow(body,source->http)*PG_MODEL_FAILURE_CONTEXT_OVERFLOW)};
}
static int native_model_complete(void *ctx,struct json_object *assistant){return pg_mimo_complete(ctx,assistant);}
static int acquire_model(const struct json_object *configuration,const pg_tool_registry *tools,pg_model *out){
 if(durable.model_leased)return 1;
 const pg_model_provider *selected=durable.model_provider;
 const char *workspace=json_object_get_string(json_object_object_get(configuration,"workspace"));if(workspace&&strcmp(workspace,application.path))return 2;
 const char *identity=json_object_get_string(json_object_object_get(configuration,"provider"));if(identity&&strcmp(identity,selected->identity))return 2;
 if(selected->configure(selected->model.context,configuration,tools)){if(selected->model.close)selected->model.close(selected->model.context);return 2;}
 *out=selected->model;durable.model_leased=1;return 0;
}
static int model_acquire(void *ctx,const struct json_object *configuration,pg_model *out){
 (void)ctx;if(durable.model_leased)return 1;int result=pg_extensions_resolve(durable.extensions,configuration,durable.declared_tools,&durable.model_extensions);if(!result)result=pg_agent_tools_select(&durable.model_extensions.tools,configuration,1,&durable.offered_tools);if(!result)result=acquire_model(configuration,&durable.offered_tools,out);if(result)pg_extensions_close(&durable.model_extensions);return result?2:0;
}
static int compaction_acquire(void *ctx,const struct json_object *configuration,pg_model *out){(void)ctx;return acquire_model(configuration,0,out);}
static void model_release(void *ctx,const pg_model *model){(void)ctx;if(model->close)model->close(model->context);durable.model_leased=0;pg_extensions_close(&durable.model_extensions);}
static void model_failure(void *ctx,const pg_model *model,pg_model_failure *out){
 (void)ctx;if(durable.model_provider->failure)durable.model_provider->failure(model->context,out);
 else *out=(pg_model_failure){.message={"Model request failed",20}};
}
static int model_complete(void *ctx,const pg_model *model,struct json_object *assistant){(void)ctx;return durable.model_provider->complete?durable.model_provider->complete(model->context,assistant):0;}
static int model_fetch_deferred(void *ctx,const pg_model *model,const struct json_object *handle){(void)ctx;return durable.model_provider->fetch_deferred?durable.model_provider->fetch_deferred(model->context,handle):-1;}
static int model_cancel_deferred(void *ctx,const pg_model *model,const struct json_object *handle){(void)ctx;return durable.model_provider->cancel_deferred?durable.model_provider->cancel_deferred(model->context,handle):0;}
static void model_progress(void *ctx,uint64_t id,pg_slice text){(void)ctx;pg_durable_client_text(&durable.client,id,text);}
static int extension_tool_hook_poll(void *ctx,uint64_t id,unsigned event,const json_object *request,void **state,json_object **out){
 (void)ctx;*out=0;tool_scope *scope=tool_scope_find(id);if(!scope)return -5;if(!*state){if(pg_extension_hook_open(&scope->extensions,event,request,state))return -5;pg_task_runner *runner=pg_task_scheduler_find(&durable.scheduler,id);if(!runner||pg_extension_hook_bind_access(*state,runner,&durable.task_history))return -5;}int result=pg_extension_hook_poll(*state,out);if(!result){pg_extension_hook_close(*state);*state=0;}return result;
}
static void extension_tool_hook_close(void *ctx,void *state){(void)ctx;pg_extension_hook_close(state);}
static int extension_tool_validate(void *ctx,uint64_t id,const json_object *call){(void)ctx;tool_scope *scope=tool_scope_find(id);return scope?pg_tool_registry_check_json(&scope->allowed,call):-1;}
static const pg_durable_tool_hooks extension_tool_hooks={0,extension_tool_hook_poll,extension_tool_hook_close,extension_tool_validate};
static int extension_generation_hook_poll(void *ctx,uint64_t id,unsigned event,const json_object *request,void **state,json_object **out){
 (void)ctx;*out=0;if(!*state){json_object *task=0,*selection=0;if(record_read(0,PG_TABLE_TASK,id,&task))return -5;uint64_t conversation=json_object_get_uint64(json_object_object_get(task,"conversationId"));json_object_put(task);int result=pg_agent_state_read(&durable.session,conversation,&selection);if(!result)result=pg_extension_hook_resolve_open(durable.extensions,selection,durable.declared_tools,event,request,state);json_object_put(selection);if(!result){pg_task_runner *runner=pg_task_scheduler_find(&durable.scheduler,id);result=!runner?7:pg_extension_hook_bind_access(*state,runner,&durable.task_history);}if(result)return -5;}return pg_extension_hook_poll_event(*state,event,request,out);
}
static const pg_durable_generation_hooks extension_generation_hooks={0,extension_generation_hook_poll,extension_tool_hook_close};
static int tool_acquire(void *ctx,uint64_t id,pg_tools *out){
 (void)ctx;if(tool_scope_find(id))return 1;
 pg_task_runner *runner=pg_task_scheduler_find(&durable.scheduler,id);const json_object *call=pg_durable_tool_call(runner);
 if(!call)return 2;const char *name=json_object_get_string(json_object_object_get(call,"name"));if(!name)return 2;
 json_object *state=0;uint64_t conversation=json_object_get_uint64(json_object_object_get(runner->record,"conversationId"));
 tool_scope *scope=calloc(1,sizeof(*scope));if(!scope)return 2;scope->task_id=id;
 int result=pg_agent_state_read(&durable.session,conversation,&state);
 if(!result)result=pg_extensions_resolve(durable.extensions,state,durable.declared_tools,&scope->extensions);
 if(!result)result=pg_agent_tools_select(&scope->extensions.tools,state,1,&scope->allowed);json_object_put(state);
 if(result){pg_extensions_close(&scope->extensions);free(scope);return 2;}
 scope->dispatch=(pg_extension_tools){durable.selected_tools,pg_tool_registry_service(&scope->extensions.tools),durable.declared_tools,&scope->extensions.tools,0};
 scope->base=pg_extension_tools_uses_base(&scope->dispatch,(pg_slice){name,strlen(name)});scope->shared_base=scope->base;
 for(size_t i=0;i<scope->extensions.tools.count;i++)if(!strcmp(scope->extensions.tools.entries[i].name,name)){scope->definition=&scope->extensions.tools.entries[i];break;}
 if(scope->definition)for(size_t i=0;i<durable.declared_tools->count;i++){const pg_tool_definition *base=&durable.declared_tools->entries[i];if(scope->definition->context==base->context&&scope->definition->execute==base->execute&&scope->definition->async==base->async)scope->shared_base=1;}
 if(scope->definition&&scope->definition->async&&scope->definition->async->open)scope->shared_base=0;
 for(tool_scope *other=durable.tool_scopes;other;other=other->next){
  int shared_async=scope->definition&&other->definition&&scope->definition->async&&!scope->definition->async->open&&scope->definition->context==other->definition->context&&scope->definition->async==other->definition->async;
  if((scope->shared_base&&other->shared_base)||shared_async){pg_extensions_close(&scope->extensions);free(scope);return 1;}
 }
 if(scope->definition&&scope->definition->options&&pg_durable_tool_output_limits(runner,scope->definition->options)){pg_extensions_close(&scope->extensions);free(scope);return 2;}
 if(scope->shared_base&&!strcmp(name,"read")){
  json_object *metadata=0;const pg_read_image_processor *images=pg_host_find(&agent_scope,"image.processor");
  result=durable.model_metadata?durable.model_metadata->describe(durable.model_metadata->context,durable.configuration,&metadata):0;
  if(!result)result=pg_read_tool_configure_images(&reader,metadata,images);json_object_put(metadata);
  if(result){pg_extensions_close(&scope->extensions);free(scope);return 2;}
 }
 if(scope->shared_base)pg_edit_tool_clear(&editor);
 if(scope->definition&&scope->definition->async&&scope->definition->async->open){
  scope->factory=scope->definition->async;
  result=pg_task_access_open(&scope->execution.access,runner,&durable.task_history);
  scope->execution.tool=&durable.tool.definition;scope->execution.resumes=scope->definition->replay_safe;scope->execution.mint=mint;scope->execution.call_id=json_object_get_string(json_object_object_get(call,"id"));
  if(!result)result=scope->factory->open(scope->definition->context,&scope->execution,&scope->instance);
  if(result||!scope->instance){if(scope->instance)scope->factory->dispose(scope->instance);pg_extensions_close(&scope->extensions);free(scope);return 2;}
  scope->instance_async=*scope->factory;scope->instance_async.open=0;scope->instance_async.dispose=0;
  scope->definition->context=scope->instance;scope->definition->async=&scope->instance_async;scope->base=0;
  for(size_t i=0;i<scope->allowed.count;i++)if(!strcmp(scope->allowed.entries[i].name,name)){scope->allowed.entries[i].context=scope->instance;scope->allowed.entries[i].async=&scope->instance_async;}
 }
 scope->executor=pg_extension_tools_service(&scope->dispatch);scope->gate=(pg_agent_tools_gate){scope->executor,&scope->allowed};
 scope->next=durable.tool_scopes;durable.tool_scopes=scope;*out=pg_agent_tools_service(&scope->gate);pg_durable_client_tool(&durable.client,id,0);return 0;
}
static int agent_state_read(void *ctx,uint64_t conversation,json_object **out){(void)ctx;return pg_agent_state_read(&durable.session,conversation,out);}
static int agent_state_configure(void *ctx,uint64_t conversation,const json_object *change){
 (void)ctx;pg_tool_registry checked={0};pg_extension_result candidate={0};int invalid=pg_extensions_resolve(durable.extensions,change,durable.declared_tools,&candidate);if(!invalid)invalid=pg_agent_tools_select(&candidate.tools,change,1,&checked);pg_extensions_close(&candidate);if(invalid)return 5;pg_record_tx tx={0};pg_document_tx docs={0};if(pg_durable_session_begin(&durable.session,&tx,&docs,0,record_read))return 3;
 if(pg_agent_state_stage(&docs,conversation,change,mint)){pg_durable_session_abort(&durable.session);return 5;}return pg_durable_session_settle(&durable.session,0,0,0);
}
static void tool_release(void *ctx,uint64_t id,const pg_tools *tools){
 (void)ctx;tool_scope *scope=tool_scope_find(id);if(!scope){pg_durable_client_fault(&durable.client);return;}
 scope->execution.access.runner=0;int result=tools->close?tools->close(tools->context):0;if(result==1)scope->draining=1;else {tool_scope_drop(scope);if(result)pg_durable_client_fault(&durable.client);}
}
static int native_tool_snapshot(void *ctx,uint64_t id,const struct json_object *call,struct json_object **out){
 (void)ctx;(void)id;*out=0;
 const char *name=json_object_get_string(json_object_object_get(call,"name"));
 if(name&&!strcmp(name,"read"))return pg_read_tool_snapshot(&reader,out);
 if(name&&!strcmp(name,"bash")){int result=pg_bash_tool_snapshot(&shell_tool,out);return result==1?0:result;}
 if(!name||strcmp(name,"edit")||!editor.details)return 0;
 struct json_object *value=json_object_new_object();if(!value)return 5;
 struct json_object *details=json_object_get(editor.details);
 if(json_object_object_add(value,"details",details)){json_object_put(details);json_object_put(value);return 5;}
 *out=value;return 0;
}
static int tool_snapshot(void *ctx,uint64_t id,const struct json_object *call,struct json_object **out){
 (void)ctx;const pg_tool_provider *selected=durable.tool_provider;*out=0;tool_scope *scope=tool_scope_find(id);if(!scope)return 5;if(!scope->base)return 0;
 return selected->snapshot?selected->snapshot(selected->context,id,call,out):0;
}
static const char *tool_schema(void *ctx,uint64_t id,const struct json_object *call){
 (void)ctx;(void)call;tool_scope *scope=tool_scope_find(id);return scope&&scope->definition&&scope->definition->options?scope->definition->options->structured_output_schema:0;
}
static void tool_contract_report(void *ctx,uint64_t id,const char *message){(void)ctx;fprintf(stderr,"PanGu tool %llu contract: %s\n",(unsigned long long)id,message);}
static void tool_progress(void *ctx,uint64_t id,pg_slice text){(void)ctx;(void)id;pg_durable_client_output(&durable.client,id,text);}
static int client_submit(void *ctx,pg_slice text){
 (void)ctx;char expanded[4096];size_t size=0;if(foreground_resources.expand(foreground_resources.context,text,expanded,sizeof(expanded),&size))return 1;
 return durable.native_client.submit(durable.native_client.context,(pg_slice){expanded,size});
}
static void client_cancel(void *ctx){(void)ctx;durable.native_client.cancel(durable.native_client.context);}
static int client_poll(void *ctx,pg_event *event){(void)ctx;foreground();return durable.native_client.poll(durable.native_client.context,event);}
static int prompt_probe(void *context,const char *path){
 pg_read_tool *root=context;struct stat metadata;
 if(fstatat(root->directory,path,&metadata,AT_SYMLINK_NOFOLLOW))return errno==ENOENT||errno==ENOTDIR?0:-1;
 return S_ISREG(metadata.st_mode)?1:0;
}
static struct {pg_tools *files;pg_read_tool *root;const pg_tool_registry *declared;} resource_sources;
static int resource_build(void *context,pg_resource_candidate *out){
 (void)context;pg_tools *files=resource_sources.files;pg_read_tool *root=resource_sources.root;const pg_tool_registry *declared=resource_sources.declared;
 pg_prompt_workspace_source source={files,root,prompt_probe,declared,{application.path,strlen(application.path)},0,0,0,{0,0},0,0,0,{0,0},{0,0},0,{0,0}};
 size_t prompt_size=0;const char *file=getenv("PANGU_SYSTEM_PROMPT_FILE");
 {
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
  char skills_prompt[4096],project_base[4096],user_settings[4096],project_settings[4096];size_t skills_size=0;struct json_object *skills=0,*templates=0,*settings=0,*skill_paths=0,*prompt_paths=0;
  int n;
  n=snprintf(project_base,sizeof(project_base),"%s/.pangu",application.path);if(n<0||(size_t)n>=sizeof(project_base)){pg_read_tool_close(&resources);return 1;}
  n=snprintf(project_settings,sizeof(project_settings),"%s/settings.json",project_base);if(n<0||(size_t)n>=sizeof(project_settings)){pg_read_tool_close(&resources);return 1;}
  if(have_user){n=snprintf(user_settings,sizeof(user_settings),"%s/settings.json",user_canonical);if(n<0||(size_t)n>=sizeof(user_settings)){pg_read_tool_close(&resources);return 1;}}
  unsigned reader=3;for(size_t i=0;i<declared->count;i++){if(!strcmp(declared->entries[i].name,"read")){reader=0;break;}if(!strcmp(declared->entries[i].name,"bash"))reader=1;}
  int result=pg_settings_load(have_user?user_settings:0,project_settings,&settings);
  if(!result){char models_path[4096];struct json_object *catalog=0;int n=have_user?snprintf(models_path,sizeof(models_path),"%s/models.json",user_canonical):0;if(n<0||(size_t)n>=sizeof(models_path))result=1;else if(pg_models_load(have_user?models_path:0,&catalog))result=1;else if(json_object_object_add(settings,"modelCatalog",catalog)){json_object_put(catalog);result=1;}}

  if(!result){struct json_object *catalog=json_object_object_get(settings,"modelCatalog"),*diagnostics=json_object_object_get(catalog,"diagnostics");for(size_t i=0;i<json_object_array_length(diagnostics);i++){struct json_object *entry=json_object_array_get_idx(diagnostics,i);fprintf(stderr,"Models warning: %s: %s\n",json_object_get_string(json_object_object_get(entry,"path")),json_object_get_string(json_object_object_get(entry,"message")));}}
  if(!result){struct json_object *diagnostics=json_object_object_get(settings,"diagnostics");for(size_t i=0;i<json_object_array_length(diagnostics);i++){struct json_object *entry=json_object_array_get_idx(diagnostics,i);fprintf(stderr,"Settings warning: %s: %s\n",json_object_get_string(json_object_object_get(entry,"path")),json_object_get_string(json_object_object_get(entry,"message")));}}
  if(!result)result=pg_settings_resource_sources(settings,"skills",have_user?user_canonical:0,project_base,&skill_paths);
  if(!result)result=pg_settings_resource_sources(settings,"prompts",have_user?user_canonical:0,project_base,&prompt_paths);
  if(!result)result=pg_skills_load_config(0,0,skill_paths,&skills);
  if(!result){struct json_object *diagnostics=json_object_object_get(skills,"diagnostics");for(size_t i=0;i<json_object_array_length(diagnostics);i++){struct json_object *entry=json_object_array_get_idx(diagnostics,i);fprintf(stderr,"Skill warning: %s: %s\n",json_object_get_string(json_object_object_get(entry,"path")),json_object_get_string(json_object_object_get(entry,"message")));}}
  if(!result)result=pg_prompt_templates_load_config(0,0,prompt_paths,&templates);
  if(!result)result=pg_skills_prompt(skills,reader,skills_prompt,sizeof(skills_prompt),&skills_size);
  if(!result){source.skills_prompt=(pg_slice){skills_prompt,skills_size};result=file?pg_prompt_from_file(files,(pg_slice){file,strlen(file)},configured_prompt,sizeof(configured_prompt),&prompt_size):pg_prompt_workspace(&source,configured_prompt,sizeof(configured_prompt),&prompt_size);}
  json_object_put(skill_paths);json_object_put(prompt_paths);pg_read_tool_close(&resources);if(result){json_object_put(settings);json_object_put(skills);json_object_put(templates);return 1;}out->skills=skills;out->templates=templates;out->settings=settings;
 }
 out->prompt=json_object_new_string_len(configured_prompt,(int)prompt_size);return out->prompt?0:5;
}
static int resource_busy(void *context){(void)context;return durable.client.busy||durable.client.active||durable.scheduler.busy||durable.model_leased||durable.tool_scopes!=0;}
static void resource_release(struct json_object *snapshot){json_object_put(snapshot);}
static int agent_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;const pg_model_provider *selected=pg_host_get(scope,owner,"model.provider");const pg_tool_provider *toolset=pg_host_get(scope,owner,"tools.provider");
 pg_tools *executor=toolset?(pg_tools *)&toolset->tools:0;
 if(!toolset||toolset->abi!=PG_TOOL_PROVIDER_ABI||toolset->size!=sizeof(*toolset)||!toolset->catalog||!toolset->identity||!*toolset->identity||strlen(toolset->identity)>63)return 1;
 if(!selected||!executor||!executor->execute||!executor->authorize||pg_host_get(scope,owner,"session")!=&durable.session)return 1;
 if(selected->abi!=PG_MODEL_PROVIDER_ABI||selected->size!=sizeof(*selected)||!selected->identity||!*selected->identity||strlen(selected->identity)>63||!selected->configure||!selected->model.start||!selected->model.next||!selected->model.close)return 1;
 durable.model_metadata=pg_host_find(scope->parent,"model.metadata");if(durable.model_metadata&&(durable.model_metadata->abi!=PG_MODEL_METADATA_ABI||durable.model_metadata->size!=sizeof(pg_model_metadata)||!durable.model_metadata->describe))return 1;
 durable.model_provider=selected;durable.tool_provider=toolset;durable.selected_tools=*executor;durable.extensions=pg_host_find(scope->parent,"extensions.registry");if(pg_extension_registry_open(&durable.extension_registry,durable.extensions))return 1;durable.extensions=&durable.extension_registry.facade;pg_extension_registry_service(&durable.extension_registry,&durable.extension_control);
 pg_tools *files=pg_host_get(scope,owner,"files");if(!files)return 1;
 pg_read_tool *root=pg_host_get(scope,owner,"workspace");if(!root)return 1;
 const pg_tool_registry *declared=toolset->catalog;if(!declared)return 1;durable.declared_tools=declared;
 resource_sources.files=files;resource_sources.root=root;resource_sources.declared=declared;
 const char *model=getenv("PANGU_MODEL_NAME"),*endpoint=getenv("PANGU_MODEL_ENDPOINT"),*api=getenv("PANGU_MODEL_API"),*base=getenv("PANGU_MODEL_BASE_URL");if(!model)model=getenv("MIMO_MODEL");if(!model)model="mimo-v2.6-pro";if(!endpoint&&!api)endpoint="https://token-plan-cn.xiaomimimo.com/v1/chat/completions";
 durable.configuration=json_object_new_object();if(!durable.configuration)return 1;
 if(json_object_object_add(durable.configuration,"model",json_object_new_string(model))||(endpoint&&json_object_object_add(durable.configuration,"endpoint",json_object_new_string(endpoint)))||json_object_object_add(durable.configuration,"workspace",json_object_new_string(application.path)))return 1;
 if((api&&json_object_object_add(durable.configuration,"api",json_object_new_string(api)))||(base&&json_object_object_add(durable.configuration,"baseUrl",json_object_new_string(base))))return 1;
 const char *execution=getenv("PANGU_TOOL_EXECUTION");if(execution){if(strcmp(execution,"parallel")&&strcmp(execution,"sequential"))return 1;if(json_object_object_add(durable.configuration,"toolExecution",json_object_new_string(execution)))return 1;}
 const char *options=getenv("PANGU_MODEL_OPTIONS");if(options){
  size_t size=strlen(options);if(size>=4096)return 1;struct json_tokener *parser=json_tokener_new();if(!parser)return 1;
  json_tokener_set_flags(parser,JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8);struct json_object *value=json_tokener_parse_ex(parser,options,(int)size+1);
  int invalid=json_tokener_get_error(parser)!=json_tokener_success||!json_object_is_type(value,json_type_object);json_tokener_free(parser);
  if(invalid){json_object_put(value);return 1;}if(json_object_object_add(durable.configuration,"options",value)){json_object_put(value);return 1;}
 }
 if(json_object_object_add(durable.configuration,"provider",json_object_new_string(selected->identity)))return 1;
 durable.resource_host=(pg_resource_host){0,resource_build,resource_busy};if(pg_resources_open(&durable.resources,&durable.resource_host,durable.configuration))return 1;
 foreground_resources=(pg_resource_ops){&durable.resources,pg_resources_reload,pg_resources_snapshot,resource_release,pg_resources_expand};
 pg_record_memory_documents(&durable.backend.memory,&durable.task_history.documents);durable.task_history.context=&durable.backend.memory;durable.task_history.entry=task_history_entry;
 durable.tasks=(pg_task_runner_host){0,record_read,record_scan,document_scan,resolve,wall_time,report};
 durable.tool_host=(pg_durable_tool_host){0,tool_acquire,tool_release,mint,tool_progress,tool_snapshot,tool_schema,tool_contract_report};if(pg_durable_tool_init(&durable.tool,&durable.tool_host))return 1;if(durable.extensions)pg_durable_tool_bind_hooks(&durable.tool,&extension_tool_hooks);
 durable.prompt_preparer=(pg_prompt_preparer){durable.extensions,durable.declared_tools,0,prepare_load,prepare_finish,wall_time};durable.async_prepare=(pg_durable_prepare){&durable.prompt_preparer,pg_prompt_preparer_poll,pg_prompt_preparer_close};
 durable.generation_host=(pg_durable_generation_host){0,prepare,model_acquire,model_release,mint,model_progress,&durable.tool.definition,model_failure,&durable.inputs,model_complete,model_fetch_deferred,model_cancel_deferred};if(pg_durable_generation_init(&durable.generation,&durable.generation_host))return 1;pg_durable_generation_bind_prepare(&durable.generation,&durable.async_prepare);if(durable.extensions)pg_durable_generation_bind_hooks(&durable.generation,&extension_generation_hooks);
 durable.compaction_host=durable.generation_host;durable.compaction_host.prepare=prepare;durable.compaction_host.acquire=compaction_acquire;durable.compaction_host.progress=0;if(pg_durable_compaction_init(&durable.compaction,&durable.compaction_host))return 1;pg_durable_generation_bind_compaction(&durable.generation,&durable.compaction.definition);const pg_compaction_hooks *hooks=pg_host_find(scope->parent,"compaction.hooks");if(hooks&&(hooks->abi!=PG_COMPACTION_HOOKS_ABI||hooks->size!=sizeof(*hooks)||!hooks->report||(hooks->count&&!hooks->entries)))return 1;pg_durable_compaction_bind_hooks(&durable.compaction,hooks);pg_durable_compaction_bind_extensions(&durable.compaction,&extension_generation_hooks);
 durable.compaction_binding=(pg_compaction_binding){&durable.compaction,&durable.session,&durable.tasks,&durable.scheduler};pg_durable_compaction_service(&durable.compaction_binding,&durable.compaction_ops);foreground_compaction=durable.compaction_ops;
 pg_record_memory_queries(&durable.backend.memory,&durable.queries);pg_record_memory_requests(&durable.backend.memory,&durable.requests);if(pg_durable_inputs_open(&durable.inputs,&durable.session,&durable.tasks,&durable.generation,&durable.queries,&durable.requests))return 1;
 extension_tasks=pg_host_find(scope->parent,"task.definitions");const pg_task_definition *builtins[]={&durable.generation.definition,&durable.tool.definition,&durable.compaction.definition};if(pg_task_provider_validate(extension_tasks,builtins,3))return 1;
 json_object *agent_state=0,*selected_record=0;if(record_read(0,PG_TABLE_CONVERSATION,startup_conversation,&selected_record))return 1;json_object_put(selected_record);if(pg_agent_state_read(&durable.session,startup_conversation,&agent_state))return 1;pg_extension_result initial_extensions={0};int invalid_extensions=pg_extensions_resolve(durable.extensions,agent_state,durable.declared_tools,&initial_extensions);pg_extensions_close(&initial_extensions);json_object_put(agent_state);if(invalid_extensions)return 1;
 durable.task_resolver=(pg_task_resolver){0,task_acquire,pg_extension_registry_release};if(pg_task_scheduler_bind_definitions(&durable.scheduler,&durable.task_resolver)||pg_task_scheduler_open(&durable.scheduler,&durable.session,&durable.tasks,5))return 1;
 pg_durable_client_config config={&durable.session,&durable.scheduler,&durable.inputs,startup_conversation};if(pg_durable_client_open(&durable.client,&config))return 1;durable.client_ready=1;if(pg_durable_client_conversations_service(&durable.client,&foreground_conversations))return 1;foreground_conversations.create=conversation_create;if(pg_durable_client_catalog_service(&durable.client,&foreground_conversation_catalog))return 1;foreground_conversation_catalog.rename=conversation_rename;if(pg_durable_client_actions_service(&durable.client,&foreground_conversation_actions))return 1;
 durable.records.context=&durable.client;durable.records.publish=pg_durable_client_publish;
 if(foreground_conversations.history(foreground_conversations.context,&durable.history))return 1;
 durable.native_client=pg_durable_client_service(&durable.client);application.client=(pg_client){0,client_submit,client_cancel,client_poll};
 foreground_agent_state=(pg_agent_state_ops){0,agent_state_read,agent_state_configure};foreground_model_catalog=(pg_model_catalog_ops){0,catalog_list,catalog_release};
 conversation_view_binding=(pg_conversation_view_binding){&durable.session,&durable.tasks,0,0,0};pg_conversation_view_service(&conversation_view_binding,&foreground_conversation_view);pg_conversation_history_service(&conversation_view_binding,&foreground_conversation_history);pg_task_graph_service(&task_graph_binding,&durable.scheduler,&foreground_task_graph);pg_task_control_service(&durable.scheduler,&foreground_task_control);pg_usage_service(&durable.session,&foreground_usage);usage_total_binding=(pg_usage_total_binding){&durable.session,&durable.tasks};pg_usage_total_service(&usage_total_binding,&foreground_usage_total);pg_observation_service(&durable.session,&foreground_observation);
 return pg_host_provide(scope,owner,"usage.total",&foreground_usage_total)||pg_host_provide(scope,owner,"models.catalog",&foreground_model_catalog)||pg_host_provide(scope,owner,"conversation.history",&foreground_conversation_history)||pg_host_provide(scope,owner,"task.control",&foreground_task_control)||pg_host_provide(scope,owner,"conversation.actions",&foreground_conversation_actions)||pg_host_provide(scope,owner,"conversation.catalog",&foreground_conversation_catalog)||pg_host_provide(scope,owner,"conversation.client",&foreground_conversations)||pg_host_provide(scope,owner,"extensions.control",&durable.extension_control)||pg_host_provide(scope,owner,"agent.state",&foreground_agent_state)||pg_host_provide(scope,owner,"conversation.view",&foreground_conversation_view)||pg_host_provide(scope,owner,"task.graph",&foreground_task_graph)||pg_host_provide(scope,owner,"observation",&foreground_observation)||pg_host_provide(scope,owner,"usage",&foreground_usage)||pg_host_provide(scope,owner,"client",&application.client)||pg_host_provide(scope,owner,"inputs",&durable.inputs)||pg_host_provide(scope,owner,"resources",&foreground_resources)||pg_host_provide(scope,owner,"compaction",&durable.compaction_ops);
}
static int agent_suspend(void *ctx){(void)ctx;int result=pg_durable_client_suspend(&durable.client);return result==PG_TASK_PENDING?2:result;}
static int agent_stop(void *ctx){
 (void)ctx;pg_conversation_view_close(&conversation_view_binding);pg_view_binding_close(&task_graph_binding);foreground();if(durable.client_ready){int result=pg_durable_client_suspend(&durable.client);if(result)return result==PG_TASK_PENDING?1:-1;durable.client_ready=0;}else if(durable.scheduler.active){if(pg_task_scheduler_close(&durable.scheduler))return 1;}
 if(durable.tool_scopes)return 1;pg_extensions_close(&durable.model_extensions);if(pg_extension_registry_close(&durable.extension_registry))return 1;durable.extensions=0;extension_tasks=0;durable.records.publish=0;json_object_put(durable.history);durable.history=0;if(pg_resources_close(&durable.resources))return 1;json_object_put(durable.configuration);durable.configuration=0;return 0;
}
static pg_read_image_processor native_images={0,pg_image_prepare};
static int images_start(void *ctx,pg_host *scope,size_t owner){(void)ctx;return pg_host_provide(scope,owner,"image.processor",&native_images);}
static const char *const image_exports[]={"image.processor"};
static const char *const workspace_exports[]={"workspace"},*const process_exports[]={"process"},*const files_exports[]={"files"},*const tools_exports[]={"tools","tool.registry","tools.base","tool.registry.base","tools.provider","tools.provider.base"},*const model_exports[]={"model","model.provider","model.provider.base","model.metadata"},*const agent_exports[]={"client","inputs","resources","compaction","usage","usage.total","observation","task.graph","task.control","conversation.view","conversation.client","conversation.catalog","conversation.actions","conversation.history","models.catalog","agent.state","extensions.control"},*const storage_exports[]={"session"};
static const char *const file_imports[]={"workspace"},*const execution_imports[]={"workspace","files","process"},*const execution_exports[]={"execution.env"},*const tool_imports[]={"execution.env","image.processor"},*const model_imports[]={"tool.registry"},*const agent_imports[]={"model.provider","tools.provider","session","files","workspace"};
static const pg_plugin agent_plugin={"agent",0,agent_imports,5,agent_exports,17,agent_start,agent_stop};
static const pg_plugin plugins[]={
 {"images",0,0,0,image_exports,1,images_start,no_resources},
 {"model",0,model_imports,1,model_exports,4,model_start,model_stop},
 {"tools",0,tool_imports,2,tools_exports,6,tools_start,tools_stop},
 {"files",0,file_imports,1,files_exports,1,files_start,no_resources},
 {"process",0,0,0,process_exports,1,process_start,process_stop},
 {"execution",0,execution_imports,3,execution_exports,1,execution_start,no_resources},
 {"workspace",0,0,0,workspace_exports,1,workspace_start,workspace_stop},
 {"storage",0,0,0,storage_exports,1,storage_start,storage_stop},
};
int main(int argc,char **argv){
 pg_client_startup startup={0};int parsed=pg_client_startup_parse(argc,argv,&startup);if(parsed||startup.help){fputs("Usage: pangu [workspace-directory] [--session-dir directory] [--session-id id]\nSet PANGU_MODEL_NAME and PANGU_MODEL_API_KEY; select PANGU_MODEL_API or PANGU_MODEL_ENDPOINT. PANGU_MODEL_BASE_URL overrides the provider base URL. Bedrock accepts AWS environment credentials and shared profiles. Default storage: workspace/.pangu. Ctrl-C cancels; Ctrl-D exits and preserves pending work.\n",parsed?stderr:stdout);return parsed;}if(startup.version){puts("PanGu native development");return 0;}startup_conversation=startup.conversation;
 const char *endpoint=getenv("PANGU_MODEL_ENDPOINT"),*key=getenv("PANGU_MODEL_API_KEY"),*model=getenv("PANGU_MODEL_NAME"),*api=getenv("PANGU_MODEL_API");
 if(api&&(!*api||!model||!*model)){fputs("Model API selection requires an API name and model.\n",stderr);return 2;}
 if(endpoint&&(!*endpoint||!model||!*model)){fputs("Custom model connection requires endpoint and model.\n",stderr);return 2;}if(!key&&!endpoint&&!api)key=getenv("MIMO_API_KEY");if((!key||!*key)&&!(api&&!strcmp(api,"bedrock-converse-stream"))){fputs("Model credentials are required in the environment.\n",stderr);return 2;}
 if(!isatty(0)||!isatty(1)){fputs("PanGu requires an interactive terminal.\n",stderr);return 2;}if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 int rc=2;char *workspace=realpath(startup.workspace,0),*directory=0;if(!workspace)goto cleanup;application.path=workspace;
 const char *selected=startup.session_directory?startup.session_directory:getenv("PANGU_SESSION_DIR");if(selected){directory=strdup(selected);}else{size_t n=strlen(workspace)+8;directory=malloc(n);if(directory)snprintf(directory,n,"%s/.pangu",workspace);}if(!directory)goto cleanup;application.session_directory=directory;
 if(pg_host_init(&host,plugins,sizeof(plugins)/sizeof(*plugins))||pg_host_start(&host)){fprintf(stderr,"Cannot activate native plugin %s (error=%u).\n",host.error_owner<host.count?plugins[host.error_owner].name:"composition",host.error);goto cleanup;}
 pg_host *parent=&host;const char *plugin=getenv("PANGU_PLUGIN");if(plugin){if(pg_native_open(&extension,plugin,&host)){fprintf(stderr,"Cannot activate native extension (error=%u).\n",extension.error);goto cleanup;}parent=&extension.host;}
 if(pg_host_init_child(&agent_scope,parent,&agent_plugin,1)||pg_host_start(&agent_scope)){fprintf(stderr,"Cannot activate Agent consumer (error=%u).\n",agent_scope.error);goto cleanup;}
 tui_tool_observer observer={&durable.client,pg_durable_client_tool_observe,pg_durable_tool_observation_release};
 pg_client *client=pg_host_find(&agent_scope,"client");if(!client)goto cleanup;pg_tui_bridge bridge;if(pg_tui_bridge_init(&bridge,*client)||pg_tui_bridge_set_history_rows(&bridge,durable.history)||pg_tui_bridge_set_detach(&bridge,agent_suspend,0)||pg_tui_bridge_set_tool_observer(&bridge,&observer))goto cleanup;
 if(pg_durable_client_inputs_service(&durable.client,&foreground_inputs))goto cleanup;inputs_owner=&bridge.ops;
 tui_app_config config={STDIN_FILENO,STDOUT_FILENO,1,&bridge.ops,20};int reason=tui_app_run(&config);inputs_owner=0;bridge.ops.shutdown(bridge.ops.ctx);rc=reason<0||reason==TUI_EXIT_POLL_ERROR||bridge.stop_error?1:0;if(rc)fprintf(stderr,"PanGu terminal stopped (reason=%d, bridge=%u).\n",reason,bridge.stop_error);
cleanup:
 for(;;){int closing=pg_host_close(&agent_scope);if(!closing)break;if(closing<0){rc=1;goto retain_resources;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 for(;;){int closing=pg_native_close(&extension);if(!closing)break;if(closing<0){rc=1;goto retain_resources;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 for(;;){int closing=pg_host_close(&host);if(!closing)break;if(closing<0){rc=1;goto retain_resources;}struct timespec pause={0,10000000};nanosleep(&pause,0);}free(directory);free(workspace);curl_global_cleanup();
retain_resources:
 return rc;
}
