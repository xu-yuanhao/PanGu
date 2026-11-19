# C03：Agent 来源状态覆盖交接

来源固定为 Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283`，已通过
`git -C references/pi rev-parse HEAD` 核实。规格入口为
`packages/agent/src/agent.ts` 的 createMutableAgentState、PendingMessageQueue、
subscribe、reset、continue、runWithLifecycle，以及 `test/agent.test.ts`。
下面行号均指该固定快照。参考文件只读。

## 当前验收结论（2026-11-19）

C03 已验收。独立只读上下文逐要求审查实现、公开契约和实际消费者，未发现阻断项；此前长期状态缺口由持久 settings→run继承→reset/第三次工具切换闭环补齐。`instance_overrides` 覆盖8个mask、非空兼容覆盖、低层显式cfg与无回写；`--trace-persistent-state` 三行与真实Pi比较。安装外部C消费者实际设置一次、覆写输入、两次工具循环/reset。最新Core ASan14/14（11.20s），整机ASan59/59（57.05s）均通过；Android静态/共享Core+embed编译成功，无设备运行。以下历史未闭合表述按当时源码点保留，由本结论及最新实现替代；C02 Schema、C04 wire、C06平台不在本项验收内。

## 审计口径

### 2026-11-18 长期状态续接

实现入口：`state.h/state.c` 新增 model/thinking/tool_count/tool_at 与
set_model/set_thinking/set_tools，以及 replace_messages。`internal.h` 的
state_model/state_thinking/state_tools 是独立持有的实例配置，不保存 run cfg
借用指针。`core.c:create` 初始化固定来源 unknown model 字面元数据；clear/reset
保留配置，destroy 释放配置。工具数组连续，所有元数据深拷贝、函数表按值复制，
ctx 借用至成功替换/销毁；运行中（含 agent_end）setter/replace 返回 BUSY。

replace_messages 先完成全部消息深拷贝及 error 诊断独立复制，再提交并释放旧消息；
失败不改变消息或诊断，输入可以直接别名单条旧消息，或用浅数组组合多条旧视图。
保留队列/订阅/配置/诊断，不发事件。retained_error 由 clear/destroy 回收；父在
loop.c 新 run 清 error 入口释放该副本，避免保留到下一次 reset。

实际消费者：`tests/core/state.c:persistent_state/persistent_failures`，独立开关
`--persistent-state`，默认 core_state 也执行。覆盖默认完整 metadata、idle 配置
读写/输入覆写/别名替换、函数与 ctx 使用、连续 tools、两次实际模型→工具→模型
（中间 reset）、clear 后配置保留、运行回调 BUSY、无事件替换、单/多消息别名、
错误后空/别名替换、诊断 OOM 原子性和下一 run 清诊断、分配计数最终归零。

验证：Linux，独立 `build/core-state-work`，Debug、GCC 16.2.1、
`-fsanitize=address,undefined -fno-omit-frame-pointer`。
`cmake --build build/core-state-work --target pangu_core_state_test -j2` 成功；
最终 `ctest --test-dir build/core-state-work -R '^core_state$' --output-on-failure`
**1/1，0.04s**，无 sanitizer 报告。没有运行 all、真模型或其他平台。

父负责的 loop 接线通过上述公开 getters 消费：仅 stateful run 继承，低层 loop/
loop_continue 仍用显式 cfg；override_fields 与 prepare 覆盖不回写实例配置。
父继续来源差分、覆盖位组合及安装消费者的集中验收；本子组不勾选整体 C03。

该文件有 **31 个直接 it 声明及另外 5 个 it.each 声明**；参数展开不等于声明数。
下表逐个列出31个直接声明；后表补充5个参数声明，避免遗漏继续/队列边界。
消费者路径均相对仓库根。已有消费者表示本次已读代码，不表示本次重跑它的专项。
“原生”表示实际公开 C API 断言；“部分”表示不能以邻近场景替代来源完整组合。
“Diff”用于已执行真实Pi与原生消费者比较的场景，仍不外推未覆盖的来源断言。

| 来源行 | 来源行为 | 实际消费者与审计结论 |
|---|---|---|
| 93 | legacy 无 streamFn 使用默认流 | `tests/core/transcript.c:default_stream/default_exports`，实际默认/显式 provider；实例默认流为原生适配 |
| 114 | 默认状态 | `tests/core/state.c:persistent_state/default_mutators_reset`：长期 model 为固定 unknown 字面元数据、thinking OFF、tools 空；另检查空转录、空流/错误/pending/prepared、idle、两队列 ONE。active_* 仍仅运行期视图。 |
| 128 | 自定义初始状态 | `state.c:persistent_state` 以 idle setters 初始化长期 model/thinking/tools，再实际执行两次工具循环；`transcript.c:seed` 与事务 replace_messages 提供初始转录。C 构造后初始化替代 initialState 语法。 |
| 144 | 初始 prompt/tools 转录 | `transcript.c:requests/tool_updates`；新 state baseline 工具声明实际进入 run/reset |
| 164 | 工具 loadout 变化声明 | `transcript.c:requests`，实际请求前新增/移除及不重复声明 |
| 212 | 合并 pending system | **Diff**：`transcript.c:pending_declarations` skills pending system + user，system总数2、section/时间1保留、echo新增；真实Pi比较。 |
| 247 | 可执行工具集重写 pending 声明 | **Diff**：同消费者 initial first、pending伪造second/删除first，清除虚假差异、保留note/时间，system snapshot仍first；真实Pi比较。 |
| 282 | reset 恢复 baseline | `transcript.c:seed/baseline/reset_failures` 回放/原子OOM；新 `state.c:default_mutators_reset` 实际错误后清双队列并保留 baseline/tool |
| 309 | subscribe/unsubscribe | `state.c:subscriptions`，去重、当次新增参与、删除未访问 listener、自移除、reset 保留 |
| 331 | 抛出失败完整生命周期 | **Diff**：`loop.c:failure_diagnostics` + `pangu_core_report_error` 为模型/初次准备/下一轮准备/transform/convert/getApiKey复制详细诊断，真实Pi六阶段×取消比较 `provider exploded`、ERROR/ABORTED、空内容和最终state.error；status-only旧消费者仍使用默认阶段诊断。 |
| 362 | prompt 等待异步订阅者 | **Diff/原生适配**：`state.c:settlement_barrier` callback返回作为settlement，顺序/取消/两轮复用与Pi异步订阅比较；Pi另以显式latch断言pending listener期间prompt不resolve，不使用固定sleep。 |
| 400 | waitForIdle 等待订阅者 | **Diff/原生适配**：同消费者agent_end各listener内wait_idle为BUSY、run返回后OK；Pi listener暂停期间waitForIdle promise不resolve，settle后resolve，两轮验证。 |
| 435 | 订阅者接收 active abort | **Diff/原生适配**：同消费者first listener取消、second观察同一活动取消状态，第二轮重新未取消；Pi收到真实AbortSignal，比较完整settlement/cancel/reuse trace。 |
| 473 | settle 后忽略 tool update | **Diff**：新 `state.c:late_tool_updates(0/1)` 单操作：running update一次→final→late/NULL stale，不新增事件、不改结果；父真实 Pi 单工具来源差分已通过 |
| 538 | settled parallel tool 的 late update | **Diff**：同消费者双操作：end(a)且 b pending 时 late/NULL stale，无新事件、b 正常完成、两结果不变；父真实 Pi 并行来源差分已通过 |
| 614 | state mutators | `state.c:persistent_state/persistent_failures`：idle set_model/set_thinking/set_tools、读取、输入覆写、别名 setter、事务 replace_messages（含多消息浅数组别名）、无事件及 BUSY；model/tools 和消息/诊断逐分配失败保留旧状态。`transcript.c:tool_updates` 单独验证运行期 prepare 覆盖，不再替代长期状态验收。 |
| 649 | steering 队列 | `state.c:queue_preview/main`，实际模型消费 ONE/ALL、clear 与优先级 |
| 659 | follow-up 队列 | 同上，实际消费 f/g；`cancelled_queue_resume` 取消后逐条恢复 |
| 669 | abort controller | 新默认检查 idle 未取消；`state.c:cancelled_queue_resume` 实际 active cancel/ABORTED/恢复，`settlement_barrier` 下一 run 不继承取消 |
| 676 | processing 时拒绝 reset | **Diff**：`state.c:streaming_busy` 在assistant start且尚未提交时，实际reset拒绝为BUSY、原USER指针/消息数不变，恢复后恰好USER/assistant；真实Pi异步订阅者在同边界断言拒绝。 |
| 710 | streaming 时拒绝 prompt | **Diff**：`state.c:streaming_busy` 在assistant start且尚未提交时，实际prompt拒绝为BUSY、原USER指针/消息数不变，恢复后恰好USER/assistant；真实Pi异步订阅者在同边界断言拒绝。 |
| 750 | streaming 时拒绝 continue | **Diff**：`state.c:streaming_busy` 在assistant start且尚未提交时，实际continue拒绝为BUSY、原USER指针/消息数不变，恢复后恰好USER/assistant；真实Pi异步订阅者在同边界断言拒绝。 |
| 786 | assistant-tail follow-up continue | `state.c:main` FOLLOW_RESUME，实际模型消费及队列保留/恢复 |
| 860 | legacy prepareNextTurn signal | **Diff**：`state.c:legacy_hooks` 实际noop工具→结果提交→legacy prepare_next_turn，活动owner未取消、idle为BUSY，两请求；Pi原legacy signal instanceof/未取消与相同上下文角色比较。 |
| 901 | finishTurn 与 active abort/context | **Diff**：同消费者finish_turn_context在noop结果提交后观察0123角色、单个toolResult、活动owner未取消且BUSY，返回END仅一请求；Pi实际signal/context与计数比较。 |
| 1043 | non-assistant tail steering 优先 | **Diff**：同矩阵非assistant user/toolResult/custom末尾，steering在follow之前，真实Pi逐请求比较。 |
| 1109 | finishTurn end 保留队列 | `state.c:main` FINISH_END 与 `transcript.c:control_combinations`；前者预排队，后者有执行中排队控制 |
| 1136 | peek 不消耗 | `state.c:queue_preview` 重复peek、ONE/ALL、双队列优先级/clear/reset/重复内容 |
| 1155 | provider observer 转发 | `tests/core/provider.c` 的真实 provider observer 消费者；来源 API options 转为 C 配置/observer |
| 1177 | sessionId 转发/修改 | `tests/core/transcript.c:key_requests` 的 active options 及 `tests/core/proxy-request.c` serializer；公开 run 配置每次提供，不是 Agent setter |
| 1203 | 默认 identity/覆盖合并 | `transcript.c:identities` 三种实际请求；PanGu 产品归属替换 Pi，字段合并及显式禁用沿来源 |

## 5 个参数声明

| 来源行 | 参数行为 | 已有消费者 / 未闭合组合 |
|---|---|---|
| 824 | assistant-tail steering ONE/ALL | **Diff**：`state.c:continuation_startup` assistant-tail steering ONE/ALL，实际首轮跳过重复poll，与Pi比较全部请求USER输入。 |
| 945 | empty/system-only continuation 不 drain | **Diff**：同矩阵empty/system-only与双队列，INVALID且无请求/prepare/事件、不消费队列；真实Pi比较。 |
| 961 | user/toolResult tail 首次请求延迟 follow-up | **Diff**：同矩阵 user/toolResult tail × follow ONE/ALL，首请求无follow，后续按模式提交，与Pi逐请求比较。 |
| 1007 | continuation startup steering ONE/ALL | **Diff**：同矩阵 user/toolResult tail × steering/both × ONE/ALL，比较队列优先级、请求历史、prepare次数和最终余队列。 |
| 1071 | ERROR/ABORTED 即使 finish CONTINUE 仍保留队列 | `transcript.c:control_combinations` 对两 stop reason 实际覆盖 finish CONTINUE、两队列及硬退出 |

## 本次可执行交付

仅修改 `tests/core/state.c`，新增本文件。新测试仍走已有
`pangu_core_state_test` / `core_state`，未新增构建入口、依赖或生产实现。

- `late_tool_updates`：单操作与双并行操作，实际 start→pump→sink→事件→release。
  单操作有一次 initial update；并行 a 已结束且 b pending 后再向 a sink 提交 stale。
  每次 stale（含 NULL payload）均 OK、事件数不变，最终 done 内容与非错误标志不变，
  pending 清空，所有 operation release 并清除 retained sink。
- `settlement_barrier`：同一次 run 的 per-run event 先于两个订阅者，
  agent_end 内实际调用 wait_idle/run(prompt)/run(continue)/reset/clear/append，
  检查 BUSY 与转录不变；listener 取消被下一个 listener 观察。
  reset 后再 run，订阅仍存在且新 run 未取消；两轮均验证返回后 idle。
- `default_mutators_reset`：默认可观察状态、两个队列默认模式，
  append 消息和声明深拷贝，实际模型错误后 reset 清错误/双队列并保留声明基线，
  保留模式，clear 清完整转录。

迟到回调的原生边界：公开 `loop.h` 允许 async sink 保留至 release，release 必须
quiesce 且不得调用 sink；同步 provider 不得保留 sink。测试遵守此契约，绝不在
run 返回/release 后调用失效 C 指针。JS 来源可在 prompt 返回后调用闭包，C 生命周期
并不提供同等长期闭包；本次验证的是仍合法的 operation 生命周期内 final 后忽略更新。

父来源差分入口：`pangu_core_state_test --trace-late-updates` 复用同一消费者，精确两行：

```text
late:0:1:1:1
late:1:0:2:1
```

字段为 `late:parallel:update_count:end_count:ignored`。release 数与 malformed
stale payload 仅原生断言，不混入 JS 对照。已有 trace-state/trace-cancel-queues 不变。

## 验证与剩余入口

本子任务实际执行（Linux，既有 `build/core-asan`）：

```text
cmake --build build/core-asan --target pangu_core_state_test -j2
ctest --test-dir build/core-asan -R '^core_state$' --output-on-failure
```

编译成功，新增三组消费者执行通过；CTest **1/1，0.03s**，
flags 为 `-fsanitize=address,undefined -fno-omit-frame-pointer -g`，无 sanitizer 报告。
随后增加 trace 开关并成功重建。父确认最终含 trace 版本与真实 Pi 两 late 场景的
`core_state/core_pi_loop_diff` **2/2，2.09s** 通过；此项为父验证结果，本子任务未重复运行。
未运行 all、真模型、终端或其他平台；未修改 root、Core TODO、生产代码或 loop-diff。

本轮没有发现需要生产修补的 late/barrier/reset 差异。pending system 与 continuation 初始审计缺口已由下述父续接差分替代，仍不勾选整个 C03。

2026-11-18 父续接：`loop-diff.mjs` 调用未修改 Pi Agent 比较 `--trace-continuation` 的49场景（36个tail/队列/模式组合及13个空历史、custom/system末尾、准备排队/取消、ERROR/ABORTED/finish-end边界）。记录实际请求USER历史、请求及两种prepare次数、终态和余队列；余队列通过公开peek/模式/clear观察。ASan state/Pi-loop **2/2，1.74s**通过。pending_declarations 两场景此前ASan transcript/Pi-loop **2/2，1.76s**通过。上述替代对应旧缺口；finish回调取消、请求流内BUSY精确时点及其他适配仍需独立验收。

2026-11-18 streaming_busy续接：ASan state/Pi-loop **2/2，1.64s**通过；三种重入拒绝发生在assistant start且转录未提交的实际边界，替代676/710/750原先仅agent_end时点的部分覆盖。

2026-11-18 hooks/settlement续接：`legacy_hooks` 两真实noop工具链与Pi差分 ASan state/Pi-loop **2/2，1.68s**。随后复用`settlement_barrier`输出两轮实际stage/rounds/cancelled/running/idle观察，Pi显式listener latch同时检查prompt/waitForIdle未提前resolve，释放后顺序完成并reset复用；ASan state/Pi-loop **2/2，1.72s**。C为同线程同步回调settlement和非阻塞BUSY屏障，不提供JS Promise或跨线程等待；此适配已在state.h明确。来源异常诊断文本与配置/长期state适配仍需最终审查，不据此勾选C03。

2026-11-18 详细异常通道：`pangu_core_report_error` 为模型与准备回调提供owned length-delimited诊断，真实Pi六阶段×取消差分保留完整异常文本。回调局部缓冲覆写/替换/六阶段逐点OOM/下一run默认诊断及安装C provider失败均有实际消费者。Core ASan **14/14，6.29s**；整机ASan **58/58，43.45s**。已有provider终止错误不被覆盖；status-only旧宿主仍回退原生阶段诊断，工具错误通过TOOL_RESULT，不将此API用于工具/事件回调。
