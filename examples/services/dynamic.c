/* Explicit local loading example: no path discovery or network installation. */
#include "native-plugin/fixture.h"
#include <pangu/services/dynamic.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <string.h>
static pangu_status show_result(void *ctx, const pangu_message *message,
                                int final) {
  (void)ctx;
  if (!final || message->is_error)
    return PANGU_ERROR;
  for (size_t i = 0; i < message->content_count; ++i) {
    pangu_slice text = message->content[i].data;
    if (text.len && fwrite(text.data, 1, text.len, stdout) != text.len)
      return PANGU_ERROR;
  }
  putchar('\n');
  return PANGU_OK;
}
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: dynamic ABSOLUTE_CWD LOCAL_PLUGIN_PATH\n");
    return 2;
  }
  pg_host *host = NULL;
  pg_scope *scope = NULL;
  pg_dynamic_diagnostic diagnostic;
  if (pg_host_create(&host) != PG_OK)
    return 1;
  pg_native_fixture_config config = {.validate = pg_schema_validate};
  pg_status st = pg_dynamic_plugin_load(
      pg_host_root(host), (pg_name){argv[1], strlen(argv[1])},
      (pg_name){argv[2], strlen(argv[2])}, &config, sizeof(config), &scope, &diagnostic);
  if (st)
    fprintf(stderr, "plugin load failed at stage %d: %s\n",
            (int)diagnostic.stage, diagnostic.message);
  else {
    pg_lease *lease = NULL;
    st = pg_service_acquire(pg_host_root(host),
                            PG_NAME(PG_NATIVE_FIXTURE_SERVICE), 1, &lease);
    if (st == PG_OK) {
      const pg_tools_service *tools = pg_lease_table(lease);
      pangu_content call = {.kind = PANGU_TOOL_CALL,
                            .name = {"native_echo", 11},
                            .data = {"{}", 2}};
      if (tools->count != 1 ||
          tools->tools[0].validate(tools->tools[0].ctx,
                                   tools->tools[0].schema_json,
                                   call.data) != PANGU_OK ||
          tools->tools[0].execute(tools->tools[0].ctx, NULL, &call,
                                  show_result, NULL) != PANGU_OK)
        st = PG_FAILED;
      pg_lease_release(lease);
    }
    pg_status disposed = pg_scope_dispose(scope);
    if (st == PG_OK)
      st = disposed;
  }
  pg_host_destroy(host);
  return st ? 1 : 0;
}
