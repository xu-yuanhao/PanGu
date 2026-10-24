#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo.h"
#include "../../src/runtime/read_tool.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
static pg_agent agent;static pg_mimo provider;static int executions;static int marker_seen;static unsigned deltas;
static pg_tool_registry registry;
static pg_read_tool reader={.directory=-1};static pg_tool_definition original_read;
static int read_execute(void *ctx,pg_slice args,char *out,size_t cap,size_t *size){int rc=original_read.execute(ctx,args,out,cap,size);if(!rc)++executions;return rc;}
static int validate(void *ctx,pg_slice args){(void)ctx;return args.size==2 && !memcmp(args.data,"{}",2);}
static int allow(void *ctx,pg_slice args){(void)ctx;(void)args;return 1;}
static int execute(void *ctx,pg_slice args,char *out,size_t cap,size_t *size){(void)ctx;(void)args;const char *value="PANGU_TOOL_OK_7429";*size=strlen(value);if(*size>=cap)return 1;memcpy(out,value,*size);++executions;return 0;}
int main(int argc,char **argv){int cancel_test=getenv("PG_LIVE_CANCEL")!=0;const char *read_root=getenv("PG_LIVE_READ_ROOT");const char *expected=read_root?"PANGU_READ_OK_9137":"PANGU_TOOL_OK_7429";const char *prompt=argc>1?argv[1]:"Call echo exactly once, then reply with the exact marker returned by echo. This tests a native coding agent implementation.";
 if(read_root)prompt="Call read exactly once with path sample.c. Reply with just the PANGU marker found inside the file. This is a native agent integration test.";
 if(cancel_test)prompt="Write a numbered explanation of C memory ownership in 40 short lines. Do not call tools.";
 if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
 pg_tool_definition definition={"echo","Return a fixed harmless verification marker","{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}",0,validate,allow,execute};
 if(read_root){if(pg_read_tool_open(&reader,read_root))return 2;original_read=pg_read_tool_definition(&reader);definition=original_read;definition.execute=read_execute;}
 if(pg_tool_registry_init(&registry,&definition,1))return 2;
 pg_model model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,&registry);
 pg_agent_init(&agent,model,pg_tool_registry_service(&registry));pg_client c=pg_agent_client(&agent);pg_event e;
 if(c.submit(c.context,(pg_slice){prompt,strlen(prompt)}))return 2;
 for(unsigned i=0;i<7000;++i){if(c.poll(c.context,&e)){
  if(e.kind==PG_TEXT_DELTA){++deltas;printf("text: %.*s\n",(int)e.text.size,e.text.data);if(cancel_test)c.cancel(c.context);}
  if(e.kind==PG_TOOL_STARTED)puts("tool_started");if(e.kind==PG_TOOL_FINISHED)puts("tool_finished");
  if(e.kind==PG_RUN_FINISHED){for(size_t j=0;j<agent.session.count;++j)if(agent.session.messages[j].role==PG_ASSISTANT && strstr(agent.session.messages[j].text,expected))marker_seen=1;printf("terminal=%d http=%ld tool_executions=%d\n",e.outcome,provider.http,executions);printf("deltas=%u marker_verified=%d\n",deltas,marker_seen);pg_read_tool_close(&reader);curl_global_cleanup();return cancel_test?(e.outcome==PG_CANCELLED && deltas>0 && executions==0 && !provider.easy && !provider.multi?0:1):(e.outcome==PG_SUCCESS && (argc>1 || (executions==1 && marker_seen))?0:1);}
 }struct timespec t={0,10000000};nanosleep(&t,0);}
 c.cancel(c.context);while(!c.poll(c.context,&e)||e.kind!=PG_RUN_FINISHED){}pg_read_tool_close(&reader);curl_global_cleanup();return 2;}
