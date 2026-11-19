#ifndef PANGU_CORE_STATE_H
#define PANGU_CORE_STATE_H
#include <pangu/core/loop.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { PANGU_STEERING, PANGU_FOLLOW_UP } pangu_queue_kind;
typedef enum { PANGU_QUEUE_ONE, PANGU_QUEUE_ALL } pangu_queue_mode;
/* Persistent instance configuration, same-thread only. Borrowed views survive
 * run/reset/clear, and expire on successful replacement of that field/destroy.
 * NULL owner yields NULL/OFF/zero; out-of-range tool indices yield NULL. */
const pangu_model_config *pangu_core_model(const pangu_core *);
pangu_thinking_level pangu_core_thinking(const pangu_core *);
size_t pangu_core_tool_count(const pangu_core *);
const pangu_tool *pangu_core_tool_at(const pangu_core *,size_t);
/* Idle-only transactional setters: BUSY throughout run/agent_end callbacks;
 * INVALID for invalid pointers/lengths or thinking enum, NOMEM preserves old
 * state. Model slices, tool tables and metadata are deep-copied. Function
 * pointers are copied by value; tool ctx is borrowed until successful tools
 * replacement or destruction. No retain/release callbacks are invoked.
 * Inputs may alias current views. No events are emitted. Reset/clear preserve
 * configuration. Default model id/provider/api are unknown, thinking OFF,
 * tools empty; metadata follows the fixed Pi DEFAULT_MODEL literal. */
pangu_status pangu_core_set_model(pangu_core *,const pangu_model_config *);
pangu_status pangu_core_set_thinking(pangu_core *,pangu_thinking_level);
pangu_status pangu_core_set_tools(pangu_core *,const pangu_tool *,size_t);
/* Idle-only transactional deep-copy transcript replacement; input array may
 * alias existing messages. NULL is allowed only with count=0. Success expires
 * old message views, failure preserves them. Keeps configuration, queues,
 * subscriptions and diagnostics; emits no events. Diagnostics are copied before
 * old messages are freed and remain valid until next run/reset/clear/destroy
 * (or another successful replacement). Zero count clears transcript
 * only. INVALID/BUSY/NOMEM follow setter rules. */
pangu_status pangu_core_replace_messages(pangu_core *,const pangu_message *,size_t);
/* Same-thread APIs, permitted from callbacks and while idle. Enqueue deep-copies
 * transactionally. Drain points snapshot the selected prefix; messages queued
 * during emission wait for a later drain. Defaults are one-at-a-time.
 * Selected inputs not committed because of allocation/hook failure remain at
 * the front of their original queue after run return and can be retried. */
pangu_status pangu_core_enqueue(pangu_core *, pangu_queue_kind, const pangu_message *);
pangu_status pangu_core_queue_mode(pangu_core *, pangu_queue_kind, pangu_queue_mode);
/* Read the configured mode; output is unchanged on invalid input. */
pangu_status pangu_core_get_queue_mode(const pangu_core *,pangu_queue_kind,pangu_queue_mode *);
/* Selected next user input without consumption: steering wins over follow-up;
 * ONE selects its first message, ALL selects its entire queue. Prepared hook
 * input is separate, as for queue_count. Views expire on clear/drain/reset/
 * destroy; count/at reflect the current queue, so do not mutate between reads. */
size_t pangu_core_peek_queued_count(const pangu_core *);
const pangu_message *pangu_core_peek_queued_at(const pangu_core *,size_t);
size_t pangu_core_queue_count(const pangu_core *, pangu_queue_kind);
/* Borrowed until queue clear/drain/tail truncation/reset/destroy. */
const pangu_message *pangu_core_queue_at(const pangu_core *, pangu_queue_kind, size_t);
pangu_status pangu_core_clear_queue(pangu_core *, pangu_queue_kind);
/* Keep exactly the earliest count currently queued messages, freeing the tail
 * without allocation. INVALID if count exceeds the current queue size. Allowed
 * during run callbacks; cannot retract inputs already selected for injection.
 * Includes any prefix retained after an earlier failed run, excludes prepared
 * hook input. Preserves mode and retained message views; removed views expire.
 * Intended for atomic enqueue/storage rollback before yielding caller control. */
pangu_status pangu_core_truncate_queue(pangu_core *,pangu_queue_kind,size_t count);
/* Committed next-turn preparation input retained after a failed run before
 * transcript injection. Separate from user queues until merged for injection.
 * Borrowed while idle, invalidated by run/reset/clear/destroy. A queued-input
 * continuation may resume an assistant tail when these messages exist. */
size_t pangu_core_prepared_count(const pangu_core *);
const pangu_message *pangu_core_prepared_at(const pangu_core *,size_t);
/* Subscribers run in registration order, after the per-run event callback.
 * Return only when all work for the event has settled (drive the host event
 * loop inside the callback when needed). They may enqueue/subscribe/unsubscribe.
 * Removals skip listeners not yet visited; additions participate in the current
 * dispatch, matching a JS Set iterator. Callback/context pair is deduplicated.
 * Context lives until unsubscribe returns, or an active callback returns when
 * unsubscribing from inside that callback. Reset preserves subscriptions. */
pangu_status pangu_core_subscribe(pangu_core *, pangu_event_fn, void *, uint64_t *id);
pangu_status pangu_core_unsubscribe(pangu_core *, uint64_t id);
/* Nonblocking native settlement barrier: BUSY during the run, including all
 * agent_end listeners, OK after synchronous run return. No cross-thread use. */
pangu_status pangu_core_wait_idle(const pangu_core *);
/* Borrowed state: streaming message lives until the next model snapshot or
 * message_end; error is updated at turn_end when an assistant carries a diagnostic,
 * and cleared at the next run/reset/clear/destroy. Empty error means no diagnostic. */
const pangu_message *pangu_core_streaming_message(const pangu_core *);
pangu_slice pangu_core_error(const pangu_core *);
/* Model/request, preparation, or synchronous tool-stage callbacks may report a length-delimited failure
 * diagnostic before returning a non-OK status. Same run thread;
 * asynchronous tool completions use their TOOL_RESULT sink; event subscribers
 * must not report failures. The
 * bytes are copied transactionally and may be overwritten after return.
 * Empty clears the detail. INVALID outside a run or for NULL/nonzero length;
 * NOMEM leaves the previous detail unchanged and must be propagated by caller.
 * Core copies it into a synthesized assistant failure, including ABORTED,
 * or the current tool's error result. Tool errors consume the detail before
 * another tool executes; nested standalone tool calls isolate their detail.
 * The owned detail is released before run/tool-call return. Existing provider terminal errors win.
 * A callback returning OK must not report a failure. No cross-run retention. */
pangu_status pangu_core_report_error(pangu_core *,pangu_slice);
/* Pending IDs follow Pi's Set semantics: duplicate starts share one entry and
 * any end for that ID removes it, even if another operation with the same ID
 * remains active. Entries are in ID insertion order. Call views are borrowed
 * until that ID is removed or run returns; this is not an operation counter. */
size_t pangu_core_pending_tool_count(const pangu_core *);
const pangu_content *pangu_core_pending_tool_at(const pangu_core *, size_t);
#ifdef __cplusplus
}
#endif
#endif
