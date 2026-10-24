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
int main(void){memcpy(session.system_prompt,"system-test",12);session.system_size=11;session.count=6;
 message(0,PG_USER,"hello");message(1,PG_ASSISTANT,"checking");message(2,PG_TOOL_CALL,"{}");strcpy(session.messages[2].call_id,"c1");strcpy(session.messages[2].tool_name,"read");message(3,PG_TOOL_RESULT,"result");strcpy(session.messages[3].call_id,"c1");message(4,PG_ASSISTANT,"");message(5,PG_USER,"next");
 assert(!pg_mimo_encode_request(&provider,&session));json_object *root=json_tokener_parse(provider.request),*messages=get(root,"messages");assert(json_object_array_length(messages)==5);
 json_object *system=json_object_array_get_idx(messages,0);assert(!strcmp(json_object_get_string(get(system,"role")),"system"));assert(!strcmp(json_object_get_string(get(system,"content")),"system-test"));
 json_object *assistant=json_object_array_get_idx(messages,2);assert(!strcmp(json_object_get_string(get(assistant,"content")),"checking"));assert(json_object_array_length(get(assistant,"tool_calls"))==1);assert(!get(root,"tools"));json_object_put(root);
 strcpy(session.messages[3].call_id,"wrong");assert(pg_mimo_encode_request(&provider,&session));strcpy(session.messages[3].call_id,"c1");session.count=3;assert(pg_mimo_encode_request(&provider,&session));
 session.count=1;message(0,PG_TOOL_RESULT,"orphan");assert(pg_mimo_encode_request(&provider,&session));
 session.count=PG_MESSAGE_LIMIT+1;assert(pg_mimo_encode_request(&provider,&session));free(provider.request);
 puts("PASS: system context, merged assistant/tool projection, empty assistant omission, orphan and mismatched tool refusal");return 0;}
