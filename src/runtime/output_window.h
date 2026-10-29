#ifndef PANGU_OUTPUT_WINDOW_H
#define PANGU_OUTPUT_WINDOW_H
#include "../contracts/client.h"
/* Borrowed valid UTF-8 input. Result names an exact slice of the same bytes;
   whole lines are retained where possible, including a trailing newline.
   tail=0 selects head, tail=1 selects tail. Zero limits retain nothing.
   This performs no allocation or sanitization. Output is disjoint from input. */
typedef struct {size_t offset,size,dropped_bytes,dropped_lines;} pg_output_window;
int pg_output_bound(pg_slice,size_t max_bytes,size_t max_lines,unsigned tail,pg_output_window *);
#endif
