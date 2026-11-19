#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <pangu/services/tools.h>
#include <pangu/services/schema.h>
#include <pangu/services/images.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <yyjson.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
typedef struct {
  pg_file_tools_config config;
  pg_scope *scope;
  pg_tools_service service;
  pangu_tool tools[3];
} file_tools;
static int valid(pangu_slice s) {
  return s.data && s.len && !memchr(s.data, 0, s.len);
}
static char *resolve(const char *cwd, pangu_slice path) {
  if (!cwd || !valid(path))
    return NULL;
  size_t prefix = 0;
  int absolute = path.data[0] == '/' || path.data[0] == '\\' ||
                 (path.len > 2 && path.data[1] == ':');
  if (!absolute)
    prefix = strlen(cwd) + 1;
  if (path.len > SIZE_MAX - prefix - 1)
    return NULL;
  char *p = malloc(prefix + path.len + 1);
  if (!p)
    return NULL;
  if (prefix) {
    memcpy(p, cwd, prefix - 1);
    p[prefix - 1] = '/';
  }
  memcpy(p + prefix, path.data, path.len);
  p[prefix + path.len] = 0;
  return p;
}
static pangu_status local_read(void *ctx, pangu_core *owner, pangu_slice path,
                               char **out, size_t *len) {
  *out = NULL;
  *len = 0;
  if (pangu_core_is_cancelled(owner))
    return PANGU_CANCELLED;
  if (!ctx || !valid(path))
    return PANGU_INVALID;
  char *name = resolve(ctx, path);
  if (!name)
    return PANGU_NOMEM;
  uv_fs_t req;
  int fd = uv_fs_open(NULL, &req, name, O_RDONLY, 0, NULL);
  uv_fs_req_cleanup(&req);
  free(name);
  if (fd < 0)
    return PANGU_TOOL_ERROR;
  char *data = NULL;
  size_t size = 0;
  pangu_status st = PANGU_OK;
  for (;;) {
    char chunk[65536];
    uv_buf_t b = uv_buf_init(chunk, sizeof(chunk));
    int n = uv_fs_read(NULL, &req, fd, &b, 1, -1, NULL);
    uv_fs_req_cleanup(&req);
    if (n < 0) {
      st = PANGU_TOOL_ERROR;
      break;
    }
    if (!n)
      break;
    if ((size_t)n > SIZE_MAX - size - 1) {
      st = PANGU_NOMEM;
      break;
    }
    char *p = realloc(data, size + (size_t)n + 1);
    if (!p) {
      st = PANGU_NOMEM;
      break;
    }
    data = p;
    memcpy(data + size, chunk, (size_t)n);
    size += (size_t)n;
    data[size] = 0;
    if (pangu_core_is_cancelled(owner)) {
      st = PANGU_CANCELLED;
      break;
    }
  }
  uv_fs_close(NULL, &req, fd, NULL);
  uv_fs_req_cleanup(&req);
  if (st) {
    free(data);
    return st;
  }
  if (!data) {
    data = calloc(1, 1);
    if (!data)
      return PANGU_NOMEM;
  }
  *out = data;
  *len = size;
  return PANGU_OK;
}
static uv_once_t once = UV_ONCE_INIT;
static uv_mutex_t mutation;
static int lock_status;
static void init_lock(void) { lock_status = uv_mutex_init_recursive(&mutation); }
static pangu_status local_write(void *ctx, pangu_core *owner, pangu_slice path,
                                pangu_slice content) {
  if (pangu_core_is_cancelled(owner))
    return PANGU_CANCELLED;
  if (!ctx || !valid(path))
    return PANGU_INVALID;
  char *name = resolve(ctx, path);
  if (!name)
    return PANGU_NOMEM;
  uv_once(&once, init_lock);
  if (lock_status) {
    free(name);
    return PANGU_TOOL_ERROR;
  }
  uv_mutex_lock(&mutation);
  uv_fs_t req;
  pangu_status st = PANGU_OK;
  for (char *p = name + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char sep = *p;
      *p = 0;
      int rc = uv_fs_mkdir(NULL, &req, name, 0777, NULL);
      uv_fs_req_cleanup(&req);
      *p = sep;
      if (rc < 0 && rc != UV_EEXIST) {
        st = PANGU_TOOL_ERROR;
        goto done;
      }
    }
  }
  if (pangu_core_is_cancelled(owner)) {
    st = PANGU_CANCELLED;
    goto done;
  }
  int fd =
      uv_fs_open(NULL, &req, name, O_CREAT | O_WRONLY | O_TRUNC, 0666, NULL);
  uv_fs_req_cleanup(&req);
  if (fd < 0) {
    st = PANGU_TOOL_ERROR;
    goto done;
  }
  for (size_t off = 0; off < content.len;) {
    size_t n = content.len - off;
    if (n > 65536)
      n = 65536;
    uv_buf_t b = uv_buf_init((char *)content.data + off, (unsigned)n);
    int written = uv_fs_write(NULL, &req, fd, &b, 1, -1, NULL);
    uv_fs_req_cleanup(&req);
    if (written <= 0) {
      st = PANGU_TOOL_ERROR;
      break;
    }
    off += (size_t)written;
  }
  int rc = uv_fs_close(NULL, &req, fd, NULL);
  uv_fs_req_cleanup(&req);
  if (rc < 0)
    st = PANGU_TOOL_ERROR;
  if (pangu_core_is_cancelled(owner))
    st = PANGU_CANCELLED;
done:
  uv_mutex_unlock(&mutation);
  free(name);
  return st;
}
pg_file_environment pg_local_file_environment(const char *cwd) {
  return (pg_file_environment){(void *)cwd, local_read, local_write};
}
static pangu_status validate(void *ctx, pangu_slice schema, pangu_slice args) {
  file_tools *f = ctx;
  return f->config.validate(f->config.validator_ctx, schema, args);
}
static pangu_status validate_arguments(void *ctx, pangu_slice schema,
    pangu_slice args, pangu_arguments_sink sink, void *sink_ctx) {
  file_tools *f=ctx;
  if (f->config.validate_arguments)
    return f->config.validate_arguments(f->config.validator_ctx,schema,args,sink,sink_ctx);
  return pg_schema_normalize(NULL,schema,args,sink,sink_ctx);
}
/* Pi Coding Agent read/truncateHead: select first, then bound whole lines. */
static pangu_status page(yyjson_val *root, char **bytes, size_t *length,
                          char **details, size_t *details_len) {
  char *text = *bytes;
  size_t n = *length, total = 1, start = 0, line = 1;
  for (size_t i = 0; i < n; ++i) total += text[i] == '\n';
  yyjson_val *o = yyjson_obj_get(root, "offset");
  yyjson_val *l = yyjson_obj_get(root, "limit");
  size_t offset = yyjson_is_uint(o) ? yyjson_get_uint(o) : 1;
  size_t limit = yyjson_is_uint(l) ? yyjson_get_uint(l) : SIZE_MAX;
  if (!offset) offset = 1;
  if (offset > total) return PANGU_TOOL_ERROR;
  while (line < offset && start < n) if (text[start++] == '\n') ++line;
  size_t end = start, count = 0;
  while (end < n && count < limit) {
    if (text[end++] == '\n') ++count;
  }
  if (count == limit && end > start && text[end-1] == '\n') --end;
  size_t selected = end - start, kept = 0, lines = 0, total_selected_lines = 0;
  for (size_t i = 0; i < selected; ++i) total_selected_lines += text[start+i] == '\n';
  if (selected && text[end-1] != '\n') ++total_selected_lines;
  int cut = selected > 51200 || total_selected_lines > 2000;
  if (!cut) { kept = selected; lines = total_selected_lines; }
  else {
    size_t position = 0;
    while (position < selected && lines < 2000) {
      size_t next = position;
      while (next < selected && text[start+next] != '\n') ++next;
      if (next > 51200) break;
      kept = next;
      ++lines;
      if (next == selected) break;
      position = next + 1;
    }
  }
  char note[256] = "";
  if (cut && !kept)
    snprintf(note, sizeof(note), "[Line %zu exceeds 50.0KB limit. Use bash to read this line.]", offset);
  else if (cut)
    snprintf(note, sizeof(note), "\n\n[Showing lines %zu-%zu of %zu. Use offset=%zu to continue.]", offset, offset + lines - 1, total, offset + lines);
  else if (limit < total - (offset - 1))
    snprintf(note, sizeof(note), "\n\n[%zu more lines in file. Use offset=%zu to continue.]", total - (offset - 1 + limit), offset + limit);
  size_t extra = strlen(note);
  char *out = malloc(kept + extra + 1);
  if (!out) return PANGU_NOMEM;
  memcpy(out, text + start, kept);
  memcpy(out + kept, note, extra + 1);
  if (cut) {
    yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
    if (!d) { free(out); return PANGU_NOMEM; }
    yyjson_mut_val *obj = yyjson_mut_obj(d), *tr = yyjson_mut_obj(d);
    yyjson_mut_doc_set_root(d, obj);
    int ok = obj && tr && yyjson_mut_obj_add_val(d, obj, "truncation", tr)
      && yyjson_mut_obj_add_bool(d, tr, "truncated", true)
      && yyjson_mut_obj_add_str(d, tr, "truncatedBy", lines >= 2000 ? "lines" : "bytes")
      && yyjson_mut_obj_add_uint(d, tr, "totalLines", total_selected_lines)
      && yyjson_mut_obj_add_uint(d, tr, "totalBytes", selected)
      && yyjson_mut_obj_add_uint(d, tr, "outputLines", lines)
      && yyjson_mut_obj_add_uint(d, tr, "outputBytes", kept)
      && yyjson_mut_obj_add_uint(d, tr, "maxLines", 2000)
      && yyjson_mut_obj_add_uint(d, tr, "maxBytes", 51200)
      && yyjson_mut_obj_add_bool(d, tr, "firstLineExceedsLimit", !lines)
      && yyjson_mut_obj_add_bool(d, tr, "lastLinePartial", false);
    if (ok) *details = yyjson_mut_write(d, 0, details_len);
    yyjson_mut_doc_free(d);
    if (!*details) { free(out); return PANGU_NOMEM; }
  }
  free(text);
  *bytes = out;
  *length = kept + extra;
  return PANGU_OK;
}
static size_t line_count(const char *s, size_t n) {
  size_t lines = 0;
  for (size_t i = 0; i < n; ++i) lines += s[i] == '\n';
  return lines + (n && s[n-1] != '\n');
}
/* A standard unified whole-file hunk; no external process, all bytes length-bound. */
static char *patch_text(pangu_slice path, const char *a, size_t an,
                        const char *b, size_t bn) {
  if (an > (SIZE_MAX-1024)/2 || bn > (SIZE_MAX-1024)/2 - an ||
      path.len > (SIZE_MAX-1024-2*(an+bn))/2) return NULL;
  size_t cap = 1024 + 2*(an+bn) + 2*path.len;
  char *out = malloc(cap);
  if (!out) return NULL;
  if (path.len > INT_MAX) { free(out); return NULL; }
  size_t al = line_count(a, an), bl = line_count(b, bn);
  int header = snprintf(out, cap, "--- %.*s\n+++ %.*s\n@@ -%d,%zu +%d,%zu @@\n",
    (int)path.len, path.data, (int)path.len, path.data, al ? 1 : 0, al, bl ? 1 : 0, bl);
  if (header < 0 || (size_t)header >= cap) { free(out); return NULL; }
  size_t at = (size_t)header;
  for (int side = 0; side < 2; ++side) {
    const char *text = side ? b : a;
    size_t n = side ? bn : an;
    for (size_t i = 0; i < n; ++i) {
      if (!i || text[i-1] == '\n') out[at++] = side ? '+' : '-';
      out[at++] = text[i];
    }
    if (n && text[n-1] != '\n') {
      const char marker[] = "\n\\ No newline at end of file\n";
      memcpy(out+at, marker, sizeof(marker)-1); at += sizeof(marker)-1;
    }
  }
  out[at] = 0;
  return out;
}
static char *normalize_lf(const char *s, size_t n, size_t *length) {
  char *out = malloc(n+1);
  if (!out) return NULL;
  size_t at = 0;
  for (size_t i = 0; i < n; ++i) {
    if (s[i] == '\r' && i+1 < n && s[i+1] == '\n') continue;
    out[at++] = s[i] == '\r' ? '\n' : s[i];
  }
  out[at] = 0; *length = at; return out;
}
typedef struct { size_t start, len; pangu_slice replacement; char *needle; } replacement;
static int compare_replacements(const void *a, const void *b) {
  const replacement *x = a, *y = b;
  return (x->start > y->start) - (x->start < y->start);
}
static pangu_status edit_file(file_tools *f, pangu_core *owner,
                              pangu_slice path, yyjson_val *root, char **details, size_t *details_len) {
  yyjson_val *edits = yyjson_obj_get(root, "edits");
  size_t count = yyjson_arr_size(edits);
  if (!count || count > SIZE_MAX / sizeof(replacement)) return PANGU_TOOL_ERROR;
  replacement *r = calloc(count, sizeof(*r));
  if (!r) return PANGU_NOMEM;
  uv_once(&once, init_lock);
  if (lock_status) { free(r); return PANGU_TOOL_ERROR; }
  uv_mutex_lock(&mutation);
  char *old = NULL, *out = NULL;
  size_t n = 0, output_size = 0;
  int crlf = 0;
  pangu_status st = f->config.environment.read(f->config.environment.ctx, owner, path, &old, &n);
  if (!st && pangu_core_is_cancelled(owner)) st = PANGU_CANCELLED;
  if (st) goto done;
  for (size_t i = 1; i < n; ++i) if (old[i] == '\n') { crlf = old[i-1] == '\r'; break; }
  char *normalized = normalize_lf(old, n, &n);
  if (!normalized) { st = PANGU_NOMEM; goto done; }
  free(old); old = normalized;
  output_size = n;
  for (size_t i = 0; i < count; ++i) {
    yyjson_val *e = yyjson_arr_get(edits, i);
    yyjson_val *a = yyjson_obj_get(e, "oldText"), *b = yyjson_obj_get(e, "newText");
    if (!yyjson_is_str(a) || !yyjson_is_str(b) || !yyjson_get_len(a)) { st = PANGU_TOOL_ERROR; goto done; }
    size_t len = 0, newlen = 0, matches = 0;
    r[i].needle = normalize_lf(yyjson_get_str(a), yyjson_get_len(a), &len);
    char *replacement_text = normalize_lf(yyjson_get_str(b), yyjson_get_len(b), &newlen);
    r[i].replacement = (pangu_slice){replacement_text, newlen};
    if (!r[i].needle || !replacement_text) { st = PANGU_NOMEM; goto done; }
    for (size_t j = 0; j <= n && len <= n-j; ++j)
      if (!memcmp(old+j, r[i].needle, len)) { r[i].start = j; ++matches; }
    if (matches != 1) { st = PANGU_TOOL_ERROR; goto done; }
    r[i].len = len;


  }
  qsort(r, count, sizeof(*r), compare_replacements);
  for (size_t i = 1; i < count; ++i)
    if (r[i-1].start + r[i-1].len > r[i].start) { st = PANGU_TOOL_ERROR; goto done; }
  for (size_t i = 0; i < count; ++i) {
    if (r[i].replacement.len > SIZE_MAX - (output_size - r[i].len) - 1) { st = PANGU_NOMEM; goto done; }
    output_size = output_size - r[i].len + r[i].replacement.len;
  }
  out = malloc(output_size + 1);
  if (!out) { st = PANGU_NOMEM; goto done; }
  size_t from = 0, to = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t span = r[i].start - from;
    memcpy(out+to, old+from, span); to += span;
    memcpy(out+to, r[i].replacement.data, r[i].replacement.len); to += r[i].replacement.len;
    from = r[i].start + r[i].len;
  }
  memcpy(out+to, old+from, n-from);
  if (n == output_size && !memcmp(out, old, n)) { st = PANGU_TOOL_ERROR; goto done; }
  /* Build presentation data before committing: allocation failure has no write side effect. */
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d) { st = PANGU_NOMEM; goto done; }
  yyjson_mut_val *obj = yyjson_mut_obj(d);
  yyjson_mut_doc_set_root(d, obj);
  size_t first = 1;
  for (size_t i = 0; i < r[0].start; ++i) first += old[i] == '\n';
  char *patch = patch_text(path, old, n, out, output_size);
  int ok = obj && patch && yyjson_mut_obj_add_uint(d, obj, "firstChangedLine", first)
      && yyjson_mut_obj_add_strcpy(d, obj, "diff", patch)
      && yyjson_mut_obj_add_strcpy(d, obj, "patch", patch);
  free(patch);
  if (ok) *details = yyjson_mut_write(d, 0, details_len);
  yyjson_mut_doc_free(d);
  if (!*details) { st = PANGU_NOMEM; goto done; }
  if (crlf) {
    size_t newlines = 0;
    for (size_t i = 0; i < output_size; ++i) newlines += out[i] == '\n';
    if (newlines > SIZE_MAX-output_size-1) { st = PANGU_NOMEM; goto done; }
    char *restored = malloc(output_size+newlines+1);
    if (!restored) { st = PANGU_NOMEM; goto done; }
    size_t at = 0;
    for (size_t i = 0; i < output_size; ++i) {
      if (out[i] == '\n') restored[at++] = '\r';
      restored[at++] = out[i];
    }
    free(out); out = restored; output_size = at;
  }
  if (pangu_core_is_cancelled(owner)) { st = PANGU_CANCELLED; goto done; }
  st = f->config.environment.write(f->config.environment.ctx, owner, path, (pangu_slice){out, output_size});
  if (!st && pangu_core_is_cancelled(owner)) st = PANGU_CANCELLED;
done:
  free(out); free(old);
  for (size_t i = 0; i < count; ++i) { free(r[i].needle); free((void *)r[i].replacement.data); }
  free(r);
  uv_mutex_unlock(&mutation);
  return st;
}
static pangu_status execute(void *ctx, pangu_core *owner,
                            const pangu_content *call, pangu_tool_sink sink,
                            void *sink_ctx) {
  file_tools *f = ctx;
  if (pangu_core_is_cancelled(owner)) return PANGU_CANCELLED;
  yyjson_doc *doc = yyjson_read(call->data.data, call->data.len, 0);
  if (!doc)
    return PANGU_TOOL_ERROR;
  yyjson_val *root = yyjson_doc_get_root(doc),
             *p = yyjson_obj_get(root, "path"),
             *c = yyjson_obj_get(root, "content");
  if (!yyjson_is_str(p)) {
    yyjson_doc_free(doc);
    return PANGU_TOOL_ERROR;
  }
  pangu_slice path = {yyjson_get_str(p), yyjson_get_len(p)};
  char *result = NULL;
  size_t len = 0, details_len = 0;
  char *details = NULL;
  pangu_status status;
  int writing = call->name.len == 5 && !memcmp(call->name.data, "write", 5);
  int editing = call->name.len == 4 && !memcmp(call->name.data, "edit", 4);
  if (editing) {
    status = edit_file(f, owner, path, root, &details, &details_len);
  } else if (writing) {
    if (!yyjson_is_str(c)) {
      yyjson_doc_free(doc);
      return PANGU_TOOL_ERROR;
    }
    uv_once(&once, init_lock);
    if (lock_status) status = PANGU_TOOL_ERROR;
    else {
      uv_mutex_lock(&mutation);
      status = pangu_core_is_cancelled(owner) ? PANGU_CANCELLED :
          f->config.environment.write(f->config.environment.ctx, owner, path,
              (pangu_slice){yyjson_get_str(c), yyjson_get_len(c)});
      uv_mutex_unlock(&mutation);
    }
  } else
    status = f->config.environment.read(f->config.environment.ctx, owner, path,
                                        &result, &len);
  pg_lease *image_lease = NULL;
  pangu_slice mime = {0};
  if (!status && !writing && !editing) {
    pg_status found = pg_service_acquire(f->scope, PG_NAME(PG_IMAGES_SERVICE), 1, &image_lease);
    if (found == PG_OK) {
      const pg_images_service *images = pg_lease_table(image_lease);
      char *encoded = NULL;
      size_t encoded_len = 0;
      status = images->prepare(pg_lease_context(image_lease), owner,
                              (pangu_slice){result, len}, &encoded, &encoded_len, &mime);
      if (status == PANGU_INVALID) status = page(root, &result, &len, &details, &details_len);
      else if (!status) { free(result); result = encoded; len = encoded_len; }
    } else if (found == PG_NOT_FOUND) status = memchr(result, 0, len) ? PANGU_TOOL_ERROR : page(root, &result, &len, &details, &details_len);
    else status = PANGU_TOOL_ERROR;
  }
  yyjson_doc_free(doc);
  if (!status && pangu_core_is_cancelled(owner)) status = PANGU_CANCELLED;
  if (status) {
    free(result);
    free(details);
    if (image_lease) pg_lease_release(image_lease);
    return status;
  }
  pangu_content content = {.kind = mime.len ? PANGU_IMAGE : PANGU_TEXT,
                           .mime_type = mime,
                           .data = editing ? SL("Successfully replaced blocks") : writing ? SL("Successfully wrote file")
                                           : (pangu_slice){result, len}};
  pangu_message message = {
      .role = PANGU_TOOL_RESULT, .content = &content, .content_count = 1,
      .details_json = {details, details_len}};
  status = sink(sink_ctx, &message, 1);
  free(result);
  free(details);
  if (image_lease) pg_lease_release(image_lease);
  return status;
}
static void destroy(void *p) { free(p); }
static pg_status install(pg_scope *scope, const void *cfg, size_t size) {
  if (!cfg || size != sizeof(pg_file_tools_config))
    return PG_INVALID;
  const pg_file_tools_config *config = cfg;
  if ((!config->validate && !config->validate_arguments) || !config->environment.read ||
      !config->environment.write)
    return PG_INVALID;
  file_tools *f = calloc(1, sizeof(*f));
  if (!f)
    return PG_NOMEM;
  f->config = *config;
  f->scope = scope;
  f->tools[0] = (pangu_tool){
      .name = SL("read"),
      .description = SL("Read a text file, at most 2000 lines or 50KB; use offset/limit to continue"),
      .schema_json = SL(
          "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"offset\":{\"type\":\"integer\",\"minimum\":1},\"limit\":{\"type\":\"integer\",\"minimum\":0}}"
          ",\"required\":[\"path\"],\"additionalProperties\":false}"),
      .ctx = f,
      .validate = config->validate ? validate : NULL,
      .validate_call = (!config->validate_arguments && config->validate==pg_schema_validate)?pg_schema_normalize_tool:NULL,
      .validate_arguments = (config->validate_arguments || config->validate==pg_schema_validate)?validate_arguments:NULL,
      .execute = execute};
  f->tools[1] = (pangu_tool){
      .name = SL("write"),
      .description =
          SL("Create or overwrite a file, creating parent directories"),
      .schema_json =
          SL("{\"type\":\"object\",\"properties\":{\"path\":{\"type\":"
             "\"string\"},\"content\":{\"type\":\"string\"}},\"required\":["
             "\"path\",\"content\"],\"additionalProperties\":false}"),
      .ctx = f,
      .validate = config->validate ? validate : NULL,
      .validate_call = (!config->validate_arguments && config->validate==pg_schema_validate)?pg_schema_normalize_tool:NULL,
      .validate_arguments = (config->validate_arguments || config->validate==pg_schema_validate)?validate_arguments:NULL,
      .execute = execute,
      .execution_mode = PANGU_TOOLS_SEQUENTIAL};
  f->tools[2] = (pangu_tool){
      .name = SL("edit"),
      .description = SL("Replace unique non-overlapping original text regions in one file"),
      .schema_json = SL("{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"edits\":{\"type\":\"array\",\"minItems\":1,\"items\":{\"type\":\"object\",\"properties\":{\"oldText\":{\"type\":\"string\",\"minLength\":1},\"newText\":{\"type\":\"string\"}},\"required\":[\"oldText\",\"newText\"],\"additionalProperties\":false}}},\"required\":[\"path\",\"edits\"],\"additionalProperties\":false}"),
      .ctx = f, .validate = config->validate ? validate : NULL,
      .validate_call = (!config->validate_arguments && config->validate==pg_schema_validate)?pg_schema_normalize_tool:NULL,
      .validate_arguments = (config->validate_arguments || config->validate==pg_schema_validate)?validate_arguments:NULL, .execute = execute,
      .execution_mode = PANGU_TOOLS_SEQUENTIAL};
  f->service = (pg_tools_service){f->tools, 3};
  pg_status st = pg_scope_defer(scope, destroy, f);
  if (st) {
    free(f);
    return st;
  }
  return pg_service_register(scope, PG_NAME(PG_TOOLS_SERVICE), 1, &f->service,
                             f);
}
const pg_plugin pg_file_tools_plugin = {
    1,
    sizeof(pg_plugin),
    {"pangu.tools.files", sizeof("pangu.tools.files") - 1},
    NULL,
    0,
    install};
