/* Fixed injected-model workload shared with tools/integration/profile-core.mjs. */
#include <pangu/core/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { char *text; size_t chunks, bytes, events; } workload;
static void event(void *ctx,pangu_core *owner,const pangu_event *e) {
    (void)owner; (void)e; ++((workload *)ctx)->events;
}
static pangu_status model(void *ctx,pangu_core *owner,const pangu_core *input,pangu_model_sink sink,void *target) {
    (void)owner; (void)input; workload *w=ctx;
    pangu_content text={.kind=PANGU_TEXT};
    pangu_message reply={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,.stop_reason=PANGU_PENDING};
    pangu_status status=sink(target,&reply,0);
    text.data.data=w->text;
    for (size_t i=1;status==PANGU_OK && i<=w->chunks;++i) {
        text.data.len=i*w->bytes;
        if (i==w->chunks) reply.stop_reason=PANGU_STOP;
        status=sink(target,&reply,i==w->chunks);
    }
    return status;
}
static size_t number(const char *s) {
    char *end=NULL; unsigned long long n=strtoull(s,&end,10);
    return !s[0] || *end || n>10000000?0:(size_t)n;
}
int main(int argc,char **argv) {
    if (argc!=5) return 1;
    size_t instances=number(argv[1]),turns=number(argv[2]),chunks=number(argv[3]),bytes=number(argv[4]);
    if (!instances || !turns || !chunks || !bytes || chunks>SIZE_MAX/bytes) return 2;
    workload w={.chunks=chunks,.bytes=bytes};
    w.text=malloc(chunks*bytes);
    pangu_core **cores=calloc(instances,sizeof(*cores));
    if (!w.text || !cores) { free(w.text); free(cores); return 3; }
    memset(w.text,'x',chunks*bytes);
    size_t made=0; int failed=0;
    for (;made<instances;++made) if (pangu_core_create(NULL,&cores[made])!=PANGU_OK) { failed=1; break; }
    if (!failed) {
        puts("{\"ready\":true}"); fflush(stdout);
        if (getchar()!='\n') failed=1;
    }
    pangu_content text={.kind=PANGU_TEXT,.data={"profile",7}};
    pangu_message prompt={.role=PANGU_USER,.content=&text,.content_count=1};
    pangu_loop_config config={.ctx=&w,.request=model,.event=event};
    for (size_t turn=0;!failed && turn<turns;++turn) for (size_t i=0;!failed && i<instances;++i)
        failed=pangu_core_run(cores[i],&prompt,1,&config)!=PANGU_OK;
    size_t messages=0,total_bytes=0;
    for (size_t i=0;i<made;++i) {
        messages+=pangu_core_message_count(cores[i]);
        for (size_t j=0;j<pangu_core_message_count(cores[i]);++j) {
            const pangu_message *m=pangu_core_message_at(cores[i],j);
            for (size_t k=0;k<m->content_count;++k) total_bytes+=m->content[k].data.len;
        }
    }
    if (!failed) {
        printf("{\"messages\":%zu,\"bytes\":%zu,\"events\":%zu}\n",messages,total_bytes,w.events);
        fflush(stdout);
        if (getchar()!='\n') failed=1;
    }
    for (size_t i=0;i<made;++i) pangu_core_destroy(cores[i]);
    free(cores); free(w.text); return failed;
}
