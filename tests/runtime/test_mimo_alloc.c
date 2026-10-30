#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo_layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned calls,fail_at,live,tracked;
static unsigned char alive[512];
static struct json_object *objects[512];
static int reject(void){return ++calls==fail_at;}
/* json-c 0.19 iterative container release may notify userdata twice. Count
   unique allocations, not callback invocations; real frees are also checked by ASan. */
static void deleted(struct json_object *obj,void *ctx){(void)ctx;for(unsigned i=tracked;i>0;--i)if(objects[i-1]==obj){if(alive[i-1]){alive[i-1]=0;assert(live);--live;}return;}assert(0);}
static struct json_object *owned(struct json_object *obj){if(obj){assert(tracked<sizeof(alive));objects[tracked]=obj;alive[tracked++]=1;++live;json_object_set_userdata(obj,0,deleted);}return obj;}
struct json_object *__real_json_object_new_object(void);
struct json_object *__real_json_object_new_array(void);
struct json_object *__real_json_object_new_string(const char *);
struct json_object *__real_json_object_new_string_len(const char *,int);
struct json_object *__real_json_object_new_boolean(json_bool);
struct json_object *__real_json_object_new_int(int32_t);
int __real_json_object_object_add(struct json_object *,const char *,struct json_object *);
int __real_json_object_array_add(struct json_object *,struct json_object *);
struct json_object *__real_json_tokener_parse(const char *);
const char *__real_json_object_to_json_string_ext(struct json_object *,int);
char *__real_strdup(const char *);
struct json_object *__wrap_json_object_new_object(void){return reject()?0:owned(__real_json_object_new_object());}
struct json_object *__wrap_json_object_new_array(void){return reject()?0:owned(__real_json_object_new_array());}
struct json_object *__wrap_json_object_new_string(const char *s){return reject()?0:owned(__real_json_object_new_string(s));}
struct json_object *__wrap_json_object_new_string_len(const char *s,int n){return reject()?0:owned(__real_json_object_new_string_len(s,n));}
struct json_object *__wrap_json_object_new_boolean(json_bool b){return reject()?0:owned(__real_json_object_new_boolean(b));}
struct json_object *__wrap_json_object_new_int(int32_t n){return reject()?0:owned(__real_json_object_new_int(n));}
int __wrap_json_object_object_add(struct json_object *o,const char *key,struct json_object *v){return reject()?-1:__real_json_object_object_add(o,key,v);}
int __wrap_json_object_array_add(struct json_object *o,struct json_object *v){return reject()?-1:__real_json_object_array_add(o,v);}
struct json_object *__wrap_json_tokener_parse(const char *s){return reject()?0:owned(__real_json_tokener_parse(s));}
const char *__wrap_json_object_to_json_string_ext(struct json_object *o,int flags){return reject()?0:__real_json_object_to_json_string_ext(o,flags);}
char *__wrap_strdup(const char *s){return reject()?0:__real_strdup(s);}
/* Tracking userdata belongs to this fixture, not to detached JSON copies. */
int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
static int plain_copy(struct json_object *src,struct json_object *parent,const char *key,size_t index,struct json_object **out){int rc=json_c_shallow_copy_default(src,parent,key,index,out);return rc<0?rc:2;}
int __wrap_json_object_deep_copy(struct json_object *src,struct json_object **out,json_c_shallow_copy_fn *copy){return __real_json_object_deep_copy(src,out,copy?copy:plain_copy);}
static pg_session session;
static pg_mimo provider;
static pg_tool_registry registry;
static const pg_tool_options sampling_options={.max_bytes=51200,.max_lines=2000,.constrained_sampling="{\"type\":\"json_schema\",\"strict\":\"prefer\"}"};

static void anthropic_decoder_failures(void){
 const char *frames[]={
  "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg-fault\",\"usage\":{\"input_tokens\":3}}}",
  "data: {\"type\":\"content_block_start\",\"index\":4,\"content_block\":{\"type\":\"tool_use\",\"id\":\"call-fault\",\"name\":\"read\",\"input\":{}}}",
  "data: {\"type\":\"content_block_delta\",\"index\":4,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{bad\"}}",
  "data: {\"type\":\"content_block_stop\",\"index\":4}",
  "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":4}}",
  "data: {\"type\":\"message_stop\"}"
 };
 unsigned total=0;
 for(unsigned fault=0;fault<=total;fault++){
  fail_at=0;calls=tracked=0;pg_model model=pg_mimo_model(&provider);provider.configuration=__real_json_tokener_parse("{\"api\":\"anthropic-messages\",\"model\":\"claude\"}");json_object *assistant=__real_json_tokener_parse("{\"content\":[]}");fail_at=fault;calls=0;int status=0;pg_model_event event={0};
  for(size_t i=0;i<sizeof(frames)/sizeof(*frames)&&!status;i++)status=pg_mimo_decode_line(&provider,frames[i],&event);
  if(!status)status=pg_mimo_complete(&provider,assistant);
  if(!fault){assert(!status);total=calls;}else{if(!status)fprintf(stderr,"Anthropic ignored allocation failure %u/%u\n",fault,total);assert(status&&calls>=fault);}
  json_object_put(assistant);model.close(model.context);if(live)fprintf(stderr,"Anthropic fault %u leaks %u owners\n",fault,live);assert(!live);
 }
 printf("PASS: Anthropic final tool lifecycle: %u allocation/insertion failures release ownership\n",total);
}

int main(void){
 for(unsigned protocol=0;protocol<5;protocol++){
 struct json_object *canonical=0;
 memset(&session,0,sizeof(session));
 session.system_size=3;memcpy(session.system_prompt,"sys",4);session.count=5;
 const char *texts[]={"hi","checking","{}","result","done"};
 const pg_role roles[]={PG_USER,PG_ASSISTANT,PG_TOOL_CALL,PG_TOOL_RESULT,PG_ASSISTANT};
 for(unsigned i=0;i<5;++i){session.messages[i].role=roles[i];session.messages[i].size=strlen(texts[i]);strcpy(session.messages[i].text,texts[i]);}
 strcpy(session.messages[2].call_id,"one");strcpy(session.messages[2].tool_name,"read");strcpy(session.messages[3].call_id,"one");
 registry.count=1;registry.entries[0].name="read";registry.entries[0].description="read text";registry.entries[0].schema="{\"type\":\"object\"}";registry.entries[0].options=protocol>=3?&sampling_options:0;provider.tools=&registry;
 provider.configuration=protocol?__real_json_tokener_parse("{\"api\":\"openai-responses\",\"model\":\"native-responses\",\"modelProvider\":\"openai\",\"endpoint\":\"https://example.invalid/responses\"}"):0;
 if(protocol>=3){json_object_put(provider.configuration);provider.configuration=__real_json_tokener_parse("{\"api\":\"anthropic-messages\",\"model\":\"claude-test\",\"endpoint\":\"https://example.invalid/messages\",\"options\":{\"toolChoice\":{\"type\":\"tool\",\"name\":\"read\",\"disable_parallel_tool_use\":true},\"metadata\":{\"user_id\":\"native-user\"}},\"modelInfo\":{\"compat\":{\"allowedFallbackModels\":[{\"model\":\"fallback-native\"}]}}}");}
 if(protocol==2){canonical=__real_json_tokener_parse("{\"messages\":[{\"role\":\"assistant\",\"api\":\"openai-responses\",\"model\":\"native-responses\",\"provider\":\"openai\",\"content\":[{\"type\":\"thinking\",\"thinking\":\"saved thought\",\"thinkingSignature\":\"{\\\"type\\\":\\\"reasoning\\\",\\\"id\\\":\\\"rs_saved\\\",\\\"summary\\\":[{\\\"type\\\":\\\"summary_text\\\",\\\"text\\\":\\\"saved thought\\\"}],\\\"encrypted_content\\\":\\\"cipher-saved\\\"}\"},{\"type\":\"text\",\"text\":\"saved text\",\"textSignature\":\"{\\\"v\\\":1,\\\"id\\\":\\\"msg_saved\\\",\\\"phase\\\":\\\"commentary\\\"}\"},{\"type\":\"toolCall\",\"id\":\"call-saved\",\"name\":\"read\",\"arguments\":{},\"responseItemId\":\"fc_saved\"}]},{\"role\":\"toolResult\",\"toolCallId\":\"call-saved\",\"toolName\":\"read\",\"content\":[{\"type\":\"text\",\"text\":\"ok\"}]}]}");assert(canonical);session.count=1;session.messages[0].role=PG_CONTEXT_VIEW;session.messages[0].context_view=canonical;}

 if(protocol==4){json_object_object_add(json_object_object_get(provider.configuration,"modelInfo"),"input",__real_json_tokener_parse("[\"text\",\"image\"]"));canonical=__real_json_tokener_parse("{\"messages\":[{\"role\":\"assistant\",\"content\":[{\"type\":\"toolCall\",\"id\":\"image-fault\",\"name\":\"read\",\"arguments\":{}}]},{\"role\":\"toolResult\",\"toolCallId\":\"image-fault\",\"toolName\":\"read\",\"isError\":true,\"content\":[{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":\"YQ==\"},{\"type\":\"text\",\"text\":\"error\"}]}]}");session.count=1;session.messages[0].role=PG_CONTEXT_VIEW;session.messages[0].context_view=canonical;}
 for(unsigned n=1;n<=(protocol==2||protocol==4?1u:8u);n+=7){
 if(n==8){memset(session.messages,0,sizeof(session.messages));session.count=18;session.messages[0].role=PG_USER;session.messages[1].role=PG_ASSISTANT;for(unsigned j=0;j<n;++j){pg_message *c=&session.messages[2+j],*r=&session.messages[10+j];c->role=PG_TOOL_CALL;c->size=2;strcpy(c->text,"{}");strcpy(c->tool_name,"read");snprintf(c->call_id,sizeof(c->call_id),"id-%u",j);r->role=PG_TOOL_RESULT;r->size=2;strcpy(r->text,"ok");strcpy(r->call_id,c->call_id);}}
 fail_at=0;calls=0;tracked=0;
 int baseline=pg_mimo_encode_request(&provider,&session);if(baseline||live)fprintf(stderr,"projection baseline protocol=%u calls=%u status=%d live=%u\n",protocol,n,baseline,live);assert(!baseline&&provider.request&&!live);free(provider.request);provider.request=0;unsigned total=calls;
 for(unsigned i=1;i<=total;++i){fail_at=i;calls=0;tracked=0;assert(pg_mimo_encode_request(&provider,&session)&&!provider.request);if(live)fprintf(stderr,"projection failure protocol=%u index=%u calls=%u live=%u\n",protocol,i,calls,live);assert(calls>=i&&!live);}
 printf("PASS: %u-call projection: %u allocation/insertion/serialization failures release JSON ownership\n",n,total);
 }json_object_put(canonical);json_object_put(provider.configuration);provider.configuration=0;
 }
 const char *finished[]={"{\"x\":1}","{\"x\":\"a\\q\"}","{broken"};for(unsigned c=0;c<3;c++){struct json_object *value=0;fail_at=0;calls=tracked=0;assert(!pg_mimo_arguments_parse((pg_slice){finished[c],strlen(finished[c])},&value));unsigned total=calls;json_object_put(value);assert(!live);for(unsigned i=1;i<=total;i++){fail_at=i;calls=tracked=0;value=0;assert(pg_mimo_arguments_parse((pg_slice){finished[c],strlen(finished[c])},&value)&&!value&&!live);}printf("PASS: final arguments case %u: %u constructor/insertion failures release ownership\n",c,total);}
 anthropic_decoder_failures();
 return 0;
}
