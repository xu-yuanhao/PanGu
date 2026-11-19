#include <pangu/services/host.h>
#include <stdio.h>
typedef struct {
  int (*run)(void *);
} driver;
static int run(void *ctx) { return *(int *)ctx; }
static const driver table = {run};
static int core = 17, durable = 23;
static pg_status install(pg_scope *s, const void *cfg, size_t len) {
  if (len != sizeof(int))
    return PG_INVALID;
  return pg_service_register(s, PG_NAME("execution.driver"), 1, &table,
                             (void *)cfg);
}
int main(void) {
  pg_host *h = NULL;
  pg_scope *a = NULL, *b = NULL;
  pg_lease *l = NULL;
  pg_plugin p = {
      PANGU_SERVICES_ABI, sizeof(p), PG_NAME("core-driver"), NULL, 0, install};
  if (pg_host_create(&h) ||
      pg_plugin_install(pg_host_root(h), &p, &core, sizeof(core), &a))
    return 1;
  p.name = PG_NAME("durable-driver");
  if (pg_plugin_install(pg_host_root(h), &p, &durable, sizeof(durable), &b))
    return 2;
  if (pg_service_acquire(pg_host_root(h), PG_NAME("execution.driver"), 1, &l))
    return 3;
  int result = ((const driver *)pg_lease_table(l))->run(pg_lease_context(l));
  pg_lease_release(l);
  if (result != 23 || pg_scope_dispose(b))
    return 4;
  if (pg_service_acquire(pg_host_root(h), PG_NAME("execution.driver"), 1, &l))
    return 5;
  result = ((const driver *)pg_lease_table(l))->run(pg_lease_context(l));
  pg_lease_release(l);
  pg_host_destroy(h);
  puts("driver replacement: durable=23, restored core=17");
  return result != 17;
}
