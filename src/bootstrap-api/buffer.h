#ifndef PANGU_BUFFER_H
#define PANGU_BUFFER_H
#include "src/bootstrap-api/region.h"
#ifndef __ASSEMBLER__
typedef struct pangu_buffer {
 pangu_u32 abi_version,struct_size;
 const pangu_region_allocator *allocator;
 unsigned char *data;
 pangu_u64 length,capacity,limit,epoch,revision;
} pangu_buffer;
typedef struct pangu_buffer_borrow {
 const unsigned char *data;
 pangu_u64 length;
 const pangu_buffer *owner;
 pangu_u64 epoch,revision;
} pangu_buffer_borrow;
_Static_assert(sizeof(pangu_buffer)==64,"buffer layout");
_Static_assert(sizeof(pangu_buffer_borrow)==40,"view layout");
/* Zero-init metadata once. Borrowed allocator and owner storage must outlive use.
 * Binary bytes only, no NUL terminator. Appends may self-reference logical bytes.
 * Every mutation invalidates prior checked views. Raw pointers remain unchecked.
 * Growth retains old blocks until allocator reset/dispose; no per-block free.
 */
pangu_i32 pangu_buffer_init(pangu_buffer *,const pangu_region_allocator *,pangu_u64);
pangu_i32 pangu_buffer_append(pangu_buffer *,const void *,pangu_u64);
pangu_i32 pangu_buffer_clear(pangu_buffer *);
pangu_i32 pangu_buffer_dispose(pangu_buffer *);
pangu_i32 pangu_buffer_view(const pangu_buffer *,pangu_buffer_borrow *);
pangu_i32 pangu_buffer_view_validate(const pangu_buffer_borrow *);
#endif
#endif
