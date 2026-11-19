#ifndef PANGU_SERVICES_EXECUTION_H
#define PANGU_SERVICES_EXECUTION_H
#include <pangu/services/tools.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_EXECUTION_TOOLS_SERVICE "pangu.tools.execution"
/* All callbacks run synchronously on the Core run thread. argv/env are borrowed
 * NUL-terminated UTF-8 arrays through return; env=NULL inherits, otherwise it is
 * the complete environment. emit borrows bytes only during the callback and
 * must be called in observed stdout/stderr order. Return only after callbacks
 * and child IO have quiesced. Cancellation must stop the process tree. Hosts
 * may inject a restricted backend on mobile; a NULL backend is rejected.
 * poll is for same-thread UI/cancellation dispatch; contexts outlive scope.
 * Timeout is milliseconds, zero means unlimited. exit_code uses 128+signal.
 */
typedef pangu_status (*pg_process_output)(void *, pangu_slice);
typedef struct {
  const char *const *argv;
  const char *cwd;
  const char *const *env;
  uint64_t timeout_ms;
  void (*poll)(void *);
  void *poll_ctx;
} pg_process_request;
typedef struct {
  void *ctx;
  pangu_status (*run)(void *, pangu_core *, const pg_process_request *,
                      pg_process_output, void *, int *exit_code);
} pg_execution_environment;
typedef struct {
  pg_execution_environment environment;
  const char *cwd;
  const char *const *env;
  const char *shell; /* NULL = bash; injected Windows backend may select pwsh. */
  const char *temp_directory; /* NULL = libuv system temp dir. Spill files persist
                              * for read; host owns eventual deletion. */
  pangu_validate_fn validate;
  void *validator_ctx;
  void (*poll)(void *);
  void *poll_ctx;
  /* Preferred normalizing validator; pg_schema_validate selects the default
   * pg_schema_normalize path. Borrowed ctx and synchronous sink as tools.h. */
  pangu_validate_arguments_fn validate_arguments;
} pg_execution_tools_config;
/* Registers bash/grep/find/ls as a single optional tools capability, alongside
 * the file tools service (not replacing it). Consumer joins the two arrays.
 * rg/fd are executed as argv without shell interpolation. Their absence is a
 * tool error. Output callbacks and result slices never escape run return. */
extern const pg_plugin pg_execution_tools_plugin;
/* libuv native adapter: POSIX detached process group kill; Windows explicitly
 * unavailable until a Job Object/PowerShell adapter is supplied. */
pg_execution_environment pg_local_execution_environment(void);
#ifdef __cplusplus
}
#endif
#endif
