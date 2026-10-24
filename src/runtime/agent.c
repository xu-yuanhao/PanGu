#include "agent.h"
/* No OS, allocator, provider-specific or TUI dependencies. */
enum { IDLE, BEGIN, START, STREAM, TOOL, TOOL_END, FINISH };
static pg_slice slice(const char *p, size_t n) { pg_slice s={p,n}; return s; }
static size_t length(const char *p) { size_t n=0; while(n<PG_ID_LIMIT && p[n]) ++n; return n; }
static int valid(pg_slice s,size_t cap) { return s.size<cap && (!s.size || s.data); }
static int identifier(pg_slice s) {
    if(!s.size || !valid(s,PG_ID_LIMIT))return 0;
    for(size_t i=0;i<s.size;++i)if((unsigned char)s.data[i]<32 || s.data[i]==127)return 0;
    return 1;
}
static void copy(char *d,pg_slice s) { size_t i; for(i=0;i<s.size;++i)d[i]=s.data[i]; d[s.size]=0; }
static pg_message *append(pg_agent *a,pg_role role,pg_slice s) {
    pg_message *m;
    if(a->session.count==PG_MESSAGE_LIMIT || !valid(s,PG_TEXT_LIMIT))return 0;
    m=&a->session.messages[a->session.count++];
    *m=(pg_message){0}; m->role=role;m->size=s.size;copy(m->text,s);return m;
}
static void close_request(pg_agent *a) {
    if(a->request_open){a->request_open=0;a->model.close(a->model.context);}
}
static void finish(pg_agent *a,pg_outcome o) {close_request(a);a->outcome=o;a->state=FINISH;}
static void fail(pg_agent *a,const char *message) {
    size_t n=0;while(message[n] && n<sizeof(a->error_text)-1)++n;
    copy(a->error_text,slice(message,n));finish(a,PG_ERROR);
}
static int submit(void *ctx,pg_slice input) {
    pg_agent *a=ctx;
    /* Reserve user, assistant, call and result before accepting. */
    if(a->state!=IDLE || !input.size || !valid(input,PG_TEXT_LIMIT) || a->session.count>PG_MESSAGE_LIMIT-4)return 1;
    if(!append(a,PG_USER,input))return 1;
    a->cancelled=0;a->turns=0;a->error_text[0]=0;a->state=BEGIN;return 0;
}
static void cancel(void *ctx) {pg_agent *a=ctx;if(a->state!=IDLE && a->state!=FINISH)a->cancelled=1;}
static int poll(void *ctx,pg_event *out) {
    pg_agent *a=ctx;pg_model_event e={0};pg_message *m;size_t n=0;int rc;
    if(!out)return 0;
    *out=(pg_event){0};
    if(a->state==IDLE)return 0;
    if(a->cancelled && a->state!=BEGIN && a->state!=FINISH){
        if(a->request_open)a->model.cancel(a->model.context);
        if(a->state==TOOL){
            pg_message *call=&a->session.messages[a->call_index];
            pg_message *result=append(a,PG_TOOL_RESULT,slice("cancelled",9));
            if(result){
                copy(result->call_id,slice(call->call_id,length(call->call_id)));result->is_error=1;
                finish(a,PG_CANCELLED);out->kind=PG_TOOL_FINISHED;out->outcome=PG_CANCELLED;
                out->text=slice(result->text,result->size);out->call_id=slice(result->call_id,length(result->call_id));
                out->tool_name=slice(call->tool_name,length(call->tool_name));return 1;
            }
        }
        finish(a,PG_CANCELLED);
    }
    switch(a->state){
    case BEGIN:out->kind=PG_RUN_STARTED;a->state=START;return 1;
    case START:
        if(++a->turns>PG_TURN_LIMIT){fail(a,"model turn limit reached");return 0;}
        /* start failure is also closed, including partial request acquisition. */
        a->request_open=1;
        if(a->model.start(a->model.context,&a->session)){fail(a,"model request initialization failed");return 0;}
        a->assistant_index=a->session.count;
        if(!append(a,PG_ASSISTANT,slice("",0))){fail(a,"session message capacity exhausted");return 0;}
        a->state=STREAM;return 0;
    case STREAM:
        if(a->model.next(a->model.context,&e)){fail(a,"model transport or protocol failed");return 0;}
        if(e.kind==PG_MODEL_WAIT)return 0;
        if(e.kind==PG_MODEL_DONE){finish(a,PG_SUCCESS);return 0;}
        if(e.kind==PG_MODEL_ERROR){fail(a,"model reported an error");return 0;}
        if(e.kind==PG_MODEL_TEXT){
            m=&a->session.messages[a->assistant_index];
            if(!valid(e.text,PG_TEXT_LIMIT) || e.text.size>=PG_TEXT_LIMIT-m->size){fail(a,"assistant text limit exceeded");return 0;}
            copy(a->event_text,e.text);copy(m->text+m->size,e.text);m->size+=e.text.size;
            out->kind=PG_TEXT_DELTA;out->text=slice(a->event_text,e.text.size);return 1;
        }
        if(e.kind!=PG_MODEL_CALL || !identifier(e.call_id) || !identifier(e.tool_name)) {fail(a,"invalid model event or tool identifier");return 0;}
        /* CALL commits the assistant turn in this initial provider contract. */
        if(a->session.count>PG_MESSAGE_LIMIT-2){fail(a,"session has no room for tool result");return 0;}
        a->call_index=a->session.count;
        m=append(a,PG_TOOL_CALL,e.text);
        if(!m){fail(a,"tool arguments or session capacity exceeded");return 0;}
        copy(m->call_id,e.call_id);copy(m->tool_name,e.tool_name);close_request(a);
        out->kind=PG_TOOL_STARTED;out->call_id=slice(m->call_id,e.call_id.size);out->tool_name=slice(m->tool_name,e.tool_name.size);
        a->state=TOOL;return 1;
    case TOOL:
        m=&a->session.messages[a->call_index];
        if(!a->tools.authorize || !a->tools.execute || !a->tools.authorize(a->tools.context,slice(m->tool_name,length(m->tool_name)),slice(m->text,m->size))){
            copy(a->event_text,slice("tool unavailable or denied",26));n=26;rc=1;
        }else{
            rc=a->tools.execute(a->tools.context,slice(m->tool_name,length(m->tool_name)),slice(m->text,m->size),a->event_text,PG_TEXT_LIMIT,&n);
            if(n>=PG_TEXT_LIMIT){copy(a->event_text,slice("tool output limit exceeded",26));n=26;rc=1;}
        }
        {pg_message *result=append(a,PG_TOOL_RESULT,slice(a->event_text,n));
         if(!result){fail(a,"session has no room for tool result");return 0;}
         copy(result->call_id,slice(m->call_id,length(m->call_id)));result->is_error=rc!=0;}
        a->state=TOOL_END;return 0;
    case TOOL_END:
        m=&a->session.messages[a->session.count-1];
        out->kind=PG_TOOL_FINISHED;out->outcome=m->is_error?PG_ERROR:PG_SUCCESS;
        out->text=slice(m->text,m->size);out->call_id=slice(m->call_id,length(m->call_id));
        {pg_message *call=&a->session.messages[a->call_index];out->tool_name=slice(call->tool_name,length(call->tool_name));}
        a->state=START;return 1;
    case FINISH:out->kind=PG_RUN_FINISHED;out->outcome=a->outcome;
        if(a->outcome==PG_ERROR){size_t size=0;while(a->error_text[size])++size;out->text=slice(a->error_text,size);}
        a->state=IDLE;return 1;
    default:fail(a,"invalid runtime state");return 0;
    }
}
int pg_agent_init(pg_agent *a,pg_model model,pg_tools tools) {
    if(!a || !model.start || !model.next || !model.cancel || !model.close)return 1;
    *a=(pg_agent){0};a->model=model;a->tools=tools;return 0;
}
pg_client pg_agent_client(pg_agent *a) {pg_client c={a,submit,cancel,poll};return c;}

int pg_agent_set_system(pg_agent *a,pg_slice prompt) {
    if(!a || a->state!=IDLE || !valid(prompt,PG_TEXT_LIMIT))return 1;
    char temporary[PG_TEXT_LIMIT];copy(temporary,prompt);
    copy(a->session.system_prompt,slice(temporary,prompt.size));a->session.system_size=prompt.size;return 0;
}
int pg_agent_reset_session(pg_agent *a) {
    if(!a || a->state!=IDLE)return 1;
    for(size_t i=0;i<a->session.count;++i)a->session.messages[i]=(pg_message){0};
    a->session.count=0;return 0;
}
