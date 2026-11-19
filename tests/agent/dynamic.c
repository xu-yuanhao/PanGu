#include "native.h"
#include "slices.h"
#include <pangu/agent/app.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define S(x) ((pangu_slice){x,sizeof(x)-1})
typedef struct {pg_agent_app *app; pg_name path; int calls;} state;
static void busy(void *ctx) {
  state *s=ctx;
  s->calls++;
  assert(pg_agent_app_load_plugin(s->app,(pg_name){0},s->path,NULL,0,NULL)==PG_FAILED);
}
int main(int argc,char **argv) {
  assert(argc==2);
  for (int b=PG_STORE_JSONL;b<=PG_STORE_SQLITE;b++) {
    const char *path=b==PG_STORE_JSONL?"dynamic.jsonl":"dynamic.db";
    remove(path);
    pg_session_options o={{path,strlen(path)},b,1,0};
    state s={.path={argv[1],strlen(argv[1])}};
    assert(pg_agent_app_open(&o,NULL,0,&s.app)==PG_OK);
    int cleaned=0;
    agent_native_config cfg={busy,&s,&cleaned,0};
    assert(pg_agent_app_load_plugin(s.app,(pg_name){0},s.path,&cfg,sizeof(cfg),NULL)==PG_OK);
    cfg.fail=1;
    assert(pg_agent_app_load_plugin(s.app,(pg_name){0},s.path,&cfg,sizeof(cfg),NULL)==PG_FAILED);
    assert(cleaned==1); /* Failed replacement restores first provider. */
    pangu_content c={.kind=PANGU_TEXT,.data=S("native input")};
    pangu_message m={.role=PANGU_USER,.content=&c,.content_count=1};
    pangu_loop_config loop={0};
    assert(pg_agent_app_run(s.app,&m,1,&loop)==PG_SESSION_OK);
    assert(s.calls==2);
    const pangu_core *messages=pg_session_messages(pg_agent_app_session(s.app));
    int found=0;
    for(size_t j=0;j<pangu_core_message_count(messages);j++) {
      const pangu_message *r=pangu_core_message_at(messages,j);
      if(r->role==PANGU_TOOL_RESULT) {
        assert(!r->is_error && slice_contains(r->content[0].data,"native executed"));
        found++;
      }
    }
    assert(found==1);
    pg_agent_app_close(s.app);
    assert(cleaned==2);
    o.create=0;
    assert(pg_agent_app_open(&o,NULL,0,&s.app)==PG_OK);
    messages=pg_session_messages(pg_agent_app_session(s.app));
    assert(slice_contains(pangu_core_message_at(messages,pangu_core_message_count(messages)-1)->content[0].data,"native completed"));
    pg_agent_app_close(s.app);
    remove(path);
  }
  puts("app native tools/busy/rollback/close/reopen passed");
}
