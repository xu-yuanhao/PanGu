#ifndef PG_STREAM_DECODE_H
#define PG_STREAM_DECODE_H
#include "../contracts/client.h"
/* One state per byte stream. Serialized calls, disjoint state/input/output.
   UTF-8 replacement and initial BOM handling follow TextDecoder; an incomplete
   prefix is retained until the next chunk or final flush. */
typedef struct {
 unsigned char bytes[4];
 unsigned count, needed, lower, upper, started, finished;
} pg_stream_decoder;
void pg_stream_decoder_init(pg_stream_decoder *);
/* Input <=4096 bytes; capacity >=3*input.size+3 even for an empty final flush.
   Return 0 success, 1 invalid argument/capacity; rejection leaves state intact.
   Successful final seals the stream; repeated empty final is allowed. */
int pg_stream_decode(pg_stream_decoder *,pg_slice,unsigned final,
                     char *,size_t capacity,size_t *written);
#endif
