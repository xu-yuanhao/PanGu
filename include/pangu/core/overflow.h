#ifndef PANGU_CORE_OVERFLOW_H
#define PANGU_CORE_OVERFLOW_H
#include <pangu/core/core.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Optional PanGu::overflow library; base PanGu::core has no regex dependency.
 * Port of Pi ai/utils/overflow.ts at d5b3de22. A classifier owns compiled
 * patterns and allocator context must outlive it. NULL allocator selects malloc.
 * Create sets *out=NULL on failure; destroy accepts NULL. Same-thread serialized
 * calls, no workers/events/cancellation or transcript mutation. Allocation hooks
 * run on the caller thread and must not reenter/destroy the classifier. */
typedef struct pangu_overflow_classifier pangu_overflow_classifier;
pangu_status pangu_overflow_create(const pangu_allocator *,pangu_overflow_classifier **out);
void pangu_overflow_destroy(pangu_overflow_classifier *);
/* Borrow assistant/error/provider slices only through return. Error is UTF-8,
 * length-delimited, may include embedded NUL; invalid UTF-8 returns INVALID when
 * matching is required. capacity=0 means unknown. Pattern exclusions precede
 * positive matches; bodyless 400 is Cerebras-only (case-sensitive provider).
 * Numeric checks use source JS Number arithmetic; cache_write is not added.
 * On failure *out is unchanged; regex/allocator failures are never a non-match.
 * This is classification only: caller owns compaction/retry/commit decisions. */
pangu_status pangu_message_is_context_overflow(pangu_overflow_classifier *,
    const pangu_message *,uint64_t capacity,int *out);
/* Source isRecoverableLength: LENGTH, nonzero original desired output limit,
 * and output below that original (not context-clamped) limit. NULL/non-assistant
 * returns false. Integer limits are converted using source JS Number rounding.
 * No allocation; no retained data or side effects. */
int pangu_message_is_recoverable_length(const pangu_message *,uint64_t desired_max_output);
#ifdef __cplusplus
}
#endif
#endif
