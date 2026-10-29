#ifndef PANGU_OUTPUT_WINDOW_H
#define PANGU_OUTPUT_WINDOW_H
#include "../contracts/client.h"
/* Borrowed valid UTF-8 input. Result names an exact slice of the same bytes;
   whole lines are retained where possible, including a trailing newline.
   tail=0 selects head, tail=1 selects tail. Zero limits retain nothing.
   This performs no allocation or sanitization. Output is disjoint from input. */
typedef struct {size_t offset,size,dropped_bytes,dropped_lines;} pg_output_window;
int pg_output_bound(pg_slice,size_t max_bytes,size_t max_lines,unsigned tail,pg_output_window *);
/* In-place Chord sanitizeOutput. Returns retained size; no terminator written. */
size_t pg_output_sanitize(char *,size_t);
/* Decoded UTF-8 stream, zero-initialized; limits fixed before first push.
   Snapshot owns a NUL-terminated sanitized copy; caller frees text. Raw margin
   and total counters stay private until close. */
typedef struct {char *data;size_t size,total_bytes,total_newlines,max_bytes,max_lines;
 unsigned tail,ends_with_newline,full,reserved;} pg_output_buffer;
typedef struct {char *text;size_t size,dropped_bytes,dropped_lines;} pg_output_snapshot;
typedef struct {size_t bytes,newlines;unsigned ends_with_newline,reserved;} pg_output_skip;
/* Omitted decoded text immediately precedes the next push. Tail retention
   only; a nonempty skip discards previously stored margin, preserving totals. */
int pg_output_buffer_skip(pg_output_buffer *,const pg_output_skip *);
int pg_output_buffer_push(pg_output_buffer *,pg_slice);
int pg_output_buffer_snapshot(pg_output_buffer *,pg_output_snapshot *);
void pg_output_buffer_close(pg_output_buffer *);
#endif
