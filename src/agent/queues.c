/* Session-owned accepted queues. This is default-driver durability, not the
 * Durable task/submission/inbox scheduler or requestId deduplication. */
#include "internal.h"
const pangu_core *pg_queue_source(const pangu_session *s) {
  return s->queue_override ? s->queue_override : s->accepted;
}
pg_session_status pg_queues_clone(const pangu_core *source, pangu_core **out) {
  if (pangu_core_create(NULL, out) != PANGU_OK)
    return PG_SESSION_NOMEM;
  for (int k = PANGU_STEERING; k <= PANGU_FOLLOW_UP; k++) {
    pangu_queue_kind kind = (pangu_queue_kind)k;
    pangu_queue_mode mode = PANGU_QUEUE_ONE;
    if (source)
      pangu_core_get_queue_mode(source, kind, &mode);
    pangu_core_queue_mode(*out, kind, mode);
    for (size_t i = 0; i < pangu_core_queue_count(source, kind); i++) {
      pangu_status z =
          pangu_core_enqueue(*out, kind, pangu_core_queue_at(source, kind, i));
      if (z != PANGU_OK) {
        pangu_core_destroy(*out);
        *out = NULL;
        return z == PANGU_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
      }
    }
  }
  return PG_SESSION_OK;
}
yyjson_mut_val *pg_queues_encode(yyjson_mut_doc *d, const pangu_core *q) {
  yyjson_mut_val *root = yyjson_mut_obj(d);
  if (!root)
    return NULL;
  static const char *names[] = {"steering", "follow_up"},
                    *modes[] = {"steering_mode", "follow_up_mode"};
  for (int k = PANGU_STEERING; k <= PANGU_FOLLOW_UP; k++) {
    yyjson_mut_val *a = yyjson_mut_arr(d);
    pangu_queue_mode mode = PANGU_QUEUE_ONE;
    pangu_core_get_queue_mode(q, (pangu_queue_kind)k, &mode);
    if (!a || !yyjson_mut_obj_add_val(d, root, names[k], a) ||
        !yyjson_mut_obj_add_uint(d, root, modes[k], mode))
      return NULL;
    for (size_t i = 0; i < pangu_core_queue_count(q, (pangu_queue_kind)k);
         i++) {
      yyjson_mut_val *m =
          pg_encode_message(d, pangu_core_queue_at(q, (pangu_queue_kind)k, i));
      if (!m || !yyjson_mut_arr_add_val(a, m))
        return NULL;
    }
  }
  return root;
}
pg_session_status pg_queues_decode(yyjson_val *root, pangu_core **out) {
  *out = NULL;
  if (!root)
    return pg_queues_clone(NULL, out);
  if (!yyjson_is_obj(root))
    return PG_SESSION_CORRUPT;
  if (pangu_core_create(NULL, out) != PANGU_OK)
    return PG_SESSION_NOMEM;
  static const char *names[] = {"steering", "follow_up"},
                    *modes[] = {"steering_mode", "follow_up_mode"};
  pg_session_status status = PG_SESSION_OK;
  for (int k = PANGU_STEERING; k <= PANGU_FOLLOW_UP && status == PG_SESSION_OK;
       k++) {
    yyjson_val *a = yyjson_obj_get(root, names[k]),
               *mode = yyjson_obj_get(root, modes[k]), *m;
    size_t i, max;
    if (!yyjson_is_arr(a) || !yyjson_is_uint(mode) ||
        yyjson_get_uint(mode) > PANGU_QUEUE_ALL) {
      status = PG_SESSION_CORRUPT;
      break;
    }
    pangu_core_queue_mode(*out, (pangu_queue_kind)k,
                          (pangu_queue_mode)yyjson_get_uint(mode));
    yyjson_arr_foreach(a, i, max, m) {
      pangu_core *one = NULL;
      if (pangu_core_create(NULL, &one) != PANGU_OK) {
        status = PG_SESSION_NOMEM;
        break;
      }
      status = pg_decode_message(m, one);
      if (status == PG_SESSION_OK &&
          pangu_core_message_at(one, 0)->role != PANGU_USER)
        status = PG_SESSION_CORRUPT;
      if (status == PG_SESSION_OK) {
        pangu_status z = pangu_core_enqueue(*out, (pangu_queue_kind)k,
                                            pangu_core_message_at(one, 0));
        status = z == PANGU_OK      ? PG_SESSION_OK
                 : z == PANGU_NOMEM ? PG_SESSION_NOMEM
                                    : PG_SESSION_CORRUPT;
      }
      pangu_core_destroy(one);
      if (status != PG_SESSION_OK)
        break;
    }
  }
  if (status != PG_SESSION_OK) {
    pangu_core_destroy(*out);
    *out = NULL;
  }
  return status;
}
size_t pg_session_queue_count(const pangu_session *s, pangu_queue_kind kind) {
  return s && !s->closed ? pangu_core_queue_count(s->accepted, kind) : 0;
}
const pangu_message *pg_session_queue_at(const pangu_session *s,
                                         pangu_queue_kind kind, size_t i) {
  return s && !s->closed ? pangu_core_queue_at(s->accepted, kind, i) : NULL;
}
pg_session_status pg_session_enqueue(pangu_session *s, pangu_queue_kind kind,
                                     const pangu_message *message) {
  return pg_session_enqueue_input(s, kind, message, NULL);
}
pg_session_status pg_session_enqueue_input(pangu_session *s,
                                           pangu_queue_kind kind,
                                           const pangu_message *message,
                                           const pangu_message *original) {
  if (!s || s->closed || !message || message->role != PANGU_USER)
    return PG_SESSION_INVALID;
  if (s->readonly)
    return PG_SESSION_READ_ONLY;
  if (s->draft || (s->running_flag && s->prepared_hook))
    return PG_SESSION_BUSY;
  pangu_core *candidate = NULL;
  pg_session_status z = pg_queues_clone(pg_queue_source(s), &candidate);
  if (z != PG_SESSION_OK)
    return z;
  pangu_status c = pangu_core_enqueue(candidate, kind, message);
  if (c != PANGU_OK) {
    pangu_core_destroy(candidate);
    return c == PANGU_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
  }
  pangu_core *raw = NULL, *one = NULL;
  z = pg_queues_clone(s->raw_queues, &raw);
  if (z == PG_SESSION_OK)
    z = pg_input_copy(original, &one);
  if (z == PG_SESSION_OK) {
    c = pangu_core_enqueue(raw, kind, pangu_core_message_at(one, 0));
    z = c == PANGU_OK      ? PG_SESSION_OK
        : c == PANGU_NOMEM ? PG_SESSION_NOMEM
                           : PG_SESSION_INVALID;
  }
  pangu_core_destroy(one);
  if (z != PG_SESSION_OK) {
    pangu_core_destroy(raw);
    pangu_core_destroy(candidate);
    return z;
  }
  int inject =
      s->running_flag && !s->compaction_core && s->run_error == PG_SESSION_OK;
  size_t previous = pangu_core_queue_count(s->running, kind);
  if (inject) {
    /* Copy before committing; candidate owns input even when caller borrowed
     * it from the committed snapshot that commit will replace. */
    c = pangu_core_enqueue(
        s->running, kind,
        pangu_core_queue_at(candidate, kind,
                            pangu_core_queue_count(candidate, kind) - 1));
    if (c != PANGU_OK) {
      pangu_core_destroy(raw);
      pangu_core_destroy(candidate);
      return c == PANGU_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
    }
  }
  s->queue_override = candidate;
  s->raw_queue_override = raw;
  z = pg_session_begin_internal(s);
  s->queue_override = NULL;
  s->raw_queue_override = NULL;
  if (z == PG_SESSION_OK) {
    z = pg_session_commit_internal(s);
    if (z != PG_SESSION_OK)
      pg_session_rollback_internal(s);
  }
  /* No callbacks occur on a failed commit. The newly appended tail is still
   * unselected, so rollback preserves the exact prior prefix without malloc. */
  if (z != PG_SESSION_OK && inject &&
      pangu_core_truncate_queue(s->running, kind, previous) != PANGU_OK) {
    s->run_error = PG_SESSION_CORE_ERROR;
    pangu_core_cancel(s->running);
    z = PG_SESSION_CORE_ERROR;
  }
  pangu_core_destroy(raw);
  pangu_core_destroy(candidate);
  return z;
}

/* Runtime queues exclude the whole selected batch. Remove only the finalized
 * matching prefix element, so a crash between two ALL-batch inputs preserves
 * the unplaced suffix. Only the default driver's USER mailbox is accepted. */
pg_session_status pg_queues_place(pangu_session *s,
                                  const pangu_message *message) {
  if (message->role != PANGU_USER)
    return PG_SESSION_OK;
  int selected = -1;
  for (int k = PANGU_STEERING; k <= PANGU_FOLLOW_UP; k++) {
    pangu_queue_kind kind = (pangu_queue_kind)k;
    if (pangu_core_queue_count(s->accepted, kind) <=
        pangu_core_queue_count(s->running, kind))
      continue;
    const pangu_message *head = pangu_core_queue_at(s->accepted, kind, 0);
    yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
    if (!d)
      return PG_SESSION_NOMEM;
    yyjson_mut_val *a = pg_encode_message(d, head),
                   *b = pg_encode_message(d, message);
    int allocated = a && b;
    int equal = allocated && yyjson_mut_equals(a, b);
    yyjson_mut_doc_free(d);
    if (!allocated)
      return PG_SESSION_NOMEM;
    if (equal) {
      selected = k;
      break;
    }
  }
  if (selected < 0)
    return PG_SESSION_OK;
  s->placed_queue = 1;
  s->placed_input =
      pangu_core_queue_at(s->raw_queues, (pangu_queue_kind)selected, 0);
  pangu_core *next_raw = NULL;
  pg_session_status rz = pg_queues_clone(s->draft_raw_queues, &next_raw);
  if (rz != PG_SESSION_OK)
    return rz;
  pangu_status pop = PANGU_OK;
  /* Rebuild the selected raw queue without its delivered head. */
  pop = pangu_core_clear_queue(next_raw, (pangu_queue_kind)selected);
  for (size_t i = 1; pop == PANGU_OK &&
                     i < pangu_core_queue_count(s->draft_raw_queues,
                                                (pangu_queue_kind)selected);
       i++)
    pop =
        pangu_core_enqueue(next_raw, (pangu_queue_kind)selected,
                           pangu_core_queue_at(s->draft_raw_queues,
                                               (pangu_queue_kind)selected, i));
  if (pop != PANGU_OK) {
    pangu_core_destroy(next_raw);
    return pop == PANGU_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_CORE_ERROR;
  }
  pangu_core_destroy(s->draft_raw_queues);
  s->draft_raw_queues = next_raw;
  pangu_core *next = NULL;
  if (pangu_core_create(NULL, &next) != PANGU_OK)
    return PG_SESSION_NOMEM;
  for (int k = PANGU_STEERING; k <= PANGU_FOLLOW_UP; k++) {
    pangu_queue_kind kind = (pangu_queue_kind)k;
    pangu_queue_mode mode = PANGU_QUEUE_ONE;
    pangu_core_get_queue_mode(s->running, kind, &mode);
    pangu_core_queue_mode(next, kind, mode);
    for (size_t i = k == selected ? 1 : 0;
         i < pangu_core_queue_count(s->draft_queues, kind); i++) {
      pangu_status z = pangu_core_enqueue(
          next, kind, pangu_core_queue_at(s->draft_queues, kind, i));
      if (z != PANGU_OK) {
        pangu_core_destroy(next);
        return z == PANGU_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
      }
    }
  }
  pangu_core_destroy(s->draft_queues);
  s->draft_queues = next;
  return PG_SESSION_OK;
}

static pg_session_status configure_queue(pangu_session *s,
                                         pangu_queue_kind kind, int clear,
                                         pangu_queue_mode mode) {
  if (!s || s->closed)
    return PG_SESSION_INVALID;
  if (s->dispatch || s->running_flag || s->operation || s->draft)
    return PG_SESSION_BUSY;
  if (s->readonly)
    return PG_SESSION_READ_ONLY;
  pangu_core *candidate = NULL;
  pg_session_status status = pg_queues_clone(s->accepted, &candidate);
  if (status != PG_SESSION_OK)
    return status;
  pangu_status z = clear ? pangu_core_clear_queue(candidate, kind)
                         : pangu_core_queue_mode(candidate, kind, mode);
  if (z != PANGU_OK) {
    pangu_core_destroy(candidate);
    return PG_SESSION_INVALID;
  }
  s->queue_override = candidate;
  status = pg_session_begin_internal(s);
  s->queue_override = NULL;
  if (status == PG_SESSION_OK) {
    pangu_status raw_status =
        clear ? pangu_core_clear_queue(s->draft_raw_queues, kind)
              : pangu_core_queue_mode(s->draft_raw_queues, kind, mode);
    if (raw_status != PANGU_OK) {
      pg_session_rollback_internal(s);
      pangu_core_destroy(candidate);
      return PG_SESSION_INVALID;
    }
    status = pg_session_commit_internal(s);
    if (status != PG_SESSION_OK)
      pg_session_rollback_internal(s);
  }
  pangu_core_destroy(candidate);
  return status;
}
pg_session_status pg_session_clear_queue(pangu_session *s,
                                         pangu_queue_kind kind) {
  return configure_queue(s, kind, 1, PANGU_QUEUE_ONE);
}
pg_session_status pg_session_queue_mode(pangu_session *s, pangu_queue_kind kind,
                                        pangu_queue_mode mode) {
  return configure_queue(s, kind, 0, mode);
}
