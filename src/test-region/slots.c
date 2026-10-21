#include "src/bootstrap-api/region.h"
/* Test only: 16 fixed 256-byte slots. Scope is the whole process; no reset.
 * No arena or memory-provider implementation is linked in this composition. */
_Alignas(256) static unsigned char slots[16][256];
static pangu_u32 used;
pangu_i32 pangu_test_slots_allocate(void *context,pangu_u64 size,pangu_u64 alignment,void **out)
{
 (void)context;
 if(!out) return 1;
 *out=(void *)0;
 if(!size || !alignment || alignment>4096 || (alignment&(alignment-1))) return 1;
 if(size>256 || alignment>256 || used==16) return 2;
 *out=slots[used++];return 0;
}
pangu_u64 pangu_test_slots_epoch(void *context)
{
 (void)context;return 1;
}
