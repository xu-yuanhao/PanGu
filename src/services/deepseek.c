#include <curl/curl.h>
#include <limits.h>
#include <pangu/core/transcript.h>
#include <pangu/services/model.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <yyjson.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
typedef struct {
  char *data;
  size_t len, capacity;
} buffer;
static int append(buffer *b, const char *p, size_t n) {
  if (n > SIZE_MAX - b->len - 1)
    return 0;
  size_t required = b->len + n + 1;
  if (required > b->capacity) {
    size_t capacity = b->capacity ? b->capacity : 128;
    while (capacity < required) {
      if (capacity > SIZE_MAX / 2) {
        capacity = required;
        break;
      }
      capacity *= 2;
    }
    char *q = realloc(b->data, capacity);
    if (!q)
      return 0;
    b->data = q;
    b->capacity = capacity;
  }
  if (n)
    memcpy(b->data + b->len, p, n);
  b->len += n;
  b->data[b->len] = 0;
  return 1;
}
static yyjson_mut_val *str(yyjson_mut_doc *d, pangu_slice s) {
  return yyjson_mut_strncpy(d, s.data ? s.data : "", s.len);
}
static int put(yyjson_mut_doc *d, yyjson_mut_val *o, const char *k,
               yyjson_mut_val *v) {
  return v && yyjson_mut_obj_add_val(d, o, k, v);
}
static int text(yyjson_mut_doc *d, yyjson_mut_val *o, const char *k,
                pangu_slice v) {
  return put(d, o, k, str(d, v));
}
static yyjson_mut_val *json(yyjson_mut_doc *d, pangu_slice s) {
  yyjson_doc *v = yyjson_read(s.data, s.len, 0);
  if (!v)
    return NULL;
  yyjson_mut_val *r = yyjson_val_mut_copy(d, yyjson_doc_get_root(v));
  yyjson_doc_free(v);
  return r;
}
static pangu_slice slice(yyjson_val *v) {
  return (pangu_slice){yyjson_get_str(v), yyjson_get_len(v)};
}
static int is(pangu_slice s, const char *v) {
  return s.data && s.len == strlen(v) && !memcmp(s.data, v, s.len);
}
static pangu_status payload(pangu_core *owner,
                            const pangu_core *request, char **out,
                            size_t *len) {
  *out = NULL;
  const pangu_model_config *m = pangu_core_active_model(owner);
  if (!m || !m->id.len)
    return PANGU_INVALID;
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return PANGU_NOMEM;
  yyjson_mut_val *root = yyjson_mut_obj(d), *messages = yyjson_mut_arr(d);
  int ok = root && messages;
  yyjson_mut_doc_set_root(d, root);
  ok = ok && text(d, root, "model", m->id) &&
       put(d, root, "messages", messages) &&
       yyjson_mut_obj_add_bool(d, root, "stream", true);
  yyjson_mut_val *stream = yyjson_mut_obj(d);
  ok = ok && put(d, root, "stream_options", stream) &&
       yyjson_mut_obj_add_bool(d, stream, "include_usage", true);
  for (size_t i = 0; ok && i < pangu_core_message_count(request); i++) {
    const pangu_message *msg = pangu_core_message_at(request, i);
    if (msg->role == PANGU_CUSTOM)
      continue;
    const char *role = msg->role == PANGU_USER          ? "user"
                       : msg->role == PANGU_ASSISTANT   ? "assistant"
                       : msg->role == PANGU_TOOL_RESULT ? "tool"
                                                        : "system";
    yyjson_mut_val *o = yyjson_mut_obj(d), *calls = yyjson_mut_arr(d);
    buffer content = {0}, thinking = {0};
    size_t nc = 0;
    ok = o && calls && yyjson_mut_obj_add_str(d, o, "role", role);
    for (size_t j = 0; ok && j < msg->content_count; j++) {
      const pangu_content *b = &msg->content[j];
      if (b->kind == PANGU_TEXT)
        ok = append(&content, b->data.data, b->data.len);
      else if (b->kind == PANGU_THINKING)
        ok = append(&thinking, b->data.data, b->data.len);
      else if (b->kind == PANGU_TOOL_CALL) {
        yyjson_mut_val *call = yyjson_mut_obj(d), *fn = yyjson_mut_obj(d);
        ok = call && fn && text(d, call, "id", b->id) &&
             yyjson_mut_obj_add_str(d, call, "type", "function") &&
             put(d, call, "function", fn) && text(d, fn, "name", b->name) &&
             text(d, fn, "arguments", b->data) &&
             yyjson_mut_arr_append(calls, call);
        nc++;
      } else
        ok = 0;
    }
    ok = ok && text(d, o, "content", (pangu_slice){content.data, content.len});
    if (thinking.len)
      ok = ok && text(d, o, "reasoning_content",
                      (pangu_slice){thinking.data, thinking.len});
    if (nc)
      ok = ok && put(d, o, "tool_calls", calls);
    if (msg->role == PANGU_TOOL_RESULT)
      ok = ok && text(d, o, "tool_call_id", msg->tool_call_id);
    ok = ok && yyjson_mut_arr_append(messages, o);
    free(content.data);
    free(thinking.data);
  }
  size_t tool_count = pangu_core_active_tool_count(owner);
  if (tool_count) {
    yyjson_mut_val *tools = yyjson_mut_arr(d);
    ok = ok && put(d, root, "tools", tools);
    for (size_t i = 0; ok && i < tool_count; i++) {
      const pangu_tool *t = pangu_core_active_tool_at(owner, i);
      if (!t) { ok = 0; break; }
      yyjson_mut_val *o = yyjson_mut_obj(d), *fn = yyjson_mut_obj(d);
      ok = o && fn && yyjson_mut_obj_add_str(d, o, "type", "function") &&
           put(d, o, "function", fn) && text(d, fn, "name", t->name) &&
           text(d, fn, "description", t->description) &&
           put(d, fn, "parameters", json(d, t->schema_json)) &&
           yyjson_mut_arr_append(tools, o);
    }
  }
  pangu_slice opts = pangu_core_active_options(owner);
  if (opts.len) {
    yyjson_doc *o = yyjson_read(opts.data, opts.len, 0);
    if (!o || !yyjson_is_obj(yyjson_doc_get_root(o)))
      ok = 0;
    else {
      const char *keys[] = {
          "temperature",      "top_p", "max_tokens", "frequency_penalty",
          "presence_penalty", "seed",  "tool_choice"};
      for (size_t i = 0; i < sizeof(keys) / sizeof(*keys); i++) {
        yyjson_val *v = yyjson_obj_get(yyjson_doc_get_root(o), keys[i]);
        if (v)
          ok = ok && put(d, root, keys[i], yyjson_val_mut_copy(d, v));
      }
    }
    yyjson_doc_free(o);
  }
  yyjson_mut_val *think = yyjson_mut_obj(d);
  ok = ok && put(d, root, "thinking", think) &&
       yyjson_mut_obj_add_str(
           d, think, "type",
           pangu_core_active_thinking(owner) == PANGU_THINKING_OFF ? "disabled"
                                                                   : "enabled");
  char *raw = ok ? yyjson_mut_write(d, 0, len) : NULL;
  yyjson_mut_doc_free(d);
  if (!raw)
    return ok ? PANGU_NOMEM : PANGU_INVALID;
  pangu_core *prepared = NULL;
  pangu_status status =
      pangu_core_prepare_payload(owner, (pangu_slice){raw, *len}, &prepared);
  free(raw);
  if (status != PANGU_OK)
    return status;
  const pangu_message *pm = pangu_core_message_at(prepared, 0);
  pangu_slice data = pm->content[0].data;
  *out = malloc(data.len + 1);
  if (*out) {
    memcpy(*out, data.data, data.len);
    (*out)[data.len] = 0;
    *len = data.len;
  }
  pangu_core_destroy(prepared);
  return *out ? PANGU_OK : PANGU_NOMEM;
}
typedef struct {
  pangu_content value;
  buffer data, id, name;
  size_t tool_index;
  int closed;
} block;
typedef struct {
  pangu_core *owner;
  const pg_deepseek_config *config;
  pangu_model_event_sink sink;
  void *sink_ctx;
  pangu_status status;
  pangu_message message;
  block *blocks;
  size_t count;
  pangu_content *view;
  buffer line, event;
  buffer response_id, response_model, raw_stop_reason;
  int finished, done;
  long http_status;
  pangu_response_header *headers;
  size_t header_count;
} stream_state;
static pangu_status emit(stream_state *s, pangu_assistant_kind kind,
                         size_t index, pangu_slice delta) {
  if (s->status)
    return s->status;
  for (size_t i = 0; i < s->count; i++) {
    block *b = &s->blocks[i];
    b->value.data = (pangu_slice){b->data.data, b->data.len};
    b->value.id = (pangu_slice){b->id.data, b->id.len};
    b->value.name = (pangu_slice){b->name.data, b->name.len};
    s->view[i] = b->value;
  }
  s->message.response_id = (pangu_slice){s->response_id.data,s->response_id.len};
  s->message.response_model = (pangu_slice){s->response_model.data,s->response_model.len};
  s->message.raw_stop_reason = (pangu_slice){s->raw_stop_reason.data,s->raw_stop_reason.len};
  s->message.content = s->view;
  s->message.content_count = s->count;
  pangu_assistant_event e = {.kind = kind,
                             .content_index = index,
                             .delta = delta,
                             .reason = s->message.stop_reason};
  if (index < s->count) {
    if (kind == PANGU_ASSISTANT_TOOL_END)
      e.tool_call = &s->view[index];
    else if (kind == PANGU_ASSISTANT_TEXT_END ||
             kind == PANGU_ASSISTANT_THINKING_END)
      e.content = s->view[index].data;
  }
  s->status = s->sink(s->sink_ctx, &s->message, &e);
  return s->status;
}
static block *new_block(stream_state *s, pangu_content_kind kind,
                        size_t tool_index) {
  if (s->count >= SIZE_MAX / sizeof(block) - 1) {
    s->status = PANGU_NOMEM;
    return NULL;
  }
  block *b = realloc(s->blocks, (s->count + 1) * sizeof(*b));
  if (!b) {
    s->status = PANGU_NOMEM;
    return NULL;
  }
  s->blocks = b;
  pangu_content *v = realloc(s->view, (s->count + 1) * sizeof(*v));
  if (!v) {
    s->status = PANGU_NOMEM;
    return NULL;
  }
  s->view = v;
  b = &s->blocks[s->count++];
  memset(b, 0, sizeof(*b));
  b->value.kind = kind;
  b->tool_index = tool_index;
  if (kind == PANGU_THINKING)
    b->value.signature = SL("reasoning_content");
  return b;
}
static void content_delta(stream_state *s, pangu_content_kind kind,
                          pangu_slice value, size_t ti, pangu_slice id,
                          pangu_slice name) {
  if (s->status || (!value.len && kind != PANGU_TOOL_CALL))
    return;
  size_t i;
  for (i = 0; i < s->count; i++)
    if (s->blocks[i].value.kind == kind &&
        (kind != PANGU_TOOL_CALL || s->blocks[i].tool_index == ti))
      break;
  int fresh = i == s->count;
  block *b = fresh ? new_block(s, kind, ti) : &s->blocks[i];
  if (!b)
    return;
  if ((!b->id.len && id.len && !append(&b->id, id.data, id.len)) ||
      (!b->name.len && name.len && !append(&b->name, name.data, name.len))) {
    s->status = PANGU_NOMEM;
    return;
  }
  if (fresh)
    emit(s,
         kind == PANGU_TEXT       ? PANGU_ASSISTANT_TEXT_START
         : kind == PANGU_THINKING ? PANGU_ASSISTANT_THINKING_START
                                  : PANGU_ASSISTANT_TOOL_START,
         i, (pangu_slice){0});
  if (!append(&b->data, value.data, value.len)) {
    s->status = PANGU_NOMEM;
    return;
  }
  emit(s,
       kind == PANGU_TEXT       ? PANGU_ASSISTANT_TEXT_DELTA
       : kind == PANGU_THINKING ? PANGU_ASSISTANT_THINKING_DELTA
                                : PANGU_ASSISTANT_TOOL_DELTA,
       i, value);
}
static void chunk(stream_state *s) {
  if (!s->event.len || s->status)
    return;
  if (is((pangu_slice){s->event.data, s->event.len}, "[DONE]")) {
    s->done = 1;
    return;
  }
  yyjson_doc *d = yyjson_read(s->event.data, s->event.len, 0);
  if (!d) {
    s->status = PANGU_MODEL_ERROR;
    return;
  }
  yyjson_val *r = yyjson_doc_get_root(d);
  s->status = pangu_core_observe_provider_event(
      s->owner, (pangu_slice){s->event.data, s->event.len});
  if (s->status) {
    yyjson_doc_free(d);
    return;
  }
  if (yyjson_obj_get(r, "error")) {
    s->status = PANGU_MODEL_ERROR;
    yyjson_doc_free(d);
    return;
  }
  /* Pi openai-completions: first nonempty response id, and first nonempty
   * model different from the requested model. Own bytes beyond JSON lifetime. */
  pangu_slice response_id = slice(yyjson_obj_get(r,"id"));
  pangu_slice response_model = slice(yyjson_obj_get(r,"model"));
  if ((!s->response_id.len && response_id.len &&
       !append(&s->response_id,response_id.data,response_id.len)) ||
      (!s->response_model.len && response_model.len &&
       !(response_model.len==s->message.model.len &&
         !memcmp(response_model.data,s->message.model.data,response_model.len)) &&
       !append(&s->response_model,response_model.data,response_model.len))) {
    s->status=PANGU_NOMEM;yyjson_doc_free(d);return;
  }
  yyjson_val *u = yyjson_obj_get(r, "usage");
  if (u) {
    s->message.usage.input =
        yyjson_get_uint(yyjson_obj_get(u, "prompt_tokens"));
    s->message.usage.output =
        yyjson_get_uint(yyjson_obj_get(u, "completion_tokens"));
    s->message.usage.total = yyjson_get_uint(yyjson_obj_get(u, "total_tokens"));
    s->message.usage.cache_read =
        yyjson_get_uint(yyjson_obj_get(u, "prompt_cache_hit_tokens"));
    if (s->message.usage.input >= s->message.usage.cache_read)
      s->message.usage.input -= s->message.usage.cache_read;
    yyjson_val *details = yyjson_obj_get(u, "completion_tokens_details");
    yyjson_val *reasoning = yyjson_obj_get(details, "reasoning_tokens");
    if (yyjson_is_uint(reasoning)) {
      s->message.usage.has_reasoning = 1;
      s->message.usage.reasoning = yyjson_get_uint(reasoning);
    }
    yyjson_val *total = yyjson_obj_get(u, "total_tokens");
    if (yyjson_is_uint(total)) {
      s->message.usage.has_total_from_provider = 1;
      s->message.usage.total_from_provider = 1;
    }
  }
  yyjson_val *choice = yyjson_arr_get_first(yyjson_obj_get(r, "choices")),
             *delta = yyjson_obj_get(choice, "delta");
  pangu_slice reason = slice(yyjson_obj_get(choice, "finish_reason"));
  if (reason.len) {
    s->raw_stop_reason.len=0;
    if(!append(&s->raw_stop_reason,reason.data,reason.len)) {
      s->status=PANGU_NOMEM;yyjson_doc_free(d);return;
    }
    s->finished = 1;
    s->message.stop_reason = is(reason, "stop")         ? PANGU_STOP
                             : is(reason, "length")     ? PANGU_LENGTH
                             : is(reason, "tool_calls") ? PANGU_TOOL_USE
                                                        : PANGU_ERROR;
  }
  content_delta(s, PANGU_THINKING,
                slice(yyjson_obj_get(delta, "reasoning_content")), 0,
                (pangu_slice){0}, (pangu_slice){0});
  content_delta(s, PANGU_TEXT, slice(yyjson_obj_get(delta, "content")), 0,
                (pangu_slice){0}, (pangu_slice){0});
  yyjson_val *calls = yyjson_obj_get(delta, "tool_calls");
  size_t i, max;
  yyjson_val *c;
  yyjson_arr_foreach(calls, i, max, c) {
    yyjson_val *fn = yyjson_obj_get(c, "function"),
               *idx = yyjson_obj_get(c, "index");
    if (!yyjson_is_uint(idx)) {
      s->status = PANGU_MODEL_ERROR;
      break;
    }
    content_delta(s, PANGU_TOOL_CALL, slice(yyjson_obj_get(fn, "arguments")),
                  (size_t)yyjson_get_uint(idx), slice(yyjson_obj_get(c, "id")),
                  slice(yyjson_obj_get(fn, "name")));
  }
  yyjson_doc_free(d);
}
static void line(stream_state *s) {
  size_t n = s->line.len;
  if (n && s->line.data[n - 1] == '\r')
    n--;
  if (!n) {
    chunk(s);
    s->event.len = 0;
    if (s->event.data)
      s->event.data[0] = 0;
  } else if (n >= 5 && !memcmp(s->line.data, "data:", 5)) {
    size_t off = 5;
    if (off < n && s->line.data[off] == ' ')
      off++;
    if (s->event.len && !append(&s->event, "\n", 1))
      s->status = PANGU_NOMEM;
    if (!append(&s->event, s->line.data + off, n - off))
      s->status = PANGU_NOMEM;
  }
  s->line.len = 0;
  if (s->line.data)
    s->line.data[0] = 0;
}
static size_t body(char *p, size_t a, size_t b, void *ctx) {
  stream_state *s = ctx;
  size_t n = a * b;
  if (s->config->poll)
    s->status = s->config->poll(s->config->poll_ctx, s->owner);
  if (s->status)
    return 0;
  if (s->http_status < 200 || s->http_status >= 300)
    return n;
  for (size_t i = 0; i < n && !s->status; i++) {
    if (pangu_core_is_cancelled(s->owner)) {
      s->status = PANGU_CANCELLED;
      break;
    }
    if (p[i] == '\n')
      line(s);
    else if (!append(&s->line, p + i, 1))
      s->status = PANGU_NOMEM;
  }
  return s->status ? 0 : n;
}
static void clear_headers(stream_state *s) {
  for (size_t i = 0; i < s->header_count; i++) {
    free((void *)s->headers[i].name.data);
    free((void *)s->headers[i].value.data);
  }
  free(s->headers);
  s->headers = NULL;
  s->header_count = 0;
}
static size_t header(char *p, size_t a, size_t b, void *ctx) {
  stream_state *s = ctx;
  size_t n = a * b;
  if (n >= 5 && !memcmp(p, "HTTP/", 5)) {
    clear_headers(s);
    const char *space = memchr(p, ' ', n);
    s->http_status = space ? strtol(space + 1, NULL, 10) : 0;
  } else if (n == 2 && p[0] == '\r' && p[1] == '\n') {
    if (s->http_status >= 200) {
      pangu_provider_response r = {(unsigned)s->http_status, s->headers,
                                   s->header_count};
      s->status = pangu_core_observe_response(s->owner, &r);
    }
  } else {
    char *colon = memchr(p, ':', n);
    if (colon) {
      size_t key = (size_t)(colon - p), off = key + 1, end = n;
      while (off < end && (p[off] == ' ' || p[off] == '\t'))
        off++;
      while (end > off &&
             (p[end - 1] == '\r' || p[end - 1] == '\n' || p[end - 1] == ' '))
        end--;
      pangu_response_header *h =
          realloc(s->headers, (s->header_count + 1) * sizeof(*h));
      if (!h) {
        s->status = PANGU_NOMEM;
        return 0;
      }
      s->headers = h;
      buffer k = {0}, v = {0};
      if (!append(&k, p, key) || !append(&v, p + off, end - off)) {
        free(k.data);
        free(v.data);
        s->status = PANGU_NOMEM;
        return 0;
      }
      h[s->header_count++] =
          (pangu_response_header){{k.data, k.len}, {v.data, v.len}};
    }
  }
  return s->status ? 0 : n;
}
static int progress(void *ctx, curl_off_t a, curl_off_t b, curl_off_t c,
                    curl_off_t d) {
  (void)a;
  (void)b;
  (void)c;
  (void)d;
  stream_state *s = ctx;
  if (s->config->poll)
    s->status = s->config->poll(s->config->poll_ctx, s->owner);
  return s->status || pangu_core_is_cancelled(s->owner);
}
static pangu_status request_events(void *ctx, pangu_core *owner,
                                   const pangu_core *request,
                                   pangu_model_event_sink sink,
                                   void *sink_ctx) {
  const pg_deepseek_config *cfg = ctx;
  pangu_slice key = pangu_core_active_api_key(owner);
  if (!key.len)
    return PANGU_MODEL_ERROR;
  if (memchr(key.data, '\r', key.len) || memchr(key.data, '\n', key.len) ||
      memchr(key.data, 0, key.len))
    return PANGU_INVALID;
  char *data = NULL;
  size_t len = 0;
  pangu_status status = payload(owner, request, &data, &len);
  if (status)
    return status;
  stream_state s = {
      .owner = owner, .config = cfg, .sink = sink, .sink_ctx = sink_ctx};
  const pangu_model_config *m = pangu_core_active_model(owner);
  s.message =
      (pangu_message){.role = PANGU_ASSISTANT,
                      .stop_reason = PANGU_PENDING,
                      .model = m->id,
                      .provider = m->provider,
                      .api = m->api,
                      .thinking_level = pangu_core_active_thinking(owner)};
  CURL *curl = curl_easy_init();
  struct curl_slist *headers = NULL;
  buffer auth = {0}, url = {0};
  if (!curl || !append(&auth, "Authorization: Bearer ", 22) ||
      !append(&auth, key.data, key.len) ||
      !append(&url,
              cfg->endpoint.len ? cfg->endpoint.data
                                : "https://api.deepseek.com/chat/completions",
              cfg->endpoint.len
                  ? cfg->endpoint.len
                  : strlen("https://api.deepseek.com/chat/completions"))) {
    status = PANGU_NOMEM;
    goto cleanup;
  }
  headers = curl_slist_append(NULL, "Content-Type: application/json");
  if (!headers) {
    status = PANGU_NOMEM;
    goto cleanup;
  }
  struct curl_slist *next = curl_slist_append(headers, auth.data);
  if (!next) {
    status = PANGU_NOMEM;
    goto cleanup;
  }
  headers = next;
  emit(&s, PANGU_ASSISTANT_START, 0, (pangu_slice){0});
  if (s.status) {
    status = s.status;
    goto cleanup;
  }
#define SET(k, v)                                                              \
  do {                                                                         \
    if (curl_easy_setopt(curl, k, v) != CURLE_OK) {                            \
      status = PANGU_MODEL_ERROR;                                              \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0)
  SET(CURLOPT_URL, url.data);
  SET(CURLOPT_HTTPHEADER, headers);
  SET(CURLOPT_POSTFIELDS, data);
  SET(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)len);
  SET(CURLOPT_WRITEFUNCTION, body);
  SET(CURLOPT_WRITEDATA, &s);
  SET(CURLOPT_HEADERFUNCTION, header);
  SET(CURLOPT_HEADERDATA, &s);
  SET(CURLOPT_XFERINFOFUNCTION, progress);
  SET(CURLOPT_XFERINFODATA, &s);
  SET(CURLOPT_NOPROGRESS, 0L);
  SET(CURLOPT_NOSIGNAL, 1L);
  SET(CURLOPT_TIMEOUT_MS, cfg->timeout_ms ? cfg->timeout_ms : 120000L);
  SET(CURLOPT_CONNECTTIMEOUT_MS, 30000L);
  SET(CURLOPT_PROTOCOLS_STR, "http,https");
  SET(CURLOPT_FOLLOWLOCATION, 0L);
  SET(CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
  const pangu_client_identity *identity = pangu_core_active_identity(owner);
  buffer ua = {0};
  pangu_slice user_agent =
      identity && identity->user_agent == PANGU_USER_AGENT_CUSTOM
          ? identity->custom_user_agent
          : SL("PanGu/0.1");
  if (!append(&ua, user_agent.data, user_agent.len)) {
    status = PANGU_NOMEM;
    goto cleanup;
  }
  CURLcode user_agent_status =
      curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.data);
  free(ua.data);
  if (user_agent_status != CURLE_OK) {
    status = PANGU_MODEL_ERROR;
    goto cleanup;
  }
  if (!s.status) {
    CURLcode rc = curl_easy_perform(curl);
    if (s.status)
      status = s.status;
    else if (pangu_core_is_cancelled(owner))
      status = PANGU_CANCELLED;
    else if (rc != CURLE_OK || s.http_status < 200 || s.http_status >= 300)
      status = PANGU_MODEL_ERROR;
  }
  if (!status && s.line.len)
    line(&s);
  if (!status && s.event.len)
    chunk(&s);
  if (s.status)
    status = s.status;
  if (!status && (!s.finished || s.message.stop_reason == PANGU_ERROR))
    status = PANGU_MODEL_ERROR;
  if (!status) {
    for (size_t i = 0; i < s.count && !s.status; i++) {
      block *b = &s.blocks[i];
      if (b->value.kind == PANGU_TOOL_CALL) {
        yyjson_doc *args = yyjson_read(b->data.data, b->data.len, 0);
        if (!args || !b->id.len || !b->name.len) {
          yyjson_doc_free(args);
          status = PANGU_MODEL_ERROR;
          break;
        }
        yyjson_doc_free(args);
      }
      emit(&s,
           b->value.kind == PANGU_TEXT       ? PANGU_ASSISTANT_TEXT_END
           : b->value.kind == PANGU_THINKING ? PANGU_ASSISTANT_THINKING_END
                                             : PANGU_ASSISTANT_TOOL_END,
           i, (pangu_slice){0});
    }
    if (!status)
      status = emit(&s, PANGU_ASSISTANT_DONE, 0, (pangu_slice){0});
  }
cleanup:
  if (status && !s.status) {
    s.message.stop_reason =
        status == PANGU_CANCELLED ? PANGU_ABORTED : PANGU_ERROR;
    char diagnostic[80];
    /* Pi formatProviderError's provider/status prefix is consumed by overflow
     * recovery. HTTP 413 is unambiguous; never expose response body or keys. */
    int written = s.http_status == 413
        ? snprintf(diagnostic, sizeof(diagnostic),
                   "deepseek API error (413): request too large")
        : snprintf(diagnostic, sizeof(diagnostic),
                   "DeepSeek request failed (HTTP %ld)", s.http_status);
    s.message.error =
        status == PANGU_CANCELLED
            ? SL("Request aborted")
            : (pangu_slice){diagnostic, written > 0 ? (size_t)written : 0};
    emit(&s, PANGU_ASSISTANT_ERROR, 0, (pangu_slice){0});
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (auth.data) {
    volatile char *p = auth.data;
    for (size_t i = 0; i < auth.len; i++)
      p[i] = 0;
  }
  free(auth.data);
  free(url.data);
  free(data);
  for (size_t i = 0; i < s.count; i++) {
    free(s.blocks[i].data.data);
    free(s.blocks[i].id.data);
    free(s.blocks[i].name.data);
  }
  free(s.blocks);
  free(s.view);
  free(s.line.data);
  free(s.event.data);
  free(s.response_id.data);
  free(s.response_model.data);
  free(s.raw_stop_reason.data);
  clear_headers(&s);
  return status ? status : s.status;
}
static const pg_model_service model_service = {request_events};
static uv_once_t curl_once = UV_ONCE_INIT;
static CURLcode curl_init_status;
static void initialize_curl(void) {
  curl_init_status = curl_global_init(CURL_GLOBAL_DEFAULT);
}
static pg_status install(pg_scope *s, const void *cfg, size_t size) {
  if (!cfg || size != sizeof(pg_deepseek_config))
    return PG_INVALID;
  const pg_deepseek_config *c = cfg;
  if ((!c->endpoint.data && c->endpoint.len) ||
      c->timeout_ms < 0)
    return PG_INVALID;
  if (c->endpoint.len && memchr(c->endpoint.data, 0, c->endpoint.len))
    return PG_INVALID;
  uv_once(&curl_once, initialize_curl);
  if (curl_init_status != CURLE_OK)
    return PG_FAILED;
  /* Process-lifetime curl initialization; unloading a plugin cannot tear down
   * global curl state shared with other providers. */
  return pg_service_register(s, PG_NAME(PG_MODEL_SERVICE), 1, &model_service,
                             (void *)cfg);
}
const pg_plugin pg_deepseek_plugin = {
    1,
    sizeof(pg_plugin),
    {"pangu.model.deepseek", sizeof("pangu.model.deepseek") - 1},
    NULL,
    0,
    install};
