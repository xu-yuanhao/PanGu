/* Source: Pi Durable harness/compaction.ts, d5b3de22 (MIT). Prompt text and
 * summary wrapping preserved. Synchronous Core model consumer, no task
 * scheduler. */
#include "internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>
typedef struct {
  char *data;
  size_t len, capacity;
} text_buffer;
static int write_text(text_buffer *b, const char *p, size_t n) {
  if (n > SIZE_MAX - b->len - 1)
    return 0;
  size_t required = b->len + n + 1;
  if (required > b->capacity) {
    size_t cap = b->capacity ? b->capacity : 256;
    while (cap < required) {
      if (cap > SIZE_MAX / 2) {
        cap = required;
        break;
      }
      cap *= 2;
    }
    char *next = realloc(b->data, cap);
    if (!next)
      return 0;
    b->data = next;
    b->capacity = cap;
  }
  if (n)
    memcpy(b->data + b->len, p, n);
  b->len += n;
  b->data[b->len] = 0;
  return 1;
}
#define TEXT(b, s) write_text(b, s, sizeof(s) - 1)
static const char system_prompt[] =
    "You are a context summarization assistant. Your task is to read a "
    "conversation between a user and an AI assistant, then produce a "
    "structured summary following the exact format specified.\n\nDo NOT "
    "continue the conversation. Do NOT respond to any questions in the "
    "conversation. ONLY output the structured summary.";
static const char summary_prompt[] =
    "The messages above are a conversation to summarize. Create a structured "
    "context checkpoint summary that another LLM will use to continue the "
    "work. If the conversation starts with an earlier summary, preserve its "
    "information and fold the newer messages into it.\n\nUse this EXACT "
    "format:\n\n## Goal\n[What is the user trying to accomplish? Can be "
    "multiple items if the session covers different tasks.]\n\n## Constraints "
    "& Preferences\n- [Any constraints, preferences, or requirements mentioned "
    "by user]\n- [Or \"(none)\" if none were mentioned]\n\n## Progress\n### "
    "Done\n- [x] [Completed tasks/changes]\n\n### In Progress\n- [ ] [Current "
    "work]\n\n### Blocked\n- [Issues preventing progress, if any]\n\n## Key "
    "Decisions\n- **[Decision]**: [Brief rationale]\n\n## Next Steps\n1. "
    "[Ordered list of what should happen next]\n\n## Critical Context\n- [Any "
    "data, examples, or references needed to continue]\n- [Or \"(none)\" if "
    "not applicable]\n\nKeep each section concise. Preserve exact file paths, "
    "function names, and error messages.";
static size_t units(pangu_slice text) {
  size_t result = 0, pos = 0;
  while (pos < text.len) {
    utf8proc_int32_t cp;
    size_t remaining = text.len - pos;
    utf8proc_ssize_t n = utf8proc_iterate(
        (const utf8proc_uint8_t *)text.data + pos,
        (utf8proc_ssize_t)(remaining > PTRDIFF_MAX ? PTRDIFF_MAX : remaining),
        &cp);
    size_t add = n > 0 && cp > 0xffff ? 2 : 1;
    result = add > SIZE_MAX - result ? SIZE_MAX : result + add;
    pos += n > 0 ? (size_t)n : 1;
  }
  return result;
}
static size_t plus(size_t a, size_t b) {
  return b > SIZE_MAX - a ? SIZE_MAX : a + b;
}
/* Pi estimate.ts uses UTF-16 length / 3.5, rounded upward. */
static size_t tokens(size_t chars) {
  return (chars / 7) * 2 + ((chars % 7) * 2 + 6) / 7;
}
static size_t tool_tokens(const pangu_message *m, int removed) {
  size_t count = removed ? m->tools_removed_count : m->tools_added_count;
  if (!count)
    return 0;
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return SIZE_MAX;
  yyjson_mut_val *array = yyjson_mut_arr(d);
  int ok = array != NULL;
  if (ok)
    yyjson_mut_doc_set_root(d, array);
  for (size_t i = 0; i < count && ok; i++) {
    if (removed) {
      pangu_slice name = m->tools_removed[i];
      ok = yyjson_mut_arr_add_strncpy(d, array, name.data ? name.data : "",
                                      name.len);
      continue;
    }
    const pangu_tool_declaration *t = &m->tools_added[i];
    yyjson_mut_val *v = yyjson_mut_obj(d);
    yyjson_doc *schema =
        yyjson_read(t->schema_json.data, t->schema_json.len, 0);
    ok = v && schema &&
         yyjson_mut_obj_add_strncpy(
             d, v, "name", t->name.data ? t->name.data : "", t->name.len) &&
         yyjson_mut_obj_add_strncpy(d, v, "description",
                                    t->description.data ? t->description.data
                                                        : "",
                                    t->description.len) &&
         yyjson_mut_obj_add_val(
             d, v, "parameters",
             yyjson_val_mut_copy(d, yyjson_doc_get_root(schema)));
    if (ok && t->has_constrained_sampling)
      ok = yyjson_mut_obj_add_bool(d, v, "constrainedSampling",
                                   t->constrained_sampling);
    if (ok)
      ok = yyjson_mut_arr_add_val(array, v);
    yyjson_doc_free(schema);
  }
  size_t len = 0;
  char *text = ok ? yyjson_mut_write(d, 0, &len) : NULL;
  size_t result = text ? tokens(units((pangu_slice){text, len})) : SIZE_MAX;
  free(text);
  yyjson_mut_doc_free(d);
  return result;
}
static size_t estimate(const pangu_message *m) {
  size_t chars = 0, text_count = 0;
  for (size_t i = 0; i < m->content_count; i++) {
    const pangu_content *c = &m->content[i];
    size_t add = c->kind == PANGU_IMAGE ? 4800 : units(c->data);
    if (m->role == PANGU_SYSTEM && c->kind != PANGU_TEXT)
      continue;
    if (c->kind == PANGU_TOOL_CALL)
      add = plus(add, units(c->name));
    chars = plus(chars, add);
    if (m->role == PANGU_SYSTEM)
      text_count++;
  }
  if (m->role == PANGU_SYSTEM) {
    if (text_count > 1)
      chars = plus(chars, text_count - 1);
    for (size_t i = 0; i < m->section_count; i++) {
      const pangu_system_section *section = &m->sections[i];
      if (section->removed || !section->value.len)
        continue;
      if (chars)
        chars = plus(chars, 2);
      chars = plus(chars, units(section->value));
    }
    return plus(tokens(chars), plus(tool_tokens(m, 0), tool_tokens(m, 1)));
  }
  return tokens(chars);
}
static int candidate(const pangu_core *c, size_t index) {
  const pangu_message *m = pangu_core_message_at(c, index);
  if (m->role == PANGU_ASSISTANT)
    return 1;
  if (m->role != PANGU_USER)
    return 0;
  size_t prev = index;
  while (prev && pangu_core_message_at(c, prev - 1)->role != PANGU_ASSISTANT)
    prev--;
  const pangu_message *a = prev ? pangu_core_message_at(c, prev - 1) : NULL;
  for (size_t j = index + 1;
       a && j < pangu_core_message_count(c) &&
       pangu_core_message_at(c, j)->role != PANGU_ASSISTANT;
       j++) {
    const pangu_message *r = pangu_core_message_at(c, j);
    if (r->role != PANGU_TOOL_RESULT)
      continue;
    for (size_t k = 0; k < a->content_count; k++)
      if (a->content[k].kind == PANGU_TOOL_CALL &&
          a->content[k].id.len == r->tool_call_id.len &&
          !memcmp(a->content[k].id.data, r->tool_call_id.data,
                  r->tool_call_id.len))
        return 0;
  }
  return 1;
}
static size_t select_cut(const pangu_core *c, const uint64_t *ids,
                         const pg_tree *tree, size_t keep) {
  size_t total = pangu_core_message_count(c), start = 0;
  while (start < total && pangu_core_message_at(c, start)->role == PANGU_SYSTEM)
    start++;
  if (start < total && ids[start] &&
      tree->nodes[ids[start] - 1].kind == PG_ENTRY_COMPACTION)
    start++;
  size_t tokens = 0, cut = total;
  for (size_t i = total; i > start; i--) {
    size_t add = estimate(pangu_core_message_at(c, i - 1));
    tokens = add > SIZE_MAX - tokens ? SIZE_MAX : tokens + add;
    if (tokens < keep)
      continue;
    for (size_t j = i - 1; j < total; j++)
      if (candidate(c, j) && ids[j]) {
        cut = j;
        break;
      }
    if (cut == total) {
      for (size_t j = total; j > start; j--)
        if (candidate(c, j - 1) && ids[j - 1]) {
          cut = j - 1;
          break;
        }
    }
    break;
  }
  if (cut == total)
    return total;
  for (size_t i = 0; i < cut; i++)
    if (pangu_core_message_at(c, i)->role != PANGU_SYSTEM)
      return cut;
  return total;
}
static int serialize(text_buffer *b, const pangu_core *c, size_t cut) {
  for (size_t i = 0; i < cut; i++) {
    const pangu_message *m = pangu_core_message_at(c, i);
    if (m->role == PANGU_SYSTEM || m->role == PANGU_CUSTOM)
      continue;
    const char *label = m->role == PANGU_USER        ? "[User]: "
                        : m->role == PANGU_ASSISTANT ? "[Assistant]: "
                                                     : "[Tool result]: ";
    int labelled = 0;
    size_t tool_chars = 0;
    for (size_t j = 0; j < m->content_count; j++) {
      const pangu_content *v = &m->content[j];
      if (v->kind != PANGU_TEXT && v->kind != PANGU_TOOL_CALL)
        continue;
      if (!labelled) {
        if (!write_text(b, label, strlen(label)))
          return 0;
        labelled = 1;
      }
      if (v->kind == PANGU_TOOL_CALL) {
        if (!TEXT(b, "\n[Assistant tool calls]: ") ||
            !write_text(b, v->name.data, v->name.len) || !TEXT(b, "(") ||
            !write_text(b, v->data.data, v->data.len) || !TEXT(b, ")"))
          return 0;
      } else {
        size_t n = v->data.len;
        if (m->role == PANGU_TOOL_RESULT) {
          if (tool_chars >= 2000)
            continue;
          if (n > 2000 - tool_chars)
            n = 2000 - tool_chars;
          /* Do not split an encoded UTF-8 scalar at the truncation boundary. */
          while (n && n < v->data.len &&
                 ((unsigned char)v->data.data[n] & 0xc0) == 0x80)
            n--;
          tool_chars += n;
        }
        if (!write_text(b, v->data.data, n) || !TEXT(b, "\n"))
          return 0;
        if (n < v->data.len && !TEXT(b, "[... tool result truncated]\n"))
          return 0;
      }
    }
    if (labelled && !TEXT(b, "\n"))
      return 0;
  }
  return TEXT(b, "\n") &&
         write_text(b, summary_prompt, sizeof(summary_prompt) - 1);
}
static pangu_turn_decision end_summary(void *ctx, pangu_core *core,
                                       const pangu_message *m) {
  (void)ctx;
  (void)core;
  (void)m;
  return PANGU_TURN_END_RUN;
}
pg_session_status pg_session_compact_internal(pangu_session *s, size_t keep,
                                              const pangu_loop_config *cfg) {
  if (!s || s->closed || !cfg)
    return PG_SESSION_INVALID;
  if (s->running_flag || s->dispatch || s->draft)
    return PG_SESSION_BUSY;
  if (s->readonly)
    return PG_SESSION_READ_ONLY;
  pangu_core *view = NULL;
  uint64_t *ids = NULL;
  pg_session_status status = pg_tree_project(&s->tree, &view, &ids);
  if (status != PG_SESSION_OK)
    return status;
  size_t cut = select_cut(view, ids, &s->tree, keep);
  if (cut == pangu_core_message_count(view)) {
    free(ids);
    pangu_core_destroy(view);
    return PG_SESSION_OK;
  }
  uint64_t first_kept = ids[cut];
  free(ids);
  text_buffer prompt = {0};
  if (!serialize(&prompt, view, cut)) {
    pangu_core_destroy(view);
    free(prompt.data);
    return PG_SESSION_NOMEM;
  }
  pangu_core_destroy(view);
  pangu_core *summary = NULL;
  if (pangu_core_create(NULL, &summary) != PANGU_OK) {
    free(prompt.data);
    return PG_SESSION_NOMEM;
  }
  pangu_content system = {.kind = PANGU_TEXT,
                          .data = {system_prompt, sizeof(system_prompt) - 1}},
                user = {.kind = PANGU_TEXT, .data = {prompt.data, prompt.len}};
  pangu_message system_message = {.role = PANGU_SYSTEM,
                                  .content = &system,
                                  .content_count = 1},
                question = {
                    .role = PANGU_USER, .content = &user, .content_count = 1};
  if (pangu_core_append(summary, &system_message) != PANGU_OK) {
    pangu_core_destroy(summary);
    free(prompt.data);
    return PG_SESSION_NOMEM;
  }
  yyjson_mut_doc *options = yyjson_mut_doc_new(NULL);
  yyjson_doc *original = cfg->request_options_json.len
                             ? yyjson_read(cfg->request_options_json.data,
                                           cfg->request_options_json.len, 0)
                             : NULL;
  if (!options ||
      (cfg->request_options_json.len &&
       (!original || !yyjson_is_obj(yyjson_doc_get_root(original))))) {
    pg_session_status error = options ? PG_SESSION_INVALID : PG_SESSION_NOMEM;
    yyjson_mut_doc_free(options);
    yyjson_doc_free(original);
    pangu_core_destroy(summary);
    free(prompt.data);
    return error;
  }
  yyjson_mut_val *root =
      original ? yyjson_val_mut_copy(options, yyjson_doc_get_root(original))
               : yyjson_mut_obj(options);
  yyjson_doc_free(original);
  if (!root) {
    yyjson_mut_doc_free(options);
    pangu_core_destroy(summary);
    free(prompt.data);
    return PG_SESSION_NOMEM;
  }
  yyjson_mut_doc_set_root(options, root);
  yyjson_mut_obj_remove_key(root, "tool_choice");
  int added = yyjson_mut_obj_add_str(options, root, "tool_choice", "none");
  size_t option_len = 0;
  char *encoded = added ? yyjson_mut_write(options, 0, &option_len) : NULL;
  yyjson_mut_doc_free(options);
  if (!encoded) {
    pangu_core_destroy(summary);
    free(prompt.data);
    return PG_SESSION_NOMEM;
  }
  pangu_loop_config config = *cfg;
  config.request_options_json = (pangu_slice){encoded, option_len};
  /* Same provider/auth/context adapters, but no executable tools, hooks, queue
   * mutation or session event/save callbacks in the summarization operation. */
  config.event = NULL;
  config.tools = NULL;
  config.tool_count = 0;
  config.before_tool = NULL;
  config.after_tool = NULL;
  config.before_tool_context = NULL;
  config.after_tool_patch = NULL;
  config.prepare_request = NULL;
  config.prepare_request_update = NULL;
  config.prepare_next_turn = NULL;
  config.prepare_next_turn_update = NULL;
  config.finish_turn = end_summary;
  config.finish_turn_context = NULL;
  s->compaction_core = summary;
  s->running_flag = 1;
  pangu_status code = pangu_core_run(summary, &question, 1, &config);
  s->running_flag = 0;
  s->compaction_core = NULL;
  free(prompt.data);
  free(encoded);
  if (code != PANGU_OK) {
    pangu_core_destroy(summary);
    return code == PANGU_CANCELLED ? PG_SESSION_CANCELLED
           : code == PANGU_NOMEM   ? PG_SESSION_NOMEM
                                   : PG_SESSION_CORE_ERROR;
  }
  size_t count = pangu_core_message_count(summary);
  const pangu_message *answer =
      count ? pangu_core_message_at(summary, count - 1) : NULL;
  if (!answer || answer->role != PANGU_ASSISTANT ||
      answer->stop_reason != PANGU_STOP) {
    status = answer && answer->stop_reason == PANGU_ABORTED
                 ? PG_SESSION_CANCELLED
                 : PG_SESSION_CORE_ERROR;
    pangu_core_destroy(summary);
    return status;
  }
  text_buffer text = {0};
  for (size_t i = 0; i < answer->content_count; i++) {
    const pangu_content *c = &answer->content[i];
    if (c->kind == PANGU_TOOL_CALL) {
      free(text.data);
      pangu_core_destroy(summary);
      return PG_SESSION_CORE_ERROR;
    }
    if (c->kind == PANGU_TEXT) {
      if (text.len && !TEXT(&text, "\n")) {
        status = PG_SESSION_NOMEM;
        break;
      }
      if (!write_text(&text, c->data.data, c->data.len)) {
        status = PG_SESSION_NOMEM;
        break;
      }
    }
  }
  size_t start = 0, end = text.len;
  while (start < end && isspace((unsigned char)text.data[start]))
    start++;
  while (end > start && isspace((unsigned char)text.data[end - 1]))
    end--;
  if (s->cancel_requested && s->operation)
    status = PG_SESSION_CANCELLED;
  char *suffix = NULL, *details = NULL;
  size_t suffix_len = 0, details_len = 0;
  if (status == PG_SESSION_OK && end == start)
    status = PG_SESSION_CORE_ERROR;
  if (status == PG_SESSION_OK) {
    pangu_core *files_view = NULL;
    uint64_t *files_ids = NULL;
    status = pg_tree_project(&s->tree, &files_view, &files_ids);
    if (status == PG_SESSION_OK) {
      size_t files_cut = 0, total = pangu_core_message_count(files_view);
      while (files_cut < total && files_ids[files_cut] != first_kept)
        files_cut++;
      status = files_cut == total
                   ? PG_SESSION_INVALID
                   : pg_compaction_file_metadata(
                         &s->tree, files_view, files_ids, files_cut, &suffix,
                         &suffix_len, &details, &details_len);
    }
    free(files_ids);
    pangu_core_destroy(files_view);
  }
  if (status == PG_SESSION_OK) {
    text.len = end;
    if (!write_text(&text, suffix, suffix_len))
      status = PG_SESSION_NOMEM;
    else
      status = pg_session_compact_summary_internal(
          s, first_kept, (pangu_slice){text.data + start, text.len - start},
          &answer->usage, (pangu_slice){details, details_len});
  }
  free(suffix);
  free(details);
  free(text.data);
  pangu_core_destroy(summary);
  return status;
}
pg_session_status pg_session_compact(pangu_session *s, size_t keep,
                                     const pangu_loop_config *cfg) {
  if (s && s->operation)
    return PG_SESSION_BUSY;
  return pg_session_compact_internal(s, keep, cfg);
}
size_t pg_session_context_tokens(const pangu_session *s) {
  if (!s || s->closed)
    return 0;
  pangu_core *view = NULL;
  uint64_t *ids = NULL;
  if (pg_tree_project(&s->tree, &view, &ids) != PG_SESSION_OK)
    return SIZE_MAX;
  size_t count = pangu_core_message_count(view), from = 0, total = 0;
  uint64_t marker = 0;
  for (size_t i = 0; i < count; i++)
    if (ids[i] && s->tree.nodes[ids[i] - 1].kind == PG_ENTRY_COMPACTION &&
        ids[i] > marker)
      marker = ids[i];
  for (size_t i = count; i; i--) {
    const pangu_message *m = pangu_core_message_at(view, i - 1);
    if (m->role != PANGU_ASSISTANT || ids[i - 1] <= marker ||
        m->stop_reason == PANGU_ERROR || m->stop_reason == PANGU_ABORTED)
      continue;
    uint64_t tokens = m->usage.total;
    if (!tokens) {
      uint64_t parts[] = {m->usage.input, m->usage.output, m->usage.cache_read,
                          m->usage.cache_write};
      for (size_t j = 0; j < 4; j++)
        tokens =
            parts[j] > UINT64_MAX - tokens ? UINT64_MAX : tokens + parts[j];
    }
    if (tokens) {
      total = tokens > SIZE_MAX ? SIZE_MAX : (size_t)tokens;
      from = i;
      break;
    }
  }
  for (size_t i = from; i < count; i++) {
    size_t n = estimate(pangu_core_message_at(view, i));
    total = n > SIZE_MAX - total ? SIZE_MAX : total + n;
  }
  free(ids);
  pangu_core_destroy(view);
  return total;
}
pg_compaction_policy pg_session_compaction_policy(const pangu_session *s) {
  return s && !s->closed ? s->policy : (pg_compaction_policy){0};
}
pg_session_status
pg_session_set_compaction_policy(pangu_session *s,
                                 const pg_compaction_policy *policy) {
  if (!s || s->closed || !policy ||
      (policy->enabled && (!policy->context_tokens ||
                           policy->reserve_tokens >= policy->context_tokens)))
    return PG_SESSION_INVALID;
  if (s->dispatch || s->running_flag || s->operation || s->draft)
    return PG_SESSION_BUSY;
  if (s->readonly)
    return PG_SESSION_READ_ONLY;
  pg_session_status z = pg_session_begin_internal(s);
  if (z != PG_SESSION_OK)
    return z;
  s->draft_policy = *policy;
  z = pg_session_commit_internal(s);
  if (z != PG_SESSION_OK)
    pg_session_rollback_internal(s);
  return z;
}
