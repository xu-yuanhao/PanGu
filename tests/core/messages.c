#include <pangu/core/core.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) abort(); } while (0)
typedef struct { size_t calls, fail, live; } memory;
static void *alloc(void *ctx, size_t n) {
    memory *m = ctx;
    if (++m->calls == m->fail) return NULL;
    void *p = malloc(n); if (p) ++m->live; return p;
}
static void dealloc(void *ctx, void *p) { if (p) { --((memory *)ctx)->live; free(p); } }
int main(void) {
    for (size_t fail = 1; fail <= 5; ++fail) {
        memory mem = {.fail = fail};
        pangu_core_config cfg = {.allocator = {&mem, alloc, dealloc}};
        pangu_core *c = NULL;
        pangu_status s = pangu_core_create(&cfg, &c);
        if (s == PANGU_OK) {
            char bytes[] = {'a', 0, 'b'};
            pangu_content block = {.kind = PANGU_TEXT, .data = {bytes, sizeof(bytes)}};
            pangu_message msg = {.role = PANGU_ASSISTANT, .content = &block, .content_count = 1,
                .response_model={bytes,sizeof(bytes)},.response_id={bytes,sizeof(bytes)},
                .raw_stop_reason={bytes,sizeof(bytes)},.diagnostics_json={bytes,sizeof(bytes)},
                .deferred_json={bytes,sizeof(bytes)},.nested_calls_json={bytes,sizeof(bytes)},.has_end_turn=1};
            s = pangu_core_append(c, &msg);
            if (s == PANGU_OK) {
                bytes[0] = 'z';
                const pangu_message *saved = pangu_core_message_at(c, 0);
                CHECK(saved->content[0].data.len == 3);
                CHECK(memcmp(saved->content[0].data.data, "a\0b", 3) == 0);
                CHECK(saved->response_model.len==3 && !memcmp(saved->response_model.data,"a\0b",3));
                CHECK(saved->response_id.len==3 && !memcmp(saved->response_id.data,"a\0b",3));
                CHECK(saved->raw_stop_reason.len==3 && !memcmp(saved->raw_stop_reason.data,"a\0b",3));
                CHECK(saved->diagnostics_json.len==3 && !memcmp(saved->diagnostics_json.data,"a\0b",3));
                CHECK(saved->deferred_json.len==3 && !memcmp(saved->deferred_json.data,"a\0b",3));
                CHECK(saved->nested_calls_json.len==3 && !memcmp(saved->nested_calls_json.data,"a\0b",3));
                CHECK(saved->has_end_turn && !saved->end_turn);
                for (int i = 0; i < 100; ++i) {
                    size_t old = pangu_core_message_count(c);
                    s = pangu_core_append(c, &msg);
                    CHECK(s == PANGU_OK || s == PANGU_NOMEM);
                    CHECK(pangu_core_message_count(c) == old + (s == PANGU_OK));
                }
                CHECK(saved == pangu_core_message_at(c, 0));
            } else CHECK(pangu_core_message_count(c) == 0);
            CHECK(pangu_core_reset(c) == PANGU_OK);
            CHECK(!pangu_core_message_at(c, 0));
            msg.content_count = SIZE_MAX;
            CHECK(pangu_core_append(c, &msg) == PANGU_INVALID);
        } else CHECK(s == PANGU_NOMEM && !c);
        pangu_core_destroy(c);
        CHECK(mem.live == 0);
    }
    pangu_core *core=NULL; CHECK(pangu_core_create(NULL,&core)==PANGU_OK);
    for (int field=0;field<6;++field) {
        pangu_message invalid={.role=PANGU_ASSISTANT};
        if (field==0) invalid.response_model=(pangu_slice){NULL,1};
        if (field==1) invalid.response_id=(pangu_slice){NULL,1};
        if (field==2) invalid.raw_stop_reason=(pangu_slice){NULL,1};
        if (field==3) invalid.diagnostics_json=(pangu_slice){NULL,1};
        if (field==4) invalid.deferred_json=(pangu_slice){NULL,1};
        if (field==5) invalid.nested_calls_json=(pangu_slice){NULL,1};
        CHECK(pangu_core_append(core,&invalid)==PANGU_INVALID);
        CHECK(pangu_core_message_count(core)==0);
    }
    pangu_core_destroy(core);
    return 0;
}
