#ifndef PANGU_IO_H
#define PANGU_IO_H
#include "src/bootstrap-api/bootstrap.h"
#define PANGU_IO_ABI_VERSION 1
#define PANGU_IO_STRUCT_SIZE 64
#ifndef __ASSEMBLER__
/* Valid borrowed NUL-terminated path; no authorization or path confinement claim.
 * open mode 0=read, 1=exclusive create (0600). Only regular files, no final symlink.
 * fd output must initially be -1. Transfers may be short; EOF succeeds with zero.
 * close consumes fd before returning; never retry a consumed descriptor.
 */
typedef pangu_i32 (*pangu_open_fn)(void *,const char *,pangu_u32,pangu_i32 *);
typedef pangu_i32 (*pangu_read_fn)(void *,pangu_i32,void *,pangu_u64,pangu_u64 *);
typedef pangu_i32 (*pangu_write_fn)(void *,pangu_i32,const void *,pangu_u64,pangu_u64 *);
typedef pangu_i32 (*pangu_close_fn)(void *,pangu_i32 *);
typedef pangu_i32 (*pangu_clock_fn)(void *,pangu_u64 *);
typedef struct pangu_io_api {
 pangu_u32 abi_version,struct_size;
 void *context;
 pangu_open_fn open;
 pangu_read_fn read;
 pangu_write_fn write;
 pangu_close_fn close;
 pangu_clock_fn monotonic_ns;
 pangu_u64 reserved;
} pangu_io_api;
_Static_assert(sizeof(pangu_io_api)==64,"IO API size");
_Static_assert(__builtin_offsetof(pangu_io_api,open)==16,"open offset");
_Static_assert(__builtin_offsetof(pangu_io_api,monotonic_ns)==48,"clock offset");
_Static_assert(__builtin_offsetof(pangu_io_api,reserved)==56,"reserved offset");
pangu_i32 pangu_file_open(void *,const char *,pangu_u32,pangu_i32 *);
pangu_i32 pangu_file_read(void *,pangu_i32,void *,pangu_u64,pangu_u64 *);
pangu_i32 pangu_file_write(void *,pangu_i32,const void *,pangu_u64,pangu_u64 *);
pangu_i32 pangu_file_close(void *,pangu_i32 *);
pangu_i32 pangu_clock_monotonic(void *,pangu_u64 *);
#endif
#endif
