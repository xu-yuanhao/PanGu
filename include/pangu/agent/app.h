#ifndef PANGU_AGENT_APP_H
#define PANGU_AGENT_APP_H
#include <pangu/agent/plugin.h>
#include <pangu/services/dynamic.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct pg_agent_app pg_agent_app;
/* App owns host, installed scopes and leases. Additional model/tool plugins
 * are installed on root before the session plugin; presentation installs after
 * it. Descriptors/configs remain alive through open return; each plugin defines
 * retention of its own config. Open failure tears down in reverse order. Close
 * is called on owner thread after run, unsubscribes presentation before
 * releasing session and host. */
typedef struct {
  const pg_plugin *plugin;
  const void *config;
  size_t config_size;
} pg_app_plugin;
pg_status pg_agent_app_open(const pg_session_options *, const pg_app_plugin *,
                            size_t, pg_agent_app **);
pangu_session *
pg_agent_app_session(pg_agent_app *); /* borrowed through close */
pg_scope *
pg_agent_app_scope(pg_agent_app *); /* install/query optional capabilities */
/* Explicit trusted local native code, owner thread while idle only. Path/cwd
 * and config follow dynamic.h; no discovery, persistence or trust inference.
 * NULL/0 config requires a plugin that accepts it. Successful scope belongs
 * to app until close; registrations use normal host replacement semantics
 * (including pangu.tools, not implicit tool-array concatenation). Run acquires
 * the current provider/tools and holds leases until return. Close after run
 * releases leases before scope cleanup/dlclose. Failure rolls back this load,
 * preserving earlier installed plugins. PG_FAILED means app is busy.
 * Diagnostic is optional and copied; input slices borrowed through return. */
pg_status pg_agent_app_load_plugin(pg_agent_app *, pg_name cwd, pg_name path,
                                   const void *config, size_t config_size,
                                   pg_dynamic_diagnostic *);
/* Missing explicit provider is resolved from pangu.model. A NULL tools pointer
 * with zero count selects optional pangu.tools; a non-NULL empty array disables
 * automatic tools. Both service leases live through each actual request return.
 * When built with resource support, absent caller get_api_key selects optional
 * pangu.credentials ABI1. The explicit callback (with unchanged config.ctx)
 * takes precedence, including its empty result; credentials are not then read.
 * Otherwise the service is called with the current length-delimited provider
 * on each Core request, including tool continuations and summaries. NOT_FOUND
 * or an empty key preserves Core config.api_key fallback; other errors stop
 * that request with a generic error, without persisting credential details.
 * Service malloc output is copied into Core's request key, wiped then freed
 * before resolver return. Core wipes its copy after the request. Credentials
 * lease and resources snapshot remain held through synchronous run/compact;
 * callbacks run on that calling thread, including presentation actions. Nested
 * app calls on that thread restore the previous resolver binding. No caller
 * ctx, provider request_ctx, hook, or cancellation owner is replaced. Builds
 * without resource support keep caller/Core credential behavior unchanged.
 * Optional pangu.context_classifier ABI1 (recovery.h) is resolved per run and
 * held through recovery/summary/continuation. Explicit session classifier wins.
 * With PanGu::overflow selected, the execution plugin owns the default Core
 * classifier lifetime; without it recovery supports numeric usage/LENGTH only.
 * Classifier failures stop recovery instead of becoming a non-match.
 * Model protocol declarations must match this same tool set; providers with
 * an explicit config.tools (e.g. DeepSeek) are configured by the composition.
 */
/* Explicit deployment composition, owner thread while idle (also presentation
 * idle). Disabled by default to preserve the existing unguarded composition.
 * ASK gates every selected tool through durable session asked/decided then
 * pangu.approval ABI1; NEVER durably rejects without consulting providers.
 * Includes explicit loop tools, resolved pangu.tools and every committed
 * prepare_request_update/prepare_next_turn_update TOOLS replacement (including
 * empty sets). Core retains its preparation transaction/declaration ordering.
 * Caller callback/provider contexts are unchanged; tool contexts remain borrowed
 * through run return. App holds each wrapper generation and copied declarations
 * through run return, including failure, cancellation and agent_end callbacks.
 * Policy is
 * process-local: reopen must configure it again; audit grants never replay.
 * No callbacks/context retained by this setter. Providers register on app scope
 * and follow services/approval.h revoke/quiesce/lease/dispose lifetime. A UI
 * answerer may pump owner-thread input, reply via provider id and cancel the
 * session; it must return before its scope is disposed. Session audit ids and
 * provider reply ids are separate namespaces. Busy changes fail unchanged. */
typedef enum { PG_APP_APPROVAL_DISABLED, PG_APP_APPROVAL_ASK,
               PG_APP_APPROVAL_NEVER } pg_app_approval_policy;
pg_status pg_agent_app_set_approval(pg_agent_app *, pg_app_approval_policy);
pg_session_status pg_agent_app_run(pg_agent_app *, const pangu_message *,
                                   size_t, const pangu_loop_config *);
/* Enqueue copies/expands USER text with the active run's resource snapshot
 * (or current snapshot while idle), then uses the same durable session mailbox.
 * May be called by same-thread run/provider/listener callbacks; no reload
 * occurs. Inputs are borrowed through return. Roles, persistence and errors
 * follow pg_session_enqueue. Non-text content and message metadata are
 * preserved. */
pg_session_status pg_agent_app_enqueue(pg_agent_app *, pangu_queue_kind,
                                       const pangu_message *);
pg_session_status pg_agent_app_compact(pg_agent_app *,
                                       size_t keep_recent_tokens,
                                       const pangu_loop_config *);
/* Install a presentation plugin after session creation. Config ABI is owned by
 * that plugin. Service must provide pangu.tui ABI1; starts, runs, stops and
 * disposes before return. Config and loop contexts live through return.
 * The pg_tui_config legacy prefix ending before actions is accepted, as are
 * complete/current or larger future configs; truncated tails are INVALID.
 * App supplies execution actions and keeps UI lifetime separate from requests:
 * external run/compact, close and nested present remain blocked until disposal;
 * action run/compact resolve fresh services/resources each time, while enqueue
 * may run synchronously from a provider poll using the active snapshot.
 * poll is callable only by a provider on the same run thread, never UI reader.
 */
pg_status pg_agent_app_present(pg_agent_app *, const pg_plugin *, const void *,
                               size_t);
/* Optional pangu.resources ABI1 is consumed for each run: unset model identity
 * and OFF thinking use settings defaults; system prompt replaces SYSTEM
 * messages in the provider-only request view, and USER text skill/template
 * expansion is committed as input. Resources require request/request_events (or
 * a model service), not raw-state legacy model. Snapshot and leases live
 * through run return. ctx, provider ctx and all caller hooks retain their
 * original meaning. Configure providers consistently with defaultProvider; this
 * does not install a provider or switch network protocols. A new run takes a
 * fresh snapshot. These owner-thread operations run on the caller thread;
 * reload is allowed while presentation is idle, but fails during actual
 * run/compact and leaves the service snapshot unchanged on service
 * reload failure. expand returns malloc UTF-8 bytes plus length; caller free()s
 * output. expand is also allowed in same-thread run callbacks and uses the
 * active snapshot. Without a resource service expansion copies input and reload
 * returns NOT_FOUND.
 */
pg_status pg_agent_app_reload_resources(pg_agent_app *);
pg_status pg_agent_app_expand(pg_agent_app *, pangu_slice input, char **output,
                              size_t *length);
void pg_agent_app_poll(void *, pangu_core *);
void pg_agent_app_close(
    pg_agent_app *); /* ignored during run or presentation; stop/settle first */
#ifdef __cplusplus
}
#endif
#endif
