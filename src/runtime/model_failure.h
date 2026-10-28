#ifndef PANGU_MODEL_FAILURE_H
#define PANGU_MODEL_FAILURE_H
#include "../contracts/client.h"
#include <stdint.h>
typedef struct {
 /* Borrowed UTF-8 diagnostic, valid until provider close/next. */
 pg_slice message;
 unsigned retryable,http_status,transport_error,reserved;
} pg_model_failure;
/* Error-based context/request-size overflow; throttling takes precedence. */
int pg_model_context_overflow(pg_slice,long http_status);
/* Pi retry.ts classification, with native HTTP/CURL facts. Billing/quota
   exclusions precede transient classification. Text has explicit length. */
int pg_model_retryable(pg_slice,long http_status,unsigned transport_error);
#endif
