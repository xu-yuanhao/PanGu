#ifndef PANGU_RESOURCE_SELECT_H
#define PANGU_RESOURCE_SELECT_H
struct json_object;
/* Source descriptor: {path,base,patterns,defaults}. defaults applies only
   !/+/- overrides; explicit sources also apply positive glob filters.
   Return 1 enabled, 0 disabled, -1 malformed/over-budget/allocation failure. */
int pg_resource_selected(const char *,const struct json_object *);
/* Case-sensitive pathname glob with hidden-segment handling. Inputs <4096
   UTF-8 bytes; brace nesting <=32, sequence endpoints/step are safe IEEE integer
   values. Sequences are bounded to 100000 results and 4000000 expanded bytes. */
int pg_resource_glob(const char *,const char *);
/* Scoped libc fnmatch/locale ABI bridge; zero means match. */
int pg_resource_fnmatch(const char *,const char *);
#endif
