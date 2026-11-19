#include "slices.h"
#include <assert.h>
#include <pangu/agent/app.h>
#include <pangu/services/mcp.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <string.h>
#define S(s) ((pangu_slice){s, sizeof(s) - 1})
typedef struct {
  int calls;
} state;
static pangu_status model(void *ctx, pangu_core *core, pangu_model_sink sink,
                          void *out) {
  state *s = ctx;
  int first = s->calls++ == 0;
  if (!first) {
    size_t n = pangu_core_message_count(core);
    assert(n >= 2);
    const pangu_message *a = pangu_core_message_at(core, n - 2),
                        *b = pangu_core_message_at(core, n - 1);
    assert(a->role == PANGU_TOOL_RESULT && !a->is_error &&
           slice_contains(a->content[0].data, "SDK local content"));
    assert(b->role == PANGU_TOOL_RESULT && !b->is_error &&
           slice_contains(b->content[0].data, "SDK remote content") &&
           b->details_json.len);
  }
  pangu_content calls[] = {{.kind = PANGU_TOOL_CALL,
                            .id = S("local"),
                            .name = S("read"),
                            .data = S("{\"path\":\"sdk-input.txt\"}")},
                           {.kind = PANGU_TOOL_CALL,
                            .id = S("remote"),
                            .name = S("mcp__sdk__echo"),
                            .data = S("{\"text\":\"SDK remote content\"}")}};
  pangu_content text = {.kind = PANGU_TEXT, .data = S("SDK MCP completed")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = first ? calls : &text,
                     .content_count = first ? 2 : 1,
                     .stop_reason = first ? PANGU_TOOL_USE : PANGU_STOP};
  return sink(out, &m, 1);
}
int main(int argc, char **argv) {
  assert(argc == 4);
  const char *args[] = {argv[1], argv[2], "normal", argv[3], NULL};
  pg_mcp_stdio_config mcp = {.service_name = PG_NAME("pangu.test.mcp"),
                             .tool_prefix = PG_NAME("mcp__sdk__"),
                             .argv = args,
                             .cwd = argv[3],
                             .validate = pg_schema_validate,
                             .timeout_ms = 1000};
  pg_file_tools_config files = {.environment =
                                    pg_local_file_environment(argv[3]),
                                .validate = pg_schema_validate};
  pg_app_plugin plugins[] = {{&pg_file_tools_plugin, &files, sizeof(files)},
                             {&pg_mcp_stdio_plugin, &mcp, sizeof(mcp)}};
  char input[4096], path[4096];
  assert(snprintf(input, sizeof(input), "%s/sdk-input.txt", argv[3]) > 0);
  FILE *f = fopen(input, "wb");
  assert(f);
  assert(fputs("SDK local content", f) >= 0 && !fclose(f));
  for (int b = PG_STORE_JSONL; b <= PG_STORE_SQLITE; b++) {
    assert(snprintf(path, sizeof(path), "%s/sdk-%d.session", argv[3], b) > 0);
    remove(path);
    pg_session_options options = {{path, strlen(path)}, b, 1, 0};
    pg_agent_app *app = NULL;
    assert(pg_agent_app_open(&options, plugins, 2, &app) == PG_OK);
    pg_dependency sources[] = {{PG_NAME(PG_TOOLS_SERVICE), 1},
                               {PG_NAME("pangu.test.mcp"), 1}};
    pg_scope *scope = NULL;
    assert(pg_tools_aggregate_install(pg_agent_app_scope(app),
                                      PG_NAME(PG_TOOLS_SERVICE), sources, 2,
                                      &scope) == PG_OK);
    state s = {0};
    pangu_content text = {.kind = PANGU_TEXT, .data = S("SDK input")};
    pangu_message input_message = {
        .role = PANGU_USER, .content = &text, .content_count = 1};
    pangu_loop_config loop = {.ctx = &s, .model = model};
    assert(pg_agent_app_run(app, &input_message, 1, &loop) == PG_SESSION_OK &&
           s.calls == 2);
    pg_agent_app_close(app);
    options.create = 0;
    assert(pg_agent_app_open(&options, NULL, 0, &app) == PG_OK);
    const pangu_core *history = pg_session_messages(pg_agent_app_session(app));
    assert(slice_contains(
        pangu_core_message_at(history, pangu_core_message_count(history) - 1)
            ->content[0]
            .data,
        "SDK MCP completed"));
    pg_agent_app_close(app);
    remove(path);
  }
  puts("SDK local+MCP app lifecycle and both-backend reopen passed");
}
