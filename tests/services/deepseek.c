#include <curl/curl.h>
#ifdef PANGU_TEST_OVERFLOW
#include <pangu/core/overflow.h>
#endif
#include <pangu/core/state.h>
#include <pangu/services/model.h>
#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "failed %d: %s\n", __LINE__, #x);                        \
      abort();                                                                 \
    }                                                                          \
  } while (0)
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
typedef struct {
  int keys, payloads, responses, chunks, text, thinking, tool, executed, cancel,
      deny, polls, replace, empty, prepares;
  const pangu_tool *replacement;
} fixture;
static pangu_status key(void *ctx, pangu_core *c, pangu_slice provider,
                        pangu_api_key_sink sink, void *s) {
  (void)c;
  (void)provider;
  fixture *f = ctx;
  f->keys++;
  return sink(s, getenv("PANGU_LIVE_KEY")
                     ? (pangu_slice){getenv("PANGU_LIVE_KEY"),
                                     strlen(getenv("PANGU_LIVE_KEY"))}
                     : (f->keys == 1 ? SL("fixture-one") : SL("fixture-two")));
}
static pangu_status payload_hook(void *ctx, pangu_core *c,
                                 const pangu_model_config *m, pangu_slice json,
                                 pangu_payload_sink sink, void *s) {
  (void)c;
  (void)m;
  fixture *f = ctx;
  f->payloads++;
  yyjson_doc *d = yyjson_read(json.data, json.len, 0);
  CHECK(d);
  yyjson_mut_doc *o = yyjson_doc_mut_copy(d, NULL);
  CHECK(o);
  CHECK(yyjson_mut_obj_add_bool(o, yyjson_mut_doc_get_root(o), "hooked", true));
  if (getenv("PANGU_LIVE_KEY")) {
    yyjson_mut_val *root = yyjson_mut_doc_get_root(o);
    CHECK(yyjson_mut_obj_remove_key(root, "hooked"));
    CHECK(yyjson_mut_obj_add_str(o, root, "tool_choice",
                                 f->payloads == 1 ? "required" : "none"));
    CHECK(yyjson_mut_obj_add_int(o, root, "max_tokens", 256));
  }
  size_t n;
  char *p = yyjson_mut_write(o, 0, &n);
  CHECK(p);
  pangu_status st = sink(s, (pangu_slice){p, n});
  free(p);
  yyjson_mut_doc_free(o);
  yyjson_doc_free(d);
  return st;
}
static pangu_status response(void *ctx, pangu_core *c,
                             const pangu_model_config *m,
                             const pangu_provider_response *r) {
  (void)c;
  (void)m;
  fixture *f = ctx;
  CHECK(r->status >= 200);
  CHECK(r->header_count > 0);
  f->responses++;
  return f->deny ? PANGU_CANCELLED : PANGU_OK;
}
static pangu_status chunk(void *ctx, pangu_core *c, const pangu_model_config *m,
                          pangu_slice json) {
  (void)c;
  (void)m;
  (void)json;
  ((fixture *)ctx)->chunks++;
  return PANGU_OK;
}
static void event(void *ctx, pangu_core *c, const pangu_event *e) {
  fixture *f = ctx;
  if (e->assistant_event) {
    switch (e->assistant_event->kind) {
    case PANGU_ASSISTANT_TEXT_DELTA:
      f->text++;
      if (f->cancel)
        pangu_core_cancel(c);
      break;
    case PANGU_ASSISTANT_THINKING_DELTA:
      f->thinking++;
      break;
    case PANGU_ASSISTANT_TOOL_DELTA:
      f->tool++;
      break;
    default:
      break;
    }
  }
}
static pangu_status execute(void *ctx, pangu_core *c, const pangu_content *call,
                            pangu_tool_sink sink, void *s) {
  (void)c;
  (void)call;
  ((fixture *)ctx)->executed++;
  pangu_content b = {.kind = PANGU_TEXT, .data = SL("pong")};
  pangu_message m = {
      .role = PANGU_TOOL_RESULT, .content = &b, .content_count = 1};
  return sink(s, &m, 1);
}
static pangu_status poll(void *ctx, pangu_core *c) {
  (void)c;
  ((fixture *)ctx)->polls++;
  return PANGU_OK;
}
static pangu_status prepare(void *ctx, pangu_core *owner,
                            pangu_request_update_sink sink, void *sink_ctx) {
  (void)owner;
  fixture *f = ctx;
  ++f->prepares;
  pangu_request_update update = {.fields = PANGU_UPDATE_TOOLS};
  if (f->replace && f->prepares == 1) {
    update.tools = f->replacement;
    update.tool_count = 1;
  }
  return sink(sink_ctx, &update);
}
int main(int argc, char **argv) {
  CHECK(argc == 3);
  CHECK(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
  fixture f = {0};
  f.cancel = !strcmp(argv[2], "cancel") || !strcmp(argv[2], "metadata-cancel");
  f.deny = !strcmp(argv[2], "deny");
  f.replace = !strcmp(argv[2], "replace");
  f.empty = !strcmp(argv[2], "empty");
  pangu_tool tool = {.name = SL("echo"),
                     .description = SL("Echo fixture"),
                     .schema_json =
                         SL("{\"type\":\"object\",\"properties\":{\"value\":{"
                            "\"type\":\"string\"}},\"required\":[\"value\"]}"),
                     .ctx = &f,
                     .validate = pg_schema_validate,
                     .execute = execute};
  pangu_tool replacement = tool;
  replacement.name = SL("runtime_echo");
  f.replacement = &replacement;
  pg_deepseek_config model = {.endpoint = {argv[1], strlen(argv[1])},
                              .tools = &tool,
                              .tool_count = 1,
                              .timeout_ms =
                                  getenv("PANGU_LIVE_KEY") ? 60000 : 5000,
                              .poll = poll,
                              .poll_ctx = &f};
  pg_host *h = NULL;
  pg_scope *plugin = NULL;
  pg_lease *lease = NULL;
  CHECK(pg_host_create(&h) == PG_OK);
  CHECK(pg_plugin_install(pg_host_root(h), &pg_deepseek_plugin, &model,
                          sizeof(model), &plugin) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_MODEL_SERVICE), 1,
                           &lease) == PG_OK);
  pangu_core *c = NULL;
  CHECK(pangu_core_create(NULL, &c) == PANGU_OK);
  pangu_loop_config cfg = {
      .ctx = &f,
      .request_events =
          ((const pg_model_service *)pg_lease_table(lease))->request_events,
      .has_request_ctx = 1,
      .request_ctx = pg_lease_context(lease),
      .model_config = {.id = SL("deepseek-chat"),
                       .provider = SL("deepseek"),
                       .api = SL("openai-completions")},
      .prepare_request_update = (f.replace || f.empty) ? prepare : NULL,
      .get_api_key = key,
      .on_payload = payload_hook,
      .on_response = response,
      .on_provider_event = chunk,
      .event = event,
      .tools = &tool,
      .tool_count = 1,
      .thinking_level =
          getenv("PANGU_LIVE_KEY") ? PANGU_THINKING_OFF : PANGU_THINKING_HIGH};
  pangu_content b = {
      .kind = PANGU_TEXT,
      .data =
          SL("Call echo exactly once with value ping, then reply with pong.")};
  pangu_message prompt = {
      .role = PANGU_USER, .content = &b, .content_count = 1};
  pangu_status st = pangu_core_run(c, &prompt, 1, &cfg);
  if (!strcmp(argv[2], "live")) {
    CHECK(st == PANGU_OK);
    CHECK(f.keys == 2 && f.responses == 2 && f.executed == 1 && f.text &&
          f.tool);
    puts("live DeepSeek: 2 requests, tool roundtrip and text stream passed; "
         "content redacted");
  } else if (!strncmp(argv[2],"metadata",8)) {
    int cancelled=!strcmp(argv[2],"metadata-cancel");
    int failed=!strcmp(argv[2],"metadata-error");
    int defaults=!strcmp(argv[2],"metadata-default");
    CHECK(st==(cancelled?PANGU_CANCELLED:failed?PANGU_MODEL_ERROR:PANGU_OK));
    CHECK(f.responses==1 && f.executed==0);
    const pangu_message *last=pangu_core_message_at(c,pangu_core_message_count(c)-1);
    CHECK(last && last->role==PANGU_ASSISTANT);
    CHECK(last->model.len==sizeof("deepseek-chat")-1 && !memcmp(last->model.data,"deepseek-chat",last->model.len));
    CHECK(last->stop_reason==(cancelled?PANGU_ABORTED:failed?PANGU_ERROR:defaults?PANGU_STOP:PANGU_LENGTH));
    if(defaults) { CHECK(last->response_id.len==0 && last->response_model.len==0); }
    else {
      pangu_slice id=SL("response-首"),model=SL("served-模型");
      CHECK(last->response_id.len==id.len&&!memcmp(last->response_id.data,id.data,id.len));
      CHECK(last->response_model.len==model.len&&!memcmp(last->response_model.data,model.data,model.len));
    }
    pangu_slice native=cancelled?(pangu_slice){0}:failed?SL("content_filter"):defaults?SL("stop"):SL("length");
    CHECK(last->raw_stop_reason.len==native.len && (!native.len||!memcmp(last->raw_stop_reason.data,native.data,native.len)));
  } else if (f.replace || f.empty) {
    CHECK(st == PANGU_OK);
    CHECK(f.prepares == (f.replace ? 2 : 1));
    CHECK(f.keys == f.prepares && f.responses == f.prepares);
    CHECK(f.executed == (f.replace ? 1 : 0));
  } else if (!strcmp(argv[2], "ok")) {
    CHECK(st == PANGU_OK);
    CHECK(f.keys == 2 && f.payloads == 2 && f.responses == 2 &&
          f.executed == 1);
    CHECK(f.text && f.thinking && f.tool && f.chunks && f.polls);
    const pangu_message *last =
        pangu_core_message_at(c, pangu_core_message_count(c) - 1);
    CHECK(last->stop_reason == PANGU_STOP);
    CHECK(!last->response_id.len && !last->response_model.len);
    CHECK(last->raw_stop_reason.len==4&&!memcmp(last->raw_stop_reason.data,"stop",4));
    CHECK(last->usage.total == 12 && last->usage.input == 7 &&
          last->usage.cache_read == 3);
    CHECK(last->usage.has_reasoning && last->usage.reasoning == 0);
    CHECK(last->usage.has_total_from_provider &&
          last->usage.total_from_provider);
  } else if (!strcmp(argv[2], "too-large") || !strcmp(argv[2], "bad-request")) {
    int large = !strcmp(argv[2], "too-large");
    CHECK(st == PANGU_MODEL_ERROR);
    CHECK(!pangu_core_is_cancelled(c));
    CHECK(f.responses == 1 && f.executed == 0 && f.chunks == 0);
    const pangu_message *last = pangu_core_message_at(c,pangu_core_message_count(c)-1);
    CHECK(last && last->role == PANGU_ASSISTANT && last->stop_reason == PANGU_ERROR);
    pangu_slice expected = large ? SL("deepseek API error (413): request too large")
                                : SL("DeepSeek request failed (HTTP 400)");
    CHECK(last->error.len == expected.len && !memcmp(last->error.data,expected.data,expected.len));
#ifdef PANGU_TEST_OVERFLOW
    pangu_overflow_classifier *classifier = NULL;
    CHECK(pangu_overflow_create(NULL,&classifier) == PANGU_OK);
    int overflow = -1;
    CHECK(pangu_message_is_context_overflow(classifier,last,0,&overflow) == PANGU_OK);
    CHECK(overflow == large);
    pangu_overflow_destroy(classifier);
#endif
  } else {
    CHECK(st != PANGU_OK || pangu_core_error(c).len);
    CHECK(f.responses == 1);
    if (f.deny)
      CHECK(f.chunks == 0 && f.executed == 0);
  }
  pangu_core_destroy(c);
  pg_lease_release(lease);
  pg_host_destroy(h);
  curl_global_cleanup();
  puts("direct model fixture passed (no credentials logged)");
  return 0;
}
