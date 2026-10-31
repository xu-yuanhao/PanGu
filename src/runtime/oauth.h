#ifndef PANGU_OAUTH_H
#define PANGU_OAUTH_H
#include "credentials.h"
typedef struct pg_oauth_provider {
 void *context;
 const char *identity;
 double (*now_ms)(void *);
 pg_credential_update refresh;
} pg_oauth_provider;
_Static_assert(sizeof(pg_oauth_provider)==32,"OAuth provider ABI");
/* All inputs borrowed; out receives an owned credential only after successful
   persistence. Refresh rechecks current credentials under the store lock.
   Once started it is bounded to 15 seconds and persists rotated tokens. */
int pg_oauth_resolve(const pg_credential_store *,const char *,const pg_oauth_provider *,const struct json_object *,struct json_object **);
/* Owned account-id JSON string, extracted from JWT routing claims. */
int pg_oauth_codex_account(const char *,struct json_object **);
void pg_oauth_codex_init(pg_oauth_provider *);
int pg_oauth_codex_refresh(void *,const struct json_object *,struct json_object **);
/* Platform/libcurl bridges. Form response is malloc-owned and never logged. */
double pg_oauth_realtime(void *);
int pg_oauth_post_form(const char *,const char *,char **);
#endif
