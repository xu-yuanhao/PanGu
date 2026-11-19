#ifndef PG_RESOURCE_IGNORE_H
#define PG_RESOURCE_IGNORE_H
#include <pangu/services/host.h>
typedef struct pg_resource_ignore pg_resource_ignore;
/* Private per-traversal matcher. No disk repository/config writes; the root is
 * borrowed through destroy. C callbacks and matching stay on the reload thread.
 */
pg_status pg_resource_ignore_create(const char *root, pg_resource_ignore **);
pg_status pg_resource_ignore_directory(pg_resource_ignore *,
                                       const char *directory);
pg_status pg_resource_ignore_test(pg_resource_ignore *, const char *path,
                                  int *ignored);
void pg_resource_ignore_destroy(pg_resource_ignore *);
#endif
