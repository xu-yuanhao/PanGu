#ifndef PANGU_TEXT_H
#define PANGU_TEXT_H
#include "src/bootstrap-api/buffer.h"
#ifndef __ASSEMBLER__
typedef struct pangu_text_slice { const unsigned char *data; pangu_u64 length; } pangu_text_slice;
_Static_assert(sizeof(pangu_text_slice)==16,"text slice layout");
/* Borrowed readable storage must outlive the call; bytes may contain NUL.
 * Offsets are bytes, scalar values exclude surrogates. No BOM stripping,
 * normalization, grapheme segmentation, display width or UTF-16 indexing.
 * next: 0 success, 3 end, 12 invalid/truncated UTF-8, 1 invalid arguments.
 * next updates cursor/scalar only on success. Outputs must be distinct storage.
 * validate reports complete prefix scalar count and offending lead-byte offset;
 * on success error_offset==length. NULL data is allowed only for length zero.
 * append validates the whole slice before Buffer mutation. No partial append.
 */
pangu_i32 pangu_text_next(pangu_text_slice,pangu_u64 *,pangu_u32 *);
pangu_i32 pangu_text_validate(pangu_text_slice,pangu_u64 *,pangu_u64 *);
pangu_i32 pangu_text_append(pangu_buffer *,pangu_text_slice);
#endif
#endif
