#include <pangu/services/mcp.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <yyjson.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %d: %s\n", __LINE__, #x);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static int equals(pangu_slice a, pangu_slice b) {
  return a.len == b.len && (!a.len || !memcmp(a.data, b.data, a.len));
}
static int json_equals(pangu_slice a, const char *expected) {
  yyjson_doc *x = yyjson_read(a.data, a.len, 0),
             *y = yyjson_read(expected, strlen(expected), 0);
  int ok =
      x && y && yyjson_equals(yyjson_doc_get_root(x), yyjson_doc_get_root(y));
  yyjson_doc_free(x);
  yyjson_doc_free(y);
  return ok;
}
typedef struct {
  pangu_core *core;
  uint64_t deadline;
  int armed;
} polling;
static void poll(void *ctx) {
  polling *p = ctx;
  if (p->armed && uv_hrtime() / 1000000 >= p->deadline) {
    pangu_core_cancel(p->core);
    p->armed = 0;
  }
}
#ifndef PANGU_MCP_HTTP_TEST
static pg_mcp_stdio_config configuration(const char **argv, polling *p) {
  return (pg_mcp_stdio_config){.service_name =
                                   PG_NAME("pangu.tools.mcp.fixture"),
                               .tool_prefix = PG_NAME("mcp_fixture_"),
                               .argv = argv,
                               .validate = pg_schema_validate,
                               .poll = poll,
                               .poll_ctx = p,
                               .timeout_ms = 300,
                               .close_timeout_ms = 40,
                               .max_message_bytes = 8192};
}
#endif
static void delayed_release(void *ctx) {
  uv_sleep(50);
  pg_lease_release(ctx);
}
#ifndef PANGU_MCP_HTTP_TEST
static const pg_tools_service *install(pg_host *h, pg_mcp_stdio_config *cfg,
                                       pg_scope **scope, pg_lease **lease) {
  CHECK(pg_plugin_install(pg_host_root(h), &pg_mcp_stdio_plugin, cfg,
                          sizeof(*cfg), scope) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(h), cfg->service_name, 1, lease) ==
        PG_OK);
  const pg_tools_service *tools = pg_lease_table(*lease);
  CHECK(tools && tools->count == 8);
  return tools;
}
#endif
typedef struct {
  int step, cancel;
  polling *polling;
} model_context;
static pangu_status model(void *ctx, pangu_core *core, pangu_model_sink sink,
                          void *out) {
  if (pangu_core_is_cancelled(core))
    return PANGU_CANCELLED;
  model_context *f = ctx;
  pangu_content b = {0};
  pangu_message m = {
      .role = PANGU_ASSISTANT, .content = &b, .content_count = 1};
  if (f->step++ == 0) {
    b = (pangu_content){.kind = PANGU_TOOL_CALL,
                        .id = SL("mcp-call"),
                        .name = f->cancel ? SL("mcp_fixture_wait")
                                          : SL("mcp_fixture_echo"),
                        .data = SL("{\"text\":\"Core真实往返\"}")};
    m.stop_reason = PANGU_TOOL_USE;
    if (f->cancel) {
      f->polling->armed = 1;
      f->polling->deadline = uv_hrtime() / 1000000 + 30;
    }
  } else {
    const pangu_message *last =
        pangu_core_message_at(core, pangu_core_message_count(core) - 1);
    CHECK(last->role == PANGU_TOOL_RESULT && !last->is_error);
    CHECK(equals(last->content[0].data, SL("Core真实往返")));
    /* Core intentionally omits rich structured data from committed transcript.
     */
    CHECK(last->structured_json.len == 0 && last->details_json.len > 0);
    b = (pangu_content){.kind = PANGU_TEXT, .data = SL("MCP model complete")};
    m.stop_reason = PANGU_STOP;
  }
  return sink(out, &m, 1);
}
static pangu_tool_outcome invoke(pangu_core *core,
                                 const pg_tools_service *tools,
                                 const char *name, const char *args) {
  pangu_content call = {.kind = PANGU_TOOL_CALL,
                        .id = SL("standalone"),
                        .name = {name, strlen(name)},
                        .data = {args, strlen(args)}};
  pangu_message assistant = {
      .role = PANGU_ASSISTANT, .content = &call, .content_count = 1};
  pangu_loop_config config = {.tools = tools->tools,
                              .tool_count = tools->count};
  pangu_tool_outcome result = {0};
  CHECK(pangu_core_call_tool(core, &assistant, &call, &config, NULL, NULL,
                             &result) == PANGU_OK);
  return result;
}
#ifndef PANGU_MCP_HTTP_TEST
int main(int argc, char **argv) {
  CHECK(argc == 4);
  const char *process[] = {argv[1], argv[2], "normal", argv[3], NULL};
  pg_host *host;
  CHECK(pg_host_create(&host) == PG_OK);
  pangu_core *core;
  CHECK(pangu_core_create(NULL, &core) == PANGU_OK);
  polling p = {.core = core};
  pg_mcp_stdio_config cfg = configuration(process, &p);
  const char *environment[] = {"PANGU_MCP_FIXTURE=controlled", NULL};
  cfg.cwd = argv[3];
  cfg.env = environment;
  pg_scope *scope;
  pg_lease *lease;
  const pg_tools_service *tools = install(host, &cfg, &scope, &lease);
  pg_lease_release(lease);
  pg_file_tools_config files = {.environment =
                                    pg_local_file_environment(argv[3]),
                                .validate = pg_schema_validate};
  pg_scope *file_scope, *joined;
  CHECK(pg_plugin_install(pg_host_root(host), &pg_file_tools_plugin, &files,
                          sizeof(files), &file_scope) == PG_OK);
  pg_dependency sources[] = {{PG_NAME(PG_TOOLS_SERVICE), 1},
                             {cfg.service_name, 1}};
  CHECK(pg_tools_aggregate_install(pg_host_root(host),
                                   PG_NAME(PG_TOOLS_SERVICE), sources, 2,
                                   &joined) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_TOOLS_SERVICE), 1,
                           &lease) == PG_OK);
  tools = pg_lease_table(lease);
  CHECK(tools->count == 11);
  pg_scope *failed = (pg_scope *)1;
  pg_dependency duplicate[] = {{PG_NAME(PG_TOOLS_SERVICE), 1},
                               {cfg.service_name, 1}};
  CHECK(pg_tools_aggregate_install(pg_host_root(host),
                                   PG_NAME(PG_TOOLS_SERVICE), duplicate, 2,
                                   &failed) == PG_INVALID &&
        !failed);
  pg_dependency missing[] = {{PG_NAME("absent.mcp.tools"), 1}};
  CHECK(pg_tools_aggregate_install(pg_host_root(host),
                                   PG_NAME(PG_TOOLS_SERVICE), missing, 1,
                                   &failed) == PG_NOT_FOUND &&
        !failed);
  pg_lease *check;
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_TOOLS_SERVICE), 1,
                           &check) == PG_OK);
  CHECK(pg_lease_table(check) == tools);
  pg_lease_release(check);
  pangu_tool_outcome local =
      invoke(core, tools, "write",
             "{\"path\":\"aggregate.txt\",\"content\":\"local + MCP\"}");
  CHECK(!pangu_core_message_at(local.result, 0)->is_error);
  pangu_core_destroy(local.result);
  local = invoke(core, tools, "read", "{\"path\":\"aggregate.txt\"}");
  CHECK(!pangu_core_message_at(local.result, 0)->is_error &&
        equals(pangu_core_message_at(local.result, 0)->content[0].data,
               SL("local + MCP")));
  pangu_core_destroy(local.result);
  model_context f = {.polling = &p};
  pangu_loop_config loop = {.ctx = &f,
                            .model = model,
                            .tools = tools->tools,
                            .tool_count = tools->count};
  pangu_content b = {.kind = PANGU_TEXT, .data = SL("use MCP")};
  pangu_message user = {.role = PANGU_USER, .content = &b, .content_count = 1};
  CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_OK && f.step == 2);
  pangu_tool_outcome result =
      invoke(core, tools, "mcp_fixture_echo", "{\"text\":17}");
  CHECK(pangu_core_message_at(result.result, 0)->is_error);
  pangu_core_destroy(result.result);
  result = invoke(core, tools, "mcp_fixture_rpc_error", "{}");
  CHECK(pangu_core_message_at(result.result, 0)->is_error &&
        equals(pangu_core_message_at(result.result, 0)->content[0].data,
               SL("fixture RPC error")));
  CHECK(pangu_core_message_at(result.result, 0)->details_json.len > 0);
  pangu_core_destroy(result.result);
  result = invoke(core, tools, "mcp_fixture_tool_error", "{}");
  CHECK(pangu_core_message_at(result.result, 0)->is_error &&
        equals(pangu_core_message_at(result.result, 0)->content[0].data,
               SL("fixture tool error")));
  pangu_core_destroy(result.result);
  result = invoke(core, tools, "mcp_fixture_bad_result", "{}");
  CHECK(pangu_core_message_at(result.result, 0)->is_error);
  pangu_core_destroy(result.result);
  result = invoke(core, tools, "mcp_fixture_image", "{}");
  CHECK(!pangu_core_message_at(result.result, 0)->is_error &&
        pangu_core_message_at(result.result, 0)->content_count == 2);
  CHECK(pangu_core_message_at(result.result, 0)->content[0].kind ==
            PANGU_IMAGE &&
        equals(pangu_core_message_at(result.result, 0)->content[0].mime_type,
               SL("image/png")));
  CHECK(equals(pangu_core_message_at(result.result, 0)->content[1].data,
               SL("resource text")));
  pangu_core_destroy(result.result);
  result = invoke(core, tools, "mcp_fixture_wait", "{}");
  CHECK(pangu_core_message_at(result.result, 0)->is_error);
  CHECK(equals(pangu_core_message_at(result.result, 0)->content[0].data,
               SL("MCP request timed out")));
  pangu_core_destroy(result.result);
  result = invoke(core, tools, "mcp_fixture_echo", "{}");
  CHECK(!pangu_core_message_at(result.result, 0)->is_error);
  pangu_core_destroy(result.result);
  f = (model_context){.cancel = 1, .polling = &p};
  CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_CANCELLED);
  CHECK(pangu_core_is_cancelled(core));
  f = (model_context){.polling = &p};
  CHECK(pangu_core_run(core, &user, 1, &loop) == PANGU_OK && f.step == 2);
  /* Independent copied result survives lease release and process/scope cleanup.
   */
  result = invoke(core, tools, "mcp_fixture_structured", "{}");
  CHECK(!pangu_core_message_at(result.result, 0)->is_error &&
        pangu_core_message_at(result.result, 0)->structured_json.len > 0);
  pg_lease_release(lease);
  CHECK(pg_scope_dispose(scope) == PG_OK);
  CHECK(pg_scope_dispose(joined) ==
        PG_OK); /* source disposal already withdrew it */
  CHECK(pg_service_acquire(pg_host_root(host), PG_NAME(PG_TOOLS_SERVICE), 1,
                           &check) == PG_OK);
  CHECK(((const pg_tools_service *)pg_lease_table(check))->count == 3);
  pg_lease_release(check);
  CHECK(pg_scope_dispose(file_scope) == PG_OK);
  CHECK(json_equals(pangu_core_message_at(result.result, 0)->structured_json,
                    "[1,null,\"结构化\"]"));
  pangu_core_destroy(result.result);
  CHECK(pg_service_acquire(pg_host_root(host), cfg.service_name, 1, &lease) ==
        PG_NOT_FOUND);
  const char *failures[] = {"bad-version",  "init-invalid",  "init-timeout",
                            "list-invalid", "repeat-cursor", "duplicate"};
  for (size_t i = 0; i < sizeof(failures) / sizeof(*failures); ++i) {
    process[2] = failures[i];
    scope = (pg_scope *)1;
    CHECK(pg_plugin_install(pg_host_root(host), &pg_mcp_stdio_plugin, &cfg,
                            sizeof(cfg), &scope) == PG_FAILED);
    CHECK(!scope && pg_service_acquire(pg_host_root(host), cfg.service_name, 1,
                                       &lease) == PG_NOT_FOUND);
  }
  process[2] = "drop";
  tools = install(host, &cfg, &scope, &lease);
  result = invoke(core, tools, "mcp_fixture_drop", "{}");
  CHECK(pangu_core_message_at(result.result, 0)->is_error);
  CHECK(equals(pangu_core_message_at(result.result, 0)->content[0].data,
               SL("MCP connection closed")));
  pangu_core_destroy(result.result);
  pg_lease_release(lease);
  CHECK(pg_scope_dispose(scope) == PG_OK);
  process[2] = "stubborn";
  tools = install(host, &cfg, &scope, &lease);
  (void)tools;
  uv_thread_t releasing;
  CHECK(uv_thread_create(&releasing, delayed_release, lease) == 0);
  uint64_t start = uv_hrtime() / 1000000;
  CHECK(pg_scope_dispose(scope) == PG_OK &&
        uv_hrtime() / 1000000 - start >= 40 &&
        uv_hrtime() / 1000000 - start < 2000);
  CHECK(uv_thread_join(&releasing) == 0);
  const char *absent[] = {"/pangu-fixture-no-such-executable", NULL};
  cfg.argv = absent;
  CHECK(pg_plugin_install(pg_host_root(host), &pg_mcp_stdio_plugin, &cfg,
                          sizeof(cfg), &scope) == PG_FAILED &&
        !scope);
  pangu_core_destroy(core);
  pg_host_destroy(host);
  puts("mcp: initialize/pagination/Core loop/Schema/errors/cancel/reuse/close "
       "passed");
  return 0;
}

#endif
