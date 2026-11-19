#ifndef PANGU_CORE_LOOP_H
#define PANGU_CORE_LOOP_H
#include <pangu/core/core.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Identity fields merge independently with the default. Attribution is replaced
 * as a whole; a present attribution with attribution_enabled=0 is explicit null.
 * All slices/arrays are borrowed, immutable and valid through run return.
 * URL may be empty: hosts supply their own published URL. Providers map these
 * fields to protocol headers; Core does not send network headers itself. */
enum { PANGU_IDENTITY_USER_AGENT=1u, PANGU_IDENTITY_ATTRIBUTION=2u,
       PANGU_IDENTITY_VERSION=4u };
typedef enum { PANGU_USER_AGENT_DETAILED, PANGU_USER_AGENT_SHORT,
               PANGU_USER_AGENT_CUSTOM } pangu_user_agent_mode;
typedef struct {
    unsigned fields;
    pangu_user_agent_mode user_agent;
    pangu_slice custom_user_agent, version;
    int attribution_enabled;
    pangu_slice name, url;
    const pangu_slice *categories;
    size_t category_count;
} pangu_client_identity;
/* Immutable process-lifetime PanGu attribution; no allocation or release. */
const pangu_client_identity *pangu_core_default_identity(void);
/* Borrowed merged identity during run callbacks; NULL when idle. */
const pangu_client_identity *pangu_core_active_identity(const pangu_core *);
/* Synchronous native adapter: every callback runs on the run caller's thread.
 * All callbacks and their contexts remain alive through run return. Callbacks
 * may inspect state and cancel, but run/reset/append return BUSY during a run.
 * No callback may destroy the core. Synchronous providers cannot retain sinks;
 * async tool sinks follow the explicit start/release lifetime below.
 * Network providers may drive their own event loop inside the callback. */
typedef enum {
    PANGU_AGENT_START, PANGU_AGENT_END, PANGU_TURN_START, PANGU_TURN_END,
    PANGU_MESSAGE_START, PANGU_MESSAGE_UPDATE, PANGU_MESSAGE_END,
    PANGU_TOOL_START, PANGU_TOOL_UPDATE, PANGU_TOOL_END
} pangu_event_kind;
/* Pi AssistantMessageEvent tags. Snapshot is supplied separately to the sink.
 * Slices and tool_call are borrowed only through the synchronous callback. */
typedef enum {
    PANGU_ASSISTANT_START, PANGU_ASSISTANT_TEXT_START, PANGU_ASSISTANT_TEXT_DELTA,
    PANGU_ASSISTANT_TEXT_END, PANGU_ASSISTANT_THINKING_START,
    PANGU_ASSISTANT_THINKING_DELTA, PANGU_ASSISTANT_THINKING_END,
    PANGU_ASSISTANT_TOOL_START, PANGU_ASSISTANT_TOOL_DELTA,
    PANGU_ASSISTANT_TOOL_END, PANGU_ASSISTANT_DONE, PANGU_ASSISTANT_ERROR
} pangu_assistant_kind;
typedef struct {
    pangu_assistant_kind kind;
    size_t content_index;
    pangu_slice delta, content; /* delta tags / text and thinking end tags */
    const pangu_content *tool_call; /* tool end only */
    pangu_stop_reason reason; /* done/error only */
} pangu_assistant_event;
typedef struct {
    pangu_event_kind kind;
    const pangu_assistant_event *assistant_event; /* message_update; NULL for legacy snapshots */
    const pangu_message *message; /* borrowed for callback duration */
    const pangu_content *tool_call;
    int has_duration;
    uint64_t duration_ms; /* tool_end only; excludes after hook */
    size_t first_message, message_count;
    /* At agent_end, new_messages is an independent immutable view of messages
     * produced by this invocation, including prompts, even if context was
     * replaced. A stateful run preparation/transform/conversion/key-hook failure reports only
     * its synthetic assistant error, matching the Agent failure lifecycle.
     * Low-level loops return preparation errors without synthesizing a message.
     * first_message is its zero-based offset (always zero).
     * Borrowed only through the event callback; copy to retain. */
    const pangu_core *new_messages;
    /* turn_end only: this turn's finalized tool artifacts in assistant source
     * order. Empty view on error/aborted turns. Borrowed through callback. */
    const pangu_core *tool_results;
} pangu_event;
typedef void (*pangu_event_fn)(void *ctx, pangu_core *, const pangu_event *);
/* Model submits cumulative assistant snapshots, exactly one final snapshot.
 * The first snapshot emits message_start; later nonfinal snapshots emit
 * message_update. A final-only response emits start/end without update.
 * Each submission is deep-copied before return. A provider must stop on a
 * non-OK sink return. Runtime failure may be returned or encoded as ERROR. */
typedef pangu_status (*pangu_model_sink)(void *, const pangu_message *, int final);
typedef pangu_status (*pangu_model_fn)(void *ctx, pangu_core *transcript,
    pangu_model_sink sink, void *sink_ctx);
/* Providers that consume a transformed/converted request receive immutable
 * request messages separately from the owner used for state/cancellation.
 * Request views expire on provider return. transform/convert write deep copies
 * to output using pangu_core_append; errors must be returned, never thrown. */
typedef pangu_status (*pangu_request_fn)(void *ctx,pangu_core *owner,
    const pangu_core *request,pangu_model_sink,void *sink_ctx);
/* Preferred event-preserving provider. START precedes content events; DONE or
 * ERROR settles the response. Each message is copied before sink return;
 * metadata remains borrowed only during dispatch. Stop on a non-OK result.
 * Content events before START are ignored, matching Pi. Legacy request/model
 * adapters keep their cumulative-snapshot semantics. request_events wins. */
typedef pangu_status (*pangu_model_event_sink)(void *,const pangu_message *,
    const pangu_assistant_event *);
typedef pangu_status (*pangu_request_events_fn)(void *,pangu_core *owner,
    const pangu_core *request,pangu_model_event_sink,void *sink_ctx);
/* Context/conversion outputs are temporary owned containers, released even on
 * failure. Callbacks borrow input and output only until return, on the run
 * thread. Non-OK prevents provider/finish_turn calls. Stateful run emits an
 * assistant error lifecycle (except NOMEM); low-level loops return the status.
 * Returning CANCELLED or host cancellation yields ABORTED/CANCELLED for
 * stateful run; returning a status never sets the host cancel flag. Other statuses are
 * preserved; generic stage diagnostics replace JS exception text. */
typedef pangu_status (*pangu_context_fn)(void *ctx,pangu_core *owner,
    const pangu_core *input,pangu_core *output);
/* Tool sink accepts partial updates and exactly one final TOOL_RESULT.
 * id/name are supplied by Core, regardless of the provider's result values. */
typedef pangu_status (*pangu_tool_sink)(void *, const pangu_message *, int final);
typedef pangu_status (*pangu_tool_fn)(void *ctx, pangu_core *,
    const pangu_content *call, pangu_tool_sink, void *sink_ctx);
/* A mature schema validator is supplied by the host. It must validate the
 * call's complete JSON arguments against this tool's schema. A missing
 * validator is rejected; Core never silently treats unvalidated input as safe. */
typedef pangu_status (*pangu_validate_fn)(void *ctx, pangu_slice schema_json,
    pangu_slice args_json);
/* prepareArguments submits exactly one replacement JSON value; Core copies it
 * before the sink returns. The raw tool-call block remains unchanged in events.
 * A sink failure is sticky through callback return; stop submitting on failure. */
typedef pangu_status (*pangu_arguments_sink)(void *,pangu_slice);
typedef pangu_status (*pangu_arguments_fn)(void *,pangu_slice raw_json,
    pangu_arguments_sink,void *sink_ctx);
/* Normalizing validator is preferred over validate: it submits the validated,
 * coerced JSON used by hooks and execute, matching Pi's clone/convert path. */
typedef pangu_status (*pangu_validate_arguments_fn)(void *,pangu_slice schema_json,
    pangu_slice args_json,pangu_arguments_sink,void *sink_ctx);
/* Preferred owner-aware normalization: raw call/name and prepared args are
 * borrowed until synchronous return on the run thread. Same sink ownership,
 * exactly-one success and sticky-failure rules as validate_arguments. May
 * query cancellation or report a detailed failure with core_report_error;
 * NOMEM must propagate. No IO or tool side effects before validation succeeds. */
typedef pangu_status (*pangu_validate_call_fn)(void *,pangu_core *,
    const pangu_content *call,pangu_slice schema_json,pangu_slice args_json,
    pangu_arguments_sink,void *sink_ctx);
/* Asynchronous tools retain the sink until release. start returns an owned
 * operation handle; all sink calls must be dispatched by pump on the run
 * caller's thread. A final sink call settles the operation. On start failure
 * no sink may be retained; a non-NULL handle still receives release.
 * release must quiesce/cancel and join the operation,
 * without invoking the sink, even if it has not completed. */
typedef pangu_status (*pangu_tool_start_fn)(void *, pangu_core *,
    const pangu_content *, pangu_tool_sink, void *sink_ctx, void **operation);
typedef void (*pangu_tool_release_fn)(void *, void *operation);
typedef enum { PANGU_TOOLS_PARALLEL, PANGU_TOOLS_SEQUENTIAL } pangu_tool_mode;
typedef struct {
    pangu_slice name, description, schema_json;
    void *ctx;
    pangu_validate_fn validate;
    pangu_arguments_fn prepare_arguments;
    pangu_validate_arguments_fn validate_arguments;
    pangu_tool_fn execute;
    pangu_tool_start_fn start;
    pangu_tool_release_fn release;
    pangu_tool_mode execution_mode; /* first matching name governs execution
        and mode; a selected sequential tool serializes the entire batch */
    int has_constrained_sampling, constrained_sampling;
    pangu_validate_call_fn validate_call; /* preferred over other validators */
} pangu_tool;
/* Legacy return-by-value hooks borrow reason through run return. Prefer the
 * update sink below for callback-local buffers that must be copied. */
typedef struct { int block, terminate; pangu_slice reason; } pangu_before_result;
typedef pangu_before_result (*pangu_before_fn)(void *, pangu_core *,
    const pangu_message *assistant, const pangu_content *call);
/* after runs only for executed tools, receives immutable result, and may
 * replace it by submitting a complete final result to the supplied sink.
 * Sink failures remain sticky even if the hook ignores them or submits again. */
typedef pangu_status (*pangu_after_fn)(void *, pangu_core *, const pangu_content *,
    const pangu_message *result, pangu_tool_sink, void *sink_ctx);
typedef struct {
    const pangu_message *assistant;
    const pangu_content *call; /* raw call */
    pangu_slice args_json; /* prepared/validated input */
    const pangu_message *result; /* after hook only */
} pangu_tool_context;
typedef pangu_before_result (*pangu_before_context_fn)(void *,pangu_core *,const pangu_tool_context *);
/* Preferred before hook: explicitly mutate the validated args object without
 * schema revalidation, matching Pi beforeToolCall's mutable args. Raw call and
 * assistant remain unchanged; execute/after receive replacement args_json.
 * Submit zero/one update. Sink copies reason/arguments before return; all input
 * views remain valid through hook return. JSON syntax/object shape is checked
 * with yyjson, not against tool Schema. Failure creates a preflight error;
 * NOMEM stops the run. All callbacks/sinks use the run thread, no retention. */
typedef struct {
    pangu_before_result result;
    int has_arguments;
    pangu_slice arguments_json;
} pangu_before_update;
typedef pangu_status (*pangu_before_update_sink)(void *,const pangu_before_update *);
typedef pangu_status (*pangu_before_update_fn)(void *,pangu_core *,
    const pangu_tool_context *,pangu_before_update_sink,void *sink_ctx);
enum {
    PANGU_PATCH_CONTENT=1u, PANGU_PATCH_DETAILS=2u, PANGU_PATCH_STRUCTURED=4u,
    PANGU_PATCH_USAGE=8u, PANGU_PATCH_ERROR=16u, PANGU_PATCH_TERMINATE=32u
};
typedef struct {
    unsigned fields; /* explicit presence, allowing empty content / false */
    const pangu_content *content;
    size_t content_count;
    pangu_slice details_json,structured_json;
    pangu_usage usage;
    int is_error,terminate;
} pangu_tool_patch;
typedef pangu_status (*pangu_patch_sink)(void *,const pangu_tool_patch *);
/* Omitted fields preserve the executed result. Content replacement drops
 * structured JSON unless PATCH_STRUCTURED is also supplied. Exactly zero or
 * one patch may be submitted; all borrowed data is copied by the sink. */
typedef pangu_status (*pangu_after_patch_fn)(void *,pangu_core *,const pangu_tool_context *,
    pangu_patch_sink,void *sink_ctx);
typedef enum { PANGU_TURN_AUTO, PANGU_TURN_CONTINUE, PANGU_TURN_END_RUN } pangu_turn_decision;
typedef struct {
    const pangu_message *assistant;
    const pangu_core *tool_results;
    const pangu_core *new_messages; /* full invocation artifacts so far */
} pangu_turn_context;
typedef pangu_turn_decision (*pangu_finish_context_fn)(void *,pangu_core *,const pangu_turn_context *);
typedef pangu_turn_decision (*pangu_finish_fn)(void *, pangu_core *, const pangu_message *);
typedef pangu_status (*pangu_prepare_fn)(void *, pangu_core *);
enum { PANGU_UPDATE_CONTEXT=1u, PANGU_UPDATE_PROVIDER=2u, PANGU_UPDATE_MESSAGES=4u, PANGU_UPDATE_TOOLS=8u, PANGU_UPDATE_MODEL=16u, PANGU_UPDATE_THINKING=32u };
typedef struct {
    unsigned fields;
    const pangu_core *context; /* copied transcript, may be empty */
    pangu_request_fn request;
    pangu_request_events_fn request_events;
    void *request_ctx; /* borrowed through run return */
    const pangu_core *messages; /* additional messages, next-turn hook only */
    const pangu_tool *tools;
    size_t tool_count; /* TOOLS presence permits an empty replacement set */
    pangu_model_config model;
    pangu_thinking_level thinking_level;
} pangu_request_update;
typedef pangu_status (*pangu_request_update_sink)(void *,const pangu_request_update *);
/* Submit at most one update. All context messages are copied before sink
 * return. The update commits only when the hook returns OK; failures leave
 * runtime context/provider unchanged. Context replacement invalidates borrowed
 * transcript views and emits no message lifecycle events. Provider replacement
 * persists for later requests in this run. Next-turn updates may also append
 * messages with normal lifecycle events before queued input; request updates
 * reject UPDATE_MESSAGES. Tool arrays and declaration strings are deep-copied;
 * callback contexts remain borrowed until run return. Updates occur between
 * batches, after all prior tool operations have quiesced. Request-hook tool
 * changes execute immediately; transcript declarations update at the next
 * declaration point, matching Pi prepareRequest ordering.
 * Hooks must not retain the sink. */
typedef pangu_status (*pangu_prepare_update_fn)(void *,pangu_core *,
    pangu_request_update_sink,void *sink_ctx);
/* Provider-specific parsed event observer, before assistant normalization.
 * Native adapters encode unknown data as JSON using their mature JSON library.
 * Data/model borrowed only through return; callbacks run on the run thread.
 * Adapter support is explicit: proxy transport does not invent raw events.
 * Non-OK propagates to the adapter, which must stop its request. */
typedef pangu_status (*pangu_provider_event_fn)(void *,pangu_core *,
    const pangu_model_config *,pangu_slice event_json);
/* Resolve immediately after context conversion on every request. Submit zero
 * or one key; empty/absent uses config.api_key. Sink copies bytes before return.
 * Resolver and sink run on the run thread; do not retain sink. Errors stop the
 * request. Resolver ctx is config.ctx, provider is current model.provider. */
typedef pangu_status (*pangu_api_key_sink)(void *,pangu_slice);
typedef pangu_status (*pangu_api_key_fn)(void *,pangu_core *,pangu_slice provider,
    pangu_api_key_sink,void *sink_ctx);
/* Provider payloads use JSON byte values (including null/scalars), not mutable
 * JS objects. No submission preserves input; one submission replaces it, even
 * with JSON null. Sink copies before return. ctx is config.ctx, same run thread.
 * Native mutation is expressed by an explicit replacement. */
typedef pangu_status (*pangu_payload_sink)(void *,pangu_slice json);
typedef pangu_status (*pangu_payload_fn)(void *,pangu_core *,
    const pangu_model_config *,pangu_slice json,pangu_payload_sink,void *sink_ctx);
typedef struct { pangu_slice name,value; } pangu_response_header;
typedef struct {
    unsigned status;
    const pangu_response_header *headers;
    size_t header_count;
} pangu_provider_response;
/* Called by supported direct adapters after headers, before consuming body.
 * Response/model/header slices borrowed only until return. Non-OK stops request. */
typedef pangu_status (*pangu_response_fn)(void *,pangu_core *,
    const pangu_model_config *,const pangu_provider_response *);
typedef struct {
    void *ctx;
    pangu_model_fn model; /* legacy raw-state adapter, used when request is NULL */
    pangu_request_fn request;
    pangu_request_events_fn request_events;
    int has_request_ctx;
    void *request_ctx; /* separate provider context when has_request_ctx */
    pangu_context_fn transform_context, convert_to_model;
    pangu_model_config model_config; /* borrowed for the run */
    pangu_thinking_level thinking_level;
    pangu_client_identity identity; /* field-wise override of default identity */
    /* Borrowed JSON object (or empty), immutable through run return. Contains
     * sessionId, sampling, timeout and provider extension options. Providers
     * validate/consume their supported fields; callbacks remain typed below. */
    pangu_slice request_options_json;
    pangu_provider_event_fn on_provider_event; /* uses config.ctx */
    pangu_slice api_key; /* immutable borrowed fallback through run return */
    pangu_api_key_fn get_api_key;
    pangu_payload_fn on_payload;
    pangu_response_fn on_response;
    pangu_event_fn event;
    const pangu_tool *tools;
    size_t tool_count;
    pangu_before_fn before_tool;
    pangu_after_fn after_tool; /* legacy full-result replacement adapter */
    pangu_before_context_fn before_tool_context;
    pangu_before_update_fn before_tool_update; /* preferred over older before hooks */
    pangu_after_patch_fn after_tool_patch; /* preferred over legacy callback */
    pangu_prepare_fn prepare_request;
    pangu_prepare_update_fn prepare_request_update; /* preferred over legacy */
    pangu_finish_fn finish_turn;
    pangu_finish_context_fn finish_turn_context; /* preferred over legacy */
    pangu_prepare_update_fn prepare_next_turn_update;
    pangu_prepare_fn prepare_next_turn; /* after turn_end, before next turn_start */
    pangu_tool_mode tool_execution; /* zero/default is parallel, matching Pi */
    /* Wait for and dispatch at least one asynchronous update/completion.
     * Called only while unfinished operations exist. Return failures to stop
     * the batch; Core releases every started operation before returning. */
    pangu_status (*pump)(void *ctx, pangu_core *);
    uint64_t (*monotonic_ns)(void *ctx); /* optional deterministic host clock */
    /* Stateful core_run inherits idle instance model/thinking/tools when the
     * corresponding cfg value is absent. Nonempty model slices, non-OFF
     * thinking, or a non-NULL tools/count remain implicit overrides for
     * existing callers. Set these bits to explicitly override with an empty
     * model, OFF, or no tools. Overrides/prepare updates never mutate stored
     * instance settings. Low-level loop entries use cfg directly. */
    unsigned override_fields;
} pangu_loop_config;
enum {
    PANGU_RUN_OVERRIDE_MODEL=1u,
    PANGU_RUN_OVERRIDE_THINKING=2u,
    PANGU_RUN_OVERRIDE_TOOLS=4u
};
/* Parallel batches preflight in source order, then start executable calls.
 * tool_end follows completion dispatch order; result messages follow source
 * order. Synchronous executors also work in either mode. Async tools require
 * start/release and pump. Queue/state controls are declared in state.h.
 * Request transforms run before model conversion; by default custom-role
 * messages are filtered at the request boundary. Transforms require request.
 * prompts are appended with message lifecycle events. count=0 continues;
 * continuation requires conversational history; an assistant tail requires
 * queued input, with steering selected before follow-up.
 * Model id/provider/api/metadata slices require non-NULL data for nonzero
 * lengths. Invalid configuration returns INVALID before events/provider calls
 * or transcript mutation. Metadata JSON semantics belong to the provider.
 * Provider errors become transcript artifacts and normal lifecycle events.
 * A provider returning CANCELLED yields an ABORTED artifact even if the host
 * cancel flag is unset; existing terminal ERROR/ABORTED snapshots remain authoritative.
 * Allocation failure returns NOMEM with the committed prefix preserved. */
/* Current per-run request configuration, available to prepare hooks and model
 * providers. Borrowed through the next committed preparation update or run
 * return. NULL model when idle; OFF thinking when idle. MODEL updates copy
 * all slices before the sink returns; THINKING explicitly accepts OFF. */
/* Borrowed original provider options while running, empty when idle. Core
 * checks pointer/length; the selected provider validates JSON and option types.
 * No credentials or local fields are automatically put on a network request. */
/* Resolved key is borrowed only during the provider request callback; empty
 * during preparation/resolution/idle. Resolver-owned copies are wiped/released
 * before request return. Never persist this view in transcript/event data. */
pangu_slice pangu_core_active_api_key(const pangu_core *);
pangu_slice pangu_core_active_options(const pangu_core *);
/* Adapter entry for on_provider_event. INVALID outside a run or for invalid
 * pointer/length, OK when no observer is installed. Does not retain event data. */
/* Direct adapter helpers, INVALID outside run. prepare_payload validates JSON
 * using yyjson and returns an owned single JSON block in *out, or NULL on error.
 * Destroy output after sending; allocator context must outlive it. Replacement
 * commits only if callback returns OK; duplicate/invalid/OOM sink is sticky.
 * observe_response does not retain data and is OK without an installed hook.
 * Proxy adapters do not invoke these unsupported provider-specific hooks. */
pangu_status pangu_core_prepare_payload(pangu_core *,pangu_slice,pangu_core **out);
pangu_status pangu_core_observe_response(pangu_core *,const pangu_provider_response *);
pangu_status pangu_core_observe_provider_event(pangu_core *,pangu_slice event_json);
const pangu_model_config *pangu_core_active_model(const pangu_core *);
/* Effective executable tools, including committed preparation replacements.
 * Borrowed through next committed preparation update or run return (including
 * agent_end listeners). Empty/NULL outside a model run. */
size_t pangu_core_active_tool_count(const pangu_core *);
const pangu_tool *pangu_core_active_tool_at(const pangu_core *,size_t);
pangu_thinking_level pangu_core_active_thinking(const pangu_core *);
/* Install an instance-scoped fallback stream. Unlike Pi's module-global
 * default, scope is explicit so independent embedded hosts cannot overwrite
 * each other. Borrowed ctx lives until replacement/destruction. NULL clears.
 * Replacement while running returns BUSY, so return is the release barrier for
 * the old context. Reset/clear retain this provider. Explicit model/request wins. */
pangu_status pangu_core_set_default_stream(pangu_core *,pangu_request_fn,void *ctx);
pangu_status pangu_core_run(pangu_core *, const pangu_message *prompts,
    size_t count, const pangu_loop_config *);
/* Low-level counterparts of runAgentLoop/runAgentLoopContinue. loop permits
 * an empty prompt and empty/system/assistant transcript. loop_continue rejects
 * empty context and an assistant tail even when queues contain input; a
 * system-only context is allowed. core_run retains Agent wrapper validation.
 * All three share execution, callbacks and ownership; owner is the mutable
 * working transcript and receives committed output. */
pangu_status pangu_core_loop(pangu_core *,const pangu_message *,size_t,const pangu_loop_config *);
pangu_status pangu_core_loop_continue(pangu_core *,const pangu_loop_config *);
typedef struct {
    pangu_core *result; /* owned, exactly one rich TOOL_RESULT; destroy after use */
    int has_duration;
    uint64_t duration_ms;
} pangu_tool_outcome;
/* Standalone/nested invocation uses the same preparation/execution/hooks as a
 * model call. It emits no agent events, appends no transcript messages, and
 * never changes the outer run's pending-tool Set, even for a shared call ID.
 * updates receives partial results only; it must not retain sink inputs.
 * Tool failures return OK with result.is_error; infrastructure errors return
 * status with out zeroed. The allocator context must outlive out->result.
 * Allowed inside running tools/hooks. Cancellation is inherited; async
 * operations are quiesced before return. */
pangu_status pangu_core_call_tool(pangu_core *,const pangu_message *assistant,
    const pangu_content *call,const pangu_loop_config *,pangu_tool_sink updates,
    void *update_ctx,pangu_tool_outcome *out);
/* Cooperative, same-thread cancellation; provider must query during long work.
 * Cancellation is cleared at the next run. is_running includes agent_end. */
void pangu_core_cancel(pangu_core *);
int pangu_core_is_cancelled(const pangu_core *);
int pangu_core_is_running(const pangu_core *);
#ifdef __cplusplus
}
#endif
#endif
