/* Explicit original-input evidence, separate from provider transcript text. */
#include "internal.h"
#include <stdlib.h>
#include <string.h>
static pg_session_status mapped(pangu_status z) {
  return z == PANGU_OK      ? PG_SESSION_OK
         : z == PANGU_NOMEM ? PG_SESSION_NOMEM
                            : PG_SESSION_INVALID;
}
pg_session_status pg_input_copy(const pangu_message *m, pangu_core **out) {
  *out = NULL;
  if (m && (m->role != PANGU_USER || (m->content_count && !m->content)))
    return PG_SESSION_INVALID;
  size_t len = 0;
  for (size_t i = 0; m && i < m->content_count; i++) {
    const pangu_content *c = &m->content[i];
    if (c->kind != PANGU_TEXT)
      continue;
    if ((!c->data.data && c->data.len) || c->data.len > SIZE_MAX - len)
      return PG_SESSION_INVALID;
    len += c->data.len;
  }
  char *text = len ? malloc(len) : NULL;
  if (len && !text)
    return PG_SESSION_NOMEM;
  size_t at = 0;
  for (size_t i = 0; m && i < m->content_count; i++) {
    const pangu_content *c = &m->content[i];
    if (c->kind == PANGU_TEXT && c->data.len) {
      memcpy(text + at, c->data.data, c->data.len);
      at += c->data.len;
    }
  }
  pangu_content c = {.kind = PANGU_TEXT, .data = {text, len}};
  pangu_message raw = {
      .role = PANGU_USER, .content = m ? &c : NULL, .content_count = m ? 1 : 0};
  pg_session_status z = mapped(pangu_core_create(NULL, out));
  if (z == PG_SESSION_OK)
    z = mapped(pangu_core_append(*out, &raw));
  free(text);
  if (z != PG_SESSION_OK) {
    pangu_core_destroy(*out);
    *out = NULL;
  }
  return z;
}
pg_session_status pg_input_attach(pangu_session *s,
                                  const pangu_message *message) {
  if (message->role != PANGU_USER)
    return PG_SESSION_OK;
  const pangu_message *raw = s->placed_queue ? s->placed_input : NULL;
  if (!s->placed_queue && s->input_expected) {
    yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
    if (!d)
      return PG_SESSION_NOMEM;
    yyjson_mut_val *incoming = pg_encode_message(d, message);
    if (!incoming) {
      yyjson_mut_doc_free(d);
      return PG_SESSION_NOMEM;
    }
    for (size_t i = s->input_cursor;
         i < pangu_core_message_count(s->input_expected); i++) {
      const pangu_message *expected =
          pangu_core_message_at(s->input_expected, i);
      if (expected->role != PANGU_USER)
        continue;
      yyjson_mut_val *v = pg_encode_message(d, expected);
      if (!v) {
        yyjson_mut_doc_free(d);
        return PG_SESSION_NOMEM;
      }
      if (yyjson_mut_equals(incoming, v)) {
        raw = pangu_core_message_at(s->input_originals, i);
        s->input_cursor = i + 1;
        break;
      }
    }
    yyjson_mut_doc_free(d);
  }
  if (!raw || !raw->content_count)
    return PG_SESSION_OK;
  return pg_input_copy(raw,
                       &s->draft_tree.nodes[s->draft_tree.count - 1].input);
}
pg_session_status pg_raw_queues_restore(yyjson_val *v,
                                        const pangu_core *accepted,
                                        pangu_core **out) {
  pg_session_status z = pg_queues_decode(v, out);
  if (z != PG_SESSION_OK)
    return z;
  for (int k = PANGU_STEERING; k <= PANGU_FOLLOW_UP; k++) {
    size_t count = pangu_core_queue_count(accepted, k);
    if (!v) {
      pangu_message unknown = {.role = PANGU_USER};
      for (size_t i = 0; i < count; i++) {
        z = mapped(pangu_core_enqueue(*out, k, &unknown));
        if (z != PG_SESSION_OK)
          goto fail;
      }
    }
    if (pangu_core_queue_count(*out, k) != count) {
      z = PG_SESSION_CORRUPT;
      goto fail;
    }
    for (size_t i = 0; i < count; i++) {
      const pangu_message *m = pangu_core_queue_at(*out, k, i);
      if (m->content_count > 1 ||
          (m->content_count && m->content[0].kind != PANGU_TEXT)) {
        z = PG_SESSION_CORRUPT;
        goto fail;
      }
    }
  }
  return PG_SESSION_OK;
fail:
  pangu_core_destroy(*out);
  *out = NULL;
  return z;
}
pg_session_status pg_session_run_inputs(pangu_session *s,
                                        const pangu_message *prompts,
                                        size_t count,
                                        const pangu_loop_config *cfg,
                                        const pangu_message *originals) {
  if (!s || s->closed || (count && (!prompts || !originals)))
    return PG_SESSION_INVALID;
  if (s->input_expected || s->operation || s->running_flag || s->dispatch)
    return PG_SESSION_BUSY;
  pangu_core *expected = NULL, *raw = NULL;
  pg_session_status z = mapped(pangu_core_create(NULL, &expected));
  if (z == PG_SESSION_OK)
    z = mapped(pangu_core_create(NULL, &raw));
  for (size_t i = 0; z == PG_SESSION_OK && i < count; i++) {
    if (prompts[i].role != originals[i].role) {
      z = PG_SESSION_INVALID;
      break;
    }
    z = mapped(pangu_core_append(expected, &prompts[i]));
    pangu_core *one = NULL;
    if (z == PG_SESSION_OK)
      z = pg_input_copy(originals[i].role == PANGU_USER ? &originals[i] : NULL,
                        &one);
    if (z == PG_SESSION_OK)
      z = mapped(pangu_core_append(raw, pangu_core_message_at(one, 0)));
    pangu_core_destroy(one);
  }
  if (z == PG_SESSION_OK) {
    s->input_expected = expected;
    s->input_originals = raw;
    s->input_cursor = 0;
    z = pg_session_run(s, prompts, count, cfg);
    s->input_expected = NULL;
    s->input_originals = NULL;
    s->input_cursor = 0;
  }
  pangu_core_destroy(expected);
  pangu_core_destroy(raw);
  return z;
}
pg_session_status pg_session_input_history(const pangu_session *s,
                                           pg_input_history_sink sink,
                                           void *ctx) {
  if (!s || s->closed || !sink)
    return PG_SESSION_INVALID;
  pangu_core *view = NULL, *snapshot = NULL;
  uint64_t *ids = NULL;
  pg_session_status z = pg_tree_project(&s->tree, &view, &ids);
  if (z == PG_SESSION_OK)
    z = mapped(pangu_core_create(NULL, &snapshot));
  for (size_t i = 0; z == PG_SESSION_OK && i < pangu_core_message_count(view);
       i++) {
    if (!ids[i])
      continue;
    const pg_tree_node *node = &s->tree.nodes[ids[i] - 1];
    const pangu_message *m = pangu_core_message_at(view, i);
    if (node->kind != PG_ENTRY_MESSAGE || m->role != PANGU_USER)
      continue;
    if (!node->input) {
      z = PG_SESSION_NOT_FOUND;
      break;
    }
    z = mapped(
        pangu_core_append(snapshot, pangu_core_message_at(node->input, 0)));
  }
  free(ids);
  pangu_core_destroy(view);
  for (size_t i = 0;
       z == PG_SESSION_OK && i < pangu_core_message_count(snapshot); i++)
    z = sink(ctx, pangu_core_message_at(snapshot, i)->content[0].data);
  pangu_core_destroy(snapshot);
  return z;
}
