#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo.h"
#include "../../src/runtime/mimo_layout.h"
#include <curl/curl.h>
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 可控 curl 替身：perform/getinfo/info_read/add_handle/init 按脚本演出，
 * setopt 记录并转发真实 libcurl（校验变参 ABI 被真实现接受），
 * cleanup 计数用于验证 close 的释放恰好一次。无任何真实网络请求。 */

extern CURL *__real_curl_easy_init(void);
extern CURLcode __real_curl_easy_setopt(CURL *, CURLoption, ...);
extern void __real_curl_easy_cleanup(CURL *);
extern CURLM *__real_curl_multi_init(void);
extern CURLMcode __real_curl_multi_cleanup(CURLM *);
extern void __real_curl_slist_free_all(struct curl_slist *);
extern struct curl_slist *__real_curl_slist_append(struct curl_slist *,const char *);

static int script_perform_rc, script_running = 1;
static long script_http = 200;
enum { INFO_OK, INFO_NULL, INFO_NOT_DONE, INFO_RESULT_ERROR } script_info = INFO_OK;
static int script_add_rc, script_easy_fail, script_multi_fail;
static int count_easy_cleanup, count_multi_cleanup, count_slist_free;

static size_t (*captured_write)(char *, size_t, size_t, void *);
static void *captured_write_data;
static const char *captured_url, *captured_post, *captured_protocols;
static struct curl_slist *captured_headers;
static long captured_timeout = -1, captured_connect_timeout = -1, captured_nosignal = -1;
static int token_fail_at,token_calls;
struct json_tokener *__real_json_tokener_new(void);
struct json_tokener *__wrap_json_tokener_new(void){++token_calls;return token_fail_at==token_calls?0:__real_json_tokener_new();}
static int setopt_errors,setopt_fail_at,setopt_calls,getinfo_fail,slist_fail_at,slist_calls;
struct curl_slist *__wrap_curl_slist_append(struct curl_slist *list,const char *text){++slist_calls;return slist_fail_at==slist_calls?NULL:__real_curl_slist_append(list,text);}


CURL *__wrap_curl_easy_init(void) { return script_easy_fail ? NULL : __real_curl_easy_init(); }

CURLM *__wrap_curl_multi_init(void) { return script_multi_fail ? NULL : __real_curl_multi_init(); }

CURLcode __wrap_curl_easy_setopt(CURL *h, CURLoption opt, ...) {
    ++setopt_calls;
    if(setopt_fail_at==setopt_calls)return CURLE_UNKNOWN_OPTION;
    va_list ap;
    va_start(ap, opt);
    CURLcode rc = CURLE_OK;
    switch (opt) {
    case CURLOPT_URL: {
        const char *v = va_arg(ap, const char *);
        captured_url = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_PROTOCOLS_STR: {
        const char *v = va_arg(ap, const char *);
        captured_protocols = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_POSTFIELDS: {
        const char *v = va_arg(ap, const char *);
        captured_post = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_HTTPHEADER: {
        struct curl_slist *v = va_arg(ap, struct curl_slist *);
        captured_headers = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_WRITEFUNCTION: {
        size_t (*v)(char *, size_t, size_t, void *) = va_arg(ap, size_t (*)(char *, size_t, size_t, void *));
        captured_write = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_WRITEDATA: {
        void *v = va_arg(ap, void *);
        captured_write_data = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_TIMEOUT:
    case CURLOPT_CONNECTTIMEOUT:
    case CURLOPT_NOSIGNAL: {
        long v = va_arg(ap, long);
        if (opt == CURLOPT_TIMEOUT) captured_timeout = v;
        if (opt == CURLOPT_CONNECTTIMEOUT) captured_connect_timeout = v;
        if (opt == CURLOPT_NOSIGNAL) captured_nosignal = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    default:
        rc = CURLE_OK; /* 未记录选项按 long 转发无从谈起：本产品固定 9 项 */
        break;
    }
    va_end(ap);
    setopt_errors += rc != CURLE_OK;
    return rc;
}

CURLMcode __wrap_curl_multi_perform(CURLM *multi, int *running) {
    (void)multi;
    *running = script_running;
    return (CURLMcode)script_perform_rc;
}

CURLcode __wrap_curl_easy_getinfo(CURL *easy, CURLINFO info, ...) {
    (void)easy;
    assert(info == CURLINFO_RESPONSE_CODE);
    if(getinfo_fail)return CURLE_BAD_FUNCTION_ARGUMENT;
    va_list ap;
    va_start(ap, info);
    long *out = va_arg(ap, long *);
    va_end(ap);
    *out = script_http;
    return CURLE_OK;
}

CURLMsg *__wrap_curl_multi_info_read(CURLM *multi, int *left) {
    (void)multi;
    static CURLMsg msg;
    *left = 0;
    if (script_info == INFO_NULL) return NULL;
    msg.msg = script_info == INFO_NOT_DONE ? CURLMSG_NONE : CURLMSG_DONE;
    msg.easy_handle = NULL;
    msg.data.result = script_info == INFO_RESULT_ERROR ? CURLE_WRITE_ERROR : CURLE_OK;
    return &msg;
}

CURLMcode __wrap_curl_multi_add_handle(CURLM *multi, CURL *easy) {
    (void)multi;
    (void)easy;
    return (CURLMcode)script_add_rc;
}

void __wrap_curl_easy_cleanup(CURL *c) {
    count_easy_cleanup++;
    __real_curl_easy_cleanup(c);
}

CURLMcode __wrap_curl_multi_cleanup(CURLM *m) {
    count_multi_cleanup++;
    return __real_curl_multi_cleanup(m);
}

void __wrap_curl_slist_free_all(struct curl_slist *l) {
    if (l)
        count_slist_free++; /* close 语义允许对 NULL 无条件调用，仅计真实释放 */
    __real_curl_slist_free_all(l);
}

static pg_mimo m;
static pg_model model;
static pg_session session;

static void reset_script(void) {
    token_fail_at=token_calls=0;
    script_perform_rc = 0;
    script_running = 1;
    script_http = 200;
    script_info = INFO_OK;
    script_add_rc = 0;
    script_easy_fail = 0;
    script_multi_fail = 0;
    count_easy_cleanup = count_multi_cleanup = count_slist_free = 0;
    setopt_errors = 0;setopt_calls=setopt_fail_at=getinfo_fail=slist_fail_at=slist_calls=0;
    captured_write = NULL;
    captured_write_data = NULL;
    captured_url = captured_post = captured_protocols = NULL;
    captured_headers = NULL;
    captured_timeout = captured_connect_timeout = captured_nosignal = -1;
}

static void make_session(void) {
    session = (pg_session){0};
    session.count = 1;
    session.messages[0].role = PG_USER;
    session.messages[0].size = 5;
    memcpy(session.messages[0].text, "hello", 6);
}

static int start(void) { return model.start(model.context, &session); }
static int nxt(pg_model_event *e) { return model.next(model.context, e); }
static void feed(const char *data) {
    size_t n = strlen(data);
    assert(captured_write((char *)data, 1, n, captured_write_data) == n);
}
static void finish_transport(void) {
    script_running = 0;
    script_info = INFO_OK;
}

static void test_start_setup(void) {
    reset_script();
    m = (pg_mimo){0};
    model = pg_mimo_model(&m);
    unsetenv("MIMO_API_KEY");
    assert(start() == 1);
    assert(!m.easy && !m.multi && !m.request);
    model.close(model.context);
    assert(count_easy_cleanup == 0 && count_multi_cleanup == 0);
    reset_script();
    setenv("MIMO_API_KEY", "transport-unit-key", 1);
    make_session();
    assert(start() == 0);
    assert(setopt_errors == 0);
    assert(captured_url && !strcmp(captured_url, "https://token-plan-cn.xiaomimimo.com/v1/chat/completions"));
    assert(captured_post == m.request && m.request);
    assert(captured_protocols && !strcmp(captured_protocols, "https"));
    assert(captured_write && captured_write_data == &m);
    assert(captured_headers == m.headers && m.headers);
    assert(captured_timeout == 60 && captured_connect_timeout == 15 && captured_nosignal == 1);
    /* 请求体是本次投影生成的 JSON */
    assert(strstr(m.request, "\"messages\"") && strstr(m.request, "\"stream\":true"));
    /* 再次 start：close 先释放上一请求，再生成新请求（同尺寸复用同一地址是正常的） */
    char *first = m.request;
    assert(start() == 0);
    assert(m.request && strstr(m.request, "\"messages\""));
    (void)first;
    model.close(model.context);
    assert(!m.easy&&!m.multi&&!m.headers&&!m.request);
    assert(count_easy_cleanup==2&&count_multi_cleanup==2&&count_slist_free==2);
}

static void test_bound_tools_survive_request_cleanup(void){
 reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();assert(!setenv("MIMO_API_KEY","transport-unit-key",1));
 pg_tool_registry registry={.entries={{.name="write",.description="Write a file",.schema="{\"type\":\"object\"}"}},.count=1};pg_mimo_bind_tools(&m,&registry);
 for(unsigned attempt=0;attempt<2;attempt++){assert(!start()&&m.tools==&registry);json_object *wire=json_tokener_parse(captured_post);assert(wire);json_object *tools=json_object_object_get(wire,"tools");assert(json_object_is_type(tools,json_type_array)&&json_object_array_length(tools)==1&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(json_object_array_get_idx(tools,0),"function"),"name")),"write"));json_object_put(wire);model.close(model.context);assert(m.tools==&registry);}
 pg_mimo_bind_tools(&m,0);
}

static void test_text_stream(void) {
    pg_model_event e;
    reset_script();
    m = (pg_mimo){0};
    model = pg_mimo_model(&m);
    setenv("MIMO_API_KEY", "transport-unit-key", 1);
    make_session();
    assert(start() == 0);
    /* 行跨接收块拆分：前半无换行 → WAIT；补齐换行 → TEXT（拆在 content 串中间） */
    feed("data: {\"choices\":[{\"delta\":{\"content\":\"he");
    script_running = 1;
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    feed("llo\"}}]}\n");
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_TEXT && e.text.size == 5 && !memcmp(e.text.data, "hello", 5));
    /* CRLF 剥离与空行 */
    feed("\r\n");
    feed("data: {\"choices\":[{\"delta\":{\"content\":\"!\"}}]}\r\n");
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT); /* 先消费空行 */
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_TEXT && e.text.size == 1);
    /* usage 空分片 + 终态次序 */
    feed("data: {\"choices\":[]}\n");
    feed("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n");
    feed("data: [DONE]\n\n");
    finish_transport();
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT); /* usage 分片 */
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT); /* stop 行 */
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT); /* [DONE] 行本身仍为 WAIT */
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT); /* 末尾空行 */
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_DONE);
    assert(m.delivered);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_DONE); /* 终态幂等 */
    model.close(model.context);
    assert(!m.easy && !m.multi && !m.headers && !m.request);
    assert(count_easy_cleanup == 1 && count_multi_cleanup == 1 && count_slist_free == 1);
    model.close(model.context); /* 重复 close 不双重释放 */
    assert(count_easy_cleanup == 1 && count_multi_cleanup == 1 && count_slist_free == 1);
}

static void test_tool_stream(void) {
    pg_model_event e;
    reset_script();
    m = (pg_mimo){0};
    model = pg_mimo_model(&m);
    setenv("MIMO_API_KEY", "transport-unit-key", 1);
    make_session();
    assert(start() == 0);
    feed("data: {\"choices\":[{\"delta\":{\"content\":\"checking\"}}]}\n");
    feed("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call-7\",\"function\":{\"name\":\"read\"}}]}}]}\n");
    feed("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{\\\"path\\\":\"}}]}}]}\n");
    feed("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\\\"/tmp\\\"}\"}}]}}]}\n");
    feed("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n");
    feed("data: [DONE]\n");
    finish_transport();
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_TEXT);
    /* CALL 只在协议完整确认（finish+DONE+transport 终结）后交给 Loop */
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT); /* [DONE] 行 */
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_CALL);
    assert(!memcmp(e.call_id.data, "call-7", e.call_id.size) && e.call_id.size == 6);
    assert(!memcmp(e.tool_name.data, "read", e.tool_name.size) && e.tool_name.size == 4);
    assert(!memcmp(e.text.data, "{\"path\":\"/tmp\"}", e.text.size) && e.text.size == 15);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_DONE);
    model.close(model.context);
}

static void test_transport_failures(void) {
    pg_model_event e;
    reset_script();
    m = (pg_mimo){0};
    model = pg_mimo_model(&m);
    setenv("MIMO_API_KEY", "transport-unit-key", 1);
    make_session();

    /* multi_perform 错误 */
    assert(start() == 0);
    script_perform_rc = CURLM_INTERNAL_ERROR;
    assert(nxt(&e) == 1);
    model.close(model.context);

    /* HTTP 非 200 */
    reset_script();
    assert(start() == 0);
    script_http = 500;
    assert(!nxt(&e)&&e.kind==PG_MODEL_WAIT);
    finish_transport();
    assert(nxt(&e) == 1);
    model.close(model.context);

    /* 传输完成但无消息 / 非终态消息 / 错误码 */
    reset_script();
    assert(start() == 0);
    finish_transport();
    script_info = INFO_NULL;
    assert(nxt(&e) == 1);
    model.close(model.context);

    reset_script();
    assert(start() == 0);
    finish_transport();
    script_info = INFO_NOT_DONE;
    assert(nxt(&e) == 1);
    model.close(model.context);

    reset_script();
    assert(start() == 0);
    finish_transport();
    script_info = INFO_RESULT_ERROR;
    assert(nxt(&e) == 1);
    model.close(model.context);

    /* 传输结束但末行不完整：截断拒绝 */
    reset_script();
    assert(start() == 0);
    script_running = 1;
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    feed("data: {\"choices\":[{\"delta\":{");
    finish_transport();
    assert(nxt(&e) == 1);
    model.close(model.context);

    /* 协议完成但工具参数非法 JSON：终态拒绝 */
    reset_script();
    assert(start() == 0);
    feed("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c\",\"function\":{\"name\":\"read\",\"arguments\":\"{\"}}]}}]}\n");
    feed("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n");
    feed("data: [DONE]\n");
    finish_transport();
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT); /* [DONE] 行 */
    assert(nxt(&e) == 1);                            /* 参数 "{" 非 object 终态拒绝 */
    model.close(model.context);

    /* 协议完成但缺 [DONE]：终态拒绝 */
    reset_script();
    assert(start() == 0);
    feed("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n");
    finish_transport();
    assert(nxt(&e) == 0 && e.kind == PG_MODEL_WAIT);
    assert(nxt(&e) == 1);
    model.close(model.context);
}

static void test_receive_bounds(void) {
    reset_script();
    m = (pg_mimo){0};
    model = pg_mimo_model(&m);
    setenv("MIMO_API_KEY", "transport-unit-key", 1);
    make_session();
    assert(start() == 0);
    static char blob[PG_MM_BODY_LIMIT];
    memset(blob, 'x', sizeof blob);
    /* 乘法溢出拒绝 */
    assert(captured_write(blob, 4, (SIZE_MAX / 4) + 1, captured_write_data) == 0);
    assert(captured_write(blob, 2, SIZE_MAX / 2 + 2, captured_write_data) == 0);
    /* size==0：n=0 记账但不推进 */
    size_t used = m.used;
    assert(captured_write(blob, 0, 7, captured_write_data) == 0);
    assert(m.used == used);
    const char raw_nul[]={'x',0,'y'};assert(captured_write((char*)raw_nul,1,sizeof(raw_nul),captured_write_data)==0&&m.used==used);
    /* 容量边界：131070 满后 2 字节拒绝、1 字节接受、再 1 字节拒绝 */
    assert(captured_write(blob, 1, PG_MM_BODY_LIMIT - 2, captured_write_data) == PG_MM_BODY_LIMIT - 2);
    assert(captured_write(blob, 1, 2, captured_write_data) == 0);
    assert(captured_write(blob, 1, 1, captured_write_data) == 1);
    assert(m.used == PG_MM_BODY_LIMIT - 1);
    assert(m.body[m.used] == 0);
    assert(captured_write(blob, 1, 1, captured_write_data) == 0); /* 最后 1 字节留给 NUL */
    m.consumed=m.used-3;memcpy(m.body+m.consumed,"abc",3);char tail[]="wxyz";assert(captured_write(tail,1,4,&m)==4&&m.used==7&&!m.consumed&&!memcmp(m.body,"abcwxyz",8));
    model.close(model.context);
}

static void test_partial_init_close(void) {
    reset_script();
    m = (pg_mimo){0};
    model = pg_mimo_model(&m);
    setenv("MIMO_API_KEY", "transport-unit-key", 1);
    make_session();
    /* add_handle 失败：start=1，close 全量释放恰好一次 */
    script_add_rc = CURLM_BAD_HANDLE;
    assert(start() == 1);
    assert(m.easy && m.multi && m.headers && m.request);
    model.close(model.context);
    assert(count_easy_cleanup == 1 && count_multi_cleanup == 1 && count_slist_free == 1);
    model.close(model.context);
    assert(count_easy_cleanup == 1 && count_multi_cleanup == 1 && count_slist_free == 1);

    /* easy 初始化失败：半初始化（multi/headers/request 已建）也可安全关闭 */
    reset_script();
    script_easy_fail = 1;
    assert(start() == 1);
    assert(!m.easy && m.multi && m.headers && m.request);
    model.close(model.context);
    assert(count_easy_cleanup == 0 && count_multi_cleanup == 1 && count_slist_free == 1);

    /* multi 初始化失败：easy 已建，同样安全关闭 */
    reset_script();
    script_multi_fail = 1;
    assert(start() == 1);
    assert(m.easy && !m.multi && m.headers && m.request);
    model.close(model.context);
    assert(count_easy_cleanup == 1 && count_multi_cleanup == 0 && count_slist_free == 1);

    /* 取消即关闭：活动请求的 easy/multi/header/request 全部释放 */
    reset_script();
    script_easy_fail = 0;
    assert(start() == 0);
    model.cancel(model.context);
    assert(!m.easy && !m.multi && !m.headers && !m.request);
    assert(count_easy_cleanup == 1 && count_multi_cleanup == 1 && count_slist_free == 1);
}

static void test_setup_failures(void){
 for(int option=1;option<=9;++option){reset_script();setopt_fail_at=option;assert(start()==1);model.close(model.context);assert(count_easy_cleanup==1&&count_multi_cleanup==1&&count_slist_free==1);}
 for(int append=1;append<=2;++append){reset_script();slist_fail_at=append;assert(start()==1);model.close(model.context);assert(count_easy_cleanup==0&&count_multi_cleanup==0&&count_slist_free==(append==2));}
 reset_script();assert(start()==0);getinfo_fail=1;pg_model_event event;assert(nxt(&event)==1);model.close(model.context);
}

static void batch_begin(void){reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);setenv("MIMO_API_KEY","transport-unit-key",1);make_session();assert(!start());}
static void feed_call(unsigned index,const char *id,const char *name,const char *args){json_object *o=json_object_new_object(),*choices=json_object_new_array(),*choice=json_object_new_object(),*delta=json_object_new_object(),*calls=json_object_new_array(),*call=json_object_new_object(),*fn=json_object_new_object();assert(o&&choices&&choice&&delta&&calls&&call&&fn);assert(!json_object_object_add(call,"index",json_object_new_int64(index)));if(id)assert(!json_object_object_add(call,"id",json_object_new_string(id)));if(name)assert(!json_object_object_add(fn,"name",json_object_new_string(name)));if(args)assert(!json_object_object_add(fn,"arguments",json_object_new_string(args)));assert(!json_object_object_add(call,"function",fn));assert(!json_object_array_add(calls,call));assert(!json_object_object_add(delta,"tool_calls",calls));assert(!json_object_object_add(choice,"delta",delta));assert(!json_object_array_add(choices,choice));assert(!json_object_object_add(o,"choices",choices));char line[32768];int n=snprintf(line,sizeof(line),"data: %s\n",json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN));assert(n>0&&(size_t)n<sizeof(line));feed(line);json_object_put(o);}
static void batch_end(void){feed("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n");feed("data: [DONE]\n");}
static void waiting_lines(unsigned count){pg_model_event e;for(unsigned i=0;i<count;++i)assert(!nxt(&e)&&e.kind==PG_MODEL_WAIT&&!e.batch.count);}
static void test_large_write_arguments(void){pg_model_event e;reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);setenv("MIMO_API_KEY","transport-unit-key",1);make_session();assert(!start());const char *prefix="{\"path\":\"large-write.bin\",\"content\":\"";const char *suffix="\"}";char content[6001];memset(content,'L',6000);content[6000]=0;feed_call(0,"large-write","write",prefix);feed_call(0,0,0,content);feed_call(0,0,0,suffix);batch_end();waiting_lines(5);assert(!nxt(&e)&&e.kind==PG_MODEL_WAIT);finish_transport();assert(!nxt(&e)&&e.kind==PG_MODEL_CALL);assert(e.text.data==m.argument_overflow[0]&&e.text.size==strlen(e.text.data)&&e.text.size>PG_MM_TEXT_LIMIT);json_object *arguments=json_tokener_parse(e.text.data),*written=0;assert(arguments&&json_object_object_get_ex(arguments,"content",&written)&&json_object_get_string_len(written)==6000);for(size_t i=0;i<6000;++i)assert(json_object_get_string(written)[i]=='L');json_object_put(arguments);assert(!start()&&!m.argument_overflow[0]&&!m.argument_length[0]&&!m.argument_capacity[0]);model.close(model.context);assert(!m.argument_overflow[0]&&!m.calls[0].arguments.data);}
static void test_batch_stream(void){pg_model_event e;batch_begin();
 feed_call(1,"second","write","{\"n\":");feed_call(0,"first","read","{}");feed_call(1,0,0,"2}");batch_end();waiting_lines(5);assert(!nxt(&e)&&e.kind==PG_MODEL_WAIT);finish_transport();assert(!nxt(&e)&&e.kind==PG_MODEL_BATCH&&e.batch.count==2&&e.batch.calls==m.calls);assert(!strcmp(e.batch.calls[0].call_id.data,"first")&&!strcmp(e.batch.calls[1].call_id.data,"second")&&e.batch.calls[1].arguments.size==7&&!memcmp(e.batch.calls[1].arguments.data,"{\"n\":2}",7));assert(!e.call_id.data&&!e.tool_name.data);assert(!nxt(&e)&&e.kind==PG_MODEL_DONE);model.close(model.context);
 assert(!start()&&!m.call_count);for(size_t i=0;i<sizeof(m.extra);++i)assert(!((unsigned char*)m.extra)[i]);for(size_t i=0;i<sizeof(m.calls);++i)assert(!((unsigned char*)m.calls)[i]);model.close(model.context);
 batch_begin();feed("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"a\",\"function\":{\"name\":\"read\",\"arguments\":\"{}\"}},{\"index\":1,\"id\":\"b\",\"function\":{\"name\":\"write\",\"arguments\":\"{}\"}}]}}]}\n");batch_end();waiting_lines(3);finish_transport();assert(!nxt(&e)&&e.kind==PG_MODEL_BATCH&&e.batch.count==2);model.close(model.context);
 for(unsigned fault=0;fault<=8;++fault){batch_begin();for(unsigned index=0;index<8;++index){char id[16];snprintf(id,sizeof(id),"call-%u",index);feed_call(index,id,"read","{}");}batch_end();waiting_lines(10);token_calls=0;token_fail_at=(int)fault;finish_transport();int rc=nxt(&e);if(fault){assert(rc==1&&!m.delivered&&m.failed&&e.kind==PG_MODEL_WAIT&&!e.batch.calls);assert(nxt(&e)==1&&!m.delivered);}else{assert(!rc&&e.kind==PG_MODEL_BATCH&&e.batch.count==8);for(unsigned j=0;j<8;++j){char id[16];snprintf(id,sizeof(id),"call-%u",j);assert(!strcmp(e.batch.calls[j].call_id.data,id)&&e.batch.calls[j].arguments.size==2&&!memcmp(e.batch.calls[j].arguments.data,"{}",2));}}model.close(model.context);}
 for(unsigned overflow=0;overflow<3;++overflow){batch_begin();char id[64],name[64],args[4096];memset(id,'i',63);id[63]=0;memset(name,'n',63);name[63]=0;memcpy(args,"{\"x\":\"",6);memset(args+6,'z',4087);memcpy(args+4093,"\"}",3);assert(strlen(args)==4095);
  for(unsigned index=0;index<8;++index){id[62]=(char)('0'+index);feed_call(index,id,name,args);}waiting_lines(8);
  if(overflow){feed_call(7,overflow==1?"x":0,overflow==2?"x":0,0);assert(nxt(&e)==1&&!m.delivered&&m.failed&&e.kind==PG_MODEL_WAIT);for(size_t byte=0;byte<sizeof(m.calls);++byte)assert(!((unsigned char*)m.calls)[byte]);}
  else{batch_end();waiting_lines(2);finish_transport();assert(!nxt(&e)&&e.kind==PG_MODEL_BATCH&&e.batch.count==8);for(unsigned index=0;index<8;++index){assert(e.batch.calls[index].arguments.size==4095&&e.batch.calls[index].call_id.size==63&&e.batch.calls[index].tool_name.size==63);assert(!memcmp(e.batch.calls[index].arguments.data,args,4095));}}
  model.close(model.context);
 }

}
static void test_batch_rejection(void){pg_model_event e;
 for(unsigned bad=0;bad<6;++bad){batch_begin();feed_call(0,"a","read","{}");feed_call(bad==2?2:1,bad==0?"a":"b",bad==1?0:"write",bad==3?"{":bad==4?"[]":"{}");if(bad==5)feed_call(8,"ninth","read","{}");batch_end();finish_transport();int rejected=0;for(unsigned i=0;i<10;++i){int rc=nxt(&e);assert(e.kind!=PG_MODEL_BATCH&&e.kind!=PG_MODEL_CALL);if(rc){rejected=1;break;}}assert(rejected&&!m.delivered&&m.failed&&nxt(&e)==1);model.close(model.context);}
 batch_begin();feed_call(0,"a","read","{}");feed_call(1,"b","write","{}");feed("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n");finish_transport();waiting_lines(3);assert(nxt(&e)==1&&!m.delivered);model.close(model.context);
}
static void test_injected_connection(void){
 reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();
 assert(!setenv("PANGU_MODEL_ENDPOINT","https://api.deepseek.com/chat/completions",1));
 assert(!setenv("PANGU_MODEL_NAME","deepseek-flash",1));
 unsetenv("PANGU_MODEL_API_KEY");assert(start()==1&&!m.easy&&!m.multi);
 assert(!setenv("PANGU_MODEL_API_KEY","deepseek-transport-unit-key",1));
 assert(!start());assert(!strcmp(captured_url,"https://api.deepseek.com/chat/completions"));
 assert(strstr(captured_post,"\"model\":\"deepseek-flash\""));
 unsigned key_found=0;for(const struct curl_slist *header=captured_headers;header;header=header->next)if(strstr(header->data,"deepseek-transport-unit-key"))key_found++;assert(key_found==1);
 model.close(model.context);assert(!m.easy&&!m.multi&&!m.headers&&!m.request);
 unsetenv("PANGU_MODEL_ENDPOINT");unsetenv("PANGU_MODEL_NAME");unsetenv("PANGU_MODEL_API_KEY");
}
static void test_saved_connection(void){
 reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();
 struct json_object *configuration=json_tokener_parse("{\"model\":\"saved-model\",\"endpoint\":\"https://saved.invalid/chat/completions\",\"options\":{\"maxTokens\":768,\"temperature\":0.25}}");assert(configuration&&!pg_mimo_configure(&m,configuration));json_object_put(configuration);
 assert(!setenv("PANGU_MODEL_ENDPOINT","https://ambient.invalid",1)&&!setenv("PANGU_MODEL_NAME","ambient-model",1));assert(!setenv("PANGU_MODEL_API_KEY","configured-transport-unit-key",1));
 assert(!start()&&!strcmp(captured_url,"https://saved.invalid/chat/completions"));assert(strstr(captured_post,"\"model\":\"saved-model\"")&&strstr(captured_post,"\"max_tokens\":768")&&strstr(captured_post,"\"temperature\":0.25"));assert(m.configuration);
 configuration=json_tokener_parse("{\"model\":\"blocked\",\"endpoint\":\"https://blocked.invalid\"}");assert(pg_mimo_configure(&m,configuration));json_object_put(configuration);model.close(model.context);assert(!m.configuration&&!m.request&&!m.easy&&!m.multi);
 unsetenv("PANGU_MODEL_ENDPOINT");unsetenv("PANGU_MODEL_NAME");unsetenv("PANGU_MODEL_API_KEY");
}


static void test_responses_transport(void){
 for(unsigned scenario=0;scenario<5;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *configuration=json_tokener_parse("{\"model\":\"native-responses\",\"api\":\"openai-responses\",\"endpoint\":\"https://provider.invalid/v1/responses\"}");assert(!pg_mimo_configure(&m,configuration));json_object_put(configuration);setenv("PANGU_MODEL_API_KEY","responses-transport-unit-key",1);assert(!start()&&strstr(captured_post,"\"input\"")&&!strstr(captured_post,"\"messages\"")&&!strcmp(captured_url,"https://provider.invalid/v1/responses"));pg_model_event event={0};
  feed("event: response.created\r\ndata: {\"type\":\"response.created\",\"response\":{\"id\":\"resp_test\"}}\r\n\r\n");
  feed("data: {\"type\":\"response.output_text.delta\",\"output_index\":0,\"delta\":\"he");for(unsigned i=0;i<4;i++)assert(!nxt(&event));feed("llo\"}\n");assert(!nxt(&event)&&event.kind==PG_MODEL_TEXT&&event.text.size==5&&!memcmp(event.text.data,"hello",5));
  if(scenario==1||scenario==4){feed("data: {\"type\":\"response.output_item.added\",\"output_index\":4,\"item\":{\"type\":\"function_call\",\"call_id\":\"call-r\",\"name\":\"read\",\"arguments\":\"\"}}\n");feed("data: {\"type\":\"response.function_call_arguments.done\",\"output_index\":4,\"arguments\":\"{\\\"path\\\":\\\"file.txt\\\"}\"}\n");if(scenario==1)feed("data: {\"type\":\"response.output_item.done\",\"output_index\":4,\"item\":{\"type\":\"function_call\",\"id\":\"fc_r\",\"call_id\":\"call-r\",\"name\":\"read\",\"arguments\":\"{\\\"path\\\":\\\"file.txt\\\"}\"}}\n");}
  if(scenario<2||scenario==4)feed("data: {\"type\":\"response.completed\",\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":3,\"output_tokens\":2,\"total_tokens\":5}}}\n\n");
  if(scenario==3)feed("data: {\"type\":\"response.failed\",\"response\":{\"status\":\"failed\",\"error\":{\"message\":\"provider failure\"}}}\n");
  finish_transport();int status=0;for(unsigned i=0;i<12;i++){status=nxt(&event);if(status||event.kind!=PG_MODEL_WAIT)break;}if(scenario<2){assert(!status&&event.kind==(scenario?PG_MODEL_CALL:PG_MODEL_DONE)&&m.saw_done);if(scenario)assert(event.text.size==19&&!memcmp(event.text.data,"{\"path\":\"file.txt\"}",19));}else assert(status==1&&!m.saw_done);model.close(model.context);assert(!m.request&&!m.easy&&!m.configuration);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Responses actual model-service curl replay, split SSE/CRLF, text/tool terminal, early EOF and provider failure");
}

static void test_responses_arguments(void){
 const char *arguments[]={"{\"path\":\"a\\q\"}","{\"path\":\"file\",\"mode\":.5}",""};
 for(unsigned i=0;i<3;i++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-responses\",\"model\":\"test\",\"endpoint\":\"https://example.invalid/responses\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","unit-key",1);assert(!start());
  feed("data: {\"type\":\"response.output_item.added\",\"output_index\":0,\"item\":{\"type\":\"function_call\",\"id\":\"fc_args\",\"call_id\":\"args\",\"name\":\"read\",\"arguments\":\"\"}}\n");json_object *done=json_tokener_parse("{\"type\":\"response.output_item.done\",\"output_index\":0,\"item\":{\"type\":\"function_call\",\"id\":\"fc_args\",\"call_id\":\"args\",\"name\":\"read\"}}");json_object_object_add(json_object_object_get(done,"item"),"arguments",json_object_new_string(arguments[i]));const char *json=json_object_to_json_string_ext(done,JSON_C_TO_STRING_PLAIN);size_t size=strlen(json);char *frame=calloc(1,size+8);assert(frame);memcpy(frame,"data: ",6);memcpy(frame+6,json,size);frame[size+6]='\n';feed(frame);free(frame);json_object_put(done);feed("data: {\"type\":\"response.completed\",\"response\":{\"status\":\"completed\"}}\n");finish_transport();pg_model_event event={0};for(unsigned n=0;n<10;n++){assert(!nxt(&event));if(event.kind!=PG_MODEL_WAIT)break;}assert(event.kind==PG_MODEL_CALL&&m.prepared_arguments[0]);json_object *prepared=json_tokener_parse(event.text.data),*assistant=json_tokener_parse("{\"content\":[]}");assert(prepared&&!pg_mimo_complete(&m,assistant));json_object *call=json_object_array_get_idx(json_object_object_get(assistant,"content"),0);assert(json_object_equal(prepared,json_object_object_get(call,"arguments")));if(i==1)assert(json_object_object_get(call,"argumentsError")&&!json_object_object_length(prepared));else assert(!json_object_object_get(call,"argumentsError"));if(i==0)assert(!strcmp(json_object_get_string(json_object_object_get(prepared,"path")),"a\\q"));json_object_put(prepared);json_object_put(assistant);model.close(model.context);assert(!m.prepared_arguments[0]);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Responses repaired/invalid/empty final arguments travel through model CALL, completion metadata and owned cleanup");
}

static void test_anthropic_transport(void){
 for(unsigned scenario=0;scenario<7;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"anthropic-messages\",\"model\":\"claude-test\",\"endpoint\":\"https://example.invalid/v1/messages\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","anthropic-transport-unit-key",1);assert(!start());
  unsigned auth=0,version=0;for(const struct curl_slist *h=captured_headers;h;h=h->next){auth+=!strcmp(h->data,"x-api-key: anthropic-transport-unit-key");version+=!strcmp(h->data,"anthropic-version: 2023-06-01");assert(!strstr(h->data,"Authorization:"));}assert(auth==1&&version==1&&strstr(captured_post,"\"messages\"")&&!strstr(captured_post,"stream_options"));
  feed("event: message_start\r\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg-unit\",\"usage\":{\"input_tokens\":4}}}\r\n\r\n");
  if(scenario==0){feed("data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n");feed("data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"hello\"}}\n");}
  else if(scenario!=4){feed("data: {\"type\":\"content_block_start\",\"index\":9,\"content_block\":{\"type\":\"tool_use\",\"id\":\"tool-unit\",\"name\":\"read\",\"input\":{}}}\n");feed(scenario==2?"data: {\"type\":\"content_block_delta\",\"index\":9,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{bad\"}}\n":"data: {\"type\":\"content_block_delta\",\"index\":9,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"path\\\":\\\"file.txt\\\"}\"}}\n");}
  if(scenario<3||scenario==6)feed(scenario?"data: {\"type\":\"content_block_stop\",\"index\":9}\n":"data: {\"type\":\"content_block_stop\",\"index\":0}\n");
  if(scenario==4)feed("data: {\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"provider busy\"}}\n");
  else if(scenario!=5){feed(scenario==6?"data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"max_tokens\"},\"usage\":{\"output_tokens\":2}}\n":scenario?"data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":2}}\n":"data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":2}}\n");feed("data: {\"type\":\"message_stop\"}\n");}
  finish_transport();pg_model_event event={0};int status=0;unsigned texts=0;for(unsigned n=0;n<12;n++){status=nxt(&event);if(status)break;if(event.kind==PG_MODEL_TEXT){texts++;continue;}if(event.kind!=PG_MODEL_WAIT)break;}
  if(scenario<3||scenario==6){assert(!status&&m.saw_done&&event.kind==(scenario&&scenario!=6?PG_MODEL_CALL:PG_MODEL_DONE));if(!scenario)assert(texts==1);if(scenario==6)assert(m.finish_reason==3);json_object *assistant=json_tokener_parse("{\"content\":[]}");assert(!pg_mimo_complete(&m,assistant));if(scenario==2){json_object *call=json_object_array_get_idx(json_object_object_get(assistant,"content"),0);assert(json_object_object_get(call,"argumentsError")&&event.text.size==2&&!memcmp(event.text.data,"{}",2));}json_object_put(assistant);}
  else{assert(status&&!m.saw_done);if(scenario==4){pg_slice message={0},body={0};assert(!pg_mimo_failure_data(&m,&message,&body)&&strstr(message.data,"provider busy")&&strstr(body.data,"overloaded_error"));}}
  model.close(model.context);assert(!m.anthropic_blocks&&!m.anthropic_usage&&!m.headers&&!m.request);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Anthropic model-service replay with API-key/version headers, text/tool/errors, invalid arguments, max_tokens and early EOF");
}

static void test_anthropic_beta_headers(void){
 const char *options[]={"{\"reasoningEffort\":\"low\",\"maxTokens\":4096}","{\"reasoningEffort\":\"low\",\"headers\":{\"AnThRoPiC-BeTa\":\"custom-feature\"}}","{\"reasoningEffort\":\"low\",\"headers\":{\"anthropic-beta\":null}}","{\"reasoningEffort\":\"low\",\"interleavedThinking\":false}","{\"headers\":{\"anthropic-beta\":\"injected\\r\\nAuthorization: unwanted\"}}"};
 for(unsigned scenario=0;scenario<5;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"anthropic-messages\",\"model\":\"claude\",\"endpoint\":\"https://example.invalid/messages\",\"modelInfo\":{\"maxTokens\":32768}}");json_object_object_add(cfg,"options",json_tokener_parse(options[scenario]));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","beta-unit-key",1);int status=start();if(scenario==4)assert(status);else{assert(!status);unsigned beta=0;for(const struct curl_slist *h=captured_headers;h;h=h->next){if(!strncmp(h->data,"anthropic-beta:",15)){beta++;assert(strstr(h->data,scenario==1?"custom-feature":"interleaved-thinking-2025-05-14"));}}assert(beta==(scenario<2?1u:0u));}model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Anthropic thinking beta header, case-insensitive option override, null/off suppression and header-injection rejection");
}

static void test_anthropic_feature_headers(void){
 for(unsigned bits=0;bits<8;bits++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"anthropic-messages\",\"model\":\"claude\",\"endpoint\":\"https://example.invalid/messages\",\"modelInfo\":{\"maxTokens\":32768,\"compat\":{}}}");json_object *info=json_object_object_get(cfg,"modelInfo"),*compat=json_object_object_get(info,"compat");json_object_object_add(compat,"supportsEagerToolInputStreaming",json_object_new_boolean(!(bits&2)));if(bits&4)json_object_object_add(compat,"allowedFallbackModels",json_tokener_parse("[{\"model\":\"fallback\"}]"));if(bits&1)json_object_object_add(cfg,"options",json_tokener_parse("{\"reasoningEffort\":\"low\",\"maxTokens\":4096}"));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);pg_tool_registry registry={.entries={{.name="read",.description="Read",.schema="{\"type\":\"object\"}"}},.count=1};pg_mimo_bind_tools(&m,&registry);setenv("PANGU_MODEL_API_KEY","features-unit-key",1);assert(!start());unsigned found=0;for(const struct curl_slist *h=captured_headers;h;h=h->next){if(strncmp(h->data,"anthropic-beta:",15))continue;found++;assert(!!strstr(h->data,"interleaved-thinking")==!!(bits&1)&&!!strstr(h->data,"fine-grained-tool-streaming")==!!(bits&2)&&!!strstr(h->data,"server-side-fallback")==!!(bits&4));}assert(found==!!bits);model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Anthropic combined thinking/fine-grained-tool/fallback beta features match encoded request");
}

static void test_anthropic_affinity(void){
 for(unsigned scenario=0;scenario<6;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"anthropic-messages\",\"model\":\"claude\",\"endpoint\":\"https://example.invalid/messages\",\"sessionId\":\"pangu-persisted-session\",\"modelInfo\":{\"compat\":{}}}");json_object *compat=json_object_object_get(json_object_object_get(cfg,"modelInfo"),"compat");if(scenario==0||scenario==3)json_object_object_add(cfg,"modelProvider",json_object_new_string("openrouter"));if(scenario==1||scenario==2||scenario==4||scenario==5)json_object_object_add(compat,"sendSessionAffinityHeaders",json_object_new_boolean(1));if(scenario==2)json_object_object_add(compat,"sessionAffinityFormat",json_object_new_string("openrouter"));if(scenario==3)json_object_object_add(compat,"sendSessionAffinityHeaders",json_object_new_boolean(0));if(scenario==4)json_object_object_add(cfg,"options",json_tokener_parse("{\"cacheRetention\":\"none\"}"));if(scenario==5)json_object_object_add(cfg,"sessionId",json_object_new_string("bad\r\nInjected: header"));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","affinity-unit-key",1);int status=start();if(scenario==5)assert(status);else{assert(!status);unsigned found=0;for(const struct curl_slist *h=captured_headers;h;h=h->next){if(!strstr(h->data,"pangu-persisted-session"))continue;found++;assert(!strncmp(h->data,scenario==1?"x-session-affinity: ":"x-session-id: ",scenario==1?20:14));}assert(found==(scenario<3?1u:0u));}model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: PanGu Anthropic session affinity, OpenRouter/native formats, cache-off and explicit disable, injection rejection");
}

static void test_anthropic_strict_retry(void){
 const char *limits[]={"The compiled grammar is too large","Too many strict tools (32).","Schemas contains too many parameters with union types (100)."};
 for(unsigned scenario=0;scenario<9;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"anthropic-messages\",\"model\":\"claude\",\"endpoint\":\"https://example.invalid/messages\",\"modelInfo\":{\"compat\":{\"supportsStrictTools\":true}}}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);pg_tool_options preferred={.max_bytes=51200,.max_lines=2000,.constrained_sampling="{\"type\":\"json_schema\",\"strict\":\"prefer\"}"},required=preferred;required.constrained_sampling="{\"type\":\"json_schema\",\"strict\":\"require\"}";pg_tool_registry registry={.entries={{.name="prefer",.description="Prefer",.schema="{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"string\"}}}",.options=scenario==6?&required:&preferred},{.name="required",.description="Required",.schema="{\"type\":\"object\",\"properties\":{\"y\":{\"type\":\"string\"}}}",.options=&required}},.count=2};pg_mimo_bind_tools(&m,&registry);setenv("PANGU_MODEL_API_KEY","strict-retry-unit-key",1);assert(!start());json_object *initial=json_tokener_parse(captured_post),*initial_tools=json_object_object_get(initial,"tools");assert(json_object_get_boolean(json_object_object_get(json_object_array_get_idx(initial_tools,0),"strict")));json_object_put(initial);
  if(scenario==8){pg_model_event started={0};assert(!pg_mimo_decode_line(&m,"data: {\"type\":\"message_start\",\"message\":{\"id\":\"already-started\",\"usage\":{}}}",&started));}char body[512];snprintf(body,sizeof(body),"{\"type\":\"error\",\"error\":{\"type\":\"%s\",\"message\":\"%s\"}}",scenario==4?"authentication_error":"invalid_request_error",scenario==3?"unrelated invalid request":limits[scenario<3?scenario:0]);script_http=scenario==5?429:400;feed(body);finish_transport();pg_model_event event={0};int status=nxt(&event);
  if(scenario<3||scenario==7){assert(!status&&event.kind==PG_MODEL_WAIT&&m.strict_retry==1&&m.strict_diagnostics);json_object *retry=json_tokener_parse(captured_post),*tools=json_object_object_get(retry,"tools"),*first=json_object_array_get_idx(tools,0),*second=json_object_array_get_idx(tools,1);assert(!json_object_object_get(first,"strict")&&json_object_get_boolean(json_object_object_get(second,"strict")));json_object *schema=json_object_object_get(first,"input_schema");assert(!json_object_object_get(schema,"additionalProperties")&&json_object_array_length(json_object_object_get(schema,"required"))==0);json_object_put(retry);
   if(scenario==7){script_http=400;feed(body);finish_transport();assert(nxt(&event)&&m.strict_retry==1);}
   else{script_http=200;script_running=1;feed("data: {\"type\":\"message_start\",\"message\":{\"id\":\"after-fallback\",\"usage\":{\"input_tokens\":1}}}\n");feed("data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":1}}\n");feed("data: {\"type\":\"message_stop\"}\n");finish_transport();for(unsigned n=0;n<10;n++){assert(!nxt(&event));if(event.kind==PG_MODEL_DONE)break;}assert(event.kind==PG_MODEL_DONE);json_object *assistant=json_tokener_parse("{\"content\":[]}");assert(!pg_mimo_complete(&m,assistant));json_object *diagnostic=json_object_array_get_idx(json_object_object_get(assistant,"diagnostics"),0);assert(diagnostic&&!strcmp(json_object_get_string(json_object_object_get(diagnostic,"type")),"anthropic_strict_tools_fallback")&&json_object_get_int64(json_object_object_get(diagnostic,"timestamp"))>0);json_object_put(assistant);if(scenario==0){script_running=1;assert(!start()&&!m.strict_retry&&!m.strict_diagnostics);json_object *fresh=json_tokener_parse(captured_post);assert(json_object_get_boolean(json_object_object_get(json_object_array_get_idx(json_object_object_get(fresh,"tools"),0),"strict")));json_object_put(fresh);}}
  }else assert(status&&!m.strict_retry);model.close(model.context);assert(!m.strict_diagnostics&&!m.strict_retry);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Anthropic rejected strict-prefer request retries once, restores original schema, preserves required tools and emits diagnostic; unrelated/status/auth/required-only errors never downgrade");
}

static void test_google_transport(void){
 for(unsigned scenario=0;scenario<6;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();unsigned vertex=scenario&1;json_object *cfg=json_tokener_parse(vertex?"{\"api\":\"google-vertex\",\"model\":\"gemini-3-flash\",\"endpoint\":\"https://vertex.invalid/streamGenerateContent\"}":"{\"api\":\"google-generative-ai\",\"model\":\"gemini-3-flash\",\"endpoint\":\"https://google.invalid/streamGenerateContent?alt=sse\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","google-transport-unit-key",1);assert(!start());unsigned auth=0;for(const struct curl_slist *h=captured_headers;h;h=h->next)auth+=!strcmp(h->data,vertex?"Authorization: Bearer google-transport-unit-key":"x-goog-api-key: google-transport-unit-key");assert(auth==1&&strstr(captured_post,"\"contents\"")&&!strstr(captured_post,"\"messages\""));
  if(scenario==4)feed("data: {\"error\":{\"code\":429,\"message\":\"quota exhausted\"}}\n");
  else if(scenario==5)feed("data: {\"candidates\":[{\"finishReason\":\"SAFETY\"}]}\n");
  else if(scenario==2)feed("data: {\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"id\":\"google-call\",\"name\":\"read\",\"args\":{\"path\":\"file.txt\"}}}]},\"finishReason\":\"STOP\"}]}\n");
  else{feed("data: {\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"},{\"text\":\" world\"}]}}]}\r\n");if(scenario<2)feed("data: {\"candidates\":[{\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"promptTokenCount\":2,\"candidatesTokenCount\":1,\"totalTokenCount\":3}}\n");}
  finish_transport();pg_model_event event={0};int status=0;unsigned texts=0;for(unsigned n=0;n<12;n++){status=nxt(&event);if(status)break;if(event.kind==PG_MODEL_TEXT){texts++;assert(event.text.size==11&&!memcmp(event.text.data,"Hello world",11));continue;}if(event.kind!=PG_MODEL_WAIT)break;}
  if(scenario<3){assert(!status&&m.saw_done&&event.kind==(scenario==2?PG_MODEL_CALL:PG_MODEL_DONE));if(scenario==2)assert(!strcmp(event.text.data,"{\"path\":\"file.txt\"}"));else assert(texts==1);json_object *assistant=json_tokener_parse("{\"content\":[]}");assert(!pg_mimo_complete(&m,assistant)&&!json_object_object_get(assistant,"responseId"));json_object_put(assistant);}else{assert(status&&!m.saw_done);pg_slice message={0},body={0};assert(!pg_mimo_failure_data(&m,&message,&body));assert(strstr(message.data,scenario==3?"without a finish reason":scenario==4?"quota exhausted":"Provider stopped with: SAFETY"));}model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Google/Vertex actual model transport replay, API-key/bearer headers, text/tool, finish barrier, EOF and structured errors");
}
int main(void) {
    unsetenv("PANGU_MODEL_ENDPOINT");unsetenv("PANGU_MODEL_NAME");unsetenv("PANGU_MODEL_API_KEY");
    test_start_setup();
    test_bound_tools_survive_request_cleanup();
    test_text_stream();
    test_tool_stream();
    test_transport_failures();
    test_receive_bounds();
    test_partial_init_close();
    test_setup_failures();
    test_batch_stream();
    test_large_write_arguments();
    test_batch_rejection();
    test_injected_connection();
    test_saved_connection();
    test_responses_transport();
    test_responses_arguments();
    test_anthropic_transport();
    test_anthropic_beta_headers();
    test_anthropic_feature_headers();
    test_anthropic_affinity();
    test_anthropic_strict_retry();
    test_google_transport();
    puts("PASS: mimo transport lifecycle via scripted curl stand-in (no network): line split/CRLF, "
         "terminal ordering, HTTP/curl failures, receive overflow/capacity, partial-init close, idempotent close, atomic 2/8-call batches and rejection/faults");
    return 0;
}
