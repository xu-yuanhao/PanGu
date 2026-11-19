/* Native adaptation of Pi message unions; Pi MIT license:
 * third-party/Pi-LICENSE. */
#include "internal.h"
#include <stdlib.h>
#include <string.h>
static pangu_slice slice(yyjson_val *v) {
  return (pangu_slice){yyjson_get_str(v), yyjson_get_len(v)};
}
static int str(yyjson_mut_doc *d, yyjson_mut_val *o, const char *k,
               pangu_slice v) {
  return yyjson_mut_obj_add_strncpy(d, o, k, v.data ? v.data : "", v.len);
}
/* NULL is absent; a non-NULL zero-length slice is explicitly empty. */
static int optional_str(yyjson_mut_doc *d, yyjson_mut_val *o,
                        const char *k, pangu_slice v) {
  return !v.data || str(d, o, k, v);
}
static pangu_slice optional_slice(yyjson_val *v, int legacy) {
  return legacy && !yyjson_get_len(v) ? (pangu_slice){0} : slice(v);
}
yyjson_mut_val *pg_encode_message(yyjson_mut_doc *d, const pangu_message *m) {
  yyjson_mut_val *o = yyjson_mut_obj(d), *a = yyjson_mut_arr(d),
                 *u = yyjson_mut_obj(d);
  if (!o || !a || !u)
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, o, "presence", m->presence))
    return NULL;
  if (!str(d, o, "custom_role", m->custom_role))
    return NULL;
  if (!str(d, o, "tool_call_id", m->tool_call_id))
    return NULL;
  if (!str(d, o, "tool_name", m->tool_name))
    return NULL;
  if (!str(d, o, "model", m->model))
    return NULL;
  if (!str(d, o, "provider", m->provider))
    return NULL;
  if (!str(d, o, "api", m->api))
    return NULL;
  if (!optional_str(d, o, "error", m->error))
    return NULL;
  if (!str(d, o, "details_json", m->details_json))
    return NULL;
  if (!str(d, o, "structured_json", m->structured_json))
    return NULL;
  if (!optional_str(d, o, "provider_thinking_level", m->provider_thinking_level))
    return NULL;
  if ((m->response_model.data &&
       !str(d, o, "response_model", m->response_model)) ||
      (m->response_id.data && !str(d, o, "response_id", m->response_id)) ||
      (m->raw_stop_reason.data &&
       !str(d, o, "raw_stop_reason", m->raw_stop_reason)))
    return NULL;
  if ((m->diagnostics_json.len &&
       !str(d, o, "diagnostics_json", m->diagnostics_json)) ||
      (m->deferred_json.len &&
       !str(d, o, "deferred_json", m->deferred_json)) ||
      (m->nested_calls_json.len &&
       !str(d, o, "nested_calls_json", m->nested_calls_json)) ||
      (m->has_end_turn &&
       !yyjson_mut_obj_add_bool(d, o, "end_turn", m->end_turn)))
    return NULL;
  if (!yyjson_mut_obj_add_sint(d, o, "role", (int64_t)m->role))
    return NULL;
  if (!yyjson_mut_obj_add_sint(d, o, "stop_reason", (int64_t)m->stop_reason))
    return NULL;
  if (!yyjson_mut_obj_add_sint(d, o, "thinking_level",
                               (int64_t)m->thinking_level))
    return NULL;
  if (!yyjson_mut_obj_add_sint(d, o, "is_error", (int64_t)m->is_error))
    return NULL;
  if (!yyjson_mut_obj_add_sint(d, o, "terminate", (int64_t)m->terminate))
    return NULL;
  if (!yyjson_mut_obj_add_sint(d, o, "timestamp_ms", (int64_t)m->timestamp_ms))
    return NULL;
  if (!yyjson_mut_obj_add_sint(d, o, "has_duration", (int64_t)m->has_duration))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, o, "duration_ms", m->duration_ms))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, u, "input", m->usage.input))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, u, "output", m->usage.output))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, u, "cache_read", m->usage.cache_read))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, u, "cache_write", m->usage.cache_write))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, u, "total", m->usage.total))
    return NULL;
  if (!yyjson_mut_obj_add_real(d, u, "input_cost", m->usage.input_cost))
    return NULL;
  if (!yyjson_mut_obj_add_real(d, u, "output_cost", m->usage.output_cost))
    return NULL;
  if (!yyjson_mut_obj_add_real(d, u, "cache_read_cost",
                               m->usage.cache_read_cost))
    return NULL;
  if (!yyjson_mut_obj_add_real(d, u, "cache_write_cost",
                               m->usage.cache_write_cost))
    return NULL;
  if (!yyjson_mut_obj_add_real(d, u, "total_cost", m->usage.total_cost))
    return NULL;
  if (!yyjson_mut_obj_add_bool(d, u, "has_cache_write_1h",
                               m->usage.has_cache_write_1h))
    return NULL;
  if (!yyjson_mut_obj_add_bool(d, u, "has_reasoning", m->usage.has_reasoning))
    return NULL;
  if (!yyjson_mut_obj_add_bool(d, u, "has_total_from_provider",
                               m->usage.has_total_from_provider))
    return NULL;
  if (!yyjson_mut_obj_add_bool(d, u, "total_from_provider",
                               m->usage.total_from_provider))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, u, "cache_write_1h", m->usage.cache_write_1h))
    return NULL;
  if (!yyjson_mut_obj_add_uint(d, u, "reasoning", m->usage.reasoning))
    return NULL;
  if (!yyjson_mut_obj_add_val(d, o, "usage", u))
    return NULL;
  for (size_t i = 0; i < m->content_count; i++) {
    const pangu_content *c = &m->content[i];
    yyjson_mut_val *v = yyjson_mut_obj(d);
    if (!v || !yyjson_mut_obj_add_uint(d, v, "kind", c->kind))
      return NULL;
    if (!str(d, v, "data", c->data))
      return NULL;
    if (!str(d, v, "mime_type", c->mime_type))
      return NULL;
    if (!str(d, v, "id", c->id))
      return NULL;
    if (!str(d, v, "name", c->name))
      return NULL;
    if (!optional_str(d, v, "signature", c->signature))
      return NULL;
    if (!optional_str(d, v, "tool_namespace", c->tool_namespace))
      return NULL;
    if (!yyjson_mut_obj_add_bool(d, v, "has_redacted", c->has_redacted) ||
        !yyjson_mut_obj_add_bool(d, v, "redacted", c->redacted))
      return NULL;
    if (!optional_str(d, v, "arguments_error", c->arguments_error))
      return NULL;
    if (!yyjson_mut_arr_add_val(a, v))
      return NULL;
  }
  if (!yyjson_mut_obj_add_val(d, o, "content", a))
    return NULL;
  a = yyjson_mut_arr(d);
  if (!a)
    return NULL;
  for (size_t i = 0; i < m->section_count; i++) {
    const pangu_system_section *v = &m->sections[i];
    yyjson_mut_val *x = yyjson_mut_obj(d);
    if (!x || !str(d, x, "name", v->name) || !str(d, x, "value", v->value) ||
        !yyjson_mut_obj_add_bool(d, x, "removed", v->removed) ||
        !yyjson_mut_arr_add_val(a, x))
      return NULL;
  }
  if (!yyjson_mut_obj_add_val(d, o, "sections", a))
    return NULL;
  a = yyjson_mut_arr(d);
  if (!a)
    return NULL;
  for (size_t i = 0; i < m->tools_added_count; i++) {
    const pangu_tool_declaration *v = &m->tools_added[i];
    yyjson_mut_val *x = yyjson_mut_obj(d);
    if (!x || !str(d, x, "name", v->name) ||
        !str(d, x, "description", v->description) ||
        !str(d, x, "schema_json", v->schema_json) ||
        !yyjson_mut_obj_add_bool(d, x, "has_constrained_sampling",
                                 v->has_constrained_sampling) ||
        !yyjson_mut_obj_add_bool(d, x, "constrained_sampling",
                                 v->constrained_sampling) ||
        !yyjson_mut_arr_add_val(a, x))
      return NULL;
  }
  if (!yyjson_mut_obj_add_val(d, o, "tools_added", a))
    return NULL;
  a = yyjson_mut_arr(d);
  if (!a)
    return NULL;
  for (size_t i = 0; i < m->tools_removed_count; i++) {
    pangu_slice v = m->tools_removed[i];
    if (!yyjson_mut_arr_add_strncpy(d, a, v.data ? v.data : "", v.len))
      return NULL;
  }
  if (!yyjson_mut_obj_add_val(d, o, "tools_removed", a))
    return NULL;
  return o;
}
pg_session_status pg_decode_message(yyjson_val *o, pangu_core *core) {
  pangu_message m = {0};
  pg_session_status status = PG_SESSION_CORRUPT;
  pangu_content *c = NULL;
  pangu_system_section *sections = NULL;
  pangu_tool_declaration *tools = NULL;
  pangu_slice *removed = NULL;
  yyjson_val *v, *a, *u;
  size_t i, max;
  int legacy;
  if (!yyjson_is_obj(o))
    goto done;
  v = yyjson_obj_get(o, "presence");
  legacy = v == NULL;
  if (!legacy && (!yyjson_is_uint(v) || yyjson_get_uint(v) > UINT32_MAX))
    goto done;
  m.presence = (uint32_t)yyjson_get_uint(v);
  v = yyjson_obj_get(o, "custom_role");
  if (!yyjson_is_str(v))
    goto done;
  m.custom_role = slice(v);
  v = yyjson_obj_get(o, "tool_call_id");
  if (!yyjson_is_str(v))
    goto done;
  m.tool_call_id = slice(v);
  v = yyjson_obj_get(o, "tool_name");
  if (!yyjson_is_str(v))
    goto done;
  m.tool_name = slice(v);
  v = yyjson_obj_get(o, "model");
  if (!yyjson_is_str(v))
    goto done;
  m.model = slice(v);
  v = yyjson_obj_get(o, "provider");
  if (!yyjson_is_str(v))
    goto done;
  m.provider = slice(v);
  v = yyjson_obj_get(o, "api");
  if (!yyjson_is_str(v))
    goto done;
  m.api = slice(v);
  v = yyjson_obj_get(o, "error");
  if (v && !yyjson_is_str(v))
    goto done;
  m.error = optional_slice(v, legacy);
  v = yyjson_obj_get(o, "details_json");
  if (!yyjson_is_str(v))
    goto done;
  m.details_json = slice(v);
  v = yyjson_obj_get(o, "structured_json");
  if (!yyjson_is_str(v))
    goto done;
  m.structured_json = slice(v);
  v = yyjson_obj_get(o, "provider_thinking_level");
  if (v && !yyjson_is_str(v))
    goto done;
  m.provider_thinking_level = optional_slice(v, legacy);
  v = yyjson_obj_get(o, "response_model");
  if (v && !yyjson_is_str(v))
    goto done;
  m.response_model = optional_slice(v, legacy);
  v = yyjson_obj_get(o, "response_id");
  if (v && !yyjson_is_str(v))
    goto done;
  m.response_id = optional_slice(v, legacy);
  v = yyjson_obj_get(o, "raw_stop_reason");
  if (v && !yyjson_is_str(v))
    goto done;
  m.raw_stop_reason = optional_slice(v, legacy);
  v = yyjson_obj_get(o, "diagnostics_json");
  if (v && !yyjson_is_str(v))
    goto done;
  m.diagnostics_json = slice(v);
  v = yyjson_obj_get(o, "deferred_json");
  if (v && !yyjson_is_str(v))
    goto done;
  m.deferred_json = slice(v);
  v = yyjson_obj_get(o, "nested_calls_json");
  if (v && !yyjson_is_str(v))
    goto done;
  m.nested_calls_json = slice(v);
  v = yyjson_obj_get(o, "end_turn");
  if (v && !yyjson_is_bool(v))
    goto done;
  m.has_end_turn = v != NULL;
  m.end_turn = yyjson_get_bool(v);
  v = yyjson_obj_get(o, "role");
  if (!yyjson_is_int(v))
    goto done;
  m.role = yyjson_get_sint(v);
  v = yyjson_obj_get(o, "stop_reason");
  if (!yyjson_is_int(v))
    goto done;
  m.stop_reason = yyjson_get_sint(v);
  v = yyjson_obj_get(o, "thinking_level");
  if (!yyjson_is_int(v))
    goto done;
  m.thinking_level = yyjson_get_sint(v);
  v = yyjson_obj_get(o, "is_error");
  if (!yyjson_is_int(v))
    goto done;
  m.is_error = yyjson_get_sint(v);
  v = yyjson_obj_get(o, "terminate");
  if (!yyjson_is_int(v))
    goto done;
  m.terminate = yyjson_get_sint(v);
  v = yyjson_obj_get(o, "timestamp_ms");
  if (!yyjson_is_int(v))
    goto done;
  m.timestamp_ms = yyjson_get_sint(v);
  v = yyjson_obj_get(o, "has_duration");
  if (!yyjson_is_int(v))
    goto done;
  m.has_duration = yyjson_get_sint(v);
  v = yyjson_obj_get(o, "duration_ms");
  if (!yyjson_is_uint(v))
    goto done;
  m.duration_ms = yyjson_get_uint(v);
  if (m.role > PANGU_CUSTOM || m.stop_reason > PANGU_DEFERRED ||
      m.thinking_level > PANGU_THINKING_MAX)
    goto done;
  u = yyjson_obj_get(o, "usage");
  if (!yyjson_is_obj(u))
    goto done;
  v = yyjson_obj_get(u, "input");
  if (!yyjson_is_uint(v))
    goto done;
  m.usage.input = yyjson_get_uint(v);
  v = yyjson_obj_get(u, "output");
  if (!yyjson_is_uint(v))
    goto done;
  m.usage.output = yyjson_get_uint(v);
  v = yyjson_obj_get(u, "cache_read");
  if (!yyjson_is_uint(v))
    goto done;
  m.usage.cache_read = yyjson_get_uint(v);
  v = yyjson_obj_get(u, "cache_write");
  if (!yyjson_is_uint(v))
    goto done;
  m.usage.cache_write = yyjson_get_uint(v);
  v = yyjson_obj_get(u, "total");
  if (!yyjson_is_uint(v))
    goto done;
  m.usage.total = yyjson_get_uint(v);
  v = yyjson_obj_get(u, "input_cost");
  if (!yyjson_is_num(v))
    goto done;
  m.usage.input_cost = yyjson_get_num(v);
  v = yyjson_obj_get(u, "output_cost");
  if (!yyjson_is_num(v))
    goto done;
  m.usage.output_cost = yyjson_get_num(v);
  v = yyjson_obj_get(u, "cache_read_cost");
  if (!yyjson_is_num(v))
    goto done;
  m.usage.cache_read_cost = yyjson_get_num(v);
  v = yyjson_obj_get(u, "cache_write_cost");
  if (!yyjson_is_num(v))
    goto done;
  m.usage.cache_write_cost = yyjson_get_num(v);
  v = yyjson_obj_get(u, "total_cost");
  if (!yyjson_is_num(v))
    goto done;
  m.usage.total_cost = yyjson_get_num(v);
  v = yyjson_obj_get(u, "has_cache_write_1h");
  if (v && !yyjson_is_bool(v))
    goto done;
  m.usage.has_cache_write_1h = yyjson_get_bool(v);
  v = yyjson_obj_get(u, "has_reasoning");
  if (v && !yyjson_is_bool(v))
    goto done;
  m.usage.has_reasoning = yyjson_get_bool(v);
  v = yyjson_obj_get(u, "has_total_from_provider");
  if (v && !yyjson_is_bool(v))
    goto done;
  m.usage.has_total_from_provider = yyjson_get_bool(v);
  v = yyjson_obj_get(u, "total_from_provider");
  if (v && !yyjson_is_bool(v))
    goto done;
  m.usage.total_from_provider = yyjson_get_bool(v);
  v = yyjson_obj_get(u, "cache_write_1h");
  if (v && !yyjson_is_uint(v))
    goto done;
  m.usage.cache_write_1h = yyjson_get_uint(v);
  v = yyjson_obj_get(u, "reasoning");
  if (v && !yyjson_is_uint(v))
    goto done;
  m.usage.reasoning = yyjson_get_uint(v);
  a = yyjson_obj_get(o, "content");
  if (!yyjson_is_arr(a))
    goto done;
  m.content_count = yyjson_arr_size(a);
  if (m.content_count && !(c = calloc(m.content_count, sizeof(*c)))) {
    status = PG_SESSION_NOMEM;
    goto done;
  }
  m.content = c;
  yyjson_arr_foreach(a, i, max, v) {
    yyjson_val *x = yyjson_obj_get(v, "kind");
    if (!yyjson_is_uint(x) || yyjson_get_uint(x) > PANGU_JSON)
      goto done;
    c[i].kind = yyjson_get_uint(x);
    x = yyjson_obj_get(v, "data");
    if (!yyjson_is_str(x))
      goto done;
    c[i].data = slice(x);
    x = yyjson_obj_get(v, "mime_type");
    if (!yyjson_is_str(x))
      goto done;
    c[i].mime_type = slice(x);
    x = yyjson_obj_get(v, "id");
    if (!yyjson_is_str(x))
      goto done;
    c[i].id = slice(x);
    x = yyjson_obj_get(v, "name");
    if (!yyjson_is_str(x))
      goto done;
    c[i].name = slice(x);
    x = yyjson_obj_get(v, "signature");
    if (x && !yyjson_is_str(x))
      goto done;
    c[i].signature = optional_slice(x, legacy);
    x = yyjson_obj_get(v, "tool_namespace");
    if (x && !yyjson_is_str(x))
      goto done;
    c[i].tool_namespace = optional_slice(x, legacy);
    x = yyjson_obj_get(v, "arguments_error");
    if (x && !yyjson_is_str(x))
      goto done;
    c[i].arguments_error = optional_slice(x, legacy);
    x = yyjson_obj_get(v, "has_redacted");
    if (x && !yyjson_is_bool(x))
      goto done;
    c[i].has_redacted = yyjson_get_bool(x);
    x = yyjson_obj_get(v, "redacted");
    if (x && !yyjson_is_bool(x))
      goto done;
    c[i].redacted = yyjson_get_bool(x);
  }
  a = yyjson_obj_get(o, "sections");
  if (!yyjson_is_arr(a))
    goto done;
  m.section_count = yyjson_arr_size(a);
  if (m.section_count &&
      !(sections = calloc(m.section_count, sizeof(*sections)))) {
    status = PG_SESSION_NOMEM;
    goto done;
  }
  m.sections = sections;
  yyjson_arr_foreach(a, i, max, v) {
    yyjson_val *n = yyjson_obj_get(v, "name"), *x = yyjson_obj_get(v, "value"),
               *r = yyjson_obj_get(v, "removed");
    if (!yyjson_is_str(n) || !yyjson_is_str(x) || !yyjson_is_bool(r))
      goto done;
    sections[i] =
        (pangu_system_section){slice(n), slice(x), yyjson_get_bool(r)};
  }
  a = yyjson_obj_get(o, "tools_added");
  if (!yyjson_is_arr(a))
    goto done;
  m.tools_added_count = yyjson_arr_size(a);
  if (m.tools_added_count &&
      !(tools = calloc(m.tools_added_count, sizeof(*tools)))) {
    status = PG_SESSION_NOMEM;
    goto done;
  }
  m.tools_added = tools;
  yyjson_arr_foreach(a, i, max, v) {
    yyjson_val *n = yyjson_obj_get(v, "name"),
               *x = yyjson_obj_get(v, "description"),
               *j = yyjson_obj_get(v, "schema_json");
    yyjson_val *h = yyjson_obj_get(v, "has_constrained_sampling"),
               *b = yyjson_obj_get(v, "constrained_sampling");
    if (!yyjson_is_str(n) || !yyjson_is_str(x) || !yyjson_is_str(j) ||
        !yyjson_is_bool(h) || !yyjson_is_bool(b))
      goto done;
    tools[i] = (pangu_tool_declaration){slice(n), slice(x), slice(j),
                                        yyjson_get_bool(h), yyjson_get_bool(b)};
  }
  a = yyjson_obj_get(o, "tools_removed");
  if (!yyjson_is_arr(a))
    goto done;
  m.tools_removed_count = yyjson_arr_size(a);
  if (m.tools_removed_count &&
      !(removed = calloc(m.tools_removed_count, sizeof(*removed)))) {
    status = PG_SESSION_NOMEM;
    goto done;
  }
  m.tools_removed = removed;
  yyjson_arr_foreach(a, i, max, v) {
    if (!yyjson_is_str(v))
      goto done;
    removed[i] = slice(v);
  }
  /* Pre-marker writers projected these zero-valued fields unconditionally. */
  if (legacy) {
    if (m.role == PANGU_ASSISTANT)
      m.presence |= PANGU_MESSAGE_HAS_THINKING_LEVEL;
    if (m.role == PANGU_TOOL_RESULT)
      m.presence |= PANGU_MESSAGE_HAS_USAGE;
  }
  {
    pangu_status r = pangu_core_append(core, &m);
    status = r == PANGU_OK      ? PG_SESSION_OK
             : r == PANGU_NOMEM ? PG_SESSION_NOMEM
                                : PG_SESSION_CORRUPT;
  }
done:
  free(c);
  free(sections);
  free(tools);
  free(removed);
  return status;
}
