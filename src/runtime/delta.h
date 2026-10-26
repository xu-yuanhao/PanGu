#ifndef PANGU_DELTA_H
#define PANGU_DELTA_H
#include "documents.h"
/* Decoded Chord Op tuples over borrowed finite UTF-8 JSON values. Success
   returns a detached owned value (NULL represents JSON null); failure clears
   output. Output is disjoint from both inputs. Paths use string keys or
   nonnegative safe integers; object keys use the json-c C-string vocabulary.
   Text truncation counts UTF-16 units and requires a scalar boundary.
   Inputs and operations remain unchanged, including on failure. */
pg_document_status pg_document_delta_apply(const struct json_object *,const struct json_object *,struct json_object **);
/* Ordered array of decoded operation batches; only the final value is exposed. */
pg_document_status pg_document_delta_batches(const struct json_object *,const struct json_object *,struct json_object **);
#endif
