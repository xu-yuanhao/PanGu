#include <pangu/services/execution.h>
#include <stdlib.h>
#include <signal.h>
#include <uv.h>
#ifndef _WIN32
#include <unistd.h>
#endif
/* POSIX differences are confined to group termination. libuv owns spawn, pipes,
 * timers, cwd and environment; no global cwd/environment mutation. */
#ifndef _WIN32
typedef struct {
  uv_loop_t loop;
  uv_process_t child;
  uv_pipe_t pipes[2];
  uv_timer_t timer;
  pangu_core *owner;
  const pg_process_request *request;
  pg_process_output emit;
  void *ctx;
  pangu_status status;
  uint64_t start, activity;
  int exited, code;
} process;
static void close_handle(uv_handle_t *h) {
  if (!uv_is_closing(h)) uv_close(h, NULL);
}
static void stop_tree(process *p) {
  if (p->child.pid > 0) (void)kill(-p->child.pid, SIGKILL);
}
static void allocate(uv_handle_t *h, size_t suggested, uv_buf_t *b) {
  (void)h; (void)suggested;
  b->base = malloc(65536); b->len = b->base ? 65536 : 0;
}
static void read_output(uv_stream_t *s, ssize_t n, const uv_buf_t *b) {
  process *p = s->data;
  if (n > 0) {
    p->activity = uv_now(&p->loop);
    if (!p->status) p->status = p->emit(p->ctx, (pangu_slice){b->base, (size_t)n});
    if (p->status) stop_tree(p);
  } else if (n < 0) {
    if (n != UV_EOF && !p->status) p->status = PANGU_TOOL_ERROR;
    close_handle((uv_handle_t *)s);
  }
  free(b->base);
}
static void exited(uv_process_t *child, int64_t code, int sig) {
  process *p = child->data;
  p->exited = 1; p->code = sig ? 128 + sig : (int)code;
  p->activity = uv_now(&p->loop);
  close_handle((uv_handle_t *)child);
}
static void tick(uv_timer_t *timer) {
  process *p = timer->data;
  if (p->request->poll) p->request->poll(p->request->poll_ctx);
  if (!p->status && pangu_core_is_cancelled(p->owner)) p->status = PANGU_CANCELLED;
  if (!p->status && p->request->timeout_ms &&
      uv_now(&p->loop)-p->start >= p->request->timeout_ms) p->status = PANGU_TOOL_ERROR;
  if (p->status) stop_tree(p);
  /* Pi #5303: idle grace resets on every chunk after parent exit. */
  if (p->exited && (p->status || uv_now(&p->loop)-p->activity >= 100 ||
      (uv_is_closing((uv_handle_t *)&p->pipes[0]) && uv_is_closing((uv_handle_t *)&p->pipes[1])))) {
    close_handle((uv_handle_t *)&p->pipes[0]);
    close_handle((uv_handle_t *)&p->pipes[1]);
    close_handle((uv_handle_t *)&p->timer);
  }
}
static pangu_status native_run(void *ctx, pangu_core *owner, const pg_process_request *r,
                               pg_process_output emit, void *out, int *code) {
  (void)ctx;
  if (!r || !r->argv || !r->argv[0] || !r->cwd || !emit || !code) return PANGU_INVALID;
  if (pangu_core_is_cancelled(owner)) return PANGU_CANCELLED;
  process p = {.owner=owner, .request=r, .emit=emit, .ctx=out};
  if (uv_loop_init(&p.loop)) return PANGU_TOOL_ERROR;
  int initialized = 0;
  for (int i=0; i<2; ++i) {
    if (uv_pipe_init(&p.loop, &p.pipes[i], 0)) goto fail;
    p.pipes[i].data = &p; ++initialized;
  }
  if (uv_timer_init(&p.loop, &p.timer)) goto fail;
  ++initialized; p.timer.data = &p;
  uv_stdio_container_t io[3] = {{.flags=UV_IGNORE},
    {.flags=UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream=(uv_stream_t *)&p.pipes[0]},
    {.flags=UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream=(uv_stream_t *)&p.pipes[1]}};
  uv_process_options_t options = {.file=r->argv[0], .args=(char **)r->argv,
    .cwd=r->cwd, .env=(char **)r->env, .flags=UV_PROCESS_DETACHED,
    .stdio_count=3, .stdio=io, .exit_cb=exited};
  p.child.data = &p;
  int rc = uv_spawn(&p.loop, &p.child, &options);
  if (rc) { close_handle((uv_handle_t *)&p.child); goto fail; }
  p.start = p.activity = uv_now(&p.loop);
  for (int i=0; i<2; ++i) if (uv_read_start((uv_stream_t *)&p.pipes[i], allocate, read_output)) {
    p.status = PANGU_TOOL_ERROR; stop_tree(&p);
  }
  uv_timer_start(&p.timer, tick, 0, 10);
  uv_run(&p.loop, UV_RUN_DEFAULT);
  uv_loop_close(&p.loop);
  *code = p.code;
  return p.status;
fail:
  for (int i=0; i<initialized && i<2; ++i) close_handle((uv_handle_t *)&p.pipes[i]);
  if (initialized==3) close_handle((uv_handle_t *)&p.timer);
  uv_run(&p.loop, UV_RUN_DEFAULT); uv_loop_close(&p.loop);
  return PANGU_TOOL_ERROR;
}
#else
static pangu_status native_run(void *ctx, pangu_core *owner, const pg_process_request *r,
                               pg_process_output emit, void *out, int *code) {
  (void)ctx; (void)owner; (void)r; (void)emit; (void)out; (void)code;
  return PANGU_TOOL_ERROR;
}
#endif
pg_execution_environment pg_local_execution_environment(void) {
  return (pg_execution_environment){NULL, native_run};
}
