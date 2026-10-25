#ifndef PANGU_NATIVE_PLUGIN_H
#define PANGU_NATIVE_PLUGIN_H
#include "host.h"
#include <stdint.h>
#define PG_NATIVE_ABI 1u
/* Shared libraries export this read-only object as pangu_plugin_v1. */
typedef struct {
 uint32_t abi,size,plugin_size,reserved;
 const pg_plugin *plugins;
 size_t count;
} pg_native_manifest;
typedef struct {
 pg_host host;
 void *library;
 unsigned state,error;
} pg_native_plugin;
typedef enum { PG_NATIVE_INVALID=1,PG_NATIVE_OPEN,PG_NATIVE_SYMBOL,
 PG_NATIVE_ABI_MISMATCH,PG_NATIVE_HOST,PG_NATIVE_UNLOAD } pg_native_error;
/* Zeroed/completely closed storage, serialized calls. Absolute NUL-terminated
   path <=4095 bytes. NULL parent opens an isolated root, otherwise a child.
   Native code has process privileges. Returns 0 active, 1 failed; failed open
   may retain a library while cleanup is pending: always drive close afterward. */
int pg_native_open(pg_native_plugin *,const char *,pg_host *parent);
/* Releases scope before unloading code. 0 released, 1 pending, -1 error;
   pending/error retain storage and handle. Repeated completed close is safe. */
int pg_native_close(pg_native_plugin *);
#endif
