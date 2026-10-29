#ifndef PANGU_JSON_SCHEMA_BRIDGE_H
#define PANGU_JSON_SCHEMA_BRIDGE_H
struct json_object;
#ifdef __cplusplus
extern "C" {
#endif
/* Draft-7 strong validation through Valijson. Compile owns its schema; inputs
   are borrowed. Owned error text uses malloc/free. Remote refs have no fetcher.
   0 success, 1 value mismatch, 4 invalid schema/argument, 5 allocation failure. */
int pg_json_schema_compile(const char *,void **,char **);
int pg_json_schema_validate(void *,const struct json_object *,char **);
void pg_json_schema_close(void *);
#ifdef __cplusplus
}
#endif
#endif
