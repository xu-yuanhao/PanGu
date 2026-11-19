#ifndef PANGU_SERVICES_TOOLS_H
#define PANGU_SERVICES_TOOLS_H
#include <pangu/core/loop.h>
#include <pangu/services/host.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_TOOLS_SERVICE "pangu.tools"
typedef struct {
  const pangu_tool *tools;
  size_t count;
} pg_tools_service;
/* Publish one ordered tools view from ABI1 pg_tools_service dependencies.
 * sources/output names are borrowed through return and copied by the host.
 * Install AFTER all source plugins; sources may include the previous binding
 * of output (e.g. pangu.tools plus separate MCP services). Empty input is
 * valid. Declarations are copied, callback data/slices remain borrowed under
 * injected dependency leases. Duplicate tool names return PG_INVALID; any
 * failure leaves out=NULL and restores the previous output binding. ABI
 * mismatch returns PG_ABI. Keep a lease on the aggregate for every Core
 * run/call, then release before disposal. Disposing a source automatically
 * withdraws/disposes its aggregates before source cleanup, waiting for
 * aggregate leases. Explicit teardown order: stop consumers, release aggregate
 * leases, dispose aggregate, dispose sources. Same serialized
 * administration/thread/cancellation rules as host.h. Available in
 * PanGu::services; no extra target or dependency is required. */
pg_status pg_tools_aggregate_install(pg_scope *parent, pg_name output,
                                     const pg_dependency *sources,
                                     size_t source_count, pg_scope **out);
/* Injectable filesystem seam: callbacks are synchronous on run thread. Input
 * paths are UTF-8 byte slices (NUL forbidden), relative paths resolved by env.
 * read returns malloc-owned bytes freed by the tools plugin after sink returns.
 * write creates parents and overwrites, retaining mutation ordering until it
 * settles. Cancellation queried before/after IO; injected env must quiesce IO
 * before returning. Context and validator outlive plugin scope. A mature Schema
 * validator is supplied by caller; no permissive default is installed. */
typedef struct {
  void *ctx;
  pangu_status (*read)(void *, pangu_core *, pangu_slice path, char **,
                       size_t *);
  pangu_status (*write)(void *, pangu_core *, pangu_slice path,
                        pangu_slice data);
} pg_file_environment;
typedef struct {
  pg_file_environment environment;
  void *validator_ctx;
  pangu_validate_fn validate;
  /* Optional normalizing validator, preferred over validate. pg_schema_validate
   * selects pg_schema_normalize automatically for default file tools. Context
   * remains borrowed through scope disposal. Same synchronous sink contract
   * as pangu_tool.validate_arguments; no IO before successful validation. */
  pangu_validate_arguments_fn validate_arguments;
} pg_file_tools_config;
/* Installs read/write/edit in pangu.tools. edit matches nonempty unique,
 * disjoint regions against the original LF-normalized file, preserves BOM and
 * the original CRLF convention, and rejects no-op writes. Result details_json
 * carries diff/patch/firstChangedLine; sink borrows them only during callback.
 * read accepts integer offset (1-based) and limit (>=0), returning complete
 * lines bounded by 2000 lines/51200 bytes plus a continuation diagnostic.
 * Mutations hold a process-wide recursive lock through read/write settlement;
 * unrelated files are currently serialized too. No callbacks may re-enter
 * tool execution from another thread while waiting for their own completion.
 */
extern const pg_plugin pg_file_tools_plugin;
/* Local libuv filesystem implementation. cwd is borrowed, absolute UTF-8 with
 * trailing NUL, owned by caller; it must outlive env users. Thread-safe across
 * contexts; write operations are serialized process-wide, including mkdir.
 * Mobile hosts can supply their own allowed operations instead. */
pg_file_environment pg_local_file_environment(const char *cwd);
#ifdef __cplusplus
}
#endif
#endif
