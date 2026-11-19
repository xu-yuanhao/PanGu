/* Session codec regression: actual JSONL/SQLite consumers and legacy decode. */
#include "internal.h"
#include <assert.h>
#include <pangu/core/proxy_request.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define S(x) ((pangu_slice){x, sizeof(x) - 1})
static pangu_core *wire(const pangu_core *c) {
  pangu_core *out = NULL;
  pangu_model_config model = {
      .id = S("fixture"), .provider = S("fixture"), .api = S("fixture")};
  assert(pangu_proxy_request_json((pangu_core *)c, c, &model, (pangu_slice){0},
                                  &out) == PANGU_OK);
  return out;
}
static void equal_wire(const pangu_core *a, const pangu_core *b) {
  pangu_core *x = wire(a), *y = wire(b);
  pangu_slice p = pangu_core_message_at(x, 0)->content[0].data;
  pangu_slice q = pangu_core_message_at(y, 0)->content[0].data;
  assert(p.len == q.len && !memcmp(p.data, q.data, p.len));
  pangu_core_destroy(x);
  pangu_core_destroy(y);
}
static void sessions(void) {
  for (int b = PG_STORE_JSONL; b <= PG_STORE_SQLITE; b++) {
    const char *path =
        b == PG_STORE_JSONL ? "presence.jsonl" : "presence.sqlite";
    remove(path);
    pg_session_options o = {{path, strlen(path)}, b, 1, 0};
    pangu_session *s = NULL;
    assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
    assert(pg_session_begin(s) == PG_SESSION_OK);
    pangu_core *expected = NULL;
    assert(pangu_core_create(NULL, &expected) == PANGU_OK);
    for (int i = 0; i < 12; i++) {
      pangu_content c = {.kind = PANGU_TEXT, .data = S("")};
      pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
      switch (i) {
      case 0:
        m.content = NULL;
        m.content_count = 0;
        break;
      case 1:
        m.presence = PANGU_MESSAGE_CONTENT_STRING;
        break;
      case 2:
        m.presence = PANGU_MESSAGE_CONTENT_STRING;
        m.content = NULL;
        m.content_count = 0;
        break;
      case 3:
        m.role = PANGU_SYSTEM;
        m.content = NULL;
        m.content_count = 0;
        m.presence = PANGU_MESSAGE_HAS_SECTIONS |
                     PANGU_MESSAGE_HAS_TOOLS_ADDED |
                     PANGU_MESSAGE_HAS_TOOLS_REMOVED;
        break;
      case 4:
        m.role = PANGU_ASSISTANT;
        break;
      case 5:
        m.role = PANGU_ASSISTANT;
        m.presence = PANGU_MESSAGE_HAS_THINKING_LEVEL;
        break;
      case 6:
        m.role = PANGU_TOOL_RESULT;
        m.tool_call_id = S("id");
        m.tool_name = S("tool");
        break;
      case 7:
        m.role = PANGU_TOOL_RESULT;
        m.tool_call_id = S("id");
        m.tool_name = S("tool");
        m.presence = PANGU_MESSAGE_HAS_USAGE;
        break;
      case 8:
        m.role = PANGU_ASSISTANT;
        m.thinking_level = PANGU_THINKING_HIGH;
        break;
      case 9:
        m.role = PANGU_TOOL_RESULT;
        m.tool_call_id = S("id");
        m.tool_name = S("tool");
        m.usage.total = 1;
        break;
      case 10:
      case 11:
        m.role = PANGU_ASSISTANT;
        if (i == 11) {
          m.response_model = m.response_id = m.raw_stop_reason =
              m.provider_thinking_level = m.error = S("");
          c.signature = c.tool_namespace = c.arguments_error = S("");
        }
        break;
      }
      assert(pangu_core_append(expected, &m) == PANGU_OK);
      assert(pg_session_append(s, &m) == PG_SESSION_OK);
    }
    assert(pg_session_commit(s) == PG_SESSION_OK);
    equal_wire(expected, pg_session_messages(s));
    pg_session_destroy(s);
    o.create = 0;
    assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
    equal_wire(expected, pg_session_messages(s));
    for (int i = 10; i < 12; i++) {
      const pangu_message *m = pangu_core_message_at(pg_session_messages(s), i);
      assert(!!m->response_model.data == (i == 11));
      assert(!!m->response_id.data == (i == 11));
      assert(!!m->raw_stop_reason.data == (i == 11));
      assert(!!m->provider_thinking_level.data == (i == 11));
      assert(!!m->error.data == (i == 11));
      assert(!!m->content[0].signature.data == (i == 11));
      assert(!!m->content[0].tool_namespace.data == (i == 11));
      assert(!!m->content[0].arguments_error.data == (i == 11));
    }
    pg_session_destroy(s);
    pangu_core_destroy(expected);
    remove(path);
  }
}
static void *fail_alloc(void *ctx, size_t n) {
  return *(int *)ctx ? NULL : malloc(n);
}
static void *fail_realloc(void *ctx, void *p, size_t old, size_t n) {
  (void)old;
  return *(int *)ctx ? NULL : realloc(p, n);
}
static void release(void *ctx, void *p) {
  (void)ctx;
  free(p);
}
static void legacy_sessions(void) {
  for (int b = PG_STORE_JSONL; b <= PG_STORE_SQLITE; b++) {
    const char *path = b == PG_STORE_JSONL ? "legacy-presence.jsonl"
                                           : "legacy-presence.sqlite";
    remove(path);
    pg_session_options options = {{path, strlen(path)}, b, 1, 0};
    pangu_session *session = NULL;
    assert(pg_session_open(&options, &session, NULL) == PG_SESSION_OK);
    yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(d), *messages = yyjson_mut_arr(d);
    assert(root && messages);
    yyjson_mut_doc_set_root(d, root);
    assert(yyjson_mut_obj_add_str(d, root, "format", "pangu.session"));
    assert(yyjson_mut_obj_add_uint(d, root, "version", 1));
    assert(yyjson_mut_obj_add_uint(d, root, "seq", 1));
    assert(yyjson_mut_obj_add_obj(d, root, "documents"));
    assert(yyjson_mut_obj_add_val(d, root, "messages", messages));
    pangu_core *expected = NULL;
    assert(pangu_core_create(NULL, &expected) == PANGU_OK);
    for (int role = PANGU_ASSISTANT; role <= PANGU_TOOL_RESULT; role++) {
      pangu_content c = {.kind = PANGU_TEXT,
                         .data = S(""),
                         .signature = S(""),
                         .tool_namespace = S(""),
                         .arguments_error = S("")};
      pangu_message m = {.role = role,
                         .content = &c,
                         .content_count = 1,
                         .tool_call_id = S("id"),
                         .tool_name = S("tool"),
                         .error = S(""),
                         .provider_thinking_level = S("")};
      yyjson_mut_val *v = pg_encode_message(d, &m);
      assert(v && yyjson_mut_obj_remove_key(v, "presence"));
      assert(yyjson_mut_arr_add_val(messages, v));
      m.presence = role == PANGU_ASSISTANT ? PANGU_MESSAGE_HAS_THINKING_LEVEL
                                           : PANGU_MESSAGE_HAS_USAGE;
      m.error = m.provider_thinking_level = c.signature = c.tool_namespace =
          c.arguments_error = (pangu_slice){0};
      assert(pangu_core_append(expected, &m) == PANGU_OK);
    }
    size_t len = 0;
    char *text = yyjson_mut_write(d, 0, &len);
    assert(text);
    /* Exercise the real storage write and open/restore consumers. */
    size_t prefix = b == PG_STORE_JSONL ? session->log_len : 0;
    char *log = malloc(prefix + len + 1);
    assert(log);
    if (prefix)
      memcpy(log, session->log, prefix);
    memcpy(log + prefix, text, len);
    log[prefix + len] = '\n';
    assert(pg_store_write(session, log, prefix + len + 1) == PG_SESSION_OK);
    free(log);
    free(text);
    yyjson_mut_doc_free(d);
    pg_session_destroy(session);
    options.create = 0;
    assert(pg_session_open(&options, &session, NULL) == PG_SESSION_OK);
    equal_wire(expected, pg_session_messages(session));
    assert(pg_session_begin(session) == PG_SESSION_OK);
    assert(pg_session_commit(session) == PG_SESSION_OK);
    pg_session_destroy(session);
    assert(pg_session_open(&options, &session, NULL) == PG_SESSION_OK);
    equal_wire(expected, pg_session_messages(session));
    pg_session_destroy(session);
    pangu_core_destroy(expected);
    remove(path);
  }
}
static void codec(void) {
  for (int role = PANGU_ASSISTANT; role <= PANGU_TOOL_RESULT; role++) {
    pangu_content c = {.kind = PANGU_TEXT, .data = S("")};
    pangu_message m = {.role = role,
                       .content = &c,
                       .content_count = 1,
                       .tool_call_id = S("id"),
                       .tool_name = S("tool"),
                       .error = S("")};
    yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *v = pg_encode_message(d, &m);
    assert(v && yyjson_mut_get_uint(yyjson_mut_obj_get(v, "presence")) == 0);
    yyjson_mut_obj_remove_key(v, "presence");
    yyjson_mut_doc_set_root(d, v);
    yyjson_doc *r = yyjson_mut_doc_imut_copy(d, NULL);
    assert(r);
    pangu_core *core = NULL;
    assert(pangu_core_create(NULL, &core) == PANGU_OK);
    assert(pg_decode_message(yyjson_doc_get_root(r), core) == PG_SESSION_OK);
    const pangu_message *got = pangu_core_message_at(core, 0);
    assert(got->presence &
           (role == PANGU_ASSISTANT ? PANGU_MESSAGE_HAS_THINKING_LEVEL
                                    : PANGU_MESSAGE_HAS_USAGE));
    assert(!got->error.data);
    pangu_core_destroy(core);
    yyjson_doc_free(r);
    assert(yyjson_mut_obj_add_sint(d, v, "presence", -1));
    r = yyjson_mut_doc_imut_copy(d, NULL);
    assert(pangu_core_create(NULL, &core) == PANGU_OK);
    assert(pg_decode_message(yyjson_doc_get_root(r), core) ==
           PG_SESSION_CORRUPT);
    assert(!pangu_core_message_count(core));
    pangu_core_destroy(core);
    yyjson_doc_free(r);
    yyjson_mut_obj_remove_key(v, "presence");
    r = yyjson_mut_doc_imut_copy(d, NULL);
    /* Core deep-copy allocation failure propagates and leaves no message. */
    int failing = 0;
    pangu_core_config cfg = {.allocator = {&failing, fail_alloc, release}};
    assert(pangu_core_create(&cfg, &core) == PANGU_OK);
    failing = 1;
    assert(pg_decode_message(yyjson_doc_get_root(r), core) == PG_SESSION_NOMEM);
    assert(!pangu_core_message_count(core));
    failing = 0;
    assert(pg_decode_message(yyjson_doc_get_root(r), core) == PG_SESSION_OK);
    pangu_core_destroy(core);
    yyjson_alc alc = {fail_alloc, fail_realloc, release, &failing};
    yyjson_mut_doc *limited = yyjson_mut_doc_new(&alc);
    assert(limited);
    failing = 1;
    assert(!pg_encode_message(limited, &m));
    yyjson_mut_doc_free(limited);
    yyjson_doc_free(r);
    /* Invalid marker / optional types and role-specific bits are corrupt. */
    for (int invalid = 0; invalid < 4; invalid++) {
      yyjson_mut_obj_remove_key(v, "presence");
      yyjson_mut_obj_remove_key(v, "error");
      if (invalid == 0)
        assert(yyjson_mut_obj_add_bool(d, v, "presence", false));
      if (invalid == 1)
        assert(yyjson_mut_obj_add_uint(d, v, "presence", UINT64_MAX));
      if (invalid == 2) {
        assert(yyjson_mut_obj_add_uint(d, v, "presence", 0));
        assert(yyjson_mut_obj_add_null(d, v, "error"));
      }
      if (invalid == 3)
        assert(yyjson_mut_obj_add_uint(d, v, "presence",
                                       PANGU_MESSAGE_CONTENT_STRING));
      r = yyjson_mut_doc_imut_copy(d, NULL);
      assert(r && pangu_core_create(NULL, &core) == PANGU_OK);
      assert(pg_decode_message(yyjson_doc_get_root(r), core) ==
             PG_SESSION_CORRUPT);
      assert(!pangu_core_message_count(core));
      pangu_core_destroy(core);
      yyjson_doc_free(r);
    }
    yyjson_mut_doc_free(d);
  }
  int failing = 1;
  pangu_core_config cfg = {.allocator = {&failing, fail_alloc, release}};
  pangu_core *core = (void *)1;
  assert(pangu_core_create(&cfg, &core) == PANGU_NOMEM && !core);
}
int main(void) {
  sessions();
  legacy_sessions();
  codec();
  puts("presence");
  return 0;
}
