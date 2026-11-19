#ifndef PANGU_SERVICES_RESOURCES_H
#define PANGU_SERVICES_RESOURCES_H
#include <pangu/core/core.h>
#include <pangu/services/host.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_RESOURCES_SERVICE "pangu.resources"
#define PG_CREDENTIALS_SERVICE "pangu.credentials"
typedef struct pg_resource_snapshot pg_resource_snapshot;
typedef enum {
  PG_RESOURCE_CONTEXT,
  PG_RESOURCE_SKILL,
  PG_RESOURCE_PROMPT
} pg_resource_kind;
typedef struct {
  pg_resource_kind kind;
  pangu_slice name, description, path, body;
  int project, disable_model_invocation;
} pg_resource;
typedef struct {
  const char *user_directory; /* e.g. $HOME/.pangu/agent; absolute UTF-8 */
  const char *cwd;            /* absolute UTF-8 */
  pangu_slice overrides_json; /* object; last precedence */
  int project_trusted;        /* 0/1 explicit host decision; -1 loads trust.json
                               * nearest ancestor (no remembered decision denies). */
} pg_resources_config;
/* Administration, reload and updates are serialized by the caller, outside a
 * running Core request. Returned snapshots own immutable storage and survive
 * reload and plugin unload; caller releases each snapshot. All input slices
 * are length-delimited UTF-8, NUL forbidden in names/paths. Plugin copies
 * config. Failed reload preserves prior snapshot. No command substitution or
 * resource execution is performed. read-only snapshot access may occur on any
 * thread. settings merges nested objects; arrays replace. patch reads latest
 * disk state, merges object keys, writes same-directory temp+fsync+rename, then
 * reloads. A post-commit reload error reports failure but disk patch remains
 * committed. No implicit trust UI: runtime owns approval and passes explicit
 * decision.
 */
typedef struct {
  pg_status (*snapshot)(void *, pg_resource_snapshot **);
  pg_status (*reload)(void *);
  pg_status (*patch_settings)(void *, int project, pangu_slice object_json);
  pg_status (*set_project_trusted)(void *, int trusted);
  pg_status (*remember_project_trust)(void *, int trusted);
} pg_resources_service;
/* Credentials are a separate service on the same scope. Secrets never appear
 * in snapshots/diagnostics. get_api_key re-reads auth.json each request, then
 * PROVIDER_API_KEY; caller owns malloc output (free after request). api_key and
 * environment literals are supported; OAuth refresh belongs to model providers.
 * Key strings beginning ! are rejected (no implicit shell command execution).
 * put_api_key(provider, empty slice) removes stored credentials. */
typedef struct {
  pg_status (*get_api_key)(void *, pangu_slice provider, char **, size_t *);
  pg_status (*put_api_key)(void *, pangu_slice provider, pangu_slice key);
} pg_credentials_service;
extern const pg_plugin pg_resources_plugin;
void pg_resource_snapshot_release(pg_resource_snapshot *);
pangu_slice pg_resource_settings(const pg_resource_snapshot *);
pangu_slice pg_resource_system_prompt(const pg_resource_snapshot *);
/* JSON array of diagnostic objects {path,message}; excludes secret data. */
pangu_slice pg_resource_diagnostics(const pg_resource_snapshot *);
size_t pg_resource_count(const pg_resource_snapshot *);
const pg_resource *pg_resource_at(const pg_resource_snapshot *, size_t);
/* Expand /template with quoted args, positional/default/all/slice placeholders.
 * Unknown command is copied verbatim. malloc output, release with free(). */
pg_status pg_resource_expand(const pg_resource_snapshot *, pangu_slice input,
                             char **output, size_t *length);
#ifdef __cplusplus
}
#endif
#endif
