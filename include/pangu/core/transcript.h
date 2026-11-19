#ifndef PANGU_CORE_TRANSCRIPT_H
#define PANGU_CORE_TRANSCRIPT_H
#include <pangu/core/core.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Return an independently owned snapshot with zero or one replayed system
 * message. Appends nonempty system text with blank-line separators, patches
 * sections by name (deletion/re-addition changes insertion order), and applies
 * tool removals before additions. Caller destroys *out. Failure sets it NULL.
 * The source allocator/context must outlive the returned snapshot. */
pangu_status pangu_core_system_snapshot(const pangu_core *, pangu_core **out);
#ifdef __cplusplus
}
#endif
#endif
