#include "../../src/runtime/agent.h"
#include "../../src/runtime/session_layout.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(pg_session)==PG_S_BYTES,"session layout");
_Static_assert(sizeof(pg_session_control)==PG_SC_BYTES,"control layout");
_Static_assert(sizeof(pg_session_change)==PG_CHANGE_BYTES,"change layout");
_Static_assert(offsetof(pg_session_control,sequence)==PG_SC_SEQ,"sequence");
_Static_assert(offsetof(pg_session_control,busy)==PG_SC_BUSY,"busy");
_Static_assert(offsetof(pg_session_control,failed)==PG_SC_FAILED,"failed");
typedef struct {
 pg_session saved;
 pg_session *visible;
 pg_session_control *control;
 unsigned calls,fail_at,mode,reenter;
 pg_message *source;
 uint64_t sequence;
} store;
static pg_store_result write_batch(void *ctx,const pg_session_change *change,uint64_t previous,uint64_t *next){
 store *s=ctx;++s->calls;assert(s->control->busy==1);
 assert(previous==s->sequence&&s->control->sequence==previous);
 assert(!memcmp(s->visible,&s->saved,sizeof(s->saved)));
 if(s->source){assert(change->messages!=s->source);memset(s->source,0,2*sizeof(pg_message));s->source=0;}
 if(s->reenter){s->reenter=0;assert(pg_session_commit(s->visible,s->control,change));}
 if(s->calls==s->fail_at)return s->mode==2?PG_STORE_UNCERTAIN:PG_STORE_REJECTED;
 if(s->mode==3){*next=previous+2;return PG_STORE_OK;}
 if(change->kind==PG_SESSION_APPEND){assert(change->count<=2);memcpy(s->saved.messages+s->saved.count,change->messages,change->count*sizeof(pg_message));s->saved.count+=change->count;}
 else if(change->kind==PG_SESSION_SYSTEM){memcpy(s->saved.system_prompt,change->text.data,change->text.size);s->saved.system_prompt[change->text.size]=0;s->saved.system_size=change->text.size;}
 else{memset(s->saved.messages,0,s->saved.count*sizeof(pg_message));s->saved.count=0;}
 *next=++s->sequence;return PG_STORE_OK;
}
static pg_session view;
static pg_session_control control;
static store storage;
static pg_message batch[2];
static pg_session_change append(void){return (pg_session_change){.kind=PG_SESSION_APPEND,.messages=batch,.count=2};}
static void fresh(void){memset(&view,0,sizeof(view));memset(&control,0,sizeof(control));memset(&storage,0,sizeof(storage));storage.visible=&view;storage.control=&control;control.storage=(pg_storage){&storage,write_batch};memset(batch,0,sizeof(batch));batch[0].role=PG_ASSISTANT;memcpy(batch[0].text,"answer",7);batch[0].size=6;batch[1].role=PG_TOOL_CALL;memcpy(batch[1].text,"{}",3);batch[1].size=2;memcpy(batch[1].call_id,"id-1",5);memcpy(batch[1].tool_name,"read",5);}
static void transactions(void){fresh();pg_session_change c=append();storage.reenter=1;assert(!pg_session_commit(&view,&control,&c));assert(storage.calls==1&&view.count==2&&control.sequence==1&&!control.busy);assert(!memcmp(&view,&storage.saved,sizeof(view)));
 pg_message original=view.messages[0];storage.fail_at=2;assert(pg_session_commit(&view,&control,&c));assert(view.count==2&&control.sequence==1&&!control.failed&&!memcmp(&original,&view.messages[0],sizeof(original)));
 storage.fail_at=0;assert(!pg_session_commit(&view,&control,&c));assert(view.count==4&&control.sequence==2);
 c=(pg_session_change){.kind=PG_SESSION_SYSTEM,.text={"system",6}};storage.fail_at=4;assert(pg_session_commit(&view,&control,&c));assert(view.system_size==0&&control.sequence==2);
 storage.fail_at=0;assert(!pg_session_commit(&view,&control,&c));assert(view.system_size==6);
 c.text=(pg_slice){view.system_prompt+1,5};assert(!pg_session_commit(&view,&control,&c));assert(!strcmp(view.system_prompt,"ystem"));
 c=(pg_session_change){.kind=PG_SESSION_RESET};storage.fail_at=storage.calls+1;assert(pg_session_commit(&view,&control,&c));assert(view.count==4);
 storage.fail_at=0;assert(!pg_session_commit(&view,&control,&c));assert(!view.count&&view.system_size==5);assert(!memcmp(&view,&storage.saved,sizeof(view)));
 fresh();c=append();batch[1].role=(pg_role)99;assert(pg_session_commit(&view,&control,&c));assert(!storage.calls&&!view.count);
 batch[1].role=PG_TOOL_CALL;memset(batch[1].call_id,'x',PG_ID_LIMIT);assert(pg_session_commit(&view,&control,&c));assert(!storage.calls);
 fresh();c=append();view.count=63;assert(pg_session_commit(&view,&control,&c));assert(!storage.calls&&view.count==63);
 fresh();c=append();control.sequence=UINT64_MAX;assert(pg_session_commit(&view,&control,&c));assert(!storage.calls);
 fresh();c=append();storage.mode=3;assert(pg_session_commit(&view,&control,&c));assert(control.failed&&view.count==0);assert(pg_session_commit(&view,&control,&c)&&storage.calls==1);
 fresh();c=append();storage.mode=2;storage.fail_at=1;assert(pg_session_commit(&view,&control,&c));assert(control.failed&&!view.count);assert(pg_session_commit(&view,&control,&c)&&storage.calls==1);
 fresh();c=append();storage.source=batch;assert(!pg_session_commit(&view,&control,&c));assert(view.messages[0].size==6&&view.messages[1].role==PG_TOOL_CALL);
 fresh();control.storage=(pg_storage){0,pg_session_volatile_commit};c=append();assert(!pg_session_commit(&view,&control,&c)&&view.count==2&&control.sequence==1);
}
typedef struct {unsigned starts,step,closes,executed;} model_state;
static int start(void *ctx,const pg_session *session){model_state *m=ctx;++m->starts;m->step=0;assert(session->count);return 0;}
static int next(void *ctx,pg_model_event *e){model_state *m=ctx;*e=(pg_model_event){0};if(!m->step++){e->kind=PG_MODEL_TEXT;e->text=(pg_slice){"text",4};}else if(m->starts==1){e->kind=PG_MODEL_CALL;e->text=(pg_slice){"{}",2};e->call_id=(pg_slice){"id-1",4};e->tool_name=(pg_slice){"read",4};}else e->kind=PG_MODEL_DONE;return 0;}
static void cancel(void *ctx){(void)ctx;}
static void close_model(void *ctx){++((model_state*)ctx)->closes;}
static int authorize(void *ctx,pg_slice name,pg_slice args){(void)ctx;(void)name;(void)args;return 1;}
static int execute(void *ctx,pg_slice name,pg_slice args,char *out,size_t cap,size_t *size){(void)name;(void)args;assert(cap>2);++((model_state*)ctx)->executed;memcpy(out,"ok",2);*size=2;return 0;}
static pg_agent agent;
static void runtime(void){for(unsigned fail=0;fail<=4;++fail){model_state m={0};assert(!pg_agent_init(&agent,(pg_model){&m,start,next,cancel,close_model},(pg_tools){&m,authorize,execute}));memset(&storage,0,sizeof(storage));storage.visible=&agent.session;storage.control=&agent.commits;storage.fail_at=fail;assert(!pg_agent_bind_storage(&agent,(pg_storage){&storage,write_batch}));pg_client client=pg_agent_client(&agent);
 if(fail==1){assert(client.submit(client.context,(pg_slice){"input",5}));pg_event e;assert(!client.poll(client.context,&e));assert(!agent.session.count&&!m.starts);storage.fail_at=0;}
 assert(!client.submit(client.context,(pg_slice){"input",5}));assert(pg_agent_bind_storage(&agent,(pg_storage){&storage,write_batch}));unsigned begins=0,results=0,finished=0;
 for(unsigned i=0;i<40;++i){pg_event e;if(!client.poll(client.context,&e))continue;
  if(e.kind==PG_TOOL_STARTED){++begins;assert(agent.session.count==3&&storage.saved.count==3);}
  if(e.kind==PG_TOOL_FINISHED){++results;assert(agent.session.count==4&&storage.saved.count==4);}
  if(e.kind==PG_RUN_FINISHED){++finished;assert(e.outcome==((fail>=2)?PG_ERROR:PG_SUCCESS));break;}
 }
 assert(finished==1&&!memcmp(&storage.saved,&agent.session,sizeof(storage.saved)));
 if(fail==2)assert(!begins&&!results&&!m.executed&&agent.session.count==1);
 if(fail==3)assert(begins==1&&!results&&m.executed==1&&agent.session.count==3);
 if(fail==4)assert(begins==1&&results==1&&agent.session.count==4);
 if(fail>=2){assert(agent.commits.failed);assert(client.submit(client.context,(pg_slice){"retry",5}));}
 else assert(begins==1&&results==1&&m.closes==2);
 }}
static void cancelled_result_failure(void){model_state m={0};assert(!pg_agent_init(&agent,(pg_model){&m,start,next,cancel,close_model},(pg_tools){&m,authorize,execute}));memset(&storage,0,sizeof(storage));storage.visible=&agent.session;storage.control=&agent.commits;storage.fail_at=3;assert(!pg_agent_bind_storage(&agent,(pg_storage){&storage,write_batch}));pg_client client=pg_agent_client(&agent);assert(!client.submit(client.context,(pg_slice){"cancel",6}));pg_event e={0};
 for(unsigned i=0;i<16;++i)if(client.poll(client.context,&e)&&e.kind==PG_TOOL_STARTED)break;
 assert(e.kind==PG_TOOL_STARTED);client.cancel(client.context);unsigned finished=0;
 for(unsigned i=0;i<8;++i)if(client.poll(client.context,&e)){assert(e.kind!=PG_TOOL_FINISHED);if(e.kind==PG_RUN_FINISHED){assert(e.outcome==PG_ERROR);finished=1;break;}}
 assert(finished&&agent.commits.failed&&!m.executed&&agent.session.count==3);
}
int main(void){transactions();runtime();cancelled_result_failure();puts("PASS: detached atomic session batches, storage replacement, reject/uncertain/sequence/reentry, and runtime publication order");return 0;}
