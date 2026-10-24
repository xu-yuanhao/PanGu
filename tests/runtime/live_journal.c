#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo.h"
#include "../../src/runtime/journal.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static pg_agent agent;
static pg_mimo provider;
static pg_journal journal={.file=-1,.directory=-1};
int main(int argc,char **argv){
 if(argc!=3)return 2;
 int recall=!strcmp(argv[1],"recall"),resume=!strcmp(argv[1],"resume"),interrupt=!strcmp(argv[1],"interrupt");
 if(!recall&&!resume&&!interrupt&&strcmp(argv[1],"store"))return 2;
 if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 int result=2,dir=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir<0)goto finish;
 if(pg_agent_init(&agent,pg_mimo_model(&provider),(pg_tools){0}))goto cleanup;
 if(pg_journal_open(&journal,dir,"live.pgj",&agent.session,&agent.commits))goto cleanup;
 if(recall){if(agent.session.count!=2||agent.commits.run.phase!=PG_RUN_TERMINAL)goto cleanup;}
 else if(resume){if(agent.session.count!=1||agent.commits.run.phase!=PG_RUN_MODEL||agent.commits.run.turns!=1)goto cleanup;}
 else{const char *system="You are a minimal agent test assistant. Remember facts only from the supplied conversation. Answer concisely.";if(pg_agent_set_system(&agent,(pg_slice){system,strlen(system)}))goto cleanup;}
 uint64_t before=agent.commits.sequence;
 const char *prompt=recall?"What was the exact marker? Reply with only that marker.":"Remember the marker PANGU_DISK_8264. Reply only ACK.";
 const char *expected=recall?"PANGU_DISK_8264":"ACK";
 pg_client client=pg_agent_client(&agent);size_t first=agent.session.count;
 if(resume){if(pg_agent_resume(&agent,0))goto cleanup;}
 else if(client.submit(client.context,(pg_slice){prompt,strlen(prompt)}))goto cleanup;
 for(unsigned i=0;i<7000;++i){pg_event event;if(client.poll(client.context,&event)){
  if(interrupt&&event.kind==PG_TEXT_DELTA){if(pg_agent_suspend(&agent)||agent.request_open||agent.commits.run.phase!=PG_RUN_MODEL||agent.session.count!=1)goto cleanup;
   printf("live_journal phase=interrupt http=%ld verified=1 sequence=%llu messages=%zu checkpoint=%d\n",provider.http,(unsigned long long)agent.commits.sequence,agent.session.count,agent.commits.run.phase);result=0;goto cleanup;}
  if(event.kind==PG_RUN_FINISHED){int found=0;for(size_t j=first;j<agent.session.count;++j)if(agent.session.messages[j].role==PG_ASSISTANT&&strstr(agent.session.messages[j].text,expected))found=1;
   result=event.outcome!=PG_SUCCESS||!found||agent.commits.sequence!=before+3||agent.commits.run.phase!=PG_RUN_TERMINAL;
   printf("live_journal phase=%s outcome=%d http=%ld verified=%d sequence=%llu messages=%zu checkpoint=%d\n",argv[1],event.outcome,provider.http,found,(unsigned long long)agent.commits.sequence,agent.session.count,agent.commits.run.phase);goto cleanup;}
 }
 struct timespec pause={0,10000000};nanosleep(&pause,0);
 }
 client.cancel(client.context);for(unsigned i=0;i<8;++i){pg_event event;if(client.poll(client.context,&event)&&event.kind==PG_RUN_FINISHED)break;}
cleanup:
 pg_journal_close(&journal);close(dir);
finish:
 curl_global_cleanup();return result;
}
