#ifndef PANGU_PI_ARGUMENTS_H
#define PANGU_PI_ARGUMENTS_H
#include "src/bootstrap-api/text.h"
#ifndef __ASSEMBLER__
/* Pi repairJson lexical transformation only: NOT JSON validation/completion,
 * final tool arguments, Schema conversion or authorization to execute.
 * Input must be complete valid UTF-8, borrowed/immutable; escaped UTF-16 units
 * remain ASCII spelling, no replacement/normalization. max_bytes bounds input.
 * NULL output with capacity zero measures and succeeds; otherwise writes exact
 * bytes without NUL. written must be valid, aligned and disjoint from source
 * and destination capacity; source/destination must not overlap. Every failure
 * leaves destination and written unchanged. 0 success, 1 invalid/overlap/range,
 * 2 input limit/capacity/size overflow, 12 invalid UTF-8. Caller owns all storage.
 */
pangu_i32 pangu_pi_repair_utf8(pangu_text_slice,unsigned char *,pangu_u64,pangu_u64,pangu_u64 *);
#include "src/bootstrap-api/json.h"
typedef struct pangu_pi_arguments_result {
 pangu_text_slice input;pangu_json_result json;
 pangu_u32 path,reserved;pangu_u64 original_error_offset;
} pangu_pi_arguments_result;
_Static_assert(sizeof(pangu_pi_arguments_result)==64,"Pi arguments result layout");
enum { PANGU_PI_ARGUMENTS_STRICT=1,PANGU_PI_ARGUMENTS_REPAIRED=2,PANGU_PI_ARGUMENTS_EMPTY=3 };
/* Complete-tool-text adapter, not Schema validation or execution authority.
 * final=0 publishes no tokens and returns 13; final=1 attempts strict JSON first,
 * then only on syntax/incomplete tries lexical repair, never completion. Empty
 * ECMAScript-trim whitespace maps to a static borrowed {}. Only OBJECT succeeds.
 * Input must remain immutable. Caller owns scratch/tokens; every storage region
 * (input, scratch capacity, tokens capacity, result) must be mutually disjoint.
 * Scratch/tokens are disposable scratch and may change on any runtime failure;
 * callers must invalidate all previous borrowed views before every call.
 * Success borrows source, scratch, or static {} via result.input and caller
 * tokens via result.json. Failure clears published input/tokens and stores its
 * status; precondition error 1 leaves result untouched. Always check return.
 * Limits bound both original and selected normalized bytes, depth and token
 * capacity. max_bytes must be >=2 to accept blank-as-{}. UTF8 error 12,
 * quotas 2, syntax 14/incomplete 13, nonobject 9. path records selected branch,
 * even on failure; original_error_offset is the strict parse error offset when
 * repair was considered. Parser error offsets refer to the selected source.
 * No JS error-message reproduction, partial object salvage, optional-null,
 * coercion, Schema validation, allocation or tool execution. Borrowed storage
 * validity/alignment/lifetime is the caller's responsibility, not authenticated.
 */
pangu_i32 pangu_pi_arguments_parse(pangu_text_slice,pangu_u32,unsigned char *,pangu_u64,pangu_json_token *,pangu_u64,pangu_json_limits,pangu_pi_arguments_result *);
#include "src/bootstrap-api/schema.h"
/* Pi normalizeOptionalNulls stage ONLY; does not coerce/validate parameters or
 * authorize execution. Supported Schema profile is foundation.schema's subset.
 * Drops optional own properties whose cached child validator rejects null;
 * boolean child Schemas preserve null, matching Pi primitive WeakMap failure.
 * Traverses
 * properties/items regardless of declared type. Required/nullable nulls remain.
 * Duplicate decoded names use last-wins; deleting a logical field removes every
 * raw duplicate so shadowed values cannot reappear. Escaped UTF16/raw numeric
 * lexemes survive. Output is equivalent JSON, not JS stringify/wire byte parity.
 * Caller supplies immutable matching successful schema/value views and mutable
 * drops workspace (u64 indices), destination, written and report; all mutable
 * ranges mutually disjoint and disjoint from BOTH views/results/inputs/tokens.
 * Workspace may change on runtime failure. Output bytes/written unchanged on
 * every failure; precondition 1/10 also preserves report. NULL/0 destination
 * measures successfully. No NUL; caller reparses before further processing.
 * Limits bound supported Schema/visited paths and aggregate abstract work;
 * unchanged subtrees may be copied raw without visiting their descendants.
 * 0 normalized (possibly still Schema-invalid),2 quota,9 unsupported,16 invalid
 * Schema; source errors propagate. No allocation or raw-pointer authentication.
 */
pangu_i32 pangu_pi_optional_nulls(const pangu_schema_view *,const pangu_schema_view *,pangu_schema_limits,pangu_u64 *,pangu_u64,unsigned char *,pangu_u64,pangu_u64 *,pangu_schema_report *);
/* ECMAScript Number(string) prerequisite, NOT TypeBox TryNumber or full Pi
 * coercion. Accepts decoded strict UTF8, trims ECMAScript whitespace, handles
 * empty/+/-/decimal/exponents/Infinity and unsigned 0x/0o/0b integer text.
 * Invalid numeric grammar succeeds with canonical quiet NaN bits; callers must
 * apply their own finite/integer/conversion policy. No raw unpaired UTF16 input.
 * Decimal canonicalization uses caller scratch (at most input.length+2 bytes),
 * then unchanged strict JSON and binary64 conversion; numeric lexeme max1MiB.
 * max_bytes bounds input before scanning. Nondecimal/empty/Infinity/invalid
 * grammar need no scratch; decimal scratch shortage/limits return2, UTF8 12,
 * invalid arguments/overlap1. Source, scratch capacity and aligned u64 output
 * must be disjoint. Every failure preserves bits; scratch is disposable.
 * No allocation/FP arithmetic/libc or tool execution; caller owns lifetimes.
 */
pangu_i32 pangu_pi_number_string(pangu_text_slice,unsigned char *,pangu_u64,pangu_u64,pangu_u64 *);
enum pangu_pi_scalar_target { PANGU_PI_SCALAR_NUMBER=1,PANGU_PI_SCALAR_INTEGER,PANGU_PI_SCALAR_STRING,PANGU_PI_SCALAR_BOOLEAN,PANGU_PI_SCALAR_NULL };
enum pangu_pi_scalar_mode { PANGU_PI_TYPEBOX_RULE=1,PANGU_PI_PLAIN_RULE,PANGU_PI_TYPED_BASIC };
typedef struct pangu_pi_scalar_result {
 pangu_u32 kind,reserved;pangu_u64 number_bits;pangu_text_slice text;
} pangu_pi_scalar_result;
_Static_assert(sizeof(pangu_pi_scalar_result)==32,"scalar result layout");
/* One UNCONSTRAINED primitive interior-value rule, not complete Schema coercion,
 * traversal, optional-null normalization, literal/enum/union, validation or tool
 * authorization. Modes: TypeBox rule; Pi plain rule; fixed TypeBox1.3.27 basic
 * typed rule THEN Pi plain rule. Caller selects origin; JSON cannot retain kind
 * metadata. Source must be successful immutable matching JSON value (not key).
 * Result kind0 retains original token; replacements use JSON NULL/TRUE/FALSE/
 * NUMBER/STRING codes. Replacement numbers are finite binary64; strings borrow
 * static ASCII or workspace (consume before reuse), with no NUL terminator.
 * Original STRING tokens are preserved including lone escaped UTF16. A lone
 * surrogate cannot match numeric/boolean/null syntax and is retained unchanged.
 * max_bytes bounds selected lexeme. Caller workspace supports decoded UTF8 plus
 * decimal scratch, or number-to-string output: max(25,2*lexeme_bytes+2) suffices
 * subject to existing1MiB numeric cap. NULL/zero allowed where scratch unneeded.
 * Workspace disposable; result unchanged on every failure. Source/view/result/
 * tokens, workspace capacity and output must be disjoint (readonly contexts may
 * overlap). 1 preconditions,2 quotas,9 unsupported kind,10 bad result; source
 * errors propagate. Status0 may still produce a Schema-invalid retained value.
 * Caller owns allocation, alignment, lifetime and exclusive workspace. No heap,
 * raw-pointer authentication or JS stringify/wire serialization. Preserve -0
 * when transporting replacement number bits between conversion stages.
 */
pangu_i32 pangu_pi_scalar_convert(const pangu_schema_view *,pangu_u32,pangu_u32,unsigned char *,pangu_u64,pangu_u64,pangu_pi_scalar_result *);
/* EXPERIMENTAL tree stage: kind metadata supplied by caller; never infer from
 * serialized Schema. Pass1 TypeBox, pass2 Pi plain. Kind0 identity, 1..5 scalar
 * targets,6 Array,7 Object,8 Literal,9 Enum; other typed kinds unsupported9.
 * Matching successful immutable views required; no authorization/final validation.
 * Caller supplies disjoint scratch/output/written/report and immutable kind map
 * indexed by schema token. Measurement NULL/0 succeeds; failure preserves output
 * and written. Scratch/report disposable after runtime failure. Depth <=64.
 * This is a staged converter, not public validateToolArguments (root return differs).
 */
typedef struct pangu_pi_tree_schema {
 pangu_schema_view json;const pangu_u32 *kinds;pangu_u64 kind_count;
} pangu_pi_tree_schema;
_Static_assert(sizeof(pangu_pi_tree_schema)==48,"tree schema layout");
pangu_i32 pangu_pi_tree_convert(const pangu_pi_tree_schema *,const pangu_schema_view *,pangu_u32,pangu_schema_limits,unsigned char *,pangu_u64,unsigned char *,pangu_u64,pangu_u64 *,pangu_schema_report *);
#endif
#endif
