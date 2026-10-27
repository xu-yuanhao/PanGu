#ifndef PANGU_MODEL_PROVIDER_H
#define PANGU_MODEL_PROVIDER_H
#include "tools.h"
#include "model_failure.h"
struct json_object;
/* Service model.provider; borrowed through all requests and pending cleanup.
   configure honors the committed public configuration and current tool catalog.
   The host serializes one lease; close ends a request before provider stop.
   identity is a stable nonempty versioned name, saved with each request. */
typedef struct {
 uint32_t abi,size;
 pg_model model;
 int (*configure)(void *,const struct json_object *,const pg_tool_registry *);
 void (*failure)(void *,pg_model_failure *);
 int (*complete)(void *,struct json_object *assistant);
 const char *identity;
} pg_model_provider;
#define PG_MODEL_PROVIDER_ABI 1u
#endif
