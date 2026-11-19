#ifndef PANGU_STREAMING_JSON_H
#define PANGU_STREAMING_JSON_H
#include "pangu/core/core.h"
/* Private Core ABI. Pi d5b3de22, ai/src/utils/streaming-json.ts.
 * Single-owner synchronous calls; no callbacks, threads or cancellation work.
 * Allocator is required, paired, and lives through destroy. Every allocation
 * uses it. append copies its byte slice (NULL only with zero length).
 * snapshot/finish slices are borrowed through the next append/finish/destroy.
 * Snapshots are valid JSON objects, with a credit-bounded preview that may lag.
 * finish repairs invalid escapes/raw controls as Pi does, then validates the
 * complete structure strictly; malformed/nonobject input returns
 * {} plus an error, not a partial value. Empty/whitespace input returns {}.
 * OOM is sticky until destruction. Invalid call arguments do not poison state.
 * No replace/change-event API: consumers may destroy/create for replacement.
 */
typedef struct pangu_json_stream pangu_json_stream;
/* Borrowed view after ECMAScript String.trim; caller supplies a valid slice.
 * Used for empty tool arguments and proxy SSE data framing, not JSON grammar. */
pangu_slice pangu_json_trim(pangu_slice bytes);
pangu_status pangu_json_stream_create(pangu_allocator allocator, pangu_json_stream **out);
void pangu_json_stream_destroy(pangu_json_stream *stream);
pangu_status pangu_json_stream_append(pangu_json_stream *stream, pangu_slice bytes);
pangu_status pangu_json_stream_snapshot(pangu_json_stream *stream, pangu_slice *json);
pangu_status pangu_json_stream_finish(pangu_json_stream *stream, pangu_slice *arguments, pangu_slice *error);
#endif
