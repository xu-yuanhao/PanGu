#include "resource-ignore.h"
#include <ctype.h>
#include <fcntl.h>
#include <pangu/services/resources.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <uv.h>
#include <yaml.h>
#include <yyjson.h>
typedef struct {
  char *data;
  size_t len, cap;
} buffer;
static int add(buffer *b, const char *s, size_t n) {
  if (n > SIZE_MAX - b->len - 1)
    return 0;
  size_t need = b->len + n + 1;
  if (need > b->cap) {
    size_t cap = need > SIZE_MAX / 2 ? need : need * 2;
    char *p = realloc(b->data, cap);
    if (!p)
      return 0;
    b->data = p;
    b->cap = cap;
  }
  if (n)
    memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = 0;
  return 1;
}
static int str(buffer *b, const char *s) { return add(b, s, strlen(s)); }
static char *copy(const char *s, size_t n) {
  char *p = malloc(n + 1);
  if (p) {
    if (n)
      memcpy(p, s, n);
    p[n] = 0;
  }
  return p;
}
static char *join(const char *a, const char *b) {
  buffer out = {0};
  if (!str(&out, a) || !str(&out, "/") || !str(&out, b)) {
    free(out.data);
    return NULL;
  }
  return out.data;
}
static pg_status read_file(const char *path, char **out, size_t *size) {
  *out = NULL;
  *size = 0;
  uv_fs_t req;
  int fd = uv_fs_open(NULL, &req, path, O_RDONLY, 0, NULL);
  uv_fs_req_cleanup(&req);
  if (fd < 0)
    return fd == UV_ENOENT ? PG_NOT_FOUND : PG_FAILED;
  buffer b = {0};
  pg_status st = PG_OK;
  for (;;) {
    char chunk[16384];
    uv_buf_t v = uv_buf_init(chunk, sizeof(chunk));
    int n = uv_fs_read(NULL, &req, fd, &v, 1, -1, NULL);
    uv_fs_req_cleanup(&req);
    if (n < 0) {
      st = PG_FAILED;
      break;
    }
    if (!n)
      break;
    if (!add(&b, chunk, (size_t)n)) {
      st = PG_NOMEM;
      break;
    }
  }
  int rc = uv_fs_close(NULL, &req, fd, NULL);
  uv_fs_req_cleanup(&req);
  if (rc < 0)
    st = PG_FAILED;
  if (!st && b.len >= 3 && !memcmp(b.data, "\xef\xbb\xbf", 3)) {
    memmove(b.data, b.data + 3, b.len - 3);
    b.len -= 3;
    b.data[b.len] = 0;
  }
  if (!st && b.len && memchr(b.data, 0, b.len))
    st = PG_INVALID;
  if (st) {
    free(b.data);
    return st;
  }
  if (!b.data)
    b.data = copy("", 0);
  if (!b.data)
    return PG_NOMEM;
  *out = b.data;
  *size = b.len;
  return PG_OK;
}
struct pg_resource_snapshot {
  atomic_size_t refs;
  char *settings, *system;
  buffer diagnostics;
  size_t settings_len, system_len;
  char **visited;
  size_t visited_count;
  pg_resource *items;
  size_t count;
};
typedef struct {
  pg_resources_service service;
  pg_credentials_service credentials;
  char *user, *cwd, *project, *overrides;
  int trusted;
  pg_resource_snapshot *current;
} resources;
void pg_resource_snapshot_release(pg_resource_snapshot *s) {
  if (!s || atomic_fetch_sub(&s->refs, 1) != 1)
    return;
  for (size_t i = 0; i < s->count; ++i) {
    pg_resource *r = &s->items[i];
    free((void *)r->name.data);
    free((void *)r->description.data);
    free((void *)r->path.data);
    free((void *)r->body.data);
  }
  for (size_t i = 0; i < s->visited_count; ++i)
    free(s->visited[i]);
  free(s->visited);
  free(s->items);
  free(s->diagnostics.data);
  free(s->settings);
  free(s->system);
  free(s);
}
pangu_slice pg_resource_settings(const pg_resource_snapshot *s) {
  return s ? (pangu_slice){s->settings, s->settings_len} : (pangu_slice){0};
}
pangu_slice pg_resource_system_prompt(const pg_resource_snapshot *s) {
  return s ? (pangu_slice){s->system, s->system_len} : (pangu_slice){0};
}
pangu_slice pg_resource_diagnostics(const pg_resource_snapshot *s) {
  return s ? (pangu_slice){s->diagnostics.data, s->diagnostics.len}
           : (pangu_slice){0};
}
static pg_status diagnostic(pg_resource_snapshot *s, const char *path,
                            const char *message) {
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return PG_NOMEM;
  yyjson_mut_val *o = yyjson_mut_obj(d);
  yyjson_mut_doc_set_root(d, o);
  int ok = o && yyjson_mut_obj_add_strcpy(d, o, "path", path) &&
           yyjson_mut_obj_add_strcpy(d, o, "message", message);
  size_t n = 0;
  char *text = ok ? yyjson_mut_write(d, 0, &n) : NULL;
  if (s->diagnostics.len && s->diagnostics.data[s->diagnostics.len - 1] == ']')
    --s->diagnostics.len;
  if (s->diagnostics.len > 1)
    ok = ok && str(&s->diagnostics, ",");
  ok = ok && text && add(&s->diagnostics, text, n) && str(&s->diagnostics, "]");
  free(text);
  yyjson_mut_doc_free(d);
  return ok ? PG_OK : PG_NOMEM;
}
size_t pg_resource_count(const pg_resource_snapshot *s) {
  return s ? s->count : 0;
}
const pg_resource *pg_resource_at(const pg_resource_snapshot *s, size_t i) {
  return s && i < s->count ? &s->items[i] : NULL;
}
static yyjson_mut_val *merge(yyjson_mut_doc *d, yyjson_mut_val *base,
                             yyjson_val *layer) {
  if (!yyjson_is_obj(layer))
    return NULL;
  size_t i, n;
  yyjson_val *key, *value;
  yyjson_obj_foreach(layer, i, n, key, value) {
    const char *k = yyjson_get_str(key);
    yyjson_mut_val *old = yyjson_mut_obj_get(base, k), *next;
    if (yyjson_is_obj(value) && yyjson_mut_is_obj(old))
      next = merge(d, old, value);
    else
      next = yyjson_val_mut_copy(d, value);
    yyjson_mut_val *name = yyjson_mut_strcpy(d, k);
    if (!next || !name || !yyjson_mut_obj_put(base, name, next))
      return NULL;
  }
  return base;
}
static yyjson_mut_val *merge_settings(yyjson_mut_doc *d, yyjson_mut_val *base,
                                      yyjson_val *layer) {
  yyjson_mut_val *old = yyjson_mut_obj_get(base, "defaultTools");
  yyjson_val *tools = yyjson_obj_get(layer, "defaultTools");
  int modifiers = yyjson_is_arr(tools);
  size_t i, n;
  yyjson_val *v;
  yyjson_arr_foreach(tools, i, n, v) {
    const char *text = yyjson_get_str(v);
    if (!text || (*text != '+' && *text != '-'))
      modifiers = 0;
  }
  if (!merge(d, base, layer))
    return NULL;
  if (modifiers && yyjson_mut_is_arr(old)) {
    yyjson_mut_val *next = yyjson_mut_arr(d);
    if (!next)
      return NULL;
    size_t size = yyjson_mut_arr_size(old);
    for (i = 0; i < size; ++i) {
      yyjson_mut_val *item =
          yyjson_mut_val_mut_copy(d, yyjson_mut_arr_get(old, i));
      if (!item || !yyjson_mut_arr_append(next, item))
        return NULL;
    }
    yyjson_arr_foreach(tools, i, n, v) {
      yyjson_mut_val *item = yyjson_val_mut_copy(d, v);
      if (!item || !yyjson_mut_arr_append(next, item))
        return NULL;
    }
    if (!yyjson_mut_obj_put(base, yyjson_mut_str(d, "defaultTools"), next))
      return NULL;
  }
  return base;
}
static int normalize_tools(yyjson_mut_doc *d, yyjson_mut_val *root) {
  yyjson_mut_val *input = yyjson_mut_obj_get(root, "defaultTools");
  if (!yyjson_mut_is_arr(input))
    return 1;
  size_t n = yyjson_mut_arr_size(input);
  int has_plain = 0;
  for (size_t i = 0; i < n; ++i) {
    const char *v = yyjson_mut_get_str(yyjson_mut_arr_get(input, i));
    if (v && *v != '+' && *v != '-')
      has_plain = 1;
  }
  const char **names = calloc(n + 4, sizeof(*names));
  if (!names)
    return 0;
  size_t count = 0;
  if (!has_plain && n) {
    names[count++] = "read";
    names[count++] = "bash";
    names[count++] = "edit";
    names[count++] = "write";
  }
  for (size_t i = 0; i < n; ++i) {
    const char *v = yyjson_mut_get_str(yyjson_mut_arr_get(input, i));
    if (!v)
      continue;
    int remove = *v == '-', modifier = *v == '+' || remove;
    const char *name = v + modifier;
    size_t found = count;
    for (size_t j = 0; j < count; ++j)
      if (!strcmp(names[j], name)) {
        found = j;
        break;
      }
    if (remove && found < count) {
      memmove(names + found, names + found + 1,
              (count - found - 1) * sizeof(*names));
      --count;
    } else if (!remove && found == count && *name)
      names[count++] = name;
  }
  yyjson_mut_val *out = yyjson_mut_arr(d);
  int ok = out != NULL;
  for (size_t i = 0; i < count && ok; ++i)
    ok = yyjson_mut_arr_add_strcpy(d, out, names[i]);
  free(names);
  return ok && yyjson_mut_obj_put(root, yyjson_mut_str(d, "defaultTools"), out);
}

static pg_status layer(yyjson_mut_doc *d, yyjson_mut_val *base,
                       const char *path) {
  char *data;
  size_t n;
  pg_status st = read_file(path, &data, &n);
  if (st == PG_NOT_FOUND)
    return PG_OK;
  if (st)
    return st;
  yyjson_doc *doc = yyjson_read(
      data, n, YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS);
  free(data);
  if (!doc)
    return PG_INVALID;
  st = !yyjson_is_obj(yyjson_doc_get_root(doc))            ? PG_INVALID
       : merge_settings(d, base, yyjson_doc_get_root(doc)) ? PG_OK
                                                           : PG_NOMEM;
  yyjson_doc_free(doc);
  return st;
}
static pg_status settings(resources *r, pg_resource_snapshot *s) {
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return PG_NOMEM;
  yyjson_mut_val *root = yyjson_mut_obj(d);
  yyjson_mut_doc_set_root(d, root);
  char *u = join(r->user, "settings.json"),
       *p = join(r->project, "settings.json");
  pg_status st = u && p && root ? layer(d, root, u) : PG_NOMEM;
  if (!st && r->trusted)
    st = layer(d, root, p);
  if (!st) {
    yyjson_doc *o = yyjson_read(r->overrides, strlen(r->overrides), 0);
    if (!o)
      st = PG_INVALID;
    else {
      if (!merge_settings(d, root, yyjson_doc_get_root(o)))
        st = PG_INVALID;
      yyjson_doc_free(o);
    }
  }
  if (!st) {
    if (!normalize_tools(d, root)) {
      free(u);
      free(p);
      yyjson_mut_doc_free(d);
      return PG_NOMEM;
    }
    s->settings = yyjson_mut_write(d, 0, &s->settings_len);
    if (!s->settings)
      st = PG_NOMEM;
  }
  free(u);
  free(p);
  yyjson_mut_doc_free(d);
  return st;
}
static pg_status append_resource(pg_resource_snapshot *s, pg_resource_kind kind,
                                 const char *name, const char *description,
                                 const char *path, const char *body, size_t len,
                                 int project, int disabled) {
  if (s->count == SIZE_MAX / sizeof(pg_resource))
    return PG_NOMEM;
  pg_resource *items = realloc(s->items, (s->count + 1) * sizeof(*items));
  if (!items)
    return PG_NOMEM;
  s->items = items;
  pg_resource v = {
      .kind = kind, .project = project, .disable_model_invocation = disabled};
  v.name = (pangu_slice){copy(name, strlen(name)), strlen(name)};
  v.description = (pangu_slice){copy(description, strlen(description)),
                                strlen(description)};
  v.path = (pangu_slice){copy(path, strlen(path)), strlen(path)};
  v.body = (pangu_slice){copy(body, len), len};
  if (!v.name.data || !v.description.data || !v.path.data || !v.body.data) {
    free((void *)v.name.data);
    free((void *)v.description.data);
    free((void *)v.path.data);
    free((void *)v.body.data);
    return PG_NOMEM;
  }
  /* Pi skills/prompts keep the first discovered name; preserve user precedence.
   */
  for (size_t i = 0; i < s->count; ++i)
    if (kind != PG_RESOURCE_CONTEXT && s->items[i].kind == kind &&
        s->items[i].name.len == v.name.len &&
        !memcmp(s->items[i].name.data, v.name.data, v.name.len)) {
      free((void *)v.name.data);
      free((void *)v.description.data);
      free((void *)v.path.data);
      free((void *)v.body.data);
      return diagnostic(
          s, path,
          "duplicate resource name; first discovered resource retained");
    }
  s->items[s->count++] = v;
  return PG_OK;
}
static const char *yaml_value(yaml_document_t *d, const char *name) {
  yaml_node_t *root = yaml_document_get_root_node(d);
  if (!root || root->type != YAML_MAPPING_NODE)
    return NULL;
  for (yaml_node_pair_t *p = root->data.mapping.pairs.start;
       p < root->data.mapping.pairs.top; ++p) {
    yaml_node_t *k = yaml_document_get_node(d, p->key),
                *v = yaml_document_get_node(d, p->value);
    if (k && v && k->type == YAML_SCALAR_NODE && v->type == YAML_SCALAR_NODE &&
        !strcmp((char *)k->data.scalar.value, name))
      return (char *)v->data.scalar.value;
  }
  return NULL;
}
static pg_status markdown(pg_resource_snapshot *s, const char *path,
                          const char *fallback, int skill, int project) {
  char *raw;
  size_t n;
  pg_status st = read_file(path, &raw, &n);
  if (st == PG_NOT_FOUND)
    return PG_OK;
  if (st)
    return st;
  /* YAML accepts CRLF; normalize markdown line endings before delimiter scan.
   */
  size_t normalized = 0;
  for (size_t i = 0; i < n; ++i) {
    if (raw[i] == '\r' && i + 1 < n && raw[i + 1] == '\n')
      continue;
    raw[normalized++] = raw[i];
  }
  n = normalized;
  raw[n] = 0;
  size_t start = 0;
  yaml_document_t doc;
  int loaded = 0;
  const char *name = fallback, *description = "", *disabled = "false";
  if (n >= 4 && !memcmp(raw, "---\n", 4)) {
    size_t end = 4;
    while (end + 3 < n &&
           !(raw[end] == '-' && raw[end + 1] == '-' && raw[end + 2] == '-' &&
             (raw[end + 3] == '\n' || raw[end + 3] == '\r'))) {
      while (end < n && raw[end] != '\n')
        ++end;
      ++end;
    }
    if (end + 3 >= n) {
      free(raw);
      return diagnostic(s, path, "unterminated frontmatter");
    }
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
      free(raw);
      return PG_NOMEM;
    }
    yaml_parser_set_input_string(&parser, (unsigned char *)raw + 4, end - 4);
    loaded = yaml_parser_load(&parser, &doc);
    yaml_parser_delete(&parser);
    if (!loaded) {
      free(raw);
      return diagnostic(s, path, "invalid YAML frontmatter");
    }
    const char *v = yaml_value(&doc, "name");
    if (skill && v)
      name = v;
    v = yaml_value(&doc, "description");
    if (v)
      description = v;
    v = yaml_value(&doc, "disable-model-invocation");
    if (v)
      disabled = v;
    start = end + 3;
    while (start < n && (raw[start] == '\r' || raw[start] == '\n'))
      ++start;
  }
  char *first_description = NULL;
  if (!skill && !*description) {
    size_t first = start;
    while (first < n && isspace((unsigned char)raw[first]))
      ++first;
    size_t end = first;
    while (end < n && raw[end] != '\n')
      ++end;
    if (end - first > 60)
      end = first + 60;
    first_description = copy(raw + first, end - first);
    if (!first_description) {
      if (loaded)
        yaml_document_delete(&doc);
      free(raw);
      return PG_NOMEM;
    }
    description = first_description;
  }
  if (skill) {
    int invalid = strlen(name) > 64 || !*name || name[0] == '-' ||
                  name[strlen(name) - 1] == '-' || strstr(name, "--");
    for (const char *p = name; *p; ++p)
      if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-'))
        invalid = 1;
    if (invalid)
      st = diagnostic(s, path,
                      "skill name must use lowercase letters, digits and "
                      "single internal hyphens, at most 64 characters");
    if (strlen(description) > 1024 && !st)
      st = diagnostic(s, path, "skill description exceeds 1024 bytes");
  }
  if (!st && skill && !*description)
    st = diagnostic(s, path, "skill description is required");
  else if (!st)
    st = append_resource(s, skill ? PG_RESOURCE_SKILL : PG_RESOURCE_PROMPT,
                         name, description, path, raw + start, n - start,
                         project, !strcmp(disabled, "true"));
  if (loaded)
    yaml_document_delete(&doc);
  free(first_description);
  free(raw);
  return st;
}
static int is_dir(const char *path) {
  uv_fs_t req;
  int rc = uv_fs_stat(NULL, &req, path, NULL);
  int result = rc == 0 && (req.statbuf.st_mode & S_IFMT) == S_IFDIR;
  uv_fs_req_cleanup(&req);
  return result;
}
static pg_status discover_walk(pg_resource_snapshot *s, const char *path,
                               int skill, int project, unsigned depth,
                               pg_resource_ignore *ignore) {
  if (depth > 1024)
    return diagnostic(s, path, "resource discovery nesting limit reached");
  uv_fs_t canonical;
  if (uv_fs_realpath(NULL, &canonical, path, NULL) == 0) {
    const char *real = canonical.ptr;
    size_t len = strlen(real);
    char *key = malloc(len + 2);
    if (!key) {
      uv_fs_req_cleanup(&canonical);
      return PG_NOMEM;
    }
    key[0] = (char)('0' + skill);
    memcpy(key + 1, real, len + 1);
    uv_fs_req_cleanup(&canonical);
    for (size_t i = 0; i < s->visited_count; ++i)
      if (!strcmp(s->visited[i], key)) {
        free(key);
        return PG_OK;
      }
    char **next = realloc(s->visited, (s->visited_count + 1) * sizeof(*next));
    if (!next) {
      free(key);
      return PG_NOMEM;
    }
    s->visited = next;
    s->visited[s->visited_count++] = key;
  } else
    uv_fs_req_cleanup(&canonical);
  if (!is_dir(path)) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    size_t n = strlen(base);
    if (n < 3 || strcmp(base + n - 3, ".md"))
      return PG_OK;
    char *name = copy(base, n - 3);
    if (!name)
      return PG_NOMEM;
    pg_status st = markdown(s, path, name, skill, project);
    free(name);
    return st;
  }
  if (ignore) {
    pg_status st = pg_resource_ignore_directory(ignore, path);
    if (st)
      return st;
  }
  if (skill) {
    char *root = join(path, "SKILL.md");
    if (!root)
      return PG_NOMEM;
    uv_fs_t stat;
    int found = uv_fs_stat(NULL, &stat, root, NULL) == 0;
    uv_fs_req_cleanup(&stat);
    if (found && ignore) {
      int ignored = 0;
      pg_status st = pg_resource_ignore_test(ignore, root, &ignored);
      if (st) {
        free(root);
        return st;
      }
      if (ignored)
        found = 0;
    }
    if (found) {
      const char *base = strrchr(path, '/');
      pg_status st = markdown(s, root, base ? base + 1 : path, 1, project);
      free(root);
      return st;
    }
    free(root);
  }
  uv_fs_t req;
  int rc = uv_fs_scandir(NULL, &req, path, 0, NULL);
  if (rc == UV_ENOENT) {
    uv_fs_req_cleanup(&req);
    return PG_OK;
  }
  if (rc < 0) {
    uv_fs_req_cleanup(&req);
    return PG_FAILED;
  }
  pg_status st = PG_OK;
  uv_dirent_t entry;
  while (!st && uv_fs_scandir_next(&req, &entry) == 0) {
    if (entry.name[0] == '.' || !strcmp(entry.name, "node_modules"))
      continue;
    char *child = join(path, entry.name);
    if (!child) {
      st = PG_NOMEM;
      break;
    }
    if (ignore) {
      int ignored = 0;
      st = pg_resource_ignore_test(ignore, child, &ignored);
      if (st || ignored) {
        free(child);
        if (st)
          break;
        continue;
      }
    }
    if ((entry.type == UV_DIRENT_DIR ||
         (entry.type == UV_DIRENT_LINK && is_dir(child))) &&
        skill)
      st = discover_walk(s, child, skill, project, depth + 1, ignore);
    else if ((entry.type == UV_DIRENT_FILE ||
              (entry.type == UV_DIRENT_LINK && !is_dir(child))) &&
             (!skill || depth == 0))
      st = discover_walk(s, child, skill, project, depth + 1, ignore);
    free(child);
  }
  uv_fs_req_cleanup(&req);
  return st;
}
static pg_status discover(pg_resource_snapshot *s, const char *path, int skill,
                          int project, unsigned depth) {
  pg_resource_ignore *ignore = NULL;
  pg_status st = PG_OK;
  if (skill && is_dir(path))
    st = pg_resource_ignore_create(path, &ignore);
  if (!st)
    st = discover_walk(s, path, skill, project, depth, ignore);
  pg_resource_ignore_destroy(ignore);
  return st;
}
static const char *const context_names[] = {
    "AGENTS.override.md", "AGENTS.md", "AGENTS.MD", "CLAUDE.md", "CLAUDE.MD"};
static int regular_file(const char *path) {
  uv_fs_t req;
  int rc = uv_fs_stat(NULL, &req, path, NULL);
  int yes = rc == 0 && (req.statbuf.st_mode & S_IFMT) == S_IFREG;
  uv_fs_req_cleanup(&req);
  return yes;
}
static pg_status canonical_path(const char *path, char **out) {
  *out = NULL;
  uv_fs_t req;
  int rc = uv_fs_realpath(NULL, &req, path, NULL);
  if (!rc)
    *out = copy(req.ptr, strlen(req.ptr));
  uv_fs_req_cleanup(&req);
  return rc ? PG_NOT_FOUND : *out ? PG_OK : PG_NOMEM;
}
static int parent_path(char *path) {
  char *slash = strrchr(path, '/');
  if (!slash || !strcmp(path, "/"))
    return 0;
  if (slash == path)
    slash[1] = 0;
  else
    *slash = 0;
  return 1;
}
static void trim_end(char *text, size_t *length) {
  while (*length && isspace((unsigned char)text[*length - 1]))
    --*length;
  text[*length] = 0;
}
/* Mirrors resource-loader.findShadowedContextFile + footer findGitPaths.
 * Compare canonical directories, never the target of an AGENTS symlink. */
static pg_status context_shadow(const char *cwd, char **out) {
  *out = NULL;
  char *dir = copy(cwd, strlen(cwd));
  if (!dir)
    return PG_NOMEM;
  char *git = NULL, *gitdir = NULL, *common = NULL, *mainroot = NULL,
       *candidate = NULL;
  char *data = NULL;
  size_t n = 0;
  pg_status st = PG_OK;
  for (;;) {
    git = join(dir, ".git");
    if (!git) {
      st = PG_NOMEM;
      goto done;
    }
    uv_fs_t probe;
    int exists = uv_fs_stat(NULL, &probe, git, NULL) == 0;
    uv_fs_req_cleanup(&probe);
    if (exists)
      break;
    free(git);
    git = NULL;
    if (!parent_path(dir))
      goto done;
  }
  if (!regular_file(git))
    goto done;
  st = read_file(git, &data, &n);
  if (st) {
    if (st != PG_NOMEM)
      st = PG_OK;
    goto done;
  }
  trim_end(data, &n);
  if (n < 8 || memcmp(data, "gitdir: ", 8))
    goto done;
  char *value = data + 8;
  while (isspace((unsigned char)*value))
    ++value;
  candidate = value[0] == '/' || (strlen(value) > 2 && value[1] == ':')
                  ? copy(value, strlen(value))
                  : join(dir, value);
  if (!candidate) {
    st = PG_NOMEM;
    goto done;
  }
  st = canonical_path(candidate, &gitdir);
  free(candidate);
  candidate = NULL;
  if (st) {
    if (st != PG_NOMEM)
      st = PG_OK;
    goto done;
  }
  candidate = join(gitdir, "HEAD");
  if (!candidate) {
    st = PG_NOMEM;
    goto done;
  }
  int has_head = regular_file(candidate);
  free(candidate);
  candidate = NULL;
  if (!has_head)
    goto done;
  free(data);
  data = NULL;
  candidate = join(gitdir, "commondir");
  if (!candidate) {
    st = PG_NOMEM;
    goto done;
  }
  st = read_file(candidate, &data, &n);
  free(candidate);
  candidate = NULL;
  if (st == PG_NOT_FOUND) {
    st = PG_OK;
    goto done;
  }
  if (st) {
    if (st != PG_NOMEM)
      st = PG_OK;
    goto done;
  }
  trim_end(data, &n);
  candidate = data[0] == '/' || (n > 2 && data[1] == ':') ? copy(data, n)
                                                          : join(gitdir, data);
  if (!candidate) {
    st = PG_NOMEM;
    goto done;
  }
  st = canonical_path(candidate, &common);
  free(candidate);
  candidate = NULL;
  if (st) {
    if (st != PG_NOMEM)
      st = PG_OK;
    goto done;
  }
  mainroot = copy(common, strlen(common));
  if (!mainroot) {
    st = PG_NOMEM;
    goto done;
  }
  if (!parent_path(mainroot))
    goto done;
  size_t len = strlen(mainroot);
  if (strncmp(dir, mainroot, len) || dir[len] != '/')
    goto done;
  candidate = join(mainroot, ".git");
  if (!candidate) {
    st = PG_NOMEM;
    goto done;
  }
  char *actual = NULL;
  st = canonical_path(candidate, &actual);
  free(candidate);
  candidate = NULL;
  if (st) {
    if (st != PG_NOMEM)
      st = PG_OK;
    free(actual);
    goto done;
  }
  int same = !strcmp(actual, common);
  free(actual);
  if (!same)
    goto done;
  for (size_t i = 0; i < sizeof(context_names) / sizeof(*context_names); ++i) {
    candidate = join(dir, context_names[i]);
    if (!candidate) {
      st = PG_NOMEM;
      goto done;
    }
    int found = regular_file(candidate);
    free(candidate);
    candidate = NULL;
    if (found) {
      *out = join(mainroot, context_names[i]);
      if (!*out)
        st = PG_NOMEM;
      break;
    }
  }
done:
  free(dir);
  free(git);
  free(gitdir);
  free(common);
  free(mainroot);
  free(candidate);
  free(data);
  return st;
}

static pg_status context(pg_resource_snapshot *s, const char *dir, int project,
                         const char *shadow) {
  for (size_t i = 0; i < 5; ++i) {
    char *path = join(dir, context_names[i]);
    if (!path)
      return PG_NOMEM;
    if (!regular_file(path)) {
      free(path);
      continue;
    }
    if (shadow) {
      char *canonical = NULL;
      pg_status resolved = canonical_path(dir, &canonical);
      if (resolved == PG_NOMEM) {
        free(path);
        return resolved;
      }
      char *key = canonical ? join(canonical, context_names[i]) : NULL;
      free(canonical);
      if (resolved == PG_OK && !key) {
        free(path);
        return PG_NOMEM;
      }
      int skip = key && !strcmp(key, shadow);
      free(key);
      if (skip) {
        free(path);
        return PG_OK;
      }
    }
    char *text;
    size_t n;
    pg_status st = read_file(path, &text, &n);
    if (st == PG_NOT_FOUND) {
      free(path);
      continue;
    }
    if (!st) {
      st = append_resource(s, PG_RESOURCE_CONTEXT, context_names[i], "", path,
                           text, n, project, 0);
      free(text);
    }
    free(path);
    return st;
  }
  return PG_OK;
}
static pg_status ancestors(pg_resource_snapshot *s, const char *dir,
                           const char *shadow) {
  char *parent = copy(dir, strlen(dir));
  if (!parent)
    return PG_NOMEM;
  char *sep = strrchr(parent, '/');
  pg_status st = PG_OK;
  if (sep && sep != parent) {
    *sep = 0;
    st = ancestors(s, parent, shadow);
  } else if (sep && strlen(parent) > 1)
    st = context(s, "/", 1, shadow);
  free(parent);
  return st ? st : context(s, dir, 1, shadow);
}
static pg_status special(resources *r, const char *name, char **text,
                         size_t *n) {
  pg_status st = PG_NOT_FOUND;
  if (r->trusted) {
    char *p = join(r->project, name);
    if (!p)
      return PG_NOMEM;
    st = read_file(p, text, n);
    free(p);
  }
  if (st == PG_NOT_FOUND) {
    char *p = join(r->user, name);
    if (!p)
      return PG_NOMEM;
    st = read_file(p, text, n);
    free(p);
  }
  return st;
}
static int xml(buffer *b, pangu_slice text) {
  for (size_t i = 0; i < text.len; ++i) {
    const char *entity = text.data[i] == '&'    ? "&amp;"
                         : text.data[i] == '<'  ? "&lt;"
                         : text.data[i] == '>'  ? "&gt;"
                         : text.data[i] == '\"' ? "&quot;"
                         : text.data[i] == '\'' ? "&apos;"
                                                : NULL;
    if (entity) {
      if (!str(b, entity))
        return 0;
    } else if (!add(b, text.data + i, 1))
      return 0;
  }
  return 1;
}
static int section(buffer *b, const char *tag, const char *text, size_t len) {
  return str(b, "\n\n<") && str(b, tag) && str(b, ">\n") && add(b, text, len) &&
         str(b, "\n</") && str(b, tag) && str(b, ">");
}
static char *resource_path(const char *base, const char *path) {
  if (path[0] == '/' || (strlen(path) > 2 && path[1] == ':'))
    return copy(path, strlen(path));
  if (path[0] == '~' && (!path[1] || path[1] == '/')) {
    char home[4096];
    size_t n = sizeof(home);
    if (uv_os_homedir(home, &n))
      return NULL;
    return path[1] ? join(home, path + 2) : copy(home, strlen(home));
  }
  return join(base, path);
}
static pg_status explicit_resources(resources *r, pg_resource_snapshot *s) {
  yyjson_doc *d = yyjson_read(s->settings, s->settings_len, 0);
  if (!d)
    return PG_INVALID;
  pg_status st = PG_OK;
  for (int skill = 0; skill < 2 && !st; ++skill) {
    yyjson_val *array =
        yyjson_obj_get(yyjson_doc_get_root(d), skill ? "skills" : "prompts");
    if (!array)
      continue;
    if (!yyjson_is_arr(array)) {
      st = diagnostic(s, r->cwd, "resource paths must be an array");
      continue;
    }
    size_t i, n;
    yyjson_val *v;
    yyjson_arr_foreach(array, i, n, v) {
      if (!yyjson_is_str(v) ||
          memchr(yyjson_get_str(v), 0, yyjson_get_len(v))) {
        st = diagnostic(s, r->cwd, "resource path must be a string");
        if (st)
          break;
        continue;
      }
      char *path = resource_path(r->cwd, yyjson_get_str(v));
      if (!path) {
        st = PG_NOMEM;
        break;
      }
      st = discover(s, path, skill, 0, 0);
      free(path);
      if (st)
        break;
    }
  }
  yyjson_doc_free(d);
  return st;
}

static pg_status build(resources *r, pg_resource_snapshot **out) {
  *out = NULL;
  pg_resource_snapshot *s = calloc(1, sizeof(*s));
  if (!s)
    return PG_NOMEM;
  atomic_init(&s->refs, 1);
  if (!str(&s->diagnostics, "[]")) {
    pg_resource_snapshot_release(s);
    return PG_NOMEM;
  }
  pg_status st = settings(r, s);
  if (!st)
    st = context(s, r->user, 0, NULL);
  char *shadow = NULL;
  if (!st)
    st = context_shadow(r->cwd, &shadow);
  if (!st)
    st = ancestors(s, r->cwd, shadow);
  free(shadow);
  for (int project = 0; project < 2 && !st; ++project) {
    if (project && !r->trusted)
      break;
    const char *base = project ? r->project : r->user;
    for (int skill = 0; skill < 2 && !st; ++skill) {
      char *path = join(base, skill ? "skills" : "prompts");
      if (!path) {
        st = PG_NOMEM;
        break;
      }
      st = discover(s, path, skill, project, 0);
      free(path);
    }
  }
  if (!st)
    st = explicit_resources(r, s);
  buffer prompt = {0};
  char *text = NULL;
  size_t n = 0;
  if (!st) {
    pg_status loaded = special(r, "SYSTEM.md", &text, &n);
    if (loaded != PG_OK && loaded != PG_NOT_FOUND)
      st = loaded;
    else if (!str(&prompt, text ? text
                                : "You are an expert assistant operating "
                                  "inside PanGu, an agent harness. You help "
                                  "users by reading files, executing commands, "
                                  "editing code, and writing new files."))
      st = PG_NOMEM;
    free(text);
    text = NULL;
  }
  if (!st) {
    pg_status loaded = special(r, "APPEND_SYSTEM.md", &text, &n);
    if (loaded != PG_OK && loaded != PG_NOT_FOUND)
      st = loaded;
    else if (text && !section(&prompt, "addendum", text, n))
      st = PG_NOMEM;
    free(text);
    text = NULL;
  }
  for (size_t i = 0; i < s->count && !st; ++i) {
    pg_resource *v = &s->items[i];
    if (v->kind == PG_RESOURCE_CONTEXT) {
      buffer b = {0};
      if (!str(&b, "Project-specific instructions and "
                   "guidelines:\n\n<project_instructions path=\"") ||
          !xml(&b, v->path) || !str(&b, "\">\n") ||
          !add(&b, v->body.data, v->body.len) ||
          !str(&b, "\n</project_instructions>") ||
          !section(&prompt, "project_context", b.data, b.len))
        st = PG_NOMEM;
      free(b.data);
    }
  }
  buffer skills = {0};
  int read_tool = 1, bash_tool = 1;
  yyjson_doc *selected = yyjson_read(s->settings, s->settings_len, 0);
  yyjson_val *selected_tools =
      yyjson_obj_get(yyjson_doc_get_root(selected), "defaultTools");
  if (yyjson_is_arr(selected_tools)) {
    read_tool = 0;
    bash_tool = 0;
    size_t i, n;
    yyjson_val *v;
    yyjson_arr_foreach(selected_tools, i, n, v) {
      const char *name = yyjson_get_str(v);
      if (name) {
        read_tool |= !strcmp(name, "read");
        bash_tool |= !strcmp(name, "bash");
      }
    }
  }
  yyjson_doc_free(selected);
  size_t visible_skills = 0;
  for (size_t i = 0; i < s->count; ++i)
    if ((read_tool || bash_tool) && s->items[i].kind == PG_RESOURCE_SKILL &&
        !s->items[i].disable_model_invocation)
      ++visible_skills;
  if (visible_skills &&
      !str(&skills,
           "The following skills provide specialized instructions for specific "
           "tasks.\nLoad a skill's file when the task "
           "matches its description.\nWhen a skill file references a relative "
           "path, resolve it against the skill directory (parent of SKILL.md / "
           "dirname of the path) and use that absolute path in tool "
           "commands.\n\n<available_skills>"))
    st = PG_NOMEM;
  for (size_t i = 0; i < s->count && !st; ++i) {
    pg_resource *v = &s->items[i];
    if ((read_tool || bash_tool) && v->kind == PG_RESOURCE_SKILL &&
        !v->disable_model_invocation) {
      if (!str(&skills, "\n  <skill>\n    <name>") || !xml(&skills, v->name) ||
          !str(&skills, "</name>\n    <description>") ||
          !xml(&skills, v->description) ||
          !str(&skills, "</description>\n    <location>") ||
          !xml(&skills, v->path) || !str(&skills, "</location>\n  </skill>"))
        st = PG_NOMEM;
    }
  }
  if (visible_skills && (!str(&skills, "\n</available_skills>") ||
                         !section(&prompt, "skills", skills.data, skills.len)))
    st = PG_NOMEM;
  free(skills.data);
  if (!st && !section(&prompt, "cwd", r->cwd, strlen(r->cwd)))
    st = PG_NOMEM;
  if (st) {
    free(prompt.data);
    pg_resource_snapshot_release(s);
    return st;
  }
  s->system = prompt.data;
  s->system_len = prompt.len;
  *out = s;
  return PG_OK;
}
static pg_status reload(void *ctx) {
  resources *r = ctx;
  pg_resource_snapshot *s;
  pg_status st = build(r, &s);
  if (st)
    return st;
  pg_resource_snapshot *old = r->current;
  r->current = s;
  pg_resource_snapshot_release(old);
  return PG_OK;
}
static pg_status snapshot(void *ctx, pg_resource_snapshot **out) {
  if (!out)
    return PG_INVALID;
  resources *r = ctx;
  *out = r->current;
  atomic_fetch_add(&r->current->refs, 1);
  return PG_OK;
}
static pg_status trust(void *ctx, int trusted) {
  resources *r = ctx;
  int old = r->trusted;
  r->trusted = !!trusted;
  pg_status st = reload(r);
  if (st)
    r->trusted = old;
  return st;
}
static pg_status atomic_write(const char *path, const char *data, size_t n) {
  char *name = copy(path, strlen(path));
  if (!name)
    return PG_NOMEM;
  uv_fs_t req;
  for (char *p = name + 1; *p; ++p)
    if (*p == '/') {
      *p = 0;
      int rc = uv_fs_mkdir(NULL, &req, name, 0700, NULL);
      uv_fs_req_cleanup(&req);
      *p = '/';
      if (rc < 0 && rc != UV_EEXIST) {
        free(name);
        return PG_FAILED;
      }
    }
  free(name);
  name = malloc(strlen(path) + 12);
  if (!name)
    return PG_NOMEM;
  sprintf(name, "%s.XXXXXX", path);
  int fd = uv_fs_mkstemp(NULL, &req, name, NULL);
  if (fd >= 0)
    strcpy(name, req.path);
  uv_fs_req_cleanup(&req);
  if (fd < 0) {
    free(name);
    return PG_FAILED;
  }
  pg_status st = PG_OK;
  for (size_t off = 0; off < n;) {
    size_t amount = n - off;
    if (amount > 65536)
      amount = 65536;
    uv_buf_t b = uv_buf_init((char *)data + off, (unsigned)amount);
    int rc = uv_fs_write(NULL, &req, fd, &b, 1, -1, NULL);
    uv_fs_req_cleanup(&req);
    if (rc <= 0) {
      st = PG_FAILED;
      break;
    }
    off += (size_t)rc;
  }
  if (!st) {
    int rc = uv_fs_fsync(NULL, &req, fd, NULL);
    uv_fs_req_cleanup(&req);
    if (rc)
      st = PG_FAILED;
  }
  int rc = uv_fs_close(NULL, &req, fd, NULL);
  uv_fs_req_cleanup(&req);
  if (rc)
    st = PG_FAILED;
  if (!st) {
    rc = uv_fs_rename(NULL, &req, name, path, NULL);
    uv_fs_req_cleanup(&req);
    if (rc)
      st = PG_FAILED;
  }
  if (st) {
    uv_fs_unlink(NULL, &req, name, NULL);
    uv_fs_req_cleanup(&req);
  }
  free(name);
  return st;
}
static pg_status patch_unlocked(void *ctx, int project, pangu_slice input) {
  resources *r = ctx;
  if (project && !r->trusted)
    return PG_CLOSED;
  yyjson_doc *doc = yyjson_read(input.data, input.len, 0);
  if (!doc || !yyjson_is_obj(yyjson_doc_get_root(doc))) {
    yyjson_doc_free(doc);
    return PG_INVALID;
  }
  char *path = join(project ? r->project : r->user, "settings.json");
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!path || !d) {
    free(path);
    yyjson_doc_free(doc);
    yyjson_mut_doc_free(d);
    return PG_NOMEM;
  }
  yyjson_mut_val *root = yyjson_mut_obj(d);
  yyjson_mut_doc_set_root(d, root);
  pg_status st = root ? layer(d, root, path) : PG_NOMEM;
  if (!st && !merge(d, root, yyjson_doc_get_root(doc)))
    st = PG_NOMEM;
  size_t n = 0;
  char *data =
      st ? NULL : yyjson_mut_write(d, YYJSON_WRITE_PRETTY_TWO_SPACES, &n);
  if (!st)
    st = data ? atomic_write(path, data, n) : PG_NOMEM;
  free(path);
  free(data);
  yyjson_mut_doc_free(d);
  yyjson_doc_free(doc);
  return st ? st : reload(r);
}
static pg_status remember_unlocked(void *ctx, int trusted) {
  resources *r = ctx;
  char *path = join(r->user, "trust.json");
  if (!path)
    return PG_NOMEM;
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d) {
    free(path);
    return PG_NOMEM;
  }
  yyjson_mut_val *root = yyjson_mut_obj(d);
  yyjson_mut_doc_set_root(d, root);
  pg_status st = root ? layer(d, root, path) : PG_NOMEM;
  if (!st && !yyjson_mut_obj_put(root, yyjson_mut_strcpy(d, r->cwd),
                                 yyjson_mut_bool(d, !!trusted)))
    st = PG_NOMEM;
  size_t n = 0;
  char *data = st ? NULL : yyjson_mut_write(d, 0, &n);
  if (!st)
    st = data ? atomic_write(path, data, n) : PG_NOMEM;
  free(path);
  free(data);
  yyjson_mut_doc_free(d);
  return st ? st : trust(r, trusted);
}
static int valid_name(pangu_slice name) {
  return name.data && name.len && !memchr(name.data, 0, name.len);
}
static pg_status auth_read(resources *r, yyjson_doc **out) {
  *out = NULL;
  char *path = join(r->user, "auth.json");
  if (!path)
    return PG_NOMEM;
  char *data;
  size_t n;
  pg_status st = read_file(path, &data, &n);
  free(path);
  if (st)
    return st;
  *out = yyjson_read(data, n, 0);
  free(data);
  if (!*out || !yyjson_is_obj(yyjson_doc_get_root(*out))) {
    yyjson_doc_free(*out);
    *out = NULL;
    return PG_INVALID;
  }
  return PG_OK;
}
static pg_status environment_value(yyjson_val *local, const char *name,
                                   char **out, size_t *length) {
  *out = NULL;
  *length = 0;
  yyjson_val *v = yyjson_obj_get(local, name);
  if (yyjson_is_str(v) && yyjson_get_len(v)) {
    *length = yyjson_get_len(v);
    *out = copy(yyjson_get_str(v), *length);
    return *out ? PG_OK : PG_NOMEM;
  }
  char initial[1];
  size_t n = sizeof(initial);
  int rc = uv_os_getenv(name, initial, &n);
  if (rc == UV_ENOENT)
    return PG_NOT_FOUND;
  if (!rc) {
    *out = copy(initial, n);
    *length = n;
    return *out ? PG_OK : PG_NOMEM;
  }
  if (rc != UV_ENOBUFS)
    return PG_FAILED;
  char *text = malloc(n);
  if (!text)
    return PG_NOMEM;
  rc = uv_os_getenv(name, text, &n);
  if (rc) {
    free(text);
    return PG_FAILED;
  }
  *out = text;
  *length = n;
  return n ? PG_OK : PG_NOT_FOUND;
}
static pg_status resolve_key(const char *value, size_t n, yyjson_val *local,
                             char **out, size_t *length) {
  buffer b = {0};
  pg_status st = PG_OK;
  for (size_t i = 0; i < n && !st;) {
    if (value[i] != '$') {
      if (!add(&b, value + i++, 1))
        st = PG_NOMEM;
      continue;
    }
    ++i;
    if (i < n && (value[i] == '$' || value[i] == '!')) {
      if (!add(&b, value + i++, 1))
        st = PG_NOMEM;
      continue;
    }
    int brace = i < n && value[i] == '{';
    size_t begin = i + brace, end = begin;
    if (end < n && (isalpha((unsigned char)value[end]) || value[end] == '_')) {
      ++end;
      while (end < n &&
             (isalnum((unsigned char)value[end]) || value[end] == '_'))
        ++end;
    }
    if (end == begin || (brace && (end == n || value[end] != '}'))) {
      if (!str(&b, "$"))
        st = PG_NOMEM;
      continue;
    }
    char *name = copy(value + begin, end - begin);
    if (!name) {
      st = PG_NOMEM;
      break;
    }
    char *text = NULL;
    size_t len = 0;
    st = environment_value(local, name, &text, &len);
    free(name);
    if (!st && !add(&b, text, len))
      st = PG_NOMEM;
    free(text);
    i = end + brace;
  }
  if (st) {
    free(b.data);
    return st;
  }
  if (!b.data)
    b.data = copy("", 0);
  if (!b.data)
    return PG_NOMEM;
  *out = b.data;
  *length = b.len;
  return PG_OK;
}

static pg_status get_key(void *ctx, pangu_slice provider, char **out,
                         size_t *length) {
  if (!out || !length || !valid_name(provider))
    return PG_INVALID;
  *out = NULL;
  *length = 0;
  resources *r = ctx;
  yyjson_doc *d;
  pg_status st = auth_read(r, &d);
  if (st != PG_OK && st != PG_NOT_FOUND)
    return st;
  if (d) {
    yyjson_val *entry =
        yyjson_obj_getn(yyjson_doc_get_root(d), provider.data, provider.len);
    if (entry) {
      const char *type = yyjson_get_str(yyjson_obj_get(entry, "type"));
      yyjson_val *key = yyjson_obj_get(entry, "key");
      if (!type || strcmp(type, "api_key") || !yyjson_is_str(key)) {
        yyjson_doc_free(d);
        return PG_ABI;
      }
      const char *value = yyjson_get_str(key);
      size_t n = yyjson_get_len(key);
      if (memchr(value, 0, n) || (n && value[0] == '!')) {
        yyjson_doc_free(d);
        return PG_INVALID;
      }
      pg_status result =
          resolve_key(value, n, yyjson_obj_get(entry, "env"), out, length);
      yyjson_doc_free(d);
      return result;
    }
    yyjson_doc_free(d);
  }
  if (provider.len > 128)
    return PG_INVALID;
  char env[144];
  for (size_t i = 0; i < provider.len; ++i) {
    unsigned char c = (unsigned char)provider.data[i];
    env[i] = isalnum(c) ? (char)toupper(c) : '_';
  }
  memcpy(env + provider.len, "_API_KEY", 9);
  size_t n = 1;
  char initial[1];
  int rc = uv_os_getenv(env, initial, &n);
  if (!rc) {
    *out = copy(initial, n);
    *length = n;
    return *out ? PG_OK : PG_NOMEM;
  }
  if (rc == UV_ENOENT)
    return PG_NOT_FOUND;
  if (rc != UV_ENOBUFS)
    return PG_FAILED;
  *out = malloc(n);
  if (!*out)
    return PG_NOMEM;
  rc = uv_os_getenv(env, *out, &n);
  if (rc) {
    free(*out);
    *out = NULL;
    return PG_FAILED;
  }
  *length = n;
  return PG_OK;
}
static pg_status put_key_unlocked(void *ctx, pangu_slice provider,
                                  pangu_slice key) {
  if (!valid_name(provider) || (!key.data && key.len) ||
      (key.len && memchr(key.data, 0, key.len)) ||
      (key.len && key.data[0] == '!'))
    return PG_INVALID;
  resources *r = ctx;
  char *path = join(r->user, "auth.json");
  if (!path)
    return PG_NOMEM;
  yyjson_doc *current;
  pg_status st = auth_read(r, &current);
  if (st != PG_OK && st != PG_NOT_FOUND) {
    free(path);
    return st;
  }
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d) {
    free(path);
    yyjson_doc_free(current);
    return PG_NOMEM;
  }
  yyjson_mut_val *root =
      current ? yyjson_val_mut_copy(d, yyjson_doc_get_root(current))
              : yyjson_mut_obj(d);
  yyjson_mut_doc_set_root(d, root);
  yyjson_doc_free(current);
  yyjson_mut_val *name = yyjson_mut_strncpy(d, provider.data, provider.len),
                 *entry = NULL;
  if (key.len) {
    entry = yyjson_mut_obj(d);
    if (!entry || !yyjson_mut_obj_add_str(d, entry, "type", "api_key") ||
        !yyjson_mut_obj_add_strncpy(d, entry, "key", key.data, key.len))
      st = PG_NOMEM;
  } else
    st = PG_OK;
  if (root && name && (st == PG_OK || st == PG_NOT_FOUND)) {
    st = yyjson_mut_obj_put(root, name, entry) ? PG_OK : PG_NOMEM;
  } else if (!root || !name)
    st = PG_NOMEM;
  size_t n = 0;
  char *data = st ? NULL : yyjson_mut_write(d, 0, &n);
  if (!st)
    st = data ? atomic_write(path, data, n) : PG_NOMEM;
  free(data);
  free(path);
  yyjson_mut_doc_free(d);
  return st;
}

/* mkdir locks coordinate native writers across processes without changing the
 * process cwd. A live lock is never forcibly broken; timeout returns failure.
 */
static pg_status lock_file(const char *path, char **out) {
  *out = NULL;
  char *name = copy(path, strlen(path));
  if (!name)
    return PG_NOMEM;
  uv_fs_t req;
  for (char *p = name + 1; *p; ++p)
    if (*p == '/') {
      *p = 0;
      int rc = uv_fs_mkdir(NULL, &req, name, 0700, NULL);
      uv_fs_req_cleanup(&req);
      *p = '/';
      if (rc < 0 && rc != UV_EEXIST) {
        free(name);
        return PG_FAILED;
      }
    }
  free(name);
  name = malloc(strlen(path) + 6);
  if (!name)
    return PG_NOMEM;
  sprintf(name, "%s.lock", path);
  for (unsigned i = 0; i < 200; ++i) {
    int rc = uv_fs_mkdir(NULL, &req, name, 0700, NULL);
    uv_fs_req_cleanup(&req);
    if (!rc) {
      *out = name;
      return PG_OK;
    }
    if (rc != UV_EEXIST)
      break;
    uv_sleep(10);
  }
  free(name);
  return PG_FAILED;
}
static void unlock_file(char *path) {
  if (path) {
    uv_fs_t req;
    uv_fs_rmdir(NULL, &req, path, NULL);
    uv_fs_req_cleanup(&req);
    free(path);
  }
}
static pg_status patch(void *ctx, int project, pangu_slice input) {
  resources *r = ctx;
  if (project && !r->trusted)
    return PG_CLOSED;
  char *path = join(project ? r->project : r->user, "settings.json"),
       *lock = NULL;
  if (!path)
    return PG_NOMEM;
  pg_status st = lock_file(path, &lock);
  free(path);
  if (st)
    return st;
  st = patch_unlocked(ctx, project, input);
  unlock_file(lock);
  return st;
}
static pg_status remember(void *ctx, int trusted) {
  resources *r = ctx;
  char *path = join(r->user, "trust.json"), *lock = NULL;
  if (!path)
    return PG_NOMEM;
  pg_status st = lock_file(path, &lock);
  free(path);
  if (st)
    return st;
  st = remember_unlocked(ctx, trusted);
  unlock_file(lock);
  return st;
}
static pg_status put_key(void *ctx, pangu_slice provider, pangu_slice key) {
  resources *r = ctx;
  char *path = join(r->user, "auth.json"), *lock = NULL;
  if (!path)
    return PG_NOMEM;
  pg_status st = lock_file(path, &lock);
  free(path);
  if (st)
    return st;
  st = put_key_unlocked(ctx, provider, key);
  unlock_file(lock);
  return st;
}

static pg_status stored_trust(resources *r) {
  char *path = join(r->user, "trust.json");
  if (!path)
    return PG_NOMEM;
  char *data;
  size_t n;
  pg_status st = read_file(path, &data, &n);
  free(path);
  if (st == PG_NOT_FOUND)
    return PG_OK;
  if (st)
    return st;
  yyjson_doc *d = yyjson_read(data, n, 0);
  free(data);
  if (!d || !yyjson_is_obj(yyjson_doc_get_root(d))) {
    yyjson_doc_free(d);
    return PG_INVALID;
  }
  char *candidate = copy(r->cwd, strlen(r->cwd));
  if (!candidate) {
    yyjson_doc_free(d);
    return PG_NOMEM;
  }
  for (;;) {
    yyjson_val *v = yyjson_obj_get(yyjson_doc_get_root(d), candidate);
    if (yyjson_is_bool(v)) {
      r->trusted = yyjson_get_bool(v);
      break;
    }
    char *slash = strrchr(candidate, '/');
    if (!slash || !strcmp(candidate, "/"))
      break;
    if (slash == candidate)
      slash[1] = 0;
    else
      *slash = 0;
  }
  free(candidate);
  yyjson_doc_free(d);
  return PG_OK;
}

static void destroy(void *ctx) {
  resources *r = ctx;
  pg_resource_snapshot_release(r->current);
  free(r->user);
  free(r->cwd);
  free(r->project);
  free(r->overrides);
  free(r);
}
static pg_status install(pg_scope *scope, const void *cfg, size_t size) {
  if (!cfg || size != sizeof(pg_resources_config))
    return PG_INVALID;
  const pg_resources_config *c = cfg;
  if (!c->cwd || !c->user_directory ||
      (!c->overrides_json.data && c->overrides_json.len) ||
      (c->overrides_json.len &&
       memchr(c->overrides_json.data, 0, c->overrides_json.len)))
    return PG_INVALID;
  if (!(c->cwd[0] == '/' || (strlen(c->cwd) > 2 && c->cwd[1] == ':')) ||
      !(c->user_directory[0] == '/' ||
        (strlen(c->user_directory) > 2 && c->user_directory[1] == ':')))
    return PG_INVALID;
  resources *r = calloc(1, sizeof(*r));
  if (!r)
    return PG_NOMEM;
  r->user = copy(c->user_directory, strlen(c->user_directory));
  r->cwd = copy(c->cwd, strlen(c->cwd));
  r->project = join(c->cwd, ".pangu");
  r->overrides = copy(c->overrides_json.len ? c->overrides_json.data : "{}",
                      c->overrides_json.len ? c->overrides_json.len : 2);
  r->trusted = c->project_trusted > 0;
  if (!r->user || !r->cwd || !r->project || !r->overrides) {
    destroy(r);
    return PG_NOMEM;
  }
  /* Trust keys use canonical cwd, including aliases into the same project. */
  uv_fs_t canonical;
  if (uv_fs_realpath(NULL, &canonical, r->cwd, NULL) == 0) {
    char *cwd = copy(canonical.ptr, strlen(canonical.ptr));
    if (!cwd) {
      uv_fs_req_cleanup(&canonical);
      destroy(r);
      return PG_NOMEM;
    }
    free(r->cwd);
    r->cwd = cwd;
    free(r->project);
    r->project = join(r->cwd, ".pangu");
  }
  uv_fs_req_cleanup(&canonical);
  if (!r->project) {
    destroy(r);
    return PG_NOMEM;
  }
  pg_status st = c->project_trusted < 0 ? stored_trust(r) : PG_OK;
  if (!st)
    st = reload(r);
  if (st) {
    destroy(r);
    return st;
  }
  r->service = (pg_resources_service){snapshot, reload, patch, trust, remember};
  st = pg_scope_defer(scope, destroy, r);
  if (st) {
    destroy(r);
    return st;
  }
  st = pg_service_register(scope, PG_NAME(PG_RESOURCES_SERVICE), 1, &r->service,
                           r);
  if (st)
    return st;
  r->credentials = (pg_credentials_service){get_key, put_key};
  return pg_service_register(scope, PG_NAME(PG_CREDENTIALS_SERVICE), 1,
                             &r->credentials, r);
}
const pg_plugin pg_resources_plugin = {
    1,
    sizeof(pg_plugin),
    {"pangu.resources", sizeof("pangu.resources") - 1},
    NULL,
    0,
    install};
/* Template expansion is a single pass over source, never over substituted data.
 */
pg_status pg_resource_expand(const pg_resource_snapshot *s, pangu_slice input,
                             char **output, size_t *length) {
  if (!s || !output || !length || (!input.data && input.len) ||
      (input.len && memchr(input.data, 0, input.len)))
    return PG_INVALID;
  *output = NULL;
  *length = 0;
  const pg_resource *t = NULL;
  size_t command = 1;
  if (input.len && input.data[0] == '/') {
    while (command < input.len && !isspace((unsigned char)input.data[command]))
      ++command;
    for (size_t i = 0; i < s->count; ++i)
      if (s->items[i].kind == PG_RESOURCE_PROMPT &&
          s->items[i].name.len == command - 1 &&
          !memcmp(s->items[i].name.data, input.data + 1, command - 1)) {
        t = &s->items[i];
        break;
      }
  }
  if (!t) {
    *output = copy(input.data, input.len);
    if (!*output)
      return PG_NOMEM;
    *length = input.len;
    return PG_OK;
  }
  char **args = NULL;
  size_t count = 0;
  buffer token = {0};
  int quote = 0, started = 0;
  pg_status st = PG_OK;
  for (size_t i = command; i <= input.len; ++i) {
    unsigned char c = i < input.len ? (unsigned char)input.data[i] : 0;
    if (quote && c) {
      if (c == quote)
        quote = 0;
      else if (!add(&token, (const char *)&input.data[i], 1)) {
        st = PG_NOMEM;
        break;
      }
    } else if (c == '\'' || c == '"') {
      quote = c;
      started = 1;
    } else if (!c || isspace(c)) {
      if (started) {
        char **next = realloc(args, (count + 1) * sizeof(*args));
        if (!next) {
          st = PG_NOMEM;
          break;
        }
        args = next;
        args[count] = copy(token.data ? token.data : "", token.len);
        if (!args[count]) {
          st = PG_NOMEM;
          break;
        }
        ++count;
        token.len = 0;
        if (token.data)
          token.data[0] = 0;
        started = 0;
      }
    } else {
      started = 1;
      if (!add(&token, input.data + i, 1)) {
        st = PG_NOMEM;
        break;
      }
    }
  }
  buffer result = {0};
  const char *body = t->body.data;
  size_t n = t->body.len;
  for (size_t i = 0; i < n && !st;) {
    if (body[i] != '$') {
      if (!add(&result, body + i++, 1))
        st = PG_NOMEM;
      continue;
    }
    size_t begin = i++, target = i;
    int brace = i < n && body[i] == '{';
    if (brace)
      target = ++i;
    int all = 0;
    size_t index = 0;
    if (i < n && body[i] == '@') {
      all = 1;
      ++i;
    } else if (n - i >= 9 && !memcmp(body + i, "ARGUMENTS", 9)) {
      all = 1;
      i += 9;
    } else {
      while (i < n && isdigit((unsigned char)body[i])) {
        if (index > SIZE_MAX / 10) {
          st = PG_INVALID;
          break;
        }
        index = index * 10 + (size_t)(body[i++] - '0');
      }
    }
    size_t from = all     ? 0
                  : index ? index - 1
                          : count,
           to = all            ? count
                : from < count ? from + 1
                               : count;
    const char *fallback = NULL;
    size_t fallback_len = 0;
    if (i == target) {
      i = begin + 1;
      if (!add(&result, "$", 1))
        st = PG_NOMEM;
      continue;
    }
    if (brace && i + 1 < n && body[i] == ':' && body[i + 1] == '-') {
      size_t start = i + 2;
      i = start;
      while (i < n && body[i] != '}')
        ++i;
      fallback = body + start;
      fallback_len = i - start;
    } else if (brace && all && i < n && body[i] == ':') {
      ++i;
      size_t pos = 0;
      while (i < n && isdigit((unsigned char)body[i])) {
        if (pos > SIZE_MAX / 10) {
          st = PG_INVALID;
          break;
        }
        pos = pos * 10 + (size_t)(body[i++] - '0');
      }
      from = pos ? pos - 1 : 0;
      to = count;
      if (i < n && body[i] == ':') {
        ++i;
        size_t amount = 0;
        while (i < n && isdigit((unsigned char)body[i])) {
          if (amount > SIZE_MAX / 10) {
            st = PG_INVALID;
            break;
          }
          amount = amount * 10 + (size_t)(body[i++] - '0');
        }
        to = amount > count          ? count
             : from > count - amount ? count
                                     : from + amount;
      }
    }
    if (brace) {
      if (i >= n || body[i] != '}') {
        i = begin + 1;
        if (!add(&result, "$", 1))
          st = PG_NOMEM;
        continue;
      }
      ++i;
    }
    buffer value = {0};
    for (size_t j = from; j < to && j < count; ++j) {
      if (j > from && !str(&value, " "))
        st = PG_NOMEM;
      if (!str(&value, args[j]))
        st = PG_NOMEM;
    }
    if (!st && !add(&result,
                    value.len  ? value.data
                    : fallback ? fallback
                               : "",
                    value.len ? value.len : fallback_len))
      st = PG_NOMEM;
    free(value.data);
  }
  for (size_t i = 0; i < count; ++i)
    free(args[i]);
  free(args);
  free(token.data);
  if (st) {
    free(result.data);
    return st;
  }
  if (!result.data)
    result.data = copy("", 0);
  if (!result.data)
    return PG_NOMEM;
  *output = result.data;
  *length = result.len;
  return PG_OK;
}
