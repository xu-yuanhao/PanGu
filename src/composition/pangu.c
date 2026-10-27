#define _XOPEN_SOURCE 700
#include "../runtime/mimo.h"
#include "../runtime/host.h"
#include "../runtime/read_tool.h"
#include "../runtime/write_tool.h"
#include "../runtime/edit_tool.h"
#include "../runtime/bash_tool.h"
#include "../runtime/profile.h"
#include "../runtime/prompt.h"
#include "../runtime/journal.h"
#include "../client/tui/runtime_bridge.h"
#include "../client/tui/tui_app.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
/* Explicit static composition: neither Loop nor TUI chooses a provider. */
static pg_agent agent_storage;
static pg_host host;
static struct {const char *path,*session_directory;int storage_directory,storage_loaded;pg_session_source source;pg_model model;pg_tools tools,files;pg_process_ops process;pg_client client;int agent_ready;} application;
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
static pg_journal journal={.file=-1,.directory=-1};

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
 return pg_host_provide(scope,owner,"tools",&application.tools)||pg_host_provide(scope,owner,"tool.registry",&registry);
}
static int tools_stop(void *ctx){(void)ctx;return application.tools.close?application.tools.close(application.tools.context):0;}
static int model_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_tool_registry *tools=pg_host_get(scope,owner,"tool.registry");if(!tools)return 1;
 application.model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,tools);return pg_host_provide(scope,owner,"model",&application.model);
}
static int model_stop(void *ctx){(void)ctx;if(application.model.close)application.model.close(application.model.context);return 0;}
static int volatile_load(void *ctx,pg_session *session,pg_session_control *control){
 (void)ctx;(void)session;if(application.storage_loaded)return 1;application.storage_loaded=1;control->storage=(pg_storage){0,pg_session_volatile_commit};return 0;
}
static int journal_load(void *ctx,pg_session *session,pg_session_control *control){
 (void)ctx;if(application.storage_loaded)return 1;application.storage_loaded=1;
 application.storage_directory=open(application.session_directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 return application.storage_directory<0||pg_journal_open(&journal,application.storage_directory,"session.pgj",session,control);
}
static int storage_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;application.storage_directory=-1;application.storage_loaded=0;
 application.source=(pg_session_source){0,application.session_directory?journal_load:volatile_load};
 return pg_host_provide(scope,owner,"session.source",&application.source);
}
static int storage_stop(void *ctx){
 (void)ctx;pg_journal_close(&journal);if(application.storage_directory>=0)close(application.storage_directory);application.storage_directory=-1;return 0;
}
static int agent_start(void *ctx,pg_host *scope,size_t owner){
 (void)ctx;pg_model *model=pg_host_get(scope,owner,"model");pg_tools *tools=pg_host_get(scope,owner,"tools");
 if(!model||!tools||!pg_host_get(scope,owner,"session.source")||pg_agent_init(&agent_storage,*model,*tools))return 1;application.agent_ready=1;application.client=pg_agent_client(&agent_storage);
 return pg_host_provide(scope,owner,"agent",&agent_storage)||pg_host_provide(scope,owner,"client",&application.client);
}
static int agent_stop(void *ctx){
 (void)ctx;if(!application.agent_ready)return 0;int status=pg_agent_suspend(&agent_storage);if(status==2)return 1;if(status)return -1;application.agent_ready=0;return 0;
}
static const char *const workspace_exports[]={"workspace"},*const process_exports[]={"process"},*const files_exports[]={"files"},*const tools_exports[]={"tools","tool.registry"},*const model_exports[]={"model"},*const agent_exports[]={"agent","client"},*const storage_exports[]={"session.source"};
static const char *const file_imports[]={"workspace"},*const tool_imports[]={"workspace","files","process"},*const model_imports[]={"tool.registry"},*const agent_imports[]={"model","tools","session.source"};
static const pg_plugin plugins[]={
 {"agent",0,agent_imports,3,agent_exports,2,agent_start,agent_stop},
 {"model",0,model_imports,1,model_exports,1,model_start,model_stop},
 {"tools",0,tool_imports,3,tools_exports,2,tools_start,tools_stop},
 {"files",0,file_imports,1,files_exports,1,files_start,no_resources},
 {"process",0,0,0,process_exports,1,process_start,process_stop},
 {"workspace",0,0,0,workspace_exports,1,workspace_start,workspace_stop},
 {"storage",0,0,0,storage_exports,1,storage_start,storage_stop},
};

int main(int argc,char **argv){
 if(argc>2 || (argc==2 && (!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h")))){fprintf(stderr,"Usage: pangu [workspace-directory]\nSet PANGU_MODEL_ENDPOINT, PANGU_MODEL_NAME and PANGU_MODEL_API_KEY for an OpenAI-compatible provider, or MIMO_API_KEY with optional MIMO_MODEL. PANGU_SYSTEM_PROMPT_FILE optionally replaces the default system prompt from a workspace-relative UTF-8 file. PANGU_SESSION_DIR optionally selects an existing private directory for session.pgj. Ctrl-C cancels while busy and exits while idle; Ctrl-D exits. Persistent exit preserves pending work. Tools: read, write, edit and bash. Commands are not sandboxed; shell environment contains PATH and LANG only.\n");return argc>2?2:0;}
 const char *endpoint=getenv("PANGU_MODEL_ENDPOINT"),*api_key=getenv("PANGU_MODEL_API_KEY"),*selected_model=getenv("PANGU_MODEL_NAME");
 if(endpoint&&(!*endpoint||!selected_model||!*selected_model)){fputs("Custom model connection requires PANGU_MODEL_ENDPOINT and PANGU_MODEL_NAME.\n",stderr);return 2;}
 if(!api_key&&!endpoint)api_key=getenv("MIMO_API_KEY");
 if(!api_key||!*api_key){fputs("Model API key is required in the environment.\n",stderr);return 2;}
 if(!isatty(STDIN_FILENO)||!isatty(STDOUT_FILENO)){fputs("PanGu requires an interactive terminal.\n",stderr);return 2;}
 if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 int rc=2;
 pg_agent *agent=0;
 char *workspace=0;
 const char *path=argc==2?argv[1]:".",*session_directory=getenv("PANGU_SESSION_DIR");
 application.path=path;application.session_directory=session_directory;
 if(pg_host_init(&host,plugins,sizeof(plugins)/sizeof(*plugins))||pg_host_start(&host)){fprintf(stderr,"Cannot activate native plugin %s (error=%u).\n",host.error_owner<host.count?plugins[host.error_owner].name:"composition",host.error);goto cleanup;}
 pg_session_source *source=pg_host_find(&host,"session.source");
 agent=pg_host_find(&host,"agent");pg_client *client=pg_host_find(&host,"client");
 pg_read_tool *root=pg_host_find(&host,"workspace");pg_tools *file_service=pg_host_find(&host,"files");pg_tool_registry *tool_registry=pg_host_find(&host,"tool.registry");
 if(!source||!agent||!client||!root||!file_service||!tool_registry)goto cleanup;
 if(session_directory){
  struct stat status;unsigned char identity[4096];size_t identity_size=0;
  const char *model_name=selected_model;if(!model_name)model_name=getenv("MIMO_MODEL");if(!model_name)model_name="mimo-v2.6-pro";
  workspace=realpath(path,0);
  if(!workspace || fstat(root->directory,&status)){fputs("Cannot identify workspace.\n",stderr);goto cleanup;}
  pg_profile_spec profile={.provider={endpoint?endpoint:"mimo-token-plan-cn/v1",strlen(endpoint?endpoint:"mimo-token-plan-cn/v1")},.model={model_name,strlen(model_name)},.workspace={workspace,strlen(workspace)},.implementation={"pangu.native.read-write-edit-bash-v2",sizeof("pangu.native.read-write-edit-bash-v2")-1},.tools=tool_registry,.device=(uint64_t)status.st_dev,.inode=(uint64_t)status.st_ino};
  if(pg_profile_encode(&profile,identity,sizeof(identity),&identity_size)){fputs("Runtime profile exceeds supported bounds or contains invalid configuration.\n",stderr);goto cleanup;}
  if(source->load(source->context,&agent->session,&agent->commits)){fputs("Cannot open persistent session (directory, lock or journal validation).\n",stderr);goto cleanup;}
  if(!agent->commits.profile_size && agent->commits.sequence){fputs("Persistent session has no runtime identity; automatic adoption is refused.\n",stderr);goto cleanup;}
  if(pg_agent_bind_profile(agent,(pg_slice){(const char*)identity,identity_size})){fputs("Pending session runtime profile mismatch; restore its original model, tools and workspace.\n",stderr);goto cleanup;}
 }
 if(!session_directory&&source->load(source->context,&agent->session,&agent->commits))goto cleanup;
 const char *prompt="You are PanGu, a minimal native coding agent. Answer concisely. Use read to inspect workspace files and write to create or overwrite files when requested. Use edit for targeted replacements against original file content. File tool paths are relative to the configured workspace. Use bash for commands in that working directory; commands are not sandboxed. The shell receives PATH and LANG only, not your credentials or the parent environment. For long output, use the reported raw output file if needed. If a tool was interrupted and may have partially run, inspect current state before deciding whether to repeat a write or command. Never claim an action that was not executed.";
 size_t prompt_size=strlen(prompt);
 const char *prompt_file=getenv("PANGU_SYSTEM_PROMPT_FILE");
 if((!agent->commits.run.id || agent->commits.run.phase==PG_RUN_TERMINAL) && prompt_file){
  if(pg_prompt_from_file(file_service,(pg_slice){prompt_file,strlen(prompt_file)},configured_prompt,sizeof(configured_prompt),&prompt_size)){fputs("Cannot load configured system prompt (workspace-relative UTF-8 file, max 4095 bytes).\n",stderr);goto cleanup;}
  prompt=configured_prompt;
 }
 if((!agent->commits.run.id || agent->commits.run.phase==PG_RUN_TERMINAL) &&
    (agent->session.system_size!=prompt_size || memcmp(agent->session.system_prompt,prompt,prompt_size)) &&
    pg_agent_set_system(agent,(pg_slice){prompt,prompt_size}))goto cleanup;
 if(session_directory && agent->commits.run.id && agent->commits.run.phase!=PG_RUN_TERMINAL){
  int resumed=pg_agent_resume(agent,0);
  if(resumed){fputs(resumed==2?"Pending session is parked at its recovery limit; explicit recovery is required.\n":"Cannot resume pending session.\n",stderr);goto cleanup;}
 }
 pg_tui_bridge bridge;if(pg_tui_bridge_init(&bridge,*client))goto cleanup;
 if(pg_tui_bridge_set_history(&bridge,&agent->session))goto cleanup;
 if(session_directory && pg_tui_bridge_set_detach(&bridge,pg_agent_suspend,agent))goto cleanup;
 tui_app_config config={STDIN_FILENO,STDOUT_FILENO,1,&bridge.ops,20};
 int reason=tui_app_run(&config);bridge.ops.shutdown(bridge.ops.ctx);rc=reason<0||reason==TUI_EXIT_POLL_ERROR||bridge.stop_error?1:0;
cleanup:
 for(;;){int closing=pg_host_close(&host);if(!closing)break;if(closing<0){rc=1;break;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 free(workspace);
 curl_global_cleanup();return rc;
}
