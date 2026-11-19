#ifndef PANGU_CORE_H
#define PANGU_CORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* All slices carry byte lengths; NULL is valid only for length zero. No UTF-8
 * normalization is performed. Inputs are borrowed for the duration of a call.
 * A core is single-owner: serialize all calls, including destruction. */
typedef struct { const char *data; size_t len; } pangu_slice;
typedef enum { PANGU_OK, PANGU_INVALID, PANGU_NOMEM, PANGU_BUSY,
    PANGU_CANCELLED, PANGU_MODEL_ERROR, PANGU_TOOL_ERROR } pangu_status;
typedef enum { PANGU_SYSTEM, PANGU_USER, PANGU_ASSISTANT, PANGU_TOOL_RESULT,
    PANGU_CUSTOM } pangu_role;
typedef enum { PANGU_TEXT, PANGU_THINKING, PANGU_IMAGE, PANGU_TOOL_CALL,
    PANGU_JSON } pangu_content_kind;
typedef enum { PANGU_PENDING, PANGU_STOP, PANGU_LENGTH, PANGU_TOOL_USE,
    PANGU_ERROR, PANGU_ABORTED, PANGU_DEFERRED } pangu_stop_reason;
typedef enum { PANGU_THINKING_OFF, PANGU_THINKING_MINIMAL, PANGU_THINKING_LOW,
    PANGU_THINKING_MEDIUM, PANGU_THINKING_HIGH, PANGU_THINKING_XHIGH,
    PANGU_THINKING_MAX } pangu_thinking_level;
typedef struct {
    pangu_slice id,provider,api;
    pangu_slice metadata_json; /* host model capabilities/options, encoded JSON */
} pangu_model_config;
typedef struct {
    pangu_content_kind kind;
    pangu_slice data; /* text, thinking, base64 image, or JSON */
    pangu_slice mime_type, id, name, signature;
    /* Optional content signature/namespace/arguments_error distinguish absent
     * (NULL,0) from explicitly empty (non-NULL,0), including owned copies. */
    pangu_slice tool_namespace, arguments_error; /* tool-call transport metadata */
    int has_redacted, redacted; /* thinking: absent differs from explicit false */
} pangu_content;
typedef struct {
    uint64_t input, output, cache_read, cache_write, total;
    double input_cost, output_cost, cache_read_cost, cache_write_cost, total_cost;
    /* Optional subtotals are subsets, never added again to output/cache_write.
     * Presence preserves explicit zero/false versus provider omission. */
    int has_cache_write_1h, has_reasoning, has_total_from_provider;
    uint64_t cache_write_1h, reasoning;
    int total_from_provider;
} pangu_usage;
typedef struct {
    pangu_slice name, value;
    int removed; /* sections use explicit deletion, distinct from empty value */
} pangu_system_section;
typedef struct {
    pangu_slice name, description, schema_json;
    int has_constrained_sampling, constrained_sampling;
} pangu_tool_declaration;
enum {
    PANGU_MESSAGE_HAS_THINKING_LEVEL=1u,
    PANGU_MESSAGE_HAS_USAGE=2u,
    PANGU_MESSAGE_HAS_SECTIONS=4u,
    PANGU_MESSAGE_HAS_TOOLS_ADDED=8u,
    PANGU_MESSAGE_HAS_TOOLS_REMOVED=16u,
    PANGU_MESSAGE_CONTENT_STRING=32u
};
typedef struct {
    pangu_role role;
    const pangu_content *content;
    size_t content_count;
    pangu_slice custom_role, tool_call_id, tool_name, model, provider, api;
    pangu_slice error, details_json, structured_json;
    pangu_slice provider_thinking_level; /* actual provider level, distinct from requested */
    pangu_usage usage;
    pangu_stop_reason stop_reason;
    pangu_thinking_level thinking_level;
    int is_error, terminate;
    int64_t timestamp_ms; /* UTC milliseconds. Inputs/provider snapshots retain
                           their value; Core-created transcript messages use
                           the wall clock, independently of tool duration. */
    int has_duration;
    uint64_t duration_ms;
    const pangu_system_section *sections;
    size_t section_count;
    const pangu_tool_declaration *tools_added;
    size_t tools_added_count;
    const pangu_slice *tools_removed;
    size_t tools_removed_count;
    /* Provider-reported response identity and native termination reason.
     * NULL/0 means absent; non-NULL/0 preserves explicitly empty strings.
     * Borrowed on input; append/model sinks deep-copy.
     * model remains the requested model; these do not change loop control. */
    pangu_slice response_model, response_id, raw_stop_reason;
    /* Optional source metadata, encoded JSON: diagnostics is an array;
     * deferred and nestedCalls are objects. Empty slices mean absent, while
     * []/{} remain present. Core copies bytes without interpretation; proxy
     * serialization validates JSON and its array/object root shape.
     * Provider-specific deferred.data and
     * diagnostic details retain their JSON representation. Redaction belongs
     * to the producer; never put credentials in these persisted fields. */
    pangu_slice diagnostics_json, deferred_json, nested_calls_json;
    int has_end_turn, end_turn; /* absent differs from explicit false */
    /* Optional wire fields: explicit OFF/zero usage/empty system containers
     * require their HAS bit. Non-OFF thinking, nonzero usage and nonempty
     * containers remain implicitly present for existing callers. Assistant
     * usage is required and always present; tool-result usage may be absent.
     * CONTENT_STRING selects system/user string content (0/1 text blocks,
     * no signature); otherwise content is an array. Other bits/role mixtures
     * are INVALID. Copies, events and persistence retain this mask. */
    uint32_t presence;
} pangu_message;
/* Allocators must be paired and remain alive until core destruction. */
typedef void *(*pangu_alloc_fn)(void *ctx, size_t bytes);
typedef void (*pangu_free_fn)(void *ctx, void *ptr);
typedef struct { void *ctx; pangu_alloc_fn alloc; pangu_free_fn free; } pangu_allocator;
typedef struct pangu_core pangu_core;
typedef struct { pangu_allocator allocator; } pangu_core_config;
/* On failure *out is NULL. A NULL config selects malloc/free. */
pangu_status pangu_core_create(const pangu_core_config *config, pangu_core **out);
/* Destruction during a callback is ignored. Destroy after the run settles. */
void pangu_core_destroy(pangu_core *core);
/* Deep-copy append is transactional, including allocation failure. */
pangu_status pangu_core_append(pangu_core *core, const pangu_message *message);
/* Borrowed snapshot survives append; reset/destroy invalidates all views.
 * The pointer array itself is private; fetch individual messages by index. */
size_t pangu_core_message_count(const pangu_core *core);
const pangu_message *pangu_core_message_at(const pangu_core *core, size_t index);
/* Reset retains a replayed system prompt/section/tool baseline, clears queues
 * and runtime errors, and preserves subscribers. Allocation failure is atomic. */
pangu_status pangu_core_reset(pangu_core *core);
/* Clear all transcript messages, queues and errors without retaining baseline. */
pangu_status pangu_core_clear(pangu_core *core);
#ifdef __cplusplus
}
#endif
#endif
