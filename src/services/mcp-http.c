/* Pi MCP Streamable HTTP transport, native libcurl multi adaptation.
 * Shared client owns discovery, validation and results; no duplicate tool
 * logic. */
#include "mcp-client.h"
#include <ctype.h>
#include <curl/curl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

typedef struct http_client http_client;
typedef struct {
  char *data;
  size_t len, cap;
} buffer;
typedef struct transfer {
  http_client *owner;
  CURL *easy;
  struct curl_slist *headers;
  long status;
  CURLcode code;
  int done, sse, json, failed, oom, ignored_event, is_get;
  buffer body, line, event;
  char *cursor, *challenge, *session, *token;
  uint64_t retry;
  int has_retry, received;
  size_t data_lines;
  yyjson_doc *reply;
  uint64_t waiting;
} transfer;
typedef struct outgoing {
  struct outgoing *next;
  char *body;
  size_t size;
  uint64_t began;
  transfer *request;
} outgoing;
struct http_client {
  client base;
  pg_mcp_http_config cfg;
  char *url, *session, *version, *token;
  pg_mcp_http_header *headers;
  CURLM *multi;
  transfer *get;
  unsigned get_attempt;
  uint64_t get_at;
  int get_disabled, get_auth_retried, busy, closing;
  uint64_t next_id;
  char *get_cursor;
  char error_text[112];
  outgoing *outgoing, *outgoing_tail;
  size_t outgoing_bytes;
};
static uint64_t now(void) { return uv_hrtime() / 1000000; }
static char *dup_bytes(const char *s, size_t n) {
  if (n == SIZE_MAX || (n && !s))
    return NULL;
  char *r = malloc(n + 1);
  if (r) {
    if (n)
      memcpy(r, s, n);
    r[n] = 0;
  }
  return r;
}
static int append(buffer *b, const char *s, size_t n, size_t limit) {
  if (n > limit || b->len > limit - n || b->len + n == SIZE_MAX)
    return 0;
  size_t needed = b->len + n + 1;
  if (needed > b->cap) {
    size_t cap = b->cap ? b->cap : 256;
    while (cap < needed) {
      if (cap > SIZE_MAX / 2) {
        cap = needed;
        break;
      }
      cap *= 2;
    }
    char *p = realloc(b->data, cap);
    if (!p)
      return -1;
    b->data = p;
    b->cap = cap;
  }
  if (n)
    memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = 0;
  return 1;
}
static void reset(buffer *b) {
  b->len = 0;
  if (b->data)
    b->data[0] = 0;
}
static int clean(pg_name n) {
  return (!n.size || n.data) && !memchr(n.data ? n.data : "", 0, n.size) &&
         !memchr(n.data ? n.data : "", '\r', n.size) &&
         !memchr(n.data ? n.data : "", '\n', n.size);
}
static int eq(const char *a, size_t n, const char *b) {
  if (n != strlen(b))
    return 0;
  for (size_t i = 0; i < n; i++)
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
      return 0;
  return 1;
}
static int replace(char **p, const char *s, size_t n) {
  char *q = dup_bytes(s, n);
  if (!q)
    return 0;
  free(*p);
  *p = q;
  return 1;
}
static void accept_message(transfer *t, yyjson_val *v) {
  if (!yyjson_is_obj(v))
    return;
  yyjson_val *rpc = yyjson_obj_get(v, "jsonrpc"), *id = yyjson_obj_get(v, "id");
  if (!yyjson_is_str(rpc) || strcmp(yyjson_get_str(rpc), "2.0"))
    return;
  if (yyjson_is_str(yyjson_obj_get(v, "method"))) {
    if (!yyjson_is_str(id) && !yyjson_is_num(id))
      return;
    yyjson_mut_doc *d = pg_mcp_client_server_response(v);
    size_t n = 0;
    char *body = d ? yyjson_mut_write(d, 0, &n) : NULL;
    yyjson_mut_doc_free(d);
    if (!body) {
      t->failed = t->oom = 1;
      return;
    }
    http_client *c = t->owner;
    if (n > c->cfg.max_message_bytes - c->outgoing_bytes) {
      free(body);
      t->failed = 1;
      return;
    }
    outgoing *o = calloc(1, sizeof(*o));
    if (!o) {
      free(body);
      t->failed = t->oom = 1;
      return;
    }
    o->body = body;
    o->size = n;
    if (c->outgoing_tail)
      c->outgoing_tail->next = o;
    else
      c->outgoing = o;
    c->outgoing_tail = o;
    c->outgoing_bytes += n;
    return;
  }
  yyjson_val *result = yyjson_obj_get(v, "result"),
             *error = yyjson_obj_get(v, "error");
  if ((result && error) ||
      (!result && (!yyjson_is_obj(error) ||
                   !yyjson_is_num(yyjson_obj_get(error, "code")) ||
                   !yyjson_is_str(yyjson_obj_get(error, "message")))))
    return;
  if (!t->reply && t->waiting && yyjson_is_uint(id) &&
      yyjson_get_uint(id) == t->waiting) {
    size_t n;
    char *s = yyjson_val_write(v, 0, &n);
    if (s) {
      t->reply = yyjson_read(s, n, 0);
      free(s);
    }
    if (!t->reply)
      t->oom = 1;
  }
}
static void parse_message(transfer *t, const char *s, size_t n) {
  yyjson_doc *d = yyjson_read(s, n, 0);
  if (!d)
    return;
  yyjson_val *v = yyjson_doc_get_root(d);
  if (yyjson_is_arr(v)) {
    size_t i, m;
    yyjson_val *item;
    yyjson_arr_foreach(v, i, m, item) accept_message(t, item);
  } else
    accept_message(t, v);
  yyjson_doc_free(d);
}
static void dispatch(transfer *t) {
  if (t->data_lines)
    t->received = 1;
  if (t->event.len && !t->ignored_event)
    parse_message(t, t->event.data, t->event.len);
  reset(&t->event);
  t->ignored_event = 0;
  t->data_lines = 0;
}
static void sse_line(transfer *t) {
  size_t n = t->line.len;
  char *s = t->line.data;
  if (n && s[n - 1] == '\r')
    n--;
  if (!n) {
    dispatch(t);
    return;
  }
  if (s[0] == ':')
    return;
  size_t k = 0;
  while (k < n && s[k] != ':')
    k++;
  size_t off = k < n ? k + 1 : n;
  if (off < n && s[off] == ' ')
    off++;
  const char *v = s + off;
  size_t len = n - off;
  if (k == 4 && !memcmp(s, "data", 4)) {
    int a = 1;
    if (t->data_lines++)
      a = append(&t->event, "\n", 1, t->owner->cfg.max_message_bytes);
    if (a > 0)
      a = append(&t->event, v, len, t->owner->cfg.max_message_bytes);
    if (a <= 0) {
      t->failed = 1;
      t->oom = a < 0;
    }
  } else if (k == 2 && !memcmp(s, "id", 2) && !memchr(v, 0, len)) {
    if (!replace(&t->cursor, v, len))
      t->oom = t->failed = 1;
  } else if (k == 5 && !memcmp(s, "event", 5)) {
    t->ignored_event = !(len == 7 && !memcmp(v, "message", 7)) && len != 0;
  } else if (k == 5 && !memcmp(s, "retry", 5) && len) {
    uint64_t value = 0;
    int valid = 1;
    for (size_t i = 0; i < len; i++) {
      if (v[i] < '0' || v[i] > '9' || value > (UINT64_MAX - 9) / 10) {
        valid = 0;
        break;
      }
      value = value * 10 + (unsigned)(v[i] - '0');
    }
    if (valid) {
      t->retry = value;
      t->has_retry = 1;
    }
  }
}
static size_t body_cb(char *s, size_t size, size_t count, void *ctx) {
  transfer *t = ctx;
  if (size && count > SIZE_MAX / size)
    return 0;
  size_t n = size * count;
  if (t->status < 200 || t->status >= 300)
    return n; /* never retain remote error bodies/credentials */
  if (!t->waiting && !t->sse)
    return n;
  if (t->sse) {
    for (size_t i = 0; i < n; i++) {
      if (s[i] == '\n') {
        sse_line(t);
        reset(&t->line);
      } else {
        int a = append(&t->line, s + i, 1, t->owner->cfg.max_message_bytes);
        if (a <= 0) {
          t->failed = 1;
          t->oom = a < 0;
        }
      }
      if (t->failed)
        return 0;
    }
  } else if (t->json) {
    int a = append(&t->body, s, n, t->owner->cfg.max_message_bytes);
    if (a <= 0) {
      t->failed = 1;
      t->oom = a < 0;
      return 0;
    }
  }
  return n;
}
static size_t header_cb(char *s, size_t size, size_t count, void *ctx) {
  transfer *t = ctx;
  if (size && count > SIZE_MAX / size)
    return 0;
  size_t n = size * count;
  if (n >= 5 && !memcmp(s, "HTTP/", 5)) {
    char *p = memchr(s, ' ', n);
    t->status = p ? strtol(p + 1, NULL, 10) : 0;
    t->sse = t->json = 0;
    return n;
  }
  char *colon = memchr(s, ':', n);
  if (!colon)
    return n;
  size_t key = (size_t)(colon - s), off = key + 1, end = n;
  while (off < end && (s[off] == ' ' || s[off] == '\t'))
    off++;
  while (end > off && (s[end - 1] == '\r' || s[end - 1] == '\n' ||
                       s[end - 1] == ' ' || s[end - 1] == '\t'))
    end--;
  const char *v = s + off;
  size_t len = end - off;
  if (eq(s, key, "content-type")) {
    size_t m = 0;
    while (m < len && v[m] != ';' && v[m] != ' ')
      m++;
    t->sse = eq(v, m, "text/event-stream");
    t->json = eq(v, m, "application/json");
  } else if (eq(s, key, "mcp-session-id") && len && t->status >= 200 &&
             t->status < 300) {
    if (!clean((pg_name){v, len}))
      return 0;
    if (!replace(&t->session, v, len))
      t->oom = t->failed = 1;
  } else if (eq(s, key, "www-authenticate")) {
    if (!replace(&t->challenge, v, len))
      t->oom = t->failed = 1;
  }
  return t->failed ? 0 : n;
}
static int add_header(transfer *t, const char *name, const char *value,
                      size_t len) {
  size_t k = strlen(name);
  if (len > SIZE_MAX - k - 3)
    return 0;
  char *s = malloc(k + len + 3);
  if (!s)
    return 0;
  memcpy(s, name, k);
  s[k] = ':';
  s[k + 1] = ' ';
  memcpy(s + k + 2, value, len);
  s[k + 2 + len] = 0;
  struct curl_slist *h = curl_slist_append(t->headers, s);
  free(s);
  if (!h)
    return 0;
  t->headers = h;
  return 1;
}
static int owned_header(pg_name n) {
  return eq(n.data, n.size, "accept") || eq(n.data, n.size, "content-type") ||
         eq(n.data, n.size, "mcp-session-id") ||
         eq(n.data, n.size, "mcp-protocol-version") ||
         eq(n.data, n.size, "last-event-id");
}
static void drop(transfer *t) {
  if (!t)
    return;
  if (t->easy) {
    curl_multi_remove_handle(t->owner->multi, t->easy);
    curl_easy_cleanup(t->easy);
  }
  curl_slist_free_all(t->headers);
  free(t->body.data);
  free(t->line.data);
  free(t->event.data);
  free(t->cursor);
  free(t->session);
  free(t->token);
  free(t->challenge);
  yyjson_doc_free(t->reply);
  free(t);
}
static pangu_status refresh_token(http_client *c) {
  if (!c->cfg.token)
    return PANGU_OK;
  pangu_slice token = {0};
  pangu_status st = c->cfg.token(c->cfg.auth_ctx, &token);
  if (st != PANGU_OK)
    return st;
  if (!clean((pg_name){token.data, token.len}))
    return PANGU_INVALID;
  return replace(&c->token, token.data, token.len) ? PANGU_OK : PANGU_NOMEM;
}
static transfer *start(http_client *c, const char *method, const char *body,
                       size_t n, uint64_t id, const char *cursor) {
  transfer *t = calloc(1, sizeof(*t));
  if (!t)
    return NULL;
  t->owner = c;
  t->is_get = !strcmp(method, "GET");
  t->waiting = id;
  if (cursor && !replace(&t->cursor, cursor, strlen(cursor)))
    goto fail;
  if (c->token && !replace(&t->token, c->token, strlen(c->token)))
    goto fail;
  t->easy = curl_easy_init();
  if (!t->easy)
    goto fail;
  for (size_t i = 0; i < c->cfg.header_count; i++) {
    pg_mcp_http_header h = c->headers[i];
    int superseded = 0;
    for (size_t j = i + 1; j < c->cfg.header_count; j++)
      if (eq(h.name.data, h.name.size, c->headers[j].name.data))
        superseded = 1;
    if (superseded)
      continue;
    if (owned_header(h.name) || (c->token && *c->token &&
                                 eq(h.name.data, h.name.size, "authorization")))
      continue;
    if (!add_header(t, h.name.data, h.value.data, h.value.size))
      goto fail;
  }
  if (!add_header(t, "Accept",
                  !strcmp(method, "GET")
                      ? "text/event-stream"
                      : "application/json, text/event-stream",
                  !strcmp(method, "GET") ? 17 : 35))
    goto fail;
  if (body && !add_header(t, "Content-Type", "application/json", 16))
    goto fail;
  if (c->session &&
      !add_header(t, "Mcp-Session-Id", c->session, strlen(c->session)))
    goto fail;
  if (c->version &&
      !add_header(t, "MCP-Protocol-Version", c->version, strlen(c->version)))
    goto fail;
  if (cursor && !add_header(t, "Last-Event-ID", cursor, strlen(cursor)))
    goto fail;
  if (c->token && *c->token) {
    buffer b = {0};
    int a = append(&b, "Bearer ", 7, SIZE_MAX - 1);
    if (a > 0)
      a = append(&b, c->token, strlen(c->token), SIZE_MAX - 1);
    int ok = a > 0 && add_header(t, "Authorization", b.data, b.len);
    free(b.data);
    if (!ok)
      goto fail;
  }
#define SET(opt, val)                                                          \
  do {                                                                         \
    if (curl_easy_setopt(t->easy, opt, val) != CURLE_OK)                       \
      goto fail;                                                               \
  } while (0)
  SET(CURLOPT_URL, c->url);
  SET(CURLOPT_CUSTOMREQUEST, method);
  SET(CURLOPT_PROTOCOLS_STR, "http,https");
  SET(CURLOPT_NOSIGNAL, 1L);
  uint64_t connect_ms =
      c->closing ? c->cfg.close_timeout_ms : c->cfg.timeout_ms;
  SET(CURLOPT_CONNECTTIMEOUT_MS,
      connect_ms > LONG_MAX ? LONG_MAX : (long)connect_ms);
  SET(CURLOPT_HTTPHEADER, t->headers);
  SET(CURLOPT_WRITEFUNCTION, body_cb);
  SET(CURLOPT_WRITEDATA, t);
  SET(CURLOPT_HEADERFUNCTION, header_cb);
  SET(CURLOPT_HEADERDATA, t);
  SET(CURLOPT_PRIVATE, t);
  if (body) {
    SET(CURLOPT_POSTFIELDS, body);
    SET(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)n);
  }
  if (curl_multi_add_handle(c->multi, t->easy) != CURLM_OK)
    goto fail;
#undef SET
  return t;
fail:
  drop(t);
  return NULL;
}
static int pump(http_client *c) {
  int running = 0;
  if (curl_multi_perform(c->multi, &running) != CURLM_OK)
    return 0;
  int left;
  CURLMsg *msg;
  while ((msg = curl_multi_info_read(c->multi, &left)))
    if (msg->msg == CURLMSG_DONE) {
      transfer *t = NULL;
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &t);
      if (!t)
        continue;
      t->done = 1;
      t->code = msg->data.result;
      if (t->sse) {
        if (t->line.len)
          sse_line(t);
        dispatch(t);
      } else if (t->json && t->body.len)
        parse_message(t, t->body.data, t->body.len);
      if (t->session) {
        free(c->session);
        c->session = t->session;
        t->session = NULL;
      }
    }
  return 1;
}
static int cancelled(http_client *c, pangu_core *core) {
  if (!c->closing && c->cfg.poll)
    c->cfg.poll(c->cfg.poll_ctx);
  return core && pangu_core_is_cancelled(core);
}
static uint64_t delay(http_client *c, unsigned attempt, transfer *t) {
  if (t && t->has_retry)
    return t->retry;
  uint64_t d = c->cfg.reconnect_initial_ms;
  while (attempt-- && d < c->cfg.reconnect_max_ms) {
    if (d > UINT64_MAX / 2)
      return c->cfg.reconnect_max_ms;
    d *= 2;
  }
  return d > c->cfg.reconnect_max_ms ? c->cfg.reconnect_max_ms : d;
}
static int needs_authorization(transfer *t) {
  if (t->status == 401)
    return 1;
  if (t->status != 403 || !t->challenge)
    return 0;
  const char *p = t->challenge;
  for (size_t i = 0; p[i]; i++) {
    if (i && !isspace((unsigned char)p[i - 1]) && p[i - 1] != ',')
      continue;
    const char *q = p + i;
    const char *key = "error=";
    size_t k = 0;
    while (key[k] && q[k] && tolower((unsigned char)q[k]) == key[k])
      k++;
    if (key[k])
      continue;
    q += k;
    if (*q == '"')
      q++;
    const char *value = "insufficient_scope";
    k = 0;
    while (value[k] && q[k] && tolower((unsigned char)q[k]) == value[k])
      k++;
    if (!value[k])
      return 1;
  }
  return 0;
}
static pangu_status authorize(http_client *c, transfer *t) {
  return c->cfg.unauthorized(
      c->cfg.auth_ctx, (unsigned)t->status,
      (pangu_slice){t->challenge, t->challenge ? strlen(t->challenge) : 0},
      (pangu_slice){t->token, t->token ? strlen(t->token) : 0});
}
static void maintain_get(http_client *c, pangu_core *core) {
  if (c->get && c->get->done) {
    transfer *t = c->get;
    if (t->cursor)
      replace(&c->get_cursor, t->cursor, strlen(t->cursor));
    if (needs_authorization(t) && !c->get_auth_retried && c->cfg.unauthorized) {
      c->get_auth_retried = 1;
      pangu_status st = authorize(c, t);
      if (st == PANGU_OK && !cancelled(c, core))
        st = refresh_token(c);
      if (st == PANGU_OK && !cancelled(c, core)) {
        drop(t);
        c->get = start(c, "GET", NULL, 0, 0, c->get_cursor);
        if (!c->get)
          c->get_disabled = 1;
        return;
      }
      c->get_disabled = 1;
    }
    if (t->status >= 200 && t->status < 300)
      c->get_auth_retried = 0;
    if (t->status == 405 ||
        (t->status >= 400 && t->status < 500 && t->status != 408 &&
         t->status != 429) ||
        t->failed || (!t->sse && t->code == CURLE_OK))
      c->get_disabled = 1;
    if (t->received)
      c->get_attempt = 0;
    c->get_at = now() + delay(c, c->get_attempt, t);
    c->get_attempt++;
    drop(t);
    c->get = NULL;
  }
  if (!c->get_disabled && !c->get && c->get_at && now() >= c->get_at &&
      c->get_attempt <= c->cfg.reconnect_max_retries) {
    /* Auth callbacks only run on caller thread before dispatch. */
    if (refresh_token(c) == PANGU_OK && !cancelled(c, core))
      c->get = start(c, "GET", NULL, 0, 0, c->get_cursor);
    if (!c->get) {
      c->get_attempt++;
      c->get_at = now() + delay(c, c->get_attempt, NULL);
    }
  }
}
static void maintain_outgoing(http_client *c, pangu_core *core) {
  outgoing **link = &c->outgoing;
  while (*link) {
    outgoing *o = *link;
    if (!o->request) {
      if (refresh_token(c) != PANGU_OK || cancelled(c, core))
        return;
      o->request = start(c, "POST", o->body, o->size, 0, NULL);
      o->began = now();
    }
    if (!o->request || o->request->done ||
        now() - o->began >= c->cfg.close_timeout_ms) {
      *link = o->next;
      c->outgoing_bytes -= o->size;
      drop(o->request);
      free(o->body);
      free(o);
      continue;
    }
    link = &o->next;
  }
  c->outgoing_tail = c->outgoing;
  while (c->outgoing_tail && c->outgoing_tail->next)
    c->outgoing_tail = c->outgoing_tail->next;
}
static pangu_status transport_error(http_client *c, transfer *t) {
  if (t->oom)
    return PANGU_NOMEM;
  if (t->status == 401)
    c->base.last_error = "MCP server requires authentication";
  else if (t->status == 404 && c->session)
    c->base.last_error = "MCP session expired";
  else if (t->failed)
    c->base.last_error = "MCP response exceeds message limit";
  else if (t->status < 200 || t->status >= 300) {
    snprintf(c->error_text, sizeof(c->error_text),
             "MCP HTTP request failed with status %ld", t->status);
    c->base.last_error = c->error_text;
  } else if (t->status == 202 || t->status == 204)
    c->base.last_error = "MCP server accepted request without a response";
  else if (!t->sse && !t->json)
    c->base.last_error = "Unsupported MCP response content type";
  else
    c->base.last_error =
        "MCP response stream failed: stream ended without a response";
  return PANGU_TOOL_ERROR;
}
static pangu_status exchange(http_client *c, const char *method,
                             const char *body, size_t n, uint64_t id,
                             pangu_core *core, yyjson_doc **reply) {
  pangu_status st = c->closing ? PANGU_OK : refresh_token(c);
  if (st != PANGU_OK)
    return st;
  uint64_t begin = now(),
           budget = c->closing ? c->cfg.close_timeout_ms : c->cfg.timeout_ms;
  if (cancelled(c, core))
    return PANGU_CANCELLED;
  transfer *t = start(c, method, body, n, id, NULL);
  if (!t)
    return PANGU_NOMEM;
  unsigned retried_auth = 0, attempt = 0;
  for (;;) {
    if (cancelled(c, core)) {
      st = PANGU_CANCELLED;
      break;
    }
    if (now() - begin >= budget) {
      c->base.last_error = "MCP request timed out";
      st = PANGU_TOOL_ERROR;
      break;
    }
    if (!pump(c)) {
      st = PANGU_TOOL_ERROR;
      break;
    }
    if (!c->closing) {
      maintain_get(c, core);
      maintain_outgoing(c, core);
    }
    if (t->session) {
      free(c->session);
      c->session = t->session;
      t->session = NULL;
    }
    if (t->reply) {
      *reply = t->reply;
      t->reply = NULL;
      st = PANGU_OK;
      break;
    }
    if (t->done) {
      if (needs_authorization(t) && !retried_auth && c->cfg.unauthorized &&
          !c->closing) {
        retried_auth = 1;
        st = authorize(c, t);
        if (st != PANGU_OK || cancelled(c, core)) {
          if (core && pangu_core_is_cancelled(core))
            st = PANGU_CANCELLED;
          break;
        }
        st = refresh_token(c);
        if (st != PANGU_OK)
          break;
        if (cancelled(c, core)) {
          st = PANGU_CANCELLED;
          break;
        }
        int is_get = t->is_get;
        char *cursor =
            t->cursor ? dup_bytes(t->cursor, strlen(t->cursor)) : NULL;
        if (t->cursor && !cursor) {
          st = PANGU_NOMEM;
          break;
        }
        drop(t);
        t = start(c, is_get ? "GET" : method, is_get ? NULL : body,
                  is_get ? 0 : n, id, cursor);
        free(cursor);
        if (!t) {
          st = PANGU_NOMEM;
          break;
        }
        continue;
      }
      if (!id && t->status >= 200 && t->status < 300 && t->code == CURLE_OK) {
        st = PANGU_OK;
        break;
      }
      int transient = t->status == 408 || t->status == 429 || t->status >= 500;
      if (id && t->cursor && !t->failed && (t->status < 400 || transient) &&
          attempt < c->cfg.reconnect_max_retries) {
        if (t->received)
          attempt = 0;
        uint64_t wait = delay(c, attempt++, t), at = now();
        while (now() - at < wait && now() - begin < budget &&
               !cancelled(c, core)) {
          if (!pump(c))
            break;
          curl_multi_poll(c->multi, NULL, 0, 10, NULL);
        }
        if (cancelled(c, core)) {
          st = PANGU_CANCELLED;
          break;
        }
        char *cursor = dup_bytes(t->cursor, strlen(t->cursor));
        if (!cursor) {
          st = PANGU_NOMEM;
          break;
        }
        drop(t);
        t = start(c, "GET", NULL, 0, id, cursor);
        free(cursor);
        if (!t) {
          st = PANGU_NOMEM;
          break;
        }
        continue;
      }
      st = transport_error(c, t);
      break;
    }
    curl_multi_poll(c->multi, NULL, 0, 10, NULL);
  }
  drop(t);
  return st;
}
static char *encode(const char *method, yyjson_val *params, uint64_t id,
                    size_t *n) {
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return NULL;
  yyjson_mut_val *r = yyjson_mut_obj(d);
  int ok = r && yyjson_mut_obj_add_str(d, r, "jsonrpc", "2.0") &&
           yyjson_mut_obj_add_str(d, r, "method", method);
  if (id)
    ok = ok && yyjson_mut_obj_add_uint(d, r, "id", id);
  if (params) {
    yyjson_mut_val *p = yyjson_val_mut_copy(d, params);
    ok = ok && p && yyjson_mut_obj_add_val(d, r, "params", p);
  }
  yyjson_mut_doc_set_root(d, r);
  char *s = ok ? yyjson_mut_write(d, 0, n) : NULL;
  yyjson_mut_doc_free(d);
  return s;
}
static pangu_status send_message(client *base, const char *method,
                                 yyjson_val *params, uint64_t id) {
  http_client *c = (http_client *)base;
  size_t n;
  char *body = encode(method, params, id, &n);
  if (!body)
    return PANGU_NOMEM;
  yyjson_doc *r = NULL;
  pangu_status st = exchange(c, "POST", body, n, id, NULL, &r);
  free(body);
  yyjson_doc_free(r);
  if (st == PANGU_OK && !strcmp(method, "notifications/initialized") &&
      !c->cfg.disable_get_stream)
    c->get_at = now();
  return st;
}
static pangu_status request(client *base, const char *method,
                            yyjson_val *params, pangu_core *core,
                            yyjson_doc **out) {
  http_client *c = (http_client *)base;
  *out = NULL;
  if (c->busy) {
    base->last_error = "MCP connection busy";
    return PANGU_TOOL_ERROR;
  }
  if (cancelled(c, core))
    return PANGU_CANCELLED;
  c->busy = 1;
  base->last_error = "MCP HTTP transport failed";
  uint64_t id = ++c->next_id;
  size_t n;
  char *body = encode(method, params, id, &n);
  pangu_status st =
      body ? exchange(c, "POST", body, n, id, core, out) : PANGU_NOMEM;
  free(body);
  if ((st == PANGU_CANCELLED ||
       (st == PANGU_TOOL_ERROR &&
        !strcmp(base->last_error, "MCP request timed out"))) &&
      strcmp(method, "initialize")) {
    char json[160];
    int len =
        snprintf(json, sizeof(json), "{\"requestId\":%llu,\"reason\":\"%s\"}",
                 (unsigned long long)id,
                 st == PANGU_CANCELLED ? "Aborted" : "Request timed out");
    yyjson_doc *d = yyjson_read(json, (size_t)len, 0);
    if (d) {
      const char *error = base->last_error;
      c->closing = 1;
      (void)send_message(base, "notifications/cancelled",
                         yyjson_doc_get_root(d), 0);
      c->closing = 0;
      base->last_error = error;
      yyjson_doc_free(d);
    }
  }
  c->busy = 0;
  return st;
}
static pangu_status set_version(client *base, pangu_slice v) {
  return replace(&((http_client *)base)->version, v.data, v.len) ? PANGU_OK
                                                                 : PANGU_NOMEM;
}
static void cleanup(void *ctx) {
  http_client *c = ctx;
  c->closing = 1;
  drop(c->get);
  c->get = NULL;
  while (c->outgoing) {
    outgoing *o = c->outgoing;
    c->outgoing = o->next;
    drop(o->request);
    free(o->body);
    free(o);
  }
  c->outgoing_tail = NULL;
  if (c->session && c->multi) {
    /* No auth callback on close; exchange bypasses refresh when closing. */
    yyjson_doc *r = NULL;
    (void)exchange(c, "DELETE", NULL, 0, 0, NULL, &r);
    yyjson_doc_free(r);
  }
  if (c->multi)
    curl_multi_cleanup(c->multi);
  for (size_t i = 0; i < c->cfg.header_count; i++) {
    free((void *)c->headers[i].name.data);
    free((void *)c->headers[i].value.data);
  }
  free(c->headers);
  free(c->url);
  free(c->session);
  free(c->version);
  free(c->token);
  free(c->get_cursor);
  pg_mcp_client_clear(&c->base);
  free(c);
}
static uv_once_t curl_once = UV_ONCE_INIT;
static CURLcode curl_init_status;
static void initialize_curl(void) {
  curl_init_status = curl_global_init(CURL_GLOBAL_DEFAULT);
}
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  if (!config || size != sizeof(pg_mcp_http_config))
    return PG_INVALID;
  const pg_mcp_http_config *cfg = config;
  if (!cfg->service_name.size || !clean(cfg->service_name) ||
      !clean(cfg->tool_prefix) || !cfg->validate || !cfg->url.size ||
      !clean(cfg->url) || (cfg->header_count && !cfg->headers) ||
      cfg->header_count > SIZE_MAX / sizeof(pg_mcp_http_header))
    return PG_INVALID;
  for (size_t i = 0; i < cfg->header_count; i++) {
    pg_mcp_http_header h = cfg->headers[i];
    if (!h.name.size || !clean(h.name) || !clean(h.value))
      return PG_INVALID;
    for (size_t j = 0; j < h.name.size; j++)
      if (!isalnum((unsigned char)h.name.data[j]) &&
          !strchr("!#$%&'*+-.^_`|~", h.name.data[j]))
        return PG_INVALID;
  }
  uv_once(&curl_once, initialize_curl);
  if (curl_init_status != CURLE_OK)
    return PG_FAILED;
  http_client *c = calloc(1, sizeof(*c));
  if (!c)
    return PG_NOMEM;
  c->cfg = *cfg;
  c->cfg.header_count = 0;
  c->cfg.timeout_ms = cfg->timeout_ms ? cfg->timeout_ms : 30000;
  c->cfg.close_timeout_ms =
      cfg->close_timeout_ms ? cfg->close_timeout_ms : 1000;
  c->cfg.max_message_bytes =
      cfg->max_message_bytes ? cfg->max_message_bytes : 16 * 1024 * 1024;
  c->cfg.reconnect_initial_ms =
      cfg->reconnect_initial_ms ? cfg->reconnect_initial_ms : 1000;
  c->cfg.reconnect_max_ms =
      cfg->reconnect_max_ms ? cfg->reconnect_max_ms : 30000;
  c->cfg.reconnect_max_retries =
      cfg->reconnect_max_retries ? cfg->reconnect_max_retries : 5;
  c->base.cfg.validate = cfg->validate;
  c->base.cfg.validator_ctx = cfg->validator_ctx;
  c->base.request = request;
  c->base.send = send_message;
  c->base.set_version = set_version;
  c->url = dup_bytes(cfg->url.data, cfg->url.size);
  if (!c->url) {
    cleanup(c);
    return PG_NOMEM;
  }
  if (strncmp(c->url, "http://", 7) && strncmp(c->url, "https://", 8)) {
    cleanup(c);
    return PG_INVALID;
  }
  c->headers =
      calloc(cfg->header_count ? cfg->header_count : 1, sizeof(*c->headers));
  if (!c->headers) {
    cleanup(c);
    return PG_NOMEM;
  }
  for (size_t i = 0; i < cfg->header_count; i++) {
    c->headers[i].name = (pg_name){
        dup_bytes(cfg->headers[i].name.data, cfg->headers[i].name.size),
        cfg->headers[i].name.size};
    c->headers[i].value = (pg_name){
        dup_bytes(cfg->headers[i].value.data, cfg->headers[i].value.size),
        cfg->headers[i].value.size};
    c->cfg.header_count++;
    if (!c->headers[i].name.data || !c->headers[i].value.data) {
      cleanup(c);
      return PG_NOMEM;
    }
  }
  c->multi = curl_multi_init();
  if (!c->multi) {
    cleanup(c);
    return PG_NOMEM;
  }
  pg_status st = pg_scope_defer(scope, cleanup, c);
  if (st) {
    cleanup(c);
    return st;
  }
  return pg_mcp_client_publish(&c->base, scope, cfg->service_name,
                               cfg->tool_prefix);
}
const pg_plugin pg_mcp_http_plugin = {PANGU_SERVICES_ABI,
                                      sizeof(pg_plugin),
                                      {"pangu.mcp.http", 14},
                                      NULL,
                                      0,
                                      apply};
