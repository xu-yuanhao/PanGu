#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo_layout.h"
#include "../../src/runtime/read_tool.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static pg_mimo provider;
static pg_session session;
static pg_tool_registry registry;
int main(void){
 assert(getenv("MIMO_API_KEY")&&*getenv("MIMO_API_KEY"));
 assert(!curl_global_init(CURL_GLOBAL_DEFAULT));
 char path[]="/tmp/pangu-live-model-batch-XXXXXX";assert(mkdtemp(path));
 pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,path));
 const char *names[]={"first.txt","second.txt"};
 for(unsigned i=0;i<2;++i){int fd=openat(reader.directory,names[i],O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);assert(fd>=0&&write(fd,"fixture\n",8)==8&&!close(fd));}
 pg_tool_definition d=pg_read_tool_definition(&reader);assert(!pg_tool_registry_init(&registry,&d,1));
 pg_model model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,&registry);
 const char *system="This is a protocol verification. Emit two independent read tool calls together in one assistant response; do not serialize them into separate responses. No text answer is needed.";
 session.system_size=strlen(system);memcpy(session.system_prompt,system,session.system_size+1);
 const char *input="Call read for first.txt and read for second.txt in the same assistant response, using two tool calls.";
 session.count=1;session.messages[0].role=PG_USER;session.messages[0].size=strlen(input);memcpy(session.messages[0].text,input,strlen(input)+1);
 int result=1,started=!model.start(model.context,&session);unsigned seen=0;
 if(started)for(unsigned poll=0;poll<12000;++poll){pg_model_event e;int rc=model.next(model.context,&e);if(rc){fprintf(stderr,"real Model batch request failed: http=%ld failed=%d\n",provider.http,provider.failed);break;}if(e.kind==PG_MODEL_BATCH){assert(e.batch.count==2);unsigned mask=0;for(size_t i=0;i<e.batch.count;++i){const pg_model_call *call=&e.batch.calls[i];assert(call->tool_name.size==4&&!memcmp(call->tool_name.data,"read",4)&&call->call_id.size&&call->arguments.size<4096);struct json_object *args=json_tokener_parse(call->arguments.data),*v=0;assert(args&&json_object_object_get_ex(args,"path",&v));const char *name=json_object_get_string(v);if(!strcmp(name,names[0]))mask|=1;else if(!strcmp(name,names[1]))mask|=2;else assert(0);json_object_put(args);}assert(mask==3);seen=(unsigned)e.batch.count;result=0;break;}if(e.kind==PG_MODEL_CALL||e.kind==PG_MODEL_DONE){fprintf(stderr,"real response did not contain requested batch: kind=%d http=%ld\n",e.kind,provider.http);break;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 model.close(model.context);assert(!provider.easy&&!provider.multi&&!provider.headers&&!provider.request);
 for(unsigned i=0;i<2;++i)assert(!unlinkat(reader.directory,names[i],0));pg_read_tool_close(&reader);assert(!rmdir(path));curl_global_cleanup();
 if(!result)printf("PASS: actual MiMo Model decoded %u complete read calls in one response; no tool execution\n",seen);
 return result;
}
