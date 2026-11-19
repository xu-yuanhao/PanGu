#ifndef PANGU_MCP_CLIENT_H
#define PANGU_MCP_CLIENT_H
#include <pangu/services/mcp.h>
#include <yyjson.h>
typedef struct client client;
typedef struct {
  client *client;
  char *remote;
} tool_context;
struct client {
  pg_mcp_stdio_config cfg; /* shared validation/poll/deadline settings */
  pg_tools_service service;
  pangu_tool *tools;
  tool_context *contexts;
  const char *last_error;
  pangu_status (*request)(client *, const char *, yyjson_val *, pangu_core *,
                          yyjson_doc **);
  pangu_status (*send)(client *, const char *, yyjson_val *, uint64_t);
  pangu_status (*set_version)(client *, pangu_slice);
};
pg_status pg_mcp_client_publish(client *, pg_scope *, pg_name, pg_name);
void pg_mcp_client_clear(client *);
yyjson_mut_doc *pg_mcp_client_server_response(yyjson_val *);
#endif
