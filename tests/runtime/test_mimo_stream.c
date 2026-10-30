#include "../../src/runtime/mimo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json-c/json.h>
static pg_mimo m;
static pg_model_event e;
static int line(const char *s){return pg_mimo_decode_line(&m,s,&e);}

static void responses(void){
 pg_model model=pg_mimo_model(&m);json_object *cfg=json_tokener_parse("{\"api\":\"openai-responses\",\"model\":\"native-test\",\"endpoint\":\"https://example.invalid/v1/responses\",\"options\":{\"maxTokens\":8,\"reasoningEffort\":\"low\"}}");assert(cfg&&!pg_mimo_configure(&m,cfg));json_object_put(cfg);
 static pg_session session;strcpy(session.system_prompt,"PanGu");session.system_size=5;session.count=4;session.messages[0].role=PG_USER;strcpy(session.messages[0].text,"read");session.messages[0].size=4;session.messages[1].role=PG_TOOL_CALL;strcpy(session.messages[1].text,"{}");session.messages[1].size=2;strcpy(session.messages[1].call_id,"call-1");strcpy(session.messages[1].tool_name,"echo");session.messages[2].role=PG_TOOL_RESULT;strcpy(session.messages[2].call_id,"call-1");strcpy(session.messages[2].tool_name,"echo");strcpy(session.messages[2].text,"result");session.messages[2].size=6;session.messages[3].role=PG_USER;strcpy(session.messages[3].text,"continue");session.messages[3].size=8;
 pg_tool_registry tools={.entries={{.name="echo",.description="Echo",.schema="{\"type\":\"object\"}"}},.count=1};pg_mimo_bind_tools(&m,&tools);assert(!pg_mimo_encode_request(&m,&session));json_object *wire=json_tokener_parse(m.request);assert(wire&&!json_object_object_get(wire,"messages")&&!json_object_object_get(wire,"stream_options")&&json_object_get_int(json_object_object_get(wire,"max_output_tokens"))==16&&!json_object_get_boolean(json_object_object_get(wire,"store")));json_object *input=json_object_object_get(wire,"input");unsigned calls=0,results=0;for(size_t i=0;i<json_object_array_length(input);i++){json_object *item=json_object_array_get_idx(input,i);const char *type=json_object_get_string(json_object_object_get(item,"type"));if(type&&!strcmp(type,"function_call")){assert(!strcmp(json_object_get_string(json_object_object_get(item,"call_id")),"call-1"));calls++;}if(type&&!strcmp(type,"function_call_output"))results++;}assert(calls==1&&results==1);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_array_get_idx(json_object_object_get(wire,"tools"),0),"name")),"echo"));json_object_put(wire);
 assert(!line("event: response.created"));assert(!line("data: {\"type\":\"response.created\",\"response\":{\"id\":\"resp_native\"}}"));
 assert(!line("data: {\"type\":\"response.reasoning_summary_text.delta\",\"output_index\":0,\"delta\":\"think\"}"));assert(!strcmp(m.reasoning,"think"));
 assert(!line("data: {\"type\":\"response.output_text.delta\",\"output_index\":1,\"delta\":\"Hello世界\"}")&&e.kind==PG_MODEL_TEXT&&e.text.size==11);
 assert(!line("data: {\"type\":\"response.output_item.added\",\"output_index\":2,\"item\":{\"type\":\"function_call\",\"call_id\":\"call-a\",\"name\":\"echo\",\"arguments\":\"\"}}"));
 assert(!line("data: {\"type\":\"response.function_call_arguments.delta\",\"output_index\":2,\"delta\":\"{bad\"}"));
 assert(!line("data: {\"type\":\"response.function_call_arguments.done\",\"output_index\":2,\"arguments\":\"{}\"}"));assert(!strcmp(m.arguments,"{}"));
 assert(!line("data: {\"type\":\"response.output_item.done\",\"output_index\":7,\"item\":{\"type\":\"function_call\",\"call_id\":\"call-b\",\"name\":\"echo\",\"arguments\":\"{}\"}}"));assert(m.call_count==2&&!strcmp(m.extra[0].id,"call-b"));
 assert(!line("data: {\"type\":\"response.completed\",\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":20,\"output_tokens\":8,\"total_tokens\":28,\"input_tokens_details\":{\"cached_tokens\":5},\"output_tokens_details\":{\"reasoning_tokens\":3}}}}")&&m.finish_reason==2&&m.saw_done);

 json_object *vision_cfg=json_tokener_parse("{\"api\":\"openai-responses\",\"model\":\"vision-test\",\"endpoint\":\"https://example.invalid/responses\",\"input\":[\"text\",\"image\"]}");assert(!pg_mimo_configure(&m,vision_cfg));json_object_put(vision_cfg);json_object *context=json_tokener_parse("{\"systemPrompt\":\"PanGu\",\"messages\":[{\"role\":\"assistant\",\"content\":[{\"type\":\"toolCall\",\"id\":\"call-img\",\"name\":\"echo\",\"arguments\":{}}]},{\"role\":\"toolResult\",\"toolCallId\":\"call-img\",\"toolName\":\"echo\",\"content\":[{\"type\":\"text\",\"text\":\"caption\"},{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":\"aGVsbG8=\"}]}]}");session.count=1;session.messages[0].role=PG_CONTEXT_VIEW;session.messages[0].context_view=context;assert(!pg_mimo_encode_request(&m,&session));wire=json_tokener_parse(m.request);input=json_object_object_get(wire,"input");unsigned image_outputs=0;for(size_t i=0;i<json_object_array_length(input);i++){json_object *item=json_object_array_get_idx(input,i);const char *type=json_object_get_string(json_object_object_get(item,"type"));if(type&&!strcmp(type,"function_call_output")){json_object *parts=json_object_object_get(item,"output");assert(json_object_array_length(parts)==2);json_object *image=json_object_array_get_idx(parts,1);assert(!strcmp(json_object_get_string(json_object_object_get(image,"type")),"input_image")&&!strcmp(json_object_get_string(json_object_object_get(image,"image_url")),"data:image/png;base64,aGVsbG8="));image_outputs++;}}assert(image_outputs==1);json_object_put(wire);json_object_put(context);
 json_object *assistant=json_tokener_parse("{\"content\":[]}");assert(!pg_mimo_complete(&m,assistant));json_object *usage=json_object_object_get(assistant,"usage");assert(json_object_get_int(json_object_object_get(usage,"input"))==15&&json_object_get_int(json_object_object_get(usage,"cacheRead"))==5&&json_object_get_int(json_object_object_get(usage,"reasoning"))==3&&json_object_get_int(json_object_object_get(usage,"totalTokens"))==28);json_object_put(assistant);assert(line("data: {\"type\":\"response.output_text.delta\",\"delta\":\"late\"}"));model.close(model.context);
 cfg=json_tokener_parse("{\"model\":\"test\",\"endpoint\":\"https://example.invalid/responses\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);m.finish_reason=m.saw_done=0;
 assert(line("data: {\"type\":\"response.completed\"}junk"));assert(line("data: {\"type\":\"response.failed\",\"response\":{\"error\":{\"message\":\"bad\"}}}"));assert(!line("data: {\"type\":\"response.incomplete\",\"response\":{\"status\":\"incomplete\",\"incomplete_details\":{\"reason\":\"max_output_tokens\"}}}")&&m.finish_reason==3&&m.saw_done);model.close(model.context);
 puts("PASS: Responses request/function projection, sparse call indices, final argument replacement, text/thinking, usage and terminal/error events");
}
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
 m=(pg_mimo){0};
 assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\\u0000\"}}]}}]}"));
 const char *indices[]={"-1","8","4294967296","9223372036854775807","0.5","true","\"1\""};
 for(size_t i=0;i<sizeof(indices)/sizeof(*indices);++i){m=(pg_mimo){0};char text[512];snprintf(text,sizeof(text),"data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":%s}]}}]}",indices[i]);assert(line(text));}
 responses();
 puts("PASS: incremental text, UTF-8, split tool arguments, terminal protocol, malformed/truncated and capacity rejection");return 0;
}
