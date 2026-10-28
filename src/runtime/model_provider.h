#ifndef PANGU_MODEL_PROVIDER_H
#define PANGU_MODEL_PROVIDER_H
#include "tools.h"
#include "model_failure.h"
struct json_object;
/* Service model.provider; borrowed through all requests and pending cleanup.
   configure honors the committed public configuration and current tool catalog.
   The host serializes one lease; close ends a request before provider stop.
   identity is a stable nonempty versioned name, saved with each request.
   ABI3 accepts PG_USER_VIEW request rows and borrows their explicit-length text
   until close; the producing request owner retains storage through release. */
typedef struct {
 uint32_t abi,size;
 pg_model model;
 int (*configure)(void *,const struct json_object *,const pg_tool_registry *);
 void (*failure)(void *,pg_model_failure *);
 int (*complete)(void *,struct json_object *assistant);
 const char *identity;
 /* Optional deferred operation: start fetching into the existing model event
    stream, using a borrowed persisted handle. 0 started, other values error.
    cancel_deferred returns 0 finished, 1 pending, negative released failure;
    it must be repeatable after process interruption. No credentials in handles. */
 int (*fetch_deferred)(void *,const struct json_object *handle);
 int (*cancel_deferred)(void *,const struct json_object *handle);
} pg_model_provider;
#define PG_MODEL_PROVIDER_ABI 3u
#endif
