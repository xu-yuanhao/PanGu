# T09 对照执行与证据骨架

状态：`ORACLE_AND_COMPARATOR_LOCAL_PASS`；原生侧 `NOT_RUN`，T09 整体未完成。本文记录可运行的 Pi 参照运行、场景格式、比较器和反例。比较器通过不等于兼容验证通过；尚无原生实现可比。

## 1. 参考运行环境

Pi 只在项目外工作副本运行，`references/pi` 保持只读未安装状态。

| 项 | 值 |
|---|---|
| 参考提交 | `2ec55d808d455b1a64a2bf7888198207a096120a` |
| 工作副本 | `/home/xu-yuanhao/Documents/PanGu-artifacts/pi-oracle-t09h06` |
| 来源 | `rsync` 自 `references/pi`，排除 `.git` 与既有 `node_modules` |
| lockfile 摘要 | `5c7ef167a17240e02139c123442bf909391646fcf7beffd3aa12e486ef4e9647`（与 reference-baseline 一致） |
| Node / npm | v26.12.0 / 11.20.0 |
| 安装 | `npm ci --ignore-scripts --no-audit --no-fund`，334 个包，无 lifecycle 脚本 |
| 额外构建 | `packages/telemetry` 用本地 `tsc -p tsconfig.build.json` 构建，使 `@earendil-works/pi-telemetry` 可解析；未运行下载脚本 |
| 已知告警 | `EBADENGINE`（Node 26 高于 Pi 声明的 engines 范围）与 deprecated 警告；未忽略，也未据此宣称支持 |

首次尝试 `packages/ai` 全量构建时，`src/providers/data/*.json` 由生成步骤产出，未执行生成；实际需要的 `dist/index.js` 已由 tsc 产出。该差异记录在此，不隐藏。

## 2. 场景与假模型

场景文件：[t09-scenarios.json](../../tools/migration/t09-scenarios.json)。模型响应是数据，不含脚本：

| case | 覆盖 | feature |
|---|---|---|
| C01-single-text | 单轮文本 | AGENT-01, LLM-01 |
| C02-tool-then-text | 一次工具调用后继续 | AGENT-01, TOOL-01 |
| C03-tool-failure | 工具返回错误 | AGENT-01, TOOL-01 |
| C04-missing-tool | 调用不存在的工具 | AGENT-01, TOOL-01 |
| C05-cancel | 事件中间取消 | AGENT-04 |

隔离约束：

- 无网络、无真实 Provider、无凭据。`baseUrl` 为 `example.invalid`。
- 工具副作用由 runner 内的假实现产生，不读写真实文件树。
- 时钟与 ID 固定：消息 `timestamp` 置 0，工具调用 ID 由场景显式给出。
- 每次运行写到项目外新目录；输出目录必须不存在。

runner：[oracle_pi.mjs](../../tools/migration/oracle_pi.mjs)。它直接 import 冻结副本的 `runAgentLoop`，以固定脚本产生 assistant 流事件；参数顺序、取消信号和事件归一化在脚本内明确写出。

## 3. 比较器与证据格式

比较器：[compare_trace.py](../../tools/migration/compare_trace.py)。

- 输入：场景清单、oracle 目录、可选 native 目录、输出目录。
- 输出：`summary.json` 与每 case 的 `.diff.json`，含 oracle/native 事件数、差异与 verdict。
- verdict：`PASS`/`FAIL`/`BLOCKED`。
- 归一化只删除场景声明的噪声字段（`timestamp`、`durationMs`）；其余字段逐项精确比较。未知字段差异即 FAIL。
- 空日志、缺失文件、截断 JSON、空清单、重复 case ID 均为失败或 BLOCKED，不是 PASS。
- 输出目录已存在时拒绝覆盖。

## 4. 实际运行

oracle（5 case）：

```sh
node tools/migration/oracle_pi.mjs \
  --oracle /home/xu-yuanhao/Documents/PanGu-artifacts/pi-oracle-t09h06 \
  --scenarios tools/migration/t09-scenarios.json \
  --output <new-external-dir>
```

结果：退出 0；事件数 C01=11、C02=23、C03=23、C04=21、C05=8。原始轨迹位于 `/home/xu-yuanhao/Documents/PanGu-artifacts/t09-oracle-run-1792427602`。C05 在 `agent_start` 后取消，得到 `message_start/end`（`stopReason=aborted`）、`turn_end`、`agent_end`，与 Pi 的硬退出路径一致。

比较器自比（oracle 与自身）：`PASS`，5/5。输出 `/home/xu-yuanhao/Documents/PanGu-artifacts/t09-compare-1792427659`。

无原生侧：整体 `BLOCKED`，5/5 `BLOCKED`。输出 `/home/xu-yuanhao/Documents/PanGu-artifacts/t09-compare-oracle-only-1792427659`。这是当前原生实现的真实状态。

## 5. 反例验证

[tools/migration/test_compare_trace.py](../../tools/migration/test_compare_trace.py) 共 12 项，全部通过：

| 反例 | 期望 | 结果 |
|---|---|---|
| 相同轨迹 | PASS | 通过 |
| 删除工具结果 | FAIL（事件数不同） | 通过 |
| 交换因果事件 | FAIL | 通过 |
| 未登记字段变化（`isError`） | FAIL | 通过 |
| 声明噪声字段变化（`timestamp`/`durationMs`） | PASS | 通过 |
| 两侧均为空日志 | BLOCKED，不是 PASS | 通过 |
| 无 native 目录 | 全体 BLOCKED | 通过 |
| 截断 JSON | BLOCKED | 通过 |
| native 缺一个 case 文件 | BLOCKED | 通过 |
| 重复 case ID | 拒绝，退出 1 | 通过 |
| 空 case 清单 | 拒绝，退出 1 | 通过 |
| 输出目录已存在 | 拒绝覆盖 | 通过 |

## 6. 发现并修正的实现错误

1. `runAgentLoop` 的参数顺序为 `(prompts, context, config, emit, signal, streamFn)`。首版写错为 `(…, signal, streamFn, emit)`，得到 `emit is not a function` 与 0 事件；按源码签名修正。
2. `agentLoop` 返回的 stream 未被消费，导致 0 事件。改为 `runAgentLoop` 并直接收集事件。
3. 取消需要 runner 在 emit 回调里触发 `AbortController`；场景的 `cancel_after_events` 因此可复现。

这些错误在校验器运行前被观察到，但都未被当作通过证据。

## 7. 未完成与限制

- 原生实现不存在，因此 T09 无法整体验收。比较器的 PASS 只说明同一轨迹可以比较。
- Pi 运行是源码直跑（Node 26 类型剥离），不是发布包；已记录 Node engine 告警。
- 未比较最终 provider 请求载荷、文件副作用、资源占用或性能。
- `stopReason=length` 的截断工具参数、steering/follow-up、并行工具等场景尚未加入；它们属于同一 runner 的后续 case，不改变当前结论。
- 未使用网络或真实模型；不声称协议兼容。

## 8. 下一步

1. 在 T10 之后为每个 case 提供原生轨迹，比较器即可给出真实差异。
2. 增加协议层比较（请求体与 SSE 事件）与文件副作用比较，分别在独立临时树运行。
3. 将 case 清单纳入受保护场景清单后才可进入 G06 口径；当前清单是候选控制文件。

## 主审收尾修订（2026-10-20，覆盖旧比较器说明）

Oracle 保留原始事件，不先归一化；运行回执记录 manifest/runner 摘要、选定入口与 lockfile 摘要、每 case 退出码/错误/事件数及原始轨迹摘要。入口摘要不是完整 npm 安装闭包证明，不能据此宣称环境完全固定。

比较器仅删除明确的 Pi 元数据路径：事件 message、messages/toolResults 中的顶层消息 timestamp，assistant 流事件中的消息 timestamp，以及工具完成元数据 durationMs。工具 arguments/result/details 内所有业务同名字段必须保留。case ID 限制为安全文件名；未知事件、非连续序号、缺失 agent 结束、未闭合工具执行均拒绝。

两侧运行必须用 --oracle-run-sha256 / --native-run-sha256 指定外部回执摘要；摘要不匹配、执行失败、原始轨迹变化或用例覆盖不一致不能 PASS。调用方仍须建立这些 pin 的独立可信来源，回执不是自签审批。比较输出保存 raw 副本、normalized 与 diff，绑定原始轨迹及比较器摘要。

当前新增反例后比较器为 18 项测试。实际再次执行两份 Pi oracle 并比较，用于验证重复运行稳定性；即使第二目录由 --native-dir 传入，也明确是 Pi-vs-Pi，不是原生兼容证明。真实 native 侧仍 NOT_RUN。旧 12 项及旧 normalized-as-raw 的报告为历史记录，不代表当前能力。
