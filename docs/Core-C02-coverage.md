# C02 source-case audit

Pi snapshot: `d5b3de22d0e24a3e9c38a9410d3329b099717283`, `references/pi/packages/agent/test/agent-loop.test.ts`. The file has **36 test declarations**, including two parameterized declarations with two variants each. Titles alone are not acceptance evidence. This index records the inspected native consumers and remaining concrete combinations; it does not mark C02 complete.

“Diff” means an equivalent scripted scenario executes the unmodified prepared Pi source and compares observations in `tests/core/loop-diff.mjs`. “Native” means direct assertions exist, with no claim that every source assertion is in the differential trace. “Gap” identifies a source combination that still needs its actual loop consumer, even if its component operations are individually tested.

| Source line / behavior | Native consumer | Evidence / remaining work |
|---|---|---|
| 80 default stream omitted | `transcript.c:default_exports/default_stream` | Diff: Agent and four loop exports, fallback/explicit precedence; instance-scope adaptation separately checked. |
| 114 AgentMessage lifecycle events | `loop.c` normal fixture | Diff: complete lifecycle/messages/tool effect. |
| 160 transcript-only provider context | `transcript.c:transcript_only_context`, `proxy-request.c` | Diff: SYSTEM text/timestamp and USER from actual provider transcript, one request. Pi asserts only messages key and original system object; C ABI exposes transcript view directly, so key-list observation is an explicit structural adapter, not a C object enumeration. |
| 199 custom-message conversion | `transcript.c:projection_scenarios` notification variant | Diff: filter custom notification from provider view, preserve original custom execution history, compare projected and final transcript counts. |
| 253 transform before conversion | `transcript.c:projection_scenarios` pruning variant | Diff: four old entries plus new USER, transform keeps last two before conversion, provider sees exact retained text and execution history remains complete. |
| 305 tools and results | `loop.c` normal fixture | Diff: two requests, actual effect, updates/result and final state. |
| 402 execute duration excludes hooks | `tool-call.c:clock_ns/scenario/loop_result` | Diff with deterministic clocks: 26.5ms execute rounds to 27ms in event and transcript, excluding 100ms preflight and 500ms after-hook. Native blocked result remains untimed. |
| 456 truncated tool arguments not executed | `loop.c:TRUNCATED` | Diff: zero effects, diagnostic and lifecycle. |
| 528 beforeToolCall mutates args without revalidation | `tool-call.c:mutable_args` | Diff: raw/validated/before/execute/after views and validation count. |
| 589 argument preparation before validation | `tool-call.c:prepare/loop_result` plus normalization fixtures | Diff: raw old:true becomes prepared new:true before required-new Schema validation; before/execute/after see prepared arguments while original assistant call remains unchanged. Native callback buffers are overwritten after sink and normalization/OOM paths remain covered. |
| 668 completion order versus source commit order | `parallel.c` | Diff: reversed completions, end order, source-order messages. |
| 762 queues injected after all tools complete | `parallel.c:QUEUED_PARALLEL/QUEUED_SEQUENTIAL` | Diff: enqueue while operations are active, retain queue during all three pump completions, inject after all source-order result events, then second provider observes interrupt. Parallel and sequential variants release every operation. |
| 867 one tool forces sequential mode | `parallel.c:OVERRIDE` | Diff: per-tool override over default parallel. |
| 949 one of multiple definitions forces sequential | `parallel.c:MIXED_DEFINITIONS` | Diff: middle call uses a second, sequential definition; preflight/execution/result events serialize the entire batch, maximum active operations one. |
| 1035 all tools permit parallel | `parallel.c:PARALLEL/PARALLEL_DEFINITIONS` | Diff: two tool definitions, three active operations, reversed completion and source-order results; all operations released. |
| 1108 finishTurn after results, before turn_end | `loop.c:finish_context`, `parallel.c:finish_context` | Native finalized result views; lifecycle/finish counts in diff. |
| 1162 error/aborted hard exit despite continue | `transcript.c:control_combinations` modes 7/8 | Diff: continue ignored, one request/finish, no next preparation, both queued inputs retained. |
| 1217 action:end skips queues/preparation | `transcript.c:control_combinations` mode 9 | Diff: actual tool result, one request, no next preparation, both queues retained. |
| 1273 exactly one context-only continuation | `loop.c:FINISH_CONTINUE` | Diff: two requests and finish calls. |
| 1307 natural tool request satisfies continuation | `transcript.c:control_combinations` mode 1 | Diff: one tool effect, exactly two requests/finishes and one next preparation. |
| 1347 natural steering/follow-up satisfies continuation (two variants) | `transcript.c:control_combinations` modes 2/3 | Diff: actual second-request input, exactly two requests, consumed queue, no extra context-only turn. |
| 1404 initial preparation after pending, replaces state | `transcript.c:initial_preparation` plus transactional update fixtures | Diff: prompt/steering completed before preparation, canonical context/model/high thinking replacement, preserved generated input provenance and final requested thinking level. Native failure/OOM fixtures cover transaction rollback. |
| 1458 no steering poll after prepareRequest | `transcript.c:prepare_late_steering` | Diff: late input absent from first request, present in second; queue remains pending during first provider. Pi also asserts its three poll calls. |
| 1503 next-turn prepared snapshot | `transcript.c:next_preparation/tool_updates` | Diff: copied current context, changed model/high thinking plus prepared and queued input are adopted before second request; final assistant records high. Callback-local model bytes overwritten after sink. Native update/retry/OOM cases retain ownership and rollback. |
| 1580 steering added during next preparation | `transcript.c:next_preparation`, `state.c:HOOK_QUEUE` | Diff: next hook emits prepared input and queues steering, provider receives prepared then queued in the second request, exactly two requests/one preparation/five generated messages. Native retry/OOM tests preserve pending ownership. |
| 1633 end receives finalized context and stops polling | `transcript.c:control_combinations` mode 9 | Diff: finish receives committed tool role/name/content/error, generated count matches context, end retains both queues and skips next preparation. |
| 1732 every result terminates batch | `parallel.c:ALL_TERMINATE` | Diff: no next request. |
| 1783 blocked call terminates | `transcript.c:control_combinations` mode 4 | Diff: blocked terminating call has zero effects, one request and an error tool result. |
| 1841 mixed batch with one terminating blocked call | `transcript.c:control_combinations` mode 6 | Diff: blocked a plus executed b, source-order error flags 10, exactly two requests. |
| 1899 not all parallel results terminate | `parallel.c:MIXED_TERMINATE` | Diff: continuation and source-order results. |
| 1964 afterToolCall marks batch terminating | `transcript.c:control_combinations` mode 5 | Diff: after-hook termination suppresses next request after one actual effect; transcript strips internal terminate flag. |
| 2013 empty continuation rejected | `transcript.c:low_level_entries` | Native low-level validation separated from Agent wrapper. |
| 2031 continue without replaying USER events | `transcript.c:continuation_tails` USER variant | Diff: exactly one assistant start/end, one generated message, original input retained and total context two messages. |
| 2072 custom last message converts to USER | `transcript.c:continuation_tails` custom variant | Diff: provider-only USER projection preserves timestamp/content; original custom transcript entry unchanged; no replayed input events. |
| 2167 standalone validation/hooks/error outcomes | `tool-call.c:scenario/sticky_failures` | Diff: thirteen outcomes; native normalization, sticky sink failure, allocation failure and nested use. |
| 2214 structured content replacement/drop rules | `tool-call.c:scenario/loop_result` | Diff plus native patches for content/structured/details/usage/error/terminate. |

Additional native risk coverage includes allocation-failure sweeps, cancellation/reuse, borrowed callback buffers overwritten after return, deferred provider metadata, long text/image/signature messages and async release barriers. Those checks complement the source cases; they do not fill the Gap rows by implication.

Latest verification (2026-11-16): Core ASan/UBSan with the full Pi differential module enabled passed 14/14 (7.07s), including installed consumer, source-loop comparisons and 74 incremental/316 strict JSON vectors. This proves the current fixtures pass together; it does not by itself prove every assertion in the original source tests.

Next acceptance review: reconcile source-specific diagnostic/Schema adapter differences and real-model service evidence against C02, then audit the remaining Agent state tests for C03. Reuse existing `loop.c`, `parallel.c`, `transcript.c` and the single differential driver rather than creating parallel execution implementations. C02 stays unchecked until the remaining source combinations and stated acceptance evidence are closed.

2026-11-18 同步工具详细异常续接：`tool-call.c:tool_diagnostics`/`loop-diff.mjs`执行六阶段×独立/两工具循环，a报告详细错误、b不报告且保留独立默认错误，源序结果与Pi一致；stage validation使用lazy schema访问故障定位异常通道，不代表Schema转换实现。原生另覆盖两入口全阶段逐点OOM与内外错误不同的实际嵌套工具。已安装C消费者经公开report_error调用工具失败但不写转录。Core ASan **14/14，6.46s**通过。Schema宿主归一化/类型转换/格式差异仍未闭合。
