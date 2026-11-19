/* Pi MCP client/stdio semantics, snapshot documented in Agent服务-TODO.md.
 * Native synchronous private libuv loop; no second protocol implementation in
 * Core. */
#include "mcp-client.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <yyjson.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifndef _WIN32
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
typedef struct {
  client base;
  uv_loop_t loop;
  uv_process_t process;
  uv_pipe_t input, output, error;
  uv_timer_t tick;
  int spawned, exited, closed, io_error, busy;
  int pipes, timer;
  uint64_t next_id, waiting;
  yyjson_doc *response;
  char *line;
  size_t used, capacity, limit;
  int discard;
} stdio_client;
static int same(yyjson_val *v, const char *s) {
  return yyjson_is_str(v) && yyjson_get_len(v) == strlen(s) &&
         !memcmp(yyjson_get_str(v), s, strlen(s));
}
typedef struct {
  uv_write_t req;
  char *bytes;
  stdio_client *client;
} outgoing;
static void wrote(uv_write_t *r, int status) {
  outgoing *w = (outgoing *)r;
  if (status)
    w->client->io_error = 1;
  free(w->bytes);
  free(w);
}
static pangu_status send_doc(stdio_client *c, yyjson_mut_doc *doc) {
  size_t n = 0;
  char *json = yyjson_mut_write(doc, 0, &n);
  if (!json)
    return PANGU_NOMEM;
  if (n > UINT_MAX - 1) {
    free(json);
    return PANGU_INVALID;
  }
  outgoing *w = calloc(1, sizeof(*w));
  if (!w) {
    free(json);
    return PANGU_NOMEM;
  }
  w->bytes = malloc(n + 1);
  w->client = c;
  if (!w->bytes) {
    free(w);
    free(json);
    return PANGU_NOMEM;
  }
  memcpy(w->bytes, json, n);
  w->bytes[n] = '\n';
  free(json);
  uv_buf_t b = uv_buf_init(w->bytes, (unsigned)n + 1);
  int rc = uv_write(&w->req, (uv_stream_t *)&c->input, &b, 1, wrote);
  if (rc) {
    free(w->bytes);
    free(w);
    return PANGU_TOOL_ERROR;
  }
  return PANGU_OK;
}
static pangu_status send_message(client *base, const char *method,
                                 yyjson_val *params, uint64_t id) {
  stdio_client *c = (stdio_client *)base;
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return PANGU_NOMEM;
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
  pangu_status st = ok ? send_doc(c, d) : PANGU_NOMEM;
  yyjson_mut_doc_free(d);
  return st;
}
/* Incoming ping is supported; capabilities advertise no roots/sampling/etc. */
static void server_request(stdio_client *c, yyjson_val *root) {
  yyjson_mut_doc *d = pg_mcp_client_server_response(root);
  if (!d || send_doc(c, d) != PANGU_OK)
    c->io_error = 1;
  yyjson_mut_doc_free(d);
}

static void frame(stdio_client *c) {
  yyjson_doc *d = yyjson_read(c->line, c->used, 0);
  if (!d)
    return;
  yyjson_val *r = yyjson_doc_get_root(d), *id = yyjson_obj_get(r, "id");
  if (!yyjson_is_obj(r) || !same(yyjson_obj_get(r, "jsonrpc"), "2.0"))
    goto done;
  if (yyjson_is_str(yyjson_obj_get(r, "method"))) {
    if (yyjson_is_str(id) || yyjson_is_num(id))
      server_request(c, r);
    goto done;
  }
  yyjson_val *error = yyjson_obj_get(r, "error"),
             *result = yyjson_obj_get(r, "result");
  if ((result && error) ||
      (!result && (!yyjson_is_obj(error) ||
                   !yyjson_is_num(yyjson_obj_get(error, "code")) ||
                   !yyjson_is_str(yyjson_obj_get(error, "message")))))
    goto done;
  if (!c->response && c->waiting && yyjson_is_uint(id) &&
      yyjson_get_uint(id) == c->waiting) {
    c->response = d;
    return;
  }
done:
  yyjson_doc_free(d);
}
static void allocate(uv_handle_t *h, size_t suggested, uv_buf_t *b) {
  (void)h;
  (void)suggested;
  b->base = malloc(8192);
  b->len = b->base ? 8192 : 0;
}
static void read_output(uv_stream_t *stream, ssize_t n, const uv_buf_t *b) {
  stdio_client *c = stream->data;
  if (n < 0) {
    c->closed = 1;
    uv_read_stop(stream);
  }
  for (ssize_t i = 0; i < n; ++i) {
    char byte = b->base[i];
    if (byte == '\n') {
      if (!c->discard && c->used)
        frame(c);
      c->used = 0;
      c->discard = 0;
      continue;
    }
    if (c->discard)
      continue;
    if (c->used == c->limit) {
      c->used = 0;
      c->discard = 1;
      continue;
    }
    if (c->used == c->capacity) {
      size_t cap = c->capacity ? c->capacity * 2 : 4096;
      if (cap < c->capacity || cap > c->limit)
        cap = c->limit;
      char *p = realloc(c->line, cap);
      if (!p) {
        c->io_error = 1;
        c->discard = 1;
        continue;
      }
      c->line = p;
      c->capacity = cap;
    }
    c->line[c->used++] = byte;
  }
  free(b->base);
}
static void read_error(uv_stream_t *s, ssize_t n, const uv_buf_t *b) {
  if (n < 0)
    uv_read_stop(s);
  free(b->base);
}
static void exited(uv_process_t *p, int64_t status, int signal) {
  (void)status;
  (void)signal;
  stdio_client *c = p->data;
  c->exited = 1;
  c->closed = 1;
  uv_close((uv_handle_t *)p, NULL);
}
static void tick(uv_timer_t *t) { (void)t; }
static uint64_t now(void) { return uv_hrtime() / 1000000; }
static void pump(stdio_client *c) { uv_run(&c->loop, UV_RUN_ONCE); }
static void kill_tree(stdio_client *c, int signal) {
#ifndef _WIN32
  if (c->spawned && c->process.pid > 0) {
    if (kill(-c->process.pid, signal) != 0 && !c->exited)
      uv_process_kill(&c->process, signal);
  }
#else
  if (c->spawned && !c->exited)
    uv_process_kill(&c->process, signal);
#endif
}
static void close_handle(uv_handle_t *h) {
  if (!uv_is_closing(h))
    uv_close(h, NULL);
}
static void cleanup(void *ctx) {
  stdio_client *c = ctx;
  if (c->spawned) {
    close_handle((uv_handle_t *)&c->input);
    uint64_t start = now(), grace = c->base.cfg.close_timeout_ms < 500
                                        ? c->base.cfg.close_timeout_ms
                                        : 500;
    while (!c->exited && now() - start < grace)
      pump(c);
    /* Also terminate descendants if the parent already exited. */
    kill_tree(c, SIGTERM);
    start = now();
    while (!c->exited && now() - start < c->base.cfg.close_timeout_ms)
      pump(c);
    kill_tree(c, SIGKILL);
    while (!c->exited)
      pump(c);
  }
  if (c->pipes >= 1)
    close_handle((uv_handle_t *)&c->input);
  if (c->pipes >= 2)
    close_handle((uv_handle_t *)&c->output);
  if (c->pipes >= 3)
    close_handle((uv_handle_t *)&c->error);
  if (c->timer)
    close_handle((uv_handle_t *)&c->tick);
  uv_run(&c->loop, UV_RUN_DEFAULT);
  uv_loop_close(&c->loop);
  yyjson_doc_free(c->response);
  free(c->line);
  pg_mcp_client_clear(&c->base);
  free(c);
}
static void cancel_request(stdio_client *c, uint64_t id, const char *reason) {
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return;
  yyjson_mut_val *r = yyjson_mut_obj(d), *p = yyjson_mut_obj(d);
  int ok = r && p && yyjson_mut_obj_add_str(d, r, "jsonrpc", "2.0") &&
           yyjson_mut_obj_add_str(d, r, "method", "notifications/cancelled") &&
           yyjson_mut_obj_add_uint(d, p, "requestId", id) &&
           yyjson_mut_obj_add_str(d, p, "reason", reason) &&
           yyjson_mut_obj_add_val(d, r, "params", p);
  yyjson_mut_doc_set_root(d, r);
  if (ok)
    (void)send_doc(c, d);
  yyjson_mut_doc_free(d);
}
static pangu_status request(client *base, const char *method,
                            yyjson_val *params, pangu_core *core,
                            yyjson_doc **out) {
  stdio_client *c = (stdio_client *)base;
  *out = NULL;
  c->base.last_error =
      c->busy ? "MCP connection busy" : "MCP connection closed";
  if (c->busy || c->closed || c->io_error)
    return PANGU_TOOL_ERROR;
  if (core && pangu_core_is_cancelled(core))
    return PANGU_CANCELLED;
  c->busy = 1;
  c->waiting = ++c->next_id;
  c->base.last_error = "MCP transport write failed";
  pangu_status st = send_message(&c->base, method, params, c->waiting);
  uint64_t start = now();
  while (st == PANGU_OK && !c->response) {
    if (c->base.cfg.poll)
      c->base.cfg.poll(c->base.cfg.poll_ctx);
    if (core && pangu_core_is_cancelled(core)) {
      st = PANGU_CANCELLED;
      break;
    }
    if (c->closed || c->io_error) {
      c->base.last_error = "MCP connection closed";
      st = PANGU_TOOL_ERROR;
      break;
    }
    if (now() - start >= c->base.cfg.timeout_ms) {
      c->base.last_error = "MCP request timed out";
      st = PANGU_TOOL_ERROR;
      break;
    }
    pump(c);
  }
  if (st != PANGU_OK && strcmp(method, "initialize") && !c->closed)
    cancel_request(c, c->waiting,
                   st == PANGU_CANCELLED ? "Aborted" : "Request timed out");
  if (st == PANGU_OK) {
    *out = c->response;
    c->response = NULL;
  } else {
    yyjson_doc_free(c->response);
    c->response = NULL;
  }
  c->waiting = 0;
  c->busy = 0;
  return st;
}
static int valid_name(pg_name s) {
  return s.data && s.size && !memchr(s.data, 0, s.size);
}
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  if (!config || size != sizeof(pg_mcp_stdio_config))
    return PG_INVALID;
  const pg_mcp_stdio_config *cfg = config;
  if (!valid_name(cfg->service_name) ||
      (cfg->tool_prefix.size && !valid_name(cfg->tool_prefix)) || !cfg->argv ||
      !cfg->argv[0] || !cfg->argv[0][0] || !cfg->validate ||
      (cfg->cwd && cfg->cwd[0] != '/'))
    return PG_INVALID;
  stdio_client *c = calloc(1, sizeof(*c));
  if (!c)
    return PG_NOMEM;
  c->base.cfg = *cfg;
  c->limit = cfg->max_message_bytes ? cfg->max_message_bytes : 16 * 1024 * 1024;
  c->base.cfg.timeout_ms = cfg->timeout_ms ? cfg->timeout_ms : 30000;
  c->base.cfg.close_timeout_ms =
      cfg->close_timeout_ms ? cfg->close_timeout_ms : 2000;
  if (uv_loop_init(&c->loop)) {
    free(c);
    return PG_FAILED;
  }
  pg_status st = pg_scope_defer(scope, cleanup, c);
  if (st) {
    uv_loop_close(&c->loop);
    free(c);
    return st;
  }
  if (uv_timer_init(&c->loop, &c->tick))
    return PG_FAILED;
  c->timer = 1;
  if (uv_timer_start(&c->tick, tick, 5, 5))
    return PG_FAILED;
  if (uv_pipe_init(&c->loop, &c->input, 0))
    return PG_FAILED;
  c->pipes = 1;
  if (uv_pipe_init(&c->loop, &c->output, 0))
    return PG_FAILED;
  c->pipes = 2;
  if (uv_pipe_init(&c->loop, &c->error, 0))
    return PG_FAILED;
  c->pipes = 3;
  c->output.data = c;
  c->error.data = c;
  c->process.data = c;
  uv_stdio_container_t io[3] = {0};
  io[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
  io[0].data.stream = (uv_stream_t *)&c->input;
  io[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  io[1].data.stream = (uv_stream_t *)&c->output;
  io[2].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  io[2].data.stream = (uv_stream_t *)&c->error;
  uv_process_options_t options = {0};
  options.file = cfg->argv[0];
  options.args = (char **)cfg->argv;
  options.env = (char **)cfg->env;
  options.cwd = cfg->cwd;
  options.exit_cb = exited;
  options.stdio = io;
  options.stdio_count = 3;
  options.flags = UV_PROCESS_DETACHED;
  if (uv_spawn(&c->loop, &c->process, &options)) {
    close_handle((uv_handle_t *)&c->process);
    return PG_FAILED;
  }
  c->spawned = 1;
  if (uv_read_start((uv_stream_t *)&c->output, allocate, read_output) ||
      uv_read_start((uv_stream_t *)&c->error, allocate, read_error))
    return PG_FAILED;
  c->base.request = request;
  c->base.send = send_message;
  return pg_mcp_client_publish(&c->base, scope, cfg->service_name,
                               cfg->tool_prefix);
}
#else
static pg_status apply(pg_scope *scope, const void *config, size_t size) {
  (void)scope;
  (void)config;
  (void)size;
  return PG_FAILED;
}
#endif
const pg_plugin pg_mcp_stdio_plugin = {PANGU_SERVICES_ABI,
                                       sizeof(pg_plugin),
                                       {"pangu.mcp.stdio", 15},
                                       NULL,
                                       0,
                                       apply};
