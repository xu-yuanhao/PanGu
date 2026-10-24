#define _POSIX_C_SOURCE 200809L
#include "../runtime/mimo.h"
#include "../runtime/read_tool.h"
#include "../client/tui/runtime_bridge.h"
#include "../client/tui/tui_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Explicit static composition: neither Loop nor TUI chooses a provider. */
static pg_agent agent;
static pg_mimo provider;
static pg_tool_registry registry;
static pg_read_tool reader={.directory=-1};
int main(int argc,char **argv){
 if(argc>2 || (argc==2 && (!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h")))){fprintf(stderr,"Usage: pangu [workspace-directory]\nSet MIMO_API_KEY in the environment; MIMO_MODEL is optional. Ctrl-C cancels while busy and exits while idle; Ctrl-D exits. Only read is enabled.\n");return argc>2?2:0;}
 if(!getenv("MIMO_API_KEY") || !*getenv("MIMO_API_KEY")){fputs("MIMO_API_KEY is required (environment only).\n",stderr);return 2;}
 if(!isatty(STDIN_FILENO)||!isatty(STDOUT_FILENO)){fputs("PanGu requires an interactive terminal.\n",stderr);return 2;}
 if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 int rc=2;
 if(pg_read_tool_open(&reader,argc==2?argv[1]:".")){fputs("Cannot open workspace directory.\n",stderr);goto cleanup;}
 pg_tool_definition read=pg_read_tool_definition(&reader);if(pg_tool_registry_init(&registry,&read,1))goto cleanup;
 pg_model model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,&registry);
 if(pg_agent_init(&agent,model,pg_tool_registry_service(&registry)))goto cleanup;
 const char *prompt="You are PanGu, a minimal native coding agent. Answer concisely. Use the read tool to inspect workspace files when requested. Paths are relative to the configured workspace. You cannot write files or execute commands. Never claim an action that was not executed.";
 if(pg_agent_set_system(&agent,(pg_slice){prompt,strlen(prompt)}))goto cleanup;
 pg_tui_bridge bridge;if(pg_tui_bridge_init(&bridge,pg_agent_client(&agent)))goto cleanup;
 tui_app_config config={STDIN_FILENO,STDOUT_FILENO,1,&bridge.ops,20};
 int reason=tui_app_run(&config);bridge.ops.shutdown(bridge.ops.ctx);rc=reason<0||reason==TUI_EXIT_POLL_ERROR?1:0;
cleanup:
 pg_read_tool_close(&reader);curl_global_cleanup();return rc;
}
