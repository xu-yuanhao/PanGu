/* cc consumer: link PanGu::services_resources. No private headers. */
#include <pangu/services/resources.h>
#include <stdio.h>
int main(int argc, char **argv) {
  if (argc != 3)
    return 2; /* explicit user directory and project cwd */
  pg_host *host = NULL;
  pg_scope *scope = NULL;
  pg_lease *lease = NULL;
  if (pg_host_create(&host) != PG_OK)
    return 1;
  pg_resources_config config = {
      .user_directory = argv[1], .cwd = argv[2], .project_trusted = 0};
  pg_status status = pg_plugin_install(pg_host_root(host), &pg_resources_plugin,
                                       &config, sizeof(config), &scope);
  if (!status)
    status = pg_service_acquire(pg_host_root(host),
                                PG_NAME(PG_RESOURCES_SERVICE), 1, &lease);
  pg_resource_snapshot *snapshot = NULL;
  if (!status) {
    const pg_resources_service *service = pg_lease_table(lease);
    status = service->snapshot(pg_lease_context(lease), &snapshot);
    /* Use pg_resource_settings(snapshot) to select model/options; use
     * pg_resource_system_prompt(snapshot) as the Core system message.
     * Keep snapshot through run return. Release lease before scope disposal. */
    if (!status)
      printf("resources=%zu\n", pg_resource_count(snapshot));
  }
  if (lease)
    pg_lease_release(lease);
  pg_host_destroy(host);
  pg_resource_snapshot_release(
      snapshot); /* intentionally after plugin unload */
  return status ? 1 : 0;
}
