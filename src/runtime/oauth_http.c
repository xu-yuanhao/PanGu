#define _POSIX_C_SOURCE 200809L
#include "oauth.h"
#include <curl/curl.h>
#include <json-c/printbuf.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
struct pg_oauth_http {CURL *easy;CURLM *multi;struct curl_slist *headers;struct printbuf *body;char *form;int added,done;};
static size_t receive_token(char *data,size_t size,size_t count,void *context){
 if(size&&count>SIZE_MAX/size)return 0;size_t bytes=size*count;struct printbuf *buffer=context;
 if(bytes>INT_MAX||(size_t)buffer->bpos+bytes>INT_MAX-1)return 0;
 return printbuf_memappend(buffer,data,(int)bytes)<0?0:bytes;
}
double pg_oauth_realtime(void *context){(void)context;struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now))return -1;return (double)now.tv_sec*1000.0+(double)now.tv_nsec/1000000.0;}
void pg_oauth_http_close(struct pg_oauth_http *http){
 if(!http)return;if(http->added)curl_multi_remove_handle(http->multi,http->easy);curl_easy_cleanup(http->easy);if(http->multi)curl_multi_cleanup(http->multi);curl_slist_free_all(http->headers);if(http->body)printbuf_free(http->body);free(http->form);free(http);
}
int pg_oauth_http_start(struct pg_oauth_http **output,const char *url,const char *content_type,const char *form){
 *output=0;struct pg_oauth_http *http=calloc(1,sizeof(*http));if(!http)return 7;
 http->easy=curl_easy_init();http->multi=curl_multi_init();http->body=printbuf_new();http->form=strdup(form);http->headers=curl_slist_append(0,content_type);if(!http->easy||!http->multi||!http->body||!http->form||!http->headers)goto failed;
 if(curl_easy_setopt(http->easy,CURLOPT_URL,url)||curl_easy_setopt(http->easy,CURLOPT_PROTOCOLS_STR,"https,http")||curl_easy_setopt(http->easy,CURLOPT_HTTPHEADER,http->headers)||curl_easy_setopt(http->easy,CURLOPT_POSTFIELDS,http->form)||curl_easy_setopt(http->easy,CURLOPT_TIMEOUT_MS,15000L)||curl_easy_setopt(http->easy,CURLOPT_NOSIGNAL,1L)||curl_easy_setopt(http->easy,CURLOPT_WRITEFUNCTION,receive_token)||curl_easy_setopt(http->easy,CURLOPT_WRITEDATA,http->body))goto failed;
 if(curl_multi_add_handle(http->multi,http->easy))goto failed;http->added=1;*output=http;return 0;
failed:pg_oauth_http_close(http);return 7;
}
int pg_oauth_http_step(struct pg_oauth_http *http,long *status,const char **body){
 int running=0;if(!http||http->done)return 7;if(curl_multi_perform(http->multi,&running))return 7;if(running)return 1;
 int remaining=0;CURLMsg *message;while((message=curl_multi_info_read(http->multi,&remaining)))if(message->msg==CURLMSG_DONE&&message->easy_handle==http->easy){http->done=1;if(message->data.result||curl_easy_getinfo(http->easy,CURLINFO_RESPONSE_CODE,status)||memchr(http->body->buf,0,(size_t)http->body->bpos))return 7;*body=http->body->buf;return 0;}return 7;
}
int pg_oauth_post_form(const char *url,const char *form,char **output){
 *output=0;struct pg_oauth_http *http=0;int result=pg_oauth_http_start(&http,url,"Content-Type: application/x-www-form-urlencoded",form);if(result)return result;long status=0;const char *body=0;
 while((result=pg_oauth_http_step(http,&status,&body))==1){if(curl_multi_poll(http->multi,0,0,50,0)){result=7;break;}}
 if(!result){if(status<200||status>=300)result=7;else{*output=strdup(body);if(!*output)result=7;}}
 pg_oauth_http_close(http);return result;
}
