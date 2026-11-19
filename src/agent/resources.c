#include "internal.h"
#include "resources_internal.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>
struct pg_agent_resources_run {
  pg_lease *lease, *credentials;
  struct pg_agent_resources_run *previous;
  int bound;
  pangu_session *session;
  pg_resource_snapshot *snapshot;
  yyjson_doc *settings;
  pangu_loop_config original;
  pangu_core *prompts;
  pangu_message *messages;
};
/* Core keeps caller ctx for every hook. A thread-local stack binds only the
 * implicit resolver, allowing nested app runs without changing caller/provider
 * contexts. Binding is entered only around synchronous run/compact. */
static _Thread_local pg_agent_resources_run *credential_run;
void pg_agent_resources_enter(pg_agent_resources_run *r) {
  if (!r)
    return;
  r->previous = credential_run;
  r->bound = 1;
  credential_run = r;
}
static pangu_status credentials_key(void *ctx, pangu_core *owner,
                                    pangu_slice provider,
                                    pangu_api_key_sink sink, void *out) {
  (void)ctx;
  (void)owner;
  if (!provider.len)
    return PANGU_OK; /* Deterministic/anonymous providers need no auth. */
  pg_agent_resources_run *r = credential_run;
  if (!r || !r->credentials)
    return PANGU_INVALID;
  const pg_credentials_service *api = pg_lease_table(r->credentials);
  char *key = NULL;
  size_t length = 0;
  pg_status z = api->get_api_key(pg_lease_context(r->credentials), provider,
                                 &key, &length);
  pangu_status status = PANGU_OK;
  if (z == PG_OK)
    status = (!key && length) ? PANGU_INVALID
                              : sink(out, (pangu_slice){key, length});
  else if (z != PG_NOT_FOUND)
    status = z == PG_NOMEM ? PANGU_NOMEM : PANGU_INVALID;
  if (key) {
    volatile unsigned char *bytes = (volatile unsigned char *)key;
    for (size_t i = 0; i < length; i++)
      bytes[i] = 0;
    free(key);
  }
  return status;
}
static pg_status acquire(pg_scope *scope, pg_lease **lease,
                         pg_resource_snapshot **snapshot) {
  *lease = NULL;
  *snapshot = NULL;
  pg_status z =
      pg_service_acquire(scope, PG_NAME(PG_RESOURCES_SERVICE), 1, lease);
  if (z != PG_OK)
    return z;
  const pg_resources_service *api = pg_lease_table(*lease);
  z = api->snapshot(pg_lease_context(*lease), snapshot);
  if (z != PG_OK) {
    pg_lease_release(*lease);
    *lease = NULL;
  }
  return z;
}
static pg_status expand(const pg_resource_snapshot *snapshot, pangu_slice in,
                        char **out, size_t *length) {
  /* Pi AgentSession._expandSkillCommand: explicit invocation includes the body,
   * its location and base directory, even for model-invocation-disabled skills.
   * The immutable snapshot supplies stripped frontmatter; reload refreshes it.
   */
  if (in.len >= 7 && !memcmp(in.data, "/skill:", 7)) {
    size_t end = 7;
    while (end < in.len && !isspace((unsigned char)in.data[end]))
      end++;
    for (size_t i = 0; i < pg_resource_count(snapshot); i++) {
      const pg_resource *r = pg_resource_at(snapshot, i);
      if (r->kind != PG_RESOURCE_SKILL || r->name.len != end - 7 ||
          memcmp(r->name.data, in.data + 7, end - 7))
        continue;
      size_t arg = end, tail = in.len, body = 0, body_end = r->body.len;
      while (arg < tail && isspace((unsigned char)in.data[arg]))
        arg++;
      while (tail > arg && isspace((unsigned char)in.data[tail - 1]))
        tail--;
      while (body < body_end && isspace((unsigned char)r->body.data[body]))
        body++;
      while (body_end > body &&
             isspace((unsigned char)r->body.data[body_end - 1]))
        body_end--;
      size_t base = r->path.len;
      while (base && r->path.data[base - 1] != '/' &&
             r->path.data[base - 1] != '\\')
        base--;
      if (base > 1 && !(base == 3 && r->path.data[1] == ':'))
        base--;
      /* snprintf precision is int; reject unrepresentable resource/input sizes.
       */
      if (r->name.len > INT_MAX || r->path.len > INT_MAX || base > INT_MAX ||
          body_end - body > INT_MAX || tail - arg > INT_MAX)
        return PG_INVALID;
      const char *fmt = "<skill name=\"%.*s\" location=\"%.*s\">\nReferences "
                        "are relative to %.*s.\n\n%.*s\n</skill>%s%.*s";
      int n =
          snprintf(NULL, 0, fmt, (int)r->name.len, r->name.data,
                   (int)r->path.len, r->path.data, (int)base, r->path.data,
                   (int)(body_end - body), r->body.data + body,
                   tail > arg ? "\n\n" : "", (int)(tail - arg), in.data + arg);
      if (n < 0)
        return PG_INVALID;
      char *text = malloc((size_t)n + 1);
      if (!text)
        return PG_NOMEM;
      snprintf(text, (size_t)n + 1, fmt, (int)r->name.len, r->name.data,
               (int)r->path.len, r->path.data, (int)base, r->path.data,
               (int)(body_end - body), r->body.data + body,
               tail > arg ? "\n\n" : "", (int)(tail - arg), in.data + arg);
      pg_status z = pg_resource_expand(snapshot, (pangu_slice){text, (size_t)n},
                                       out, length);
      free(text);
      return z;
    }
  }
  return pg_resource_expand(snapshot, in, out, length);
}
pg_status pg_agent_resources_expand_active(const pg_agent_resources_run *r,
                                           pangu_slice input, char **out,
                                           size_t *length) {
  if (!r || !out || !length || (!input.data && input.len))
    return PG_INVALID;
  *out = NULL;
  *length = 0;
  if (!r->snapshot) {
    if (input.len == SIZE_MAX)
      return PG_NOMEM;
    *out = malloc(input.len + 1);
    if (!*out)
      return PG_NOMEM;
    if (input.len)
      memcpy(*out, input.data, input.len);
    (*out)[input.len] = 0;
    *length = input.len;
    return PG_OK;
  }
  return expand(r->snapshot, input, out, length);
}
pg_status pg_agent_resources_expand(pg_scope *scope, pangu_slice input,
                                    char **out, size_t *length) {
  if (!out || !length || (!input.data && input.len))
    return PG_INVALID;
  *out = NULL;
  *length = 0;
  pg_lease *lease;
  pg_resource_snapshot *snapshot;
  pg_status z = acquire(scope, &lease, &snapshot);
  if (z == PG_NOT_FOUND) {
    if (input.len == SIZE_MAX)
      return PG_NOMEM;
    *out = malloc(input.len + 1);
    if (!*out)
      return PG_NOMEM;
    if (input.len)
      memcpy(*out, input.data, input.len);
    (*out)[input.len] = 0;
    *length = input.len;
    return PG_OK;
  }
  if (z == PG_OK) {
    z = expand(snapshot, input, out, length);
    pg_resource_snapshot_release(snapshot);
    pg_lease_release(lease);
  }
  return z;
}
pg_status pg_agent_resources_reload(pg_scope *scope) {
  pg_lease *lease = NULL;
  pg_status z =
      pg_service_acquire(scope, PG_NAME(PG_RESOURCES_SERVICE), 1, &lease);
  if (z == PG_OK) {
    const pg_resources_service *api = pg_lease_table(lease);
    z = api->reload(pg_lease_context(lease));
    pg_lease_release(lease);
  }
  return z;
}
static pangu_status request_view(pg_agent_resources_run *r, pangu_core *owner,
                                 const pangu_core *input, pangu_core **out) {
  pangu_status z = pangu_core_create(NULL, out);
  if (z != PANGU_OK)
    return z;
  /* Automatic compaction uses its own Core and source-owned summary prompt.
   * Only the actual session driver receives the conversational system prompt.
   */
  if (owner != r->session->running) {
    for (size_t i = 0; z == PANGU_OK && i < pangu_core_message_count(input);
         i++)
      z = pangu_core_append(*out, pangu_core_message_at(input, i));
    return z;
  }
  pangu_content text = {.kind = PANGU_TEXT,
                        .data = pg_resource_system_prompt(r->snapshot)};
  pangu_message system = {
      .role = PANGU_SYSTEM, .content = &text, .content_count = 1};
  z = pangu_core_append(*out, &system);
  for (size_t i = 0; z == PANGU_OK && i < pangu_core_message_count(input);
       i++) {
    const pangu_message *m = pangu_core_message_at(input, i);
    if (m->role != PANGU_SYSTEM)
      z = pangu_core_append(*out, m);
  }
  return z;
}
static pangu_status request(void *ctx, pangu_core *owner,
                            const pangu_core *input, pangu_model_sink sink,
                            void *data) {
  pg_agent_resources_run *r = ctx;
  pangu_core *view = NULL;
  pangu_status z = request_view(r, owner, input, &view);
  if (z == PANGU_OK)
    z = r->original.request(
        r->original.has_request_ctx ? r->original.request_ctx : r->original.ctx,
        owner, view, sink, data);
  pangu_core_destroy(view);
  return z;
}
static pangu_status events(void *ctx, pangu_core *owner,
                           const pangu_core *input, pangu_model_event_sink sink,
                           void *data) {
  pg_agent_resources_run *r = ctx;
  pangu_core *view = NULL;
  pangu_status z = request_view(r, owner, input, &view);
  if (z == PANGU_OK)
    z = r->original.request_events(
        r->original.has_request_ctx ? r->original.request_ctx : r->original.ctx,
        owner, view, sink, data);
  pangu_core_destroy(view);
  return z;
}
void pg_agent_resources_release(pg_agent_resources_run *r) {
  if (!r)
    return;
  if (r->bound)
    credential_run = r->previous;
  if (r->credentials)
    pg_lease_release(r->credentials);
  free(r->messages);
  pangu_core_destroy(r->prompts);
  yyjson_doc_free(r->settings);
  pg_resource_snapshot_release(r->snapshot);
  if (r->lease)
    pg_lease_release(r->lease);
  free(r);
}
static pangu_slice setting(yyjson_val *root, const char *key) {
  yyjson_val *v = yyjson_obj_get(root, key);
  return yyjson_is_str(v) ? (pangu_slice){yyjson_get_str(v), yyjson_get_len(v)}
                          : (pangu_slice){0};
}
pg_status pg_agent_resources_prepare(pg_scope *scope, pangu_session *session,
                                     pangu_loop_config *cfg,
                                     const pangu_message *input, size_t count,
                                     pg_agent_resources_run **out,
                                     const pangu_message **messages) {
  *out = NULL;
  *messages = input;
  pg_agent_resources_run *r = calloc(1, sizeof(*r));
  if (!r)
    return PG_NOMEM;
  pg_status z = acquire(scope, &r->lease, &r->snapshot);
  if (z != PG_OK && z != PG_NOT_FOUND)
    goto fail;
  if (!cfg->get_api_key) {
    z = pg_service_acquire(scope, PG_NAME(PG_CREDENTIALS_SERVICE), 1,
                           &r->credentials);
    if (z != PG_OK && z != PG_NOT_FOUND)
      goto fail;
    if (z == PG_OK) {
      const pg_credentials_service *api = pg_lease_table(r->credentials);
      if (!api || !api->get_api_key) {
        z = PG_ABI;
        goto fail;
      }
      cfg->get_api_key = credentials_key;
    }
  }
  if (!r->snapshot) {
    *out = r;
    return PG_OK;
  }
  z = PG_OK;
  /* Raw-state legacy model callbacks cannot represent a separate request view
   * with the original cancellation owner. Never silently alter that contract.
   */
  if (!cfg->request && !cfg->request_events) {
    z = PG_INVALID;
    goto fail;
  }
  pangu_slice json = pg_resource_settings(r->snapshot);
  r->settings = yyjson_read(json.data, json.len, 0);
  if (!r->settings) {
    z = PG_INVALID;
    goto fail;
  }
  yyjson_val *root = yyjson_doc_get_root(r->settings);
  if (!cfg->model_config.id.len)
    cfg->model_config.id = setting(root, "defaultModel");
  if (!cfg->model_config.provider.len)
    cfg->model_config.provider = setting(root, "defaultProvider");
  /* Preserve the established CLI provider default when no model was selected.
   */
  if (!cfg->model_config.id.len && cfg->model_config.provider.len == 8 &&
      !memcmp(cfg->model_config.provider.data, "deepseek", 8))
    cfg->model_config.id = (pangu_slice){"deepseek-chat", 13};
  if (cfg->thinking_level == PANGU_THINKING_OFF) {
    pangu_slice level = setting(root, "defaultThinkingLevel");
    const char *names[] = {"off",  "minimal", "low", "medium",
                           "high", "xhigh",   "max"};
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++)
      if (level.len == strlen(names[i]) &&
          !memcmp(level.data, names[i], level.len))
        cfg->thinking_level = (pangu_thinking_level)i;
  }
  r->session = session;
  r->original = *cfg;
  cfg->has_request_ctx = 1;
  cfg->request_ctx = r;
  if (cfg->request_events)
    cfg->request_events = events;
  else
    cfg->request = request;
  if (count) {
    if (!input || count > SIZE_MAX / sizeof(*r->messages)) {
      z = PG_INVALID;
      goto fail;
    }
    if (pangu_core_create(NULL, &r->prompts) != PANGU_OK) {
      z = PG_NOMEM;
      goto fail;
    }
    r->messages = calloc(count, sizeof(*r->messages));
    if (!r->messages) {
      z = PG_NOMEM;
      goto fail;
    }
    for (size_t i = 0; i < count; i++) {
      pangu_message m = input[i];
      pangu_content *content = NULL;
      char **expanded = NULL;
      if (m.role == PANGU_USER && m.content_count) {
        if (!m.content || m.content_count > SIZE_MAX / sizeof(*content) ||
            m.content_count > SIZE_MAX / sizeof(*expanded)) {
          z = PG_INVALID;
          goto fail;
        }
        content = malloc(m.content_count * sizeof(*content));
        expanded = calloc(m.content_count, sizeof(*expanded));
        if (!content || !expanded) {
          free(content);
          free(expanded);
          z = PG_NOMEM;
          goto fail;
        }
        memcpy(content, m.content, m.content_count * sizeof(*content));
        for (size_t j = 0; j < m.content_count; j++) {
          if (content[j].kind == PANGU_TEXT) {
            z = expand(r->snapshot, content[j].data, &expanded[j],
                       &content[j].data.len);
            if (z != PG_OK)
              break;
            content[j].data.data = expanded[j];
          }
        }
        m.content = content;
      }
      if (z == PG_OK) {
        pangu_status st = pangu_core_append(r->prompts, &m);
        z = st == PANGU_OK ? PG_OK : st == PANGU_NOMEM ? PG_NOMEM : PG_INVALID;
      }
      if (expanded)
        for (size_t j = 0; j < m.content_count; j++)
          free(expanded[j]);
      free(expanded);
      free(content);
      if (z != PG_OK)
        goto fail;
      r->messages[i] = *pangu_core_message_at(r->prompts, i);
    }
    *messages = r->messages;
  }
  *out = r;
  return PG_OK;
fail:
  pg_agent_resources_release(r);
  return z;
}
