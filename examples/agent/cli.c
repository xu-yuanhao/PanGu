#include <pangu/agent/app.h>
#include <uv.h>
#ifdef PANGU_AGENT_MCP
#include "mcp_config.h"
#endif
#ifdef PANGU_AGENT_RESOURCES
#include <pangu/services/resources.h>
#endif
#ifdef PANGU_AGENT_TUI
#include <pangu/tui/plugin.h>
#endif
#ifdef PANGU_AGENT_REAL
#include <pangu/services/model.h>
#include <pangu/services/tools.h>
#include <uv.h>
#endif
#if defined(PANGU_AGENT_SCHEMA) || defined(PANGU_AGENT_MCP)
#include <pangu/services/schema.h>
#endif
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
static volatile sig_atomic_t interrupted;
static void interrupt_handler(int number) {
  (void)number;
  interrupted = 1;
}
static pangu_slice string(const char *s) {
  return (pangu_slice){s, s ? strlen(s) : 0};
}
static pangu_status demo(void *ctx, pangu_core *c, const pangu_core *request,
                         pangu_model_sink sink, void *data) {
  (void)ctx;
  (void)request;
  if (pangu_core_is_cancelled(c))
    return PANGU_CANCELLED;
  pangu_content text = {.kind = PANGU_TEXT,
                        .data = SL("PanGu deterministic demo response")};
  pangu_message m = {.role = PANGU_ASSISTANT,
                     .content = &text,
                     .content_count = 1,
                     .stop_reason = PANGU_STOP};
  return sink(data, &m, 1);
}
static void print_message(const pangu_message *m) {
  if (m->role == PANGU_SYSTEM && !m->content_count)
    return;
  printf("%s: ", m->role == PANGU_USER          ? "user"
                 : m->role == PANGU_ASSISTANT   ? "assistant"
                 : m->role == PANGU_TOOL_RESULT ? "tool"
                                                : "context");
  for (size_t i = 0; i < m->content_count; i++) {
    if (m->content[i].kind == PANGU_TEXT)
      fwrite(m->content[i].data.data, 1, m->content[i].data.len, stdout);
    else if (m->content[i].kind == PANGU_TOOL_CALL)
      printf("[tool %.*s]", (int)m->content[i].name.len,
             m->content[i].name.data);
  }
  if (m->role == PANGU_ASSISTANT &&
      (m->stop_reason == PANGU_ERROR || m->stop_reason == PANGU_ABORTED))
    fputs(m->stop_reason == PANGU_ABORTED ? "[cancelled]" : "[model failed]",
          stdout);
  putchar('\n');
  fflush(stdout);
}
static void event(void *ctx, pangu_session *s, const pg_session_event *e) {
  (void)ctx;
  (void)s;
  if (e->kind == PG_SESSION_LIVE && e->live->kind == PANGU_MESSAGE_END)
    print_message(e->live->message);
}
#ifdef PANGU_AGENT_REAL
static pangu_status poll(void *ctx, pangu_core *owner) {
  pg_agent_app_poll(ctx, owner);
  if (interrupted)
    pangu_core_cancel(owner);
  return pangu_core_is_cancelled(owner) ? PANGU_CANCELLED : PANGU_OK;
}
#endif
typedef enum {
  INPUT,
  TREE,
  SWITCH,
  RESET,
  COMPACT,
  SUMMARY,
  USAGE,
  ENQUEUE,
  RESUME,
  QUEUES,
  CLEAR_QUEUE,
  RELOAD
} action_kind;
typedef struct {
  action_kind kind;
  const char *text;
  uint64_t number;
} action;
static int number(const char *text, uint64_t *out) {
  if (!text || !*text || *text == '-')
    return 0;
  char *end;
  errno = 0;
  unsigned long long n = strtoull(text, &end, 10);
  if (errno || *end)
    return 0;
  *out = (uint64_t)n;
  return 1;
}
static void show_tree(pangu_session *s) {
  for (size_t i = 0; i < pg_session_entry_count(s); i++) {
    pg_session_entry e;
    if (pg_session_entry_at(s, i, &e) != PG_SESSION_OK)
      break;
    printf("%c entry=%llu parent=%llu kind=%s",
           e.id == pg_session_leaf(s) ? '*' : ' ', (unsigned long long)e.id,
           (unsigned long long)e.parent,
           e.kind == PG_ENTRY_MESSAGE ? "message"
           : e.kind == PG_ENTRY_RESET ? "reset"
           : e.kind == PG_ENTRY_OMISSION ? "omission"
                                         : "compaction");
    if (e.first_kept)
      printf(" kept=%llu", (unsigned long long)e.first_kept);
    putchar('\n');
  }
}
static int usage(void) {
  fputs("usage: pangu_agent_cli create|open FILE [--approval ask|never] [--sqlite] [--demo TEXT ... | "
        "--deepseek TEXT ... | --tui-demo | --tui] [--cwd DIR] [--agent-dir "
        "DIR] [--trust-project] [--no-tools] [--plugin LOCAL_PATH ...]\n"
        "--plugin explicitly executes trusted native code, in path order; "
        "relative paths use --cwd. Plugins accept NULL/0 config.\n"
        "--mcp-config FILE explicitly executes trusted stdio server commands; "
        "relative config/cwd paths use --cwd.\n"
        "DeepSeek reads user auth.json (when resources are built), "
        "DEEPSEEK_API_KEY, PANGU_MODEL, PANGU_DEEPSEEK_ENDPOINT "
        "from process environment.\n"
        "Resources: user ~/.pangu/agent (or PANGU_AGENT_DIR/--agent-dir); "
        "project .pangu requires --trust-project; --reload between inputs.\n"
        "Commands in order: --tree --switch ID --reset --handoff TEXT "
        "--compact KEEP_TOKENS --compact-summary FIRST_KEPT TEXT --usage\n"
        "Mailbox: --steer TEXT --follow-up TEXT --queues --resume "
        "--resume-demo --clear-queue steering|follow-up\n",
        stderr);
  return 2;
}
#if defined(PANGU_AGENT_REAL) && !defined(PANGU_AGENT_RESOURCES)
static pangu_status environment_key(void *ctx, pangu_core *owner,
                                     pangu_slice provider,
                                     pangu_api_key_sink sink, void *out) {
  (void)ctx;
  (void)owner;
  if (provider.len != 8 || memcmp(provider.data, "deepseek", 8))
    return PANGU_OK;
  const char *key = getenv("DEEPSEEK_API_KEY");
  return key && *key ? sink(out, string(key)) : PANGU_OK;
}
#endif
#ifdef PANGU_AGENT_MCP
static void mcp_poll(void *ctx) {
  pg_agent_app *app = ctx;
  pg_agent_app_poll(app, NULL);
  if (interrupted)
    pg_session_cancel(pg_agent_app_session(app));
}
static int install_mcp(pg_agent_app *app, const char *file, const char *cwd) {
  pg_cli_mcp_config *parsed = NULL;
  if (pg_cli_mcp_read(file, cwd, &parsed) != PG_OK)
    return 0;
  size_t count = pg_cli_mcp_count(parsed), used = 0;
  if (count >= SIZE_MAX / sizeof(pg_dependency)) {
    pg_cli_mcp_free(parsed);
    return 0;
  }
  pg_dependency *sources = calloc(count + 1, sizeof(*sources));
  if (!sources) {
    pg_cli_mcp_free(parsed);
    return 0;
  }
  pg_scope *root = pg_agent_app_scope(app), *scope = NULL;
  pg_lease *base = NULL;
  pg_status status = pg_service_acquire(root, PG_NAME(PG_TOOLS_SERVICE), 1, &base);
  if (status == PG_OK) {
    sources[used++] = (pg_dependency){PG_NAME(PG_TOOLS_SERVICE), 1};
    pg_lease_release(base);
  } else if (status == PG_NOT_FOUND)
    status = PG_OK;
  for (size_t i = 0; status == PG_OK && i < count; i++) {
    const pg_mcp_http_config *http = pg_cli_mcp_http_at(parsed, i);
    if (http) {
#ifdef PANGU_AGENT_MCP_HTTP
      pg_mcp_http_config config = *http;
      config.validate = pg_schema_validate;
      config.poll = mcp_poll;
      config.poll_ctx = app;
      status = pg_plugin_install(root, &pg_mcp_http_plugin, &config,
                                 sizeof(config), &scope);
#else
      status = PG_NOT_FOUND;
#endif
    } else {
      pg_mcp_stdio_config config = *pg_cli_mcp_at(parsed, i);
      config.validate = pg_schema_validate;
      config.poll = mcp_poll;
      config.poll_ctx = app;
      status = pg_plugin_install(root, &pg_mcp_stdio_plugin, &config,
                                 sizeof(config), &scope);
    }
    if (status == PG_OK)
      sources[used++] = (pg_dependency){pg_cli_mcp_at(parsed, i)->service_name, 1};
  }
  if (status == PG_OK)
    status = pg_tools_aggregate_install(root, PG_NAME(PG_TOOLS_SERVICE),
                                         sources, used, &scope);
  free(sources);
  pg_cli_mcp_free(parsed);
  /* Caller closes app on failure, disposing every successfully started child. */
  return status == PG_OK;
}
#endif
int main(int argc, char **argv) {
  if (argc < 3 || (strcmp(argv[1], "create") && strcmp(argv[1], "open")))
    return usage();
  int real = 0, tui = 0, no_tools = 0, mode = 0;
  const char *cwd_arg = NULL, *agent_dir = getenv("PANGU_AGENT_DIR");
  const char *mcp_file = NULL;
  int trust_project = 0;
  pg_session_backend backend = PG_STORE_JSONL;
  pg_app_approval_policy approval = PG_APP_APPROVAL_DISABLED;
  action *prompts = calloc((size_t)argc, sizeof(*prompts));
  int needs_model = 0;
  if (!prompts)
    return 1;
  size_t count = 0, native_count = 0;
  for (int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "--sqlite"))
      backend = PG_STORE_SQLITE;
    else if (!strcmp(argv[i], "--approval") && i + 1 < argc) {
      const char *policy = argv[++i];
      if (!strcmp(policy, "ask")) approval = PG_APP_APPROVAL_ASK;
      else if (!strcmp(policy, "never")) approval = PG_APP_APPROVAL_NEVER;
      else { free(prompts); return usage(); }
    }
    else if (!strcmp(argv[i], "--no-tools"))
      no_tools = 1;
    else if (!strcmp(argv[i], "--trust-project"))
      trust_project = 1;
    else if (!strcmp(argv[i], "--agent-dir") && i + 1 < argc)
      agent_dir = argv[++i];
    else if (!strcmp(argv[i], "--mcp-config") && i + 1 < argc &&
             argv[i + 1][0] && !mcp_file)
      mcp_file = argv[++i];
    else if (!strcmp(argv[i], "--plugin") && i + 1 < argc && argv[i + 1][0])
      prompts[(size_t)argc - 1 - native_count++] =
          (action){INPUT, argv[++i], 0}; /* Paths occupy the unused tail. */
    else if (!strcmp(argv[i], "--reload") && !tui)
      prompts[count++] = (action){RELOAD, NULL, 0};
    else if (!strcmp(argv[i], "--cwd") && i + 1 < argc)
      cwd_arg = argv[++i];
    else if (!strcmp(argv[i], "--demo") || !strcmp(argv[i], "--deepseek")) {
      int next = !strcmp(argv[i], "--deepseek") ? 2 : 1;
      if (tui || (mode && mode != next) || i + 1 >= argc) {
        free(prompts);
        return usage();
      }
      mode = next;
      real = next == 2;
      prompts[count++] = (action){INPUT, argv[++i], 0};
    } else if (!strcmp(argv[i], "--tui-demo") || !strcmp(argv[i], "--tui")) {
      if (mode || count) {
        free(prompts);
        return usage();
      }
      mode = 3;
      tui = 1;
      real = !strcmp(argv[i], "--tui");
    } else if ((!strcmp(argv[i], "--steer") ||
                !strcmp(argv[i], "--follow-up")) &&
               i + 1 < argc && !tui) {
      pangu_queue_kind kind =
          !strcmp(argv[i], "--steer") ? PANGU_STEERING : PANGU_FOLLOW_UP;
      prompts[count++] = (action){ENQUEUE, argv[++i], kind};
    } else if ((!strcmp(argv[i], "--resume") ||
                !strcmp(argv[i], "--resume-demo")) &&
               !tui) {
      if (!strcmp(argv[i], "--resume-demo")) {
        if (mode && mode != 1) {
          free(prompts);
          return usage();
        }
        mode = 1;
        real = 0;
      } else
        needs_model = 1;
      prompts[count++] = (action){RESUME, NULL, 0};
    } else if (!strcmp(argv[i], "--queues") && !tui) {
      prompts[count++] = (action){QUEUES, NULL, 0};
    } else if (!strcmp(argv[i], "--clear-queue") && i + 1 < argc && !tui) {
      const char *name = argv[++i];
      if (strcmp(name, "steering") && strcmp(name, "follow-up")) {
        free(prompts);
        return usage();
      }
      prompts[count++] = (action){CLEAR_QUEUE, NULL,
                                  !strcmp(name, "steering") ? PANGU_STEERING
                                                            : PANGU_FOLLOW_UP};
    } else if (!strcmp(argv[i], "--tree") || !strcmp(argv[i], "--usage") ||
               !strcmp(argv[i], "--reset")) {
      if (tui) {
        free(prompts);
        return usage();
      }
      prompts[count++] = (action){!strcmp(argv[i], "--tree")    ? TREE
                                  : !strcmp(argv[i], "--usage") ? USAGE
                                                                : RESET,
                                  NULL, 0};
    } else if (!strcmp(argv[i], "--handoff") && i + 1 < argc && !tui) {
      prompts[count++] = (action){RESET, argv[++i], 0};
    } else if ((!strcmp(argv[i], "--switch") || !strcmp(argv[i], "--compact") ||
                !strcmp(argv[i], "--compact-summary")) &&
               i + 1 < argc && !tui) {
      action_kind kind = !strcmp(argv[i], "--switch")    ? SWITCH
                         : !strcmp(argv[i], "--compact") ? COMPACT
                                                         : SUMMARY;
      uint64_t n;
      if (!number(argv[++i], &n) || (kind == COMPACT && n > SIZE_MAX)) {
        free(prompts);
        return usage();
      }
      const char *text = NULL;
      if (kind == SUMMARY) {
        if (i + 1 >= argc) {
          free(prompts);
          return usage();
        }
        text = argv[++i];
      }
      if (kind == COMPACT)
        needs_model = 1;
      prompts[count++] = (action){kind, text, n};
    } else {
      free(prompts);
      return usage();
    }
  }
  if (!mode && needs_model)
    real = 1;
#ifndef PANGU_AGENT_MCP
  if (mcp_file) {
    fputs("MCP stdio with mature Schema support was not built\n", stderr);
    free(prompts);
    return 1;
  }
#endif
#ifndef PANGU_AGENT_REAL
  if (real) {
    fputs("DeepSeek service was not built\n", stderr);
    free(prompts);
    return 1;
  }
#endif
#ifndef PANGU_AGENT_TUI
  if (tui) {
    fputs("TUI was not built\n", stderr);
    free(prompts);
    return 1;
  }
#endif
#ifndef PANGU_AGENT_RESOURCES
  int wants_resources = trust_project || agent_dir != NULL;
  for (size_t i = 0; i < count; i++)
    if (prompts[i].kind == RELOAD)
      wants_resources = 1;
  if (wants_resources) {
    fputs("Resource service was not built\n", stderr);
    free(prompts);
    return 1;
  }
#endif
#ifndef PANGU_AGENT_SCHEMA
  if (real && !no_tools) {
    fputs("Mature Schema adapter was not built; install it or explicitly "
          "select --no-tools\n",
          stderr);
    free(prompts);
    return 1;
  }
#endif
  int have_policy = 0;
  pg_compaction_policy automatic_policy = {0};
  const char *capacity = getenv("PANGU_CONTEXT_TOKENS");
  if (capacity && (real || tui)) {
    uint64_t context = 0, reserve = 16384, keep = 20000;
    const char *r = getenv("PANGU_RESERVE_TOKENS"),
               *k = getenv("PANGU_KEEP_RECENT_TOKENS");
    if (!number(capacity, &context) || (r && !number(r, &reserve)) ||
        (k && !number(k, &keep)) || context > SIZE_MAX || reserve > SIZE_MAX ||
        keep > SIZE_MAX || !context || reserve >= context) {
      fputs("Invalid compaction capacity/reserve/keep configuration\n", stderr);
      free(prompts);
      return 2;
    }
    automatic_policy = (pg_compaction_policy){1, (size_t)context,
                                              (size_t)reserve, (size_t)keep};
    have_policy = 1;
  }
  size_t cwd_size = 4096;
  char *cwd = malloc(cwd_size), *user_dir = NULL;
  if (!cwd) {
    free(prompts);
    return 1;
  }
  int cwd_code;
  while ((cwd_code = uv_cwd(cwd, &cwd_size)) == UV_ENOBUFS) {
    char *next = realloc(cwd, cwd_size);
    if (!next) {
      free(cwd);
      free(prompts);
      return 1;
    }
    cwd = next;
  }
  if (!cwd_code && cwd_arg) {
    uv_fs_t request;
    cwd_code = uv_fs_realpath(NULL, &request, cwd_arg, NULL);
    if (!cwd_code) {
      size_t n = strlen(request.ptr);
      char *next = malloc(n + 1);
      if (next)
        memcpy(next, request.ptr, n + 1);
      if (!next)
        cwd_code = UV_ENOMEM;
      else {
        free(cwd);
        cwd = next;
      }
    }
    uv_fs_req_cleanup(&request);
  }
  if (cwd_code) {
    free(cwd);
    free(prompts);
    return 1;
  }
#ifdef PANGU_AGENT_RESOURCES
  if (!agent_dir) {
    size_t home_size = 4096;
    char *home = malloc(home_size);
    if (!home) {
      free(cwd);
      free(prompts);
      return 1;
    }
    int code;
    while ((code = uv_os_homedir(home, &home_size)) == UV_ENOBUFS) {
      char *next = realloc(home, home_size);
      if (!next) {
        free(home);
        free(cwd);
        free(prompts);
        return 1;
      }
      home = next;
    }
    if (code) {
      free(home);
      free(cwd);
      free(prompts);
      return 1;
    }
    size_t n = strlen(home);
    user_dir = malloc(n + sizeof("/.pangu/agent"));
    if (user_dir) {
      memcpy(user_dir, home, n);
      memcpy(user_dir + n, "/.pangu/agent", sizeof("/.pangu/agent"));
    }
    free(home);
    if (!user_dir) {
      free(cwd);
      free(prompts);
      return 1;
    }
    agent_dir = user_dir;
  } else if (*agent_dir != '/' &&
             !(strlen(agent_dir) > 2 && agent_dir[1] == ':')) {
    size_t n = strlen(cwd), m = strlen(agent_dir);
    if (m > SIZE_MAX - n - 2) {
      free(cwd);
      free(prompts);
      return 1;
    }
    user_dir = malloc(n + m + 2);
    if (!user_dir) {
      free(cwd);
      free(prompts);
      return 1;
    }
    memcpy(user_dir, cwd, n);
    user_dir[n] = '/';
    memcpy(user_dir + n + 1, agent_dir, m + 1);
    agent_dir = user_dir;
  }
  pg_resources_config resources = {.user_directory = agent_dir,
                                   .cwd = cwd,
                                   .project_trusted = trust_project};
  pg_app_plugin plugins[] = {
      {&pg_resources_plugin, &resources, sizeof(resources)}};
#else
  (void)agent_dir;
  (void)trust_project;
#endif
  pg_session_options options = {string(argv[2]), backend,
                                !strcmp(argv[1], "create"), 0};
  pg_agent_app *app = NULL;
  if (pg_agent_app_open(&options,
#ifdef PANGU_AGENT_RESOURCES
                        plugins, 1,
#else
                        NULL, 0,
#endif
                        &app) != PG_OK) {
    fputs("session open failed\n", stderr);
    free(cwd);
    free(user_dir);
    free(prompts);
    return 1;
  }
  int rc = 0;
  pg_lease *tools = NULL;
  pangu_loop_config loop = {.request = real ? NULL : demo};
#ifdef PANGU_AGENT_REAL
  pg_deepseek_config provider = {.poll = poll, .poll_ctx = app};
  if (real) {
    const char *endpoint = getenv("PANGU_DEEPSEEK_ENDPOINT"),
               *model = getenv("PANGU_MODEL");
#ifndef PANGU_AGENT_RESOURCES
    loop.get_api_key = environment_key;
#endif
    loop.model_config =
        (pangu_model_config){.id = string(model && *model ? model : NULL),
                             .provider = SL("deepseek"),
                             .api = SL("openai-completions")};
    provider.endpoint = string(endpoint);
#ifndef PANGU_AGENT_RESOURCES
    if (!loop.model_config.id.len)
      loop.model_config.id = SL("deepseek-chat");
#endif
#ifdef PANGU_AGENT_SCHEMA
    if (!no_tools) {
      pg_file_tools_config files = {.environment =
                                        pg_local_file_environment(cwd),
                                    .validate = pg_schema_validate};
      pg_scope *scope = NULL;
      if (pg_plugin_install(pg_agent_app_scope(app), &pg_file_tools_plugin,
                            &files, sizeof(files), &scope) != PG_OK ||
          pg_service_acquire(pg_agent_app_scope(app), PG_NAME(PG_TOOLS_SERVICE),
                             1, &tools) != PG_OK) {
        rc = 1;
        goto done;
      }
      const pg_tools_service *table = pg_lease_table(tools);
      provider.tools = table->tools;
      provider.tool_count = table->count;
    }
#endif
    pg_scope *scope = NULL;
    if (pg_plugin_install(pg_agent_app_scope(app), &pg_deepseek_plugin,
                          &provider, sizeof(provider), &scope) != PG_OK) {
      fputs("model plugin installation failed\n", stderr);
      rc = 1;
      goto done;
    }
  }
#else
  (void)cwd_arg;
  (void)no_tools;
#endif
  for (size_t i = 0; i < native_count; i++) {
    const char *path = prompts[(size_t)argc - 1 - i].text;
    pg_dynamic_diagnostic diagnostic;
    pg_status loaded = pg_agent_app_load_plugin(
        app, (pg_name){cwd, strlen(cwd)}, (pg_name){path, strlen(path)},
        NULL, 0, &diagnostic);
    if (loaded != PG_OK) {
      fprintf(stderr, "native plugin load failed (status=%d stage=%d): %s\n",
              (int)loaded, (int)diagnostic.stage, diagnostic.message);
      rc = 1;
      goto done;
    }
  }
  signal(SIGINT, interrupt_handler);
#ifdef PANGU_AGENT_MCP
  if (mcp_file && !install_mcp(app, mcp_file, cwd)) {
    fputs("MCP configuration or server installation failed\n", stderr);
    rc = 1;
    goto done;
  }
#endif
  if (have_policy &&
      pg_session_set_compaction_policy(pg_agent_app_session(app),
                                       &automatic_policy) != PG_SESSION_OK) {
    rc = 1;
    goto done;
  }
  if (pg_agent_app_set_approval(app, approval) != PG_OK) {
    pg_agent_app_close(app); free(prompts); return 1;
  }
  pangu_session *session = pg_agent_app_session(app);
  if (!tui) {
    const pangu_core *history = pg_session_messages(session);
    for (size_t i = 0; i < pangu_core_message_count(history); i++)
      print_message(pangu_core_message_at(history, i));
  }
#ifdef PANGU_AGENT_TUI
  if (tui) {
    pg_tui_config config = {.loop = loop};
    rc = pg_agent_app_present(app, pg_tui_plugin(), &config, sizeof(config)) !=
         PG_OK;
  }
#endif
  if (!tui) {
    uint64_t subscription = 0;
    if (pg_session_subscribe(session, event, NULL, &subscription) !=
        PG_SESSION_OK) {
      rc = 1;
      goto done;
    }
    for (size_t i = 0; i < count; i++) {
      interrupted = 0;
      action command = prompts[i];
      pg_session_status status = PG_SESSION_OK;
      if (command.kind == TREE) {
        show_tree(session);
        continue;
      }
      if (command.kind == USAGE) {
        pangu_usage u = pg_session_usage(session);
        printf("Usage: tokens=%llu cost=%.9g\n", (unsigned long long)u.total,
               u.total_cost);
        continue;
      }
      if (command.kind == QUEUES) {
        printf("Queues: steering=%zu follow-up=%zu\n",
               pg_session_queue_count(session, PANGU_STEERING),
               pg_session_queue_count(session, PANGU_FOLLOW_UP));
        continue;
      }
      if (command.kind == RELOAD) {
        if (pg_agent_app_reload_resources(app) != PG_OK) {
          rc = 1;
          break;
        }
        continue;
      }
      if (command.kind == ENQUEUE) {
        pangu_content c = {.kind = PANGU_TEXT, .data = string(command.text)};
        pangu_message m = {
            .role = PANGU_USER, .content = &c, .content_count = 1};
        status =
            pg_agent_app_enqueue(app, (pangu_queue_kind)command.number, &m);
      } else if (command.kind == RESUME)
        status = pg_agent_app_run(app, NULL, 0, &loop);
      else if (command.kind == CLEAR_QUEUE)
        status =
            pg_session_clear_queue(session, (pangu_queue_kind)command.number);
      else if (command.kind == SWITCH)
        status = pg_session_switch(session, command.number);
      else if (command.kind == RESET)
        status = pg_session_reset(session, string(command.text));
      else if (command.kind == SUMMARY)
        status = pg_session_compact_summary(session, command.number,
                                            string(command.text), NULL);
      else if (command.kind == COMPACT)
        status = pg_agent_app_compact(app, (size_t)command.number, &loop);
      if (command.kind != INPUT) {
        if (status != PG_SESSION_OK) {
          fprintf(stderr, "Session command failed (%d)\n", status);
          rc = 1;
          break;
        }
        continue;
      }
      pangu_content content = {.kind = PANGU_TEXT,
                               .data = string(command.text)};
      pangu_message message = {
          .role = PANGU_USER, .content = &content, .content_count = 1};
      status = pg_agent_app_run(app, &message, 1, &loop);
      if (status != PG_SESSION_OK && status != PG_SESSION_CANCELLED) {
        rc = 1;
        break;
      }
      const pangu_core *history = pg_session_messages(session);
      size_t n = pangu_core_message_count(history);
      const pangu_message *tail =
          n ? pangu_core_message_at(history, n - 1) : NULL;
      if (tail && tail->role == PANGU_ASSISTANT &&
          tail->stop_reason == PANGU_ERROR) {
        rc = 1;
        break;
      }
    }
    pg_session_unsubscribe(session, subscription);
  }
done:
  if (tools)
    pg_lease_release(tools);
  pg_agent_app_close(app);
  free(cwd);
  free(user_dir);
  free(prompts);
  return rc;
}
