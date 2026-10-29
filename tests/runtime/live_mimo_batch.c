#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo_layout.h"
#include "../../src/runtime/read_tool.h"
#include "../../src/runtime/write_tool.h"
#include "../../src/runtime/edit_tool.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static pg_mimo provider;
static pg_session session;
static pg_agent agent;
static pg_tool_registry registry;
static int snapshot_edit(void){
 assert(getenv("MIMO_API_KEY")&&*getenv("MIMO_API_KEY"));assert(!curl_global_init(CURL_GLOBAL_DEFAULT));
 char path[]="/tmp/pangu-live-snapshot-XXXXXX";assert(mkdtemp(path));pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,path));pg_write_tool writer={reader.directory};pg_tool_registry files;
 pg_tool_definition file_defs[]={pg_read_snapshot_definition(&reader),pg_write_tool_definition(&writer)};assert(!pg_tool_registry_init(&files,file_defs,2));pg_edit_tool editor={pg_tool_registry_service(&files)};
 pg_tool_definition defs[]={pg_read_tool_definition(&reader),pg_write_tool_definition(&writer),pg_edit_tool_definition(&editor)};assert(!pg_tool_registry_init(&registry,defs,3));
 char original[2048],expected[2048];memset(original,'\n',2000);memcpy(original+2000,"UNIQUE_OLD\n",11);memcpy(expected,original,2011);memcpy(expected+2000,"UNIQUE_NEW",10);
 int fd=openat(reader.directory,"many-lines.txt",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);assert(fd>=0&&write(fd,original,2011)==2011&&!close(fd));
 pg_model model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,&registry);assert(!pg_agent_init(&agent,model,pg_tool_registry_service(&registry)));
 const char *input="Use edit exactly once on many-lines.txt to replace UNIQUE_OLD with UNIQUE_NEW. Preserve every other byte. Do not use write. Then use read with offset 2001 and limit 1 to verify the line. Report briefly.";
 pg_client client=pg_agent_client(&agent);assert(!client.submit(client.context,(pg_slice){input,strlen(input)}));int result=1;
 for(unsigned poll=0;poll<12000;++poll){pg_event e;if(client.poll(client.context,&e)&&e.kind==PG_RUN_FINISHED){assert(e.outcome==PG_SUCCESS);result=0;break;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 assert(!result);unsigned edits=0,reads=0;for(size_t i=0;i<agent.session.count;++i){pg_message *m=&agent.session.messages[i];if(m->role==PG_TOOL_CALL){if(!strcmp(m->tool_name,"edit"))++edits;else{assert(!strcmp(m->tool_name,"read"));++reads;}}if(m->role==PG_TOOL_RESULT)assert(!m->is_error);}assert(edits==1&&reads>=1);
 const char *edit_id=0;for(size_t i=0;i<agent.session.count;++i)if(agent.session.messages[i].role==PG_TOOL_CALL&&!strcmp(agent.session.messages[i].tool_name,"edit"))edit_id=agent.session.messages[i].call_id;
 assert(edit_id);unsigned summaries=0;for(size_t i=0;i<agent.session.count;++i)if(agent.session.messages[i].role==PG_TOOL_RESULT&&!strcmp(agent.session.messages[i].call_id,edit_id)){assert(!strcmp(agent.session.messages[i].text,"Successfully replaced 1 block(s); first changed line 2001"));++summaries;}assert(summaries==1);
 fd=openat(reader.directory,"many-lines.txt",O_RDONLY|O_CLOEXEC);assert(fd>=0);char actual[2048];assert(read(fd,actual,sizeof(actual))==2011&&!memcmp(actual,expected,2011)&&!close(fd));assert(!pg_agent_suspend(&agent));assert(!provider.easy&&!provider.multi&&!provider.headers&&!provider.request);
 assert(!unlinkat(reader.directory,"many-lines.txt",0));pg_read_tool_close(&reader);assert(!rmdir(path));curl_global_cleanup();puts("PASS: actual MiMo Agent edited beyond the display page through a complete snapshot, preserving 2000 leading newlines and verifying ranged read");return 0;
}
static int capacity_write(void){
 assert(getenv("MIMO_API_KEY")&&*getenv("MIMO_API_KEY"));assert(!curl_global_init(CURL_GLOBAL_DEFAULT));
 char path[]="/tmp/pangu-live-capacity-XXXXXX";assert(mkdtemp(path));pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,path));pg_write_tool writer={reader.directory};pg_tool_definition definition=pg_write_tool_definition(&writer);assert(!pg_tool_registry_init(&registry,&definition,1));
 for(unsigned full=0;full<2;++full){
  pg_model model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,&registry);assert(!pg_agent_init(&agent,model,pg_tool_registry_service(&registry)));
  unsigned prior=PG_MESSAGE_LIMIT-5+full;for(unsigned i=0;i<prior;++i){pg_message *m=&agent.session.messages[i];m->role=i%2?PG_ASSISTANT:PG_USER;const char *text=i%2?"Acknowledged.":"Earlier turn.";m->size=strlen(text);memcpy(m->text,text,m->size+1);}agent.session.count=prior;
  const char *input="Use write exactly once to create capacity.txt containing exactly CAPACITY_OK. Then report done briefly. Do not call any other tool.";pg_client client=pg_agent_client(&agent);assert(!client.submit(client.context,(pg_slice){input,strlen(input)}));unsigned starts=0,ends=0;int settled=0;
  for(unsigned poll=0;poll<12000;++poll){pg_event e;if(client.poll(client.context,&e)){if(e.kind==PG_TOOL_STARTED)++starts;if(e.kind==PG_TOOL_FINISHED)++ends;if(e.kind==PG_RUN_FINISHED){assert(e.outcome==(full?PG_ERROR:PG_SUCCESS));if(full)assert(e.text.size==strlen("session has no room for tool results and follow-up reply")&&!memcmp(e.text.data,"session has no room for tool results and follow-up reply",e.text.size));settled=1;break;}}struct timespec pause={0,10000000};nanosleep(&pause,0);}
  assert(settled&&starts==(full?0:1)&&ends==starts&&agent.session.count==(full?prior+1:PG_MESSAGE_LIMIT));
  int fd=openat(reader.directory,"capacity.txt",O_RDONLY|O_CLOEXEC);if(full)assert(fd<0);else{char out[32];assert(fd>=0&&read(fd,out,sizeof(out))==11&&!memcmp(out,"CAPACITY_OK",11)&&!close(fd));assert(!unlinkat(reader.directory,"capacity.txt",0));}
  assert(!pg_agent_suspend(&agent)&&!provider.easy&&!provider.multi&&!provider.headers&&!provider.request);
 }
 pg_read_tool_close(&reader);assert(!rmdir(path));curl_global_cleanup();puts("PASS: actual MiMo write at the exact session boundary retained a follow-up reply; one slot short rejected before tool start and created no file");return 0;
}

int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--snapshot"))return snapshot_edit();
 if(argc==2&&!strcmp(argv[1],"--capacity"))return capacity_write();
 int execute_batch=argc==2&&!strcmp(argv[1],"--execute");
 assert(getenv("MIMO_API_KEY")&&*getenv("MIMO_API_KEY"));
 assert(!curl_global_init(CURL_GLOBAL_DEFAULT));
 char path[]="/tmp/pangu-live-model-batch-XXXXXX";assert(mkdtemp(path));
 pg_read_tool reader={.directory=-1};assert(!pg_read_tool_open(&reader,path));
 const char *names[]={"first.txt","second.txt"};
 for(unsigned i=0;i<2;++i){int fd=openat(reader.directory,names[i],O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);assert(fd>=0&&write(fd,"fixture\n",8)==8&&!close(fd));}
 pg_tool_definition d=pg_read_tool_definition(&reader);assert(!pg_tool_registry_init(&registry,&d,1));
 pg_model model=pg_mimo_model(&provider);pg_mimo_bind_tools(&provider,&registry);
 const char *system="This is a protocol verification. Emit two independent read tool calls together in one assistant response; do not serialize them into separate responses. After the tool results, respond briefly with done.";
 session.system_size=strlen(system);memcpy(session.system_prompt,system,session.system_size+1);
 const char *input="Call read for first.txt and read for second.txt in the same assistant response, using two tool calls.";
 session.count=1;session.messages[0].role=PG_USER;session.messages[0].size=strlen(input);memcpy(session.messages[0].text,input,strlen(input)+1);
 int result=1;unsigned seen=0;
 if(execute_batch){assert(!pg_agent_init(&agent,model,pg_tool_registry_service(&registry)));assert(!pg_agent_set_system(&agent,(pg_slice){system,strlen(system)}));pg_client client=pg_agent_client(&agent);assert(!client.submit(client.context,(pg_slice){input,strlen(input)}));unsigned begins=0;
 for(unsigned poll=0;poll<12000;++poll){pg_event e;if(client.poll(client.context,&e)){if(e.kind==PG_TOOL_STARTED){++begins;if(begins==1){assert(agent.session.count==4&&agent.session.messages[2].role==PG_TOOL_CALL&&agent.session.messages[3].role==PG_TOOL_CALL);}}if(e.kind==PG_TOOL_FINISHED){assert(e.outcome==PG_SUCCESS&&e.text.size>=8);++seen;}if(e.kind==PG_RUN_FINISHED){assert(e.outcome==PG_SUCCESS&&begins==2&&seen==2);result=0;break;}}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 }else{
 int started=!model.start(model.context,&session);
 if(started)for(unsigned poll=0;poll<12000;++poll){pg_model_event e;int rc=model.next(model.context,&e);if(rc){fprintf(stderr,"real Model batch request failed: http=%ld failed=%d\n",provider.http,provider.failed);break;}if(e.kind==PG_MODEL_BATCH){assert(e.batch.count==2);unsigned mask=0;for(size_t i=0;i<e.batch.count;++i){const pg_model_call *call=&e.batch.calls[i];assert(call->tool_name.size==4&&!memcmp(call->tool_name.data,"read",4)&&call->call_id.size&&call->arguments.size<PG_TL_ARGUMENT_LIMIT);struct json_object *args=json_tokener_parse(call->arguments.data),*v=0;assert(args&&json_object_object_get_ex(args,"path",&v));const char *name=json_object_get_string(v);if(!strcmp(name,names[0]))mask|=1;else if(!strcmp(name,names[1]))mask|=2;else assert(0);json_object_put(args);}assert(mask==3);seen=(unsigned)e.batch.count;result=0;break;}if(e.kind==PG_MODEL_CALL||e.kind==PG_MODEL_DONE){fprintf(stderr,"real response did not contain requested batch: kind=%d http=%ld\n",e.kind,provider.http);break;}struct timespec pause={0,10000000};nanosleep(&pause,0);}
 }
 model.close(model.context);assert(!provider.easy&&!provider.multi&&!provider.headers&&!provider.request);
 for(unsigned i=0;i<2;++i)assert(!unlinkat(reader.directory,names[i],0));pg_read_tool_close(&reader);assert(!rmdir(path));curl_global_cleanup();
 if(!result)printf("PASS: actual MiMo %s %u complete read calls in one response\n",execute_batch?"Agent executed":"Model decoded",seen);
 return result;
}
