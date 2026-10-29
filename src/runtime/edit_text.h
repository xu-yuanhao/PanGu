#ifndef PG_EDIT_TEXT_H
#define PG_EDIT_TEXT_H
#include "session.h"
typedef struct { pg_slice old_text, new_text; } pg_text_edit;
typedef enum {
 PG_EDIT_OK, PG_EDIT_INVALID, PG_EDIT_QUOTA, PG_EDIT_NOT_FOUND,
 PG_EDIT_AMBIGUOUS, PG_EDIT_OVERLAP, PG_EDIT_UNCHANGED, PG_EDIT_NO_MEMORY
} pg_edit_status;
/* Pi Durable text transformation, without filesystem effects. Workspace and
   normalized strings are sized from the supplied content and nonempty edits.
   UTF-8 without NUL; output may alias input. size must be disjoint from
   inputs/output. On failure size=0, output unchanged. Success appends NUL.
   Preserves BOM/detected CRLF; fuzzy matching preserves untouched line blocks. */
pg_edit_status pg_edit_text_apply(pg_slice, const pg_text_edit *, size_t,
                                char *, size_t, size_t *);
#endif
