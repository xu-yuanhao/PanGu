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

int main(void) {
    test_start_setup();
    test_text_stream();
    test_tool_stream();
    test_transport_failures();
    test_receive_bounds();
    test_partial_init_close();
    test_setup_failures();
    puts("PASS: mimo transport lifecycle via scripted curl stand-in (no network): line split/CRLF, "
         "terminal ordering, HTTP/curl failures, receive overflow/capacity, partial-init close, idempotent close");
    return 0;
}
