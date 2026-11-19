#include "resource-ignore.h"
#include <ctype.h>
#include <fcntl.h>
#include <git2.h>
#include <git2/sys/config.h>
#include <git2/sys/repository.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
/* Minimal immutable config capability, not a pattern parser. libgit2 owns all
 * ignore semantics. Avoid unexported memory-backend constructors. */
static void config_entry_free(git_config_backend_entry *entry) { free(entry); }
static int config_open(git_config_backend *b, git_config_level_t level,
                       const git_repository *repo) {
  (void)b;
  (void)level;
  (void)repo;
  return 0;
}
static int config_get(git_config_backend *b, const char *key,
                      git_config_backend_entry **out) {
  (void)b;
  *out = NULL;
  if (strcmp(key, "core.ignorecase"))
    return GIT_ENOTFOUND;
  git_config_backend_entry *e = calloc(1, sizeof(*e));
  if (!e)
    return -1;
  e->entry.name = "core.ignorecase";
  e->entry.value = "true";
  e->entry.level = GIT_CONFIG_LEVEL_LOCAL;
  e->free = config_entry_free;
  *out = e;
  return 0;
}
static int iterator_next(git_config_backend_entry **out,
                         git_config_iterator *it) {
  (void)it;
  *out = NULL;
  return GIT_ITEROVER;
}
static void iterator_free(git_config_iterator *it) { free(it); }
static int config_iterator(git_config_iterator **out, git_config_backend *b) {
  *out = calloc(1, sizeof(**out));
  if (!*out)
    return -1;
  (*out)->backend = b;
  (*out)->next = iterator_next;
  (*out)->free = iterator_free;
  return 0;
}
static void config_free(git_config_backend *b) { free(b); }
static int config_snapshot(git_config_backend **out, git_config_backend *b) {
  *out = malloc(sizeof(**out));
  if (!*out)
    return -1;
  **out = *b;
  (*out)->cfg = NULL;
  return 0;
}
static int memory_config(git_config_backend **out) {
  *out = calloc(1, sizeof(**out));
  if (!*out)
    return -1;
  **out = (git_config_backend){.version = GIT_CONFIG_BACKEND_VERSION,
                               .readonly = 1,
                               .open = config_open,
                               .get = config_get,
                               .iterator = config_iterator,
                               .snapshot = config_snapshot,
                               .free = config_free};
  return 0;
}
struct pg_resource_ignore {
  git_repository *repo;
  const char *root;
  size_t length;
};
void pg_resource_ignore_destroy(pg_resource_ignore *m) {
  if (m) {
    git_repository_free(m->repo);
    free(m);
    git_libgit2_shutdown();
  }
}
pg_status pg_resource_ignore_create(const char *root,
                                    pg_resource_ignore **out) {
  *out = NULL;
  if (git_libgit2_init() < 0)
    return PG_FAILED;
  pg_resource_ignore *m = calloc(1, sizeof(*m));
  if (!m) {
    git_libgit2_shutdown();
    return PG_NOMEM;
  }
  m->root = root;
  m->length = strlen(root);
  while (m->length > 1 && root[m->length - 1] == '/')
    --m->length;
#ifdef GIT_EXPERIMENTAL_SHA256
  int rc = git_repository_new(&m->repo, NULL);
#else
  int rc = git_repository_new(&m->repo);
#endif
  git_config *config = NULL;
  if (!rc)
    rc = git_config_new(&config);
  /* An in-memory config prevents user global excludes from changing the source
   * skill-root contract. npm ignore() defaults to case-insensitive matching. */
  git_config_backend *backend = NULL;
  if (!rc)
    rc = memory_config(&backend);
  if (!rc) {
    rc = git_config_add_backend(config, backend, GIT_CONFIG_LEVEL_LOCAL,
                                m->repo, 0);
    if (!rc)
      backend = NULL; /* config owns the successfully added backend */
  }
  if (backend)
    backend->free(backend);
  if (!rc)
    rc = git_repository_set_config(m->repo, config);
  git_config_free(config);
  if (!rc)
    rc = git_repository_set_workdir(m->repo, root, 0);
  if (rc) {
    pg_resource_ignore_destroy(m);
    return PG_FAILED;
  }
  *out = m;
  return PG_OK;
}
static pg_status rules_file(pg_resource_ignore *m, const char *dir,
                            const char *filename) {
  size_t dirlen = strlen(dir), fnlen = strlen(filename);
  if (dirlen > SIZE_MAX - fnlen - 2)
    return PG_NOMEM;
  char *path = malloc(dirlen + fnlen + 2);
  if (!path)
    return PG_NOMEM;
  memcpy(path, dir, dirlen);
  path[dirlen] = '/';
  memcpy(path + dirlen + 1, filename, fnlen + 1);
  uv_fs_t req;
  int fd = uv_fs_open(NULL, &req, path, O_RDONLY, 0, NULL);
  uv_fs_req_cleanup(&req);
  free(path);
  if (fd < 0)
    return PG_OK; /* source ignores unreadable ignore files */
  char *data = NULL;
  size_t size = 0, cap = 0;
  pg_status st = PG_OK;
  for (;;) {
    char buf[16384];
    uv_buf_t b = uv_buf_init(buf, sizeof(buf));
    int n = uv_fs_read(NULL, &req, fd, &b, 1, -1, NULL);
    uv_fs_req_cleanup(&req);
    if (n <= 0)
      break;
    if ((size_t)n > SIZE_MAX - size - 1) {
      st = PG_NOMEM;
      break;
    }
    if (size + (size_t)n + 1 > cap) {
      cap = size + (size_t)n + 1;
      char *next = realloc(data, cap);
      if (!next) {
        st = PG_NOMEM;
        break;
      }
      data = next;
    }
    memcpy(data + size, buf, (size_t)n);
    size += (size_t)n;
    data[size] = 0;
  }
  uv_fs_close(NULL, &req, fd, NULL);
  uv_fs_req_cleanup(&req);
  const char *relative = dir + m->length;
  if (*relative == '/')
    ++relative;
  size_t prefix = strlen(relative);
  for (size_t start = 0; start < size && !st;) {
    size_t end = start;
    while (end < size && data[end] != '\n')
      ++end;
    size_t n = end - start;
    if (n && data[start + n - 1] == '\r')
      --n;
    const char *line = data + start;
    start = end < size ? end + 1 : end;
    size_t first = 0;
    while (first < n && isspace((unsigned char)line[first]))
      ++first;
    if (first == n || line[first] == '#')
      continue;
    int negated = line[0] == '!';
    if (negated) {
      ++line;
      --n;
    }
    /* Preserve escaped !/# as literal patterns for libgit2. */
    if (n && line[0] == '/') {
      ++line;
      --n;
    }
    if (n > SIZE_MAX - prefix - 4) {
      st = PG_NOMEM;
      break;
    }
    char *rule = malloc(n + prefix + 4);
    if (!rule) {
      st = PG_NOMEM;
      break;
    }
    size_t at = 0;
    if (negated)
      rule[at++] = '!';
    if (prefix) {
      memcpy(rule + at, relative, prefix);
      at += prefix;
      rule[at++] = '/';
    }
    memcpy(rule + at, line, n);
    at += n;
    rule[at++] = '\n';
    rule[at] = 0;
    if (git_ignore_add_rule(m->repo, rule))
      st = PG_FAILED;
    free(rule);
  }
  free(data);
  return st;
}
pg_status pg_resource_ignore_directory(pg_resource_ignore *m, const char *dir) {
  static const char *files[] = {".gitignore", ".ignore", ".fdignore"};
  for (size_t i = 0; i < 3; ++i) {
    pg_status st = rules_file(m, dir, files[i]);
    if (st)
      return st;
  }
  return PG_OK;
}
pg_status pg_resource_ignore_test(pg_resource_ignore *m, const char *path,
                                  int *ignored) {
  const char *relative = path + m->length;
  if (*relative == '/')
    ++relative;
  return git_ignore_path_is_ignored(ignored, m->repo, relative) ? PG_FAILED
                                                                : PG_OK;
}
