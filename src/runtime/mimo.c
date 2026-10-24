#include "mimo.h"
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
static size_t receive(char *p,size_t size,size_t count,void *ctx){pg_mimo *m=ctx;size_t n;
 if(size && count>SIZE_MAX/size)return 0;n=size*count;
 if(n>=sizeof(m->body)-m->used)return 0;
 memcpy(m->body+m->used,p,n);m->used+=n;m->body[m->used]=0;return n;}
static void close_model(void *ctx){pg_mimo *m=ctx;
 if(m->multi && m->easy)curl_multi_remove_handle(m->multi,m->easy);
 if(m->easy)curl_easy_cleanup(m->easy);if(m->multi)curl_multi_cleanup(m->multi);
 curl_slist_free_all(m->headers);free(m->request);m->easy=0;m->multi=0;m->headers=0;m->request=0;}
static void cancel_model(void *ctx){close_model(ctx);}
static void field(json_object *o,const char *k,const char *v){json_object_object_add(o,k,json_object_new_string(v));}
int pg_mimo_encode_request(pg_mimo *m,const pg_session *s){
 json_object *root,*messages,*tools,*tool,*fn;size_t i;
 if(!m || !s || s->count>PG_MESSAGE_LIMIT || s->system_size>=PG_TEXT_LIMIT)return 1;
 free(m->request);m->request=0;
 root=json_object_new_object();messages=json_object_new_array();
 field(root,"model",getenv("MIMO_MODEL")?getenv("MIMO_MODEL"):"mimo-v2.6-pro");
 json_object_object_add(root,"stream",json_object_new_boolean(1));
 json_object_object_add(root,"messages",messages);json_object_object_add(root,"max_tokens",json_object_new_int(512));
 json_object *thinking=json_object_new_object();field(thinking,"type","disabled");json_object_object_add(root,"thinking",thinking);
 if(s->system_size){json_object *v=json_object_new_object();field(v,"role","system");json_object_object_add(v,"content",json_object_new_string_len(s->system_prompt,(int)s->system_size));json_object_array_add(messages,v);}
 for(i=0;i<s->count;++i){const pg_message *p=&s->messages[i],*text=p;
  if(p->size>=PG_TEXT_LIMIT || p->role<PG_USER || p->role>PG_TOOL_RESULT)goto invalid_projection;
  if(p->role==PG_ASSISTANT && i+1<s->count && s->messages[i+1].role==PG_TOOL_CALL){p=&s->messages[++i];if(p->size>=PG_TEXT_LIMIT)goto invalid_projection;}
  else if(p->role==PG_ASSISTANT && !p->size)continue;
  if(p->role==PG_TOOL_CALL){
   if(!memchr(p->call_id,0,PG_ID_LIMIT) || !p->call_id[0] || !memchr(p->tool_name,0,PG_ID_LIMIT) || !p->tool_name[0] || i+1>=s->count || s->messages[i+1].role!=PG_TOOL_RESULT || !memchr(s->messages[i+1].call_id,0,PG_ID_LIMIT) || strcmp(p->call_id,s->messages[i+1].call_id))goto invalid_projection;
  }
  if(p->role==PG_TOOL_RESULT && (!i || s->messages[i-1].role!=PG_TOOL_CALL || !memchr(p->call_id,0,PG_ID_LIMIT) || strcmp(p->call_id,s->messages[i-1].call_id)))goto invalid_projection;
  json_object *v=json_object_new_object();field(v,"role",p->role==PG_USER?"user":p->role==PG_TOOL_RESULT?"tool":"assistant");
  if(p->role==PG_TOOL_CALL){json_object *calls=json_object_new_array(),*call=json_object_new_object(),*func=json_object_new_object();
   field(call,"id",p->call_id);field(call,"type","function");field(func,"name",p->tool_name);json_object_object_add(func,"arguments",json_object_new_string_len(p->text,(int)p->size));json_object_object_add(call,"function",func);json_object_array_add(calls,call);json_object_object_add(v,"tool_calls",calls);
   if(text!=p && text->size)json_object_object_add(v,"content",json_object_new_string_len(text->text,(int)text->size));
  }else {json_object_object_add(v,"content",json_object_new_string_len(p->text,(int)p->size));if(p->role==PG_TOOL_RESULT)field(v,"tool_call_id",p->call_id);}
  json_object_array_add(messages,v);
 }
 if(m->tools && m->tools->count){
  tools=json_object_new_array();
  for(size_t index=0;index<m->tools->count;++index){
   const pg_tool_definition *def=&m->tools->entries[index];
   tool=json_object_new_object();fn=json_object_new_object();field(tool,"type","function");
   field(fn,"name",def->name);field(fn,"description",def->description);
   json_object *schema=json_tokener_parse(def->schema);
   if(!schema){json_object_put(tool);json_object_put(fn);json_object_put(tools);json_object_put(root);return 1;}
   json_object_object_add(fn,"parameters",schema);json_object_object_add(tool,"function",fn);json_object_array_add(tools,tool);
  }
  json_object_object_add(root,"tools",tools);
 }
 m->request=strdup(json_object_to_json_string_ext(root,JSON_C_TO_STRING_PLAIN));json_object_put(root);if(!m->request)return 1;
 return 0;
invalid_projection:
 json_object_put(root);return 1;
}
static int start(void *ctx,const pg_session *s){pg_mimo *m=ctx;const char *key=getenv("MIMO_API_KEY");char auth[512];
 close_model(m);m->used=0;m->consumed=0;m->delivered=0;m->failed=0;m->http=0;
 m->transport_done=0;m->finish_reason=0;m->saw_done=0;m->arguments[0]=0;m->id[0]=0;m->name[0]=0;
 if(!key || strlen(key)>400 || strchr(key,'\r') || strchr(key,'\n'))return 1;
 if(pg_mimo_encode_request(m,s))return 1;
 snprintf(auth,sizeof(auth),"Authorization: Bearer %s",key);m->headers=curl_slist_append(m->headers,auth);m->headers=curl_slist_append(m->headers,"Content-Type: application/json");
 m->easy=curl_easy_init();m->multi=curl_multi_init();if(!m->easy||!m->multi)return 1;
 curl_easy_setopt(m->easy,CURLOPT_URL,"https://token-plan-cn.xiaomimimo.com/v1/chat/completions");
 curl_easy_setopt(m->easy,CURLOPT_HTTPHEADER,m->headers);curl_easy_setopt(m->easy,CURLOPT_POSTFIELDS,m->request);
 curl_easy_setopt(m->easy,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(m->easy,CURLOPT_WRITEDATA,m);
 curl_easy_setopt(m->easy,CURLOPT_TIMEOUT,60L);curl_easy_setopt(m->easy,CURLOPT_CONNECTTIMEOUT,15L);
 curl_easy_setopt(m->easy,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(m->easy,CURLOPT_PROTOCOLS_STR,"https");
 return curl_multi_add_handle(m->multi,m->easy)!=CURLM_OK;}
static json_object *get(json_object *o,const char *key){json_object *v=0;if(o)json_object_object_get_ex(o,key,&v);return v;}
static int extract(json_object *o,char *out,size_t cap){if(!o||!json_object_is_type(o,json_type_string))return 1;size_t n=(size_t)json_object_get_string_len(o);if(n>=cap || memchr(json_object_get_string(o),0,n))return 1;memcpy(out,json_object_get_string(o),n+1);return 0;}
static int accumulate(json_object *value,char *out,size_t capacity){
 if(!value || json_object_is_type(value,json_type_null))return 0;
 if(!json_object_is_type(value,json_type_string))return 1;
 size_t old=strlen(out),n=(size_t)json_object_get_string_len(value);
 if(n>=capacity-old || memchr(json_object_get_string(value),0,n))return 1;
 memcpy(out+old,json_object_get_string(value),n+1);return 0;
}
int pg_mimo_decode_line(pg_mimo *m,const char *line,pg_model_event *event){
 *event=(pg_model_event){.kind=PG_MODEL_WAIT};
 if(!line[0] || line[0]==':')return 0;
 if(strncmp(line,"data:",5))return 1;
 const char *data=line+5;if(*data==' ')++data;
 if(!strcmp(data,"[DONE]")){if(!m->finish_reason || m->saw_done)return 1;m->saw_done=1;return 0;}
 if(m->saw_done)return 1;
 json_tokener *tok=json_tokener_new();if(!tok)return 1;
 json_object *root=json_tokener_parse_ex(tok,data,(int)strlen(data));
 int failed=json_tokener_get_error(tok)!=json_tokener_success;
 size_t parsed=json_tokener_get_parse_end(tok);while(data[parsed]==' ' || data[parsed]=='\t')++parsed;
 failed|=data[parsed]!=0;json_tokener_free(tok);
 json_object *choices=get(root,"choices");
 if(failed || !choices || !json_object_is_type(choices,json_type_array)){if(root)json_object_put(root);return 1;}
 if(json_object_array_length(choices)==0){json_object_put(root);return 0;}
 if(json_object_array_length(choices)!=1 || m->finish_reason){json_object_put(root);return 1;}
 json_object *choice=json_object_array_get_idx(choices,0),*delta=get(choice,"delta"),*calls=get(delta,"tool_calls"),*content=get(delta,"content"),*reason=get(choice,"finish_reason");
 if(calls){
  if(!json_object_is_type(calls,json_type_array) || json_object_array_length(calls)!=1)failed=1;
  else {json_object *call=json_object_array_get_idx(calls,0),*index=get(call,"index"),*fn=get(call,"function");
   if(!index || !json_object_is_type(index,json_type_int) || json_object_get_int(index)!=0)failed=1;
   failed|=accumulate(get(call,"id"),m->id,sizeof(m->id));
   failed|=accumulate(get(fn,"name"),m->name,sizeof(m->name));
   failed|=accumulate(get(fn,"arguments"),m->arguments,sizeof(m->arguments));
  }
 }
 if(content && !json_object_is_type(content,json_type_null)){
  failed|=extract(content,m->text,sizeof(m->text));
  if(!failed && m->text[0]){event->kind=PG_MODEL_TEXT;event->text=(pg_slice){m->text,strlen(m->text)};}
 }
 if(reason && !json_object_is_type(reason,json_type_null)){
  const char *r=json_object_get_string(reason);
  if(!json_object_is_type(reason,json_type_string) || !r)failed=1;
  else if(!strcmp(r,"stop") && !m->id[0])m->finish_reason=1;
  else if(!strcmp(r,"tool_calls") && m->id[0] && m->name[0])m->finish_reason=2;
  else failed=1;
 }
 json_object_put(root);return failed;
}
static int next(void *ctx,pg_model_event *event){pg_mimo *m=ctx;int running=0,left=0;CURLMsg *msg;
 *event=(pg_model_event){.kind=PG_MODEL_WAIT};
 if(m->delivered){event->kind=PG_MODEL_DONE;return 0;}
 if(!m->transport_done){
  if(curl_multi_perform(m->multi,&running)!=CURLM_OK)return 1;
  curl_easy_getinfo(m->easy,CURLINFO_RESPONSE_CODE,&m->http);
  if(m->http && m->http!=200)return 1;
  if(!running){msg=curl_multi_info_read(m->multi,&left);if(!msg||msg->msg!=CURLMSG_DONE||msg->data.result!=CURLE_OK || m->http!=200)return 1;m->transport_done=1;}
 }
 if(m->consumed<m->used){
  char *start=m->body+m->consumed,*end=memchr(start,'\n',m->used-m->consumed);
  if(!end)return m->transport_done?1:0;
  size_t next_offset=(size_t)(end-m->body)+1;*end=0;if(end>start && end[-1]=='\r')end[-1]=0;
  m->consumed=next_offset;return pg_mimo_decode_line(m,start,event);
 }
 if(!m->transport_done)return 0;
 if(!m->saw_done || !m->finish_reason)return 1;
 if(m->finish_reason==2){
  json_tokener *tok=json_tokener_new();if(!tok)return 1;
  json_object *args=json_tokener_parse_ex(tok,m->arguments,(int)strlen(m->arguments));
  size_t end=json_tokener_get_parse_end(tok);while(m->arguments[end]==' ' || m->arguments[end]=='\t' || m->arguments[end]=='\r' || m->arguments[end]=='\n')++end;
  int invalid=json_tokener_get_error(tok)!=json_tokener_success || m->arguments[end]!=0 || !args || !json_object_is_type(args,json_type_object);
  json_tokener_free(tok);if(args)json_object_put(args);if(invalid)return 1;
  event->kind=PG_MODEL_CALL;event->text=(pg_slice){m->arguments,strlen(m->arguments)};
  event->call_id=(pg_slice){m->id,strlen(m->id)};event->tool_name=(pg_slice){m->name,strlen(m->name)};
 }else event->kind=PG_MODEL_DONE;
 m->delivered=1;return 0;}
pg_model pg_mimo_model(pg_mimo *m){*m=(pg_mimo){0};return (pg_model){m,start,next,cancel_model,close_model};}

void pg_mimo_bind_tools(pg_mimo *m,const pg_tool_registry *tools){m->tools=tools;}
