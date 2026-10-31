#define _POSIX_C_SOURCE 200809L
#include "oauth.h"
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <stdlib.h>
#include <string.h>
struct pg_oauth_callback {struct event_base *base;struct evhttp *http;void *context;int (*accept)(void *,int,const char *);};
static void callback_request(struct evhttp_request *request,void *context){
 struct pg_oauth_callback *callback=context;int status=callback->accept(callback->context,evhttp_request_get_command(request)==EVHTTP_REQ_GET,evhttp_request_get_uri(request));struct evbuffer *body=evbuffer_new();if(!body){evhttp_send_error(request,500,"Allocation failed");return;}
 const char *message=status==200?"Authorization received. Return to PanGu.":status==404?"Callback route not found.":status==409?"This sign-in has already been handled.":"Authorization callback rejected.";
 evbuffer_add(body,message,strlen(message));evhttp_add_header(evhttp_request_get_output_headers(request),"Content-Type","text/plain; charset=utf-8");evhttp_add_header(evhttp_request_get_output_headers(request),"Cache-Control","no-store");evhttp_send_reply(request,status,0,body);evbuffer_free(body);
}
struct pg_oauth_callback *pg_oauth_callback_open(const char *host,unsigned port,void *context,int (*accept)(void *,int,const char *)){
 struct pg_oauth_callback *callback=calloc(1,sizeof(*callback));if(!callback)return 0;callback->context=context;callback->accept=accept;callback->base=event_base_new();if(!callback->base)goto failed;callback->http=evhttp_new(callback->base);if(!callback->http||!evhttp_bind_socket_with_handle(callback->http,host,(unsigned short)port))goto failed;evhttp_set_gencb(callback->http,callback_request,callback);return callback;
failed:pg_oauth_callback_close(callback);return 0;
}
void pg_oauth_callback_close(struct pg_oauth_callback *callback){if(!callback)return;if(callback->http)evhttp_free(callback->http);if(callback->base)event_base_free(callback->base);free(callback);}
int pg_oauth_callback_pump(struct pg_oauth_callback *callback){return callback?event_base_loop(callback->base,EVLOOP_NONBLOCK):0;}
int pg_oauth_callback_path(const char *input,const char *path){struct evhttp_uri *uri=evhttp_uri_parse(input);if(!uri)return 0;const char *actual=evhttp_uri_get_path(uri);int matches=actual&&!strcmp(actual,path);evhttp_uri_free(uri);return matches;}
/* Query and URL parsing use libevent; each output transfers an owned string. */
int pg_oauth_query_fields(const char *input,int url,char **output){
 for(unsigned i=0;i<3;i++)output[i]=0;struct evhttp_uri *uri=0;const char *query=input;int result=0;struct evkeyvalq fields={0};fields.tqh_last=&fields.tqh_first;
 if(url){uri=evhttp_uri_parse(input);if(!uri||!evhttp_uri_get_scheme(uri)){result=1;goto done;}query=evhttp_uri_get_query(uri);if(!query)goto done;}
 if(evhttp_parse_query_str(query,&fields)){result=5;goto done;}
 const char *names[]={"code","state","error"};for(unsigned i=0;i<3;i++){const char *value=evhttp_find_header(&fields,names[i]);if(value){output[i]=strdup(value);if(!output[i]){result=7;break;}}}
done:evhttp_clear_headers(&fields);if(uri)evhttp_uri_free(uri);if(result)for(unsigned i=0;i<3;i++){free(output[i]);output[i]=0;}return result;
}
