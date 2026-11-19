#ifndef PG_CLI_MCP_CONFIG_H
#define PG_CLI_MCP_CONFIG_H
#include <pangu/services/mcp.h>
typedef struct pg_cli_mcp_config pg_cli_mcp_config;
/* Parse/validate complete explicit file before any process/network is started.
 * path/cwd are NUL-terminated, borrowed during read; out is cleared on error.
 * Returned configs and header strings belong to the result until free; plugin
 * installation copies them. at() exposes shared names for either transport;
 * http_at() returns NULL for stdio/out-of-range. No callbacks/threads here. */
pg_status pg_cli_mcp_read(const char *path, const char *cwd,
                          pg_cli_mcp_config **out);
size_t pg_cli_mcp_count(const pg_cli_mcp_config *);
const pg_mcp_http_config *pg_cli_mcp_http_at(const pg_cli_mcp_config *, size_t);
const pg_mcp_stdio_config *pg_cli_mcp_at(const pg_cli_mcp_config *, size_t);
void pg_cli_mcp_free(pg_cli_mcp_config *);
#endif
