#define _GNU_SOURCE
#include "../../src/runtime/agent.h"
#include "../../src/runtime/journal.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static pg_agent agent;
static pg_journal journal={.file=-1,.directory=-1};
static int root,scenario;
static unsigned step,answer;
static int policy=1;
static unsigned batch_count;
static pg_model_call batch_calls[8];
static const char *batch_ids[]={"b0","b1","b2","b3","b4","b5","b6","b7"};
static void record(char c){int fd=openat(root,"effects",O_WRONLY|O_CREAT|O_APPEND,0600);assert(fd>=0&&write(fd,&c,1)==1);close(fd);}
static int start(void *ctx,const pg_session *s){(void)ctx;assert(s->count);answer=s->messages[s->count-1].role==PG_TOOL_RESULT;step=0;record('M');return 0;}
static int next(void *ctx,pg_model_event *e){(void)ctx;*e=(pg_model_event){0};if(!step++){e->kind=PG_MODEL_TEXT;e->text=(pg_slice){answer?"done":"checking",answer?4u:8u};}else if(answer)e->kind=PG_MODEL_DONE;else if(batch_count){for(unsigned i=0;i<batch_count;++i)batch_calls[i]=(pg_model_call){{"{}",2},{batch_ids[i],2},{"read",4}};e->kind=PG_MODEL_BATCH;e->batch=(pg_model_batch){batch_calls,batch_count};}else{e->kind=PG_MODEL_CALL;e->text=(pg_slice){"{}",2};e->call_id=(pg_slice){"read-1",6};e->tool_name=(pg_slice){"read",4};}return 0;}
static void cancel(void *ctx){(void)ctx;}
static void close_model(void *ctx){(void)ctx;}
static int allow(void *ctx,pg_slice name,pg_slice args){(void)ctx;assert(name.size==4&&args.size==2);return 1;}
static int execute(void *ctx,pg_slice name,pg_slice args,char *out,size_t capacity,size_t *size){(void)ctx;(void)name;(void)args;assert(capacity>2);record(batch_count?(char)('A'+agent.call_index-2):'T');if(scenario==3||scenario==12||scenario==15)_exit(73);memcpy(out,"ok",2);*size=2;return 0;}
ssize_t __real_pwrite(int,const void *,size_t,off_t);
ssize_t __wrap_pwrite(int fd,const void *p,size_t n,off_t at){ssize_t rc=__real_pwrite(fd,p,n,at);if((scenario==8||scenario==16)&&rc==(ssize_t)n&&n>=96){const unsigned char *b=p;uint32_t version,phase;memcpy(&version,b+8,4);memcpy(&phase,b+n-56+8,4);if(((scenario==8&&version==2)||(scenario==16&&(version==2||version==5)))&&phase==PG_RUN_TERMINAL)_exit(81);}return rc;}
static int replay_safe(void *ctx,pg_slice name){(void)ctx;(void)name;return policy;}
static void open_agent(const char *name){assert(!pg_agent_init(&agent,(pg_model){0,start,next,cancel,close_model},(pg_tools){0,allow,execute,replay_safe,0,0,0}));journal=(pg_journal){.file=-1,.directory=-1};assert(!pg_journal_open(&journal,root,name,&agent.session,&agent.commits));}
static pg_outcome drain(pg_client client){unsigned terminal=0;pg_outcome outcome=PG_ERROR;for(unsigned i=0;i<100;++i){pg_event e;if(client.poll(client.context,&e)&&e.kind==PG_RUN_FINISHED){++terminal;outcome=e.outcome;break;}}assert(terminal==1);pg_event e;assert(!client.poll(client.context,&e));return outcome;}
static void interrupt_run(const char *name,int which){scenario=which;open_agent(name);pg_client client=pg_agent_client(&agent);assert(!client.submit(client.context,(pg_slice){"read",4}));assert(agent.commits.run.id==1);
 if(which==0)_exit(73);
 pg_event e;assert(client.poll(client.context,&e)&&e.kind==PG_RUN_STARTED);assert(!client.poll(client.context,&e));
 if(which==1)_exit(73);
 assert(client.poll(client.context,&e)&&e.kind==PG_TEXT_DELTA);
 if(which==9)_exit(73);
 if(which==10){assert(!pg_agent_suspend(&agent));assert(!agent.request_open&&agent.commits.run.phase==PG_RUN_MODEL);_exit(73);}
 if(which==7){client.cancel(client.context);assert(agent.commits.run.phase==PG_RUN_ABORTING);_exit(73);}
 for(unsigned i=0;i<12;++i)if(client.poll(client.context,&e)&&e.kind==PG_TOOL_STARTED)break;
 assert(e.kind==PG_TOOL_STARTED&&agent.commits.run.phase==(policy?PG_RUN_TOOL:PG_RUN_TOOL_UNSAFE));
 if(which==2)_exit(73);
 if(which==6){client.cancel(client.context);assert(agent.commits.run.phase==PG_RUN_ABORTING);_exit(73);}
 if(which==3){client.poll(client.context,&e);_exit(99);}
 if(which==4){for(unsigned i=0;i<12;++i)if(client.poll(client.context,&e)&&e.kind==PG_TOOL_FINISHED)break;assert(e.kind==PG_TOOL_FINISHED&&agent.commits.run.phase==PG_RUN_MODEL);_exit(73);}
 assert(drain(client)==PG_SUCCESS);_exit(73);
}
static void scenarios(void){for(int which=0;which<=10;++which){char name[32];snprintf(name,sizeof(name),"run-%d",which);unlinkat(root,"effects",0);pid_t child=fork();assert(child>=0);if(!child)interrupt_run(name,which);int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==(which==8?81:73));scenario=-1;open_agent(name);assert(agent.commits.run.id==1);pg_run_phase before=agent.commits.run.phase;unsigned turns=agent.commits.run.turns;assert(!pg_agent_resume(&agent,0));pg_client client=pg_agent_client(&agent);
 if(which==5||which==8){assert(before==PG_RUN_TERMINAL);pg_event e;assert(!client.poll(client.context,&e));}
 else{assert(before!=PG_RUN_TERMINAL);assert(drain(client)==((which==6||which==7)?PG_CANCELLED:PG_SUCCESS));}
 assert(agent.commits.run.phase==PG_RUN_TERMINAL&&agent.commits.run.id==1);
 if(which==6||which==7)assert(agent.commits.run.outcome==PG_CANCELLED);else assert(agent.commits.run.outcome==PG_SUCCESS);
 assert(agent.commits.run.turns>=turns&&agent.commits.run.turns<=3);
 int fd=openat(root,"effects",O_RDONLY);assert(fd>=0);char effects[32];ssize_t size=read(fd,effects,sizeof(effects));assert(size>=0);close(fd);unsigned models=0,tools=0;for(ssize_t i=0;i<size;++i){models+=effects[i]=='M';tools+=effects[i]=='T';}
 assert(tools==(which==6||which==7||(!policy&&which==2)?0u:which==3&&policy?2u:1u));
 assert(models==(which==6||which==7?1u:which==1||which==9||which==10?3u:2u));
 pg_journal_close(&journal);open_agent(name);assert(!pg_agent_resume(&agent,0));client=pg_agent_client(&agent);pg_event e;assert(!client.poll(client.context,&e));pg_journal_close(&journal);assert(!unlinkat(root,name,0));}}
static void replay_policies(void){
 for(int stored=0;stored<=1;++stored)for(int current=0;current<=3;++current)for(int ran=0;ran<=1;++ran){
  char name[40];snprintf(name,sizeof(name),"policy-%d-%d-%d",stored,current,ran);unlinkat(root,"effects",0);
  pid_t child=fork();assert(child>=0);
  if(!child){policy=stored;scenario=12;open_agent(name);pg_client c=pg_agent_client(&agent);assert(!c.submit(c.context,(pg_slice){"work",4}));pg_event e;
   for(unsigned i=0;i<20;++i){if(c.poll(c.context,&e)&&e.kind==PG_TOOL_STARTED){assert(agent.commits.run.phase==(stored?PG_RUN_TOOL:PG_RUN_TOOL_UNSAFE));if(ran)c.poll(c.context,&e);_exit(ran?99:73);}}_exit(98);
  }
  int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==73);
  policy=current;scenario=-1;open_agent(name);if(current==3)agent.tools.replay_safe=0;
  assert(!pg_agent_resume(&agent,0));assert(drain(pg_agent_client(&agent))==PG_SUCCESS);
  unsigned rerun=stored&&current==1;assert(agent.session.count==5);pg_message *result=&agent.session.messages[3];
  assert(result->role==PG_TOOL_RESULT&&result->is_error==!rerun);
  if(!rerun)assert(strstr(result->text,"interrupted")&&strstr(result->text,"partially run"));
  int fd=openat(root,"effects",O_RDONLY);assert(fd>=0);char effects[32];ssize_t n=read(fd,effects,sizeof(effects));assert(n>=0);close(fd);
  unsigned calls=0;for(ssize_t i=0;i<n;++i)calls+=effects[i]=='T';assert(calls==(unsigned)ran+rerun);
  pg_journal_close(&journal);open_agent(name);assert(!pg_agent_resume(&agent,0));pg_event e;pg_client c=pg_agent_client(&agent);assert(!c.poll(c.context,&e));pg_journal_close(&journal);assert(!unlinkat(root,name,0));
 }
 policy=1;
}
static void cancel_recovered_tool(void){
 for(int safe=0;safe<=1;++safe){
  char name[32];snprintf(name,sizeof(name),"recover-cancel-%d",safe);policy=safe;unlinkat(root,"effects",0);
  pid_t child=fork();assert(child>=0);if(!child)interrupt_run(name,2);
  int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==73);
  scenario=-1;open_agent(name);assert(!pg_agent_resume(&agent,0));pg_client c=pg_agent_client(&agent);pg_event e={0};
  for(unsigned n=0;n<10;++n)if(c.poll(c.context,&e)&&e.kind==PG_TOOL_STARTED)break;
  assert(e.kind==PG_TOOL_STARTED);c.cancel(c.context);assert(drain(c)==PG_CANCELLED);
  assert(agent.session.count==4&&agent.session.messages[3].is_error);
  int fd=openat(root,"effects",O_RDONLY);assert(fd>=0);char effects[8];ssize_t n=read(fd,effects,sizeof(effects));assert(n==1&&effects[0]=='M');close(fd);
  pg_journal_close(&journal);assert(!unlinkat(root,name,0));
 }
 policy=1;
}
static void restart_limit(void){open_agent("parked");pg_client client=pg_agent_client(&agent);assert(!client.submit(client.context,(pg_slice){"read",4}));pg_journal_close(&journal);
 for(unsigned i=1;i<=PG_RUN_RESTART_LIMIT;++i){open_agent("parked");assert(!pg_agent_resume(&agent,0));assert(agent.commits.run.restarts==i);pg_journal_close(&journal);}
 open_agent("parked");uint64_t sequence=agent.commits.sequence;assert(pg_agent_resume(&agent,0)==2&&agent.commits.sequence==sequence);assert(client.submit(client.context,(pg_slice){"duplicate",9}));assert(pg_agent_set_system(&agent,(pg_slice){"change",6}));assert(pg_agent_reset_session(&agent));assert(!pg_agent_resume(&agent,1)&&agent.commits.run.restarts==1);assert(drain(pg_agent_client(&agent))==PG_SUCCESS);pg_journal_close(&journal);assert(!unlinkat(root,"parked",0));}
static void batch_child(const char *name,unsigned current,unsigned boundary){
 scenario=-1;open_agent(name);pg_client c=pg_agent_client(&agent);assert(!c.submit(c.context,(pg_slice){"batch",5}));pg_event e;unsigned starts=0;
 for(unsigned polls=0;polls<100;++polls)if(c.poll(c.context,&e)&&e.kind==PG_TOOL_STARTED){if(starts++==current)break;}
 assert(starts==current+1&&agent.session.count==2+batch_count+current&&agent.commits.run.call_index==2+current);
 if(boundary==0)_exit(73);
 if(boundary==1){scenario=15;c.poll(c.context,&e);_exit(99);}
 if(boundary==2){assert(!c.poll(c.context,&e));assert(agent.session.count==3+batch_count+current);_exit(73);}
 c.cancel(c.context);assert(agent.commits.run.phase==PG_RUN_ABORTING);
 if(boundary==3)_exit(73);
 scenario=16;c.poll(c.context,&e);_exit(99);
}
static void batch_process_recovery(void){
 for(unsigned count=2;count<=8;count+=6)for(unsigned stored=0;stored<2;++stored)for(unsigned current=0;current<2;++current)for(unsigned item=0;item<count;++item)for(unsigned boundary=0;boundary<5;++boundary){
 batch_count=count;policy=(int)stored;char name[64];snprintf(name,sizeof(name),"batch-%u-%u-%u-%u-%u",count,stored,current,item,boundary);unlinkat(root,"effects",0);pid_t child=fork();assert(child>=0);if(!child)batch_child(name,item,boundary);int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==(boundary==4?81:73));
 policy=(int)current;scenario=-1;open_agent(name);pg_run_phase phase=agent.commits.run.phase;assert(!pg_agent_resume(&agent,0));pg_client c=pg_agent_client(&agent);if(boundary==4){assert(phase==PG_RUN_TERMINAL);pg_event e;assert(!c.poll(c.context,&e));}else assert(drain(c)==(boundary==3?PG_CANCELLED:PG_SUCCESS));
 assert(agent.commits.run.phase==PG_RUN_TERMINAL&&agent.commits.run.outcome==(boundary>=3?PG_CANCELLED:PG_SUCCESS));assert(agent.session.count==(boundary>=3?2+2*count:3+2*count));
 unsigned skipped=boundary==2?item+1:item;for(unsigned i=0;i<count;++i){pg_message *r=&agent.session.messages[2+count+i];assert(r->role==PG_TOOL_RESULT&&!strcmp(r->call_id,batch_ids[i]));assert(r->is_error==(boundary>=3?i>=item:(!(stored&&current)&&i==skipped)));}
 int fd=openat(root,"effects",O_RDONLY);assert(fd>=0);char effects[64];ssize_t length=read(fd,effects,sizeof(effects));assert(length>=0);close(fd);unsigned actual[8]={0};for(ssize_t i=0;i<length;++i)if(effects[i]>='A'&&effects[i]<='H')++actual[(unsigned)(effects[i]-'A')];for(unsigned i=0;i<count;++i){unsigned expected=boundary>=3?(i<item):1;if(boundary<3&&i==skipped)expected=stored&&current;if(boundary==1&&i==item)++expected;assert(actual[i]==expected);}
 uint64_t sequence=agent.commits.sequence;pg_journal_close(&journal);open_agent(name);assert(agent.commits.sequence==sequence&&!pg_agent_resume(&agent,0));pg_event e;c=pg_agent_client(&agent);assert(!c.poll(c.context,&e));pg_journal_close(&journal);assert(!unlinkat(root,name,0));
 }
 batch_count=0;policy=1;
 puts("PASS: 200 real-journal process-exit batch recoveries: 2/8 calls, every active position, stored/current policy, effect/result/abort boundaries and settled reopen");
}
int main(void){char directory[]="/tmp/pangu-resume-XXXXXX";assert(mkdtemp(directory));root=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(root>=0);scenarios();policy=0;scenarios();policy=1;replay_policies();cancel_recovered_tool();restart_limit();batch_process_recovery();assert(!unlinkat(root,"effects",0));close(root);assert(!rmdir(directory));puts("PASS: cross-process resume at input/request/partial/tool/result/terminal/abort, commit-before-publication crash persisted restart parking and stored/current/missing/invalid replay policies before/after effects");return 0;}
