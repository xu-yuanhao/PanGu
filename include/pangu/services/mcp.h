#ifndef PANGU_SERVICES_MCP_H
#define PANGU_SERVICES_MCP_H
#include <pangu/services/tools.h>
#ifdef __cplusplus
extern "C" {
#endif
/* One optional stdio server per plugin scope. service_name identifies its
 * pg_tools_service ABI1; tool_prefix is prepended to remote tool names (use a
 * distinct prefix per server). Both are length-delimited, copied, NUL
 * forbidden. argv is a complete NULL-terminated UTF-8 argument vector including
 * executable; cwd is an optional absolute NUL-terminated directory; env=NULL
 * inherits, otherwise env is a complete NULL-terminated KEY=VALUE environment.
 * These process inputs are borrowed only during install; no shell is used.
 * validate and poll contexts must outlive the scope. All IO/callbacks execute
 * synchronously on the installing/Core caller thread using a private uv loop.
 * Calls on one scope must be serialized; poll must not re-enter this scope.
 * Hold a service lease throughout use of the returned array and callbacks.
 * Cancel Core, wait for the call to return, release leases, then dispose.
 * Cancellation/timeout sends notifications/cancelled (never for initialize);
 * it does not claim the remote operation was rolled back. Late replies ignored.
 * Disposal closes stdin, waits briefly, then terminates/kills the process group
 * on POSIX and reaps the child before freeing IO. Windows currently rejected
 * because the process-tree termination adapter is not yet implemented.
 * stderr is drained and discarded, never mixed into protocol/model output.
 * timeout_ms=0 defaults to 30000; close_timeout_ms=0 defaults to 2000;
 * max_message_bytes=0 defaults to 16MiB. Malformed/oversize lines are discarded
 * (bounded memory), allowing a subsequent valid response until request timeout.
 * Initial discovery is immutable; reinstall to refresh tools. Text/image and
 * structured results are copied by Core's sink; full result in details_json.
 * Installation connects, initializes and discovers ALL tool pages before
 * publishing; any failure closes the server and leaves no registration.
 * Explicit trusted local execution only; no discovery/auth/HTTP/sandbox policy.
 */
typedef struct {
  pg_name service_name, tool_prefix;
  const char *const *argv;
  const char *cwd;
  const char *const *env;
  pangu_validate_fn validate;
  void *validator_ctx;
  void (*poll)(void *);
  void *poll_ctx;
  uint64_t timeout_ms, close_timeout_ms;
  size_t max_message_bytes;
} pg_mcp_stdio_config;
extern const pg_plugin pg_mcp_stdio_plugin;
/* Optional Streamable HTTP transport (Pi streamable-http.ts).
 * Inputs are length delimited; URL/header names/values/token forbid NUL, CR,
 * LF. URL must use http(s). TLS verification stays enabled; redirects are
 * disabled. Header names are HTTP tokens. Transport-owned
 * accept/content-type/session/ protocol/last-event-id override fixed headers.
 * token, when nonempty, overrides Authorization with Bearer. Static
 * Basic/API-key authentication can be supplied with fixed headers. No OAuth
 * discovery/browser/refresh implementation implied. All config strings/headers
 * are copied on install. Callback contexts are borrowed until scope disposal.
 * token returns a borrowed slice copied before return to IO (valid until its
 * next invocation); unauthorized may refresh it once on 401 or
 * insufficient_scope 403. No callbacks during close: DELETE reuses the last
 * token, bounded by close_timeout_ms (default 1000). Callbacks/IO run
 * synchronously on install/Core caller thread. Serialize calls, do not reenter,
 * and hold tools lease until callbacks finish. Cancel Core, wait for
 * completion, release leases, dispose scope. Results and discovery use the SAME
 * client mapping as stdio. Failed installation publishes no service. Defaults:
 * timeout 30000ms, max_message_bytes 16MiB (per JSON/SSE event), reconnect
 * 1000ms initial/30000ms maximum/5 retries (zero uses defaults). Response SSE
 * resumes by GET + Last-Event-ID, never replaying tools/call POST.
 * disable_get_stream=0 opens optional server GET after initialized; 405
 * disables it. Native synchronous adaptation pumps this stream during calls
 * only; idle notifications are consumed on the next call, with no background
 * thread. Timeout/cancel aborts local IO and best-effort sends
 * notifications/cancelled except initialize. This does not promise remote
 * rollback. Next call can run. Session 404/auth/HTTP errors fail the request
 * without implicit reinitializing.
 */
typedef struct {
  pg_name name, value;
} pg_mcp_http_header;
typedef pangu_status (*pg_mcp_http_token_fn)(void *, pangu_slice *token);
typedef pangu_status (*pg_mcp_http_unauthorized_fn)(void *,
                                                    unsigned http_status,
                                                    pangu_slice challenge,
                                                    pangu_slice rejected_token);
typedef struct {
  pg_name service_name, tool_prefix, url;
  const pg_mcp_http_header *headers;
  size_t header_count;
  pg_mcp_http_token_fn token;
  pg_mcp_http_unauthorized_fn unauthorized;
  void *auth_ctx;
  pangu_validate_fn validate;
  void *validator_ctx;
  void (*poll)(void *);
  void *poll_ctx;
  uint64_t timeout_ms, close_timeout_ms;
  size_t max_message_bytes;
  int disable_get_stream;
  uint64_t reconnect_initial_ms, reconnect_max_ms;
  unsigned reconnect_max_retries;
} pg_mcp_http_config;
extern const pg_plugin pg_mcp_http_plugin;
#ifdef __cplusplus
}
#endif
#endif
