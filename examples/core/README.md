# Core embedding

Configure with `cmake -S . -B build/core`, then build `pangu_core_embed`.
The example injects a scripted model and a local tool, executes a two-request
model/tool round trip, subscribes to message events, and queues a follow-up
from the tool completion listener. Core embeds pinned yyjson object code for transcript declaration comparison;
its only external runtime dependency is the C runtime. See `deps/README.md`.
The run entry blocks until settlement. Tool batches default to parallel;
asynchronous tools inject start/release and an event pump, while synchronous
tools need only execute. See `loop.h` for callback lifetime and cancellation
rules, and `tests/core/parallel.c` for a deterministic asynchronous host.
The provider uses the request-view callback, which separates converted model
messages from owner state and cancellation. `transcript.h` exposes an owned
replayed system snapshot; reset preserves that system/section/tool baseline.
The fixed fixture validator is for the scripted empty object only; a production
host must inject a mature JSON Schema validator.

Model, preparation, and synchronous tool-stage callbacks returning a failure status can call
`pangu_core_report_error(owner, detail)` to preserve a length-delimited host
diagnostic in the assistant failure and `pangu_core_error`. Check and propagate
its status; the detail is copied, so callback-local bytes may be overwritten
immediately. Report only when returning failure. Cancellation preserves that
diagnostic with an ABORTED stop reason. A tool-stage failure consumes its detail
into that tool's error result before another tool executes; nested standalone
tool calls isolate the detail. Asynchronous tool completions use their own
TOOL_RESULT sink instead. The installed C consumer demonstrates the provider
path, and `tests/core/tool-call.c` covers stage failures and nested calls.

Idle instance configuration uses `pangu_core_set_model`, `pangu_core_set_thinking`,
and `pangu_core_set_tools`; getters remain available after a run or reset.
Model bytes and tool metadata are copied; tool contexts stay borrowed until
replacement or destruction. Stateful `pangu_core_run` inherits these settings
when its config does not override them. `override_fields` permits explicit
OFF or empty tools/model; low-level loop entries use their config directly.
Setters and transactional `pangu_core_replace_messages` return BUSY while
running, including listeners. Replace preserves queues, configuration and
diagnostics; reset replays the system baseline and keeps configuration.
The installed C consumer sets once, overwrites caller buffers, runs a real
tool twice with reset between runs, and verifies the idle configuration.

Install with `cmake --install build/core --prefix /absolute/install/path`.
Consumers use `find_package(PanGuCore CONFIG REQUIRED)` and link `PanGu::core`.
Headers are C17 and C++ compatible. Provide CMAKE_PREFIX_PATH for the prefix.

Cross-platform entry points are in `cmake/toolchains/`: Windows uses MinGW;
macOS/iOS require Apple SDKs; Android accepts `-DCMAKE_ANDROID_NDK=/path/to/ndk`
or an initial `ANDROID_NDK_HOME`. The selected NDK is cached and passed to
compiler probes, so incremental builds do not require the environment variable. Use
`-DCMAKE_TOOLCHAIN_FILE=... -DBUILD_TESTING=OFF` for library configuration.
These entry points are not evidence of successful target-platform execution.

Source behavior: Pi d5b3de22d0e24a3e9c38a9410d3329b099717283,
packages/agent/src/types.ts and agent.ts, packages/ai/src/types.ts.
Message unions map to tagged C structures and allocator-owned byte slices.
Pi's license is installed alongside the library.

`pangu_core_call_tool` runs nested or standalone tools through the same hooks;
see `tests/core/tool-call.c`. Its rich result is caller-owned and must be destroyed.
`after_tool_patch` uses explicit field-presence bits. `prepare_arguments` and
`validate_arguments` submit copied JSON through scoped sinks. A per-instance
fallback provider can be installed with `pangu_core_set_default_stream`.
For a Core-only configuration pass `-DPANGU_BUILD_TUI=OFF`.

Optional HTTP proxy transport:

```
cmake -S . -B build/core-http -DPANGU_BUILD_TUI=OFF -DPANGU_BUILD_PROXY_HTTP=ON
cmake --build build/core-http
cmake --install build/core-http --prefix /absolute/install/prefix
```

Installed consumers use `find_package(PanGuProxyHTTP CONFIG REQUIRED)` and
`PanGu::proxy_http`. The base `PanGuCore` package does not require CURL.
The host serializes `{model,context,options}` and initializes libcurl globally;
see `proxy_http.h` and the actual HTTP consumer in `tests/core/proxy-http.c`.

Public interface handoff (C01):

- Runtime drivers call `pangu_core_run`, use owner state/queue APIs, and copy
  event `new_messages` before callbacks return when persistence needs it.
- Service providers implement `request`, tool execute/start/release, validators,
  and hooks in `loop.h`; async callbacks are dispatched on the run thread.
- Presenters subscribe through `state.h`, read borrowed message snapshots during
  callbacks, and enqueue steering/follow-up on that same thread.
- Host shutdown waits for synchronous run settlement, unsubscribes, releases
  borrowed provider/tool contexts, then destroys the Core instance.

`core_installed_consumer` is part of the normal Core CTest scope. It installs
into an isolated build prefix, configures a separate `find_package(PanGuCore)`
consumer, links the installed target, and runs a request/response round trip.

Pi differential tests (developer-only dependencies, isolated in build):

```
python3 tools/integration/prepare_pi_diff.py
cmake -S . -B build/core-pi-diff -DPANGU_BUILD_TUI=OFF -DPANGU_PI_DIFF_DIR="$PWD/build/pi-diff"
python3 tools/integration/check.py core --build-dir build/core-pi-diff
```

For the unmodified pi-ai public entrypoint instead of the narrow export adapter:

```
python3 tools/integration/prepare_pi_diff.py --full-runtime
cmake -S . -B build/core-pi-diff -DPANGU_PI_DIFF_DIR="$PWD/build/pi-full-diff"
ctest --test-dir build/core-pi-diff -R '^core_pi_' --output-on-failure
```

This copies the original Agent, pi-ai and telemetry sources into build, records
source SHA-256 values, and changes only package metadata to use Node's TypeScript
loader. It installs the external modules needed by the injected-model Core
scenario, not all optional provider SDKs. It validates public-entrypoint behavior;
it is not a full Pi distribution footprint or provider benchmark.

The checked-in differential runners compare the eleven complete loop scenarios, thirteen standalone tool outcomes, state/hook
observations and twenty-six streaming JSON preview/final vectors. They
are concrete partial coverage, not proof of all upstream behavior.

模型请求身份：`pangu_core_default_identity()` 返回进程寿命的 PanGu/cli-agent 默认 attribution；`pangu_loop_config.identity.fields` 指定独立覆盖字段。设置 `PANGU_IDENTITY_ATTRIBUTION` 且 `attribution_enabled=0` 显式关闭归属；设置为1时整个 attribution 替换默认。提供者在 request 回调内通过 `pangu_core_active_identity(owner)` 读取合并结果，并按协议映射请求头；闲置时返回NULL。所有自定义切片和类别数组借用到run返回。默认URL为空，宿主可配置其真实发布地址。

需要保留细粒度流事件的宿主使用 `pangu_loop_config.request_events`。提供者获得 `pangu_model_event_sink`，提交累计消息与 `pangu_assistant_event`；代理提供者可直接调用 `pangu_proxy_create_events(owner,sink,sink_ctx,&proxy)` 后 feed/finish，或在可选HTTP组合调用 `pangu_proxy_http_events(owner,&options,sink,sink_ctx)`。UI/订阅者在 `PANGU_MESSAGE_UPDATE` 读取 `event->assistant_event` 的kind、content_index、delta、结束内容或最终tool_call；这些字段只在回调内有效，保留时自行复制。旧request/model路径的assistant_event为NULL。完整调用及错误/取消/OOM场景见 `tests/core/proxy.c`，HTTP取消后重用见 `tests/core/proxy-http.c`。

排队输入可由 `pangu_core_peek_queued_count/at` 预览，按当前ONE/ALL模式选择，steering优先于follow-up；读取不消费，不包含独立持有的prepareNextTurn输入。`pangu_core_get_queue_mode`读取配置。切勿在count与at之间修改队列。示例在tool_end排入follow-up后显示选中输入。状态诊断在turn_end发布，finishTurn/message_end期间尚未更新，下一run开始时清空；`tests/core/state.c --trace-state`与Pi Agent差分覆盖这些时序。

提供者请求选项通过 `config.request_options_json` 借用至run返回，包含sessionId、temperature及提供者扩展；request回调可用 `pangu_core_active_options(owner)` 读取。代理提供者应调用 `pangu_proxy_active_request_json(owner,request,&encoded)`，使用其单个JSON块作为HTTP body，传输结束后destroy；该入口保留实际转换后的消息和当前模型，并按Pi白名单过滤选项。`tests/core/proxy-http.c`展示完整可编译接线。直接协议提供者若支持原始事件观察，在归一化前调用 `pangu_core_observe_provider_event(owner,event_json)`，检查非OK并停止；宿主的 `on_provider_event`接收借用模型/JSON，可在回调内复制或同步持久化。代理不支持该原始事件能力，不会伪造回调。sessionId修改与观察回调的参考差分见proxy-request.c/loop-diff.mjs。

提供者可在 assistant 消息的 `response_model`、`response_id`、`raw_stop_reason` 提交实际响应模型、响应标识和原生结束原因，空 slice 表示缺省。它们与请求使用的 `model` 及控制循环的 `stop_reason` 分开；sink/append 深拷贝这些字节，返回后提供者可释放或覆写输入。代理请求将非空字段分别写为 `responseModel`、`responseId`、`rawStopReason`。采用的 Pi proxy SSE 协议没有这三个响应字段，因此代理响应适配器不推断或伪造它们。消息结构有新增尾部字段，宿主与库应使用同一版头文件重新构建。

消息还可携带 `diagnostics_json`（诊断数组）、`deferred_json`（deferred handle 对象）及工具结果的 `nested_calls_json`（nestedCalls 对象）。它们沿用 JSON 长度 slice 的复制所有权，空 slice 缺省、`[]`/`{}` 显式存在；代理序列化会严格解析 JSON。`has_end_turn/end_turn` 区分缺省和显式 false，仅保留提供者信息，不覆盖 `stop_reason`。诊断与 handle 的生产者负责脱敏；Core 不执行 handle 的轮询或嵌套任务恢复，这些工作由提供者/执行插件消费。

短期凭据使用 `config.get_api_key`（ctx为config.ctx），每次请求在上下文转换后接收当前model.provider。调用一次sink提交字节，返回前Core已复制，可立即释放源数据；空值或不提交使用借用的 `config.api_key`。提供者在其request/model回调内读取 `pangu_core_active_api_key(owner)` 并交给协议认证，禁止写入消息/日志。Core在提供者返回后撤销视图、擦除并释放自有副本，host及HTTP库自有副本由各自生命周期管理。resolver失败阻止提供者执行；NOMEM保留已有提交。完整刷新/回退/OOM/擦除验证见transcript.c，HTTP实际调用见proxy-http.c。测试只使用脱敏占位值。

直接模型协议提供者可用 `pangu_core_prepare_payload(owner,json,&owned)` 调用 `config.on_payload`，取得独立持有的单JSON块后发送、destroy；无提交保留原值，一次提交替换（包括JSON null），sink返回时已复制。输入/输出均用yyjson验证，非OK停止请求。原生输入readonly，修改须显式提交replacement。收到响应头后、处理正文前调用 `pangu_core_observe_response(owner,&response)`，转发 `config.on_response`；响应切片只借用到返回。`tests/core/provider.c`展示实际prepare/send/headers/body的调用和错误副作用，loop-diff与Pi Agent比较replace/undefined/null的轨迹。Pi proxy不支持这些协议钩子，所以代理适配不自动调用。

`before_tool_update` 是可修改已验证参数的首选before钩子。它的sink接受一次 `pangu_before_update`，复制reason/arguments后返回；`has_arguments=1`以完整JSON对象表达对validated args的修改，不再做Schema验证。原始assistant/tool_call不变，execute与after看到更新参数；原context视图到before回调返回前仍有效。缺省不提交等价于不修改。失败产生preflight工具错误，NOMEM走运行错误路径；独立 `pangu_core_call_tool` 复用相同实现。`tests/core/tool-call.c --trace-before`与Pi来源的beforeToolCall修改String→Number而不重验用例对照。

安装Core独立组件用 `cmake --install BUILD --prefix PREFIX --component Core`，无需先构建可选服务/TUI/Agent。选中服务组合的完整安装另提供 `PanGuServicesConfig.cmake`；纯C工程可 `find_package(PanGuServices CONFIG REQUIRED)`，链接 `PanGu::services` 与 `PanGu::services_schema`，C++运行时闭包由模块导出。外部消费者与许可证闭包回归在 `tests/integration/services-consumer/`，只验证已选Linux依赖，不代替跨平台SDK验收。

运行层持久队列可在入队前记录 `pangu_core_queue_count`，保存失败时调用 `pangu_core_truncate_queue(core,kind,previous_count)` 撤回尚未选中的新尾消息。该操作不分配，适用于内存不足的回滚路径，不影响模式、另一队列或prepared输入；已选入当前轮的输入无法用它撤回。实际消费者是 `src/agent/queues.c`，回归覆盖空闲及运行回调中的保存失败、旧队列保留和重开一致性。

Resource measurements use the optional `PANGU_BUILD_PROFILES=ON` Release host
and `python3 tools/integration/profile_core.py`. The paired Pi workload uses
`prepare_pi_diff.py --full-runtime`; every sample checks message/byte/event totals.
Results and precise limits are recorded in
[Linux Core measurements](../../docs/measurements/core-linux-2026-11-14.md).
The cumulative 1MiB stream currently exposes a native copying regression; this
is an open optimization item, not a full product resource acceptance.

Optional overflow classifier: enable `PANGU_BUILD_OVERFLOW=ON`, consume
`find_package(PanGuOverflow CONFIG REQUIRED)` / `PanGu::overflow`, and include
`pangu/core/overflow.h`. Create a classifier for the execution plugin lifetime,
classify finalized assistant messages with their original context capacity,
and destroy it after in-flight calls finish. Classification returns a status and
an explicit output boolean: allocation/regex errors are not non-matches. The
caller owns the compaction/retry/commit policy. Source-pattern and numeric
coverage lives in `tests/core/overflow-vectors.json`; tests compare the original
Pi `ai/src/utils/overflow.ts`. The separate module uses PCRE2; basic Core builds
and its package do not query/link that dependency.

The model-temporary allocator now reuses one retired large block; [follow-up measurements](../../docs/measurements/core-linux-2026-11-14-reuse.md) retain the increased peak-RSS tradeoff and the remaining cumulative-copy gap. Tool result ownership and credentials do not use this cache.

The source differential also reads the adopted Pi test fixture
`references/pi/packages/ai/test/data/json-test-suite.json` in place (316
JSONTestSuite parsing documents, source/license attribution retained there).
The existing native driver accepts stdin so raw NUL bytes are tested. Preview
comparisons align C UTF-8 byte input with complete JavaScript code-point
boundaries; strict final arguments/errors are compared for every corpus item.
An all-whitespace argument uses the ECMAScript trim set, including BOM, and
finishes as `{}` without error; nonempty input still uses strict JSON parsing.
