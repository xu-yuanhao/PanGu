/* Native scope/lease adaptation of Cordis fiber effects and Harness scope.
 * Sources and adopted revisions: examples/services/README.md. */
#include <pangu/services/host.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
typedef struct binding binding;
typedef struct effect effect;
typedef struct listener listener;
struct binding {
  binding *next;
  pg_scope *owner;
  pg_name name;
  uint32_t abi;
  const void *table;
  void *ctx;
};
struct effect {
  effect *next;
  pg_cleanup_fn fn;
  void *ctx;
};
struct listener {
  listener *next;
  pg_scope *owner;
  pg_name name;
  pg_event_fn fn;
  void *ctx;
};
struct pg_scope {
  pg_host *host;
  pg_scope *parent, *next, *publication;
  int state;
  size_t users;
  effect *effects;
  pg_lease **deps;
  size_t dep_count;
};
struct pg_host {
  uv_mutex_t mutex;
  uv_cond_t cond;
  pg_scope *scopes, *root;
  binding *bindings;
  listener *listeners, *tail;
  size_t dispatches;
};
struct pg_lease {
  binding *binding;
};
static int valid(pg_name n) { return n.data && n.size; }
static int equal(pg_name a, pg_name b) {
  return a.size == b.size && !memcmp(a.data, b.data, a.size);
}
static int within(pg_scope *s, pg_scope *ancestor) {
  for (; s; s = s->parent)
    if (s == ancestor)
      return 1;
  return 0;
}
static int active(pg_scope *s) {
  for (; s; s = s->parent)
    if (s->state)
      return 0;
  return 1;
}
static char *copy_name(pg_name n) {
  char *p = malloc(n.size);
  if (p)
    memcpy(p, n.data, n.size);
  return p;
}
pg_status pg_host_create(pg_host **out) {
  if (!out)
    return PG_INVALID;
  *out = NULL;
  pg_host *h = calloc(1, sizeof(*h));
  if (!h)
    return PG_NOMEM;
  if (uv_mutex_init(&h->mutex)) {
    free(h);
    return PG_FAILED;
  }
  if (uv_cond_init(&h->cond)) {
    uv_mutex_destroy(&h->mutex);
    free(h);
    return PG_FAILED;
  }
  pg_scope *s = calloc(1, sizeof(*s));
  if (!s) {
    uv_cond_destroy(&h->cond);
    uv_mutex_destroy(&h->mutex);
    free(h);
    return PG_NOMEM;
  }
  s->host = h;
  h->scopes = h->root = s;
  *out = h;
  return PG_OK;
}
pg_scope *pg_host_root(pg_host *h) { return h ? h->root : NULL; }
pg_status pg_scope_create(pg_scope *p, pg_scope **out) {
  if (!out)
    return PG_INVALID;
  *out = NULL;
  if (!p)
    return PG_INVALID;
  if (!active(p))
    return PG_CLOSED;
  pg_scope *s = calloc(1, sizeof(*s));
  if (!s)
    return PG_NOMEM;
  s->host = p->host;
  s->parent = p;
  s->next = p->host->scopes;
  p->host->scopes = s;
  *out = s;
  return PG_OK;
}
pg_status pg_scope_defer(pg_scope *s, pg_cleanup_fn fn, void *ctx) {
  if (!s || !fn)
    return PG_INVALID;
  if (!active(s))
    return PG_CLOSED;
  effect *e = malloc(sizeof(*e));
  if (!e)
    return PG_NOMEM;
  *e = (effect){s->effects, fn, ctx};
  s->effects = e;
  return PG_OK;
}
pg_status pg_service_register(pg_scope *s, pg_name n, uint32_t abi,
                              const void *table, void *ctx) {
  if (!s || !valid(n) || !abi || !table)
    return PG_INVALID;
  if (!active(s))
    return PG_CLOSED;
  binding *b = calloc(1, sizeof(*b));
  if (!b)
    return PG_NOMEM;
  b->name = (pg_name){copy_name(n), n.size};
  if (!b->name.data) {
    free(b);
    return PG_NOMEM;
  }
  b->owner = s;
  b->abi = abi;
  b->table = table;
  b->ctx = ctx;
  b->next = s->host->bindings;
  s->host->bindings = b;
  return PG_OK;
}
pg_status pg_service_acquire(pg_scope *s, pg_name n, uint32_t abi,
                             pg_lease **out) {
  if (!out)
    return PG_INVALID;
  *out = NULL;
  if (!s || !valid(n) || !abi)
    return PG_INVALID;
  if (!active(s))
    return PG_CLOSED;
  binding *b = NULL;
  for (pg_scope *p = s; p && !b; p = p->parent)
    for (binding *q = s->host->bindings; q; q = q->next)
      if ((q->owner->publication ? q->owner->publication : q->owner) == p &&
          active(q->owner) && equal(n, q->name)) {
        b = q;
        break;
      }
  if (!b)
    return PG_NOT_FOUND;
  if (b->abi != abi)
    return PG_ABI;
  pg_lease *l = malloc(sizeof(*l));
  if (!l)
    return PG_NOMEM;
  l->binding = b;
  uv_mutex_lock(&s->host->mutex);
  b->owner->users++;
  uv_mutex_unlock(&s->host->mutex);
  *out = l;
  return PG_OK;
}
const void *pg_lease_table(const pg_lease *l) {
  return l ? l->binding->table : NULL;
}
void *pg_lease_context(const pg_lease *l) { return l ? l->binding->ctx : NULL; }
void pg_lease_release(pg_lease *l) {
  if (!l)
    return;
  pg_scope *s = l->binding->owner;
  uv_mutex_lock(&s->host->mutex);
  s->users--;
  uv_cond_broadcast(&s->host->cond);
  uv_mutex_unlock(&s->host->mutex);
  free(l);
}
pg_status pg_scope_dispose(pg_scope *s) {
  if (!s)
    return PG_INVALID;
  if (s->state == 2)
    return PG_OK;
  if (s->state == 1 || s->host->dispatches)
    return PG_FAILED;
  s->state = 1;
  /* Drain injected consumers before the resources they borrow. */
  for (pg_scope *c = s->host->scopes; c; c = c->next) {
    if (c->state || within(c, s))
      continue;
    for (size_t i = 0; i < c->dep_count; i++)
      if (within(c->deps[i]->binding->owner, s)) {
        pg_scope_dispose(c);
        break;
      }
  }
  for (pg_scope *c = s->host->scopes; c; c = c->next)
    if (c->parent == s)
      pg_scope_dispose(c);
  uv_mutex_lock(&s->host->mutex);
  while (s->users)
    uv_cond_wait(&s->host->cond, &s->host->mutex);
  uv_mutex_unlock(&s->host->mutex);
  while (s->effects) {
    effect *e = s->effects;
    s->effects = e->next;
    e->fn(e->ctx);
    free(e);
  }
  for (size_t i = s->dep_count; i; i--)
    pg_lease_release(s->deps[i - 1]);
  free(s->deps);
  s->deps = NULL;
  s->dep_count = 0;
  binding **bp = &s->host->bindings;
  while (*bp) {
    binding *b = *bp;
    if (b->owner == s) {
      *bp = b->next;
      free((void *)b->name.data);
      free(b);
    } else
      bp = &b->next;
  }
  listener **lp = &s->host->listeners;
  s->host->tail = NULL;
  while (*lp) {
    listener *l = *lp;
    if (l->owner == s) {
      *lp = l->next;
      free((void *)l->name.data);
      free(l);
    } else {
      s->host->tail = l;
      lp = &l->next;
    }
  }
  s->state = 2;
  return PG_OK;
}
const pg_lease *pg_scope_dependency(const pg_scope *s, size_t i) {
  return s && i < s->dep_count ? s->deps[i] : NULL;
}
pg_status pg_plugin_install(pg_scope *p, const pg_plugin *d, const void *cfg,
                            size_t len, pg_scope **out) {
  if (!out)
    return PG_INVALID;
  *out = NULL;
  if (!p || !d || d->size < sizeof(*d) || !d->apply || !valid(d->name) ||
      (!cfg && len) || (!d->dependencies && d->dependency_count))
    return PG_INVALID;
  if (d->abi != PANGU_SERVICES_ABI)
    return PG_ABI;
  pg_scope *s = NULL;
  pg_status st = pg_scope_create(p, &s);
  if (st)
    return st;
  s->publication = p;
  if (d->dependency_count) {
    s->deps = calloc(d->dependency_count, sizeof(*s->deps));
    if (!s->deps) {
      pg_scope_dispose(s);
      return PG_NOMEM;
    }
  }
  for (size_t i = 0; i < d->dependency_count; i++) {
    st = pg_service_acquire(p, d->dependencies[i].name, d->dependencies[i].abi,
                            &s->deps[i]);
    if (st) {
      pg_scope_dispose(s);
      return st;
    }
    s->dep_count++;
  }
  st = d->apply(s, cfg, len);
  if (st) {
    pg_scope_dispose(s);
    return st;
  }
  *out = s;
  return PG_OK;
}
pg_status pg_event_on(pg_scope *s, pg_name n, pg_event_fn fn, void *ctx) {
  if (!s || !valid(n) || !fn)
    return PG_INVALID;
  if (!active(s))
    return PG_CLOSED;
  listener *l = calloc(1, sizeof(*l));
  if (!l)
    return PG_NOMEM;
  l->name = (pg_name){copy_name(n), n.size};
  if (!l->name.data) {
    free(l);
    return PG_NOMEM;
  }
  l->owner = s;
  l->fn = fn;
  l->ctx = ctx;
  if (s->host->tail)
    s->host->tail->next = l;
  else
    s->host->listeners = l;
  s->host->tail = l;
  return PG_OK;
}
pg_status pg_event_emit(pg_scope *s, pg_name n, const void *data, size_t len) {
  if (!s || !valid(n) || (!data && len))
    return PG_INVALID;
  if (!active(s))
    return PG_CLOSED;
  pg_host *h = s->host;
  listener *end = h->tail;
  h->dispatches++;
  for (listener *l = h->listeners; l; l = l->next) {
    if (active(l->owner) && within(s, l->owner) && equal(n, l->name))
      l->fn(l->ctx, n, data, len);
    if (l == end)
      break;
  }
  h->dispatches--;
  return PG_OK;
}
void pg_host_destroy(pg_host *h) {
  if (!h)
    return;
  if (pg_scope_dispose(h->root) != PG_OK)
    return;
  while (h->bindings) {
    binding *b = h->bindings;
    h->bindings = b->next;
    free((void *)b->name.data);
    free(b);
  }
  while (h->listeners) {
    listener *l = h->listeners;
    h->listeners = l->next;
    free((void *)l->name.data);
    free(l);
  }
  while (h->scopes) {
    pg_scope *s = h->scopes;
    h->scopes = s->next;
    free(s);
  }
  uv_cond_destroy(&h->cond);
  uv_mutex_destroy(&h->mutex);
  free(h);
}
