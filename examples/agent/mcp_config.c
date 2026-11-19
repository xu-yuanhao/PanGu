/* Pi d5b3de22 extensions/mcp/config.ts and core/mcp-servers.ts (MIT).
 * Explicit-file stdio/HTTP direct subset; process/protocol lives in Services. */
#include "mcp_config.h"
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <yyjson.h>
struct pg_cli_mcp_config {
  yyjson_doc *doc;
  pg_mcp_stdio_config *servers;
  pg_mcp_http_config *http;
  size_t count;
};
static const char *cstring(yyjson_val *v) {
  const char *s = yyjson_get_str(v);
  return s && !memchr(s, 0, yyjson_get_len(v)) ? s : NULL;
}
static int equal(yyjson_val *v, const char *s) {
  const char *a = cstring(v);
  return a && !strcmp(a, s);
}
static char *absolute(const char *cwd, const char *path, pg_status *status) {
  size_t a = strlen(cwd), b = strlen(path);
  if (a > SIZE_MAX - 2 || b > SIZE_MAX - a - 2) {
    *status = PG_NOMEM;
    return NULL;
  }
  char *joined = malloc(a + b + 2);
  if (!joined) {
    *status = PG_NOMEM;
    return NULL;
  }
  if (path[0] == '/' || (b > 2 && path[1] == ':'))
    memcpy(joined, path, b + 1);
  else {
    memcpy(joined, cwd, a);
    joined[a] = '/';
    memcpy(joined + a + 1, path, b + 1);
  }
  uv_fs_t req;
  int z = uv_fs_realpath(NULL, &req, joined, NULL);
  free(joined);
  char *result = NULL;
  if (!z) {
    size_t n = strlen(req.ptr);
    result = malloc(n + 1);
    if (result)
      memcpy(result, req.ptr, n + 1);
    else
      *status = PG_NOMEM;
  }
  if (z == UV_ENOMEM)
    *status = PG_NOMEM;
  uv_fs_req_cleanup(&req);
  return result;
}
void pg_cli_mcp_free(pg_cli_mcp_config *c) {
  if (!c)
    return;
  for (size_t i = 0; i < c->count; i++) {
    for (size_t j = 0; j < c->http[i].header_count; j++)
      free((void *)c->http[i].headers[j].value.data);
    free((void *)c->http[i].headers);
    free((void *)c->servers[i].argv);
    free((void *)c->servers[i].cwd);
    free((void *)c->servers[i].service_name.data);
    free((void *)c->servers[i].tool_prefix.data);
  }
  free(c->servers);
  free(c->http);
  yyjson_doc_free(c->doc);
  free(c);
}
size_t pg_cli_mcp_count(const pg_cli_mcp_config *c) { return c ? c->count : 0; }
const pg_mcp_stdio_config *pg_cli_mcp_at(const pg_cli_mcp_config *c, size_t i) {
  return c && i < c->count ? &c->servers[i] : NULL;
}
const pg_mcp_http_config *pg_cli_mcp_http_at(const pg_cli_mcp_config *c, size_t i) {
  return c && i < c->count && c->http[i].url.size ? &c->http[i] : NULL;
}
/* Pi parseConfigValueTemplate/resolveHeadersOrThrow pure-env subset.
 * Scan source once: expansions and escaped dollars are never reinterpreted. */
static int env_start(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static int env_rest(unsigned char c) {
  return env_start(c) || (c >= '0' && c <= '9');
}
static char *header_value(const char *s, pg_status *status) {
  if (*s == '!') return NULL; /* Explicitly unsupported, never invoke a shell. */
  size_t len = 0;
  char *out = calloc(1, 1);
  if (!out) { *status = PG_NOMEM; return NULL; }
  while (*s) {
    const char *part = s, *key = NULL;
    size_t n = 1, k = 0;
    if (s[0] == '$' && (s[1] == '$' || s[1] == '!')) {
      part = s + 1;
      s += 2;
    } else if (s[0] == '$' && s[1] == '{') {
      const char *end = strchr(s + 2, '}');
      if (end) {
        n = (size_t)(end - s + 1);
        const char *p = s + 2;
        if (p < end && env_start((unsigned char)*p)) {
          p++;
          while (p < end && env_rest((unsigned char)*p)) p++;
          if (p == end) { key = s + 2; k = (size_t)(end - key); }
        }
      } /* Unclosed brace: literal '$', then continue scanning the source. */
      s += n;
    } else if (s[0] == '$' && env_start((unsigned char)s[1])) {
      key = s + 1;
      const char *end = key + 1;
      while (env_rest((unsigned char)*end)) end++;
      k = (size_t)(end - key);
      s = end;
    } else s++;
    if (key) {
      char *name = malloc(k + 1);
      if (!name) { *status = PG_NOMEM; free(out); return NULL; }
      memcpy(name, key, k); name[k] = 0;
      part = getenv(name); free(name);
      /* Pi env?.[name] || process.env[name] || undefined: empty is missing.
       * OrThrow fails the entire header/config instead of omitting a header. */
      if (!part || !*part) { free(out); return NULL; }
      n = strlen(part);
    }
    if (n > SIZE_MAX - len - 1) { *status = PG_NOMEM; free(out); return NULL; }
    char *next = realloc(out, len + n + 1);
    if (!next) { *status = PG_NOMEM; free(out); return NULL; }
    out = next; memcpy(out + len, part, n); len += n; out[len] = 0;
  }
  if (strchr(out, '\r') || strchr(out, '\n')) { free(out); return NULL; }
  return out;
}
pg_status pg_cli_mcp_read(const char *path, const char *cwd,
                          pg_cli_mcp_config **out) {
  if (!out) return PG_INVALID;
  *out = NULL;
  if (!path || !cwd) return PG_INVALID;
  pg_cli_mcp_config *c = calloc(1, sizeof(*c));
  if (!c)
    return PG_NOMEM;
  pg_status status = PG_INVALID;
  char *file = absolute(cwd, path, &status);
  if (!file) {
    pg_cli_mcp_free(c);
    return status;
  }
  yyjson_read_err error;
  c->doc = yyjson_read_file(file, YYJSON_READ_ALLOW_BOM, NULL, &error);
  free(file);
  if (!c->doc) {
    if (error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION)
      status = PG_NOMEM;
    goto fail;
  }
  yyjson_val *root = yyjson_doc_get_root(c->doc);
  yyjson_val *servers = yyjson_obj_get(root, "mcpServers");
  if (!yyjson_is_obj(root) || !yyjson_is_obj(servers) ||
      yyjson_obj_size(root) != 1)
    goto fail;
  size_t total = yyjson_obj_size(servers);
  if (total > SIZE_MAX / sizeof(*c->servers)) {
    status = PG_NOMEM;
    goto fail;
  }
  c->servers = total ? calloc(total, sizeof(*c->servers)) : NULL;
  c->http = total ? calloc(total, sizeof(*c->http)) : NULL;
  if (total && (!c->servers || !c->http)) {
    status = PG_NOMEM;
    goto fail;
  }
  size_t index, maximum;
  yyjson_val *key, *value;
  yyjson_obj_foreach(servers, index, maximum, key, value) {
    const char *name = cstring(key);
    size_t len = yyjson_get_len(key);
    if (!name || !len || !yyjson_is_obj(value))
      goto fail;
    for (size_t j = 0; j < len; j++) {
      unsigned char ch = (unsigned char)name[j];
      if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
        goto fail;
    }
    size_t vi, vmax;
    yyjson_val *k, *v;
    yyjson_obj_foreach(value, vi, vmax, k, v) {
      if (!equal(k, "command") && !equal(k, "args") && !equal(k, "cwd") &&
          !equal(k, "enabled") && !equal(k, "type") && !equal(k, "timeout") &&
          !equal(k, "url") && !equal(k, "headers"))
        goto fail; /* Unsupported credentials/transport/exposure never ignored.
                    */
    }
    yyjson_val *command = yyjson_obj_get(value, "command");
    yyjson_val *args = yyjson_obj_get(value, "args");
    yyjson_val *enabled = yyjson_obj_get(value, "enabled");
    yyjson_val *type = yyjson_obj_get(value, "type");
    yyjson_val *dir = yyjson_obj_get(value, "cwd");
    yyjson_val *timeout = yyjson_obj_get(value, "timeout");
    yyjson_val *url = yyjson_obj_get(value, "url");
    yyjson_val *headers = yyjson_obj_get(value, "headers");
    int http = url != NULL;
    const char *endpoint = cstring(url);
    if (http && (!endpoint ||
        (strncmp(endpoint, "http://", 7) && strncmp(endpoint, "https://", 8)) ||
        !strstr(endpoint, "://")[3] || strpbrk(endpoint, "\r\n ") ||
        command || args || dir || (headers && !yyjson_is_obj(headers)) ||
        (type && !equal(type, "http") && !equal(type, "streamable-http"))))
      goto fail;
    if (!http && headers) goto fail;
    const char *exe = cstring(command);
    if ((!http && (!exe || !*exe || (args && !yyjson_is_arr(args)))) ||
        (enabled && !yyjson_is_bool(enabled)) ||
        (!http && type && !equal(type, "stdio")) || (dir && !cstring(dir)))
      goto fail;
    size_t ai, amax;
    yyjson_val *arg;
    yyjson_arr_foreach(args, ai, amax, arg) if (!cstring(arg)) goto fail;
    double ms = timeout ? yyjson_get_num(timeout) * 1000 : 30000;
    if ((timeout && !yyjson_is_num(timeout)) || !(ms >= 1) ||
        ms >= (double)UINT64_MAX)
      goto fail;
    
    pg_mcp_stdio_config *s = &c->servers[c->count++];
    if (len > SIZE_MAX - 16) {
      status = PG_NOMEM;
      goto fail;
    }
    char *service = malloc(len + 11), *prefix = malloc(len + 8);
    s->service_name = (pg_name){service, len + 10};
    s->tool_prefix = (pg_name){prefix, len + 7};
    if (!service || !prefix) {
      status = PG_NOMEM;
      goto fail;
    }
    memcpy(service, "pangu.mcp.", 10);
    memcpy(service + 10, name, len + 1);
    memcpy(prefix, "mcp__", 5);
    for (size_t j = 0; j < len; j++)
      prefix[5 + j] = name[j] == '-' ? '_' : name[j];
    memcpy(prefix + 5 + len, "__", 3);
    for (size_t j = 0; j + 1 < c->count; j++)
      if (!strcmp(c->servers[j].tool_prefix.data, prefix))
        goto fail;
    if (http) {
      pg_mcp_http_config *h = &c->http[c->count - 1];
      h->service_name = s->service_name; h->tool_prefix = s->tool_prefix;
      h->url = (pg_name){endpoint, strlen(endpoint)};
      h->timeout_ms = (uint64_t)ms;
      size_t hn = yyjson_obj_size(headers);
      pg_mcp_http_header *table = hn ? calloc(hn, sizeof(*table)) : NULL;
      if (hn && !table) { status = PG_NOMEM; goto fail; }
      h->headers = table;
      yyjson_obj_foreach(headers, vi, vmax, k, v) {
        const char *key = cstring(k), *raw = cstring(v);
        if (!key || !*key || !raw) goto fail;
        for (const char *p = key; *p; p++)
          if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                (*p >= '0' && *p <= '9') || strchr("!#$%&'*+-.^_`|~", *p))) goto fail;
        char *expanded = header_value(raw, &status);
        if (!expanded) goto fail;
        table[h->header_count++] = (pg_mcp_http_header){
            {key, strlen(key)}, {expanded, strlen(expanded)}};
      }
#ifndef PANGU_AGENT_MCP_HTTP
      if (!enabled || yyjson_get_bool(enabled)) { status = PG_NOT_FOUND; goto fail; }
#endif
    } else {
    size_t count = yyjson_arr_size(args);
    if (count > SIZE_MAX / sizeof(char *) - 2) {
      status = PG_NOMEM;
      goto fail;
    }
    const char **argv = calloc(count + 2, sizeof(*argv));
    if (!argv) {
      status = PG_NOMEM;
      goto fail;
    }
    s->argv = argv;
    argv[0] = exe;
    yyjson_arr_foreach(args, ai, amax, arg) argv[ai + 1] = cstring(arg);
    s->cwd = absolute(cwd, dir ? cstring(dir) : cwd, &status);
    if (!s->cwd)
      goto fail;
    s->timeout_ms = (uint64_t)ms;
    }
    /* Keep validated disabled entries out of installation, releasing their data. */
    if (enabled && !yyjson_get_bool(enabled)) {
      size_t i = --c->count;
      for (size_t j = 0; j < c->http[i].header_count; j++)
        free((void *)c->http[i].headers[j].value.data);
      free((void *)c->http[i].headers);
      free((void *)s->argv); free((void *)s->cwd);
      free((void *)s->service_name.data); free((void *)s->tool_prefix.data);
      memset(s, 0, sizeof(*s)); memset(&c->http[i], 0, sizeof(c->http[i]));
    }
  }
  *out = c;
  return PG_OK;
fail:
  pg_cli_mcp_free(c);
  return status;
}
