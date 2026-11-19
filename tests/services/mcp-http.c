/* Reuse the actual stdio/Core consumer assertions and model, with a real HTTP
 * peer. Production transports share mcp-client.c rather than duplicate mapping.
 */
#define PANGU_MCP_HTTP_TEST
#include "mcp.c"
typedef struct {
  int tokens, challenges, refreshed, cancel_recovery, cancel_token;
  pangu_core *core;
} auth_context;
static pangu_status token(void *ctx, pangu_slice *out) {
  auth_context *a = ctx;
  a->tokens++;
  if (a->cancel_token)
    pangu_core_cancel(a->core);
  *out = a->refreshed ? SL("fixture-fresh") : SL("fixture-old");
  return PANGU_OK;
}
static pangu_status unauthorized(void *ctx, unsigned status,
                                 pangu_slice challenge, pangu_slice rejected) {
  auth_context *a = ctx;
  a->challenges++;
  CHECK(status == 401 || status == 403);
  CHECK(challenge.len > 0);
  CHECK(equals(rejected, SL("fixture-old")));
  if (a->cancel_recovery)
    pangu_core_cancel(a->core);
  a->refreshed = 1;
  return PANGU_OK;
}
static pg_mcp_http_config config_http(const char *url, polling *p) {
  return (pg_mcp_http_config){.service_name =
                                  PG_NAME("pangu.tools.mcp.fixture"),
                              .tool_prefix = PG_NAME("mcp_fixture_"),
                              .url = {url, strlen(url)},
                              .validate = pg_schema_validate,
                              .poll = poll,
                              .poll_ctx = p,
                              .timeout_ms = 300,
                              .close_timeout_ms = 80,
                              .max_message_bytes = 8192,
                              .reconnect_initial_ms = 2,
                              .reconnect_max_ms = 10,
                              .reconnect_max_retries = 2};
}
static const pg_tools_service *install_http(pg_host *host,
                                            pg_mcp_http_config *cfg,
                                            pg_scope **scope,
                                            pg_lease **lease) {
  CHECK(pg_plugin_install(pg_host_root(host), &pg_mcp_http_plugin, cfg,
                          sizeof(*cfg), scope) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host), cfg->service_name, 1, lease) ==
        PG_OK);
  const pg_tools_service *s = pg_lease_table(*lease);
  CHECK(s && s->count == 8);
  return s;
}
static void expect_error(pangu_core *core, const pg_tools_service *tools,
                         const char *name, const char *text) {
  pangu_tool_outcome o = invoke(core, tools, name, "{}");
  const pangu_message *m = pangu_core_message_at(o.result, 0);
  CHECK(m->is_error);
  if (text)
    CHECK(equals(m->content[0].data, (pangu_slice){text, strlen(text)}));
  pangu_core_destroy(o.result);
}
int main(int argc, char **argv) {
  CHECK(argc == 4);
  char url[512];
  pg_host *host;
  CHECK(pg_host_create(&host) == PG_OK);
  pangu_core *core;
  CHECK(pangu_core_create(NULL, &core) == PANGU_OK);
  polling p = {.core = core};
  pg_scope *scope, *joined, *filescope;
  pg_lease *lease;
  snprintf(url, sizeof(url), "%s/normal", argv[1]);
  pg_mcp_http_config cfg = config_http(url, &p);
  pg_mcp_http_header headers[] = {
      {PG_NAME("X-PanGu-Fixture"), PG_NAME("controlled")},
      {PG_NAME("Authorization"), PG_NAME("Basic Zml4dHVyZTpwYXNz")},
      {PG_NAME("MCP-Protocol-Version"), PG_NAME("must-be-overridden")},
      {PG_NAME("Accept"), PG_NAME("must-be-overridden")}};
  cfg.headers = headers;
  cfg.header_count = 4;
  const pg_tools_service *tools = install_http(host, &cfg, &scope, &lease);
  pg_lease_release(lease);
  pg_file_tools_config files = {.environment =
                                    pg_local_file_environment(argv[3]),
                                .validate = pg_schema_validate};
  CHECK(pg_plugin_install(pg_host_root(host), &pg_file_tools_plugin, &files,
                          sizeof(files), &filescope) == PG_OK);
  pg_dependency sources[] = {{PG_NAME(PG_TOOLS_SERVICE), 1},
                             {cfg.service_name, 1}};
  CHECK(pg_tools_aggregate_install(pg_host_root(host),
                                   PG_NAME(PG_TOOLS_SERVICE), sources, 2,
                                   &joined) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_TOOLS_SERVICE), 1,
                           &lease) == PG_OK);
  tools = pg_lease_table(lease);
  CHECK(tools->count == 11);
  model_context f = {.polling = &p};
  pangu_loop_config loop = {.ctx = &f,
                            .model = model,
                            .tools = tools->tools,
                            .tool_count = tools->count};
  pangu_content b = {.kind = PANGU_TEXT,
                     .data = SL("HTTP model tool roundtrip")};
  pangu_message user = {.role = PANGU_USER, .content = &b, .content_count = 1};
  CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_OK && f.step == 2);
  pangu_tool_outcome o =
      invoke(core, tools, "write",
             "{\"path\":\"http.txt\",\"content\":\"HTTP + local\"}");
  CHECK(!pangu_core_message_at(o.result, 0)->is_error);
  pangu_core_destroy(o.result);
  o = invoke(core, tools, "mcp_fixture_echo", "{\"text\":17}");
  CHECK(pangu_core_message_at(o.result, 0)->is_error);
  pangu_core_destroy(o.result);
  expect_error(core, tools, "mcp_fixture_rpc_error", "fixture RPC error");
  expect_error(core, tools, "mcp_fixture_tool_error", "fixture tool error");
  expect_error(core, tools, "mcp_fixture_bad_result",
               "Invalid MCP tools/call result");
  o = invoke(core, tools, "mcp_fixture_image", "{}");
  const pangu_message *m = pangu_core_message_at(o.result, 0);
  CHECK(!m->is_error && m->content_count == 2 &&
        m->content[0].kind == PANGU_IMAGE);
  CHECK(equals(m->content[0].mime_type, SL("image/png")) &&
        equals(m->content[1].data, SL("resource text")));
  pangu_core_destroy(o.result);
  expect_error(core, tools, "mcp_fixture_wait", "MCP request timed out");
  f = (model_context){.cancel = 1, .polling = &p};
  CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_CANCELLED);
  f = (model_context){.polling = &p};
  CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_OK && f.step == 2);
  o = invoke(core, tools, "mcp_fixture_structured", "{}");
  CHECK(json_equals(pangu_core_message_at(o.result, 0)->structured_json,
                    "[1,null,\"结构化\"]"));
  pg_lease_release(lease);
  CHECK(pg_scope_dispose(scope) == PG_OK);
  CHECK(pg_scope_dispose(joined) == PG_OK);
  CHECK(json_equals(pangu_core_message_at(o.result, 0)->structured_json,
                    "[1,null,\"结构化\"]"));
  pangu_core_destroy(o.result);
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_TOOLS_SERVICE), 1,
                           &lease) == PG_OK);
  CHECK(((const pg_tools_service *)pg_lease_table(lease))->count == 3);
  pg_lease_release(lease);
  CHECK(pg_scope_dispose(filescope) == PG_OK);
  const char *failures[] = {
      "bad-version",      "init-invalid", "init-timeout",    "list-invalid",
      "repeat-cursor",    "duplicate",    "unauthenticated", "accepted-init",
      "bad-content-init", "oversize-init"};
  for (size_t i = 0; i < sizeof(failures) / sizeof(*failures); i++) {
    snprintf(url, sizeof(url), "%s/%s", argv[1], failures[i]);
    cfg.url = (pg_name){url, strlen(url)};
    scope = (pg_scope *)1;
    CHECK(pg_plugin_install(pg_host_root(host), &pg_mcp_http_plugin, &cfg,
                            sizeof(cfg), &scope) == PG_FAILED &&
          !scope);
    CHECK(pg_service_acquire(pg_host_root(host), cfg.service_name, 1, &lease) ==
          PG_NOT_FOUND);
  }
  const char *modes[] = {
      "resume",  "broken",     "accepted",     "bad-content",   "oversize",
      "expired", "auth-error", "server-error", "get-reconnect", "json-batch",
      "no-get",  "ping",       "init-sse",     "disabled-get"};
  for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); i++) {
    snprintf(url, sizeof(url), "%s/%s", argv[1], modes[i]);
    cfg.url = (pg_name){url, strlen(url)};
    cfg.disable_get_stream = !strcmp(modes[i], "disabled-get");
    tools = install_http(host, &cfg, &scope, &lease);
    if (i == 0 || i >= 8) {
      o = invoke(core, tools, "mcp_fixture_echo", "{}");
      m = pangu_core_message_at(o.result, 0);
      CHECK(!m->is_error && equals(m->content[0].data, SL("你好 MCP")));
      pangu_core_destroy(o.result);
    } else {
      const char *errors[] = {
          NULL,
          "MCP response stream failed: stream ended without a response",
          "MCP server accepted request without a response",
          "Unsupported MCP response content type",
          "MCP response exceeds message limit",
          "MCP session expired",
          "MCP server requires authentication",
          "MCP HTTP request failed with status 500"};
      expect_error(core, tools, "mcp_fixture_echo", errors[i]);
    }
    /* Every single-request transport failure leaves the connection reusable. */
    o = invoke(core, tools, "mcp_fixture_structured", "{}");
    CHECK(!pangu_core_message_at(o.result, 0)->is_error);
    pangu_core_destroy(o.result);
    pg_lease_release(lease);
    CHECK(pg_scope_dispose(scope) == PG_OK);
  }
  /* Auth refresh and fixed header precedence; close must not ask for a token.
   */
  const char *authmodes[] = {"auth401",     "auth403",     "auth-always",
                             "auth-cancel", "auth-resume", "get-auth"};
  for (size_t i = 0; i < 6; i++) {
    snprintf(url, sizeof(url), "%s/%s", argv[1], authmodes[i]);
    cfg = config_http(url, &p);
    auth_context a = {.core = core};
    cfg.token = token;
    cfg.unauthorized = unauthorized;
    cfg.auth_ctx = &a;
    if (i < 2) {
      tools = install_http(host, &cfg, &scope, &lease);
      CHECK(a.challenges == 1 && a.refreshed);
      int tokens = a.tokens;
      pg_lease_release(lease);
      CHECK(pg_scope_dispose(scope) == PG_OK);
      CHECK(a.tokens == tokens);
    } else if (i == 2) {
      CHECK(pg_plugin_install(pg_host_root(host), &pg_mcp_http_plugin, &cfg,
                              sizeof(cfg), &scope) == PG_FAILED &&
            !scope);
      CHECK(a.challenges == 1);
    } else if (i >= 4) {
      pangu_core_destroy(core);
      CHECK(pangu_core_create(NULL, &core) == PANGU_OK);
      p.core = core;
      a.core = core;
      cfg.reconnect_max_retries = 3;
      tools = install_http(host, &cfg, &scope, &lease);
      o = invoke(core, tools, "mcp_fixture_echo", "{}");
      CHECK(!pangu_core_message_at(o.result, 0)->is_error);
      pangu_core_destroy(o.result);
      CHECK(a.challenges == 1 && a.refreshed);
      int tokens = a.tokens;
      pg_lease_release(lease);
      CHECK(pg_scope_dispose(scope) == PG_OK);
      CHECK(a.tokens == tokens);
    } else {
      tools = install_http(host, &cfg, &scope, &lease);
      a.refreshed = 0;
      a.cancel_recovery = 1;
      f = (model_context){.polling = &p};
      loop.tools = tools->tools;
      loop.tool_count = tools->count;
      CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_CANCELLED);
      CHECK(a.challenges == 1);
      pg_lease_release(lease);
      CHECK(pg_scope_dispose(scope) == PG_OK);
    }
  }
  /* A token callback cancellation must prevent tools/call dispatch. */
  pangu_core_destroy(core);
  CHECK(pangu_core_create(NULL, &core) == PANGU_OK);
  p.core = core;
  snprintf(url, sizeof(url), "%s/token-cancel", argv[1]);
  cfg = config_http(url, &p);
  auth_context cancelled_token = {.core = core};
  cfg.token = token;
  cfg.auth_ctx = &cancelled_token;
  tools = install_http(host, &cfg, &scope, &lease);
  cancelled_token.cancel_token = 1;
  f = (model_context){.polling = &p};
  loop.tools = tools->tools;
  loop.tool_count = tools->count;
  CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_CANCELLED);
  pg_lease_release(lease);
  CHECK(pg_scope_dispose(scope) == PG_OK);
  cfg = config_http(argv[1], &p);
  cfg.url = PG_NAME("file:///tmp/forbidden");
  CHECK(pg_plugin_install(pg_host_root(host), &pg_mcp_http_plugin, &cfg,
                          sizeof(cfg), &scope) == PG_INVALID &&
        !scope);
  cfg.url = (pg_name){argv[1], strlen(argv[1])};
  pg_mcp_http_header invalid = {PG_NAME("X-Fixture"), PG_NAME("bad\r\nvalue")};
  cfg.headers = &invalid;
  cfg.header_count = 1;
  CHECK(pg_plugin_install(pg_host_root(host), &pg_mcp_http_plugin, &cfg,
                          sizeof(cfg), &scope) == PG_INVALID &&
        !scope);
  /* Lease disposal barrier also applies to HTTP; copied results above survive.
   */
  snprintf(url, sizeof(url), "%s/lease", argv[1]);
  cfg = config_http(url, &p);
  cfg.disable_get_stream = 1;
  tools = install_http(host, &cfg, &scope, &lease);
  (void)tools;
  uv_thread_t th;
  CHECK(uv_thread_create(&th, delayed_release, lease) == 0);
  uint64_t at = uv_hrtime() / 1000000;
  CHECK(pg_scope_dispose(scope) == PG_OK && uv_hrtime() / 1000000 - at >= 40);
  CHECK(uv_thread_join(&th) == 0);
  pangu_core_destroy(core);
  pg_host_destroy(host);
  puts("MCP HTTP Core consumer passed");
  return 0;
}
