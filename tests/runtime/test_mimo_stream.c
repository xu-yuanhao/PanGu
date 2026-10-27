#include "../../src/runtime/mimo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static pg_mimo m;
static pg_model_event e;
static int line(const char *s){return pg_mimo_decode_line(&m,s,&e);}
int main(void){
 assert(!line(": heartbeat"));assert(!line(""));
 assert(!line("data: {\"choices\":[{\"delta\":{\"content\":\"hello \"},\"finish_reason\":null}]}"));assert(e.kind==PG_MODEL_TEXT&&e.text.size==6);
 assert(!line("data: {\"choices\":[{\"delta\":{\"content\":\"世界\"},\"finish_reason\":null}]}"));assert(e.text.size==6);
 assert(!line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}"));assert(m.finish_reason==1);
 assert(!line("data: [DONE]"));assert(m.saw_done);assert(line("data: [DONE]"));
 m=(pg_mimo){0};assert(line("data: [DONE]"));assert(line("data: broken"));assert(line("data: {\"choices\":[]}junk"));
 assert(!line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"length\"}]}"));assert(m.finish_reason==3);
 m=(pg_mimo){0};
 assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call1\",\"function\":{\"name\":\"echo\",\"arguments\":\"{\"}}]}}]}"));
 assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"}\"}}]}}]}"));
 assert(!strcmp(m.arguments,"{}"));assert(!strcmp(m.id,"call1"));assert(!strcmp(m.name,"echo"));
 assert(!line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}"));assert(m.finish_reason==2);assert(!line("data: [DONE]"));
 m=(pg_mimo){0};assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1}]}}]}"));assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}"));
 m=(pg_mimo){0};memset(m.arguments,'x',sizeof(m.arguments)-1);
 assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"a\"}}]}}]}"));
 const char *indices[]={"-1","8","4294967296","9223372036854775807","0.5","true","\"1\""};
 for(size_t i=0;i<sizeof(indices)/sizeof(*indices);++i){m=(pg_mimo){0};char text[512];snprintf(text,sizeof(text),"data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":%s}]}}]}",indices[i]);assert(line(text));}
 puts("PASS: incremental text, UTF-8, split tool arguments, terminal protocol, malformed/truncated and capacity rejection");return 0;
}
