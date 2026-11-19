#ifndef PANGU_SERVICES_SCHEMA_H
#define PANGU_SERVICES_SCHEMA_H
#include <pangu/core/loop.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Production JSON Schema draft-7 validator (Valijson). Suitable directly for
 * pg_file_tools_config.validate or pangu_tool.validate; ctx is ignored. Strict
 * types, no coercion, external $ref fetching disabled. JSON/schema errors
 * return TOOL_ERROR, allocation failure NOMEM; no exception crosses C ABI.
 * Input slices borrowed until synchronous return on caller thread; no retained
 * resources. */
pangu_status pg_schema_validate(void *ctx, pangu_slice schema,
                                pangu_slice args);
/* Plain serialized JSON Schema adaptation of Pi d5b3de22 validation.ts.
 * ctx is NULL or a borrowed pg_schema_diagnostics. Input bytes never change.
 * On success calls sink exactly once with complete validated JSON, borrowed
 * until synchronous sink return on the caller thread. On validation failure
 * calls diagnostic (if provided), never sink. Callback errors propagate;
 * allocation failure returns NOMEM. No retained data, cancellation or IO.
 * Schema/value recursion is limited to 256 traversals (TOOL_ERROR).
 * TypeBox symbol/transform metadata is not representable in this C JSON ABI.
 * Diagnostic includes original arguments, so hosts must treat it as user data. */
typedef struct {
  pangu_slice tool_name;
  pangu_arguments_sink diagnostic;
  void *diagnostic_ctx;
} pg_schema_diagnostics;
pangu_status pg_schema_normalize(void *ctx, pangu_slice schema,
                                 pangu_slice args, pangu_arguments_sink sink,
                                 void *sink_ctx);
/* Owner-aware adapter for tool preflight. Ignores ctx, derives name from
 * borrowed call and reports diagnostics transactionally with
 * pangu_core_report_error(owner,...). owner must be the current synchronous
 * run/standalone invocation. No owner/name is retained or inferred. */
pangu_status pg_schema_normalize_tool(void *ctx, pangu_core *owner,
    const pangu_content *call, pangu_slice schema, pangu_slice args,
    pangu_arguments_sink sink, void *sink_ctx);
/* Mature strict validator with detail-only sink (no value output).
 * Errors use dot paths and Valijson messages; callback lifetime as above.
 * Invalid schema returns INVALID (allowing optional-null probes to preserve
 * values when a standalone subschema cannot compile), invalid value TOOL_ERROR. */
pangu_status pg_schema_check(pangu_slice schema, pangu_slice args,
                             pangu_arguments_sink diagnostic, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
