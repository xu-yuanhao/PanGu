#ifndef PG_MIMO_H
#define PG_MIMO_H
#include "agent.h"
#include "tools.h"
#include <curl/curl.h>
typedef struct {
 CURL *easy; CURLM *multi; struct curl_slist *headers;
 char body[131072], text[PG_TEXT_LIMIT], arguments[PG_TEXT_LIMIT], id[PG_ID_LIMIT], name[PG_ID_LIMIT];
 size_t used, consumed; int delivered, failed, transport_done, finish_reason, saw_done; long http;
 char *request;
 const pg_tool_registry *tools;
} pg_mimo;
pg_model pg_mimo_model(pg_mimo *);
/* Bind the same immutable registry used by the executor before starting runs. */
void pg_mimo_bind_tools(pg_mimo *, const pg_tool_registry *);
/* Internal projection test seam: creates owned request; no network/credentials. */
int pg_mimo_encode_request(pg_mimo *, const pg_session *);
/* Internal decoder: one complete OpenAI SSE data line, no network operations. */
int pg_mimo_decode_line(pg_mimo *, const char *, pg_model_event *);
#endif
