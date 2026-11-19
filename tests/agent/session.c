#include "slices.h"
#include <assert.h>
#include <pangu/agent/session.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define S(x) ((pangu_slice){x, sizeof(x) - 1})
static pangu_message message(const char *text, pangu_content *c) {
  *c = (pangu_content){.kind = PANGU_TEXT, .data = {text, strlen(text)}};
  return (pangu_message){.role = PANGU_USER, .content = c, .content_count = 1};
}
static int commits, live, closed;
static void observe(void *ctx, pangu_session *s, const pg_session_event *e) {
  (void)ctx;
  if (e->kind == PG_SESSION_COMMITTED) {
    commits++;
    assert(pg_session_sequence(s) == e->sequence);
    assert(pg_session_begin(s) == PG_SESSION_BUSY);
  }
  if (e->kind == PG_SESSION_LIVE) {
    live++;
    if (e->live->kind == PANGU_MESSAGE_END)
      assert(pangu_core_message_count(pg_session_messages(s)) >= 1);
  }
  if (e->kind == PG_SESSION_CLOSED)
    closed++;
}
static pangu_status model(void *ctx, pangu_core *core, pangu_model_sink sink,
                          void *data) {
  (void)ctx;
  assert(pangu_core_message_count(core) > 0);
  pangu_content c = {.kind = PANGU_TEXT, .data = S("saved reply")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  return sink(data, &m, 1);
}
static void storage(pg_session_backend backend) {
  const char *path =
      backend == PG_STORE_JSONL ? "agent-test.jsonl" : "agent-test.sqlite";
  remove(path);
  pg_session_options o = {{path, strlen(path)}, backend, 0, 0};
  pg_session_diagnostic d;
  pangu_session *s = NULL;
  assert(pg_session_open(&o, &s, &d) == PG_SESSION_NOT_FOUND);
  assert(!s);
  FILE *f = fopen(path, "rb");
  assert(!f);
  o.create = 1;
  assert(pg_session_open(&o, &s, &d) == PG_SESSION_OK);
  uint64_t id;
  commits = live = closed = 0;
  assert(pg_session_subscribe(s, observe, NULL, &id) == PG_SESSION_OK);
  pangu_content c;
  pangu_message m = message("你好\n世界", &c);
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &m) == PG_SESSION_OK);
  assert(pg_session_set_documents(
             s, S("{\"title\":\"会话\",\"nested\":[1,true]}")) ==
         PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 0);
  assert(commits == 0);
  assert(pg_session_commit(s) == PG_SESSION_OK);
  assert(commits == 1);
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &m) == PG_SESSION_OK);
  pg_session_cancel(s);
  assert(pg_session_commit(s) == PG_SESSION_INVALID);
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &m) == PG_SESSION_OK);
  assert(pg_session_close(s) == PG_SESSION_OK);
  assert(closed == 1);
  pg_session_destroy(s);
  o.create = 0;
  o.read_only = 1;
  assert(pg_session_open(&o, &s, &d) == PG_SESSION_OK);
  assert(pg_session_sequence(s) == 1);
  assert(pangu_core_message_count(pg_session_messages(s)) == 1);
  const pangu_message *r = pangu_core_message_at(pg_session_messages(s), 0);
  assert(r->content[0].data.len == c.data.len);
  assert(!memcmp(r->content[0].data.data, c.data.data, c.data.len));
  assert(slice_contains(pg_session_documents(s), "title"));
  assert(pg_session_begin(s) == PG_SESSION_READ_ONLY);
  pg_session_destroy(s);
  o.read_only = 0;
  assert(pg_session_open(&o, &s, &d) == PG_SESSION_OK);
  assert(pg_session_subscribe(s, observe, NULL, &id) == PG_SESSION_OK);
  pangu_loop_config cfg = {.model = model};
  assert(pg_session_run(s, &m, 1, &cfg) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 3);
  assert(live > 0);
  pg_session_destroy(s);
  assert(pg_session_open(&o, &s, &d) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 3);
  assert(pg_session_sequence(s) == 3);
  pg_session_destroy(s);
  if (backend == PG_STORE_JSONL) {
    f = fopen(path, "ab");
    assert(f);
    fputs("{truncated", f);
    fclose(f);
    assert(pg_session_open(&o, &s, &d) == PG_SESSION_CORRUPT);
    assert(!s && d.line == 5);
  }
  remove(path);
}
int main(void) {
  storage(PG_STORE_JSONL);
  storage(PG_STORE_SQLITE);
  puts("agent storage/core: 2 backends; "
       "save/reopen/readonly/rollback/corruption/live passed");
  return 0;
}
