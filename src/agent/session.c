/* Pi d5b3de22: session transaction adoption and CodingAgent message_end save.
 * Native synchronous ownership adaptation; see examples/agent/README.md. */
#include "internal.h"
#include <stdlib.h>
#include <string.h>
static char *copy(const char *p, size_t n) {
  if (n == SIZE_MAX)
    return NULL;
  char *v = malloc(n + 1);
  if (v) {
    if (n)
      memcpy(v, p, n);
    v[n] = 0;
  }
  return v;
}
static pg_session_status map(pangu_status s) {
  return s == PANGU_OK          ? PG_SESSION_OK
         : s == PANGU_NOMEM     ? PG_SESSION_NOMEM
         : s == PANGU_BUSY      ? PG_SESSION_BUSY
         : s == PANGU_CANCELLED ? PG_SESSION_CANCELLED
                                : PG_SESSION_CORE_ERROR;
}
static pg_session_status clone(const pangu_core *src, pangu_core **out) {
  pangu_status z = pangu_core_create(NULL, out);
  if (z != PANGU_OK)
    return map(z);
  for (size_t i = 0; i < pangu_core_message_count(src); i++)
    if ((z = pangu_core_append(*out, pangu_core_message_at(src, i))) !=
        PANGU_OK) {
      pangu_core_destroy(*out);
      *out = NULL;
      return map(z);
    }
  return PG_SESSION_OK;
}
static pg_session_status check(pangu_session *s) {
  return !s || s->closed                                  ? PG_SESSION_INVALID
         : s->dispatch || s->running_flag || s->operation ? PG_SESSION_BUSY
         : s->readonly                                    ? PG_SESSION_READ_ONLY
                                                          : PG_SESSION_OK;
}
static void deliver(pangu_session *s, const pg_session_event *event) {
  s->dispatch++;
  size_t count = s->listener_count;
  for (size_t i = 0; i < count; i++) {
    pg_listener listener = s->listeners[i];
    if (listener.fn)
      listener.fn(listener.ctx, s, event);
  }
  s->dispatch--;
}
static void drain(pangu_session *s) {
  if (s->dispatch || s->draining)
    return;
  s->draining = 1;
  while (s->notifications) {
    pg_notification *n = s->notifications;
    s->notifications = n->next;
    if (!s->notifications)
      s->notification_tail = NULL;
    deliver(s, &n->event);
    free(n);
  }
  s->draining = 0;
}
static void publish(pangu_session *s, pg_notification *head,
                    pg_notification *tail) {
  if (s->notification_tail)
    s->notification_tail->next = head;
  else
    s->notifications = head;
  s->notification_tail = tail;
  drain(s);
}
static void emit(pangu_session *s, pg_session_event event) {
  deliver(s, &event);
  drain(s);
}
static pg_session_status restore(pangu_session *s,
                                 pg_session_diagnostic *diag) {
  size_t pos = 0, line = 0;
  uint64_t prev = 0;
  while (pos < s->log_len) {
    const char *end = memchr(s->log + pos, '\n', s->log_len - pos);
    line++;
    if (diag)
      diag->line = line;
    if (!end)
      return PG_SESSION_CORRUPT;
    size_t n = (size_t)(end - (s->log + pos));
    yyjson_doc *d = yyjson_read(s->log + pos, n, 0);
    if (!d)
      return PG_SESSION_CORRUPT;
    yyjson_val *r = yyjson_doc_get_root(d), *f = yyjson_obj_get(r, "format"),
               *v = yyjson_obj_get(r, "version");
    yyjson_val *seq = yyjson_obj_get(r, "seq"),
               *a = yyjson_obj_get(r, "messages"),
               *docs = yyjson_obj_get(r, "documents");
    if (!yyjson_equals_str(f, "pangu.session") || !yyjson_is_uint(v) ||
        yyjson_get_uint(v) != 1 || !yyjson_is_uint(seq) || !yyjson_is_arr(a) ||
        !yyjson_is_obj(docs) ||
        (line == 1 && s->backend == PG_STORE_JSONL &&
         yyjson_get_uint(seq) != 0) ||
        (line > 1 &&
         (prev == UINT64_MAX || yyjson_get_uint(seq) != prev + 1))) {
      yyjson_doc_free(d);
      return PG_SESSION_CORRUPT;
    }
    pangu_core *c = NULL;
    pg_session_status z = map(pangu_core_create(NULL, &c));
    if (z != PG_SESSION_OK) {
      yyjson_doc_free(d);
      return z;
    }
    size_t i, max;
    yyjson_val *msg;
    yyjson_arr_foreach(a, i, max, msg) {
      z = pg_decode_message(msg, c);
      if (z != PG_SESSION_OK)
        break;
    }
    pg_tree tree = {0};
    if (z == PG_SESSION_OK)
      z = pg_tree_decode(yyjson_obj_get(r, "tree"), c, &tree);
    if (z == PG_SESSION_OK && yyjson_obj_get(r, "tree")) {
      pangu_core *projected = NULL;
      z = pg_tree_project(&tree, &projected, NULL);
      if (z == PG_SESSION_OK &&
          pangu_core_message_count(projected) != pangu_core_message_count(c))
        z = PG_SESSION_CORRUPT;
      yyjson_mut_doc *compare = yyjson_mut_doc_new(NULL);
      if (z == PG_SESSION_OK && !compare)
        z = PG_SESSION_NOMEM;
      for (size_t j = 0; z == PG_SESSION_OK && j < pangu_core_message_count(c);
           j++) {
        yyjson_mut_val *a = pg_encode_message(compare,
                                              pangu_core_message_at(c, j)),
                       *b = pg_encode_message(
                           compare, pangu_core_message_at(projected, j));
        if (!a || !b)
          z = PG_SESSION_NOMEM;
        else if (!yyjson_mut_equals(a, b))
          z = PG_SESSION_CORRUPT;
      }
      yyjson_mut_doc_free(compare);
      pangu_core_destroy(projected);
    }
    pg_compaction_policy policy = {0};
    yyjson_val *p = yyjson_obj_get(r, "compaction_policy");
    if (z == PG_SESSION_OK && p) {
      yyjson_val *enabled = yyjson_obj_get(p, "enabled"),
                 *context = yyjson_obj_get(p, "context_tokens"),
                 *reserve = yyjson_obj_get(p, "reserve_tokens"),
                 *keep = yyjson_obj_get(p, "keep_recent_tokens");
      if (!yyjson_is_obj(p) || !yyjson_is_bool(enabled) ||
          !yyjson_is_uint(context) || !yyjson_is_uint(reserve) ||
          !yyjson_is_uint(keep) || yyjson_get_uint(context) > SIZE_MAX ||
          yyjson_get_uint(reserve) > SIZE_MAX ||
          yyjson_get_uint(keep) > SIZE_MAX)
        z = PG_SESSION_CORRUPT;
      else {
        policy = (pg_compaction_policy){
            yyjson_get_bool(enabled), (size_t)yyjson_get_uint(context),
            (size_t)yyjson_get_uint(reserve), (size_t)yyjson_get_uint(keep)};
        if (policy.enabled && (!policy.context_tokens ||
                               policy.reserve_tokens >= policy.context_tokens))
          z = PG_SESSION_CORRUPT;
      }
    }
    pangu_core *queues = NULL;
    if (z == PG_SESSION_OK)
      z = pg_queues_decode(yyjson_obj_get(r, "queues"), &queues);
    pangu_core *raw_queues = NULL;
    if (z == PG_SESSION_OK)
      z = pg_raw_queues_restore(yyjson_obj_get(r, "original_queues"), queues, &raw_queues);
    char *text = z == PG_SESSION_OK ? yyjson_val_write(docs, 0, NULL) : NULL;
    if (z == PG_SESSION_OK && !text)
      z = PG_SESSION_NOMEM;
    char *approval_events = NULL;
    if (z == PG_SESSION_OK)
      z = pg_approval_decode(yyjson_obj_get(r, "approval_events"), &approval_events);
    if (z != PG_SESSION_OK) {
      free(text);
      pangu_core_destroy(raw_queues);
      pangu_core_destroy(queues);
      pg_tree_clear(&tree);
      pangu_core_destroy(c);
      yyjson_doc_free(d);
      return z;
    }
    pg_tree_clear(&s->tree);
    s->tree = tree;
    s->policy = policy;
    pangu_core_destroy(s->accepted);
    s->accepted = queues;
    pangu_core_destroy(s->raw_queues);
    s->raw_queues = raw_queues;
    pangu_core_destroy(s->committed);
    free(s->documents);
    s->committed = c;
    s->documents = text;
    free(s->approval_events);
    s->approval_events = approval_events;
    prev = s->seq = yyjson_get_uint(seq);
    yyjson_doc_free(d);
    pos += (n + 1);
  }
  if (!line)
    return PG_SESSION_CORRUPT;
  if (diag)
    diag->line = 0;
  return PG_SESSION_OK;
}
pg_session_status pg_session_open(const pg_session_options *o,
                                  pangu_session **out,
                                  pg_session_diagnostic *diag) {
  if (out)
    *out = NULL;
  if (diag)
    *diag = (pg_session_diagnostic){0};
  if (!out || !o || !o->path.data || !o->path.len ||
      memchr(o->path.data, 0, o->path.len) || o->backend > PG_STORE_SQLITE ||
      (o->read_only && o->create))
    return PG_SESSION_INVALID;
  pangu_session *s = calloc(1, sizeof(*s));
  if (!s)
    return PG_SESSION_NOMEM;
  s->path = copy(o->path.data, o->path.len);
  s->backend = o->backend;
  s->readonly = o->read_only;
  if (!s->path) {
    free(s);
    return PG_SESSION_NOMEM;
  }
  pg_session_status z = pg_store_open(s, o->create, diag);
  if (z == PG_SESSION_OK)
    z = restore(s, diag);
  if (z == PG_SESSION_OK)
    z = clone(s->committed, &s->running);
  if (z != PG_SESSION_OK) {
    pg_session_destroy(s);
    return z;
  }
  if (!s->readonly) {
    z = pg_approval_recover(s);
    if (z != PG_SESSION_OK) {pg_session_destroy(s); return z;}
  }
  *out = s;
  return PG_SESSION_OK;
}
uint64_t pg_session_sequence(const pangu_session *s) {
  return s && !s->closed ? s->seq : 0;
}
const pangu_core *pg_session_messages(const pangu_session *s) {
  return s && !s->closed ? s->committed : NULL;
}
pangu_slice pg_session_documents(const pangu_session *s) {
  return s && !s->closed && s->documents
             ? (pangu_slice){s->documents, strlen(s->documents)}
             : (pangu_slice){0};
}
pg_session_status pg_session_subscribe(pangu_session *s, pg_session_listener fn,
                                       void *ctx, uint64_t *id) {
  if (!s || s->closed || !fn || !id)
    return PG_SESSION_INVALID;
  if (s->listener_count == SIZE_MAX / sizeof(pg_listener) ||
      s->next_listener == UINT64_MAX)
    return PG_SESSION_NOMEM;
  pg_listener *v = realloc(s->listeners, (s->listener_count + 1) * sizeof(*v));
  if (!v)
    return PG_SESSION_NOMEM;
  s->listeners = v;
  *id = ++s->next_listener;
  s->listeners[s->listener_count++] = (pg_listener){*id, fn, ctx};
  return PG_SESSION_OK;
}
pg_session_status pg_session_unsubscribe(pangu_session *s, uint64_t id) {
  if (!s || s->closed)
    return PG_SESSION_INVALID;
  for (size_t i = 0; i < s->listener_count; i++)
    if (s->listeners[i].id == id && s->listeners[i].fn) {
      s->listeners[i].fn = NULL;
      return PG_SESSION_OK;
    }
  return PG_SESSION_NOT_FOUND;
}
pg_session_status pg_session_begin_internal(pangu_session *s) {
  if (s->draft)
    return PG_SESSION_BUSY;
  pg_session_status z = clone(s->committed, &s->draft);
  if (z != PG_SESSION_OK)
    return z;
  s->draft_documents = copy(s->documents, strlen(s->documents));
  if (!s->draft_documents) {
    pangu_core_destroy(s->draft);
    s->draft = NULL;
    return PG_SESSION_NOMEM;
  }
  s->draft_approval_events = copy(s->approval_events, strlen(s->approval_events));
  if (!s->draft_approval_events) {pg_session_rollback_internal(s); return PG_SESSION_NOMEM;}
  z = pg_tree_clone(&s->tree, &s->draft_tree);
  if (z != PG_SESSION_OK) {
    pg_session_rollback_internal(s);
    return z;
  }
  s->context_changed = 0;
  s->placed_queue = 0;
  s->placed_input = NULL;
  s->draft_policy = s->policy;
  z = pg_queues_clone(pg_queue_source(s), &s->draft_queues);
  if (z != PG_SESSION_OK) {
    pg_session_rollback_internal(s);
    return z;
  }
  z = pg_queues_clone(s->raw_queue_override ? s->raw_queue_override : s->raw_queues,
                       &s->draft_raw_queues);
  if (z != PG_SESSION_OK) {
    pg_session_rollback_internal(s);
    return z;
  }
  return PG_SESSION_OK;
}
pg_session_status pg_session_begin(pangu_session *s) {
  pg_session_status z = check(s);
  return z == PG_SESSION_OK ? pg_session_begin_internal(s) : z;
}
pg_session_status pg_session_append_draft(pangu_session *s,
                                          const pangu_message *m) {
  uint64_t previous = s->draft_tree.leaf;
  pg_session_status z = pg_tree_append(&s->draft_tree, PG_ENTRY_MESSAGE, m, 0);
  if (z != PG_SESSION_OK)
    return z;
  z = pg_input_attach(s, m);
  if (z == PG_SESSION_OK)
    z = map(pangu_core_append(s->draft, m));
  if (z != PG_SESSION_OK) {
    pg_tree_node *node = &s->draft_tree.nodes[--s->draft_tree.count];
    pangu_core_destroy(node->input);
    pangu_core_destroy(node->value);
    s->draft_tree.leaf = previous;
  }
  return z;
}
pg_session_status pg_session_append(pangu_session *s, const pangu_message *m) {
  pg_session_status z = check(s);
  return z != PG_SESSION_OK ? z
         : !s->draft        ? PG_SESSION_INVALID
                            : pg_session_append_draft(s, m);
}
pg_session_status pg_session_set_documents(pangu_session *s, pangu_slice json) {
  pg_session_status z = check(s);
  if (z != PG_SESSION_OK)
    return z;
  if (!s->draft || !json.data)
    return PG_SESSION_INVALID;
  yyjson_doc *d = yyjson_read(json.data, json.len, 0);
  if (!d)
    return PG_SESSION_INVALID;
  if (!yyjson_is_obj(yyjson_doc_get_root(d))) {
    yyjson_doc_free(d);
    return PG_SESSION_INVALID;
  }
  char *p = yyjson_write(d, 0, NULL);
  yyjson_doc_free(d);
  if (!p)
    return PG_SESSION_NOMEM;
  free(s->draft_documents);
  s->draft_documents = p;
  return PG_SESSION_OK;
}
void pg_session_rollback_internal(pangu_session *s) {
  pangu_core_destroy(s->draft);
  s->draft = NULL;
  free(s->draft_documents);
  s->draft_documents = NULL;
  free(s->draft_approval_events);
  s->draft_approval_events = NULL;
  pg_tree_clear(&s->draft_tree);
  pangu_core_destroy(s->draft_queues);
  s->draft_queues = NULL;
  pangu_core_destroy(s->draft_raw_queues);
  s->draft_raw_queues = NULL;
  s->placed_queue = 0;
  s->placed_input = NULL;
  s->context_changed = 0;
}
pg_session_status pg_session_rollback(pangu_session *s) {
  pg_session_status z = check(s);
  if (z != PG_SESSION_OK)
    return z;
  pg_session_rollback_internal(s);
  return PG_SESSION_OK;
}
pg_session_status pg_session_commit_internal(pangu_session *s) {
  if (!s->draft || s->seq == UINT64_MAX)
    return PG_SESSION_INVALID;
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return PG_SESSION_NOMEM;
  yyjson_mut_val *r = yyjson_mut_obj(d), *a = yyjson_mut_arr(d);
  yyjson_doc *docs =
      yyjson_read(s->draft_documents, strlen(s->draft_documents), 0);
  pg_session_status z = PG_SESSION_NOMEM;
  char *text = NULL, *log = NULL;
  pg_notification *commit_event = calloc(1, sizeof(*commit_event)),
                  *context_event = NULL;
  if (s->context_changed)
    context_event = calloc(1, sizeof(*context_event));
  size_t len = 0;
  if (!r || !a || !docs || !commit_event ||
      (s->context_changed && !context_event))
    goto done;
  yyjson_mut_doc_set_root(d, r);
  if (!yyjson_mut_obj_add_str(d, r, "format", "pangu.session") ||
      !yyjson_mut_obj_add_uint(d, r, "version", 1) ||
      !yyjson_mut_obj_add_uint(d, r, "seq", s->seq + 1) ||
      !yyjson_mut_obj_add_val(d, r, "messages", a) ||
      !yyjson_mut_obj_add_val(
          d, r, "documents", yyjson_val_mut_copy(d, yyjson_doc_get_root(docs))))
    goto done;
  for (size_t i = 0; i < pangu_core_message_count(s->draft); i++) {
    yyjson_mut_val *v =
        pg_encode_message(d, pangu_core_message_at(s->draft, i));
    if (!v || !yyjson_mut_arr_add_val(a, v))
      goto done;
  }
  yyjson_doc *audit = yyjson_read(s->draft_approval_events, strlen(s->draft_approval_events), 0);
  if (!audit) goto done;
  yyjson_mut_val *audit_value = yyjson_val_mut_copy(d, yyjson_doc_get_root(audit));
  yyjson_doc_free(audit);
  if (!audit_value || !yyjson_mut_obj_add_val(d, r, "approval_events", audit_value)) goto done;
  yyjson_mut_val *tree = pg_tree_encode(d, &s->draft_tree);
  if (!tree || !yyjson_mut_obj_add_val(d, r, "tree", tree))
    goto done;
  yyjson_mut_val *queues = pg_queues_encode(d, s->draft_queues);
  if (!queues || !yyjson_mut_obj_add_val(d, r, "queues", queues))
    goto done;
  yyjson_mut_val *raw_queues = pg_queues_encode(d, s->draft_raw_queues);
  if (!raw_queues || !yyjson_mut_obj_add_val(d, r, "original_queues", raw_queues))
    goto done;
  yyjson_mut_val *policy = yyjson_mut_obj(d);
  if (!policy ||
      !yyjson_mut_obj_add_bool(d, policy, "enabled", s->draft_policy.enabled) ||
      !yyjson_mut_obj_add_uint(d, policy, "context_tokens",
                               s->draft_policy.context_tokens) ||
      !yyjson_mut_obj_add_uint(d, policy, "reserve_tokens",
                               s->draft_policy.reserve_tokens) ||
      !yyjson_mut_obj_add_uint(d, policy, "keep_recent_tokens",
                               s->draft_policy.keep_recent_tokens) ||
      !yyjson_mut_obj_add_val(d, r, "compaction_policy", policy))
    goto done;
  text = yyjson_mut_write(d, 0, &len);
  if (!text || len > SIZE_MAX - 2)
    goto done;
  size_t prefix = s->backend == PG_STORE_JSONL ? s->log_len : 0;
  if (prefix > SIZE_MAX - len - 2)
    goto done;
  log = malloc(prefix + len + 2);
  if (!log)
    goto done;
  if (prefix)
    memcpy(log, s->log, prefix);
  memcpy(log + prefix, text, len);
  log[prefix + len] = '\n';
  log[prefix + len + 1] = 0;
  z = pg_store_write(s, log, prefix + len + 1);
  if (z != PG_SESSION_OK)
    goto done;
  size_t first = pangu_core_message_count(s->committed),
         count = pangu_core_message_count(s->draft);
  pangu_core_destroy(s->committed);
  s->committed = s->draft;
  s->draft = NULL;
  free(s->documents);
  s->documents = s->draft_documents;
  s->draft_documents = NULL;
  free(s->approval_events);
  s->approval_events = s->draft_approval_events;
  s->draft_approval_events = NULL;
  free(s->log);
  s->log = log;
  log = NULL;
  s->log_len = prefix + len + 1;
  pangu_core_destroy(s->accepted);
  s->accepted = s->draft_queues;
  s->draft_queues = NULL;
  pangu_core_destroy(s->raw_queues);
  s->raw_queues = s->draft_raw_queues;
  s->draft_raw_queues = NULL;
  s->placed_queue = 0;
  s->placed_input = NULL;
  s->policy = s->draft_policy;
  pg_tree_clear(&s->tree);
  s->tree = s->draft_tree;
  s->draft_tree = (pg_tree){0};
  int changed = s->context_changed;
  s->context_changed = 0;
  s->seq++;
  commit_event->event =
      (pg_session_event){PG_SESSION_COMMITTED, s->seq, changed ? 0 : first,
                         changed ? count : count - first, NULL};
  if (changed) {
    context_event->event =
        (pg_session_event){PG_SESSION_CONTEXT_CHANGED, s->seq, 0, count, NULL};
    context_event->next = commit_event;
    publish(s, context_event, commit_event);
  } else
    publish(s, commit_event, commit_event);
  commit_event = NULL;
  context_event = NULL;
done:
  free(commit_event);
  free(context_event);
  free(log);
  free(text);
  yyjson_doc_free(docs);
  yyjson_mut_doc_free(d);
  return z;
}
pg_session_status pg_session_commit(pangu_session *s) {
  pg_session_status z = check(s);
  return z == PG_SESSION_OK ? pg_session_commit_internal(s) : z;
}
typedef struct {
  pangu_session *s;
  const pangu_loop_config *config;
  uint64_t *desired_output;
} bridge;
static void core_event(void *ctx, pangu_core *core, const pangu_event *e) {
  bridge *b = ctx;
  pangu_session *s = b->s;
  if (s->run_error != PG_SESSION_OK)
    return;
  if (e->kind == PANGU_MESSAGE_START && e->message &&
      e->message->role == PANGU_USER)
    s->recovery_attempted = 0;
  if (e->kind == PANGU_MESSAGE_END && e->message &&
      e->message->role == PANGU_ASSISTANT) {
    if (e->message->stop_reason != PANGU_ERROR &&
        e->message->stop_reason != PANGU_LENGTH)
      s->recovery_attempted = 0;
    *b->desired_output = 0;
    const pangu_model_config *model = pangu_core_active_model(core);
    if (model && model->metadata_json.data && model->metadata_json.len) {
      yyjson_read_err error;
      yyjson_doc *d =
          yyjson_read_opts((char *)model->metadata_json.data,
                           model->metadata_json.len, 0, NULL, &error);
      if (!d && error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
        s->run_error = PG_SESSION_NOMEM;
        pangu_core_cancel(core);
        return;
      }
      yyjson_val *v =
          d ? yyjson_obj_get(yyjson_doc_get_root(d), "maxTokens") : NULL;
      if (yyjson_is_uint(v))
        *b->desired_output = yyjson_get_uint(v);
      yyjson_doc_free(d);
    }
  }
  if (e->kind == PANGU_MESSAGE_END && e->message) {
    pg_session_status z = pg_session_begin_internal(s);
    if (z == PG_SESSION_OK)
      z = pg_queues_place(s, e->message);
    if (z == PG_SESSION_OK)
      z = pg_session_append_draft(s, e->message);
    if (z == PG_SESSION_OK)
      z = pg_session_commit_internal(s);
    if (z != PG_SESSION_OK) {
      pg_session_rollback_internal(s);
      s->run_error = z;
      pangu_core_cancel(core);
      return;
    }
  }
  if (b->config->event)
    b->config->event(b->config->ctx, core, e);
  emit(s, (pg_session_event){PG_SESSION_LIVE, s->seq, 0, 0, e});
}
static pg_session_status run_once(pangu_session *s,
                                  const pangu_message *prompts, size_t count,
                                  const pangu_loop_config *cfg,
                                  uint64_t *desired_output) {
  pg_session_status z;
  if (cfg->prepare_next_turn_update &&
      (pangu_core_queue_count(s->accepted, PANGU_STEERING) ||
       pangu_core_queue_count(s->accepted, PANGU_FOLLOW_UP)))
    return PG_SESSION_INVALID;
  pangu_core *next = NULL;
  z = clone(s->committed, &next);
  if (z != PG_SESSION_OK)
    return z;
  for (int k = PANGU_STEERING; k <= PANGU_FOLLOW_UP; k++) {
    pangu_queue_mode mode;
    pangu_core_get_queue_mode(s->accepted, (pangu_queue_kind)k, &mode);
    pangu_core_queue_mode(next, (pangu_queue_kind)k, mode);
    for (size_t i = 0;
         i < pangu_core_queue_count(s->accepted, (pangu_queue_kind)k); i++) {
      z = map(pangu_core_enqueue(
          next, (pangu_queue_kind)k,
          pangu_core_queue_at(s->accepted, (pangu_queue_kind)k, i)));
      if (z != PG_SESSION_OK) {
        pangu_core_destroy(next);
        return z;
      }
    }
  }
  pangu_core_destroy(s->running);
  s->running = next;
  s->running_flag = 1;
  s->prepared_hook = cfg->prepare_next_turn_update != NULL;
  s->run_error = PG_SESSION_OK;
  bridge b = {s, cfg, desired_output};
  uint64_t id;
  pangu_status c = pangu_core_subscribe(next, core_event, &b, &id);
  if (c == PANGU_OK) {
    pangu_loop_config config = *cfg;
    config.event = NULL;
    c = pangu_core_run(next, prompts, count, &config);
    pangu_core_unsubscribe(next, id);
  }
  s->running_flag = 0;
  s->prepared_hook = 0;
  return s->run_error != PG_SESSION_OK ? s->run_error : map(c);
}
pg_session_status pg_session_set_context_classifier(
    pangu_session *s, const pg_context_classifier *classifier, void *ctx) {
  if (!s || s->closed ||
      (classifier && (classifier->size < sizeof(*classifier) ||
                      !classifier->is_context_overflow ||
                      !classifier->is_recoverable_length)))
    return PG_SESSION_INVALID;
  if (s->operation || s->running_flag || s->dispatch || s->draft)
    return PG_SESSION_BUSY;
  s->classifier = classifier;
  s->classifier_ctx = classifier ? ctx : NULL;
  return PG_SESSION_OK;
}
const pg_context_classifier *
pg_session_context_classifier(const pangu_session *s, void **ctx) {
  if (ctx)
    *ctx = s && !s->closed ? s->classifier_ctx : NULL;
  return s && !s->closed ? s->classifier : NULL;
}
static int same_slice(pangu_slice a, pangu_slice b) {
  return a.len == b.len &&
         (!a.len || (a.data && b.data && !memcmp(a.data, b.data, a.len)));
}
static pangu_status recovery_kind(pangu_session *s, const pangu_message *m,
                                  const pangu_loop_config *cfg, size_t capacity,
                                  uint64_t desired_output, int *kind) {
  *kind = 0;
  if (!m || m->role != PANGU_ASSISTANT || m->stop_reason == PANGU_ABORTED ||
      !same_slice(m->model, cfg->model_config.id) ||
      !same_slice(m->provider, cfg->model_config.provider))
    return PANGU_OK;
  /* Match source JS Number arithmetic; cache_write is not context input. */
  double input = (double)m->usage.input + (double)m->usage.cache_read;
  int overflow = 0;
  if (s->classifier) {
    pangu_status status = s->classifier->is_context_overflow(
        s->classifier_ctx, m, capacity, &overflow);
    if (status != PANGU_OK)
      return status;
  } else
    overflow = (m->stop_reason == PANGU_STOP && input > (double)capacity) ||
               (m->stop_reason == PANGU_LENGTH && !m->usage.output &&
                input >= (double)capacity * 0.99);
  int truncated = s->classifier
                      ? s->classifier->is_recoverable_length(s->classifier_ctx,
                                                             m, desired_output)
                      : (m->stop_reason == PANGU_LENGTH && desired_output &&
                         (double)m->usage.output < (double)desired_output);
  if (overflow && m->stop_reason == PANGU_STOP)
    *kind = 1; /* Completed reply must not be replayed. */
  else if ((overflow && (m->stop_reason == PANGU_ERROR ||
                         m->stop_reason == PANGU_LENGTH)) ||
           (truncated && m->stop_reason == PANGU_LENGTH))
    *kind = 2;
  return PANGU_OK;
}
/* The outer operation owns the recovery budget and cancellation across the
 * settled Core, omission commit, summarization, and fresh Core continuation. */
pg_session_status pg_session_run(pangu_session *s, const pangu_message *prompts,
                                 size_t count, const pangu_loop_config *cfg) {
  pg_session_status z = check(s);
  if (z != PG_SESSION_OK)
    return z;
  if (!cfg || s->draft)
    return PG_SESSION_INVALID;
  s->operation = 1;
  s->cancel_requested = s->recovery_attempted = 0;
  if (s->policy.enabled &&
      pg_session_context_tokens(s) >=
          s->policy.context_tokens - s->policy.reserve_tokens) {
    z = pg_session_compact_internal(s, s->policy.keep_recent_tokens, cfg);
    if (z != PG_SESSION_OK || s->cancel_requested)
      goto done;
  }
  uint64_t desired_output = 0;
  z = run_once(s, prompts, count, cfg, &desired_output);
  while (s->policy.enabled && !s->cancel_requested &&
         (z == PG_SESSION_OK || z == PG_SESSION_CORE_ERROR)) {
    pangu_core *view = NULL;
    uint64_t *ids = NULL;
    pg_session_status projected = pg_tree_project(&s->tree, &view, &ids);
    if (projected != PG_SESSION_OK) {
      z = projected;
      break;
    }
    size_t total = pangu_core_message_count(view), assistant_index = total;
    /* Source tracks the last assistant independently of trailing tool results.
     * A finish hook may settle a truncated tool turn after results commit. */
    while (assistant_index &&
           pangu_core_message_at(view, assistant_index - 1)->role !=
               PANGU_ASSISTANT)
      assistant_index--;
    const pangu_message *last =
        assistant_index ? pangu_core_message_at(view, assistant_index - 1)
                        : NULL;
    int kind = 0;
    pangu_status classified = recovery_kind(
        s, last, cfg, s->policy.context_tokens, desired_output, &kind);
    uint64_t target = assistant_index ? ids[assistant_index - 1] : 0;
    free(ids);
    pangu_core_destroy(view);
    if (s->cancel_requested) {
      z = PG_SESSION_CANCELLED;
      break;
    }
    if (classified != PANGU_OK) {
      z = classified == PANGU_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_CORE_ERROR;
      break;
    }
    if (!kind || (kind == 2 && s->recovery_attempted))
      break;
    pg_session_status original = z;
    if (kind == 2) {
      s->recovery_attempted = 1;
      z = pg_session_omit_attempt(s, target);
      if (z != PG_SESSION_OK || s->cancel_requested)
        break;
    }
    uint64_t before = s->tree.leaf;
    z = pg_session_compact_internal(s, s->policy.keep_recent_tokens, cfg);
    if (z != PG_SESSION_OK || s->cancel_requested)
      break;
    if (kind == 1) {
      z = original;
      if (!pangu_core_queue_count(s->accepted, PANGU_STEERING) &&
          !pangu_core_queue_count(s->accepted, PANGU_FOLLOW_UP))
        break; /* Preserve completed reply; only accepted new input continues.
                */
    } else if (s->tree.leaf == before) {
      z = original;
      break; /* No usable cut: no uncompressed automatic retry. */
    }
    desired_output = 0;
    z = run_once(s, NULL, 0, cfg, &desired_output);
  }
done:
  if (s->cancel_requested && z == PG_SESSION_OK)
    z = PG_SESSION_CANCELLED;
  s->operation = 0;
  return z;
}
void pg_session_cancel(pangu_session *s) {
  if (!s || s->closed)
    return;
  if (s->operation)
    s->cancel_requested = 1;
  if (s->running_flag)
    pangu_core_cancel(s->compaction_core ? s->compaction_core : s->running);
  else if (!s->dispatch)
    pg_session_rollback_internal(s);
}
pg_session_status pg_session_close(pangu_session *s) {
  if (!s)
    return PG_SESSION_INVALID;
  if (s->closed)
    return PG_SESSION_OK;
  if (s->dispatch || s->running_flag || s->operation)
    return PG_SESSION_BUSY;
  pg_session_rollback_internal(s);
  s->closed = 1;
  emit(s, (pg_session_event){PG_SESSION_CLOSED, s->seq, 0, 0, NULL});
  if (s->db) {
    sqlite3_close(s->db);
    s->db = NULL;
  }
  return PG_SESSION_OK;
}
void pg_session_destroy(pangu_session *s) {
  if (!s || s->dispatch || s->running_flag || s->operation)
    return;
  pg_session_close(s);
  pangu_core_destroy(s->committed);
  pangu_core_destroy(s->running);
  pangu_core_destroy(s->accepted);
  pangu_core_destroy(s->raw_queues);
  pg_tree_clear(&s->tree);
  pg_tree_clear(&s->draft_tree);
  free(s->path);
  free(s->documents);
  free(s->approval_events);
  free(s->log);
  free(s->listeners);
  free(s);
}
