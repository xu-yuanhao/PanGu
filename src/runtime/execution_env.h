#ifndef PG_EXECUTION_ENV_H
#define PG_EXECUTION_ENV_H
#define PG_EXECUTION_ENV_ABI 1
#define PG_EE_CONTEXT 8
#define PG_EE_WORKSPACE 16
#define PG_EE_FILES 24
#define PG_EE_PROCESS 80
#define PG_EE_DIRECTORY 120
#define PG_EE_ENVIRONMENT 128
#define PG_EE_PREPARE 136
#define PG_EE_BYTES 144
#ifndef __ASSEMBLER__
#include "process.h"
#include "read_tool.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
 pg_slice command;
 int directory;
 char *const *environment;
} pg_execution_request;

/* Per-call shell preparation follows Durable BashPrepare: it may replace the
   command, cwd directory fd, and complete explicit environment. The command
   and environment are borrowed through process.start; the directory fd is
   needed only until that call returns. A nonzero result rejects the call. */
typedef int (*pg_execution_prepare)(void *,pg_execution_request *);

/* Host-owned composite of the capabilities used by coding tools. Provider
   storage and all referenced services remain valid through dependent close. */
typedef struct {
 uint32_t abi,size;
 void *context;
 pg_read_tool *workspace;
 pg_tools files;
 pg_process_ops process;
 int directory;
 char *const *environment;
 pg_execution_prepare prepare;
} pg_execution_env;

_Static_assert(offsetof(pg_execution_env,context)==PG_EE_CONTEXT,"execution env context ABI");
_Static_assert(offsetof(pg_execution_env,workspace)==PG_EE_WORKSPACE,"execution env workspace ABI");
_Static_assert(offsetof(pg_execution_env,files)==PG_EE_FILES,"execution env files ABI");
_Static_assert(offsetof(pg_execution_env,process)==PG_EE_PROCESS,"execution env process ABI");
_Static_assert(offsetof(pg_execution_env,directory)==PG_EE_DIRECTORY,"execution env directory ABI");
_Static_assert(offsetof(pg_execution_env,environment)==PG_EE_ENVIRONMENT,"execution env environment ABI");
_Static_assert(offsetof(pg_execution_env,prepare)==PG_EE_PREPARE,"execution env prepare ABI");
_Static_assert(sizeof(pg_execution_env)==PG_EE_BYTES,"execution env size ABI");
_Static_assert(sizeof(pg_execution_request)==32&&offsetof(pg_execution_request,directory)==16&&offsetof(pg_execution_request,environment)==24,"execution request ABI");
#endif
#endif
