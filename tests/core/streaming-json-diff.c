#include "../../src/core/streaming_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void *take(void *ctx,size_t size) { (void)ctx; return malloc(size); }
static void drop(void *ctx,void *p) { (void)ctx; free(p); }
int main(int argc,char **argv) {
    if (argc>2) return 1;
    int final_only=argc==2 && !strcmp(argv[1],"--final");
    pangu_json_stream *s=NULL;
    if (pangu_json_stream_create((pangu_allocator){NULL,take,drop},&s)!=PANGU_OK) return 2;
    pangu_status status=PANGU_OK;
    size_t i=0;
    for (;;) {
        int ch=argc==2 && !final_only?(argv[1][i]?(unsigned char)argv[1][i++]:EOF):getchar();
        if (ch==EOF) break;
        char byte=(char)ch;
        status=pangu_json_stream_append(s,(pangu_slice){&byte,1});
        pangu_slice snapshot;
        if (final_only) {if(status!=PANGU_OK)break;continue;}
        if (status==PANGU_OK) status=pangu_json_stream_snapshot(s,&snapshot);
        if (status!=PANGU_OK) break;
        fwrite(snapshot.data,1,snapshot.len,stdout); putchar('\n');
    }
    pangu_slice arguments,error;
    if (status==PANGU_OK) status=pangu_json_stream_finish(s,&arguments,&error);
    if (status==PANGU_OK) {
        fputs("{\"arguments\":",stdout); fwrite(arguments.data,1,arguments.len,stdout);
        printf(",\"hasError\":%s}\n",error.len?"true":"false");
    }
    pangu_json_stream_destroy(s); return status==PANGU_OK?0:3;
}
