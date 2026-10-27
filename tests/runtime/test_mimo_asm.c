#define _POSIX_C_SOURCE 200809L
#include "../../src/runtime/mimo.h"
#include "../../src/runtime/mimo_layout.h"
#include <json-c/json.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* 布局头数值与公开头一致由 mimo_layout.h 静态断言承担；此处为纯运行时证据。 */
static pg_mimo m;
static pg_model_event e;
static pg_session session;

static int line(const char *s) {
    memset(&e, 0x5a, sizeof e); /* 事件必须整体清零，不留 0x5a 残迹 */
    return pg_mimo_decode_line(&m, s, &e);
}
static void reset(void) { m = (pg_mimo){0}; }
static json_object *root_of(const char *s) { return json_tokener_parse(s); }
static json_object *get(json_object *o, const char *k) {
    json_object *v = 0;
    json_object_object_get_ex(o, k, &v);
    return v;
}

static void test_model_init(void) {
    static pg_mimo mm, zero;
    pg_model model;
    memset(&mm, 0xa5, sizeof mm);
    model = pg_mimo_model(&mm);
    assert(!memcmp(&mm, &zero, sizeof mm));
    assert(model.context == &mm);
    assert(model.start && model.next && model.cancel && model.close);
    model.close(model.context); /* 全零状态关闭必须安全 */
    model.close(model.context); /* 重复关闭幂等 */
    pg_mimo_bind_tools(&mm, (const pg_tool_registry *)0x1234);
    assert(mm.tools == (const pg_tool_registry *)0x1234);
}

static void test_projection_model_env(void) {
    pg_mimo p = (pg_mimo){0};
    session = (pg_session){0};
    session.count = 1;
    session.messages[0].role = PG_USER;
    session.messages[0].size = 5;
    memcpy(session.messages[0].text, "hello", 6);
    setenv("MIMO_MODEL", "unit-model-x", 1);
    assert(!pg_mimo_encode_request(&p, &session));
    json_object *root = root_of(p.request);
    assert(root);
    assert(!strcmp(json_object_get_string(get(root, "model")), "unit-model-x"));
    json_object *messages = get(root, "messages");
    assert(json_object_array_length(messages) == 1);
    json_object_put(root);
    unsetenv("MIMO_MODEL");
    free(p.request);
    p.request = 0;
    assert(!pg_mimo_encode_request(&p, &session));
    root = root_of(p.request);
    assert(!strcmp(json_object_get_string(get(root, "model")), "mimo-v2.6-pro"));
    json_object *stream = get(root, "stream");
    assert(json_object_is_type(stream, json_type_boolean) && json_object_get_boolean(stream));
    assert(json_object_get_int(get(root, "max_tokens")) == 512);
    assert(!strcmp(json_object_get_string(get(get(root, "thinking"), "type")), "disabled"));
    json_object_put(root);
    free(p.request);
}

static void test_projection_rejections(void) {
    pg_mimo p = (pg_mimo){0};
    session = (pg_session){0};
    session.count = 1;
    session.messages[0].role = PG_USER;
    session.messages[0].size = 1;
    session.messages[0].text[0] = 'x';
    /* 入口拒绝不动旧 request */
    char *kept = p.request;
    assert(pg_mimo_encode_request(&p, 0));
    assert(pg_mimo_encode_request(0, &session));
    session.count = PG_MESSAGE_LIMIT + 1;
    assert(pg_mimo_encode_request(&p, &session));
    session.count = 1;
    session.system_size = PG_TEXT_LIMIT;
    assert(pg_mimo_encode_request(&p, &session));
    assert(p.request == kept);
    session.system_size = 0;
    /* 文本达到上限（size 为字节数，等于上限即拒绝） */
    session.messages[0].size = PG_TEXT_LIMIT;
    assert(pg_mimo_encode_request(&p, &session));
    session.messages[0].size = PG_TEXT_LIMIT - 1;
    assert(!pg_mimo_encode_request(&p, &session));
    free(p.request);
    p.request = 0;
    /* 角色越界（负值与超界） */
    session.messages[0].size = 1;
    session.messages[0].role = (pg_role)9;
    assert(pg_mimo_encode_request(&p, &session));
    session.messages[0].role = (pg_role)-1;
    assert(pg_mimo_encode_request(&p, &session));
    assert(!p.request);
    /* 未配对的 tool_call（末尾草稿不得回送模型） */
    session.messages[0].role = PG_TOOL_CALL;
    session.messages[0].size = 2;
    memcpy(session.messages[0].text, "{}", 3);
    strcpy(session.messages[0].call_id, "c9");
    strcpy(session.messages[0].tool_name, "read");
    assert(pg_mimo_encode_request(&p, &session));
    /* call_id 未在界内 NUL 终止 */
    memset(session.messages[0].call_id, 'c', PG_ID_LIMIT);
    session.count = 2;
    session.messages[1] = (pg_message){0};
    session.messages[1].role = PG_TOOL_RESULT;
    assert(pg_mimo_encode_request(&p, &session));
    memset(session.messages[0].call_id, 0, PG_ID_LIMIT);
    strcpy(session.messages[0].call_id, "c9");
    /* 空工具名拒绝 */
    session.messages[0].tool_name[0] = 0;
    assert(pg_mimo_encode_request(&p, &session));
    strcpy(session.messages[0].tool_name, "read");
    /* 结果 id 与调用 id 不一致 */
    session.messages[1].size = 2;
    memcpy(session.messages[1].text, "ok", 3);
    strcpy(session.messages[1].call_id, "zz");
    assert(pg_mimo_encode_request(&p, &session));
    strcpy(session.messages[1].call_id, "c9");
    assert(!pg_mimo_encode_request(&p, &session));
    json_object *root = root_of(p.request);
    json_object *messages = get(root, "messages");
    /* assistant(tool_calls) 与配对 tool result 各一条 */
    assert(json_object_array_length(messages) == 2);
    json_object *assistant = json_object_array_get_idx(messages, 0);
    assert(!strcmp(json_object_get_string(get(assistant, "role")), "assistant"));
    json_object *tool_result = json_object_array_get_idx(messages, 1);
    assert(!strcmp(json_object_get_string(get(tool_result, "role")), "tool"));
    assert(!strcmp(json_object_get_string(get(tool_result, "tool_call_id")), "c9"));
    json_object_put(root);
    free(p.request);
}

static void test_plain_conversation_projection(void){
 pg_mimo provider={0};session=(pg_session){0};
 const char *texts[]={"hello","answer","follow up"};const pg_role roles[]={PG_USER,PG_ASSISTANT,PG_USER};
 for(size_t i=0;i<3;++i){session.messages[i].role=roles[i];session.messages[i].size=strlen(texts[i]);strcpy(session.messages[i].text,texts[i]);}
 for(size_t count=2;count<=3;++count){session.count=count;assert(!pg_mimo_encode_request(&provider,&session));json_object *root=root_of(provider.request);json_object *messages=get(root,"messages");assert(json_object_array_length(messages)==count);assert(!strcmp(json_object_get_string(get(json_object_array_get_idx(messages,1),"role")),"assistant"));assert(!strcmp(json_object_get_string(get(json_object_array_get_idx(messages,1),"content")),"answer"));json_object_put(root);free(provider.request);provider.request=0;}
}

static void test_decode_terminators(void) {
    reset();
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\\u0000other\"}]}"));
    assert(!m.finish_reason);

    reset();
    /* [DONE] 前无 finish 拒绝；重复 [DONE] 拒绝；DONE 后数据拒绝 */
    assert(line("data: [DONE]"));
    assert(!line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}"));
    assert(m.finish_reason == 1);
    assert(!line("data: [DONE]"));
    assert(line("data: [DONE]"));
    assert(line("data: {\"choices\":[{\"delta\":{}}]}"));
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}"));
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}"));
    reset();
    /* 未知结束理由拒绝 */
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"length\"}]}"));
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"content_filter\"}]}"));
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}")); /* 无 id/name */
    reset();
    /* stop 带工具 id 拒绝；tool_calls 缺 id/name 拒绝 */
    assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"i1\",\"function\":{\"name\":\"n\"}}]}}]}"));
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}"));
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"i1\"}]}}]}"));
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}"));
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"name\":\"n\",\"arguments\":\"{}\"}}]}}]}"));
    assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}"));
}

static void test_decode_structure(void) {
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{\"content\":\"hi\"}}]}"));
    assert(e.kind == PG_MODEL_TEXT && e.text.size == 2 && e.text.data == m.text);
    /* usage 空分片：WAIT 成功 */
    assert(!line("data: {\"choices\":[]}"));
    assert(e.kind == PG_MODEL_WAIT && !e.text.data && !e.call_id.data && !e.tool_name.data);
    /* 空/注释行 */
    assert(!line(""));
    assert(!line(": keep-alive"));
    /* 非 data 行拒绝 */
    assert(line("event: ping"));
    assert(line("data"));
    reset();
    /* 结构非法：choices 非数组/多 choice/双份 finish */
    assert(line("data: {\"choices\":{}}"));
    reset();
    assert(line("data: {\"choices\":[{\"delta\":{}},{\"delta\":{}}]}"));
    reset();
    /* JSON 后残余与截断；纯空白残余允许 */
    assert(line("data: {\"choices\":[{\"delta\":{}}]}junk"));
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{}}]} "));
    assert(line("data: broken"));
    assert(line("data: "));
    reset();
    /* 嵌入 NUL 的解码文本/参数拒绝（长度按解码字节检查） */
    assert(line("data: {\"choices\":[{\"delta\":{\"content\":\"a\\u0000b\"}}]}"));
    assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"x\\u0000y\"}}]}}]}"));
    /* content 非字符串拒绝 */
    reset();
    assert(line("data: {\"choices\":[{\"delta\":{\"content\":7}}]}"));
    /* index 非法/不完整：缺失/类型错，索引1空分片不能完成整组 */
    reset();
    assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"function\":{\"arguments\":\"{}\"}}]}}]}"));
    reset();
    assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":\"0\"}]}},{\"delta\":{}}]}"));
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1}]}}]}"));assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}"));
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0},{\"index\":1}]}}]}"));assert(line("data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}"));
    reset();
    assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":{}}}]}"));
    /* 空字符串分片：成功但不产生 TEXT 事件 */
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{\"content\":\"\"}}]}"));
    assert(e.kind == PG_MODEL_WAIT);
}

static void test_decode_capacity(void) {
    reset();
    /* text 分片：4095 接受、4096 拒绝 */
    char buf[4096 + 64];
    buf[0] = 0;
    strcpy(buf, "data: {\"choices\":[{\"delta\":{\"content\":\"");
    size_t prefix = strlen(buf);
    memset(buf + prefix, 'a', PG_TEXT_LIMIT);
    strcpy(buf + prefix + PG_TEXT_LIMIT - 1, "\"}}]}");
    assert(!line(buf));
    assert(e.kind == PG_MODEL_TEXT && e.text.size == PG_TEXT_LIMIT - 1);
    strcpy(buf + prefix + PG_TEXT_LIMIT, "\"}}]}");
    assert(line(buf));
    reset();
    /* id 增量追加到界：63+1 拒绝；name 同界 */
    memset(m.id, 'i', PG_ID_LIMIT - 1);
    assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"x\"}]}}]}"));
    memset(m.id, 0, sizeof m.id);
    memset(m.name, 'n', PG_ID_LIMIT - 1);
    assert(line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"name\":\"x\"}}]}}]}"));
    reset();
    /* null 视为缺席不追加 */
    memset(m.id, 0, sizeof m.id);
    strcpy(m.id, "keep");
    assert(!line("data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":null}]}}]}"));
    assert(!strcmp(m.id, "keep"));
}

static void test_event_borrow(void) {
    reset();
    assert(!line("data: {\"choices\":[{\"delta\":{\"content\":\"first\"}}]}"));
    assert(e.kind == PG_MODEL_TEXT && e.text.size == 5);
    /* 借用缓冲：下一次解码覆写后旧事件切片指向新内容（契约：过期） */
    assert(!line("data: {\"choices\":[{\"delta\":{\"content\":\"second\"}}]}"));
    assert(!strncmp(e.text.data, "second", 6));
}

static void test_abi_probes(void) {
    reset();
    extern unsigned pg_mimo_abi_probe3(void (*)(void), void *, void *, void *);
    extern unsigned pg_mimo_abi_probe2(void (*)(void), void *, void *);
    extern void pg_mimo_abi_corrupt(void *, const void *);
    /* 探针自身必须能发现破坏（负例） */
    assert(pg_mimo_abi_probe3((void (*)(void))pg_mimo_abi_corrupt, 0, 0, 0) != 0);
    assert(pg_mimo_abi_probe2((void (*)(void))pg_mimo_abi_corrupt, 0, 0) != 0);
    reset();
    assert(pg_mimo_abi_probe3((void (*)(void))pg_mimo_decode_line, &m,
                              "data: {\"choices\":[{\"delta\":{\"content\":\"probe\"}}]}", &e) == 0);
    assert(e.kind == PG_MODEL_TEXT && e.text.size == 5);
    session = (pg_session){0};
    session.count = 1;
    session.messages[0].role = PG_USER;
    session.messages[0].size = 4;
    memcpy(session.messages[0].text, "ping", 5);
    assert(pg_mimo_abi_probe2((void (*)(void))pg_mimo_encode_request, &m, &session) == 0);
    free(m.request);
    m.request = 0;
}

static void test_page_tail(void) {
    /* 输入行与输出事件各自贴保护页：任何越过 NUL/56 字节的读写下探即崩溃 */
    long page = sysconf(_SC_PAGESIZE);
    char *input_map = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char *event_map = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(input_map != MAP_FAILED && event_map != MAP_FAILED);
    assert(!mprotect(input_map + page, (size_t)page, PROT_NONE));
    assert(!mprotect(event_map + page, (size_t)page, PROT_NONE));
    char *input_tail = input_map + page;
    const char *lines[] = {
        "data: {\"choices\":[{\"delta\":{\"content\":\"page-tail\"}}]}",
        "data: [DONE]",
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"pt\",\"function\":{\"name\":\"read\",\"arguments\":\"{}\"}}]}}]}",
        ": comment-at-page-tail",
    };
    const int expected[] = {0, 1, 0, 0}; /* [DONE] 无 finish 前置，按协议拒绝 */
    pg_model_event *ev = (pg_model_event *)(event_map + page - sizeof *ev);
    for (size_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
        reset();
        size_t n = strlen(lines[i]);
        memcpy(input_tail - n - 1, lines[i], n + 1); /* NUL 恰为可读页最后一字节 */
        memset(ev, 0x5a, sizeof *ev);                /* 事件末字节贴保护页 */
        int r = pg_mimo_decode_line(&m, input_tail - n - 1, ev);
        assert(r == expected[i]);
        assert(ev->kind == PG_MODEL_WAIT || ev->kind == PG_MODEL_TEXT);
        assert(ev->text.size <= 9);
    }
    assert(!munmap(input_map, (size_t)page * 2));
    assert(!munmap(event_map, (size_t)page * 2));
}

static void test_empty_tool_text(void){
 const char *cases[]={""," ","\t\r\n", "\xc2\xa0", "\xe1\x9a\x80", "\xe2\x80\x80", "\xe2\x80\x8a", "\xe2\x80\xa8", "\xe2\x80\xa9", "\xe2\x80\xaf", "\xe2\x81\x9f", "\xe3\x80\x80", "\xef\xbb\xbf", " x ", "\xc2\x85", "\xe2\x80\x8b"};
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);i++){
  session=(pg_session){0};session.count=3;session.messages[0].role=PG_USER;session.messages[0].size=1;memcpy(session.messages[0].text,"x",2);
  pg_message *call=&session.messages[1],*result=&session.messages[2];call->role=PG_TOOL_CALL;call->size=2;memcpy(call->text,"{}",3);strcpy(call->call_id,"call");strcpy(call->tool_name,"bash");result->role=PG_TOOL_RESULT;strcpy(result->call_id,"call");result->size=strlen(cases[i]);memcpy(result->text,cases[i],result->size+1);
  pg_mimo provider={0};assert(!pg_mimo_encode_request(&provider,&session));json_object *root=root_of(provider.request),*messages=get(root,"messages");const char *text=json_object_get_string(get(json_object_array_get_idx(messages,2),"content"));assert(!strcmp(text,i<13?"(no tool output)":cases[i]));json_object_put(root);free(provider.request);
 }
}

static void test_saved_configuration(void){
 pg_mimo provider={0};pg_model model=pg_mimo_model(&provider);session=(pg_session){0};session.count=1;session.messages[0].role=PG_USER;session.messages[0].size=1;strcpy(session.messages[0].text,"x");
 json_object *configuration=json_tokener_parse("{\"model\":\"saved-model\",\"endpoint\":\"https://saved.invalid/chat/completions\",\"options\":{\"maxTokens\":768,\"temperature\":0.25,\"maxTokensField\":\"max_completion_tokens\"}}");assert(configuration&&!pg_mimo_configure(&provider,configuration));
 assert(!json_object_object_add(configuration,"model",json_object_new_string("mutated-model")));json_object_put(configuration);
 assert(!setenv("PANGU_MODEL_NAME","ambient-model",1));assert(!pg_mimo_encode_request(&provider,&session));json_object *body=root_of(provider.request);assert(!strcmp(json_object_get_string(get(body,"model")),"saved-model"));assert(!get(body,"max_tokens")&&json_object_get_int(get(body,"max_completion_tokens"))==768&&json_object_get_double(get(body,"temperature"))==0.25);json_object_put(body);
 json_object *retained=provider.configuration;configuration=json_tokener_parse("{\"model\":\"other\",\"endpoint\":\"https://other.invalid\",\"options\":{\"maxTokens\":0}}");assert(pg_mimo_configure(&provider,configuration)&&provider.configuration==retained);json_object_put(configuration);
 configuration=json_tokener_parse("{\"model\":\"nul\\u0000model\",\"endpoint\":\"https://other.invalid\"}");assert(pg_mimo_configure(&provider,configuration)&&provider.configuration==retained);json_object_put(configuration);
 model.close(model.context);assert(!provider.configuration&&!provider.request);model.close(model.context);unsetenv("PANGU_MODEL_NAME");
}

int main(void) {
    test_model_init();
    test_projection_model_env();
    test_empty_tool_text();
    test_saved_configuration();
    test_projection_rejections();
    test_plain_conversation_projection();
    test_decode_terminators();
    test_decode_structure();
    test_decode_capacity();
    test_event_borrow();
    test_abi_probes();
    test_page_tail();
    puts("PASS: mimo asm init/projection bounds, SSE terminator/structure/capacity rejection, event borrow, ABI probes, page-tail guards");
    return 0;
}
