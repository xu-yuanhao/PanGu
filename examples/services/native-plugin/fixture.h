#ifndef PANGU_EXAMPLE_NATIVE_FIXTURE_H
#define PANGU_EXAMPLE_NATIVE_FIXTURE_H
#include <pangu/services/tools.h>
#define PG_NATIVE_FIXTURE_SERVICE "example.native.tools"
typedef struct {
  pangu_validate_fn validate;
  void *validator_ctx;
  int *cleaned;
  int fail_after_registration;
} pg_native_fixture_config;
#endif
