#define _POSIX_C_SOURCE 200809L
#include "oauth.h"
#include <curl/curl.h>
#include <json-c/printbuf.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
static size_t receive_token(char *data,size_t size,size_t count,void *context){
 if(size&&count>SIZE_MAX/size)return 0;size_t bytes=size*count;struct printbuf *buffer=context;
 if(bytes>INT_MAX||(size_t)buffer->bpos+bytes>INT_MAX-1)return 0;
 return printbuf_memappend(buffer,data,(int)bytes)<0?0:bytes;
}
double pg_oauth_realtime(void *context){(void)context;struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now))return -1;return (double)now.tv_sec*1000.0+(double)now.tv_nsec/1000000.0;}
int pg_oauth_post_form(const char *url,const char *form,char **output){
 *output=0;int result=7;CURL *easy=curl_easy_init();struct curl_slist *headers=0;struct printbuf *body=printbuf_new();if(!easy||!body)goto done;
 headers=curl_slist_append(0,"Content-Type: application/x-www-form-urlencoded");if(!headers)goto done;
 if(curl_easy_setopt(easy,CURLOPT_URL,url)||curl_easy_setopt(easy,CURLOPT_PROTOCOLS_STR,"https,http")||curl_easy_setopt(easy,CURLOPT_HTTPHEADER,headers)||curl_easy_setopt(easy,CURLOPT_POSTFIELDS,form)||curl_easy_setopt(easy,CURLOPT_TIMEOUT_MS,15000L)||curl_easy_setopt(easy,CURLOPT_NOSIGNAL,1L)||curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,receive_token)||curl_easy_setopt(easy,CURLOPT_WRITEDATA,body))goto done;
 if(curl_easy_perform(easy))goto done;long status=0;if(curl_easy_getinfo(easy,CURLINFO_RESPONSE_CODE,&status)||status<200||status>=300)goto done;
 if(memchr(body->buf,0,(size_t)body->bpos))goto done;
 *output=strdup(body->buf);if(*output)result=0;
done:
 curl_easy_cleanup(easy);curl_slist_free_all(headers);if(body)printbuf_free(body);return result;
}
