#ifndef PG_MIMO_H
#define PG_MIMO_H
#include "agent.h"
#include "tools.h"
#include <curl/curl.h>
struct json_object;
typedef struct { char arguments[PG_TEXT_LIMIT],id[PG_ID_LIMIT],name[PG_ID_LIMIT]; } pg_mimo_call_storage;
typedef struct {
 CURL *easy; CURLM *multi; struct curl_slist *headers;
 char body[131072], text[PG_TEXT_LIMIT], arguments[PG_TEXT_LIMIT], id[PG_ID_LIMIT], name[PG_ID_LIMIT];
 size_t used, consumed; int delivered, failed, transport_done, finish_reason, saw_done; long http;
 char *request;
 const pg_tool_registry *tools;
 pg_mimo_call_storage extra[PG_TOOL_BATCH_LIMIT-1];
 pg_model_call calls[PG_TOOL_BATCH_LIMIT];
 unsigned call_count,transport_error;
 struct json_object *usage; /* owned final provider usage snapshot */
 struct json_object *configuration; /* private owned request identity/options */
} pg_mimo;
/* OpenAI-compatible SSE transport. Explicit PANGU_MODEL_ENDPOINT,
   PANGU_MODEL_NAME and PANGU_MODEL_API_KEY select a connection; custom endpoint
   requires its explicit key. Default MiMo connection reads MIMO_API_KEY/MODEL.
   Environment stays immutable throughout active requests. Initialize
   fresh/released storage only; close before discarding an instance. */
pg_model pg_mimo_model(pg_mimo *);
/* Bind a detached credential-free {model,endpoint,options?} while idle.
   options: maxTokens, temperature, maxTokensField (max_tokens or
   max_completion_tokens). 0 success; failure keeps previous binding. Public
   close/cancel releases it; start's transport reset preserves it. */
int pg_mimo_configure(pg_mimo *,const struct json_object *);
/* Complete a private assistant JSON before close with stopReason and reported
   token usage (input/output/cacheRead/cacheWrite/reasoning/totalTokens).
   Absent provider usage stays absent; this API reports token counts only. */
int pg_mimo_complete(const pg_mimo *,struct json_object *assistant);
/* Bind the same immutable registry used by the executor before starting runs. */
void pg_mimo_bind_tools(pg_mimo *, const pg_tool_registry *);
/* Internal projection test seam: creates owned request; no network/credentials. */
int pg_mimo_encode_request(pg_mimo *, const pg_session *);
/* Internal decoder: one complete OpenAI SSE data line, no network operations. */
int pg_mimo_decode_line(pg_mimo *, const char *, pg_model_event *);
#endif
