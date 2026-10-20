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
#endif
#endif
