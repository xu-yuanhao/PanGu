/* Shared MCP client behavior for stdio and HTTP; Pi snapshot in service TODO.
 */
#include "mcp-client.h"
#include <stdlib.h>
#include <string.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
static int same(yyjson_val *v, const char *s) {
  return yyjson_is_str(v) && yyjson_get_len(v) == strlen(s) &&
         !memcmp(yyjson_get_str(v), s, strlen(s));
}
static pangu_slice slice(yyjson_val *v) {
  return (pangu_slice){yyjson_get_str(v), yyjson_get_len(v)};
}
static char *copy(const char *s, size_t n) {
  if (n == SIZE_MAX)
    return NULL;
  char *p = malloc(n + 1);
  if (p) {
    if (n)
      memcpy(p, s, n);
    p[n] = 0;
  }
  return p;
}
static pangu_status request(client *c, const char *m, yyjson_val *p,
                            pangu_core *core, yyjson_doc **out) {
  return c->request(c, m, p, core, out);
}
static pangu_status send_message(client *c, const char *m, yyjson_val *p,
                                 uint64_t id) {
  return c->send(c, m, p, id);
}
static pangu_status validate(void *ctx, pangu_slice schema, pangu_slice args) {
  tool_context *t = ctx;
  return t->client->cfg.validate(t->client->cfg.validator_ctx, schema, args);
}
static pangu_status tool_error(pangu_tool_sink sink, void *out,
                               const char *text) {
  pangu_content block = {.kind = PANGU_TEXT, .data = {text, strlen(text)}};
  pangu_message m = {.role = PANGU_TOOL_RESULT,
                     .content = &block,
                     .content_count = 1,
                     .is_error = 1};
  return sink(out, &m, 1);
}
static pangu_status execute(void *ctx, pangu_core *core,
                            const pangu_content *call, pangu_tool_sink sink,
                            void *out) {
  tool_context *t = ctx;
  client *c = t->client;
  yyjson_doc *args = yyjson_read(call->data.data, call->data.len, 0);
  if (!args || !yyjson_is_obj(yyjson_doc_get_root(args))) {
    yyjson_doc_free(args);
    return tool_error(sink, out, "Invalid MCP tool arguments");
  }
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d) {
    yyjson_doc_free(args);
    return PANGU_NOMEM;
  }
  yyjson_mut_val *p = yyjson_mut_obj(d),
                 *a = yyjson_val_mut_copy(d, yyjson_doc_get_root(args));
  int ok = p && a && yyjson_mut_obj_add_str(d, p, "name", t->remote) &&
           yyjson_mut_obj_add_val(d, p, "arguments", a);
  yyjson_mut_doc_set_root(d, p);
  size_t n = 0;
  char *json = ok ? yyjson_mut_write(d, 0, &n) : NULL;
  yyjson_mut_doc_free(d);
  yyjson_doc_free(args);
  if (!json)
    return PANGU_NOMEM;
  yyjson_doc *params = yyjson_read(json, n, 0);
  free(json);
  if (!params)
    return PANGU_NOMEM;
  yyjson_doc *reply = NULL;
  pangu_status st =
      request(c, "tools/call", yyjson_doc_get_root(params), core, &reply);
  yyjson_doc_free(params);
  if (st == PANGU_CANCELLED || st == PANGU_NOMEM)
    return st;
  if (st != PANGU_OK)
    return tool_error(sink, out, c->last_error);
  yyjson_val *root = yyjson_doc_get_root(reply),
             *error = yyjson_obj_get(root, "error"),
             *result = yyjson_obj_get(root, "result");
  char *details = yyjson_val_write(error ? error : result, 0, &n);
  if (!details) {
    yyjson_doc_free(reply);
    return PANGU_NOMEM;
  }
  pangu_message m = {.role = PANGU_TOOL_RESULT, .details_json = {details, n}};
  pangu_content single = {.kind = PANGU_TEXT};
  pangu_content *blocks = NULL;
  char *structured = NULL;
  if (error) {
    single.data = slice(yyjson_obj_get(error, "message"));
    m.is_error = 1;
    m.content = &single;
    m.content_count = 1;
  } else {
    yyjson_val *content = yyjson_obj_get(result, "content"),
               *sc = yyjson_obj_get(result, "structuredContent");
    if (!yyjson_is_obj(result) || (content && !yyjson_is_arr(content))) {
      free(details);
      yyjson_doc_free(reply);
      return tool_error(sink, out, "Invalid MCP tools/call result");
    }
    m.is_error = yyjson_is_true(yyjson_obj_get(result, "isError"));
    if (sc) {
      size_t len = 0;
      structured = yyjson_val_write(sc, YYJSON_WRITE_PRETTY, &len);
      if (!structured) {
        free(details);
        yyjson_doc_free(reply);
        return PANGU_NOMEM;
      }
      m.structured_json = (pangu_slice){structured, len};
    }
    size_t count = yyjson_arr_size(content);
    if (count) {
      blocks = calloc(count, sizeof(*blocks));
      if (!blocks) {
        free(structured);
        free(details);
        yyjson_doc_free(reply);
        return PANGU_NOMEM;
      }
      size_t i, max;
      yyjson_val *v;
      yyjson_arr_foreach(content, i, max, v) {
        blocks[i].kind = PANGU_TEXT;
        yyjson_val *type = yyjson_obj_get(v, "type");
        if (same(type, "text") && yyjson_is_str(yyjson_obj_get(v, "text")))
          blocks[i].data = slice(yyjson_obj_get(v, "text"));
        else if (same(type, "image") &&
                 yyjson_is_str(yyjson_obj_get(v, "data")) &&
                 yyjson_is_str(yyjson_obj_get(v, "mimeType"))) {
          blocks[i].kind = PANGU_IMAGE;
          blocks[i].data = slice(yyjson_obj_get(v, "data"));
          blocks[i].mime_type = slice(yyjson_obj_get(v, "mimeType"));
        } else if (same(type, "resource") &&
                   yyjson_is_str(
                       yyjson_obj_get(yyjson_obj_get(v, "resource"), "text")))
          blocks[i].data =
              slice(yyjson_obj_get(yyjson_obj_get(v, "resource"), "text"));
        else
          blocks[i].data = SL("[unsupported MCP content; see details]");
      }
      m.content = blocks;
      m.content_count = count;
    } else if (sc) {
      single.data = m.structured_json;
      m.content = &single;
      m.content_count = 1;
    }
  }
  st = sink(out, &m, 1);
  free(blocks);
  free(structured);
  free(details);
  yyjson_doc_free(reply);
  return st;
}
static pg_status discover(client *c, pg_name prefix) {
  char **cursors = NULL;
  size_t cursor_count = 0;
  pg_status st = PG_FAILED;
  for (size_t page = 0; page < 1000; ++page) {
    yyjson_mut_doc *pd = yyjson_mut_doc_new(NULL);
    if (!pd) {
      st = PG_NOMEM;
      break;
    }
    yyjson_mut_val *po = yyjson_mut_obj(pd);
    int ok = po && (!cursor_count ||
                    yyjson_mut_obj_add_str(pd, po, "cursor",
                                           cursors[cursor_count - 1]));
    yyjson_mut_doc_set_root(pd, po);
    size_t n;
    char *encoded = ok ? yyjson_mut_write(pd, 0, &n) : NULL;
    yyjson_mut_doc_free(pd);
    if (!encoded) {
      st = PG_NOMEM;
      break;
    }
    yyjson_doc *params = yyjson_read(encoded, n, 0);
    free(encoded);
    if (!params) {
      st = PG_NOMEM;
      break;
    }
    yyjson_doc *reply = NULL;
    pangu_status rs =
        request(c, "tools/list", yyjson_doc_get_root(params), NULL, &reply);
    yyjson_doc_free(params);
    if (rs != PANGU_OK)
      break;
    yyjson_val *result = yyjson_obj_get(yyjson_doc_get_root(reply), "result"),
               *tools = yyjson_obj_get(result, "tools"),
               *cursor = yyjson_obj_get(result, "nextCursor");
    if (!yyjson_is_arr(tools) ||
        (cursor && !yyjson_is_null(cursor) && !yyjson_is_str(cursor))) {
      yyjson_doc_free(reply);
      break;
    }
    size_t i, max;
    yyjson_val *v;
    int invalid = 0;
    yyjson_arr_foreach(tools, i, max, v) {
      yyjson_val *name = yyjson_obj_get(v, "name"),
                 *schema = yyjson_obj_get(v, "inputSchema"),
                 *desc = yyjson_obj_get(v, "description");
      size_t len = yyjson_get_len(name);
      if (!yyjson_is_str(name) || !len ||
          memchr(yyjson_get_str(name), 0, len) || !yyjson_is_obj(schema) ||
          (desc && !yyjson_is_str(desc)) || len > SIZE_MAX - prefix.size - 1) {
        invalid = 1;
        break;
      }
      size_t index = c->service.count;
      if (index >= SIZE_MAX / sizeof(pangu_tool) - 1 ||
          index >= SIZE_MAX / sizeof(tool_context) - 1) {
        st = PG_NOMEM;
        invalid = 1;
        break;
      }
      pangu_tool *tt = realloc(c->tools, (index + 1) * sizeof(*tt));
      if (!tt) {
        st = PG_NOMEM;
        invalid = 1;
        break;
      }
      c->tools = tt;
      tool_context *cc = realloc(c->contexts, (index + 1) * sizeof(*cc));
      if (!cc) {
        st = PG_NOMEM;
        invalid = 1;
        break;
      }
      c->contexts = cc;
      char *local = malloc(prefix.size + len + 1),
           *remote = copy(yyjson_get_str(name), len),
           *description =
               copy(desc ? yyjson_get_str(desc) : "", yyjson_get_len(desc)),
           *sj = yyjson_val_write(schema, 0, &n);
      if (!local || !remote || !description || !sj) {
        free(local);
        free(remote);
        free(description);
        free(sj);
        st = PG_NOMEM;
        invalid = 1;
        break;
      }
      if (prefix.size)
        memcpy(local, prefix.data, prefix.size);
      memcpy(local + prefix.size, remote, len + 1);
      for (size_t j = 0; j < index; ++j)
        if (!strcmp(c->contexts[j].remote, remote))
          invalid = 1;
      if (invalid) {
        free(local);
        free(remote);
        free(description);
        free(sj);
        break;
      }
      c->contexts[index] = (tool_context){c, remote};
      c->tools[index] =
          (pangu_tool){.name = {local, prefix.size + len},
                       .description = {description, yyjson_get_len(desc)},
                       .schema_json = {sj, n},
                       .validate = validate,
                       .execute = execute,
                       .execution_mode = PANGU_TOOLS_SEQUENTIAL};
      ++c->service.count;
    }
    if (invalid) {
      yyjson_doc_free(reply);
      break;
    }
    if (!cursor || yyjson_is_null(cursor) || !yyjson_get_len(cursor)) {
      yyjson_doc_free(reply);
      st = PG_OK;
      break;
    }
    if (memchr(yyjson_get_str(cursor), 0, yyjson_get_len(cursor))) {
      yyjson_doc_free(reply);
      break;
    }
    char *next = copy(yyjson_get_str(cursor), yyjson_get_len(cursor));
    if (!next) {
      yyjson_doc_free(reply);
      st = PG_NOMEM;
      break;
    }
    for (size_t j = 0; j < cursor_count; ++j)
      if (!strcmp(cursors[j], next))
        invalid = 1;
    yyjson_doc_free(reply);
    if (invalid) {
      free(next);
      break;
    }
    char **cs = realloc(cursors, (cursor_count + 1) * sizeof(*cs));
    if (!cs) {
      free(next);
      st = PG_NOMEM;
      break;
    }
    cursors = cs;
    cursors[cursor_count++] = next;
  }
  for (size_t i = 0; i < cursor_count; ++i)
    free(cursors[i]);
  free(cursors);
  if (st == PG_OK) {
    c->service.tools = c->tools;
    for (size_t i = 0; i < c->service.count; ++i)
      c->tools[i].ctx = &c->contexts[i];
  }
  return st;
}
void pg_mcp_client_clear(client *c) {
  for (size_t i = 0; i < c->service.count; ++i) {
    free((void *)c->tools[i].name.data);
    free((void *)c->tools[i].description.data);
    free((void *)c->tools[i].schema_json.data);
    free(c->contexts[i].remote);
  }
  free(c->tools);
  free(c->contexts);
}
pg_status pg_mcp_client_publish(client *c, pg_scope *scope,
                                pg_name service_name, pg_name prefix) {
  const char init[] = "{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},"
                      "\"clientInfo\":{\"name\":\"PanGu\",\"version\":\"1\"}}";
  yyjson_doc *params = yyjson_read(init, sizeof(init) - 1, 0), *reply = NULL;
  if (!params)
    return PG_NOMEM;
  pangu_status rs =
      request(c, "initialize", yyjson_doc_get_root(params), NULL, &reply);
  yyjson_doc_free(params);
  if (rs != PANGU_OK)
    return rs == PANGU_NOMEM ? PG_NOMEM : PG_FAILED;
  yyjson_val *r = yyjson_obj_get(yyjson_doc_get_root(reply), "result"),
             *version = yyjson_obj_get(r, "protocolVersion"),
             *info = yyjson_obj_get(r, "serverInfo"),
             *instructions = yyjson_obj_get(r, "instructions");
  int ok = yyjson_is_obj(r) &&
           yyjson_is_obj(yyjson_obj_get(r, "capabilities")) &&
           yyjson_is_str(yyjson_obj_get(info, "name")) &&
           yyjson_is_str(yyjson_obj_get(info, "version")) &&
           (!instructions || yyjson_is_str(instructions)) &&
           (same(version, "2025-11-25") || same(version, "2025-06-18") ||
            same(version, "2025-03-26") || same(version, "2024-11-05"));
  if (ok && c->set_version)
    ok = c->set_version(c, slice(version)) == PANGU_OK;
  yyjson_doc_free(reply);
  if (!ok)
    return PG_FAILED;
  if (send_message(c, "notifications/initialized", NULL, 0) != PANGU_OK)
    return PG_FAILED;
  pg_status st = discover(c, prefix);
  if (st)
    return st;
  return pg_service_register(scope, service_name, 1, &c->service, c);
}

/* Common incoming request contract: only ping, no advertised client powers. */
yyjson_mut_doc *pg_mcp_client_server_response(yyjson_val *root) {
  yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
  if (!d)
    return NULL;
  yyjson_mut_val *r = yyjson_mut_obj(d),
                 *id = yyjson_val_mut_copy(d, yyjson_obj_get(root, "id")),
                 *result = yyjson_mut_obj(d);
  int ok = r && id && result &&
           yyjson_mut_obj_add_str(d, r, "jsonrpc", "2.0") &&
           yyjson_mut_obj_add_val(d, r, "id", id);
  if (same(yyjson_obj_get(root, "method"), "ping"))
    ok = ok && yyjson_mut_obj_add_val(d, r, "result", result);
  else
    ok = ok && yyjson_mut_obj_add_int(d, result, "code", -32601) &&
         yyjson_mut_obj_add_str(d, result, "message", "Method not found") &&
         yyjson_mut_obj_add_val(d, r, "error", result);
  if (!ok) {
    yyjson_mut_doc_free(d);
    return NULL;
  }
  yyjson_mut_doc_set_root(d, r);
  return d;
}
