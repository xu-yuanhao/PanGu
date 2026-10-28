#ifndef PANGU_SETTINGS_H
#define PANGU_SETTINGS_H
#include "skills.h"
#include <json-c/json_object_iterator.h>
_Static_assert(sizeof(struct json_object_iterator)==sizeof(void *),"json-c iterator ABI");
/* Detached recursive object merge; arrays/scalars/null are replaced. */
int pg_settings_merge(const struct json_object *,const struct json_object *,struct json_object **);
/* Optional absolute filenames. Owned {global,project,settings,diagnostics}.
   Missing files are empty; malformed files produce diagnostics and an empty
   layer. Failures leave *out NULL. No settings values are logged or persisted. */
int pg_settings_load(const char *,const char *,struct json_object **);
/* Resolve plain resource paths in each owning scope. Caller owns JSON array;
   root and inputs borrowed. Relative paths use the corresponding base directory.
   Native configuration paths are bounded to 4095 bytes. */
int pg_settings_resource_paths(const struct json_object *,const char *,const char *,const char *,struct json_object **);
#endif
