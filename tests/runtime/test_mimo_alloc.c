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
static pg_session session;
static pg_mimo provider;
static pg_tool_registry registry;
int main(void){
 session.system_size=3;memcpy(session.system_prompt,"sys",4);session.count=5;
 const char *texts[]={"hi","checking","{}","result","done"};
 const pg_role roles[]={PG_USER,PG_ASSISTANT,PG_TOOL_CALL,PG_TOOL_RESULT,PG_ASSISTANT};
 for(unsigned i=0;i<5;++i){session.messages[i].role=roles[i];session.messages[i].size=strlen(texts[i]);strcpy(session.messages[i].text,texts[i]);}
 strcpy(session.messages[2].call_id,"one");strcpy(session.messages[2].tool_name,"read");strcpy(session.messages[3].call_id,"one");
 registry.count=1;registry.entries[0].name="read";registry.entries[0].description="read text";registry.entries[0].schema="{\"type\":\"object\"}";provider.tools=&registry;
 for(unsigned n=1;n<=8;n+=7){
 if(n==8){memset(session.messages,0,sizeof(session.messages));session.count=18;session.messages[0].role=PG_USER;session.messages[1].role=PG_ASSISTANT;for(unsigned j=0;j<n;++j){pg_message *c=&session.messages[2+j],*r=&session.messages[10+j];c->role=PG_TOOL_CALL;c->size=2;strcpy(c->text,"{}");strcpy(c->tool_name,"read");snprintf(c->call_id,sizeof(c->call_id),"id-%u",j);r->role=PG_TOOL_RESULT;r->size=2;strcpy(r->text,"ok");strcpy(r->call_id,c->call_id);}}
 fail_at=0;calls=0;tracked=0;
 assert(!pg_mimo_encode_request(&provider,&session)&&provider.request&&!live);free(provider.request);provider.request=0;unsigned total=calls;
 for(unsigned i=1;i<=total;++i){fail_at=i;calls=0;tracked=0;assert(pg_mimo_encode_request(&provider,&session)&&!provider.request);assert(calls>=i&&!live);}
 printf("PASS: %u-call projection: %u allocation/insertion/serialization failures release JSON ownership\n",n,total);
 }return 0;
}
