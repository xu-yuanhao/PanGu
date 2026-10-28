#ifndef PANGU_MODEL_FAILURE_H
#define PANGU_MODEL_FAILURE_H
#include "../contracts/client.h"
#include <stdint.h>
typedef struct {
 /* Borrowed UTF-8 diagnostic, valid until provider close/next. */
 pg_slice message;
 unsigned retryable,http_status,transport_error,flags;
} pg_model_failure;
/* Optional flags preserve classification from a private provider response body. */
enum {PG_MODEL_FAILURE_CONTEXT_CHECKED=1,PG_MODEL_FAILURE_CONTEXT_OVERFLOW=2};
/* Error-based context/request-size overflow; throttling takes precedence. */
int pg_model_context_overflow(pg_slice,long http_status);
/* Pi retry.ts classification, with native HTTP/CURL facts. Billing/quota
   exclusions precede transient classification. Text has explicit length. */
int pg_model_retryable(pg_slice,long http_status,unsigned transport_error);
#endif
