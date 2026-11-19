/* Pi d5b3de22 compaction/utils.ts and compaction.ts file-operation tracking.
 * MIT: third-party/Pi-LICENSE. JSON/UTF-8 handled by yyjson/utf8proc. */
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>
typedef struct {
  char *path;
  size_t len;
  int modified;
} file_path;
typedef struct {
  file_path *paths;
  size_t count, capacity;
} file_ops;
static int equal(pangu_slice s, const char *name) {
  return s.len == strlen(name) && !memcmp(s.data, name, s.len);
}
static pg_session_status put(file_ops *ops, pangu_slice path, int modified) {
  if (!path.len)
    return PG_SESSION_OK;
  for (size_t i = 0; i < ops->count; i++)
    if (ops->paths[i].len == path.len &&
        !memcmp(ops->paths[i].path, path.data, path.len)) {
      ops->paths[i].modified |= modified;
      return PG_SESSION_OK;
    }
  if (ops->count == ops->capacity) {
    size_t cap = ops->capacity ? ops->capacity * 2 : 16;
    if (cap < ops->capacity || cap > SIZE_MAX / sizeof(file_path))
      return PG_SESSION_NOMEM;
    file_path *next = realloc(ops->paths, cap * sizeof(*next));
    if (!next)
      return PG_SESSION_NOMEM;
    ops->paths = next;
    ops->capacity = cap;
  }
  char *copy = malloc(path.len);
  if (!copy)
    return PG_SESSION_NOMEM;
  memcpy(copy, path.data, path.len);
  ops->paths[ops->count++] = (file_path){copy, path.len, modified};
  return PG_SESSION_OK;
}
static pangu_slice string(yyjson_val *v) {
  return yyjson_is_str(v) ? (pangu_slice){yyjson_get_str(v), yyjson_get_len(v)}
                          : (pangu_slice){0};
}
static yyjson_doc *parse(pangu_slice json, pg_session_status *status) {
  if (!json.len)
    return NULL;
  yyjson_read_err error;
  yyjson_doc *d =
      yyjson_read_opts((char *)json.data, json.len, 0, NULL, &error);
  if (!d && error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION)
    *status = PG_SESSION_NOMEM;
  /* A malformed/partial tool argument has no source JS argument object. */
  return d;
}
static pg_session_status call(file_ops *ops, pangu_slice name,
                              yyjson_val *args) {
  int read = equal(name, "read"),
      modify = equal(name, "write") || equal(name, "edit");
  if (!read && !modify)
    return PG_SESSION_OK;
  return put(ops, string(yyjson_obj_get(args, "path")), modify);
}
static pg_session_status message(file_ops *ops, const pangu_message *m) {
  pg_session_status status = PG_SESSION_OK;
  if (m->role == PANGU_ASSISTANT) {
    for (size_t i = 0; i < m->content_count && status == PG_SESSION_OK; i++) {
      const pangu_content *c = &m->content[i];
      if (c->kind != PANGU_TOOL_CALL)
        continue;
      yyjson_doc *d = parse(c->data, &status);
      if (d && status == PG_SESSION_OK)
        status = call(ops, c->name, yyjson_doc_get_root(d));
      yyjson_doc_free(d);
    }
  } else if (m->role == PANGU_TOOL_RESULT) {
    /* Standard source toolResult.nestedCalls takes precedence, including {}.
     * Old PanGu producers stored {"nestedCalls":{...}} in details_json; use
     * that adapter only when the standard field is absent. No codemode
     * executor is implied. Truncated argumentsBytes are ignored. */
    int standard = m->nested_calls_json.len != 0;
    yyjson_doc *d = parse(standard ? m->nested_calls_json : m->details_json,
                          &status);
    yyjson_val *root = d ? yyjson_doc_get_root(d) : NULL;
    if (!standard)
      root = yyjson_obj_get(root, "nestedCalls");
    yyjson_val *calls =
                   yyjson_obj_get(root, "calls"),
               *item;
    size_t i, max;
    yyjson_arr_foreach(calls, i, max, item) {
      if (status != PG_SESSION_OK)
        break;
      status = call(ops, string(yyjson_obj_get(item, "name")),
                    yyjson_obj_get(item, "arguments"));
    }
    yyjson_doc_free(d);
  }
  return status;
}
static pg_session_status previous(file_ops *ops, pangu_slice details) {
  pg_session_status status = PG_SESSION_OK;
  yyjson_doc *d = parse(details, &status);
  yyjson_val *root = d ? yyjson_doc_get_root(d) : NULL;
  const char *keys[] = {"readFiles", "modifiedFiles"};
  for (int modified = 0; modified < 2 && status == PG_SESSION_OK; modified++) {
    yyjson_val *list = yyjson_obj_get(root, keys[modified]), *v;
    size_t i, max;
    yyjson_arr_foreach(list, i, max, v) {
      status = put(ops, string(v), modified);
      if (status != PG_SESSION_OK)
        break;
    }
  }
  yyjson_doc_free(d);
  return status;
}
typedef struct {
  const file_path *path;
  size_t pos;
  int pending;
} cursor;
static int next_unit(cursor *c) {
  if (c->pending) {
    int unit = c->pending;
    c->pending = 0;
    return unit;
  }
  if (c->pos == c->path->len)
    return -1;
  utf8proc_int32_t cp;
  size_t remain = c->path->len - c->pos;
  utf8proc_ssize_t n = utf8proc_iterate(
      (const utf8proc_uint8_t *)c->path->path + c->pos,
      (utf8proc_ssize_t)(remain > PTRDIFF_MAX ? PTRDIFF_MAX : remain), &cp);
  if (n < 0) { /* JSON inputs normally guarantee valid UTF-8. */
    cp = (unsigned char)c->path->path[c->pos];
    n = 1;
  }
  c->pos += (size_t)n;
  if (cp > 0xffff) {
    cp -= 0x10000;
    c->pending = 0xdc00 + (cp & 0x3ff);
    return 0xd800 + (cp >> 10);
  }
  return cp;
}
static int compare(const void *left, const void *right) {
  cursor a = {.path = left}, b = {.path = right};
  for (;;) {
    int x = next_unit(&a), y = next_unit(&b);
    if (x != y)
      return x < y ? -1 : 1;
    if (x < 0)
      return 0;
  }
}
/* Return exact source XML suffix and the independently persisted details JSON.
 * Only the compacted prefix contributes new operations; prior generated details
 * carry forward across compactions, regardless of model summary wording. */
pg_session_status pg_compaction_file_metadata(const pg_tree *tree,
                                              const pangu_core *view,
                                              const uint64_t *ids, size_t cut,
                                              char **suffix, size_t *suffix_len,
                                              char **details,
                                              size_t *details_len) {
  *suffix = *details = NULL;
  *suffix_len = *details_len = 0;
  file_ops ops = {0};
  pg_session_status status = PG_SESSION_OK;
  const pangu_message *prior = NULL;
  uint64_t latest = 0;
  for (size_t i = 0; i < pangu_core_message_count(view); i++)
    if (ids[i] > latest &&
        tree->nodes[ids[i] - 1].kind == PG_ENTRY_COMPACTION) {
      latest = ids[i];
      prior = pangu_core_message_at(view, i);
    }
  if (prior)
    status = previous(&ops, prior->details_json);
  for (size_t i = 0; i < cut && status == PG_SESSION_OK; i++)
    status = message(&ops, pangu_core_message_at(view, i));
  if (status != PG_SESSION_OK)
    goto done;
  if (ops.count > 1)
    qsort(ops.paths, ops.count, sizeof(*ops.paths), compare);
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = d ? yyjson_mut_obj(d) : NULL;
  if (!root) {
    yyjson_mut_doc_free(d);
    status = PG_SESSION_NOMEM;
    goto done;
  }
  yyjson_mut_doc_set_root(d, root);
  size_t size = 1;
  for (size_t i = 0; i < ops.count; i++) {
    if (size == SIZE_MAX || ops.paths[i].len > SIZE_MAX - size - 1) {
      status = PG_SESSION_NOMEM;
      break;
    }
    size += ops.paths[i].len + 1;
  }
  if (size > SIZE_MAX - 100)
    status = PG_SESSION_NOMEM;
  char *xml = status == PG_SESSION_OK ? malloc(size + 100) : NULL;
  if (!xml && status == PG_SESSION_OK)
    status = PG_SESSION_NOMEM;
  size_t length = 0;
  const char *keys[] = {"readFiles", "modifiedFiles"};
  const char *open[] = {"\n\n<read-files>\n", "\n\n<modified-files>\n"};
  const char *close[] = {"\n</read-files>", "\n</modified-files>"};
  for (int modified = 0; modified < 2 && status == PG_SESSION_OK; modified++) {
    yyjson_mut_val *array = yyjson_mut_arr(d);
    if (!array || !yyjson_mut_obj_add_val(d, root, keys[modified], array)) {
      status = PG_SESSION_NOMEM;
      break;
    }
    size_t count = 0;
    for (size_t i = 0; i < ops.count; i++) {
      file_path *path = &ops.paths[i];
      if (path->modified != modified)
        continue;
      if (!yyjson_mut_arr_add_strncpy(d, array, path->path, path->len)) {
        status = PG_SESSION_NOMEM;
        break;
      }
      if (!count) {
        size_t n = strlen(open[modified]);
        memcpy(xml + length, open[modified], n);
        length += n;
      } else
        xml[length++] = '\n';
      memcpy(xml + length, path->path, path->len);
      length += path->len;
      count++;
    }
    if (count) {
      size_t n = strlen(close[modified]);
      memcpy(xml + length, close[modified], n);
      length += n;
    }
  }
  if (status == PG_SESSION_OK) {
    *details = yyjson_mut_write(d, 0, details_len);
    if (!*details)
      status = PG_SESSION_NOMEM;
    else {
      xml[length] = 0;
      *suffix = xml;
      *suffix_len = length;
      xml = NULL;
    }
  }
  free(xml);
  yyjson_mut_doc_free(d);
done:
  for (size_t i = 0; i < ops.count; i++)
    free(ops.paths[i].path);
  free(ops.paths);
  if (status != PG_SESSION_OK) {
    free(*details);
    *details = NULL;
    *details_len = 0;
  }
  return status;
}
