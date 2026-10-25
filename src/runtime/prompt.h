#ifndef PANGU_PROMPT_H
#define PANGU_PROMPT_H
#include "tools.h"
/* Borrowed synchronous complete UTF-8 read service. No Session mutation.
   Inputs, output and size storage must be disjoint.
   Success replaces the whole system prompt, including an intentionally empty file.
   Failure clears output/size; partial and asynchronous results are not accepted. */
int pg_prompt_from_file(const pg_tools *, pg_slice path, char *, size_t, size_t *);
#endif
