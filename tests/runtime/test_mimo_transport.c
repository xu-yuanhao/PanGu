#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo.h"
#include "../../src/runtime/mimo_layout.h"
#include <curl/curl.h>
#include <zstd.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>

/* 可控 curl 替身：perform/getinfo/info_read/add_handle/init 按脚本演出，
 * setopt 记录并转发真实 libcurl（校验变参 ABI 被真实现接受），
 * cleanup 计数用于验证 close 的释放恰好一次；显式签名验收使用本地 TLS。 */

static int zstd_fail,real_multi;
extern CURLMcode __real_curl_multi_perform(CURLM *,int *);
extern CURLMsg *__real_curl_multi_info_read(CURLM *,int *);
extern CURLMcode __real_curl_multi_add_handle(CURLM *,CURL *);
extern size_t __real_ZSTD_compress(void *,size_t,const void *,size_t,int);
size_t __wrap_ZSTD_compress(void *out,size_t capacity,const void *source,size_t size,int level){return zstd_fail?(size_t)-1:__real_ZSTD_compress(out,capacity,source,size,level);}
extern CURL *__real_curl_easy_init(void);
extern CURLcode __real_curl_easy_setopt(CURL *, CURLoption, ...);
extern CURLcode __real_curl_easy_getinfo(CURL *,CURLINFO,...);
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
static size_t (*captured_header)(char *,size_t,size_t,void *);
static void *captured_header_data;
static char *captured_decoded;
static long captured_post_size = -1;
static const char *captured_url, *captured_post, *captured_protocols;
static struct curl_slist *captured_headers;
static char captured_sigv4[128],captured_userpwd[256];
static long captured_http_version = -1;
static long captured_timeout = -1, captured_connect_timeout = -1, captured_nosignal = -1;
static int token_fail_at,token_calls;
struct json_tokener *__real_json_tokener_new(void);
struct json_tokener *__wrap_json_tokener_new(void){++token_calls;return token_fail_at==token_calls?0:__real_json_tokener_new();}
static int setopt_errors,setopt_fail_at,setopt_calls,getinfo_fail,slist_fail_at,slist_calls;
struct curl_slist *__wrap_curl_slist_append(struct curl_slist *list,const char *text){++slist_calls;return slist_fail_at==slist_calls?NULL:__real_curl_slist_append(list,text);}


CURL *__wrap_curl_easy_init(void) { CURL *easy=script_easy_fail?NULL:__real_curl_easy_init();const char *ca=getenv("PANGU_TEST_SIGV4_CA");if(easy&&ca)assert(!__real_curl_easy_setopt(easy,CURLOPT_CAINFO,ca));return easy; }

CURLM *__wrap_curl_multi_init(void) { return script_multi_fail ? NULL : __real_curl_multi_init(); }

CURLcode __wrap_curl_easy_setopt(CURL *h, CURLoption opt, ...) {
    ++setopt_calls;
    if(setopt_fail_at==setopt_calls)return CURLE_UNKNOWN_OPTION;
    va_list ap;
    va_start(ap, opt);
    CURLcode rc = CURLE_OK;
    switch (opt) {
    case CURLOPT_AWS_SIGV4:
    case CURLOPT_USERPWD: {
        const char *v=va_arg(ap,const char *);char *out=opt==CURLOPT_AWS_SIGV4?captured_sigv4:captured_userpwd;size_t size=opt==CURLOPT_AWS_SIGV4?sizeof(captured_sigv4):sizeof(captured_userpwd);assert(strlen(v)<size);strcpy(out,v);rc=__real_curl_easy_setopt(h,opt,v);break;
    }
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
        if(captured_post_size>=0){unsigned long long size=ZSTD_getFrameContentSize(v,(size_t)captured_post_size);assert(size!=ZSTD_CONTENTSIZE_ERROR&&size!=ZSTD_CONTENTSIZE_UNKNOWN&&size<16*1024*1024);captured_decoded=malloc((size_t)size+1);assert(captured_decoded);assert(ZSTD_decompress(captured_decoded,(size_t)size,v,(size_t)captured_post_size)==size);captured_decoded[size]=0;captured_post=captured_decoded;}
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_HTTPHEADER: {
        struct curl_slist *v = va_arg(ap, struct curl_slist *);
        captured_headers = v;
        rc = __real_curl_easy_setopt(h, opt, v);
        break;
    }
    case CURLOPT_HEADERFUNCTION: {
        size_t (*v)(char *,size_t,size_t,void *)=va_arg(ap,size_t (*)(char *,size_t,size_t,void *));captured_header=v;rc=__real_curl_easy_setopt(h,opt,v);break;
    }
    case CURLOPT_HEADERDATA: {
        void *v=va_arg(ap,void *);captured_header_data=v;rc=__real_curl_easy_setopt(h,opt,v);break;
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
    case CURLOPT_SUPPRESS_CONNECT_HEADERS:
    case CURLOPT_POSTFIELDSIZE:
    case CURLOPT_HTTP_VERSION:
    case CURLOPT_TIMEOUT:
    case CURLOPT_TIMEOUT_MS:
    case CURLOPT_CONNECTTIMEOUT:
    case CURLOPT_NOSIGNAL: {
        long v = va_arg(ap, long);
        if (opt == CURLOPT_POSTFIELDSIZE) captured_post_size = v;
        if (opt == CURLOPT_HTTP_VERSION) captured_http_version = v;
        if (opt == CURLOPT_TIMEOUT_MS) captured_timeout = v;
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
    if(real_multi)return __real_curl_multi_perform(multi,running);
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
    if(real_multi)return __real_curl_easy_getinfo(easy,info,out);
    *out = script_http;
    return CURLE_OK;
}

CURLMsg *__wrap_curl_multi_info_read(CURLM *multi, int *left) {
    if(real_multi)return __real_curl_multi_info_read(multi,left);
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
    if(real_multi)return __real_curl_multi_add_handle(multi,easy);
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
    captured_headers = NULL;captured_sigv4[0]=captured_userpwd[0]=0;
    real_multi=getenv("PANGU_TEST_REAL_MULTI")!=NULL;zstd_fail=0;free(captured_decoded);captured_decoded=NULL;captured_post_size=-1;
    captured_http_version = -1;
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
    if(pg_mimo_config_codex(m.configuration)&&n&&data[n-1]=='\n'&&!(n>=2&&data[n-2]=='\n')&&!(n>=4&&!memcmp(data+n-4,"\r\n\r\n",4)))assert(captured_write("\n",1,1,captured_write_data)==1);
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
    assert(captured_timeout == 60000 && captured_connect_timeout == 15 && captured_nosignal == 1);
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
 for(unsigned azure=0;azure<3;azure++){
 for(unsigned scenario=0;scenario<5;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *configuration=json_tokener_parse(azure==2?"{\"model\":\"native-responses\",\"api\":\"openai-codex-responses\"}":azure==1?"{\"model\":\"native-responses\",\"api\":\"azure-openai-responses\",\"options\":{\"azureResourceName\":\"pangu-resource\",\"azureDeploymentName\":\"deployed-model\"}}":"{\"model\":\"native-responses\",\"api\":\"openai-responses\",\"endpoint\":\"https://provider.invalid/v1/responses\"}");if(azure==2){json_object *options=json_tokener_parse("{\"sessionId\":\"pangu-session-identity\",\"serviceTier\":\"priority\",\"textVerbosity\":\"high\",\"reasoningEffort\":\"low\",\"reasoningSummary\":\"detailed\"}");json_object_object_add(options,"cacheRetention",json_object_new_string(scenario==2?"none":"long"));if(scenario==4)json_object_object_add(options,"sessionId",json_object_new_string("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));if(scenario>=1&&scenario<=3){json_object_object_del(options,"reasoningEffort");json_object_object_add(configuration,"modelInfo",json_tokener_parse(scenario==2?"{\"reasoning\":true,\"thinkingLevelMap\":{\"off\":null}}":scenario==3?"{\"reasoning\":true,\"thinkingLevelMap\":{\"off\":\"medium\"}}":"{\"reasoning\":true}"));}json_object *info=json_object_object_get(configuration,"modelInfo");if(!info){info=json_object_new_object();json_object_object_add(configuration,"modelInfo",info);}json_object_object_add(info,"cost",json_tokener_parse("{\"input\":1,\"output\":2,\"cacheRead\":0.1,\"cacheWrite\":1.25}"));json_object_object_add(options,"headers",json_tokener_parse("{\"Session-ID\":\"caller-value\",\"X-Client-Request-ID\":\"caller-value\"}"));json_object_object_add(configuration,"options",options);}assert(!pg_mimo_configure(&m,configuration));json_object_put(configuration);setenv("PANGU_MODEL_API_KEY",azure==2?"e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature":"responses-transport-unit-key",1);assert(!start()&&strstr(captured_post,"\"input\"")&&!strstr(captured_post,"\"messages\"")&&!strcmp(captured_url,azure==2?"https://chatgpt.com/backend-api/codex/responses":azure==1?"https://pangu-resource.openai.azure.com/openai/v1/responses?api-version=v1":"https://provider.invalid/v1/responses"));if(azure==1){unsigned keys=0;for(const struct curl_slist *h=captured_headers;h;h=h->next){keys+=!strcmp(h->data,"api-key: responses-transport-unit-key");assert(!strstr(h->data,"Authorization:"));}assert(keys==1&&strstr(captured_post,"deployed-model"));}if(azure==2){assert(m.wire_request&&m.wire_size==(size_t)captured_post_size&&!strcmp(captured_post,m.request));assert(strstr(captured_post,"instructions")&&strstr(captured_post,"reasoning.encrypted_content")&&!strstr(captured_post,"max_output_tokens"));unsigned accounts=0;for(const struct curl_slist *h=captured_headers;h;h=h->next)accounts+=!strcmp(h->data,"chatgpt-account-id: pangu-test-account");assert(accounts==1);json_object *body=json_tokener_parse(captured_post);assert(body&&!json_object_object_get(body,"prompt_cache_retention")&&!strcmp(json_object_get_string(json_object_object_get(body,"service_tier")),"priority")&&!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(body,"text"),"verbosity")),"high"));unsigned session_headers=0;for(const struct curl_slist *h=captured_headers;h;h=h->next)session_headers+=!strcmp(h->data,scenario==4?"session-id: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa":"session-id: pangu-session-identity")||!strcmp(h->data,scenario==4?"x-client-request-id: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa":"x-client-request-id: pangu-session-identity");unsigned caller_headers=0;for(const struct curl_slist *h=captured_headers;h;h=h->next)caller_headers+=strstr(h->data,"caller-value")!=0;assert(caller_headers==(scenario==2?2:0));assert(session_headers==(scenario==2?0:2));assert((json_object_object_get(body,"prompt_cache_key")!=0)==(scenario!=2));json_object *reason=json_object_object_get(body,"reasoning");if(scenario==2)assert(!reason);else if(scenario==1||scenario==3)assert(reason&&!strcmp(json_object_get_string(json_object_object_get(reason,"effort")),scenario==1?"none":"medium"));json_object_put(body);}pg_model_event event={0};
  feed("event: response.created\r\ndata: {\"type\":\"response.created\",\"response\":{\"id\":\"resp_test\"}}\r\n\r\n");
  feed("data: {\"type\":\"response.output_text.delta\",\"output_index\":0,\"delta\":\"he");for(unsigned i=0;i<4;i++)assert(!nxt(&event));feed("llo\"}\n");assert(!nxt(&event)&&event.kind==PG_MODEL_TEXT&&event.text.size==5&&!memcmp(event.text.data,"hello",5));
  if(scenario==1||scenario==4){feed("data: {\"type\":\"response.output_item.added\",\"output_index\":4,\"item\":{\"type\":\"function_call\",\"call_id\":\"call-r\",\"name\":\"read\",\"arguments\":\"\"}}\n");feed("data: {\"type\":\"response.function_call_arguments.done\",\"output_index\":4,\"arguments\":\"{\\\"path\\\":\\\"file.txt\\\"}\"}\n");if(scenario==1)feed("data: {\"type\":\"response.output_item.done\",\"output_index\":4,\"item\":{\"type\":\"function_call\",\"id\":\"fc_r\",\"call_id\":\"call-r\",\"name\":\"read\",\"arguments\":\"{\\\"path\\\":\\\"file.txt\\\"}\"}}\n");}
  if(scenario<2||scenario==4)feed("data: {\"type\":\"response.completed\",\"response\":{\"status\":\"completed\",\"service_tier\":\"default\",\"usage\":{\"input_tokens\":3,\"output_tokens\":2,\"total_tokens\":5}}}\n\n");
  if(scenario==3)feed("data: {\"type\":\"response.failed\",\"response\":{\"status\":\"failed\",\"error\":{\"message\":\"provider failure\"}}}\n");
  finish_transport();int status=0;for(unsigned i=0;i<12;i++){status=nxt(&event);if(status||event.kind!=PG_MODEL_WAIT)break;}if(scenario<2){assert(!status&&event.kind==(scenario?PG_MODEL_CALL:PG_MODEL_DONE)&&m.saw_done);if(scenario)assert(event.text.size==19&&!memcmp(event.text.data,"{\"path\":\"file.txt\"}",19));}else assert(status==1&&!m.saw_done);if(scenario<2){json_object *assistant=json_object_new_object();assert(!pg_mimo_complete(&m,assistant));assert(!strcmp(json_object_get_string(json_object_object_get(assistant,"api")),azure==2?"openai-codex-responses":azure==1?"azure-openai-responses":"openai-responses"));assert(!strcmp(json_object_get_string(json_object_object_get(assistant,"model")),"native-responses"));if(azure==2){json_object *cost=json_object_object_get(json_object_object_get(assistant,"usage"),"cost");assert(cost&&fabs(json_object_get_double(json_object_object_get(cost,"total"))-0.000014)<1e-15);}json_object_put(assistant);}model.close(model.context);assert(!m.request&&!m.easy&&!m.configuration&&!m.wire_request&&!m.wire_size);
 }
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Responses actual model-service curl replay, split SSE/CRLF, text/tool terminal, early EOF and provider failure");
}

static int codex_compressed_wire(void){
 const char *endpoint=getenv("PANGU_TEST_CODEX_ENDPOINT");assert(endpoint);reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-wire\",\"options\":{\"sessionId\":\"wire-codex-session\",\"maxRetries\":1}}");const char *timing=getenv("PANGU_TEST_HEADER_MODE");unsigned expect_timeout=timing&&!strcmp(timing,"deadline");if(timing){json_object *options=json_object_object_get(cfg,"options");json_object_object_add(options,"timeoutMs",json_object_new_int(!strcmp(timing,"zero")?0:100));json_object_object_add(options,"maxRetries",json_object_new_int(0));}json_object_object_add(cfg,"endpoint",json_object_new_string(endpoint));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);assert(!start()&&m.wire_request&&m.wire_size);if(real_multi){pg_model_event event={0};int next_status=0;for(unsigned tick=0;tick<10000;tick++){next_status=nxt(&event);if(next_status||event.kind==PG_MODEL_DONE)break;struct timespec pause={0,1000000};nanosleep(&pause,NULL);}if(expect_timeout){assert(next_status&&m.header_timed_out&&m.transport_error==CURLE_OPERATION_TIMEDOUT);pg_slice message={0},raw={0};assert(!pg_mimo_failure_data(&m,&message,&raw)&&strstr(message.data,"response headers timed out after 100ms"));model.close(model.context);free(captured_decoded);captured_decoded=NULL;puts("PASS: actual Codex TLS response-header deadline expires and releases request");return 0;}assert(!next_status&&event.kind==PG_MODEL_DONE&&m.retry_attempt==(timing?0u:1u));json_object *assistant=json_object_new_object();assert(!pg_mimo_complete(&m,assistant)&&json_object_get_boolean(json_object_object_get(assistant,"endTurn")));json_object_put(assistant);}else assert(!curl_easy_perform(m.easy));long status=0;assert(!__real_curl_easy_getinfo(m.easy,CURLINFO_RESPONSE_CODE,&status)&&status==200);model.close(model.context);assert(!m.wire_request&&!m.wire_size);free(captured_decoded);captured_decoded=NULL;puts("PASS: Codex binary Zstd body, declared byte length and request identity verified by actual TLS receiver");return 0;
}
static void test_codex_compression_failures(void){
 unsigned setopts=0,headers=0;
 for(unsigned mode=0;mode<4;mode++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-test\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature",1);
  if(mode==1)zstd_fail=1;if(mode==2)slist_fail_at=(int)headers;if(mode==3)setopt_fail_at=(int)setopts;
  int status=start();if(mode>=2)assert(status);else{assert(!status);if(!mode){assert(m.wire_request);setopts=(unsigned)setopt_calls;headers=(unsigned)slist_calls;}else{assert(!m.wire_request&&!m.wire_size&&captured_post_size==-1);for(const struct curl_slist *h=captured_headers;h;h=h->next)assert(!strstr(h->data,"content-encoding"));}}
  model.close(model.context);assert(!m.easy&&!m.multi&&!m.headers&&!m.request&&!m.wire_request&&!m.wire_size);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Codex compression fallback, header/setopt failure and binary request ownership cleanup");
}
static void test_codex_transport_retries(void){
 for(unsigned scenario=0;scenario<11;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-retry\",\"options\":{\"maxRetries\":2,\"maxRetryDelayMs\":10000}}");if(scenario==8)json_object_object_del(json_object_object_get(cfg,"options"),"maxRetries");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature",1);assert(!start());void *wire=m.wire_request;size_t wire_size=m.wire_size;
  script_http=scenario==2?400:scenario==5?0:scenario==6?200:scenario==1||scenario==0?429:503;feed(scenario==1?"{\"error\":\"insufficient_quota billing\"}":"{\"error\":\"provider unavailable\"}");finish_transport();if(scenario==5||scenario==6)script_info=INFO_RESULT_ERROR;
  const char *header=scenario==3?"Retry-After-Ms: 20000\r\n":scenario==4?"Retry-After-Ms: -5\r\n":scenario==9?"Retry-After: Thu, 01 Jan 1970 00:00:00 GMT\r\n":scenario==10?"Retry-After-Ms:  \t\r\n":"Retry-After-Ms: 5000\r\n";assert(captured_header((char *)header,1,strlen(header),captured_header_data)==strlen(header));
  pg_model_event event={0};int status=nxt(&event);if(scenario==1||scenario==2||scenario==3||scenario==6||scenario==8){assert(status&&!m.retry_attempt&&!m.retry_at);assert(m.retry_blocked==(scenario==3));if(scenario==3){pg_slice message={0},raw={0};const char *expected="Server requested 20s retry delay (max: 10s)";assert(!pg_mimo_failure_data(&m,&message,&raw)&&message.size==strlen(expected)&&!memcmp(message.data,expected,message.size));}}else{
   assert(!status&&event.kind==PG_MODEL_WAIT&&m.retry_attempt==1&&m.retry_at&&m.wire_request==wire&&m.wire_size==wire_size);
   m.retry_at=UINT64_MAX;assert(!nxt(&event)&&event.kind==PG_MODEL_WAIT&&m.retry_attempt==1);
   if(scenario==7){for(unsigned attempt=1;attempt<=2;attempt++){m.retry_at=1;script_running=1;script_http=0;script_info=INFO_OK;assert(!nxt(&event)&&!m.retry_at);script_http=503;feed("failure");finish_transport();status=nxt(&event);if(attempt==1)assert(!status&&m.retry_attempt==2&&m.retry_at);else assert(status&&m.retry_attempt==2&&!m.retry_at);}}
   else if(scenario!=5){m.retry_at=1;script_running=1;script_http=200;script_info=INFO_OK;assert(!nxt(&event)&&!m.retry_at&&!m.retry_after[0]&&!m.retry_after_ms[0]);feed("data: {\"type\":\"response.completed\",\"response\":{\"status\":\"completed\"}}\n");finish_transport();for(unsigned n=0;n<5;n++){assert(!nxt(&event));if(event.kind==PG_MODEL_DONE)break;}assert(event.kind==PG_MODEL_DONE);}
  }
  model.cancel(model.context);assert(!m.easy&&!m.multi&&!m.request&&!m.wire_request&&!m.retry_at&&!m.retry_attempt);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Codex transient/quota classification, retry deadlines, header bounds/date/negative delays, success, exhaustion and cancel without body reconstruction");
}
static void test_codex_failure_display(void){
 const char *bodies[]={"{\"error\":{\"code\":\"rate_limit_exceeded\",\"message\":\"raw quota detail\",\"plan_type\":\"PLUS\",\"resets_at\":1}}","{\"error\":{\"code\":\"usage_not_included\",\"message\":\"raw subscription detail\",\"plan_type\":\"TEAM\"}}","{\"error\":{\"code\":\"invalid_api_key\",\"message\":\"Invalid credential\"}}","unstructured server failure","{\"error\":\"capacity exhausted\"}","{\"error\":{\"code\":\"usage_limit_reached\",\"plan_type\":\"PrO\"}}"};
 const char *expected[]={"You have hit your ChatGPT usage limit (plus plan). Try again in ~0 min.","You have hit your ChatGPT usage limit (team plan).","Invalid credential",NULL,"You have hit your ChatGPT usage limit.","You have hit your ChatGPT usage limit (pro plan)."};
 for(unsigned scenario=0;scenario<6;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-error\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature",1);assert(!start());script_http=scenario==1||scenario==2?400:scenario==3?503:429;feed(bodies[scenario]);finish_transport();pg_model_event event={0};assert(nxt(&event));pg_slice detail={0},raw={0};int status=pg_mimo_failure_data(&m,&detail,&raw);if(expected[scenario]){assert(!status&&detail.size==strlen(expected[scenario])&&!memcmp(detail.data,expected[scenario],detail.size));if(scenario==0)assert(strstr(raw.data,"raw quota detail")&&strstr(raw.data,"rate_limit_exceeded"));}else assert(status);model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Codex quota plan/reset display preserves raw error classification, ordinary JSON errors and non-JSON fallback");
}
static void test_codex_terminal_frames(void){
 const char *types[]={"response.done","response.completed","response.incomplete"};const char *statuses[]={"completed","incomplete","failed","cancelled","queued","in_progress","unknown",NULL,NULL};
 for(unsigned type=0;type<3;type++)for(unsigned state=0;state<9;state++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-frame\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature",1);assert(!start());json_object *event=json_object_new_object(),*response=json_object_new_object();json_object_object_add(event,"type",json_object_new_string(types[type]));json_object_object_add(event,"response",response);if(statuses[state])json_object_object_add(response,"status",json_object_new_string(statuses[state]));if(state==8)json_object_object_add(response,"status",json_object_new_int(37));json_object_object_add(response,"end_turn",json_object_new_boolean(state%2));if(state==1)json_object_object_add(response,"incomplete_details",json_tokener_parse("{\"reason\":\"max_output_tokens\"}"));char frame[2048];int length=snprintf(frame,sizeof(frame),"data: %s\n\n",json_object_to_json_string_ext(event,JSON_C_TO_STRING_PLAIN));assert(length>0&&(size_t)length<sizeof(frame));assert(captured_write(frame,1,(size_t)length,captured_write_data)==(size_t)length);json_object_put(event);
  pg_model_event result={0};int status=nxt(&result);if(state==2||state==3)assert(status);else{assert(!status&&result.kind==PG_MODEL_DONE&&m.saw_done);json_object *assistant=json_object_new_object();assert(!pg_mimo_complete(&m,assistant));json_object *end=json_object_object_get(assistant,"endTurn");assert(end&&json_object_get_boolean(end)==(int)(state%2));assert(!strcmp(json_object_get_string(json_object_object_get(assistant,"stopReason")),state==1?"length":"stop"));if(state>=6)assert(!json_object_object_get(assistant,"rawStopReason"));json_object_put(assistant);}model.close(model.context);assert(!m.codex_frame);
 }
 for(unsigned mode=0;mode<8;mode++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-frame\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature",1);assert(!start());
  const char *frame=mode==0?"data: {\"type\":\"response.done\",\n" : "";
  if(mode==0)frame="data: {\"type\":\"response.done\",\n" "data: \"response\":{\"status\":\"completed\"}}\n\n";
  else if(mode==1)frame="data: {\"type\":\"response.done\",\"response\":{\"status\":\"completed\"}}";
  else if(mode==2)frame="data: {\"type\":\"response.done\",\"response\":{\"status\":\"completed\"}}\n";
  else if(mode==3)frame="data: {bad JSON}\n\n";
  else if(mode==4)frame="data: [DONE]\n\ndata: {\"type\":\"response.done\",\"response\":{\"status\":\"completed\"}}\n\n";
  else if(mode==5)frame=": ping\r\nevent: ignored\r\nretry: 10\r\ndata: {\"type\":\"response.done\",\"response\":{\"status\":\"completed\"}}\r\n\r\n";
  else if(mode==6)frame="data: {\"type\":\"response.done\",\"response\":{\"status\":\"completed\"}}\n\ndata: {bad trailing JSON}\n\n";
  else frame="data: {\"type\":\"response.done\",\n";
  size_t length=strlen(frame);assert(captured_write((char *)frame,1,length,captured_write_data)==length);if(mode==1||mode==2)finish_transport();pg_model_event result={0};int status=nxt(&result);if(mode==3)assert(status);else if(mode==7)assert(!status&&result.kind==PG_MODEL_WAIT&&m.codex_frame);else{for(unsigned n=0;n<4&&!status&&result.kind!=PG_MODEL_DONE;n++)status=nxt(&result);assert(!status&&result.kind==PG_MODEL_DONE);}model.cancel(model.context);assert(!m.codex_frame&&!m.easy);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Codex terminal aliases/status/endTurn and SSE multiline, CRLF, EOF, sentinel, trailing events, malformed frames and cancellation");
}
static void test_codex_stream_failures(void){
 const char *events[]={
 "{\"type\":\"response.done\",\"response\":{\"status\":\"incomplete\",\"incomplete_details\":{\"reason\":\"content_filter\"}}}",
 "{\"type\":\"response.done\",\"response\":{\"status\":\"incomplete\"}}",
 "{\"type\":\"error\",\"message\":\"\",\"code\":\"code-only\"}",
 "{\"type\":\"error\",\"code\":\"root-code\",\"message\":\"root-message\",\"error\":{\"code\":\"nested-code\",\"message\":\"nested-message\"}}",
 "{\"type\":\"error\",\"code\":7,\"message\":false,\"error\":{\"code\":\"nested-code\",\"message\":\"nested-message\"}}",
 "{\"type\":\"error\",\"code\":\"code-only\"}",
 "{\"type\":\"response.failed\",\"response\":{\"error\":{\"code\":\"failed-code\",\"message\":\"failed-message\"}}}",
 "{\"type\":\"response.failed\",\"response\":{}}",
 "{\"type\":\"response.done\",\"response\":{\"status\":\"cancelled\",\"error\":{\"code\":\"cancelled-code\",\"message\":\"cancelled-message\"}}}"
 };
 const char *expected[]={"Response incomplete: content_filter","Response incomplete without a provider reason","Codex error: code-only","Codex error: root-message","Codex error: nested-message","Codex error: code-only","failed-message","Codex response failed","cancelled-message"};
 for(unsigned i=0;i<sizeof(events)/sizeof(*events);i++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-failure\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature",1);assert(!start());char frame[2048];snprintf(frame,sizeof(frame),"data: %s\n\n",events[i]);size_t length=strlen(frame);assert(captured_write(frame,1,length,captured_write_data)==length);pg_model_event event={0};assert(nxt(&event));pg_slice message={0},raw={0};assert(!pg_mimo_failure_data(&m,&message,&raw)&&message.size==strlen(expected[i])&&!memcmp(message.data,expected[i],message.size));json_object *detail=json_tokener_parse(raw.data),*payload=json_object_object_get(detail,"payload"),*original=json_tokener_parse(events[i]);assert(payload&&json_object_equal(payload,original));json_object_put(detail);json_object_put(original);model.close(model.context);assert(!m.response_terminal&&!m.codex_frame);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Codex stream errors preserve root/nested precedence, classification and complete provider event through failure_data");
}
static void test_codex_header_deadlines(void){
 for(unsigned mode=0;mode<6;mode++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-timeout\",\"options\":{}}");json_object *options=json_object_object_get(cfg,"options");if(mode!=0)json_object_object_add(options,"timeoutMs",mode==1?json_object_new_int(0):mode==2?json_object_new_double(100.9):json_object_new_int(100));if(mode==5)json_object_object_add(options,"maxRetries",json_object_new_int(1));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","e30.eyJodHRwczovL2FwaS5vcGVuYWkuY29tL2F1dGgiOnsiY2hhdGdwdF9hY2NvdW50X2lkIjoicGFuZ3UtdGVzdC1hY2NvdW50In19.signature",1);assert(!start()&&captured_timeout==0&&captured_connect_timeout==0);
  if(mode<2){assert(!m.header_until&&!m.header_timeout_ms);}else{assert(m.header_timeout_ms==100&&m.header_until);}
  pg_model_event event={0};if(mode==3||mode==5){script_http=0;m.header_until=1;int status=nxt(&event);if(mode==3){assert(status&&m.header_timed_out&&m.transport_error==CURLE_OPERATION_TIMEDOUT);pg_slice message={0},raw={0};const char *wanted="Codex SSE response headers timed out after 100ms";assert(!pg_mimo_failure_data(&m,&message,&raw)&&message.size==strlen(wanted)&&!memcmp(message.data,wanted,message.size));}else{assert(!status&&m.retry_at&&m.retry_attempt==1);m.retry_at=1;assert(!nxt(&event)&&!m.header_timed_out&&m.header_until);}}
  else if(mode>=2){const char *headers[]={"HTTP/1.1 100 Continue\r\n","\r\n","HTTP/2 200\r\n","x-extra: value\r\n","\r\n"};for(unsigned n=0;n<5;n++){assert(captured_header((char *)headers[n],1,strlen(headers[n]),captured_header_data)==strlen(headers[n]));if(n<4)assert(m.header_until&&!m.header_complete);}assert(!m.header_until&&m.header_complete);assert(!nxt(&event)&&event.kind==PG_MODEL_WAIT);}
  model.cancel(model.context);assert(!m.easy&&!m.multi&&!m.header_until&&!m.header_timeout_ms&&!m.header_timed_out);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Codex absent/zero/fractional header timeouts, final-header boundary, timeout diagnostics, retry rearm and cancellation");
}
static void test_codex_auth_failures(void){
 const char *tokens[]={"invalid","a.e30=.b","a.!!!!.b","a.e30=.b.extra"};
 for(unsigned i=0;i<4;i++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"openai-codex-responses\",\"model\":\"codex-test\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY",tokens[i],1);assert(start());model.close(model.context);assert(!m.easy&&!m.multi&&!m.headers&&!m.configuration);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Codex invalid JWT structure, Base64 and missing account fail before transport and release request state");
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

static void test_google_endpoint_config(void){
 const char *configurations[]={"{\"api\":\"google-generative-ai\",\"model\":\"gemini-3-flash\"}","{\"api\":\"google-generative-ai\",\"model\":\"model with space\",\"baseUrl\":\"https://proxy.invalid/v1beta/\"}","{\"api\":\"google-vertex\",\"model\":\"gemini-3-flash\",\"options\":{\"authMode\":\"api-key\"}}","{\"api\":\"google-vertex\",\"model\":\"gemini-3-flash\",\"options\":{\"project\":\"pangu-project\",\"location\":\"global\"}}","{\"api\":\"google-vertex\",\"model\":\"gemini-3-flash\",\"options\":{\"project\":\"pangu-project\",\"location\":\"us-central1\"}}","{\"api\":\"google-vertex\",\"model\":\"gemini-3-flash\",\"baseUrl\":\"https://proxy.invalid\",\"options\":{\"authMode\":\"api-key\"}}","{\"api\":\"google-vertex\",\"model\":\"gemini-3-flash\",\"baseUrl\":\"https://proxy.invalid/v1/projects/custom/locations/global\",\"options\":{\"project\":\"custom\",\"location\":\"global\"}}"};
 const char *urls[]={"https://generativelanguage.googleapis.com/v1beta/models/gemini-3-flash:streamGenerateContent?alt=sse","https://proxy.invalid/v1beta/models/model%20with%20space:streamGenerateContent?alt=sse","https://aiplatform.googleapis.com/v1/publishers/google/models/gemini-3-flash:streamGenerateContent?alt=sse","https://aiplatform.googleapis.com/v1/projects/pangu-project/locations/global/publishers/google/models/gemini-3-flash:streamGenerateContent?alt=sse","https://us-central1-aiplatform.googleapis.com/v1/projects/pangu-project/locations/us-central1/publishers/google/models/gemini-3-flash:streamGenerateContent?alt=sse","https://proxy.invalid/v1/publishers/google/models/gemini-3-flash:streamGenerateContent?alt=sse","https://proxy.invalid/v1/projects/custom/locations/global/publishers/google/models/gemini-3-flash:streamGenerateContent?alt=sse"};
 for(unsigned n=0;n<7;n++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse(configurations[n]);assert(!pg_mimo_configure(&m,cfg)&&!json_object_object_get(cfg,"endpoint"));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","route-unit-key",1);assert(!start()&&!strcmp(captured_url,urls[n]));unsigned key=0;for(const struct curl_slist *h=captured_headers;h;h=h->next)key+=!strcmp(h->data,n<3||n==5?"x-goog-api-key: route-unit-key":"Authorization: Bearer route-unit-key");assert(key==1);model.close(model.context);
 }

 reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);json_object *cfg=json_tokener_parse("{\"api\":\"google-generative-ai\",\"model\":\"valid\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);char *stable=strdup(json_object_to_json_string_ext(m.configuration,JSON_C_TO_STRING_PLAIN));assert(stable);const char *invalid[]={"{\"api\":\"google-vertex\",\"model\":\"missing-project-and-location\"}","{\"api\":\"google-vertex\",\"model\":\"bad-mode\",\"endpoint\":\"https://example.invalid/streamGenerateContent\",\"options\":{\"authMode\":\"invalid\"}}"};for(unsigned n=0;n<2;n++){cfg=json_tokener_parse(invalid[n]);assert(pg_mimo_configure(&m,cfg)&&!strcmp(json_object_to_json_string_ext(m.configuration,JSON_C_TO_STRING_PLAIN),stable));json_object_put(cfg);}free(stable);model.close(model.context);
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Google default/custom and Vertex API-key/global/regional/collection endpoints reach transport with matching auth");
}
static void test_copilot_gateway(void){
 const char *apis[]={"openai-completions","openai-responses","anthropic-messages"};
 for(unsigned api=0;api<3;api++)for(unsigned scenario=0;scenario<6;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();
  json_object *cfg=json_tokener_parse("{\"model\":\"copilot-unit\",\"endpoint\":\"https://copilot.invalid/request\",\"modelInfo\":{\"provider\":\"github-copilot\",\"headers\":{\"X-Initiator\":\"model-value\"}}}");json_object_object_add(cfg,"api",json_object_new_string(apis[api]));
  if(scenario==4)json_object_object_add(cfg,"options",json_tokener_parse("{\"headers\":{\"X-Initiator\":\"explicit-value\",\"Copilot-Vision-Request\":null}}"));
  if(scenario==5)json_object_object_add(json_object_object_get(cfg,"modelInfo"),"provider",json_object_new_string("other"));
  if(scenario==3)json_object_object_del(cfg,"endpoint");
  assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","copilot-test-token",1);
  json_object *context=json_tokener_parse(scenario==1?"{\"messages\":[{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"continue\"}]}]}":scenario==2||scenario==4?"{\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":\"aGk=\"}]},{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"seen\"}]}]}":"{\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"hello\"}]}]}");
  if(scenario!=3){session.count=1;session.messages[0].role=PG_CONTEXT_VIEW;session.messages[0].context_view=context;}
  assert(!start());if(scenario==3)assert(!strcmp(captured_url,api==0?"https://api.individual.githubcopilot.com/chat/completions":api==1?"https://api.individual.githubcopilot.com/responses":"https://api.individual.githubcopilot.com/v1/messages"));unsigned initiator=0,intent=0,vision=0,bearer=0,api_key=0;
  for(const struct curl_slist *h=captured_headers;h;h=h->next){
   if(!strncasecmp(h->data,"x-initiator:",12)){initiator++;assert(strstr(h->data,scenario==4?"explicit-value":scenario==5?"model-value":scenario==1||scenario==2?"agent":"user"));}
   intent+=!strcasecmp(h->data,"openai-intent: conversation-edits");vision+=!strcasecmp(h->data,"copilot-vision-request: true");bearer+=!strcmp(h->data,"Authorization: Bearer copilot-test-token");api_key+=!strncmp(h->data,"x-api-key:",10);
  }
  assert(initiator==1&&intent==(scenario!=5)&&vision==(scenario==2));if(api==2&&scenario!=5)assert(bearer==1&&!api_key);
  model.close(model.context);json_object_put(context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Copilot Chat/Responses/Anthropic initiator, transcript vision, legacy input, model/options precedence and Anthropic Bearer authentication");
}
static void test_bedrock_auth(void){
 unsetenv("PANGU_MODEL_API_KEY");setenv("AWS_PROFILE","ambient-unit-profile",1);setenv("AWS_ACCESS_KEY_ID","AKID_PANGU_UNIT",1);setenv("AWS_SECRET_ACCESS_KEY","pangu-unit-secret",1);setenv("AWS_SESSION_TOKEN","pangu-unit-session",1);
 char long_token[2049];memset(long_token,'t',2048);long_token[2048]=0;unsigned iam_calls=0;
 for(unsigned mode=0;mode<4;mode++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-test\",\"options\":{\"region\":\"us-west-2\",\"headers\":{\"Authorization\":\"untrusted\",\"Host\":\"untrusted\",\"X-Amz-Date\":\"untrusted\",\"X-Project\":\"pangu\"}}}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);
  if(mode==0){unsetenv("AWS_BEARER_TOKEN_BEDROCK");unsetenv("PANGU_MODEL_API_KEY");}else if(mode==1)setenv("AWS_BEARER_TOKEN_BEDROCK","aws-unit-bearer",1);else if(mode==2)setenv("PANGU_MODEL_API_KEY","pangu-unit-bearer",1);else{unsetenv("PANGU_MODEL_API_KEY");setenv("AWS_BEARER_TOKEN_BEDROCK",long_token,1);}
  assert(!start());unsigned auth=0,token=0,project=0;for(const struct curl_slist *h=captured_headers;h;h=h->next){assert(!strstr(h->data,"untrusted"));auth+=mode==3?(!strncmp(h->data,"Authorization: Bearer ",22)&&strlen(h->data)==2070):!strcmp(h->data,mode==2?"Authorization: Bearer pangu-unit-bearer":"Authorization: Bearer aws-unit-bearer");token+=!strcmp(h->data,"X-Amz-Security-Token: pangu-unit-session");project+=!strcmp(h->data,"x-project: pangu");}assert(project==1);if(!mode)assert(!auth&&token==1&&!strcmp(captured_sigv4,"aws:amz:us-west-2:bedrock")&&!strcmp(captured_userpwd,"AKID_PANGU_UNIT:pangu-unit-secret"));else assert(auth==1&&!token&&!captured_sigv4[0]);if(!mode)iam_calls=(unsigned)setopt_calls;model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");unsetenv("AWS_BEARER_TOKEN_BEDROCK");
 for(unsigned fault=1;fault<=iam_calls+2;fault++){reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"test\",\"options\":{\"region\":\"us-west-2\"}}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);if(fault<=iam_calls)setopt_fail_at=(int)fault;else slist_fail_at=(int)(fault-iam_calls);assert(start());model.close(model.context);assert(!m.easy&&!m.headers&&!m.configuration);}
 const char *bad[]={"", "bad\r\nsecret"};for(unsigned n=0;n<2;n++){reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"test\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("AWS_SECRET_ACCESS_KEY",bad[n],1);assert(start());model.close(model.context);assert(!m.easy&&!m.headers&&!m.configuration);}
 unsetenv("AWS_PROFILE");unsetenv("AWS_ACCESS_KEY_ID");unsetenv("AWS_SECRET_ACCESS_KEY");unsetenv("AWS_SESSION_TOKEN");puts("PASS: Bedrock native SigV4 options, temporary token, Bearer precedence, reserved headers and invalid credentials");
}
static void test_bedrock_profiles(void){
 char directory[]="/tmp/pangu-aws-profile-XXXXXX",config[256],credentials[256];assert(mkdtemp(directory));
 snprintf(config,sizeof(config),"%s/config",directory);snprintf(credentials,sizeof(credentials),"%s/credentials",directory);
 FILE *file=fopen(config,"w");assert(file);fputs("[default]\nregion=us-east-2\n[profile PanGu]\nregion=eu-west-1\naws_access_key_id=CONFIG_ACCESS\naws_secret_access_key=config-secret\n[profile Role]\nregion=eu-central-1\nrole_arn=arn:aws:iam::123:role/test\n",file);assert(!fclose(file));
 file=fopen(credentials,"w");assert(file);fputs("[default]\naws_access_key_id=DEFAULT_ACCESS\naws_secret_access_key=default-secret\n[PanGu]\naws_access_key_id=PROFILE_ACCESS\naws_secret_access_key=profile-secret\naws_session_token=profile-token\n",file);assert(!fclose(file));
 setenv("AWS_CONFIG_FILE",config,1);setenv("AWS_SHARED_CREDENTIALS_FILE",credentials,1);
 unsetenv("PANGU_MODEL_API_KEY");unsetenv("AWS_BEARER_TOKEN_BEDROCK");unsetenv("AWS_REGION");unsetenv("AWS_DEFAULT_REGION");
 for(unsigned scenario=0;scenario<8;scenario++){
  unsetenv("AWS_PROFILE");unsetenv("AWS_ACCESS_KEY_ID");unsetenv("AWS_SECRET_ACCESS_KEY");unsetenv("AWS_SESSION_TOKEN");
  if(scenario==1||scenario==2||scenario==3){setenv("AWS_PROFILE","PanGu",1);if(scenario!=3){setenv("AWS_ACCESS_KEY_ID","ENV_ACCESS",1);setenv("AWS_SECRET_ACCESS_KEY","env-secret",1);}}
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-test\",\"options\":{}}");
  if(scenario==1||scenario>=4)json_object_object_add(json_object_object_get(cfg,"options"),"profile",json_object_new_string(scenario==5?"pangu":scenario==6?"Role":scenario==7?"Missing":"PanGu"));
  assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);
  if(scenario>=5){assert(start());}else{
   assert(!start());const char *expected=scenario==0?"DEFAULT_ACCESS:default-secret":scenario==2?"ENV_ACCESS:env-secret":"PROFILE_ACCESS:profile-secret";
   assert(!strcmp(captured_userpwd,expected));assert(!strcmp(captured_sigv4,scenario==0||scenario==4?"aws:amz:us-east-1:bedrock":"aws:amz:eu-west-1:bedrock"));
   assert(strstr(captured_url,scenario==0||scenario==4?"bedrock-runtime.us-east-1.amazonaws.com":"bedrock-runtime.eu-west-1.amazonaws.com"));
   unsigned tokens=0;for(const struct curl_slist *h=captured_headers;h;h=h->next)tokens+=!strcmp(h->data,"X-Amz-Security-Token: profile-token");assert(tokens==(scenario!=0&&scenario!=2));
   const char *public_config=json_object_to_json_string(m.configuration);assert(!strstr(public_config,"PROFILE_ACCESS")&&!strstr(public_config,"profile-secret")&&!strstr(public_config,"profile-token")&&!strstr(public_config,"env-secret"));
  }
  model.close(model.context);assert(!m.easy&&!m.headers&&!m.configuration);
 }
 unsetenv("AWS_PROFILE");unsetenv("AWS_ACCESS_KEY_ID");unsetenv("AWS_SECRET_ACCESS_KEY");unsetenv("AWS_SESSION_TOKEN");unsetenv("AWS_CONFIG_FILE");unsetenv("AWS_SHARED_CREDENTIALS_FILE");assert(!unlink(config)&&!unlink(credentials)&&!rmdir(directory));
 puts("PASS: shared AWS profiles, case-sensitive selection, config/credentials precedence, ambient keys, profile regions and missing/role refusal");
}
static void test_bedrock_proxy_modes(void){
 for(unsigned mode=0;mode<5;mode++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();
  setenv("AWS_BEDROCK_SKIP_AUTH",mode==4?"true":"1",1);setenv("AWS_BEDROCK_FORCE_HTTP1",mode==3?"true":"1",1);
  setenv("PANGU_MODEL_API_KEY","explicit-bearer-must-be-skipped",1);setenv("AWS_BEARER_TOKEN_BEDROCK","ambient-bearer-must-be-skipped",1);setenv("AWS_SESSION_TOKEN","ambient-token-must-be-skipped",1);
  json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"test\",\"endpoint\":\"https://proxy.invalid/converse-stream\",\"options\":{\"region\":\"us-west-2\"}}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);
  if(mode==1)setopt_fail_at=4;
  if(mode==1){assert(start());}else{
   assert(!start());assert(captured_http_version==(mode==3?-1:CURL_HTTP_VERSION_1_1));
   if(mode==4){assert(!captured_sigv4[0]);}else{assert(!strcmp(captured_userpwd,"dummy-access-key:dummy-secret-key")&&!strcmp(captured_sigv4,"aws:amz:us-west-2:bedrock"));for(const struct curl_slist *h=captured_headers;h;h=h->next)assert(!strstr(h->data,"Bearer")&&!strstr(h->data,"ambient-token"));}
  }
  model.close(model.context);assert(!m.easy&&!m.multi&&!m.headers&&!m.configuration);
 }
 unsetenv("AWS_BEDROCK_SKIP_AUTH");unsetenv("AWS_BEDROCK_FORCE_HTTP1");unsetenv("PANGU_MODEL_API_KEY");unsetenv("AWS_BEARER_TOKEN_BEDROCK");unsetenv("AWS_SESSION_TOKEN");
 puts("PASS: Bedrock proxy skip-auth wins Bearer and session credentials; exact switches, HTTP/1.1 and transport failure cleanup");
}
static int bedrock_sigv4_wire(void){
 const char *endpoint=getenv("PANGU_TEST_SIGV4_ENDPOINT");assert(endpoint);reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-test\",\"options\":{\"region\":\"us-west-2\",\"headers\":{\"X-Project\":\"pangu-wire\"}}}");json_object_object_add(cfg,"endpoint",json_object_new_string(endpoint));unsigned timeout=getenv("PANGU_TEST_DEADLINE")!=0;if(timeout)json_object_object_add(json_object_object_get(cfg,"options"),"timeoutMs",json_object_new_int(50));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);assert(!start());CURLcode result=curl_easy_perform(m.easy);if(timeout){assert(result==CURLE_OPERATION_TIMEDOUT);model.cancel(model.context);assert(!m.easy&&!m.multi&&!m.headers&&!m.configuration);puts("PASS: actual TLS request deadline expires and cancellation releases transport");return 0;}assert(!result&&!strcmp(m.response_request_id,"wire-public-request-id"));long status=0;assert(!__real_curl_easy_getinfo(m.easy,CURLINFO_RESPONSE_CODE,&status)&&status==200);model.close(model.context);puts("PASS: native Bedrock request signed by actual libcurl and verified by independent TLS receiver");return 0;
}
static uint32_t bedrock_crc(const unsigned char *data,size_t size){uint32_t crc=~0u;while(size--){crc^=*data++;for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}return ~crc;}
static void bedrock_u32(unsigned char *out,uint32_t value){out[0]=(unsigned char)(value>>24);out[1]=(unsigned char)(value>>16);out[2]=(unsigned char)(value>>8);out[3]=(unsigned char)value;}
static void bedrock_feed(const char *type,const char *payload,unsigned corrupt){
 unsigned exception=strstr(type,"Exception")!=0;size_t name=strlen(type),headers=(exception?45:37)+name,body=strlen(payload),size=16+headers+body;unsigned char *frame=calloc(1,size);assert(frame&&name<64);bedrock_u32(frame,(uint32_t)size);bedrock_u32(frame+4,(uint32_t)headers);bedrock_u32(frame+8,bedrock_crc(frame,8));frame[12]=13;memcpy(frame+13,":message-type",13);frame[26]=7;frame[27]=0;frame[28]=exception?9:5;memcpy(frame+29,exception?"exception":"event",exception?9:5);size_t offset=exception?38:34;size_t key_size=exception?15:11;frame[offset]=(unsigned char)key_size;memcpy(frame+offset+1,exception?":exception-type":":event-type",key_size);frame[offset+1+key_size]=7;frame[offset+2+key_size]=(unsigned char)(name>>8);frame[offset+3+key_size]=(unsigned char)name;memcpy(frame+offset+4+key_size,type,name);memcpy(frame+12+headers,payload,body);bedrock_u32(frame+size-4,bedrock_crc(frame,size-4));if(corrupt==1)frame[8]^=1;if(corrupt==2)frame[size-1]^=1;
 if(!corrupt&&!strcmp(type,"messageStart")){assert(captured_write((char *)frame,1,7,captured_write_data)==7);pg_model_event partial={0};assert(!nxt(&partial)&&partial.kind==PG_MODEL_WAIT);assert(captured_write((char *)frame+7,1,size-7,captured_write_data)==size-7);}else assert(captured_write((char *)frame,1,size,captured_write_data)==size);free(frame);
}
static void test_bedrock_transport(void){
 assert(bedrock_crc((const unsigned char *)"123456789",9)==0xcbf43926u);
 for(unsigned scenario=0;scenario<6;scenario++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-test\",\"endpoint\":\"https://bedrock.invalid/model/test/converse-stream\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);session.count=3;session.messages[1].role=PG_TOOL_CALL;strcpy(session.messages[1].call_id,"previous-call");strcpy(session.messages[1].tool_name,"read");strcpy(session.messages[1].text,"{\"path\":\"before.txt\"}");session.messages[1].size=strlen(session.messages[1].text);session.messages[2].role=PG_TOOL_RESULT;strcpy(session.messages[2].call_id,"previous-call");strcpy(session.messages[2].tool_name,"read");strcpy(session.messages[2].text,"previous result");session.messages[2].size=strlen(session.messages[2].text);setenv("PANGU_MODEL_API_KEY","bedrock-unit-token",1);assert(!start());json_object *wire=json_tokener_parse(captured_post);assert(json_object_object_get(wire,"inferenceConfig")&&!json_object_object_get(wire,"model")&&!json_object_object_get(wire,"stream")&&strstr(captured_post,"toolUse")&&strstr(captured_post,"toolResult")&&strstr(captured_post,"previous result"));json_object_put(wire);
  bedrock_feed("messageStart","{\"role\":\"assistant\"}",scenario==2?1:scenario==3?2:0);
  bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":0,\"delta\":{\"text\":\"hello\"}}",0);
  if(scenario==1){bedrock_feed("contentBlockStart","{\"contentBlockIndex\":5,\"start\":{\"toolUse\":{\"toolUseId\":\"call-bedrock\",\"name\":\"read\"}}}",0);bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":5,\"delta\":{\"toolUse\":{\"input\":\"{\\\"path\\\":\\\"file.txt\\\"}\"}}}",0);}
  if(scenario!=4)bedrock_feed("messageStop",scenario==1?"{\"stopReason\":\"tool_use\"}":scenario==5?"{\"stopReason\":\"model_context_window_exceeded\"}":"{\"stopReason\":\"end_turn\"}",0);
  bedrock_feed("metadata","{\"usage\":{\"inputTokens\":3,\"outputTokens\":2,\"totalTokens\":5}}",0);finish_transport();pg_model_event event={0};int status=0;unsigned texts=0;for(unsigned n=0;n<20;n++){status=nxt(&event);if(status)break;if(event.kind==PG_MODEL_TEXT){texts++;assert(event.text.size==5&&!memcmp(event.text.data,"hello",5));}if(event.kind==PG_MODEL_DONE||event.kind==PG_MODEL_CALL)break;}
  if(scenario<2||scenario==5){assert(!status&&texts==1&&event.kind==(scenario==1?PG_MODEL_CALL:PG_MODEL_DONE));json_object *assistant=json_object_new_object();assert(!pg_mimo_complete(&m,assistant));assert(!strcmp(json_object_get_string(json_object_object_get(assistant,"api")),"bedrock-converse-stream"));json_object_put(assistant);}else assert(status);model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Bedrock binary event transport, prelude/message CRC, text/tool finalization, metadata, corrupt frames and missing stop");
}
static void test_bedrock_multimodal(void){
 for(unsigned variant=0;variant<5;variant++){
  const char *name=(variant==1||variant==3)?"openai.gpt-test":"anthropic.claude-test";reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"modelProvider\":\"bedrock\",\"modelInfo\":{\"input\":[\"text\",\"image\"]}}");json_object_object_add(cfg,"model",json_object_new_string(name));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);
  json_object *context=json_tokener_parse("{\"systemPrompt\":\"PanGu\",\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"inspect\"},{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":\"AQID\"}]},{\"role\":\"assistant\",\"provider\":\"bedrock\",\"api\":\"bedrock-converse-stream\",\"content\":[{\"type\":\"thinking\",\"thinking\":\"old plan\",\"thinkingSignature\":\"saved-signature\"},{\"type\":\"toolCall\",\"id\":\"image-call\",\"name\":\"read\",\"arguments\":{\"path\":\"image.png\"}}]},{\"role\":\"toolResult\",\"toolCallId\":\"image-call\",\"toolName\":\"read\",\"content\":[{\"type\":\"text\",\"text\":\"image result\"},{\"type\":\"image\",\"mimeType\":\"image/jpeg\",\"data\":\"BAUG\"}]}]}");json_object_object_add(json_object_array_get_idx(json_object_object_get(context,"messages"),2),"isError",json_object_new_boolean(1));json_object *old=json_object_array_get_idx(json_object_object_get(context,"messages"),1);json_object_object_add(old,"model",json_object_new_string(name));if(variant==4)json_object_object_add(json_object_array_get_idx(json_object_object_get(old,"content"),0),"thinking",json_object_new_string(" \n"));if(variant==3){json_object *thought=json_object_array_get_idx(json_object_object_get(old,"content"),0);json_object_object_add(thought,"redacted",json_object_new_boolean(1));json_object_object_add(thought,"thinkingSignature",json_object_new_string("AAE"));}if(variant==2)json_object_object_del(json_object_array_get_idx(json_object_object_get(old,"content"),0),"thinkingSignature");session.count=1;session.messages[0].role=PG_CONTEXT_VIEW;session.messages[0].context_view=context;setenv("PANGU_MODEL_API_KEY","bedrock-image-unit",1);assert(!start());assert(strstr(captured_post,"\"format\":\"png\"")&&strstr(captured_post,"\"format\":\"jpeg\"")&&strstr(captured_post,"\"bytes\":\"AQID\"")&&strstr(captured_post,"\"bytes\":\"BAUG\""));assert(!!strstr(captured_post,"Images from the error result")== (variant!=1&&variant!=3));assert(!!strstr(captured_post,"saved-signature")==!variant);if(variant==2||variant==3||variant==4)assert(!strstr(captured_post,"reasoningText"));else assert(strstr(captured_post,"reasoningText"));
  bedrock_feed("messageStart","{\"role\":\"assistant\"}",0);bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":2,\"delta\":{\"reasoningContent\":{\"text\":\"new plan\",\"signature\":\"opaque-\"}}}",0);bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":2,\"delta\":{\"reasoningContent\":{\"signature\":\"signature\"}}}",0);if(variant==3){assert(strstr(captured_post,"\"redactedContent\":\"AAE=\""));bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":2,\"delta\":{\"reasoningContent\":{\"redactedContent\":\"AAE=\"}}}",0);bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":2,\"delta\":{\"reasoningContent\":{\"redactedContent\":\"/w==\"}}}",0);bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":2,\"delta\":{\"reasoningContent\":{\"signature\":\"ignored-after-redaction\"}}}",0);}bedrock_feed("contentBlockDelta","{\"contentBlockIndex\":4,\"delta\":{\"text\":\"done\"}}",0);bedrock_feed("messageStop","{\"stopReason\":\"end_turn\"}",0);finish_transport();pg_model_event event={0};for(unsigned n=0;n<20;n++){assert(!nxt(&event));if(event.kind==PG_MODEL_DONE)break;}assert(event.kind==PG_MODEL_DONE);json_object *assistant=json_tokener_parse("{\"role\":\"assistant\"}");assert(!pg_mimo_complete(&m,assistant));json_object *thinking=json_object_array_get_idx(json_object_object_get(assistant,"content"),0);assert(!strcmp(json_object_get_string(json_object_object_get(thinking,"thinking")),variant==3?"new plan[Reasoning redacted]":"new plan")&&!strcmp(json_object_get_string(json_object_object_get(thinking,"thinkingSignature")),variant==3?"AAH/":"opaque-signature"));json_object *history=0;assert(!pg_mimo_responses_history(assistant,m.configuration,&history));if(variant==3)assert(!strcmp(json_object_get_string(json_object_object_get(json_object_array_get_idx(history,0),"data")),"AAH/"));else assert(strstr(json_object_to_json_string(history),"opaque-signature"));json_object_put(history);if(variant==3){assert(json_object_get_boolean(json_object_object_get(thinking,"redacted")));json_object_array_put_idx(json_object_object_get(context,"messages"),1,json_object_get(assistant));assert(!pg_mimo_encode_request(&m,&session));json_object *again=json_tokener_parse(m.request),*message=json_object_array_get_idx(json_object_object_get(again,"messages"),1),*block=json_object_array_get_idx(json_object_object_get(message,"content"),0);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(block,"reasoningContent"),"redactedContent")),"AAH/"));json_object_put(again);}json_object_put(assistant);model.close(model.context);json_object_put(context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Bedrock user/tool image bytes, signed and unsigned reasoning policy, split reasoning/signature stream and canonical replay");
}
static void test_response_identity(void){
 const char *headers[]={"X-Amzn-RequestId:  aws-public-id  \r\n","x-request-id: openai-public-id\r\n","Request-Id: anthropic-public-id\r\n"};const char *ids[]={"aws-public-id","openai-public-id","anthropic-public-id"};
 for(unsigned n=0;n<3;n++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"identity-test\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","identity-unit-key",1);assert(!start()&&captured_header&&captured_header_data==&m);const char *status="HTTP/1.1 200 OK\r\n";assert(captured_header((char *)status,1,strlen(status),captured_header_data)==strlen(status));assert(captured_header((char *)headers[n],1,strlen(headers[n]),captured_header_data)==strlen(headers[n]));json_object *assistant=json_tokener_parse("{\"role\":\"assistant\",\"stopReason\":\"error\",\"errorMessage\":\"provider failed\"}");assert(!pg_mimo_complete(&m,assistant));json_object *diagnostic=json_object_array_get_idx(json_object_object_get(assistant,"diagnostics"),0);assert(!strcmp(json_object_get_string(json_object_object_get(json_object_object_get(diagnostic,"details"),"requestId")),ids[n])&&json_object_get_double(json_object_object_get(diagnostic,"timestamp"))>0&&!strcmp(json_object_get_string(json_object_object_get(diagnostic,"type")),"bedrock_response_failure"));json_object_put(assistant);
  char excessive[240];memset(excessive,'x',sizeof(excessive));memcpy(excessive,"x-request-id: ",14);assert(captured_header(excessive,1,sizeof(excessive),captured_header_data)==sizeof(excessive)&&!m.response_request_id[0]);captured_header((char *)headers[n],1,strlen(headers[n]),captured_header_data);captured_header((char *)status,1,strlen(status),captured_header_data);assert(!m.response_request_id[0]);model.close(model.context);assert(!m.response_request_id[0]);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: public response request IDs captured case-insensitively, trimmed/bounded, failure-completed and reset per HTTP response/close");
}
static void test_request_deadline(void){
 const char *apis[]={"openai-completions","openai-responses","anthropic-messages","google-generative-ai","google-vertex","azure-openai-responses","bedrock-converse-stream"};
 for(unsigned n=0;n<7;n++){reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"model\":\"deadline-unit\",\"endpoint\":\"https://example.invalid/request\",\"options\":{\"timeoutMs\":1234}}");json_object_object_add(cfg,"api",json_object_new_string(apis[n]));assert(!pg_mimo_configure(&m,cfg));setenv("PANGU_MODEL_API_KEY","deadline-unit-key",1);assert(!start()&&captured_timeout==1234);model.cancel(model.context);assert(!m.easy&&!m.multi&&!m.headers&&!m.configuration);json_object_put(cfg);}
 const char *bad[]={"0","-1","1.5","true","\"1000\"","2147483648"};for(unsigned n=0;n<6;n++){reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);json_object *cfg=json_tokener_parse("{\"model\":\"deadline-unit\",\"endpoint\":\"https://example.invalid/request\"}");assert(!pg_mimo_configure(&m,cfg));json_object *old=m.configuration,*options=json_object_new_object();json_object_object_add(options,"timeoutMs",json_tokener_parse(bad[n]));json_object_object_add(cfg,"options",options);assert(pg_mimo_configure(&m,cfg)&&m.configuration==old);json_object_put(cfg);model.close(model.context);}unsetenv("PANGU_MODEL_API_KEY");puts("PASS: seven-protocol millisecond deadlines, cancellation cleanup and invalid deadline atomic rejection");
}
static void test_bedrock_errors(void){
 const char *names[]={"internalServerException","modelStreamErrorException","validationException","throttlingException","serviceUnavailableException"};const char *prefixes[]={"Internal server error","Model stream error","Validation error","Throttling error","Service unavailable"};
 for(unsigned n=0;n<10;n++){unsigned category=n%5;reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"test\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","error-unit",1);assert(!start());bedrock_feed("messageStart","{\"role\":\"assistant\"}",0);bedrock_feed(names[category],n<5?"{\"message\":\"provider detail\"}":"{\"message\":\"provider detail: Data Retention Mode unavailable\"}",0);finish_transport();pg_model_event event={0};int failed=0;for(unsigned i=0;i<5;i++)if(nxt(&event)){failed=1;break;}assert(failed&&!m.saw_done);pg_slice detail={0},body={0};assert(!pg_mimo_failure_data(&m,&detail,&body)&&detail.size&&strstr(detail.data,prefixes[category])&&strstr(detail.data,"provider detail"));assert(!!strstr(detail.data,"https://docs.aws.amazon.com/bedrock/latest/userguide/data-retention.html")== (n>=5));model.close(model.context);}unsetenv("PANGU_MODEL_API_KEY");puts("PASS: five Bedrock service exceptions preserve stable classification prefixes and raw detail, never success");
}
static void test_bedrock_regions(void){
 const char *sources[]={
 "{\"baseUrl\":\"https://bedrock-runtime.eu-central-1.amazonaws.com\"}",
 "{\"baseUrl\":\"https://bedrock-runtime.eu-central-1.amazonaws.com\",\"options\":{\"region\":\"us-west-2\"}}",
 "{\"baseUrl\":\"https://bedrock-vpc.example.com/proxy\",\"options\":{\"region\":\"ap-south-1\"}}",
 "{\"baseUrl\":\"https://bedrock-runtime-fips.us-gov-west-1.amazonaws.com\"}",
 "{\"model\":\"arn:aws:bedrock:us-west-2:123:application-inference-profile/abc\",\"options\":{\"region\":\"us-east-1\"}}",
 "{\"model\":\"arn:aws-us-gov:bedrock:us-gov-west-1:123:application-inference-profile/abc\",\"options\":{\"region\":\"us-east-1\"}}"};
 const char *regions[]={"eu-central-1","us-west-2","ap-south-1","us-gov-west-1","us-west-2","us-gov-west-1"};
 const char *bases[]={"https://bedrock-runtime.eu-central-1.amazonaws.com/","https://bedrock-runtime.us-west-2.amazonaws.com/","https://bedrock-vpc.example.com/proxy/","https://bedrock-runtime-fips.us-gov-west-1.amazonaws.com/","https://bedrock-runtime.us-west-2.amazonaws.com/","https://bedrock-runtime.us-gov-west-1.amazonaws.com/"};
 unsetenv("PANGU_MODEL_API_KEY");unsetenv("AWS_BEARER_TOKEN_BEDROCK");unsetenv("AWS_REGION");unsetenv("AWS_DEFAULT_REGION");setenv("AWS_ACCESS_KEY_ID","region-unit-access",1);setenv("AWS_SECRET_ACCESS_KEY","region-unit-secret",1);
 for(unsigned n=0;n<6;n++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse(sources[n]);json_object_object_add(cfg,"api",json_object_new_string("bedrock-converse-stream"));if(!json_object_object_get(cfg,"model"))json_object_object_add(cfg,"model",json_object_new_string("anthropic.claude-test"));assert(!pg_mimo_configure(&m,cfg)&&!json_object_object_get(cfg,"bedrockRegion"));json_object_put(cfg);assert(!start()&&!strncmp(captured_url,bases[n],strlen(bases[n])));char signature[128];snprintf(signature,sizeof(signature),"aws:amz:%s:bedrock",regions[n]);assert(!strcmp(signature,captured_sigv4));model.close(model.context);
 }
 unsetenv("AWS_ACCESS_KEY_ID");unsetenv("AWS_SECRET_ACCESS_KEY");puts("PASS: Bedrock standard/FIPS endpoint region, configured region override, custom proxy and partition ARN use the same region in URL and SigV4");
}
static void test_bedrock_history_ids(void){
 const char *ids[]={"call|native:1","汉字😀/call","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-long-id"};
 const char *normalized[]={"call_native_1","_____call","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-l"};
 for(unsigned same=0;same<2;same++)for(unsigned n=0;n<3;n++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-test\",\"modelProvider\":\"bedrock\"}");assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);json_object *context=json_tokener_parse("{\"messages\":[{\"role\":\"assistant\",\"api\":\"bedrock-converse-stream\",\"provider\":\"bedrock\",\"content\":[{\"type\":\"toolCall\",\"name\":\"read\",\"arguments\":{}}]},{\"role\":\"toolResult\",\"toolName\":\"read\",\"content\":[{\"type\":\"text\",\"text\":\"ok\"}]}]}");json_object *messages=json_object_object_get(context,"messages"),*assistant=json_object_array_get_idx(messages,0),*call=json_object_array_get_idx(json_object_object_get(assistant,"content"),0),*result=json_object_array_get_idx(messages,1);json_object_object_add(assistant,"model",json_object_new_string(same?"anthropic.claude-test":"foreign-model"));json_object_object_add(call,"id",json_object_new_string(ids[n]));json_object_object_add(result,"toolCallId",json_object_new_string(ids[n]));session.count=1;session.messages[0].role=PG_CONTEXT_VIEW;session.messages[0].context_view=context;assert(!pg_mimo_encode_request(&m,&session));json_object *wire=json_tokener_parse(m.request),*rows=json_object_object_get(wire,"messages"),*use=json_object_object_get(json_object_array_get_idx(json_object_object_get(json_object_array_get_idx(rows,0),"content"),0),"toolUse"),*reply=json_object_object_get(json_object_array_get_idx(json_object_object_get(json_object_array_get_idx(rows,1),"content"),0),"toolResult");const char *wanted=same?ids[n]:normalized[n];assert(!strcmp(json_object_get_string(json_object_object_get(use,"toolUseId")),wanted)&&!strcmp(json_object_get_string(json_object_object_get(reply,"toolUseId")),wanted));assert(!strstr(m.request,"panguForeignToolId")&&!strcmp(json_object_get_string(json_object_object_get(call,"id")),ids[n])&&!strcmp(json_object_get_string(json_object_object_get(result,"toolCallId")),ids[n]));json_object_put(wire);json_object_put(context);model.close(model.context);
 }
 puts("PASS: Bedrock foreign tool/result IDs normalize together by UTF-16 units and 64-character limit; same-model IDs and stored context remain unchanged");
}
static void test_bedrock_policies(void){
 for(unsigned none=0;none<2;none++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-opus-4-8\",\"modelInfo\":{\"compat\":{\"supportsStrictMode\":true}},\"options\":{}}");json_object_object_add(json_object_object_get(cfg,"options"),"toolChoice",json_object_new_string(none?"none":"any"));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);pg_tool_options strict={.constrained_sampling="{\"type\":\"json_schema\",\"strict\":\"require\"}"};pg_tool_registry registry={.entries={{.name="check",.description="Check",.schema="{\"type\":\"object\",\"$ref\":\"unsupported\"}",.options=&strict}},.count=1};pg_mimo_bind_tools(&m,&registry);setenv("PANGU_MODEL_API_KEY","bedrock-policy-unit",1);if(none){assert(!start()&&!strstr(captured_post,"toolConfig"));}else assert(start());model.close(model.context);
 }
 for(unsigned variant=0;variant<5;variant++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-opus-4-8\"}");json_object_object_add(cfg,"modelInfo",json_tokener_parse("{\"cost\":{\"input\":3,\"output\":15,\"cacheRead\":0.3,\"cacheWrite\":3.75,\"tiers\":[{\"inputTokensAbove\":1000000,\"input\":6,\"output\":30,\"cacheRead\":0.6,\"cacheWrite\":7.5}]}}"));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);assert(!start());bedrock_feed("messageStart","{\"role\":\"assistant\"}",0);
  json_object *metadata=json_tokener_parse("{\"usage\":{\"inputTokens\":100,\"outputTokens\":5,\"totalTokens\":1000105,\"cacheReadInputTokens\":10,\"cacheWriteInputTokens\":1000000,\"cacheDetails\":[{\"ttl\":\"1h\",\"inputTokens\":150000},{\"ttl\":\"5m\",\"inputTokens\":600000},{\"ttl\":\"1h\",\"inputTokens\":250000}]}}");json_object *usage=json_object_object_get(metadata,"usage");if(variant==2)json_object_object_del(usage,"totalTokens");if(variant==4)json_object_object_add(usage,"cacheDetails",json_object_new_object());bedrock_feed("metadata",json_object_to_json_string_ext(metadata,JSON_C_TO_STRING_PLAIN),0);json_object_put(metadata);bedrock_feed("messageStop","{\"stopReason\":\"end_turn\"}",0);if(variant==1)bedrock_feed("metadata","{\"usage\":{\"outputTokens\":7}}",0);if(variant==3)bedrock_feed("metadata","{}",0);finish_transport();pg_model_event event={0};int status=0;for(unsigned n=0;n<12;n++){status=nxt(&event);if(status||event.kind==PG_MODEL_DONE)break;}if(variant==4){assert(status);model.close(model.context);continue;}assert(!status&&event.kind==PG_MODEL_DONE);json_object *assistant=json_tokener_parse("{\"role\":\"assistant\"}");assert(!pg_mimo_complete(&m,assistant));usage=json_object_object_get(assistant,"usage");assert(json_object_get_int64(json_object_object_get(usage,"input"))==(variant==1?0:100)&&json_object_get_int64(json_object_object_get(usage,"output"))==(variant==1?7:5)&&json_object_get_int64(json_object_object_get(usage,"cacheRead"))==(variant==1?0:10)&&json_object_get_int64(json_object_object_get(usage,"cacheWrite"))==(variant==1?0:1000000)&&json_object_get_int64(json_object_object_get(usage,"cacheWrite1h"))==(variant==1?0:400000)&&json_object_get_int64(json_object_object_get(usage,"totalTokens"))==(variant==1?7:variant==2?105:1000105));json_object *cost=json_object_object_get(usage,"cost");assert(cost&&!json_object_get_boolean(json_object_object_get(cost,"totalFromProvider")));double expected_input=variant==1?0:0.0006,expected_output=variant==1?0.000105:0.00015,expected_write=variant==1?0:9.3;assert(fabs(json_object_get_double(json_object_object_get(cost,"input"))-expected_input)<1e-12&&fabs(json_object_get_double(json_object_object_get(cost,"output"))-expected_output)<1e-12&&fabs(json_object_get_double(json_object_object_get(cost,"cacheWrite"))-expected_write)<1e-12);json_object_put(assistant);model.close(model.context);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: Bedrock none omits strict-invalid declarations; reported/fallback totals, 1h writes, missing/partial/trailing usage and malformed metadata");
}
static void test_azure_configuration(void){
 const char *bases[]={" https://r.openai.azure.com/ ","https://r.cognitiveservices.azure.com/openai/","https://r.services.ai.azure.com/openai/v1/responses?old=yes","https://proxy.invalid/v1?api-version=old&custom=true&api-version=old2","https://r.openai.azure.com/custom/path/"};
 const char *expected[]={"https://r.openai.azure.com/openai/v1/responses?api-version=v1","https://r.cognitiveservices.azure.com/openai/v1/responses?api-version=v1","https://r.services.ai.azure.com/openai/v1/responses?api-version=v1","https://proxy.invalid/v1/responses?custom=true&api-version=v1","https://r.openai.azure.com/custom/path/responses?api-version=v1"};
 for(unsigned n=0;n<5;n++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"api\":\"azure-openai-responses\",\"model\":\"logical-model\"}");if(!n)json_object_object_add(cfg,"options",json_tokener_parse("{\"azureBaseUrl\":\" \\t \"}"));setenv("AZURE_OPENAI_BASE_URL",bases[n],1);setenv("AZURE_OPENAI_DEPLOYMENT_NAME_MAP","invalid,other=unused, logical-model = first ,logical-model= final-deployment=ignored",1);assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","azure-config-key",1);assert(!start()&&!strcmp(captured_url,expected[n])&&strstr(captured_post,"final-deployment"));model.close(model.context);
 }
 unsetenv("AZURE_OPENAI_BASE_URL");unsetenv("AZURE_OPENAI_DEPLOYMENT_NAME_MAP");
 reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);json_object *cfg=json_tokener_parse("{\"api\":\"azure-openai-responses\",\"model\":\"logical-model\",\"options\":{\"azureResourceName\":\"pangu\",\"azureApiVersion\":\"2025-01-preview\",\"azureDeploymentName\":\"explicit\"}}");setenv("AZURE_OPENAI_DEPLOYMENT_NAME_MAP","logical-model=mapped",1);assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);assert(strstr(json_object_get_string(json_object_object_get(m.configuration,"endpoint")),"api-version=2025-01-preview")&&!strcmp(json_object_get_string(json_object_object_get(m.configuration,"azureDeploymentName")),"explicit"));json_object *old=m.configuration;cfg=json_tokener_parse("{\"api\":\"azure-openai-responses\",\"model\":\"logical-model\",\"baseUrl\":\"not-a-url\"}");assert(pg_mimo_configure(&m,cfg)&&m.configuration==old);json_object_put(cfg);model.close(model.context);unsetenv("AZURE_OPENAI_DEPLOYMENT_NAME_MAP");unsetenv("PANGU_MODEL_API_KEY");
 puts("PASS: Azure upstream root/path/query normalization, proxy preservation, deployment map last-wins, explicit override, API version and failed-config retention");
}
static void test_provider_headers(void){
 const char *apis[]={"openai-completions","openai-responses","anthropic-messages","google-generative-ai","google-vertex"};
 for(unsigned protocol=0;protocol<5;protocol++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();
  char source[2048];snprintf(source,sizeof(source),"{\"api\":\"%s\",\"model\":\"header-model\",\"endpoint\":\"https://example.invalid/stream\",\"modelInfo\":{\"headers\":{\"X-Route\":\"model\",\"X-Removed\":\"model\"}},\"headers\":{\"x-ROUTE\":\"root\",\"x-removed\":null},\"options\":{\"headers\":{\"X-Route\":\"request\",\"X-Empty\":\"\",\"Content-Type\":\"application/custom+json\"}}}",apis[protocol]);
  json_object *cfg=json_tokener_parse(source);assert(cfg&&!pg_mimo_configure(&m,cfg));json_object_put(cfg);setenv("PANGU_MODEL_API_KEY","header-unit-key",1);assert(!start());
  unsigned route=0,empty=0,type=0;for(const struct curl_slist *h=captured_headers;h;h=h->next){assert(!strstr(h->data,"removed")&&!strstr(h->data,"model")&&!strstr(h->data,"root"));route+=!strcmp(h->data,"x-route: request");empty+=!strcmp(h->data,"x-empty;");type+=!strcmp(h->data,"content-type: application/custom+json");assert(strcmp(h->data,"Content-Type: application/json"));}assert(route==1&&empty==1&&type==1);model.close(model.context);
  unsigned append_count=(unsigned)slist_calls;
  for(unsigned fault=1;fault<=append_count;fault++){
   reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();cfg=json_tokener_parse(source);assert(cfg&&!pg_mimo_configure(&m,cfg));json_object_put(cfg);slist_fail_at=(int)fault;assert(start());model.close(model.context);assert(!m.headers&&!m.easy&&!m.multi);
  }

 }
 const char *invalid[]={"{\"bad:name\":\"value\"}","{\"x-test\":\"a\\r\\nb\"}","{\"x-test\":42}","{\"x-test\":\"a\\u0000b\"}","[]"};
 for(unsigned n=0;n<sizeof(invalid)/sizeof(*invalid);n++){
  reset_script();m=(pg_mimo){0};model=pg_mimo_model(&m);make_session();json_object *cfg=json_tokener_parse("{\"model\":\"header-model\",\"endpoint\":\"https://example.invalid/chat/completions\"}");json_object_object_add(cfg,"headers",json_tokener_parse(invalid[n]));assert(!pg_mimo_configure(&m,cfg));json_object_put(cfg);assert(start());model.close(model.context);assert(!m.headers&&!m.easy&&!m.multi);
 }
 unsetenv("PANGU_MODEL_API_KEY");puts("PASS: five-protocol provider headers merge by case, layer precedence, null removal, empty value, default replacement and invalid-input cleanup");
}
int main(int argc,char **argv) {
    if(argc==2&&!strcmp(argv[1],"--codex-wire"))return codex_compressed_wire();
    if(argc==2&&!strcmp(argv[1],"--sigv4-wire"))return bedrock_sigv4_wire();
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
    test_codex_auth_failures();
    test_codex_stream_failures();
    test_codex_header_deadlines();
    test_codex_terminal_frames();
    test_codex_failure_display();
    test_codex_transport_retries();
    test_codex_compression_failures();
    test_responses_arguments();
    test_anthropic_transport();
    test_anthropic_beta_headers();
    test_anthropic_feature_headers();
    test_anthropic_affinity();
    test_anthropic_strict_retry();
    test_google_transport();
    test_google_endpoint_config();
    test_provider_headers();
    test_azure_configuration();
    test_bedrock_transport();
    test_copilot_gateway();
    test_bedrock_auth();
    test_bedrock_profiles();
    test_bedrock_proxy_modes();
    test_bedrock_multimodal();
    test_bedrock_policies();
    test_bedrock_history_ids();
    test_bedrock_regions();
    test_bedrock_errors();
    test_request_deadline();
    test_response_identity();
    puts("PASS: mimo transport lifecycle via scripted curl stand-in (no network): line split/CRLF, "
         "terminal ordering, HTTP/curl failures, receive overflow/capacity, partial-init close, idempotent close, atomic 2/8-call batches and rejection/faults");
    return 0;
}
