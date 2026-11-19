#ifndef PANGU_CORE_INTERNAL_H
#define PANGU_CORE_INTERNAL_H
#include <pangu/core/state.h>
typedef struct pangu_subscription pangu_subscription;
struct pangu_core {
    pangu_allocator memory;
    pangu_message **messages;
    size_t count, capacity;
    int running, cancelled;
    pangu_model_config state_model;
    void *state_model_storage;
    pangu_thinking_level state_thinking;
    pangu_tool *state_tools;
    size_t state_tool_count;
    const pangu_loop_config *active_config;
    pangu_slice active_api_key;
    pangu_request_fn default_request;
    void *default_request_ctx;
    pangu_core *queues[2];
    pangu_queue_mode queue_modes[2];
    pangu_message **held_messages;
    size_t held_count, held_capacity;
    pangu_queue_kind held_kind;
    pangu_message **prepared_messages;
    size_t prepared_count,prepared_capacity;
    pangu_subscription *subscriptions, *subscription_tail;
    uint64_t next_subscription;
    unsigned dispatch_depth;
    const pangu_message *streaming;
    pangu_slice error;
    void *retained_error; /* replace_messages-owned diagnostic */
    pangu_slice failure_detail; /* owned, current run only */
    void *active_tool_tasks;
    size_t active_tool_count;
};
pangu_status pangu_core_append_internal(pangu_core *, const pangu_message *);
pangu_status pangu_core_move_prefix(pangu_core *, pangu_core *, size_t);
void pangu_core_dispatch(pangu_core *, const pangu_event *);
void pangu_core_state_destroy(pangu_core *);
void pangu_core_state_init(pangu_core *);
pangu_status pangu_core_declare_tools(pangu_core *,pangu_core *,const pangu_tool *,size_t);
uint64_t pangu_core_monotonic_ns(void);
int64_t pangu_core_wallclock_ms(void);
pangu_status pangu_core_validate_json(pangu_core *,pangu_slice,int require_object);
#endif
