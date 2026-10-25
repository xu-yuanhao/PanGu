#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo.h"
#include <json-c/json.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pg_session session;static pg_mimo provider;
static void message(size_t index,pg_role role,const char *text){pg_message *m=&session.messages[index];*m=(pg_message){0};m->role=role;m->size=strlen(text);memcpy(m->text,text,m->size+1);}
static json_object *get(json_object *o,const char *key){json_object *v=0;json_object_object_get_ex(o,key,&v);return v;}
static void batch(size_t n){
 memset(&session,0,sizeof(session));session.count=2+2*n;
 message(0,PG_USER,"run");message(1,PG_ASSISTANT,"checking");
 for(size_t i=0;i<n;++i){char id[16];snprintf(id,sizeof(id),"id-%zu",i);message(2+i,PG_TOOL_CALL,"{}");strcpy(session.messages[2+i].call_id,id);strcpy(session.messages[2+i].tool_name,"read");message(2+n+i,PG_TOOL_RESULT,"result");strcpy(session.messages[2+n+i].call_id,id);}
}
static void batch_projection(void){
 for(size_t n=2;n<=8;++n){batch(n);assert(!pg_mimo_encode_request(&provider,&session));json_object *root=json_tokener_parse(provider.request),*messages=get(root,"messages"),*assistant=json_object_array_get_idx(messages,1),*calls=get(assistant,"tool_calls");assert(json_object_array_length(messages)==n+2&&json_object_array_length(calls)==n);assert(!strcmp(json_object_get_string(get(assistant,"content")),"checking"));for(size_t i=0;i<n;++i){char id[16];snprintf(id,sizeof(id),"id-%zu",i);assert(!strcmp(json_object_get_string(get(json_object_array_get_idx(calls,i),"id")),id));assert(!strcmp(json_object_get_string(get(json_object_array_get_idx(messages,2+i),"tool_call_id")),id));}json_object_put(root);
 --session.count;assert(pg_mimo_encode_request(&provider,&session)&&!provider.request);
 batch(n);strcpy(session.messages[2+n].call_id,"wrong");assert(pg_mimo_encode_request(&provider,&session));
 batch(n);strcpy(session.messages[3].call_id,"id-0");assert(pg_mimo_encode_request(&provider,&session));
 batch(n);memset(session.messages[1+n].tool_name,'x',64);assert(pg_mimo_encode_request(&provider,&session));
 batch(n);session.messages[1+n].size=PG_TEXT_LIMIT;assert(pg_mimo_encode_request(&provider,&session));
 batch(n);session.messages[2+n].role=PG_USER;assert(pg_mimo_encode_request(&provider,&session));
 }
 batch(9);assert(pg_mimo_encode_request(&provider,&session));
 batch(2);session.count=9;message(6,PG_ASSISTANT,"");message(7,PG_TOOL_CALL,"{}");strcpy(session.messages[7].call_id,"next");strcpy(session.messages[7].tool_name,"read");message(8,PG_TOOL_RESULT,"last");strcpy(session.messages[8].call_id,"next");assert(!pg_mimo_encode_request(&provider,&session));json_object *root=json_tokener_parse(provider.request),*messages=get(root,"messages");assert(json_object_array_length(messages)==6);assert(json_object_array_length(get(json_object_array_get_idx(messages,4),"tool_calls"))==1);json_object_put(root);
 free(provider.request);provider.request=0;
}
static void updated_system_projection(void){
 memset(&session,0,sizeof(session));session.count=2;message(0,PG_USER,"Say hello.");message(1,PG_ASSISTANT,"old answer");memcpy(session.system_prompt,"first rules",12);session.system_size=11;
 assert(!pg_mimo_encode_request(&provider,&session));json_object *root=json_tokener_parse(provider.request),*messages=get(root,"messages");assert(!strcmp(json_object_get_string(get(json_object_array_get_idx(messages,0),"content")),"first rules"));json_object_put(root);
 memcpy(session.system_prompt,"second rules",13);session.system_size=12;assert(!pg_mimo_encode_request(&provider,&session));root=json_tokener_parse(provider.request);messages=get(root,"messages");assert(json_object_array_length(messages)==3);assert(!strcmp(json_object_get_string(get(json_object_array_get_idx(messages,0),"content")),"second rules"));assert(!strcmp(json_object_get_string(get(json_object_array_get_idx(messages,2),"content")),"old answer"));json_object_put(root);free(provider.request);provider.request=0;
}
int main(void){updated_system_projection();batch_projection();memset(&session,0,sizeof(session));memcpy(session.system_prompt,"system-test",12);session.system_size=11;session.count=6;
 message(0,PG_USER,"hello");message(1,PG_ASSISTANT,"checking");message(2,PG_TOOL_CALL,"{}");strcpy(session.messages[2].call_id,"c1");strcpy(session.messages[2].tool_name,"read");message(3,PG_TOOL_RESULT,"result");strcpy(session.messages[3].call_id,"c1");message(4,PG_ASSISTANT,"");message(5,PG_USER,"next");
 assert(!pg_mimo_encode_request(&provider,&session));json_object *root=json_tokener_parse(provider.request),*messages=get(root,"messages");assert(json_object_array_length(messages)==5);
 json_object *system=json_object_array_get_idx(messages,0);assert(!strcmp(json_object_get_string(get(system,"role")),"system"));assert(!strcmp(json_object_get_string(get(system,"content")),"system-test"));
 json_object *assistant=json_object_array_get_idx(messages,2);assert(!strcmp(json_object_get_string(get(assistant,"content")),"checking"));assert(json_object_array_length(get(assistant,"tool_calls"))==1);assert(!get(root,"tools"));json_object_put(root);
 strcpy(session.messages[3].call_id,"wrong");assert(pg_mimo_encode_request(&provider,&session));strcpy(session.messages[3].call_id,"c1");session.count=3;assert(pg_mimo_encode_request(&provider,&session));
 session.count=1;message(0,PG_TOOL_RESULT,"orphan");assert(pg_mimo_encode_request(&provider,&session));
 session.count=PG_MESSAGE_LIMIT+1;assert(pg_mimo_encode_request(&provider,&session));free(provider.request);
 puts("PASS: system context, merged assistant/tool projection, empty assistant omission, orphan and mismatched tool refusal");return 0;}
