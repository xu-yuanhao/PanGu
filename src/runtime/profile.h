#ifndef PANGU_PROFILE_H
#define PANGU_PROFILE_H
#include "tools.h"
/* Public, credential-free runtime configuration. Strings and registry are borrowed.
   implementation identifies the provider/tool policy contract, never function pointers. */
typedef struct {
    pg_slice provider, model, workspace, implementation;
    const pg_tool_registry *tools;
    uint64_t device, inode;
} pg_profile_spec;
/* Produces an exact length-framed identity <=4096 bytes. Failure leaves size zero. */
int pg_profile_encode(const pg_profile_spec *, void *out, size_t capacity, size_t *size);
#endif
