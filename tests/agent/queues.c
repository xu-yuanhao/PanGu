#include <assert.h>
#include <pangu/agent/session.h>
#include <pangu/agent/tree.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>
#define S(s) ((pangu_slice){s, sizeof(s) - 1})
typedef struct {
  pangu_session *s;
  const char *path;
  pg_session_backend backend;
  char seen[32];
  size_t n;
  int add_b, inject_c, all, checked, borrow, fail_active;
  uint64_t notified;
} state;
static pangu_message user(const char *s, pangu_content *c) {
  *c = (pangu_content){.kind = PANGU_TEXT, .data = {s, strlen(s)}};
  return (pangu_message){.role = PANGU_USER, .content = c, .content_count = 1};
}
static int is(const pangu_message *m, const char *s) {
  return m && m->role == PANGU_USER && m->content_count &&
         m->content[0].data.len == strlen(s) &&
         !memcmp(m->content[0].data.data, s, strlen(s));
}
static pangu_status model(void *ctx, pangu_core *c, pangu_model_sink sink,
                          void *data) {
  state *f = ctx;
  if (f->fail_active) {
    f->fail_active = 0;
    assert(pangu_core_queue_count(c, PANGU_FOLLOW_UP) == 1);
    assert(is(pangu_core_queue_at(c, PANGU_FOLLOW_UP, 0), "A"));
    assert(!rename(f->path, "queue-active.old"));
    uv_fs_t req;
    assert(!uv_fs_mkdir(NULL, &req, f->path, 0700, NULL));
    uv_fs_req_cleanup(&req);
    pangu_content content;
    pangu_message input = user("B", &content);
    uint64_t sequence = pg_session_sequence(f->s);
    assert(pg_session_enqueue(f->s, PANGU_FOLLOW_UP, &input) == PG_SESSION_IO);
    assert(pg_session_sequence(f->s) == sequence);
    assert(pangu_core_queue_count(c, PANGU_FOLLOW_UP) == 1);
    assert(is(pangu_core_queue_at(c, PANGU_FOLLOW_UP, 0), "A"));
    assert(pg_session_queue_count(f->s, PANGU_FOLLOW_UP) == 1);
    assert(is(pg_session_queue_at(f->s, PANGU_FOLLOW_UP, 0), "A"));
    assert(!uv_fs_rmdir(NULL, &req, f->path, NULL));
    uv_fs_req_cleanup(&req);
    assert(!rename("queue-active.old", f->path));
    pg_session_options options = {
        {f->path, strlen(f->path)}, PG_STORE_JSONL, 0, 1};
    pangu_session *reader = NULL;
    assert(pg_session_open(&options, &reader, NULL) == PG_SESSION_OK);
    assert(pg_session_queue_count(reader, PANGU_FOLLOW_UP) == 1);
    assert(is(pg_session_queue_at(reader, PANGU_FOLLOW_UP, 0), "A"));
    pg_session_destroy(reader);
  }
  if (f->borrow) {
    f->borrow = 0;
    const pangu_core *saved = pg_session_messages(f->s);
    const pangu_message *input =
        pangu_core_message_at(saved, pangu_core_message_count(saved) - 1);
    assert(pg_session_enqueue(f->s, PANGU_FOLLOW_UP, input) == PG_SESSION_OK);
  }
  for (size_t i = pangu_core_message_count(c); i; i--) {
    const pangu_message *m = pangu_core_message_at(c, i - 1);
    if (m->role == PANGU_USER) {
      f->seen[f->n++] = m->content[0].data.data[0];
      break;
    }
  }
  pangu_content text = {.kind = PANGU_TEXT, .data = S("reply")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &text,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  return sink(data, &m, 1);
}
static void acceptance(void *ctx, pangu_session *s, const pg_session_event *e) {
  state *f = ctx;
  if (e->kind != PG_SESSION_COMMITTED)
    return;
  if (f->add_b) {
    f->add_b = 0;
    pangu_content c;
    pangu_message m = user("B", &c);
    assert(pg_session_enqueue(s, PANGU_STEERING, &m) == PG_SESSION_OK);
  }
}
static void ordered(void *ctx, pangu_session *s, const pg_session_event *e) {
  state *f = ctx;
  if (e->kind != PG_SESSION_COMMITTED)
    return;
  assert(e->sequence > f->notified);
  f->notified = e->sequence;
  const pangu_core *view = pg_session_messages(s);
  size_t n = pangu_core_message_count(view);
  if (f->all && n && is(pangu_core_message_at(view, n - 1), "A") &&
      e->message_count) {
    assert(pg_session_queue_count(s, PANGU_STEERING) == 2);
    pg_session_options options = {{f->path, strlen(f->path)}, f->backend, 0, 1};
    pangu_session *reader = NULL;
    assert(pg_session_open(&options, &reader, NULL) == PG_SESSION_OK);
    assert(pg_session_queue_count(reader, PANGU_STEERING) == 2);
    assert(is(pg_session_queue_at(reader, PANGU_STEERING, 0), "B"));
    assert(is(pg_session_queue_at(reader, PANGU_STEERING, 1), "C"));
    pg_session_destroy(reader);
    f->checked++;
  }
}
static void events(void *ctx, pangu_core *core, const pangu_event *e) {
  state *f = ctx;
  if (e->kind == PANGU_AGENT_START && f->all)
    assert(pangu_core_queue_mode(core, PANGU_STEERING, PANGU_QUEUE_ALL) ==
           PANGU_OK);
  if (e->kind == PANGU_MESSAGE_START && f->inject_c && is(e->message, "A")) {
    f->inject_c = 0;
    pangu_content c;
    pangu_message m = user("C", &c);
    assert(pg_session_enqueue(f->s, PANGU_STEERING, &m) == PG_SESSION_OK);
    assert(pg_session_queue_count(f->s, PANGU_STEERING) == 3);
  }
}
static void test(pg_session_backend backend) {
  const char *path =
      backend == PG_STORE_JSONL ? "queues.jsonl" : "queues.sqlite";
  remove(path);
  pg_session_options options = {{path, strlen(path)}, backend, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&options, &s, NULL) == PG_SESSION_OK);
  state f = {.s = s, .path = path, .backend = backend, .borrow = 1};
  pangu_loop_config config = {.ctx = &f, .model = model, .event = events};
  pangu_content c;
  pangu_message m = user("seed", &c);
  assert(pg_session_run(s, &m, 1, &config) == PG_SESSION_OK);
  m = user("F", &c);
  assert(pg_session_enqueue(s, PANGU_FOLLOW_UP, &m) == PG_SESSION_OK);
  m = user("S", &c);
  assert(pg_session_enqueue(s, PANGU_STEERING, &m) == PG_SESSION_OK);
  assert(pg_session_queue_count(s, PANGU_STEERING) == 1 &&
         pg_session_queue_count(s, PANGU_FOLLOW_UP) == 1);
  pg_session_destroy(s);
  options.create = 0;
  assert(pg_session_open(&options, &s, NULL) == PG_SESSION_OK);
  f.s = s;
  f.n = 0;
  assert(pg_session_run(s, NULL, 0, &config) == PG_SESSION_OK);
  assert(f.n == 2 && f.seen[0] == 'S' && f.seen[1] == 'F');
  assert(!pg_session_queue_count(s, PANGU_STEERING) &&
         !pg_session_queue_count(s, PANGU_FOLLOW_UP));
  uint64_t a, b;
  f.add_b = 1;
  assert(pg_session_subscribe(s, acceptance, &f, &a) == PG_SESSION_OK);
  assert(pg_session_subscribe(s, ordered, &f, &b) == PG_SESSION_OK);
  m = user("A", &c);
  assert(pg_session_enqueue(s, PANGU_STEERING, &m) == PG_SESSION_OK);
  assert(pg_session_queue_count(s, PANGU_STEERING) == 2);
  assert(is(pg_session_queue_at(s, PANGU_STEERING, 0), "A") &&
         is(pg_session_queue_at(s, PANGU_STEERING, 1), "B"));
  assert(pg_session_queue_mode(s, PANGU_STEERING, PANGU_QUEUE_ALL) ==
         PG_SESSION_OK);
  f.all = 1;
  f.inject_c = 1;
  f.n = 0;
  assert(pg_session_run(s, NULL, 0, &config) == PG_SESSION_OK);
  assert(f.checked == 1);
  assert(f.n == 2 && f.seen[0] == 'B' && f.seen[1] == 'C');
  assert(!pg_session_queue_count(s, PANGU_STEERING));
  pg_session_destroy(s);
  assert(pg_session_open(&options, &s, NULL) == PG_SESSION_OK);
  assert(!pg_session_queue_count(s, PANGU_STEERING));
  m = user("discard", &c);
  assert(pg_session_enqueue(s, PANGU_FOLLOW_UP, &m) == PG_SESSION_OK);
  assert(pg_session_clear_queue(s, PANGU_FOLLOW_UP) == PG_SESSION_OK);
  assert(!pg_session_queue_count(s, PANGU_FOLLOW_UP));
  pg_session_destroy(s);
  remove(path);
}
static void failure(void) {
  const char *path = "queue-failure.jsonl";
  remove(path);
  remove("queue-failure.old");
  pg_session_options options = {S("queue-failure.jsonl"), PG_STORE_JSONL, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&options, &s, NULL) == PG_SESSION_OK);
  assert(!rename(path, "queue-failure.old"));
  uv_fs_t req;
  assert(!uv_fs_mkdir(NULL, &req, path, 0700, NULL));
  uv_fs_req_cleanup(&req);
  pangu_content c;
  pangu_message m = user("not accepted", &c);
  assert(pg_session_enqueue(s, PANGU_STEERING, &m) == PG_SESSION_IO);
  assert(!pg_session_queue_count(s, PANGU_STEERING) &&
         pg_session_sequence(s) == 0);
  assert(!uv_fs_rmdir(NULL, &req, path, NULL));
  uv_fs_req_cleanup(&req);
  assert(!rename("queue-failure.old", path));
  assert(pg_session_enqueue(s, PANGU_STEERING, &m) == PG_SESSION_OK);
  pg_session_destroy(s);
  options.create = 0;
  assert(pg_session_open(&options, &s, NULL) == PG_SESSION_OK);
  assert(pg_session_queue_count(s, PANGU_STEERING) == 1);
  pg_session_destroy(s);
  remove(path);
}
static void active_failure(void) {
  const char *path = "queue-active.jsonl";
  remove(path);
  pg_session_options options = {S("queue-active.jsonl"), PG_STORE_JSONL, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&options, &s, NULL) == PG_SESSION_OK);
  state f = {.s = s, .path = path};
  pangu_loop_config config = {.ctx = &f, .model = model};
  pangu_content content;
  pangu_message input = user("seed", &content);
  assert(pg_session_run(s, &input, 1, &config) == PG_SESSION_OK);
  input = user("A", &content);
  assert(pg_session_enqueue(s, PANGU_FOLLOW_UP, &input) == PG_SESSION_OK);
  f.fail_active = 1;
  f.n = 0;
  input = user("prompt", &content);
  assert(pg_session_run(s, &input, 1, &config) == PG_SESSION_OK);
  assert(f.n == 2 && f.seen[0] == 'p' && f.seen[1] == 'A');
  pg_session_destroy(s);
  options.create = 0;
  assert(pg_session_open(&options, &s, NULL) == PG_SESSION_OK);
  assert(!pg_session_queue_count(s, PANGU_FOLLOW_UP));
  const pangu_core *messages = pg_session_messages(s);
  size_t count_a = 0;
  for (size_t i = 0; i < pangu_core_message_count(messages); i++) {
    const pangu_message *m = pangu_core_message_at(messages, i);
    assert(!is(m, "B"));
    count_a += is(m, "A");
  }
  assert(count_a == 1);
  pg_session_destroy(s);
  remove(path);
}
typedef struct {
  const char **items;
  size_t count, at;
} history_check;
static pg_session_status history_item(void *ctx, pangu_slice text) {
  history_check *h = ctx;
  assert(h->at < h->count);
  const char *want = h->items[h->at++];
  assert(text.len == strlen(want) && !memcmp(text.data, want, text.len));
  return PG_SESSION_OK;
}
static pangu_status cancel_model(void *ctx, pangu_core *core,
                                 pangu_model_sink sink, void *data) {
  (void)core;
  (void)sink;
  (void)data;
  pg_session_cancel(ctx);
  return PANGU_CANCELLED;
}
static void raw_failure(pg_session_backend backend) {
  const char *path =
      backend == PG_STORE_JSONL ? "raw-failure.jsonl" : "raw-failure.db";
  remove(path);
  pg_session_options opts = {{path, strlen(path)}, backend, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&opts, &s, NULL) == PG_SESSION_OK);
  sqlite3 *db = NULL;
  uv_fs_t req;
  if (backend == PG_STORE_JSONL) {
    assert(!rename(path, "raw-failure.old"));
    assert(!uv_fs_mkdir(NULL, &req, path, 0700, NULL));
    uv_fs_req_cleanup(&req);
  } else {
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    assert(
        sqlite3_exec(db,
                     "CREATE TRIGGER fail_raw BEFORE INSERT ON pangu_session "
                     "BEGIN SELECT RAISE(ABORT,'fixture'); END",
                     NULL, NULL, NULL) == SQLITE_OK);
  }
  pangu_content pc, rc;
  pangu_message prompt = user("expanded", &pc), raw = user("/raw failure", &rc);
  state f = {.s = s};
  pangu_loop_config cfg = {.ctx = &f, .model = model};
  assert(pg_session_enqueue_input(s, PANGU_FOLLOW_UP, &prompt, &raw) ==
         PG_SESSION_IO);
  assert(pg_session_run_inputs(s, &prompt, 1, &cfg, &raw) == PG_SESSION_IO);
  history_check h = {0};
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         !h.at);
  assert(!pg_session_sequence(s) &&
         !pg_session_queue_count(s, PANGU_FOLLOW_UP));
  if (backend == PG_STORE_JSONL) {
    assert(!uv_fs_rmdir(NULL, &req, path, NULL));
    uv_fs_req_cleanup(&req);
    assert(!rename("raw-failure.old", path));
  } else {
    assert(sqlite3_exec(db, "DROP TRIGGER fail_raw", NULL, NULL, NULL) ==
           SQLITE_OK);
    sqlite3_close(db);
  }
  cfg = (pangu_loop_config){.ctx = s, .model = cancel_model};
  assert(pg_session_run_inputs(s, &prompt, 1, &cfg, &raw) ==
         PG_SESSION_CANCELLED);
  const char *items[] = {"/raw failure"};
  h = (history_check){items, 1, 0};
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 1);
  pg_session_destroy(s);
  opts.create = 0;
  assert(pg_session_open(&opts, &s, NULL) == PG_SESSION_OK);
  h.at = 0;
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 1);
  pg_session_destroy(s);
  remove(path);
}
static void provenance(pg_session_backend backend) {
  const char *path =
      backend == PG_STORE_JSONL ? "raw-queue.jsonl" : "raw-queue.db";
  remove(path);
  pg_session_options opts = {{path, strlen(path)}, backend, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&opts, &s, NULL) == PG_SESSION_OK);
  pangu_content pc, rc;
  pangu_message prompt = user("expanded secret body", &pc),
                raw = user("/skill:raw 原始", &rc);
  assert(pg_session_enqueue_input(s, PANGU_FOLLOW_UP, &prompt, &raw) ==
         PG_SESSION_OK);
  history_check h = {0};
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         !h.at);
  pg_session_destroy(s);
  opts.create = 0;
  assert(pg_session_open(&opts, &s, NULL) == PG_SESSION_OK);
  state f = {.s = s, .path = path, .backend = backend};
  pangu_loop_config cfg = {.ctx = &f, .model = model};
  prompt = user("expanded seed", &pc);
  raw = user("/template seed", &rc);
  assert(pg_session_run_inputs(s, &prompt, 1, &cfg, &raw) == PG_SESSION_OK);
  const char *items[] = {"/template seed", "/skill:raw 原始"};
  h = (history_check){items, 2, 0};
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 2);
  pg_session_destroy(s);
  assert(pg_session_open(&opts, &s, NULL) == PG_SESSION_OK);
  h.at = 0;
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 2);
  assert(pg_session_enqueue_input(s, PANGU_STEERING, &prompt, &raw) ==
         PG_SESSION_OK);
  assert(pg_session_clear_queue(s, PANGU_STEERING) == PG_SESSION_OK);
  pg_session_destroy(s);
  assert(pg_session_open(&opts, &s, NULL) == PG_SESSION_OK);
  assert(!pg_session_queue_count(s, PANGU_STEERING));
  h.at = 0;
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 2);
  uint64_t saved_leaf = pg_session_leaf(s);
  assert(pg_session_switch(s, 1) == PG_SESSION_OK);
  h = (history_check){items, 1, 0};
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 1);
  raw = user("branch raw", &rc);
  assert(pg_session_run_inputs(s, &prompt, 1, &cfg, &raw) == PG_SESSION_OK);
  const char *branch_items[] = {"/template seed", "branch raw"};
  h = (history_check){branch_items, 2, 0};
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 2);
  assert(pg_session_switch(s, saved_leaf) == PG_SESSION_OK);
  assert(pg_session_compact_summary(s, 3, S("summary"), NULL) == PG_SESSION_OK);
  const char *kept[] = {"/skill:raw 原始"};
  h = (history_check){kept, 1, 0};
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 1);
  pg_session_destroy(s);
  assert(pg_session_open(&opts, &s, NULL) == PG_SESSION_OK);
  h.at = 0;
  assert(pg_session_input_history(s, history_item, &h) == PG_SESSION_OK &&
         h.at == 1);
  /* Legacy append is explicitly unavailable, with no partial sink calls. */
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &prompt) == PG_SESSION_OK);
  assert(pg_session_commit(s) == PG_SESSION_OK);
  h.at = 0;
  assert(pg_session_input_history(s, history_item, &h) ==
             PG_SESSION_NOT_FOUND &&
         !h.at);
  pg_session_destroy(s);
  remove(path);
}
int main(void) {
  raw_failure(PG_STORE_JSONL);
  raw_failure(PG_STORE_SQLITE);
  provenance(PG_STORE_JSONL);
  provenance(PG_STORE_SQLITE);
  test(PG_STORE_JSONL);
  test(PG_STORE_SQLITE);
  failure();
  active_failure();
  puts("durable mailbox FIFO/reopen/ALL batch/accept-on-start/IO rollback "
       "passed");
  return 0;
}
