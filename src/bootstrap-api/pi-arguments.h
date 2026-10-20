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
#endif
#endif
