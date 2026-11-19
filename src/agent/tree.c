/* Pi d5b3de22 SessionManager branch()/buildSessionContext, Durable compaction
 * and reset head markers. MIT notice: third-party/Pi-LICENSE. */
#include "internal.h"
#include <pangu/core/transcript.h>
#include <stdlib.h>
#include <string.h>
void pg_tree_clear(pg_tree *t) {
  for (size_t i = 0; i < t->count; i++) {
    pangu_core_destroy(t->nodes[i].value);
    pangu_core_destroy(t->nodes[i].input);
  }
  free(t->nodes);
  *t = (pg_tree){0};
}
pg_session_status pg_tree_append(pg_tree *t, pg_entry_kind kind,
                                 const pangu_message *m, uint64_t kept) {
  if (t->count >= SIZE_MAX / sizeof(*t->nodes) || t->count == UINT64_MAX)
    return PG_SESSION_NOMEM;
  pangu_core *c = NULL;
  if (m) {
    pangu_status z = pangu_core_create(NULL, &c);
    if (z != PANGU_OK)
      return PG_SESSION_NOMEM;
    z = pangu_core_append(c, m);
    if (z != PANGU_OK) {
      pangu_core_destroy(c);
      return z == PANGU_NOMEM ? PG_SESSION_NOMEM : PG_SESSION_INVALID;
    }
  }
  pg_tree_node *nodes = realloc(t->nodes, (t->count + 1) * sizeof(*nodes));
  if (!nodes) {
    pangu_core_destroy(c);
    return PG_SESSION_NOMEM;
  }
  t->nodes = nodes;
  t->nodes[t->count++] = (pg_tree_node){.parent=t->leaf, .first_kept=kept, .kind=kind, .value=c};
  t->leaf = t->count;
  return PG_SESSION_OK;
}
pg_session_status pg_tree_clone(const pg_tree *src, pg_tree *dst) {
  *dst = (pg_tree){0};
  for (size_t i = 0; i < src->count; i++) {
    const pg_tree_node *n = &src->nodes[i];
    dst->leaf = n->parent;
    pg_session_status z = pg_tree_append(
        dst, n->kind, pangu_core_message_at(n->value, 0), n->first_kept);
    if (z == PG_SESSION_OK && n->input)
      z = pg_input_copy(pangu_core_message_at(n->input, 0),
                         &dst->nodes[i].input);
    if (z != PG_SESSION_OK) {
      pg_tree_clear(dst);
      return z;
    }
  }
  dst->leaf = src->leaf;
  return PG_SESSION_OK;
}
yyjson_mut_val *pg_tree_encode(yyjson_mut_doc *d, const pg_tree *t) {
  yyjson_mut_val *r = yyjson_mut_obj(d), *a = yyjson_mut_arr(d);
  if (!r || !a)
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, r, "leaf", t->leaf) ||
      !yyjson_mut_obj_add_val(d, r, "nodes", a))
    return NULL;
  for (size_t i = 0; i < t->count; i++) {
    const pg_tree_node *n = &t->nodes[i];
    yyjson_mut_val *v = yyjson_mut_obj(d);
    if (!v)
      return NULL;
    if (!yyjson_mut_obj_add_uint(d, v, "parent", n->parent) ||
        !yyjson_mut_obj_add_uint(d, v, "first_kept", n->first_kept) ||
        !yyjson_mut_obj_add_uint(d, v, "kind", n->kind))
      return NULL;
    if (n->value) {
      yyjson_mut_val *m =
          pg_encode_message(d, pangu_core_message_at(n->value, 0));
      if (!m || !yyjson_mut_obj_add_val(d, v, "message", m))
        return NULL;
    }
    if (n->input) {
      pangu_slice raw = pangu_core_message_at(n->input, 0)->content[0].data;
      if (!yyjson_mut_obj_add_strncpy(d, v, "original_input",
                                       raw.data ? raw.data : "", raw.len))
        return NULL;
    }
    if (!yyjson_mut_arr_add_val(a, v))
      return NULL;
  }
  return r;
}
static pg_session_status add(pangu_core *c, uint64_t *ids, uint64_t id,
                             const pangu_message *m) {
  size_t n = pangu_core_message_count(c);
  pangu_status z = pangu_core_append(c, m);
  if (z == PANGU_OK)
    ids[n] = id;
  return z == PANGU_OK      ? PG_SESSION_OK
         : z == PANGU_NOMEM ? PG_SESSION_NOMEM
                            : PG_SESSION_CORRUPT;
}
pg_session_status pg_tree_project(const pg_tree *t, pangu_core **out,
                                  uint64_t **mapping) {
  *out = NULL;
  if (mapping)
    *mapping = NULL;
  if (t->leaf > t->count || t->count > SIZE_MAX / sizeof(uint64_t) - 2)
    return PG_SESSION_CORRUPT;
  uint64_t *path = malloc((t->count + 1) * sizeof(*path)),
           *ids = malloc((t->count + 2) * sizeof(*ids));
  if (!path || !ids) {
    free(path);
    free(ids);
    return PG_SESSION_NOMEM;
  }
  size_t count = 0;
  uint64_t leaf = t->leaf;
  while (leaf) {
    if (leaf > t->count || count >= t->count) {
      free(path);
      free(ids);
      return PG_SESSION_CORRUPT;
    }
    path[count++] = leaf;
    leaf = t->nodes[leaf - 1].parent;
  }
  pangu_core *c = NULL;
  pg_session_status z = pangu_core_create(NULL, &c) == PANGU_OK
                            ? PG_SESSION_OK
                            : PG_SESSION_NOMEM;
  while (z == PG_SESSION_OK && count) {
    uint64_t id = path[--count];
    const pg_tree_node *n = &t->nodes[id - 1];
    if (n->kind == PG_ENTRY_MESSAGE) {
      if (!n->value) {
        z = PG_SESSION_CORRUPT;
        break;
      }
      z = add(c, ids, id, pangu_core_message_at(n->value, 0));
      continue;
    }
    if (n->kind == PG_ENTRY_OMISSION) {
      size_t target = 0, total = pangu_core_message_count(c);
      while (target < total && ids[target] != n->first_kept)
        target++;
      const pangu_message *failed =
          target < total ? pangu_core_message_at(c, target) : NULL;
      if (!failed || failed->role != PANGU_ASSISTANT || n->value) {
        z = PG_SESSION_CORRUPT;
        break;
      }
      pangu_core *next = NULL;
      if (pangu_core_create(NULL, &next) != PANGU_OK) {
        z = PG_SESSION_NOMEM;
        break;
      }
      int in_results = 1;
      for (size_t j = 0; j < total && z == PG_SESSION_OK; j++) {
        const pangu_message *m = pangu_core_message_at(c, j);
        int omit = j == target;
        if (j > target && m->role == PANGU_ASSISTANT)
          in_results = 0;
        if (j > target && in_results && m->role == PANGU_TOOL_RESULT)
          for (size_t k = 0; k < failed->content_count; k++) {
            const pangu_content *call = &failed->content[k];
            if (call->kind == PANGU_TOOL_CALL &&
                call->id.len == m->tool_call_id.len &&
                (!call->id.len ||
                 !memcmp(call->id.data, m->tool_call_id.data, call->id.len)))
              omit = 1;
          }
        if (!omit)
          z = add(next, ids, ids[j], m);
      }
      pangu_core_destroy(c);
      c = next;
      continue;
    }
    pangu_core *prefix = NULL, *next = NULL;
    size_t cut = 0, total = pangu_core_message_count(c);
    if (n->kind == PG_ENTRY_RESET)
      cut = total;
    else if (n->kind == PG_ENTRY_COMPACTION) {
      while (cut < total && ids[cut] != n->first_kept)
        cut++;
      if (cut == total || !n->value) {
        z = PG_SESSION_CORRUPT;
        break;
      }
    } else {
      z = PG_SESSION_CORRUPT;
      break;
    }
    if (pangu_core_create(NULL, &prefix) != PANGU_OK) {
      z = PG_SESSION_NOMEM;
      break;
    }
    for (size_t i = 0; i < cut && z == PG_SESSION_OK; i++) {
      pangu_status r = pangu_core_append(prefix, pangu_core_message_at(c, i));
      if (r != PANGU_OK)
        z = PG_SESSION_NOMEM;
    }
    if (z == PG_SESSION_OK &&
        pangu_core_system_snapshot(prefix, &next) != PANGU_OK)
      z = PG_SESSION_NOMEM;
    pangu_core_destroy(prefix);
    if (z != PG_SESSION_OK) {
      pangu_core_destroy(next);
      break;
    }
    /* Save the previous kept IDs before assigning the rebuilt baseline IDs. */
    size_t kept = n->kind == PG_ENTRY_COMPACTION ? total - cut : 0;
    uint64_t *oldids = NULL;
    if (kept) {
      oldids = malloc(kept * sizeof(*oldids));
      if (!oldids) {
        pangu_core_destroy(next);
        z = PG_SESSION_NOMEM;
        break;
      }
      memcpy(oldids, ids + cut, kept * sizeof(*oldids));
    }
    for (size_t i = 0; i < pangu_core_message_count(next); i++)
      ids[i] = 0;
    if (n->value)
      z = add(next, ids, id, pangu_core_message_at(n->value, 0));
    for (size_t i = 0; i < kept && z == PG_SESSION_OK; i++)
      z = add(next, ids, oldids[i], pangu_core_message_at(c, cut + i));
    free(oldids);
    pangu_core_destroy(c);
    c = next;
  }
  free(path);
  if (z != PG_SESSION_OK) {
    free(ids);
    pangu_core_destroy(c);
    return z;
  }
  *out = c;
  if (mapping)
    *mapping = ids;
  else
    free(ids);
  return PG_SESSION_OK;
}
pg_session_status pg_tree_decode(yyjson_val *v, const pangu_core *legacy,
                                 pg_tree *t) {
  *t = (pg_tree){0};
  pg_session_status z = PG_SESSION_OK;
  if (!v) {
    for (size_t i = 0; i < pangu_core_message_count(legacy); i++) {
      z = pg_tree_append(t, PG_ENTRY_MESSAGE, pangu_core_message_at(legacy, i),
                         0);
      if (z != PG_SESSION_OK)
        break;
    }
    return z;
  }
  yyjson_val *leaf = yyjson_obj_get(v, "leaf"), *a = yyjson_obj_get(v, "nodes"),
             *node;
  size_t i, max;
  if (!yyjson_is_obj(v) || !yyjson_is_uint(leaf) || !yyjson_is_arr(a))
    return PG_SESSION_CORRUPT;
  yyjson_arr_foreach(a, i, max, node) {
    yyjson_val *parent = yyjson_obj_get(node, "parent"),
               *kind = yyjson_obj_get(node, "kind"),
               *kept = yyjson_obj_get(node, "first_kept"),
               *m = yyjson_obj_get(node, "message");
    if (!yyjson_is_obj(node) || !yyjson_is_uint(parent) ||
        yyjson_get_uint(parent) > i || !yyjson_is_uint(kind) ||
        yyjson_get_uint(kind) > PG_ENTRY_OMISSION || !yyjson_is_uint(kept) ||
        yyjson_get_uint(kept) > i) {
      z = PG_SESSION_CORRUPT;
      break;
    }
    pg_entry_kind k = (pg_entry_kind)yyjson_get_uint(kind);
    if (k != PG_ENTRY_RESET && k != PG_ENTRY_OMISSION && !m) {
      z = PG_SESSION_CORRUPT;
      break;
    }
    pangu_core *c = NULL;
    if (m) {
      if (pangu_core_create(NULL, &c) != PANGU_OK) {
        z = PG_SESSION_NOMEM;
        break;
      }
      z = pg_decode_message(m, c);
      if (z != PG_SESSION_OK) {
        pangu_core_destroy(c);
        break;
      }
    }
    t->leaf = yyjson_get_uint(parent);
    z = pg_tree_append(t, k, pangu_core_message_at(c, 0),
                       yyjson_get_uint(kept));
    pangu_core_destroy(c);
    yyjson_val *raw = yyjson_obj_get(node, "original_input");
    if (z == PG_SESSION_OK && raw) {
      const pangu_message *stored = pangu_core_message_at(t->nodes[i].value, 0);
      if (!yyjson_is_str(raw) || k != PG_ENTRY_MESSAGE || !stored ||
          stored->role != PANGU_USER)
        z = PG_SESSION_CORRUPT;
      else {
        pangu_content text = {.kind=PANGU_TEXT,
                              .data={yyjson_get_str(raw), yyjson_get_len(raw)}};
        pangu_message original = {.role=PANGU_USER, .content=&text, .content_count=1};
        z = pg_input_copy(&original, &t->nodes[i].input);
      }
    }
    if (z != PG_SESSION_OK)
      break;
  }
  if (z == PG_SESSION_OK) {
    for (size_t j = 0; j < t->count; j++) {
      const pg_tree_node *entry = &t->nodes[j];
      if (entry->kind == PG_ENTRY_MESSAGE && entry->first_kept) {
        z = PG_SESSION_CORRUPT;
        break;
      }
      if (entry->kind == PG_ENTRY_RESET && entry->first_kept) {
        z = PG_SESSION_CORRUPT;
        break;
      }
      const pangu_message *message = pangu_core_message_at(entry->value, 0);
      if (entry->kind == PG_ENTRY_OMISSION && message) {
        z = PG_SESSION_CORRUPT;
        break;
      }
      if (entry->kind != PG_ENTRY_MESSAGE && message &&
          message->role != PANGU_USER) {
        z = PG_SESSION_CORRUPT;
        break;
      }
      if (entry->kind == PG_ENTRY_COMPACTION ||
          entry->kind == PG_ENTRY_OMISSION) {
        if (!entry->first_kept) {
          z = PG_SESSION_CORRUPT;
          break;
        }
        t->leaf = j + 1;
        pangu_core *check = NULL;
        z = pg_tree_project(t, &check, NULL);
        pangu_core_destroy(check);
        if (z != PG_SESSION_OK)
          break;
      }
    }
  }
  if (z == PG_SESSION_OK) {
    t->leaf = yyjson_get_uint(leaf);
    pangu_core *projected = NULL;
    z = pg_tree_project(t, &projected, NULL);
    pangu_core_destroy(projected);
  }
  if (z != PG_SESSION_OK)
    pg_tree_clear(t);
  return z;
}
size_t pg_session_entry_count(const pangu_session *s) {
  return s && !s->closed ? s->tree.count : 0;
}
uint64_t pg_session_leaf(const pangu_session *s) {
  return s && !s->closed ? s->tree.leaf : 0;
}
pg_session_status pg_session_entry_at(const pangu_session *s, size_t i,
                                      pg_session_entry *out) {
  if (!s || s->closed || !out)
    return PG_SESSION_INVALID;
  if (i >= s->tree.count)
    return PG_SESSION_NOT_FOUND;
  const pg_tree_node *n = &s->tree.nodes[i];
  *out = (pg_session_entry){i + 1, n->parent, n->first_kept, n->kind,
                            pangu_core_message_at(n->value, 0)};
  return PG_SESSION_OK;
}
static pg_session_status mutation(pangu_session *s) {
  if (!s || s->closed)
    return PG_SESSION_INVALID;
  if (s->dispatch || s->running_flag || s->operation || s->draft)
    return PG_SESSION_BUSY;
  if (s->readonly)
    return PG_SESSION_READ_ONLY;
  return pg_session_begin_internal(s);
}
static pg_session_status finish(pangu_session *s) {
  pangu_core *c = NULL;
  pg_session_status z = pg_tree_project(&s->draft_tree, &c, NULL);
  if (z == PG_SESSION_OK) {
    pangu_core_destroy(s->draft);
    s->draft = c;
    s->context_changed = 1;
    z = pg_session_commit_internal(s);
  }
  if (z != PG_SESSION_OK)
    pg_session_rollback_internal(s);
  return z;
}
/* Source _omitRecoveryAttempt: append a durable context edit, retaining the
 * attempt and usage in the archive. Projection also omits paired tool results.
 */
pg_session_status pg_session_omit_attempt(pangu_session *s, uint64_t target) {
  pg_session_status z = pg_session_begin_internal(s);
  if (z == PG_SESSION_OK)
    z = pg_tree_append(&s->draft_tree, PG_ENTRY_OMISSION, NULL, target);
  if (z == PG_SESSION_OK)
    return finish(s);
  pg_session_rollback_internal(s);
  return z;
}
pg_session_status pg_session_switch(pangu_session *s, uint64_t id) {
  if (!s || s->closed)
    return PG_SESSION_INVALID;
  if (id > s->tree.count)
    return PG_SESSION_NOT_FOUND;
  pg_session_status z = mutation(s);
  if (z != PG_SESSION_OK)
    return z;
  s->draft_tree.leaf = id;
  return finish(s);
}
pg_session_status pg_session_reset(pangu_session *s, pangu_slice handoff) {
  if (!handoff.data && handoff.len)
    return PG_SESSION_INVALID;
  pg_session_status z = mutation(s);
  if (z != PG_SESSION_OK)
    return z;
  pangu_content c = {.kind = PANGU_TEXT, .data = handoff};
  pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
  z = pg_tree_append(&s->draft_tree, PG_ENTRY_RESET, handoff.len ? &m : NULL,
                     0);
  if (z == PG_SESSION_OK)
    return finish(s);
  pg_session_rollback_internal(s);
  return z;
}
pg_session_status pg_session_compact_summary_internal(pangu_session *s,
                                                      uint64_t kept,
                                                      pangu_slice summary,
                                                      const pangu_usage *usage,
                                                      pangu_slice details) {
  if (!summary.data || !summary.len || summary.len > SIZE_MAX - 120)
    return PG_SESSION_INVALID;
  if (!s || s->closed)
    return PG_SESSION_INVALID;
  pangu_core *view = NULL;
  uint64_t *ids = NULL;
  pg_session_status z = pg_tree_project(&s->tree, &view, &ids);
  if (z != PG_SESSION_OK)
    return z;
  size_t i = 0, total = pangu_core_message_count(view);
  while (i < total && ids[i] != kept)
    i++;
  int has_prefix = 0;
  for (size_t j = 0; j < i; j++)
    if (pangu_core_message_at(view, j)->role != PANGU_SYSTEM)
      has_prefix = 1;
  const pangu_message *anchor =
      i < total ? pangu_core_message_at(view, i) : NULL;
  int valid = anchor &&
              (anchor->role == PANGU_USER || anchor->role == PANGU_ASSISTANT) &&
              has_prefix && kept;
  /* A user injected between calls and their results cannot split that batch. */
  if (valid && anchor->role == PANGU_USER) {
    size_t prev = i;
    while (prev &&
           pangu_core_message_at(view, prev - 1)->role != PANGU_ASSISTANT)
      prev--;
    const pangu_message *a =
        prev ? pangu_core_message_at(view, prev - 1) : NULL;
    for (size_t j = i + 1;
         a && j < total &&
         pangu_core_message_at(view, j)->role != PANGU_ASSISTANT;
         j++) {
      const pangu_message *result = pangu_core_message_at(view, j);
      if (result->role != PANGU_TOOL_RESULT)
        continue;
      for (size_t k = 0; k < a->content_count; k++)
        if (a->content[k].kind == PANGU_TOOL_CALL &&
            a->content[k].id.len == result->tool_call_id.len &&
            !memcmp(a->content[k].id.data, result->tool_call_id.data,
                    result->tool_call_id.len))
          valid = 0;
    }
  }
  free(ids);
  pangu_core_destroy(view);
  if (!valid)
    return PG_SESSION_INVALID;
  if (s->dispatch || s->running_flag || s->draft)
    return PG_SESSION_BUSY;
  if (s->readonly)
    return PG_SESSION_READ_ONLY;
  z = pg_session_begin_internal(s);
  if (z != PG_SESSION_OK)
    return z;
  static const char prefix[] =
      "The conversation history before this point was compacted into the "
      "following summary:\n\n<summary>\n",
                    suffix[] = "\n</summary>";
  size_t n = sizeof(prefix) - 1 + summary.len + sizeof(suffix) - 1;
  char *text = malloc(n);
  if (!text) {
    pg_session_rollback_internal(s);
    return PG_SESSION_NOMEM;
  }
  memcpy(text, prefix, sizeof(prefix) - 1);
  memcpy(text + sizeof(prefix) - 1, summary.data, summary.len);
  memcpy(text + sizeof(prefix) - 1 + summary.len, suffix, sizeof(suffix) - 1);
  pangu_content c = {.kind = PANGU_TEXT, .data = {text, n}};
  pangu_message m = {.role = PANGU_USER, .content = &c, .content_count = 1};
  m.details_json = details;
  if (usage)
    m.usage = *usage;
  z = pg_tree_append(&s->draft_tree, PG_ENTRY_COMPACTION, &m, kept);
  free(text);
  if (z == PG_SESSION_OK)
    return finish(s);
  pg_session_rollback_internal(s);
  return z;
}
pg_session_status pg_session_compact_summary(pangu_session *s, uint64_t kept,
                                             pangu_slice summary,
                                             const pangu_usage *usage) {
  if (s && s->operation)
    return PG_SESSION_BUSY;
  return pg_session_compact_summary_internal(s, kept, summary, usage,
                                             (pangu_slice){0});
}
pangu_usage pg_session_usage(const pangu_session *s) {
  pangu_usage sum = {0};
  if (!s || s->closed)
    return sum;
  for (size_t i = 0; i < s->tree.count; i++) {
    const pangu_message *m = pangu_core_message_at(s->tree.nodes[i].value, 0);
    if (!m)
      continue;
#define TOTAL(f) sum.f += m->usage.f
    TOTAL(input);
    TOTAL(output);
    TOTAL(cache_read);
    TOTAL(cache_write);
    TOTAL(total);
    TOTAL(input_cost);
    TOTAL(output_cost);
    TOTAL(cache_read_cost);
    TOTAL(cache_write_cost);
    TOTAL(total_cost);
    TOTAL(cache_write_1h);
    TOTAL(reasoning);
#undef TOTAL
    sum.has_cache_write_1h |= m->usage.has_cache_write_1h;
    sum.has_reasoning |= m->usage.has_reasoning;
  }
  /* A sum has no single provider-origin total; provenance remains on entries.
   */
  return sum;
}
