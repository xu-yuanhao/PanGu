#ifndef PANGU_SCHEMA_H
#define PANGU_SCHEMA_H
#include "src/bootstrap-api/json.h"
#ifndef __ASSEMBLER__
typedef struct pangu_schema_view {
 pangu_text_slice input;const pangu_json_result *result;pangu_u64 root;
} pangu_schema_view;
typedef struct pangu_schema_limits {
 pangu_u64 max_work;pangu_u32 max_depth,max_nodes;
} pangu_schema_limits;
typedef struct pangu_schema_report {
 pangu_i32 status;pangu_u32 reason;pangu_u64 schema_token,value_token,work;
} pangu_schema_report;
enum pangu_schema_reason { PANGU_SCHEMA_OK=0,PANGU_SCHEMA_LIMIT,
 PANGU_SCHEMA_UNSUPPORTED,PANGU_SCHEMA_DEFINITION,PANGU_SCHEMA_TYPE,
 PANGU_SCHEMA_REQUIRED,PANGU_SCHEMA_ENUM,PANGU_SCHEMA_ADDITIONAL,PANGU_SCHEMA_FALSE };
_Static_assert(sizeof(pangu_schema_view)==32,"Schema view layout");
_Static_assert(sizeof(pangu_schema_limits)==16,"Schema limits layout");
_Static_assert(sizeof(pangu_schema_report)==32,"Schema report layout");
/* Strict, explicit subset; no coercion, default insertion or repair. Every
 * logical child Schema is checked even if its value path is absent. Supports
 * boolean Schemas; type/name arrays; properties, required, one items Schema,
 * boolean additionalProperties; primitive enum/const; title/description/$comment
 * annotations. Unsupported forms/keywords and JS prototype-sensitive declared
 * property/required names return 9, never unchecked success. Number is finite
 * binary64; integer uses Number.isInteger after conversion. Object lookup uses
 * decoded-key last-wins. No arbitrary Schema dialect/$ref/regex support.
 * Inputs/results must be successful, matching and immutable. max_depth 1..64,
 * positive max_nodes/work; work is deterministic operation + examined-byte
 * accounting, not CPU time. No heap. Report storage must be disjoint from both
 * views/results/tokens/inputs and limits. Precondition errors 1/10 leave report
 * unchanged; after traversal starts it reports status/reason/token/work.
 * 0 valid, 15 value mismatch, 16 invalid definition, 9 unsupported, 2 quota;
 * source errors may propagate. Missing token index is UINT64_MAX. Caller owns
 * all storage/lifetimes; bounds checks do not authenticate arbitrary pointers.
 */
pangu_i32 pangu_schema_check(const pangu_schema_view *,const pangu_schema_limits *,pangu_schema_report *);
pangu_i32 pangu_schema_validate(const pangu_schema_view *,const pangu_schema_view *,const pangu_schema_limits *,pangu_schema_report *);
#endif
#endif
