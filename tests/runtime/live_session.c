#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static pg_agent agent;static pg_mimo provider;
static int run(pg_client client,const char *prompt,const char *expected,int cancel_first){
 size_t before=agent.session.count;unsigned deltas=0;pg_event event;
 if(client.submit(client.context,(pg_slice){prompt,strlen(prompt)}))return 1;
 for(unsigned i=0;i<7000;++i){if(client.poll(client.context,&event)){
  if(event.kind==PG_TEXT_DELTA){++deltas;if(cancel_first)client.cancel(client.context);}
  if(event.kind==PG_RUN_FINISHED){int found=!expected;for(size_t j=before;j<agent.session.count;++j)if(agent.session.messages[j].role==PG_ASSISTANT && expected && strstr(agent.session.messages[j].text,expected))found=1;
   printf("session_run outcome=%d http=%ld deltas=%u context_verified=%d messages=%zu\n",event.outcome,provider.http,deltas,found,agent.session.count);
   return cancel_first?!(event.outcome==PG_CANCELLED&&deltas&&!provider.easy&&!provider.multi):!(event.outcome==PG_SUCCESS&&found);
  }
 }struct timespec pause={0,10000000};nanosleep(&pause,0);}
 client.cancel(client.context);for(unsigned i=0;i<4;++i)if(client.poll(client.context,&event)&&event.kind==PG_RUN_FINISHED)break;return 1;
}
int main(void){if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;pg_model model=pg_mimo_model(&provider);if(pg_agent_init(&agent,model,(pg_tools){0}))return 2;
 const char *system="You are a C coding agent test assistant. Answer very briefly. Remember user facts within the supplied conversation only. If a requested marker is absent from the conversation, answer exactly NO_CONTEXT. Never invent a marker.";
 if(pg_agent_set_system(&agent,(pg_slice){system,strlen(system)}))return 2;pg_client client=pg_agent_client(&agent);
 int rc=run(client,"Remember the marker PANGU_SESSION_6382. Reply ACK.","ACK",0);
 if(!rc)rc=run(client,"What was the marker? Reply with only the exact marker.","PANGU_SESSION_6382",0);
 if(!rc)rc=pg_agent_reset_session(&agent);
 if(!rc)rc=run(client,"What was the marker?","NO_CONTEXT",0);
 if(!rc)rc=run(client,"List 40 numbered facts about C pointers, one per line.",0,1);
 if(!rc)rc=run(client,"Reply exactly READY.","READY",0);
 printf("live_session_exit=%d\n",rc);curl_global_cleanup();return rc;}
