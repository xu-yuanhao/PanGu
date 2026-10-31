#ifndef PANGU_CREDENTIALS_H
#define PANGU_CREDENTIALS_H
#include <stddef.h>
struct json_object;
/* Current credential is borrowed and immutable; next transfers one owned value.
   A NULL next preserves current. Callbacks execute under the store write lock. */
typedef int (*pg_credential_update)(void *,const struct json_object *,struct json_object **);
typedef struct pg_credential_store {
 void *context;
 int (*read)(void *,const char *,struct json_object **);
 int (*list)(void *,struct json_object **);
 int (*modify)(void *,const char *,pg_credential_update,void *,struct json_object **);
 int (*remove)(void *,const char *);
} pg_credential_store;
_Static_assert(sizeof(pg_credential_store)==40,"credential store ABI");
_Static_assert(offsetof(pg_credential_store,read)==8,"credential read ABI");
/* Path is borrowed for the store lifetime. Results are detached owned JSON.
   File transactions use a bounded synchronous lock wait.
   Missing entries return NULL; list exposes providerId/type only. Status 0 is
   success, 5 is invalid data/arguments, 7 is storage/allocation failure. */
int pg_credentials_file_init(pg_credential_store *,const char *);
/* POSIX transaction bridge: callback receives NUL-terminated file bytes.
   Replacement is owned malloc storage, persisted atomically and freed here. */
int pg_credentials_file_access(const char *,int,int (*)(void *,const char *,size_t,char **),void *);
#endif
