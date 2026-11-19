#include <pangu/services/host.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "failed line %d: %s\n", __LINE__, #x);                   \
      abort();                                                                 \
    }                                                                          \
  } while (0)
static int table = 42, seq[16], count;
static void cleanup(void *p) { seq[count++] = *(int *)p; }
static pg_status provide(pg_scope *s, const void *cfg, size_t n) {
  CHECK(n == sizeof(int));
  CHECK(pg_scope_defer(s, cleanup, (void *)cfg) == PG_OK);
  return pg_service_register(s, PG_NAME("model"), 1, &table, (void *)cfg);
}
static pg_status fail(pg_scope *s, const void *cfg, size_t n) {
  CHECK(provide(s, cfg, n) == PG_OK);
  return PG_FAILED;
}
static pg_status consume(pg_scope *s, const void *cfg, size_t n) {
  (void)n;
  CHECK(*(const int *)pg_lease_table(pg_scope_dependency(s, 0)) == 42);
  return pg_scope_defer(s, cleanup, (void *)cfg);
}
static void event(void *p, pg_name n, const void *data, size_t len) {
  (void)n;
  CHECK(len == sizeof(int));
  *(int *)p += *(const int *)data;
}
typedef struct {
  pg_lease *lease;
  uv_sem_t ready;
} work;
static void release_worker(void *p) {
  work *w = p;
  uv_sem_wait(&w->ready);
  pg_lease_release(w->lease);
}
static void unblock(void *p) {
  work *w = p;
  uv_sem_post(&w->ready);
}
int main(void) {
  pg_host *h = NULL;
  CHECK(pg_host_create(&h) == PG_OK);
  pg_scope *r = pg_host_root(h), *a = NULL, *b = NULL, *c = NULL;
  int one = 1, two = 2, three = 3;
  pg_plugin p = {1, sizeof(p), PG_NAME("provider"), NULL, 0, provide};
  CHECK(pg_plugin_install(r, &p, &one, sizeof(one), &a) == PG_OK);
  pg_lease *old = NULL, *now = NULL;
  CHECK(pg_service_acquire(r, PG_NAME("model"), 1, &old) == PG_OK);
  CHECK(pg_plugin_install(r, &p, &two, sizeof(two), &b) == PG_OK);
  CHECK(pg_service_acquire(r, PG_NAME("model"), 1, &now) == PG_OK);
  CHECK(*(int *)pg_lease_context(now) == 2);
  CHECK(*(int *)pg_lease_context(old) == 1);
  pg_lease_release(now);
  pg_lease_release(old);
  pg_dependency d = {PG_NAME("model"), 1};
  pg_plugin consumer = {1, sizeof(consumer), PG_NAME("consumer"), &d,
                        1, consume};
  CHECK(pg_plugin_install(r, &consumer, &three, sizeof(three), &c) == PG_OK);
  CHECK(pg_scope_dispose(b) == PG_OK);
  CHECK(count == 2 && seq[0] == 3 && seq[1] == 2);
  CHECK(pg_scope_dispose(b) == PG_OK);
  p.apply = fail;
  CHECK(pg_plugin_install(r, &p, &two, sizeof(two), &b) == PG_FAILED &&
        b == NULL);
  CHECK(seq[2] == 2);
  CHECK(pg_service_acquire(r, PG_NAME("model"), 2, &now) == PG_ABI &&
        now == NULL);
  CHECK(pg_scope_create(r, &b) == PG_OK);
  int hits = 0, childhits = 0;
  CHECK(pg_event_on(r, PG_NAME("tick"), event, &hits) == PG_OK);
  CHECK(pg_event_on(b, PG_NAME("tick"), event, &childhits) == PG_OK);
  CHECK(pg_event_emit(b, PG_NAME("tick"), &one, sizeof(one)) == PG_OK);
  CHECK(hits == 1 && childhits == 1);
  CHECK(pg_event_emit(r, PG_NAME("tick"), &one, sizeof(one)) == PG_OK);
  CHECK(hits == 2 && childhits == 1);
  CHECK(pg_scope_dispose(b) == PG_OK);
  CHECK(pg_event_emit(b, PG_NAME("tick"), NULL, 0) == PG_CLOSED);
  /* A child cleanup starts the worker release while provider teardown waits. */
  work w;
  CHECK(uv_sem_init(&w.ready, 0) == 0);
  CHECK(pg_service_acquire(r, PG_NAME("model"), 1, &w.lease) == PG_OK);
  uv_thread_t thread;
  CHECK(uv_thread_create(&thread, release_worker, &w) == 0);
  CHECK(pg_scope_create(a, &b) == PG_OK);
  CHECK(pg_scope_defer(b, unblock, &w) == PG_OK);
  CHECK(pg_scope_dispose(a) == PG_OK);
  CHECK(uv_thread_join(&thread) == 0);
  uv_sem_destroy(&w.ready);
  CHECK(pg_service_acquire(r, PG_NAME("model"), 1, &now) == PG_NOT_FOUND);
  CHECK(seq[count - 1] == 1);
  pg_host_destroy(h);
  puts("services host: replacement, dependency drain, rollback, ABI, events, "
       "cross-thread release passed");
  return 0;
}
