#ifndef PG_EDIT_DIFF_H
#define PG_EDIT_DIFF_H
#include "session.h"
struct json_object;
/* Pi Durable line comparison and four-line context. Inputs are bounded valid
   UTF-8 text without NUL (<4096 bytes); strips BOM and normalizes CRLF. Transfers owned
   {diff,patch,firstChangedLine?}; no change has no firstChangedLine. */
int pg_edit_diff_create(pg_slice path,pg_slice before,pg_slice after,struct json_object **);
#endif
