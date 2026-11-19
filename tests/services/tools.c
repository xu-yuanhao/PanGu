#include <pangu/services/schema.h>
#include <pangu/services/tools.h>
#include <pangu/services/images.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>
#include <uv.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "failed %d: %s\n", __LINE__, #x);                        \
      abort();                                                                 \
    }                                                                          \
  } while (0)
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
static pangu_status model(void *ctx, pangu_core *owner, pangu_model_sink sink,
                          void *sink_ctx) {
  int *step = ctx;
  pangu_content block = {0};
  pangu_message msg = {
      .role = PANGU_ASSISTANT, .content = &block, .content_count = 1};
  if ((*step)++ == 0) {
    block = (pangu_content){
        .kind = PANGU_TOOL_CALL,
        .id = SL("write-1"),
        .name = SL("write"),
        .data = SL("{\"path\":\"目录 with "
                   "space/a.txt\",\"content\":\"你好\\nPanGu\"}")};
    msg.stop_reason = PANGU_TOOL_USE;
  } else if (*step == 2) {
    block = (pangu_content){.kind = PANGU_TOOL_CALL,
      .id = SL("edit-1"), .name = SL("edit"),
      .data = SL("{\"path\":\"目录 with space/a.txt\",\"edits\":[{\"oldText\":\"PanGu\",\"newText\":\"Core\"},{\"oldText\":\"你好\",\"newText\":\"您好\"}]}")};
    msg.stop_reason = PANGU_TOOL_USE;
  } else if (*step == 3) {
    const pangu_message *last = pangu_core_message_at(owner, pangu_core_message_count(owner)-1);
    CHECK(last->details_json.len > 0);

    block = (pangu_content){.kind = PANGU_TOOL_CALL,
                            .id = SL("read-1"),
                            .name = SL("read"),
                            .data = SL("{\"path\":\"目录 with space/a.txt\",\"offset\":\"1\",\"limit\":null}")};
    msg.stop_reason = PANGU_TOOL_USE;
  } else {
    const pangu_message *last =
        pangu_core_message_at(owner, pangu_core_message_count(owner) - 1);
    CHECK(last->role == PANGU_TOOL_RESULT);
#ifdef PANGU_TEST_IMAGES
    if (*step == 4) {
#endif
    CHECK(last->content[0].data.len == strlen("您好\nCore"));
    CHECK(!memcmp(last->content[0].data.data, "您好\nCore", last->content[0].data.len));
#ifdef PANGU_TEST_IMAGES
      block = (pangu_content){.kind = PANGU_TOOL_CALL, .id = SL("image-1"),
        .name = SL("read"), .data = SL("{\"path\":\"image.png\"}")};
      msg.stop_reason = PANGU_TOOL_USE;
      return sink(sink_ctx, &msg, 1);
    }
    CHECK(last->content[0].kind == PANGU_IMAGE);
    CHECK(last->content[0].data.len > 8);
#endif
    block = (pangu_content){.kind = PANGU_TEXT, .data = SL("done")};
    msg.stop_reason = PANGU_STOP;
  }
  return sink(sink_ctx, &msg, 1);
}
static pangu_status capture(void *ctx, const pangu_message *m, int final) {
  (void)final;
  const char *expected = ctx;
  CHECK(m->content_count == 1);
  if (strstr(expected, "[Showing lines") || strstr(expected, "exceeds 50.0KB")) {
    yyjson_doc *d = yyjson_read(m->details_json.data, m->details_json.len, 0);
    CHECK(d);
    CHECK(yyjson_is_obj(yyjson_obj_get(yyjson_doc_get_root(d), "truncation")));
    yyjson_doc_free(d);
  }
  CHECK(m->content[0].data.len == strlen(expected));
  CHECK(!memcmp(m->content[0].data.data, expected, strlen(expected)));
  return PANGU_OK;
}
#ifdef PANGU_TEST_IMAGES
static pangu_status image_capture(void *ctx, const pangu_message *m, int final) {
  (void)ctx; (void)final;
  CHECK(m->content_count == 1 && m->content[0].kind == PANGU_IMAGE);
  CHECK(m->content[0].mime_type.len == 9);
  CHECK(!memcmp(m->content[0].mime_type.data, "image/png", 9));
  CHECK(m->content[0].data.len > 8 && !memcmp(m->content[0].data.data, "iVBORw0K", 8));
  return PANGU_OK;
}
#endif
static pangu_status invoke(const pg_tools_service *tools, pangu_core *core,
                           const char *name, const char *json, const char *expected) {
  for (size_t i = 0; i < tools->count; ++i) {
    const pangu_tool *t = &tools->tools[i];
    if (t->name.len != strlen(name) || memcmp(t->name.data, name, strlen(name))) continue;
    pangu_content call = {.kind = PANGU_TOOL_CALL, .name = {name, strlen(name)}, .data = {json, strlen(json)}};
    pangu_status st = t->validate(t->ctx, t->schema_json, call.data);
    if (st) return st;
#ifdef PANGU_TEST_IMAGES
    if (!expected) return t->execute(t->ctx, core, &call, image_capture, NULL);
#endif
    return t->execute(t->ctx, core, &call, capture, (void *)expected);
  }
  return PANGU_INVALID;
}
static int has(pangu_slice text,const char *needle) {
  size_t n=strlen(needle);
  for(size_t i=0;n<=text.len && i<=text.len-n;++i) if(!memcmp(text.data+i,needle,n)) return 1;
  return 0;
}
typedef struct { const pg_tools_service *tools; const char *json; } thread_edit;
static void parallel_edit(void *ctx) {
  thread_edit *work = ctx;
  pangu_core *c = NULL;
  CHECK(pangu_core_create(NULL, &c) == PANGU_OK);
  CHECK(invoke(work->tools, c, "edit", work->json, "Successfully replaced blocks") == PANGU_OK);
  pangu_core_destroy(c);
}
int main(int argc, char **argv) {
  CHECK(argc == 2);
  pg_host *h = NULL;
  pg_scope *s = NULL;
  pg_lease *l = NULL;
  CHECK(pg_host_create(&h) == PG_OK);
#ifdef PANGU_TEST_IMAGES
  pg_scope *images = NULL;
  CHECK(pg_plugin_install(pg_host_root(h), &pg_images_plugin, NULL, 0, &images) == PG_OK);
#endif
  pg_file_tools_config cfg = {.environment = pg_local_file_environment(argv[1]),
                              .validate = pg_schema_validate};
  CHECK(pg_plugin_install(pg_host_root(h), &pg_file_tools_plugin, &cfg,
                          sizeof(cfg), &s) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_TOOLS_SERVICE), 1, &l) ==
        PG_OK);
  const pg_tools_service *tools = pg_lease_table(l);
  pangu_core *c = NULL;
  CHECK(pangu_core_create(NULL, &c) == PANGU_OK);
  int step = 0;
  pangu_loop_config loop = {.ctx = &step,
                            .model = model,
                            .tools = tools->tools,
                            .tool_count = tools->count};
  pangu_content b = {.kind = PANGU_TEXT, .data = SL("write then read")};
  pangu_message p = {.role = PANGU_USER, .content = &b, .content_count = 1};
  CHECK(pangu_core_run(c, &p, 1, &loop) == PANGU_OK);
#ifdef PANGU_TEST_IMAGES
  CHECK(step == 5);
#else
  CHECK(step == 4);
#endif
  /* Core uses cloned/coerced args; transcript retains original model bytes. */
  int found_raw=0;
  for (size_t i=0;i<pangu_core_message_count(c);++i) {
    const pangu_message *m=pangu_core_message_at(c,i);
    for (size_t j=0;j<m->content_count;++j) if (m->content[j].kind==PANGU_TOOL_CALL && m->content[j].id.len==6 && !memcmp(m->content[j].id.data,"read-1",6)) {
      yyjson_doc *raw=yyjson_read(m->content[j].data.data,m->content[j].data.len,0); CHECK(raw);
      CHECK(yyjson_equals_str(yyjson_obj_get(yyjson_doc_get_root(raw),"offset"),"1"));
      CHECK(yyjson_is_null(yyjson_obj_get(yyjson_doc_get_root(raw),"limit"))); yyjson_doc_free(raw); found_raw=1;
    }
  }
  CHECK(found_raw);
  /* Failed validation must not reach write. Existing file stays unchanged. */
  pangu_content bad={.kind=PANGU_TOOL_CALL,.id=SL("bad-schema"),.name=SL("write"),
    .data=SL("{\"path\":\"目录 with space/a.txt\",\"content\":{},\"extra\":true}")};
  pangu_message assistant={.role=PANGU_ASSISTANT,.content=&bad,.content_count=1,.stop_reason=PANGU_TOOL_USE};
  pangu_tool_outcome outcome={0};
  CHECK(pangu_core_call_tool(c,&assistant,&bad,&loop,NULL,NULL,&outcome)==PANGU_OK);
  CHECK(outcome.result && pangu_core_message_at(outcome.result,0)->is_error); pangu_core_destroy(outcome.result);
  /* Production owner-aware validation: no explicit owner/name binding. */
  CHECK(tools->tools[1].validate_call==pg_schema_normalize_tool);
  outcome=(pangu_tool_outcome){0};
  CHECK(pangu_core_call_tool(c,&assistant,&bad,&loop,NULL,NULL,&outcome)==PANGU_OK);
  const pangu_message *error=pangu_core_message_at(outcome.result,0);
  CHECK(error && error->is_error);
  CHECK(has(error->content[0].data,"Validation failed for tool \"write\":") && has(error->content[0].data,"content:") && has(error->content[0].data,"Received arguments:") && has(error->content[0].data,"\"extra\": true"));
  pangu_core_destroy(outcome.result);
  /* Default production validator is selected automatically, with no custom ctx. */
  pangu_content numeric={.kind=PANGU_TOOL_CALL,.id=SL("numeric"),.name=SL("write"),.data=SL("{\"path\":\"normalized.txt\",\"content\":42}")};
  assistant.content=&numeric;
  outcome=(pangu_tool_outcome){0};
  CHECK(pangu_core_call_tool(c,&assistant,&numeric,&loop,NULL,NULL,&outcome)==PANGU_OK);
  CHECK(outcome.result && !pangu_core_message_at(outcome.result,0)->is_error);
  pangu_core_destroy(outcome.result);
  CHECK(invoke(tools,c,"read","{\"path\":\"normalized.txt\"}","42")==PANGU_OK);
  CHECK(invoke(tools, c, "read", "{\"path\":\"目录 with space/a.txt\",\"offset\":2}", "Core") == PANGU_OK);
  CHECK(invoke(tools, c, "read", "{\"path\":\"目录 with space/a.txt\",\"limit\":1}", "您好\n\n[1 more lines in file. Use offset=2 to continue.]") == PANGU_OK);
  CHECK(invoke(tools, c, "read", "{\"path\":\"目录 with space/a.txt\",\"offset\":3}", "") == PANGU_TOOL_ERROR);
  CHECK(invoke(tools, c, "edit", "{\"path\":\"目录 with space/a.txt\",\"edits\":[{\"oldText\":\"Core\",\"newText\":\"x\"},{\"oldText\":\"ore\",\"newText\":\"y\"}]}", "") == PANGU_TOOL_ERROR);
  CHECK(invoke(tools, c, "read", "{\"path\":\"目录 with space/a.txt\"}", "您好\nCore") == PANGU_OK);
  CHECK(invoke(tools, c, "edit", "{\"path\":\"目录 with space/a.txt\",\"edits\":[{\"oldText\":\"Core\",\"newText\":\"Core\"}]}", "") == PANGU_TOOL_ERROR);
  CHECK(invoke(tools, c, "read", "{\"path\":\"long.txt\"}", "[Line 1 exceeds 50.0KB limit. Use bash to read this line.]") == PANGU_OK);
  char *many = malloc(11000);
  CHECK(many);
  size_t at = 0;
  for (size_t i = 0; i < 2000; ++i) {
    memcpy(many+at, "line", 4); at += 4;
    if (i != 1999) many[at++] = '\n';
  }
  strcpy(many+at, "\n\n[Showing lines 1-2000 of 2002. Use offset=2001 to continue.]");
  CHECK(invoke(tools, c, "read", "{\"path\":\"many.txt\"}", many) == PANGU_OK);
  free(many);
  CHECK(invoke(tools, c, "edit", "{\"path\":\"duplicate.txt\",\"edits\":[{\"oldText\":\"alpha\",\"newText\":\"x\"}]}", "") == PANGU_TOOL_ERROR);
  CHECK(invoke(tools, c, "read", "{\"path\":\"duplicate.txt\"}", "alpha alpha") == PANGU_OK);
  CHECK(invoke(tools, c, "edit", "{\"path\":\"crlf.txt\",\"edits\":[{\"oldText\":\"alpha\\nbeta\",\"newText\":\"ALPHA\\nBETA\"}]}", "Successfully replaced blocks") == PANGU_OK);
  CHECK(invoke(tools, c, "read", "{\"path\":\"crlf.txt\"}", "\xef\xbb\xbf" "ALPHA\r\nBETA\r\n") == PANGU_OK);
#ifdef PANGU_TEST_IMAGES
  CHECK(invoke(tools, c, "read", "{\"path\":\"image.png\"}", NULL) == PANGU_OK);
  CHECK(invoke(tools, c, "read", "{\"path\":\"broken.png\"}", NULL) == PANGU_TOOL_ERROR);
#endif
  thread_edit work[2] = {
    {tools, "{\"path\":\"parallel.txt\",\"edits\":[{\"oldText\":\"alpha\",\"newText\":\"ALPHA\"}]}"},
    {tools, "{\"path\":\"parallel.txt\",\"edits\":[{\"oldText\":\"beta\",\"newText\":\"BETA\"}]}"}
  };
  uv_thread_t threads[2];
  CHECK(uv_thread_create(&threads[0], parallel_edit, &work[0]) == 0);
  CHECK(uv_thread_create(&threads[1], parallel_edit, &work[1]) == 0);
  CHECK(uv_thread_join(&threads[0]) == 0);
  CHECK(uv_thread_join(&threads[1]) == 0);
  CHECK(invoke(tools, c, "read", "{\"path\":\"parallel.txt\"}", "ALPHA\nBETA\n") == PANGU_OK);
  /* Pure injected normalizing config has no callable legacy validator. */
  pg_scope *custom=NULL; pg_lease *custom_lease=NULL;
  pg_file_tools_config injected=cfg; injected.validate=NULL;
  injected.validate_arguments=pg_schema_normalize;
  CHECK(pg_plugin_install(pg_host_root(h),&pg_file_tools_plugin,&injected,sizeof(injected),&custom)==PG_OK);
  CHECK(pg_service_acquire(pg_host_root(h),PG_NAME(PG_TOOLS_SERVICE),1,&custom_lease)==PG_OK);
  const pg_tools_service *custom_tools=pg_lease_table(custom_lease);
  CHECK(!custom_tools->tools[0].validate && custom_tools->tools[0].validate_arguments && !custom_tools->tools[0].validate_call);
  pangu_loop_config custom_loop={.tools=custom_tools->tools,.tool_count=custom_tools->count};
  pangu_content custom_read={.kind=PANGU_TOOL_CALL,.name=SL("read"),.id=SL("injected"),.data=SL("{\"path\":\"normalized.txt\",\"offset\":\"1\",\"limit\":null}")};
  assistant.content=&custom_read; outcome=(pangu_tool_outcome){0};
  CHECK(pangu_core_call_tool(c,&assistant,&custom_read,&custom_loop,NULL,NULL,&outcome)==PANGU_OK);
  CHECK(outcome.result && !pangu_core_message_at(outcome.result,0)->is_error);
  pangu_core_destroy(outcome.result); pg_lease_release(custom_lease); CHECK(pg_scope_dispose(custom)==PG_OK);
  pangu_core_destroy(c);
  pg_lease_release(l);
  CHECK(pg_scope_dispose(s) == PG_OK);
  CHECK(pg_service_acquire(pg_host_root(h), PG_NAME(PG_TOOLS_SERVICE), 1, &l) ==
        PG_NOT_FOUND);
  pg_host_destroy(h);
  puts("file tools: Core write/edit/read roundtrip, pagination/truncation, Unicode/space path, parent "
       "creation, unload passed");
  return 0;
}
