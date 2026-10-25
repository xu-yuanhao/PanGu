#define _XOPEN_SOURCE 700
#include "../runtime/mimo.h"
#include "../runtime/read_tool.h"
#include "../runtime/write_tool.h"
#include "../runtime/edit_tool.h"
#include "../runtime/bash_tool.h"
#include "../runtime/profile.h"
#include "../runtime/journal.h"
#include "../client/tui/runtime_bridge.h"
#include "../client/tui/tui_app.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* Explicit static composition: neither Loop nor TUI chooses a provider. */
static pg_agent agent;
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
int main(int argc,char **argv){
 if(argc>2 || (argc==2 && (!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h")))){fprintf(stderr,"Usage: pangu [workspace-directory]\nSet MIMO_API_KEY in the environment; MIMO_MODEL is optional. PANGU_SESSION_DIR optionally selects an existing private directory for session.pgj. Ctrl-C cancels while busy and exits while idle; Ctrl-D exits. Persistent exit preserves pending work. Tools: read, write, edit and bash. Commands are not sandboxed; shell environment contains PATH and LANG only.\n");return argc>2?2:0;}
 if(!getenv("MIMO_API_KEY") || !*getenv("MIMO_API_KEY")){fputs("MIMO_API_KEY is required (environment only).\n",stderr);return 2;}
 if(!isatty(STDIN_FILENO)||!isatty(STDOUT_FILENO)){fputs("PanGu requires an interactive terminal.\n",stderr);return 2;}
 if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 int rc=2,storage=-1,initialized=0;
 char *workspace=0;
 const char *path=argc==2?argv[1]:".",*session_directory=getenv("PANGU_SESSION_DIR");
 if(pg_read_tool_open(&reader,path)){fputs("Cannot open workspace directory.\n",stderr);goto cleanup;}
 writer.directory=reader.directory;
 pg_process_init(&command_process);
 pg_process_ops command_ops=pg_process_service(&command_process);
 if(pg_bash_tool_init(&shell_tool,&command_ops,reader.directory,command_environment))goto cleanup;
 pg_tool_definition file_tools[]={pg_read_snapshot_definition(&reader),pg_write_tool_definition(&writer)};
 if(pg_tool_registry_init(&file_registry,file_tools,2))goto cleanup;
 editor.files=pg_tool_registry_service(&file_registry);
 pg_tool_definition tools[]={pg_read_tool_definition(&reader),pg_write_tool_definition(&writer),pg_edit_tool_definition(&editor),pg_bash_tool_definition(&shell_tool)};if(pg_tool_registry_init(&registry,tools,4))goto cleanup;
 pg_model model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,&registry);
 if(pg_agent_init(&agent,model,pg_tool_registry_service(&registry)))goto cleanup;
 initialized=1;
 if(session_directory){
  struct stat status;unsigned char identity[4096];size_t identity_size=0;
  const char *model_name=getenv("MIMO_MODEL");if(!model_name)model_name="mimo-v2.6-pro";
  workspace=realpath(path,0);
  if(!workspace || fstat(reader.directory,&status)){fputs("Cannot identify workspace.\n",stderr);goto cleanup;}
  pg_profile_spec profile={.provider={"mimo-token-plan-cn/v1",sizeof("mimo-token-plan-cn/v1")-1},.model={model_name,strlen(model_name)},.workspace={workspace,strlen(workspace)},.implementation={"pangu.native.read-write-edit-bash-v2",sizeof("pangu.native.read-write-edit-bash-v2")-1},.tools=&registry,.device=(uint64_t)status.st_dev,.inode=(uint64_t)status.st_ino};
  if(pg_profile_encode(&profile,identity,sizeof(identity),&identity_size)){fputs("Runtime profile exceeds supported bounds or contains invalid configuration.\n",stderr);goto cleanup;}
  storage=open(session_directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
  if(storage<0 || pg_journal_open(&journal,storage,"session.pgj",&agent.session,&agent.commits)){fputs("Cannot open persistent session (directory, lock or journal validation).\n",stderr);goto cleanup;}
  if(!agent.commits.profile_size && agent.commits.sequence){fputs("Persistent session has no runtime identity; automatic adoption is refused.\n",stderr);goto cleanup;}
  if(pg_agent_bind_profile(&agent,(pg_slice){(const char*)identity,identity_size})){fputs("Pending session runtime profile mismatch; restore its original model, tools and workspace.\n",stderr);goto cleanup;}
 }
 const char *prompt="You are PanGu, a minimal native coding agent. Answer concisely. Use read to inspect workspace files and write to create or overwrite files when requested. Use edit for targeted replacements against original file content. File tool paths are relative to the configured workspace. Use bash for commands in that working directory; commands are not sandboxed. The shell receives PATH and LANG only, not your credentials or the parent environment. For long output, use the reported raw output file if needed. If a tool was interrupted and may have partially run, inspect current state before deciding whether to repeat a write or command. Never claim an action that was not executed.";
 size_t prompt_size=strlen(prompt);
 if((!agent.commits.run.id || agent.commits.run.phase==PG_RUN_TERMINAL) &&
    (agent.session.system_size!=prompt_size || memcmp(agent.session.system_prompt,prompt,prompt_size)) &&
    pg_agent_set_system(&agent,(pg_slice){prompt,prompt_size}))goto cleanup;
 if(session_directory && agent.commits.run.id && agent.commits.run.phase!=PG_RUN_TERMINAL){
  int resumed=pg_agent_resume(&agent,0);
  if(resumed){fputs(resumed==2?"Pending session is parked at its recovery limit; explicit recovery is required.\n":"Cannot resume pending session.\n",stderr);goto cleanup;}
 }
 pg_tui_bridge bridge;if(pg_tui_bridge_init(&bridge,pg_agent_client(&agent)))goto cleanup;
 if(pg_tui_bridge_set_history(&bridge,&agent.session))goto cleanup;
 if(session_directory && pg_tui_bridge_set_detach(&bridge,pg_agent_suspend,&agent))goto cleanup;
 tui_app_config config={STDIN_FILENO,STDOUT_FILENO,1,&bridge.ops,20};
 int reason=tui_app_run(&config);bridge.ops.shutdown(bridge.ops.ctx);rc=reason<0||reason==TUI_EXIT_POLL_ERROR||bridge.stop_error?1:0;
cleanup:
 if(initialized && pg_agent_suspend(&agent))rc=1;
 pg_journal_close(&journal);if(storage>=0)close(storage);free(workspace);
 pg_read_tool_close(&reader);curl_global_cleanup();return rc;
}
