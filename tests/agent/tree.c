#include "slices.h"
#include <assert.h>
#include <pangu/agent/tree.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#define S(s) ((pangu_slice){s, sizeof(s) - 1})
static void append(pangu_session *s, pangu_role role, const char *text) {
  pangu_content c = {.kind = PANGU_TEXT, .data = {text, strlen(text)}};
  pangu_message m = {.role = role,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  if (role == PANGU_ASSISTANT)
    m.usage = (pangu_usage){.output = 10, .total = 10, .total_cost = .1};
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &m) == PG_SESSION_OK);
  assert(pg_session_commit(s) == PG_SESSION_OK);
}
static int changed;
static void event(void *ctx, pangu_session *s, const pg_session_event *e) {
  (void)ctx;
  if (e->kind == PG_SESSION_CONTEXT_CHANGED) {
    changed++;
    assert(pg_session_switch(s, 0) == PG_SESSION_BUSY);
  }
}
static void tree(pg_session_backend backend) {
  const char *path = backend == PG_STORE_JSONL ? "tree.jsonl" : "tree.sqlite";
  remove(path);
  pg_session_options o = {{path, strlen(path)}, backend, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  uint64_t subscription;
  changed = 0;
  assert(pg_session_subscribe(s, event, NULL, &subscription) == PG_SESSION_OK);
  append(s, PANGU_USER, "first");
  append(s, PANGU_ASSISTANT, "answer1");
  append(s, PANGU_USER, "second");
  append(s, PANGU_ASSISTANT, "answer2");
  assert(pg_session_entry_count(s) == 4 && pg_session_leaf(s) == 4);
  assert(pg_session_switch(s, 2) == PG_SESSION_OK);
  append(s, PANGU_USER, "branch");
  pg_session_entry entry;
  assert(pg_session_entry_at(s, 4, &entry) == PG_SESSION_OK && entry.id == 5 &&
         entry.parent == 2);
  assert(pg_session_entry_count(s) == 5 &&
         pangu_core_message_count(pg_session_messages(s)) == 3);
  assert(pg_session_switch(s, 4) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 4);
  uint64_t seq = pg_session_sequence(s);
  assert(pg_session_switch(s, 100) == PG_SESSION_NOT_FOUND &&
         pg_session_sequence(s) == seq);
  pangu_usage usage = {.input = 2, .output = 1, .total = 3, .total_cost = .02};
  assert(pg_session_compact_summary(s, 3, S("safe summary"), &usage) ==
         PG_SESSION_OK);
  assert(pg_session_leaf(s) == 6);
  const pangu_core *context = pg_session_messages(s);
  assert(pangu_core_message_count(context) == 3);
  const pangu_message *m = pangu_core_message_at(context, 0);
  assert(m->role == PANGU_USER && m->content[0].data.len > 12);
  assert(pg_session_entry_at(s, 5, &entry) == PG_SESSION_OK &&
         entry.kind == PG_ENTRY_COMPACTION && entry.first_kept == 3 &&
         entry.parent == 4);
  assert(pg_session_usage(s).total == 23);
  assert(pg_session_usage(s).total_cost > .21);
  assert(pg_session_reset(s, S("handoff")) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 1);
  assert(pg_session_switch(s, 6) == PG_SESSION_OK);
  assert(changed == 5);
  pg_session_destroy(s);
  o.create = 0;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  assert(pg_session_entry_count(s) == 7 && pg_session_leaf(s) == 6 &&
         pg_session_usage(s).total == 23);
  assert(pangu_core_message_count(pg_session_messages(s)) == 3);
  assert(pg_session_switch(s, 5) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 3);
  assert(pg_session_reset(s, (pangu_slice){0}) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 0);
  assert(pg_session_switch(s, 0) == PG_SESSION_OK);
  append(s, PANGU_USER, "new root");
  assert(pg_session_entry_at(s, 8, &entry) == PG_SESSION_OK &&
         entry.parent == 0);
  pg_session_destroy(s);
  o.read_only = 1;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  assert(pg_session_switch(s, 1) == PG_SESSION_READ_ONLY);
  pg_session_destroy(s);
  remove(path);
}
typedef struct {
  pangu_session *s;
  int cancel, incomplete, called;
} summary_state;
static pangu_status summarize(void *ctx, pangu_core *core,
                              pangu_model_sink sink, void *data) {
  summary_state *state = ctx;
  state->called++;
  const pangu_message *system = pangu_core_message_at(core, 0);
  assert(system->role == PANGU_SYSTEM);
  assert(slice_contains(system->content[0].data, "Do NOT continue"));
  if (state->cancel) {
    pg_session_cancel(state->s);
    return PANGU_CANCELLED;
  }
  pangu_content c = {
      .kind = PANGU_TEXT,
      .data = S("## Goal\nRetain synthetic context\n## Next Steps\nContinue")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason =
                         state->incomplete ? PANGU_LENGTH : PANGU_STOP,
                     .usage = {.total = 7}};
  return sink(data, &m, 1);
}
static void compaction(void) {
  remove("compact.jsonl");
  pg_session_options o = {S("compact.jsonl"), PG_STORE_JSONL, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  append(s, PANGU_USER, "first user");
  append(s, PANGU_ASSISTANT, "first reply");
  append(s, PANGU_USER, "second user");
  append(s, PANGU_ASSISTANT, "second reply");
  summary_state state = {.s = s, .cancel = 1};
  pangu_loop_config cfg = {.ctx = &state, .model = summarize};
  uint64_t leaf = pg_session_leaf(s), seq = pg_session_sequence(s);
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_CANCELLED);
  assert(pg_session_leaf(s) == leaf && pg_session_sequence(s) == seq);
  state.cancel = 0;
  state.incomplete = 1;
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_CORE_ERROR);
  assert(pg_session_leaf(s) == leaf && pg_session_sequence(s) == seq);
  state.incomplete = 0;
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_OK);
  assert(pg_session_leaf(s) == 5);
  assert(pg_session_usage(s).total == 27);
  assert(state.called == 3);
  pg_session_destroy(s);
  o.create = 0;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  assert(pg_session_leaf(s) == 5 && pg_session_usage(s).total == 27);
  assert(pg_session_switch(s, 4) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 4);
  pg_session_destroy(s);
  remove("compact.jsonl");
}
static void boundaries(void) {
  remove("boundaries.jsonl");
  pg_session_options o = {S("boundaries.jsonl"), PG_STORE_JSONL, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  append(s, PANGU_SYSTEM, "Keep baseline");
  append(s, PANGU_USER, "old");
  append(s, PANGU_ASSISTANT, "done");
  append(s, PANGU_USER, "new");
  pangu_content call = {.kind = PANGU_TOOL_CALL,
                        .data = S("{}"),
                        .id = S("call"),
                        .name = S("read")};
  pangu_message assistant = {.role = PANGU_ASSISTANT,
                             .content = &call,
                             .content_count = 1,
                             .stop_reason = PANGU_TOOL_USE};
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &assistant) == PG_SESSION_OK);
  assert(pg_session_commit(s) == PG_SESSION_OK);
  append(s, PANGU_USER, "steer");
  pangu_content text = {.kind = PANGU_TEXT, .data = S("result")};
  pangu_message result = {.role = PANGU_TOOL_RESULT,
                          .content = &text,
                          .content_count = 1,
                          .tool_call_id = S("call"),
                          .tool_name = S("read")};
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, &result) == PG_SESSION_OK);
  assert(pg_session_commit(s) == PG_SESSION_OK);
  append(s, PANGU_ASSISTANT, "complete");
  uint64_t seq = pg_session_sequence(s);
  assert(pg_session_compact_summary(s, 7, S("bad"), NULL) ==
         PG_SESSION_INVALID);
  assert(pg_session_compact_summary(s, 6, S("bad"), NULL) ==
         PG_SESSION_INVALID);
  assert(pg_session_sequence(s) == seq);
  assert(pg_session_compact_summary(s, 5, S("valid"), NULL) == PG_SESSION_OK);
  const pangu_core *view = pg_session_messages(s);
  assert(pangu_core_message_count(view) == 6);
  assert(pangu_core_message_at(view, 0)->role == PANGU_SYSTEM);
  assert(pangu_core_message_at(view, 2)->content[0].kind == PANGU_TOOL_CALL);
  assert(pg_session_reset(s, (pangu_slice){0}) == PG_SESSION_OK);
  assert(pangu_core_message_count(pg_session_messages(s)) == 1);
  assert(pangu_core_message_at(pg_session_messages(s), 0)->role ==
         PANGU_SYSTEM);
  pg_session_destroy(s);
  remove("boundaries.jsonl");
}
typedef struct {
  int summaries, replies;
} auto_state;
static pangu_status automatic_model(void *ctx, pangu_core *core,
                                    pangu_model_sink sink, void *data) {
  auto_state *state = ctx;
  const pangu_message *first = pangu_core_message_at(core, 0);
  int summary = first && first->role == PANGU_SYSTEM &&
                slice_contains(first->content[0].data, "Do NOT continue");
  if (summary) {
    state->summaries++;
    pangu_slice options = pangu_core_active_options(core);
    assert(options.len && slice_contains(options, "none"));
  } else
    state->replies++;
  pangu_content c = {.kind = PANGU_TEXT,
                     .data = summary ? S("safe summary") : S("new reply")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP,
                     .usage = {.total = summary ? 7 : 13}};
  return sink(data, &m, 1);
}
static void automatic(void) {
  remove("automatic.jsonl");
  pg_session_options o = {S("automatic.jsonl"), PG_STORE_JSONL, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  append(s, PANGU_USER, "old first");
  append(s, PANGU_ASSISTANT, "old answer");
  append(s, PANGU_USER, "new first");
  append(s, PANGU_ASSISTANT, "new answer");
  pg_compaction_policy p = {1, 11, 1, 1};
  assert(pg_session_set_compaction_policy(s, &p) == PG_SESSION_OK);
  assert(pg_session_context_tokens(s) == 10);
  auto_state state = {0};
  pangu_loop_config cfg = {.ctx = &state, .model = automatic_model};
  pangu_content c = {.kind = PANGU_TEXT, .data = S("continue")};
  pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
  assert(pg_session_run(s, &m, 1, &cfg) == PG_SESSION_OK);
  assert(state.summaries == 1 && state.replies == 1);
  assert(pg_session_context_tokens(s) == 13);
  assert(pg_session_usage(s).total == 40);
  pg_session_destroy(s);
  o.create = 0;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  assert(pg_session_compaction_policy(s).enabled &&
         pg_session_compaction_policy(s).keep_recent_tokens == 1);
  assert(pg_session_usage(s).total == 40);
  pg_session_destroy(s);
  remove("automatic.jsonl");
}
static void unicode_estimate(void) {
  remove("unicode.jsonl");
  pg_session_options o = {S("unicode.jsonl"), PG_STORE_JSONL, 1, 0};
  pangu_session *s = NULL;
  assert(pg_session_open(&o, &s, NULL) == PG_SESSION_OK);
  append(s, PANGU_USER, "😀你abc");
  assert(pg_session_context_tokens(s) == 2);
  pg_session_destroy(s);
  remove("unicode.jsonl");
}
/* Pi agent-session-compaction suite: one recovery, no successful reply replay,
 * cancellation and persistent omission before summarization. */
typedef struct {
  pangu_session *session;
  const char *path;
  int mode, replies, summaries, omissions, compactions, faulted, queued;
  uint64_t last_seen;
  pg_session_backend backend;
  sqlite3 *lock;
} recovery_state;
static void storage_fault(recovery_state *r, int enabled) {
  if (r->backend == PG_STORE_SQLITE) {
    if (enabled) {
      assert(sqlite3_open(r->path, &r->lock) == SQLITE_OK);
      assert(sqlite3_exec(r->lock, "BEGIN EXCLUSIVE", NULL, NULL, NULL) ==
             SQLITE_OK);
    } else {
      assert(sqlite3_exec(r->lock, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK);
      assert(sqlite3_close(r->lock) == SQLITE_OK);
      r->lock = NULL;
    }
  } else {
    char saved[128];
    snprintf(saved, sizeof(saved), "%s.saved", r->path);
    uv_fs_t req;
    if (enabled) {
      assert(!rename(r->path, saved));
      assert(uv_fs_mkdir(NULL, &req, r->path, 0700, NULL) == 0);
    } else {
      assert(uv_fs_rmdir(NULL, &req, r->path, NULL) == 0);
      uv_fs_req_cleanup(&req);
      assert(!rename(saved, r->path));
    }
    uv_fs_req_cleanup(&req);
  }
}
static void recovery_event(void *ctx, pangu_session *session,
                           const pg_session_event *event) {
  recovery_state *r = ctx;
  if (event->kind != PG_SESSION_COMMITTED)
    return;
  pg_session_entry entry;
  assert(pg_session_entry_at(session, pg_session_entry_count(session) - 1,
                             &entry) == PG_SESSION_OK);
  if (entry.id == r->last_seen)
    return;
  r->last_seen = entry.id;
  if (((r->mode == 14 && entry.kind == PG_ENTRY_COMPACTION) ||
       (r->mode == 15 && entry.kind == PG_ENTRY_OMISSION)) &&
      !r->queued) {
    r->queued = 1;
    pangu_content c = {.kind = PANGU_TEXT, .data = S("queued during recovery")};
    pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
    assert(pg_session_enqueue(session, PANGU_FOLLOW_UP, &m) == PG_SESSION_OK);
  }
  if (entry.kind == PG_ENTRY_OMISSION) {
    r->omissions++;
    if (r->mode == 4)
      pg_session_cancel(session);
  }
  if (entry.kind == PG_ENTRY_COMPACTION) {
    r->compactions++;
    if (r->mode == 12)
      pg_session_cancel(session);
  }
  int fail = (r->mode == 5 && entry.message &&
              entry.message->stop_reason == PANGU_LENGTH) ||
             (r->mode == 6 && entry.kind == PG_ENTRY_OMISSION) ||
             (r->mode == 16 && entry.kind == PG_ENTRY_COMPACTION);
  if (fail && !r->faulted) {
    r->faulted = 1;
    storage_fault(r, 1);
  }
}
static pangu_status recovery_model(void *ctx, pangu_core *core,
                                   pangu_model_sink sink, void *out) {
  recovery_state *r = ctx;
  const pangu_message *first = pangu_core_message_at(core, 0);
  int summary = first && first->role == PANGU_SYSTEM &&
                slice_contains(first->content[0].data, "Do NOT continue");
  pangu_content text = {.kind = PANGU_TEXT,
                        .data = summary ? S("recovered summary")
                                        : S("bounded reply")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &text,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP,
                     .model = S("recovery"),
                     .provider = S("synthetic")};
  assert(pg_session_run(r->session, NULL, 0, &(pangu_loop_config){0}) ==
         PG_SESSION_BUSY);
  if (summary) {
    r->summaries++;
    m.usage.total = 5;
    if (r->mode == 2)
      return PANGU_MODEL_ERROR;
    if (r->mode == 3) {
      pg_session_cancel(r->session);
      return PANGU_CANCELLED;
    }
  } else {
    if (r->replies >= 100) {
      const pangu_message *last =
          pangu_core_message_at(core, pangu_core_message_count(core) - 1);
      assert(last && last->role == PANGU_USER &&
             slice_contains(last->content[0].data, "after recovery reopen"));
    }
    r->replies++;
    if (r->mode == 17 && r->replies == 2) {
      pg_session_cancel(r->session);
      return PANGU_CANCELLED;
    }
    m.usage.total = r->replies == 1 ? 7 : 11;
    if (r->replies == 1 || r->mode == 1) {
      m.stop_reason = PANGU_LENGTH;
      m.usage.output = r->mode == 8 ? 100 : 3;
      if (r->mode == 13) {
        m.usage.output = 0;
        m.usage.input = 99000;
      }
      if (r->mode == 7 || r->mode == 14) {
        m.stop_reason = PANGU_STOP;
        m.usage.input = 100001;
      }
      if (r->mode == 11)
        m.provider = S("different-provider");
    }
  }
  pangu_content omitted = {.kind = PANGU_TOOL_CALL,
                           .name = S("read"),
                           .id = S("omitted-call"),
                           .data = S("{\"path\":\"omitted-file.txt\"}")};
  if (!summary && r->mode == 18 && r->replies == 1) {
    m.content = &omitted;
    m.content_count = 1;
  }
  if (summary && r->mode == 18)
    for (size_t i = 0; i < pangu_core_message_count(core); i++) {
      const pangu_message *message = pangu_core_message_at(core, i);
      for (size_t j = 0; j < message->content_count; j++)
        assert(!slice_contains(message->content[j].data, "omitted-file.txt"));
    }
  return sink(out, &m, 1);
}
static pangu_turn_decision settle_truncated(void *ctx, pangu_core *core,
                                            const pangu_message *assistant) {
  (void)ctx;
  (void)core;
  return assistant->stop_reason == PANGU_LENGTH ? PANGU_TURN_END_RUN
                                                : PANGU_TURN_AUTO;
}
static void recovery(pg_session_backend backend, int mode) {
  char path[100];
  snprintf(path, sizeof(path), "recovery-%d-%d.db", backend, mode);
  remove(path);
  pg_session_options options = {{path, strlen(path)}, backend, 1, 0};
  recovery_state r = {.path = path, .mode = mode, .backend = backend};
  assert(pg_session_open(&options, &r.session, NULL) == PG_SESSION_OK);
  if (mode != 9) {
    append(r.session, PANGU_USER, "old request");
    append(r.session, PANGU_ASSISTANT, "old response");
    append(r.session, PANGU_USER, "recent request");
    append(r.session, PANGU_ASSISTANT, "recent response");
  }
  pg_compaction_policy policy = {mode != 10, 100000, 1,
                                 mode == 9 ? 1000000 : 1};
  assert(pg_session_set_compaction_policy(r.session, &policy) == PG_SESSION_OK);
  uint64_t id;
  assert(pg_session_subscribe(r.session, recovery_event, &r, &id) ==
         PG_SESSION_OK);
  pangu_loop_config cfg = {
      .ctx = &r,
      .model = recovery_model,
      .model_config = {.id = S("recovery"),
                       .provider = S("synthetic"),
                       .metadata_json =
                           mode == 13 ? S("{}") : S("{\"maxTokens\":100}")}};
  if (mode == 18)
    cfg.finish_turn = settle_truncated;
  pangu_content text = {.kind = PANGU_TEXT, .data = S("recover this input")};
  pangu_message input = {
      .role = PANGU_USER, .content = &text, .content_count = 1};
  pg_session_status status = pg_session_run(r.session, &input, 1, &cfg);
  if (r.faulted)
    storage_fault(&r, 0);
  assert(status == ((mode == 3 || mode == 4 || mode == 12 || mode == 17)
                        ? PG_SESSION_CANCELLED
                    : (mode == 5 || mode == 6 || mode == 16) ? PG_SESSION_IO
                    : mode == 2 ? PG_SESSION_CORE_ERROR
                                : PG_SESSION_OK));
  int omitted = mode != 5 && mode != 7 && mode != 8 && mode != 10 &&
                mode != 11 && mode != 14;
  int compacted =
      mode == 0 || mode == 1 || mode == 7 || mode == 12 || mode >= 13;
  int retried = mode == 0 || mode == 1 || mode == 13 || mode >= 15;
  int queued_reply = mode == 14 || mode == 15;
  assert(r.replies == 1 + retried + queued_reply);
  assert(r.omissions == omitted && r.compactions == compacted);
  assert(r.summaries == (compacted || mode == 2 || mode == 3 || mode == 6));
  if (mode == 18)
    for (size_t i = 0; i < pg_session_entry_count(r.session); i++) {
      pg_session_entry entry;
      assert(pg_session_entry_at(r.session, i, &entry) == PG_SESSION_OK);
      if (entry.kind == PG_ENTRY_COMPACTION) {
        assert(slice_contains(entry.message->details_json, "readFiles"));
        assert(
            !slice_contains(entry.message->details_json, "omitted-file.txt"));
      }
    }
  pangu_usage usage = pg_session_usage(r.session);
  assert(usage.total ==
         (uint64_t)((mode == 9 ? 0 : 20) + 7 + compacted * 5 +
                    (retried && mode != 16 && mode != 17 ? 11 : 0) +
                    queued_reply * 11));
  assert(pg_session_queue_count(r.session, PANGU_FOLLOW_UP) == 0);
  size_t count = pangu_core_message_count(pg_session_messages(r.session));
  uint64_t seq = pg_session_sequence(r.session),
           leaf = pg_session_leaf(r.session);
  pg_session_destroy(r.session);
  options.create = 0;
  assert(pg_session_open(&options, &r.session, NULL) == PG_SESSION_OK);
  assert(pg_session_sequence(r.session) == seq &&
         pg_session_leaf(r.session) == leaf);
  assert(pg_session_usage(r.session).total == usage.total);
  assert(pangu_core_message_count(pg_session_messages(r.session)) == count);
  size_t truncated = 0;
  for (size_t i = 0; i < count; i++)
    truncated +=
        pangu_core_message_at(pg_session_messages(r.session), i)->stop_reason ==
        PANGU_LENGTH;
  assert(truncated ==
         (size_t)((!omitted && mode != 7 && mode != 14) || mode == 1));
  /* Reopened cancellation/failure prefixes remain usable, and stale usage
   * before a committed summary must not trigger another compaction. */
  r.mode = 0;
  r.replies = 100;
  r.summaries = 0;
  text.data = S("after recovery reopen");
  assert(pg_session_run(r.session, &input, 1, &cfg) == PG_SESSION_OK);
  assert(r.replies == 101 && r.summaries == 0);
  pg_session_destroy(r.session);
  remove(path);
}
typedef struct {
  pangu_session *session;
  int cancelled;
  int requests, tools;
} files_state;
/* Deterministic fixture validator accepts exactly the one scripted payload. */
static pangu_status file_validate(void *ctx, pangu_slice schema,
                                   pangu_slice args) {
  (void)ctx;
  (void)schema;
  const char *expected = "{\"path\":\"both.txt\"}";
  return args.len == strlen(expected) &&
                 !memcmp(args.data, expected, args.len)
             ? PANGU_OK : PANGU_INVALID;
}
static pangu_status file_execute(void *ctx, pangu_core *core,
                                  const pangu_content *call,
                                  pangu_tool_sink sink, void *out) {
  (void)core;
  files_state *s = ctx;
  s->tools++;
  assert(slice_contains(call->data, "both.txt"));
  pangu_content c = {.kind = PANGU_TEXT, .data = S("write callback result")};
  pangu_message m = {.role = PANGU_TOOL_RESULT, .content = &c,
                     .content_count = 1,
                     .nested_calls_json =
                         S("{\"calls\":[{\"name\":\"read\",\"arguments\":{"
                           "\"path\":\"nested.txt\"},\"durationMs\":0}],"
                           "\"complete\":false}"),
                     .details_json =
                         S("{\"nestedCalls\":{\"calls\":[{\"name\":\"write\","
                           "\"arguments\":{\"path\":\"ignored-native-priority\"}}]}}")};
  return sink(out, &m, 1);
}
static pangu_status file_model(void *ctx, pangu_core *core,
                                pangu_model_sink sink, void *out) {
  files_state *s = ctx;
  int first = s->requests++ == 0;
  if (!first) {
    const pangu_message *result = pangu_core_message_at(
        core, pangu_core_message_count(core) - 1);
    assert(result->role == PANGU_TOOL_RESULT &&
           slice_contains(result->content[0].data, "write callback result"));
  }
  pangu_content c = first
      ? (pangu_content){.kind = PANGU_TOOL_CALL, .name = S("write"),
                         .id = S("executed-write"),
                         .data = S("{\"path\":\"both.txt\"}")}
      : (pangu_content){.kind = PANGU_TEXT, .data = S("write completed")};
  pangu_message m = {.role = PANGU_ASSISTANT, .content = &c,
                     .content_count = 1,
                     .stop_reason = first ? PANGU_TOOL_USE : PANGU_STOP};
  return sink(out, &m, 1);
}
static pangu_status file_summary(void *ctx, pangu_core *core,
                                 pangu_model_sink sink, void *out) {
  files_state *s = ctx;
  assert(slice_contains(pangu_core_message_at(core, 0)->content[0].data,
                        "Do NOT continue"));
  if (s->cancelled) {
    pg_session_cancel(s->session);
    return PANGU_CANCELLED;
  }
  pangu_content c = {.kind = PANGU_TEXT, .data = S("checkpoint")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP,
                     .usage = {.total = 7}};
  return sink(out, &m, 1);
}
static void append_message(pangu_session *s, const pangu_message *m) {
  assert(pg_session_begin(s) == PG_SESSION_OK);
  assert(pg_session_append(s, m) == PG_SESSION_OK);
  assert(pg_session_commit(s) == PG_SESSION_OK);
}
static void file_call(pangu_session *s, const char *name, const char *args) {
  pangu_content c = {.kind = PANGU_TOOL_CALL,
                     .name = {name, strlen(name)},
                     .data = {args, strlen(args)},
                     .id = S("file-call")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &c,
                     .content_count = 1,
                     .stop_reason = PANGU_TOOL_USE};
  append_message(s, &m);
  /* Source records attempted operations even if the result is an error. */
  c = (pangu_content){.kind = PANGU_TEXT, .data = S("synthetic result")};
  m = (pangu_message){.role = PANGU_TOOL_RESULT,
                      .content = &c,
                      .content_count = 1,
                      .tool_call_id = S("file-call"),
                      .is_error = 1};
  append_message(s, &m);
}
static const pangu_message *latest_summary(pangu_session *s) {
  for (size_t i = pg_session_entry_count(s); i; i--) {
    pg_session_entry entry;
    assert(pg_session_entry_at(s, i - 1, &entry) == PG_SESSION_OK);
    if (entry.kind == PG_ENTRY_COMPACTION)
      return entry.message;
  }
  assert(0);
  return NULL;
}
static void check_file_details(pangu_session *s, const char *expected) {
  const pangu_message *m = latest_summary(s);
  assert(m->details_json.len == strlen(expected) &&
         !memcmp(m->details_json.data, expected, m->details_json.len));
}
static void file_metadata(pg_session_backend backend) {
  const char *path =
      backend == PG_STORE_JSONL ? "file-metadata.jsonl" : "file-metadata.db";
  remove(path);
  pg_session_options options = {{path, strlen(path)}, backend, 1, 0};
  files_state state = {0};
  assert(pg_session_open(&options, &state.session, NULL) == PG_SESSION_OK);
  pangu_session *s = state.session;
  append(s, PANGU_USER, "old context");
  file_call(s, "read", "{\"path\":\"a.txt\"}");
  file_call(s, "read", "{\"path\":\"a.txt\"}");
  file_call(s, "read", "{\"path\":\"both.txt\"}");
  pangu_tool tool = {.name = S("write"), .schema_json = S("{}"),
                     .ctx = &state, .execute = file_execute,
                     .validate = file_validate};
  pangu_loop_config run = {.ctx = &state, .model = file_model,
                           .tools = &tool, .tool_count = 1};
  pangu_content input_text = {.kind = PANGU_TEXT, .data = S("write file")};
  pangu_message input = {.role = PANGU_USER, .content = &input_text,
                         .content_count = 1};
  assert(pg_session_run(s, &input, 1, &run) == PG_SESSION_OK);
  assert(state.requests == 2 && state.tools == 1);
  file_call(s, "edit", "{\"path\":\"edit.txt\"}");
  file_call(s, "read", "{\"path\":\"\\ue000\"}");
  file_call(s, "read", "{\"path\":\"\\ud83d\\ude00\"}");
  file_call(s, "read", "{\"path\":7}");
  file_call(s, "read", "{\"path\":\"\"}");
  file_call(s, "read", "{broken");
  file_call(s, "not-read", "{\"path\":\"ignored\"}");
  pangu_message nested = {
      .role = PANGU_TOOL_RESULT,
      .is_error = 1,
      .details_json =
          S("{\"nestedCalls\":{\"complete\":false,\"calls\":["
            "{\"name\":\"read\",\"arguments\":{\"path\":\"nested.txt\"}},"
            "{\"name\":\"edit\",\"arguments\":{\"path\":\"both.txt\"}},"
            "{\"name\":\"write\",\"argumentsBytes\":40000}]}}")};
  append_message(s, &nested);
  nested.nested_calls_json = S("{}");
  nested.details_json =
      S("{\"nestedCalls\":{\"calls\":[{\"name\":\"write\",\"arguments\":{"
        "\"path\":\"ignored-empty-standard\"}}]}}");
  append_message(s, &nested);
  file_call(s, "read", "{\"path\":\"retained.txt\"}");
  /* Standard nestedCalls must survive disk before the summary consumes it. */
  pg_session_destroy(s);
  options.create = 0;
  assert(pg_session_open(&options, &state.session, NULL) == PG_SESSION_OK);
  s = state.session;
  pangu_loop_config cfg = {.ctx = &state, .model = file_summary};
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_OK);
  const char *first =
      "{\"readFiles\":[\"a.txt\",\"nested.txt\",\"😀\",\"\"],"
      "\"modifiedFiles\":[\"both.txt\",\"edit.txt\"]}";
  check_file_details(s, first);
  assert(slice_contains(
      latest_summary(s)->content[0].data,
      "checkpoint\n\n<read-files>\na.txt\nnested.txt\n😀\n\n</read-files>"
      "\n\n<modified-files>\nboth.txt\nedit.txt\n</modified-files>"));
  assert(!slice_contains(latest_summary(s)->details_json, "retained.txt"));
  const pangu_core *kept = pg_session_messages(s);
  size_t kept_count = pangu_core_message_count(kept);
  /* Actual execution also persists tool declarations in system messages. */
  assert(kept_count >= 3 &&
         pangu_core_message_at(kept, kept_count - 2)->content[0].kind ==
             PANGU_TOOL_CALL &&
         pangu_core_message_at(kept, kept_count - 1)->role == PANGU_TOOL_RESULT);
  uint64_t marker = pg_session_leaf(s);
  /* An inactive branch's operations must not enter the next summary. */
  file_call(s, "write", "{\"path\":\"inactive.txt\"}");
  assert(pg_session_switch(s, marker) == PG_SESSION_OK);
  pg_session_destroy(s);
  options.create = 0;
  assert(pg_session_open(&options, &state.session, NULL) == PG_SESSION_OK);
  s = state.session;
  check_file_details(s, first);
  file_call(s, "read", "{\"path\":\"tail.txt\"}");
  file_call(s, "edit", "{\"path\":\"a.txt\"}");
  append(s, PANGU_USER, "keep two");
  uint64_t seq = pg_session_sequence(s), leaf = pg_session_leaf(s);
  state.cancelled = 1;
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_CANCELLED);
  assert(pg_session_sequence(s) == seq && pg_session_leaf(s) == leaf);
  check_file_details(s, first);
  state.cancelled = 0;
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_OK);
  const char *second =
      "{\"readFiles\":[\"nested.txt\",\"retained.txt\",\"tail.txt\",\"😀\","
      "\"\"],"
      "\"modifiedFiles\":[\"a.txt\",\"both.txt\",\"edit.txt\"]}";
  check_file_details(s, second);
  assert(!slice_contains(latest_summary(s)->content[0].data, "inactive.txt"));
  pg_session_destroy(s);
  assert(pg_session_open(&options, &state.session, NULL) == PG_SESSION_OK);
  s = state.session;
  check_file_details(s, second);
  /* A host-supplied summary has no generated file details to inherit. */
  append(s, PANGU_USER, "manual kept");
  assert(pg_session_compact_summary(s, pg_session_leaf(s), S("host summary"),
                                    NULL) == PG_SESSION_OK);
  assert(!latest_summary(s)->details_json.len);
  file_call(s, "write", "{\"path\":\"hook-new.txt\"}");
  append(s, PANGU_USER, "keep three");
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_OK);
  check_file_details(s,
                     "{\"readFiles\":[],\"modifiedFiles\":[\"hook-new.txt\"]}");
  /* Reset drops inherited operations; an empty set adds no XML suffix. */
  assert(pg_session_reset(s, (pangu_slice){0}) == PG_SESSION_OK);
  append(s, PANGU_USER, "no files");
  file_call(s, "read", "null");
  file_call(s, "read", "{\"path\":false}");
  append(s, PANGU_USER, "keep empty tail");
  assert(pg_session_compact(s, 1, &cfg) == PG_SESSION_OK);
  check_file_details(s, "{\"readFiles\":[],\"modifiedFiles\":[]}");
  assert(slice_contains(latest_summary(s)->content[0].data, "checkpoint") &&
         !slice_contains(latest_summary(s)->content[0].data, "<read-files>") &&
         !slice_contains(latest_summary(s)->content[0].data, "<modified-files>"));
  pg_session_destroy(s);
  assert(pg_session_open(&options, &state.session, NULL) == PG_SESSION_OK);
  check_file_details(state.session,
                     "{\"readFiles\":[],\"modifiedFiles\":[]}");
  pg_session_destroy(state.session);
  remove(path);
}
int main(void) {
  tree(PG_STORE_JSONL);
  tree(PG_STORE_SQLITE);
  compaction();
  boundaries();
  automatic();
  unicode_estimate();
  file_metadata(PG_STORE_JSONL);
  file_metadata(PG_STORE_SQLITE);
  for (int backend = PG_STORE_JSONL; backend <= PG_STORE_SQLITE; backend++)
    for (int mode = 0; mode < 19; mode++)
      recovery((pg_session_backend)backend, mode);
  puts("tree branches/reset/summary/model-cancel/reopen passed");
  return 0;
}
