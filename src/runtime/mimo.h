#ifndef PG_MIMO_H
#define PG_MIMO_H
#include "agent.h"
#include "tools.h"
#include <curl/curl.h>
struct json_object;
typedef struct { char arguments[PG_TEXT_LIMIT],id[PG_ID_LIMIT],name[PG_ID_LIMIT]; } pg_mimo_call_storage;
struct printbuf;
typedef struct {uint64_t index;unsigned kind,closed;struct printbuf *text,*signature;struct json_object *item;} pg_mimo_anthropic_block;
typedef struct {
 CURL *easy; CURLM *multi; struct curl_slist *headers;
 char body[131072], text[PG_TEXT_LIMIT], arguments[PG_TEXT_LIMIT], id[PG_ID_LIMIT], name[PG_ID_LIMIT];
 size_t used, consumed; int delivered, failed, transport_done, finish_reason, saw_done; long http;
 char *request;
 const pg_tool_registry *tools;
 pg_mimo_call_storage extra[PG_TOOL_BATCH_LIMIT-1];
 pg_model_call calls[PG_TOOL_BATCH_LIMIT];
 unsigned call_count,transport_error;
 char reasoning[PG_TEXT_LIMIT];
 struct json_object *usage; /* owned final provider usage snapshot */
 struct json_object *configuration; /* private owned request identity/options */
 /* Provider owns overflow; call slices borrow it until decode/start/close. */
 char *argument_overflow[PG_TOOL_BATCH_LIMIT];
 size_t argument_length[PG_TOOL_BATCH_LIMIT],argument_capacity[PG_TOOL_BATCH_LIMIT];
 uint64_t response_indices[PG_TOOL_BATCH_LIMIT]; /* output_index to dense call slot */
 struct json_object *response_output,*response_identity; /* owned final item slots/id */
 struct json_object *response_terminal; /* owned terminal envelope */
 unsigned response_closed; /* completed function-call slots */
 struct json_object *prepared_arguments[PG_TOOL_BATCH_LIMIT];
 pg_mimo_anthropic_block *anthropic_blocks;size_t anthropic_count,anthropic_capacity;
 struct json_object *anthropic_usage;
 unsigned strict_retry;struct json_object *strict_diagnostics;
 struct json_object *google_text; /* owned event slice, borrowed until next Google decode/close */
 char response_request_id[201]; /* public header value, reset at each HTTP response/start */
 void *wire_request;size_t wire_size; /* owned until curl cleanup; request remains plain JSON */
 uint64_t retry_attempt,retry_at; /* request-local monotonic milliseconds; zero means active */
 char retry_after[128],retry_after_ms[128]; /* bounded public response delay headers */
 unsigned retry_blocked; /* provider terminal policy prevents an outer retry */
 struct printbuf *codex_frame; /* owned pending SSE data frame */
} pg_mimo;
/* OpenAI Chat Completions / Responses Anthropic Messages and Google/Vertex SSE transport. Explicit PANGU_MODEL_ENDPOINT,
   PANGU_MODEL_NAME and PANGU_MODEL_API_KEY select a connection; custom endpoint
   requires its explicit key. Default MiMo connection reads MIMO_API_KEY/MODEL.
   Environment stays immutable throughout active requests. Initialize
   fresh/released storage only; close before discarding an instance. */
pg_model pg_mimo_model(pg_mimo *);
/* Bind a detached credential-free {model,endpoint,options?} while idle.
   options: maxTokens, temperature, reasoningEffort (DeepSeek thinking toggle),
   maxTokensField (max_tokens or
   max_completion_tokens). 0 success; failure keeps previous binding. Public
   close/cancel releases it; start's transport reset preserves it. */
int pg_mimo_configure(pg_mimo *,const struct json_object *);
/* Complete a private assistant JSON before close with stopReason and reported
   token usage (input/output/cacheRead/cacheWrite/reasoning/totalTokens).
   Absent provider usage stays absent; public prices compute optional cost.
   Error assistants receive available provider diagnostics before close.
   Responses consumers complete before tool dispatch to retain final
   argumentsError metadata alongside the prepared argument object. */
int pg_mimo_complete(const pg_mimo *,struct json_object *assistant);
/* Bind the same immutable registry used by the executor before starting runs. */
void pg_mimo_bind_tools(pg_mimo *, const pg_tool_registry *);
/* Internal projection test seam: creates owned request; no network/credentials. */
int pg_mimo_encode_request(pg_mimo *, const pg_session *);
int pg_mimo_uses_responses(const pg_mimo *);
int pg_mimo_config_responses(const struct json_object *);
int pg_mimo_config_codex(const struct json_object *);
int pg_mimo_codex_line(pg_mimo *,char *,pg_model_event *);
int pg_mimo_config_anthropic(const struct json_object *);
int pg_mimo_uses_anthropic(const pg_mimo *);
int pg_mimo_uses_google(const pg_mimo *);
/* Internal byte codec: decode returns 0/1 invalid/2 allocation failure; caller frees bytes.
   Encode returns an owned JSON string, or NULL. */
int pg_mimo_base64_decode(const struct json_object *,unsigned char **,size_t *);
struct json_object *pg_mimo_base64_encode(const void *,size_t);
int pg_mimo_config_bedrock(const struct json_object *);
int pg_mimo_config_google(const struct json_object *);
int pg_google_signature_valid(const struct json_object *);
int pg_google_requires_id(const char *);
void pg_mimo_anthropic_clear(pg_mimo *);
int pg_mimo_anthropic_strict_retry(pg_mimo *);
/* Internal protocol-selected final-item/canonical replay adapters; history returns an owned array.
   Native call_id and provider responseItemId remain separate identities. */
int pg_mimo_responses_complete(const pg_mimo *,struct json_object *);
int pg_mimo_responses_history(const struct json_object *,const struct json_object *,struct json_object **);
/* Borrowed structured failure text/body, valid through close or the next start. */
int pg_mimo_failure_data(const pg_mimo *,pg_slice *,pg_slice *);
/* Owned {arguments, argumentsError?}; raw input stays borrowed. */
int pg_mimo_arguments_parse(pg_slice,struct json_object **);
int pg_mimo_prepare_arguments(pg_mimo *,unsigned);
/* Internal decoder: one complete SSE line or complete Bedrock binary frame;
   the transport checks available frame length first. api/modelInfo.api selects protocol,
   with a /responses endpoint fallback. No network operations. */
int pg_mimo_decode_line(pg_mimo *, const char *, pg_model_event *);
#endif
