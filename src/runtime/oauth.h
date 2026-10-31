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
struct pg_oauth_http;
typedef struct pg_oauth_device_config {
 const char *start_url,*poll_url,*token_url,*verification_uri,*redirect_uri;
 void *clock_context;double (*now_ms)(void *);
} pg_oauth_device_config;
typedef struct pg_oauth_device {
 const pg_credential_store *store;
 const pg_oauth_device_config *config;
 struct pg_oauth_http *http;
 struct json_object *device,*credential;
 unsigned phase,slow_down;
 unsigned long long interval_ms,deadline,next_poll;
 struct json_object *browser;
 const struct pg_oauth_browser_config *browser_config;
 struct pg_oauth_callback *callback;
 const char *identity;
 const struct pg_copilot_login_config *copilot_config;
 char *enterprise_domain;
 unsigned long long retry_deadline;
 unsigned retry_count,resume_phase;
 size_t policy_index;
} pg_oauth_device;
_Static_assert(sizeof(pg_oauth_device)==144,"device login ABI");
_Static_assert(offsetof(pg_oauth_device,next_poll)==64,"device timer ABI");
_Static_assert(offsetof(pg_oauth_device_config,now_ms)==48,"device clock ABI");
/* Serialized calls. Config/store are borrowed; NULL config selects Codex.
   Poll returns a detached public snapshot; credentials never appear in it.
   Cancel closes in-flight HTTP; completed credentials are already persisted. */
void pg_oauth_device_init(pg_oauth_device *,const pg_credential_store *,const pg_oauth_device_config *);
int pg_oauth_device_begin(void *,const char *);
int pg_oauth_device_poll(void *,struct json_object **);
void pg_oauth_device_cancel(void *);
void pg_oauth_device_close(pg_oauth_device *);
typedef struct pg_auth_ops {
 void *context;
 int (*begin)(void *,const char *);
 int (*poll)(void *,struct json_object **);
 void (*cancel)(void *);
 int (*list)(void *,struct json_object **);
 int (*remove)(void *,const char *);
 int (*browser_begin)(void *,const char *);
 int (*submit_code)(void *,const char *);
 int (*copilot_begin)(void *,const char *);
} pg_auth_ops;
_Static_assert(sizeof(pg_auth_ops)==72,"auth operations ABI");
void pg_oauth_device_service(pg_oauth_device *,pg_auth_ops *);
/* Shared final token validation for code exchange and refresh. */
int pg_oauth_codex_token(const struct json_object *,struct json_object **,double);
int pg_oauth_http_start(struct pg_oauth_http **,const char *,const char *,const char *);
/* 1 waiting, 0 complete, 7 failure; body is borrowed through close. */
int pg_oauth_http_step(struct pg_oauth_http *,long *,const char **);
void pg_oauth_http_close(struct pg_oauth_http *);
typedef struct pg_oauth_browser_config {
 const char *authorize_url,*token_url,*redirect_uri,*host;
 unsigned port;
} pg_oauth_browser_config;
void pg_oauth_browser_configure(pg_oauth_device *,const pg_oauth_browser_config *);
int pg_oauth_browser_begin(void *,const char *);
int pg_oauth_browser_submit(void *,const char *);
void pg_oauth_browser_poll(pg_oauth_device *);
int pg_oauth_authorization_parse(const char *,struct json_object **);
int pg_oauth_pkce(struct json_object **);
/* Shared code exchange; copies the form into the cooperative HTTP request. */
int pg_oauth_login_exchange(pg_oauth_device *,const struct json_object *,const char *,const char *);
struct pg_oauth_callback *pg_oauth_callback_open(const char *,unsigned,void *,int (*)(void *,int,const char *));
void pg_oauth_callback_close(struct pg_oauth_callback *);
int pg_oauth_callback_pump(struct pg_oauth_callback *);
int pg_oauth_callback_path(const char *,const char *);
int pg_oauth_query_fields(const char *,int,char **);
typedef struct pg_copilot_config {
 const char *token_url,*models_base_url,*enterprise_domain;
} pg_copilot_config;
void pg_oauth_copilot_init(pg_oauth_provider *);
int pg_oauth_copilot_refresh(void *,const struct json_object *,struct json_object **);
/* Owned JSON base URL derived from access token / enterprise identity. */
struct json_object *pg_oauth_copilot_base(const struct json_object *);
int pg_copilot_model_allowed(const struct json_object *,const char *);
int pg_copilot_catalog(const struct json_object *,int,struct json_object **);
/* Owned result; an unfiltered catalog retains the input reference. */
int pg_copilot_filter_catalog(const struct json_object *,const struct json_object *,struct json_object **);
int pg_oauth_get_json(const char *,const char *const *,size_t,double,char **);
struct json_object *pg_oauth_json_parse(const char *);
const char *pg_oauth_string_field(const struct json_object *,const char *);
typedef struct pg_copilot_login_config {
 const char *device_url,*access_url,*token_url,*models_base_url,*enterprise_domain;
 void *clock_context;double (*now_ms)(void *);
 int (*known_model)(const char *);
} pg_copilot_login_config;
void pg_oauth_copilot_configure(pg_oauth_device *,const pg_copilot_login_config *);
int pg_oauth_copilot_begin(void *,const char *);
void pg_oauth_copilot_poll(pg_oauth_device *);
char *pg_oauth_copilot_domain(const char *);
int pg_oauth_copilot_token(const struct json_object *,const char *,const char *,struct json_object **);
int pg_copilot_policy_models(const struct json_object *,int,int (*)(const char *),struct json_object **);
int pg_copilot_merge_models(const struct json_object *,const struct json_object *,struct json_object **);
int pg_oauth_http_headers(struct pg_oauth_http *,const char *const *,size_t,unsigned);
const char *pg_oauth_http_retry_after(const struct pg_oauth_http *);
#endif
