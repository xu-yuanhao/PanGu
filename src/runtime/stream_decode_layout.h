#ifndef PG_STREAM_DECODE_LAYOUT_H
#define PG_STREAM_DECODE_LAYOUT_H
#define PG_D_BYTES 0
#define PG_D_COUNT 4
#define PG_D_NEEDED 8
#define PG_D_LOWER 12
#define PG_D_UPPER 16
#define PG_D_STARTED 20
#define PG_D_FINISHED 24
#define PG_D_SIZE 28
#ifndef __ASSEMBLER__
#include "stream_decode.h"
#include <stddef.h>
#define PG_DO(f,n) _Static_assert(offsetof(pg_stream_decoder,f)==n,"decoder." #f)
PG_DO(bytes,PG_D_BYTES);PG_DO(count,PG_D_COUNT);PG_DO(needed,PG_D_NEEDED);
PG_DO(lower,PG_D_LOWER);PG_DO(upper,PG_D_UPPER);PG_DO(started,PG_D_STARTED);
PG_DO(finished,PG_D_FINISHED);
_Static_assert(sizeof(pg_stream_decoder)==PG_D_SIZE,"decoder size");
#endif
#endif
