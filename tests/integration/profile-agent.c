/* Agent app/storage consumer for the same cumulative-stream shape as Core. */
#include <pangu/agent/app.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { char *text; size_t chunks,bytes,events; } workload;
static void event(void *ctx,pangu_core *owner,const pangu_event *e) {
    (void)owner;(void)e;++((workload *)ctx)->events;
}
static pangu_status model(void *ctx,pangu_core *owner,const pangu_core *view,pangu_model_sink sink,void *target) {
    (void)owner;(void)view;workload *w=ctx;
    pangu_content text={.kind=PANGU_TEXT};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_PENDING};
    pangu_status status=sink(target,&reply,0);text.data.data=w->text;
    for(size_t i=1;status==PANGU_OK && i<=w->chunks;++i){
        text.data.len=i*w->bytes;if(i==w->chunks)reply.stop_reason=PANGU_STOP;
        status=sink(target,&reply,i==w->chunks);
    }
    return status;
}
static size_t number(const char *s) {
    char *end=NULL;unsigned long long n=strtoull(s,&end,10);
    return !s[0] || *end || n>10000000?0:(size_t)n;
}
static int verify(pg_agent_app *app,size_t turns,const workload *w) {
    const pangu_core *view=pg_session_messages(pg_agent_app_session(app));
    if(pangu_core_message_count(view)!=turns*2)return 0;
    for(size_t i=0;i<turns*2;++i){
        const pangu_message *m=pangu_core_message_at(view,i);
        size_t bytes=i%2?w->chunks*w->bytes:7;
        if(m->role!=(i%2?PANGU_ASSISTANT:PANGU_USER) || m->content_count!=1 ||
            m->content[0].kind!=PANGU_TEXT || m->content[0].data.len!=bytes ||
            memcmp(m->content[0].data.data,i%2?w->text:"profile",bytes))return 0;
        if(i%2 && m->stop_reason!=PANGU_STOP)return 0;
    }
    return 1;
}
int main(int argc,char **argv) {
    if(argc!=7)return 1;
    pg_session_backend backend=!strcmp(argv[1],"sqlite")?PG_STORE_SQLITE:PG_STORE_JSONL;
    if(strcmp(argv[1],"sqlite") && strcmp(argv[1],"jsonl"))return 2;
    size_t instances=number(argv[3]),turns=number(argv[4]),chunks=number(argv[5]),bytes=number(argv[6]);
    if(!instances || !turns || !chunks || !bytes || chunks>SIZE_MAX/bytes || instances>SIZE_MAX/sizeof(pg_agent_app *))return 2;
    workload w={.chunks=chunks,.bytes=bytes};w.text=malloc(chunks*bytes);
    pg_agent_app **apps=calloc(instances,sizeof(*apps));
    size_t path_bytes=strlen(argv[2])+64;char *path=malloc(path_bytes);
    if(!w.text || !apps || !path){free(w.text);free(apps);free(path);return 3;}
    memset(w.text,'x',chunks*bytes);size_t made=0;int failed=0;
    for(;made<instances;++made){
        int n=snprintf(path,path_bytes,"%s/session-%zu",argv[2],made);
        if(n<0 || (size_t)n>=path_bytes){failed=1;break;}
        pg_session_options options={.path={path,(size_t)n},.backend=backend,.create=1};
        if(pg_agent_app_open(&options,NULL,0,&apps[made])!=PG_OK){failed=1;break;}
    }
    if(!failed){puts("{\"ready\":true}");fflush(stdout);if(getchar()!='\n')failed=1;}
    pangu_content text={.kind=PANGU_TEXT,.data={"profile",7}};
    pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
    pangu_loop_config config={.ctx=&w,.request=model,.event=event};
    for(size_t turn=0;!failed && turn<turns;++turn)for(size_t i=0;!failed && i<instances;++i)
        failed=pg_agent_app_run(apps[i],&prompt,1,&config)!=PG_SESSION_OK;
    for(size_t i=0;!failed && i<made;++i)failed=!verify(apps[i],turns,&w);
    if(!failed){
        printf("{\"messages\":%zu,\"bytes\":%zu,\"events\":%zu}\n",instances*turns*2,instances*turns*(7+chunks*bytes),w.events);
        fflush(stdout);if(getchar()!='\n')failed=1;
    }
    for(size_t i=0;i<made;++i){pg_agent_app_close(apps[i]);apps[i]=NULL;}
    /* Reopen every real store after the retained-state resource sample. */
    for(size_t i=0;!failed && i<made;++i){
        int n=snprintf(path,path_bytes,"%s/session-%zu",argv[2],i);
        pg_session_options options={.path={path,(size_t)n},.backend=backend};
        if(pg_agent_app_open(&options,NULL,0,&apps[i])!=PG_OK)failed=1;
        else {failed=!verify(apps[i],turns,&w);pg_agent_app_close(apps[i]);apps[i]=NULL;}
    }
    free(path);free(apps);free(w.text);return failed;
}
