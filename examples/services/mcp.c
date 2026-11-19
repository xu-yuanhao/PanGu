/* Explicit trusted stdio command, using the same public plugin as the product.
 */
#include <pangu/services/mcp.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc < 4) {
#ifdef PANGU_MCP_HTTP_EXAMPLE
    fprintf(stderr, "usage: mcp-http TOOL_NAME ARGUMENTS_JSON URL\n");
#else
    fprintf(stderr, "usage: mcp TOOL_NAME ARGUMENTS_JSON COMMAND [ARGS...]\n");
#endif
    return 2;
  }
  pg_host *host = NULL;
  pg_scope *scope = NULL;
  pg_lease *lease = NULL;
  pangu_core *core = NULL;
  pangu_tool_outcome outcome = {0};
  int status = 1;
  if (pg_host_create(&host) != PG_OK)
    return 1;
#ifdef PANGU_MCP_HTTP_EXAMPLE
  const char *bearer = getenv("PANGU_MCP_TOKEN");
  char *authorization = NULL;
  pg_mcp_http_header header = {0};
  if (bearer && *bearer) {
    size_t n = strlen(bearer);
    if (n > SIZE_MAX - 8 || !(authorization = malloc(n + 8)))
      goto done;
    memcpy(authorization, "Bearer ", 7);
    memcpy(authorization + 7, bearer, n + 1);
    header =
        (pg_mcp_http_header){PG_NAME("Authorization"), {authorization, n + 7}};
  }
  pg_mcp_http_config cfg = {.service_name = PG_NAME("example.mcp.tools"),
                            .url = {argv[3], strlen(argv[3])},
                            .headers = &header,
                            .header_count = authorization ? 1 : 0,
                            .validate = pg_schema_validate};
  pg_status installed = pg_plugin_install(
      pg_host_root(host), &pg_mcp_http_plugin, &cfg, sizeof(cfg), &scope);
  free(authorization);
  if (installed != PG_OK) {
#else
  pg_mcp_stdio_config cfg = {.service_name = PG_NAME("example.mcp.tools"),
                             .argv = (const char *const *)&argv[3],
                             .validate = pg_schema_validate};
  if (pg_plugin_install(pg_host_root(host), &pg_mcp_stdio_plugin, &cfg,
                        sizeof(cfg), &scope) != PG_OK) {
#endif
    fprintf(stderr, "MCP connection/initial discovery failed\n");
    goto done;
  }
  if (pg_service_acquire(pg_host_root(host), cfg.service_name, 1, &lease) !=
          PG_OK ||
      pangu_core_create(NULL, &core) != PANGU_OK)
    goto done;
  const pg_tools_service *tools = pg_lease_table(lease);
  pangu_loop_config loop = {.tools = tools->tools, .tool_count = tools->count};
  pangu_content call = {.kind = PANGU_TOOL_CALL,
                        .id = {"example", 7},
                        .name = {argv[1], strlen(argv[1])},
                        .data = {argv[2], strlen(argv[2])}};
  pangu_message assistant = {
      .role = PANGU_ASSISTANT, .content = &call, .content_count = 1};
  if (pangu_core_call_tool(core, &assistant, &call, &loop, NULL, NULL,
                           &outcome) != PANGU_OK)
    goto done;
  /* Result is an independent Core copy, safe after closing the server. */
  pg_lease_release(lease);
  lease = NULL;
  if (pg_scope_dispose(scope) != PG_OK)
    goto done;
  const pangu_message *result = pangu_core_message_at(outcome.result, 0);
  status = result->is_error ? 1 : 0;
  for (size_t i = 0; i < result->content_count; ++i) {
    if (result->content[i].kind != PANGU_TEXT)
      continue;
    pangu_slice text = result->content[i].data;
    if (text.len && fwrite(text.data, 1, text.len, stdout) != text.len)
      status = 1;
    putchar('\n');
  }
done:
  if (lease)
    pg_lease_release(lease);
  pangu_core_destroy(outcome.result);
  pangu_core_destroy(core);
  pg_host_destroy(host);
  return status;
}
