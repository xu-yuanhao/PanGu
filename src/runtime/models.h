#ifndef PANGU_MODELS_H
#define PANGU_MODELS_H
struct json_object;
/* Owned private catalog from built-ins and optional user models.json.
   Missing/malformed file follows settings diagnostics; release with json_object_put. */
int pg_models_load(const char *,struct json_object **);
/* Owned public metadata, selected by modelProvider/model or endpoint/model.
   An unknown model yields {}. Catalog/configuration remain borrowed. */
int pg_models_describe(const struct json_object *,const struct json_object *,struct json_object **);
/* Owned resolved enabled/reserveTokens/keepRecentTokens/backgroundTokens,
   applying compaction.modelOverrides[provider/model] field by field. */
int pg_models_compaction_policy(const struct json_object *,struct json_object **);
/* Owned public descriptors, provider NULL enumerates all registered catalogs.
   Custom entries override equal built-ins through the same describe path. */
int pg_models_list(const struct json_object *,const char *provider,struct json_object **);
typedef struct {void *context;int (*list)(void *,const char *,struct json_object **);void (*release)(struct json_object *);} pg_model_catalog_ops;
#endif
