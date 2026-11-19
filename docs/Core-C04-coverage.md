# C04 公开导出与代理请求覆盖

当前行为组：C04 index.ts 公开面审计与 proxy 请求 wire 组合。仅新增/修改本文件及 `tests/core/proxy-request.c`，不修改生产、差分驱动或其他测试。不据此勾选整个 C04。

来源：只读 Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283`（本轮 `git -C references/pi rev-parse HEAD` 一致）。入口 `packages/agent/src/index.ts`，直接展开 `agent.ts`、`agent-loop.ts`、`proxy.ts`、`types.ts`，以及显式 `stream-fn.ts:setDefaultStreamFn`。wire 数据类型查 `packages/ai/src/types.ts`。直接测试 `agent/test/{agent,agent-loop,proxy}.test.ts`。Pi MIT 许可沿用仓库记录。

## 全部导出归属

以下为源码导出清点与现有真实消费者位置；“已有消费者”不是声明每一个 TS 类型的每一种组合已完成差分。

| 导出 | C 适配与实际消费者 |
|---|---|
| Agent；AgentOptions、AgentInitialState | state.h/loop.h；state.c 的 streaming_busy、continuation_startup、settlement_barrier、legacy_hooks；loop.c 请求和错误场景 |
| agentLoop、agentLoopContinue、runAgentLoop、runAgentLoopContinue | pangu_core_loop/loop_continue 与事件订阅/同步返回；transcript.c 默认流 10 场景；loop.c 和 loop-diff.mjs 实际低层入口 |
| DEFAULT_AGENT_IDENTITY | loop.h 身份配置；loop.c/loop-diff.mjs 的身份与关闭/覆盖归属请求消费者；产品身份为 PanGu |
| runToolCall；ToolCallHooks、RunToolCallOptions | pangu_core_call_tool；tool-call.c 的独立/嵌套、参数更新、钩子、错误诊断、OOM；parallel.c 实际并行循环 |
| setDefaultStreamFn | pangu_core_set_default_stream；transcript.c 默认/显式优先级；实例作用域是有意原生适配，Pi 为模块作用域 |
| streamProxy；ProxyStreamOptions、ProxyAssistantMessageEvent | proxy.h、proxy_request.h；proxy.c 实际 typed/SSE→sink→终态；proxy-request.c 序列化；proxy-http.c/.py 可选真实 HTTP |
| AgentEventSink、StreamFn | loop.h 回调/sink 与 operation；loop.c/parallel.c/proxy.c/provider.c 的实际回调 |
| ToolExecutionMode、QueueMode（agent.ts 重导出同一 QueueMode） | loop.h/state.h 枚举；parallel.c、state.c 队列与 continuation 矩阵 |
| AgentToolCall、AgentTool、AgentToolResult、AgentToolCallOutcome、AgentToolUpdateCallback | core.h/loop.h 标签、长度与工具接口；tool-call.c、loop.c、state.c 迟到更新消费者 |
| BeforeToolCallResult、AfterToolCallResult、BeforeToolCallContext、AfterToolCallContext | loop.h before/after/update/patch；tool-call.c 参数和结果替换、拦截、失败事务 |
| AgentTurnContext、AgentTurnDecision、FinishTurn | loop.h turn view/decision/finish；state.c legacy_hooks、loop.c finish/error 消费者 |
| AgentLoopTurnUpdate、PrepareRequestContext、AgentRequestUpdate、PrepareRequest、PrepareNextTurnContext | loop.h prepare 更新 sink；loop.c、transcript.c 实际 model/tools/context 替换与错误；state.c continuation_startup |
| AgentLoopConfig、AgentContext | loop.h 配置、core transcript 和活动工具集合；loop.c、transcript.c、examples/core/embed.c |
| ThinkingLevel | core.h 七级枚举；已有默认级别和本轮 wire_matrix 的 max；未穷举全部七级与七种 stopReason 的笛卡尔积 |
| CustomAgentMessages、AgentMessage | core.h 标签 + custom_role/JSON；transcript.c 转换消费者；proxy serializer 明确拒绝未转换 custom |
| AgentState、AgentEvent | state.h 查询/订阅/队列/取消；state.c、loop.c、parallel.c；不暴露 JS 可直接赋值数组或 Set |

index.ts 不导出 getDefaultStreamFn；未把内部读取函数误列成必须提供的独立 ABI。事件数据、配置与结果的 TS 类型本身没有运行时符号，C 以头文件契约和实际回调消费覆盖。

## 请求字段及新增可执行组合

`pangu_proxy_request_json` → yyjson writer → 持有一个 JSON block 的新 Core；`pangu_proxy_active_request_json` 从真实 provider 的当前 owner/model/options 获取输入。原 `provider_options` 仍在真实 run 内调用后者，确认 idle 拒绝、未知字段仅 direct 保留、observer 顺序/失败与下次运行选项刷新。本轮 `wire_matrix` 使用前者；没有另建 serializer 或重写生产逻辑。

| wire 面 | 既有与本轮断言 |
|---|---|
| model | 原 id 覆盖/contextWindow；本轮 id/provider/api 覆盖旧 metadata，name/baseUrl/reasoning/input/cost/contextWindow/maxTokens/compat 逐值比较保留 |
| context.messages | system/user/assistant/toolResult 顺序、时间戳和内容；本轮四角色同请求，源 append 后覆写 stack text，源 Core 销毁后解析 output |
| options 11 字段 | temperature、samplingParams、maxTokens、reasoning、cacheRetention、sessionId、headers、metadata、transport、thinkingBudgets、maxRetryDelayMs：空对象全省略；同请求完整有效值（0/off/none、中文/NUL session、嵌套数组/null/false）；同请求全部 null；每字段 yyjson_equals 对比输入并断言成员数 |
| 本地/未知 options | apiKey/authToken/proxyUrl/signal/onPayload/onResponse/customOption 全部被过滤；这些是合成测试值，不用真实凭据。null 场景仅验证 JSON.stringify 保留，并不声称 TS 类型或 provider 接受 null |
| system sections/tools | 删除 null、显式空文本；toolsRemoved 两项；两个 toolsAdded，constrainedSampling false/true；嵌套 array/anyOf/null/string/enum/required/additionalProperties=false 的 Schema 与原 JSON 完整相等，无 Schema 转换声明 |
| content | text 的中文/NUL/换行/引号/反斜杠及 textSignature；image base64/mimeType；thinkingSignature/redacted true（原 false）；toolCall arguments 嵌套 null/空串/中文/0/false、namespace/thoughtSignature/argumentsError |
| assistant | 原 responseModel/responseId（内含NUL）、rawStopReason、diagnostics/deferred、endTurn=false、非有限成本 null；本轮 max/length/endTurn=true/errorMessage/durationMs=0、非零 usage 与 totalFromProvider=true |
| toolResult | 原 isError=true/details/nestedCalls；本轮 isError=false、text+image、details 数组（null/false/object）、durationMs=7；structured_json 为本地工具结果能力，不属于来源 ToolResultMessage wire，继续断言不发送 structuredContent |
| 非法/OOM | 原非法 tool arguments、diagnostics/deferred/nestedCalls 根类型 18 场景、全序列化逐分配失败扫描继续运行；失败 out=NULL、不改源 transcript、allocator live=0 |

来源 proxy.test.ts 四个直接测试主要是响应：terminal tool metadata、无 LF terminal metadata、无 terminal EOF、增量参数增长/性能。对应 proxy.c 的 repeated_tool_end、terminal_events/wire_fragments、eof_partial、参数预览及 streaming-json-diff 来源向量；本轮不重复这些响应测试，也不把原生预览测试当作全链路线性性能证明。provider.c 实际 prepare_payload→send→observe_response→body 与替换/错误/OOM 是 provider hook 证据，不是全 proxy 请求字段证据。

## 父任务可接的 trace

`build/core-asan/pangu_core_proxy_request_test --trace-wire` 输出 **3 行 JSONL**，依次 empty/all-values/all-null。每行是完整 `{model,context:{messages},options}`，没有其他 stdout、无动态时间；timestamp 固定 17/18/19/20。所有字符串按长度写出，JSON escape 保留 NUL。无新增 trace 文件或根构建入口。

父可在现有 loop-diff.mjs 用 execFileSync 获取并逐行 JSON.parse：对每行分别调用未修改的 Pi streamProxy(model, context, {...options, authToken:'fixture', proxyUrl:'https://invalid.example', apiKey:'fixture', signal:new AbortController().signal, customOption:true})，stub fetch 捕获 body，回复合法 done SSE 并消费 result，比较 JSON.parse(body) 全对象。这个步骤检验白名单透传；若 model/context 直接用 native 输出，它不独立检验 C 消息映射，必须另从上述 fixture 定义构造 source messages/model 才能检查该层。不要归一化掉 options null/false/0、内容 NUL、Schema、presence 差异。

既有 `--request-json <options-json>` 与 `--trace-options` 保持不变。新增 trace 的第三行 all-null 可由 JS 显式构造运行时对象，不应标为类型检查通过的正常配置。父差分尚未接入：本轮只报告 C 断言通过，不声称真实 Pi 差分已通过。

## 生产差异与 ABI/生命周期边界（交父处理）

1. `src/core/proxy_request.c:message` 对 assistant 总是发送 thinkingLevel（零初始化为 off）；来源 ai/types.ts 可省略 thinkingLevel，streamProxy 原样 JSON.stringify(context)。core.h 没有 has_thinking_level，无法区分缺省/off。已有夹具主动填 off 只覆盖显式值，不能消除缺省差异。
2. 同一函数对 toolResult 总是发送 usage；来源 usage 可缺省。C 没有 has_usage，缺省与显式全零不可区分。新增 fixture 的 toolResult 故意未设 usage；trace 将显示当前零对象，父独立构造 source fixture 时应保持 usage 缺省以复现差异。
3. optional() 用 slice.len 判断存在，因此显式空 textSignature/thinkingSignature/thoughtSignature/namespace/argumentsError、responseModel/responseId/rawStopReason/providerThinkingLevel/errorMessage 会省略。TS 的显式空字符串会保留；现有结构/头约定有些明确空表示缺省。这是 ABI 表达/适配差异，不在本次测试里硬编码错误的相等断言。
4. system sections/toolsAdded/toolsRemoved 的显式空容器与缺省合并；system/user 字符串 content 原生规范化为数组。Pi streamProxy 不做此转换。是否需要保留 wire presence/形状由父确定；语义正规化不能冒充字节级一致。
5. schema 原样透传已有完整值证据，不等于来源 TypeBox/Ajv 参数转换、默认值或错误诊断已实现；该缺口交服务层，测试不添加私有替代校验器。

公开 C ABI 检查：字符串/字节为 slice；append 深拷贝；serializer 输出独立持有，借用 body 到 encoded destroy 为止。默认 allocator 下新增测试证明销毁 source 不影响 output；自定义 allocator **context 必须继续存活**，不能由此推断任意 owner allocator 可提前销毁。既有逐点失败覆盖独立输出释放与回滚。serializer 同步无异步句柄/回调，无新增取消语义；取消/在途释放证据在 proxy.c 的 event_provider_lifecycle、cancelled_eof 和 state.c 的操作释放消费者，HTTP 在独立可选测试。JS Promise/AbortSignal 适配为同线程回调与显式 operation 生命周期，不声称跨线程可安全调用 core。

验证：Linux x86-64；`cmake --build build/core-asan --target pangu_core_proxy_request_test -j2`；`ctest --test-dir build/core-asan -R '^core_proxy_request$' --output-on-failure`，1/1 通过（初次新增组合 0.02s）；最终补齐 model/Schema 完整相等断言后同命令复验仍为 **1/1，0.02s**，无 ASan/UBSan 报告；`--trace-wire` 输出在构建目录以 Python 解析验证为 3 行、options 成员数 0/11/11。仅相关目标/单项，未运行 all；父已完成的 product-asan 58/58 不重复。不涉及真模型、凭据、终端或其他平台实跑。

下一交付：父接上述三个完整请求的真实 streamProxy fetch-body 差分，并决定 optional presence 的 ABI 表达及兼容策略；本子任务不修改生产或 loop-diff.mjs。

## Content 可选空串 presence 回归（父生产修补前的红灯）

新增 `content_presence`，只针对 textSignature、thinkingSignature、thoughtSignature、namespace、argumentsError 五字段。每个字段均跑 NULL/0 缺省与 nonNULL/0 显式空两个状态；nonNULL 输入指向单个非 NUL 栈字节，确保实现按长度处理。真实链路为输入 → `pangu_core_append` → 检查所持有 slice presence/独立地址 → `pangu_proxy_request_json` → 销毁 source Core → yyjson 检查对应 key 缺省或存在且是空字符串。没有扩展模型 metadata、数组或 message optional presence，也没有修改生产代码。

新 trace：`build/core-asan/pangu_core_proxy_request_test --trace-content-presence`，约定 **2 行完整请求 JSONL**，先 absent 后 empty；原 `--trace-wire` 三行契约不变。该 trace 执行同一组严格断言，生产修复前会中止，不应把第一行输出当作成功。

父在 Pi 差分独立构造 source 的定义如下（不要从 native body 复制 content）：

```js
const usage = {
  input: 0, output: 0, cacheRead: 0, cacheWrite: 0, totalTokens: 0,
  cost: {input: 0, output: 0, cacheRead: 0, cacheWrite: 0, total: 0},
};
for (const present of [false, true]) {
  const context = {messages: [{
    role: 'assistant', model: 'mock', provider: 'mock', api: 'fixture',
    timestamp: 23, stopReason: 'toolUse', thinkingLevel: 'off', usage,
    content: [
      {type: 'text', text: 'text', ...(present ? {textSignature: ''} : {})},
      {type: 'thinking', thinking: 'reason', ...(present ? {thinkingSignature: ''} : {})},
      {type: 'toolCall', id: 'one', name: 'lookup', arguments: {},
       ...(present ? {thoughtSignature: '', namespace: '', argumentsError: ''} : {})},
    ],
  }]};
  const model = {id: 'mock', provider: 'mock', api: 'fixture'};
  // streamProxy(model, context, {authToken:'fixture', proxyUrl:'https://invalid.example'})
  // Capture fetch body and compare with the corresponding JSONL row.
}
```

此 fixture 显式给 thinkingLevel=off 与 usage，是为了隔离五个 content 字段，不撤销上节缺省 thinkingLevel/toolResult usage 的 ABI 缺口；无 toolResult、无工具 Schema/数组 presence 新要求。

验证：`cmake --build build/core-asan --target pangu_core_proxy_request_test -j2` 成功；`ctest --test-dir build/core-asan -R '^core_proxy_request$' --output-on-failure` **0/1，0.01s，SIGABRT**，为修补前预期红灯。GDB 定位 `content_presence` 的 append 后 `fields[i].len==0 && !!fields[i].data==present` 断言（加入时第225行），说明 append 已丢 nonNULL empty；当前 serializer optional 也仍仅判断 len，父须两处修补后再验。没有 sanitizer 内存错误报告；不把 abort 当作通过。未跑 all。

交接：父按约定修改 `core.c:copy_slice` 与仅用于上述五字段的 serializer present helper，然后专项重建并接真实 Pi trace 差分。本子任务已准备可执行红灯及 source 定义；不替父跨写生产/loop-diff。

## 2026-11-19 父实际接线验收

content五种optional字符串的nonNULL/0深拷贝与序列化已修：copy_slice保留owned空视图，wire按presence写空串，NULL0省略。真实streamProxy捕获POST body与独立JS定义比较absent/empty两请求；三组wire_matrix另从fixture语义独立构造完整四角色、多模态、嵌套Schema/model与11个选项，不用native输出充当source输入、不删差异字段。首轮JS夹具错误把带签名system数组写成纯字符串，按来源SystemMessage允许的TextContent[]修正后通过；并非放宽生产。Core ASan14/14与整机ASan59/59通过，包含上述来源差分与安装消费者。缺省thinkingLevel/toolResult usage及其他可选字符串/空数组presence仍未闭合，C04保持未勾选。

## Message presence ABI 回归与独立来源 fixture（2026-11-19）

本节更新前文尚未闭合的 message presence 差异；旧验证记录保留为历史，不代表本轮结果。
采用同一固定 Pi 提交 `d5b3de22d0e24a3e9c38a9410d3329b099717283`，直接阅读
`packages/agent/src/proxy.ts` 的 `streamProxy` / `buildProxyRequestOptions` 和
`packages/{agent,ai}/src/types.ts` 的消息与 Usage 定义。Pi 直接 JSON.stringify context。
本子任务只改 `tests/core/proxy-request.c` 与本文，不改生产、loop-diff 或根构建。

`message_presence` 通过公开 append 深拷贝、检查所持有 presence 与五个 optional slice、
覆写调用者栈内存、proxy_request、销毁 source、解析独立 owned 输出完成真实调用链。
NULL/0 缺省、非 NUL 单字节地址/0 显式空、带 NUL 的中文非空均覆盖。
assistant usage 始终输出；thinking 缺省 off 不输出、flag 显式 off 输出、无 flag high 兼容输出。
toolResult usage 缺省不输出、HAS_USAGE 显式零输出、无 flag 非零兼容输出。
`invalid_message_presence` 还逐一覆盖 usage 三个 optional 小 flags 的显式零/false兼容输出，
26 个 unknown bits、四角色所有跨角色 bits、两块 text / image / 带空签名 text 的 string form，
以及 assistant/toolResult 的空 string form 均要求 append 返回 INVALID 且 transcript 不变。
合法 string form 覆盖 system/user 零块空串与一块带 NUL text；既有 wire_matrix 继续验证默认数组与非空 system 容器兼容。

`--trace-message-presence` 固定 **3 行完整请求 JSONL**，顺序 absent、explicit、legacy。
无标签包装、无动态时间、无其他 stdout；先完成严格断言再输出对应行。
父必须独立构造以下 JS fixture 调真实 Pi streamProxy 捕获 fetch body，逐行完整比较，
不能以 C trace 的 model/context 充当来源输入，也不能删除 presence/形状差异：

```js
const zeroUsage = () => ({
  input: 0, output: 0, cacheRead: 0, cacheWrite: 0, totalTokens: 0,
  cost: {input: 0, output: 0, cacheRead: 0, cacheWrite: 0, total: 0},
});
const model = {id: 'mock', provider: 'mock', api: 'fixture'};
const fixtures = [0, 1, 2].map(mode => {
  const text = 'a\u0000世界';
  const content = mode === 0 ? [] : mode === 1 ? '' : text;
  const assistant = {
    role: 'assistant', timestamp: 33, content: [],
    model: 'mock', provider: 'mock', api: 'fixture', stopReason: 'stop',
    usage: zeroUsage(),
  };
  if (mode !== 0) {
    Object.assign(assistant, {
      thinkingLevel: mode === 1 ? 'off' : 'high',
      responseModel: mode === 1 ? '' : text,
      responseId: mode === 1 ? '' : text,
      rawStopReason: mode === 1 ? '' : text,
      providerThinkingLevel: mode === 1 ? '' : text,
      errorMessage: mode === 1 ? '' : text,
    });
  }
  const result = {
    role: 'toolResult', timestamp: 34, content: [],
    toolCallId: 'one', toolName: 'lookup', isError: false,
  };
  if (mode !== 0) {
    result.usage = zeroUsage();
    if (mode === 2) result.usage.input = 1;
  }
  return {
    model,
    context: {messages: [
      {role: 'system', timestamp: 31, content,
       ...(mode === 1 ? {sections: {}, toolsAdded: [], toolsRemoved: []} : {})},
      {role: 'user', timestamp: 32, content},
      assistant, result,
    ]},
    options: {},
  };
});
// For each fixture: streamProxy(model, context,
//   {...options, authToken:'fixture', proxyUrl:'https://invalid.example'}).
// Stub fetch; return valid done SSE and await stream.result(); compare captured
// JSON.parse(request.body) with corresponding native JSONL row.
```

兼容 trace 不变：`--trace-wire` 仍三行，其 timestamp=20 toolResult 加 HAS_USAGE 保留此前显式零 usage；
`--request-json` assistant 加 HAS_THINKING_LEVEL、nested toolResult 加 HAS_USAGE 保留原 off/usage；
`--trace-content-presence` 仍两行，assistant 加 HAS_THINKING_LEVEL 保留显式 off。
未删除任何原断言。新 trace 不包括非法输入，非法/OOM由默认测试入口执行。
输出 lifetime 仍使用默认 allocator 验证；自定义 allocator context 必须活到 owned 输出释放。
无新增 ABI、依赖、异步或取消行为，父生产实现拥有新 ABI。

本轮验证：Linux x86-64，独立 `build/core-wire-work`，Debug/GNU 16.2.1，
`-fsanitize=address,undefined -fno-omit-frame-pointer`；Agent/Services/TUI/ProxyHTTP/Overflow/Profiles 均 OFF。
`cmake --build build/core-wire-work --target pangu_core_proxy_request_test -j2` 成功；
`ctest --test-dir build/core-wire-work -R '^core_proxy_request$' --output-on-failure` **1/1，0.05s 通过**，
无 ASan/UBSan 报告。Python subprocess 分别执行新 message trace、原 wire/content trace，
严格 JSON 解析确认为 **3/3/2 行**；`--request-json '{}'` 输出保留 assistant off 和 nested toolResult usage。
父接口落盘后首次运行即通过，无本轮生产红灯。没有构建父 `core-asan`、没有运行 all，
没有真模型、凭据、终端或其他平台验证；不声称本子任务已执行真实 Pi JS 比较。

当前交接：新增公开 append→request→source destroy→owned 输出回归和非法 bits/form 检查已可运行。
尚缺行为：新三行 message-presence trace 的真实 Pi 独立比较由父接线；C04 其他行为不据此验收。
下一入口：`build/core-wire-work/pangu_core_proxy_request_test --trace-message-presence`，
按本节独立 JS fixtures 顺序逐行比较 streamProxy 的完整 POST JSON。
