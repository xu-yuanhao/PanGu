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
 int recall=!strcmp(argv[1],"recall");if(!recall&&strcmp(argv[1],"store"))return 2;
 if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 int result=2,dir=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir<0)goto finish;
 if(pg_agent_init(&agent,pg_mimo_model(&provider),(pg_tools){0}))goto cleanup;
 if(pg_journal_open(&journal,dir,"live.pgj",&agent.session,&agent.commits))goto cleanup;
 if(recall){if(agent.session.count!=2||agent.commits.sequence!=3)goto cleanup;}
 else{const char *system="You are a minimal agent test assistant. Remember facts only from the supplied conversation. Answer concisely.";if(pg_agent_set_system(&agent,(pg_slice){system,strlen(system)}))goto cleanup;}
 const char *prompt=recall?"What was the exact marker? Reply with only that marker.":"Remember the marker PANGU_DISK_8264. Reply only ACK.";
 const char *expected=recall?"PANGU_DISK_8264":"ACK";
 pg_client client=pg_agent_client(&agent);size_t first=agent.session.count;
 if(client.submit(client.context,(pg_slice){prompt,strlen(prompt)}))goto cleanup;
 for(unsigned i=0;i<7000;++i){pg_event event;if(client.poll(client.context,&event)&&event.kind==PG_RUN_FINISHED){int found=0;for(size_t j=first;j<agent.session.count;++j)if(agent.session.messages[j].role==PG_ASSISTANT&&strstr(agent.session.messages[j].text,expected))found=1;
  result=event.outcome!=PG_SUCCESS||!found||agent.commits.sequence!=(recall?5u:3u);
  printf("live_journal phase=%s outcome=%d http=%ld verified=%d sequence=%llu messages=%zu\n",argv[1],event.outcome,provider.http,found,(unsigned long long)agent.commits.sequence,agent.session.count);goto cleanup;}
  struct timespec pause={0,10000000};nanosleep(&pause,0);
 }
 client.cancel(client.context);for(unsigned i=0;i<8;++i){pg_event event;if(client.poll(client.context,&event)&&event.kind==PG_RUN_FINISHED)break;}
cleanup:
 pg_journal_close(&journal);close(dir);
finish:
 curl_global_cleanup();return result;
}
