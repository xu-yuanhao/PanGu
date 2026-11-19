#ifndef PG_AGENT_TEST_NATIVE_H
#define PG_AGENT_TEST_NATIVE_H
typedef struct {
  void (*on_request)(void *);
  void *ctx;
  int *cleaned;
  int fail;
} agent_native_config;
#endif
