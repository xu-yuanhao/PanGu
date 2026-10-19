# 前期工作收尾报告（T05～T09 交接）

日期：2026-10-19/20。执行范围：[前期工作交接TODO](../前期工作交接TODO.md)。本报告只陈述实际运行与产物；审批状态与可执行证据分开记录。

## 1. 修改文件与目的

新增文件（全部已在 `governance/bootstrap-policy.json` 登记）：

| 文件 | 目的 |
|---|---|
| `docs/migration/t05-admission-evidence.json` | 机器可读的 T05 证据账：当前树摘要、三次隔离运行、两个构建模式、历史输入与未批准事项 |
| `docs/migration/t05-trusted-remote-closure.md` | H02 候选设计：权限矩阵、检查来源绑定、受保护执行入口、首次引导、待维护者决定 |
| `docs/migration/t07-dependency-decisions.md` | H04 能力级依赖决策表、候选版本/许可证、延期项与重新评估条件 |
| `docs/migration/t08-native-contract.md` | H05 C 边界规则、服务描述符、所有权、取消语义、事件草案与已运行的互操作实验 |
| `docs/migration/t09-comparison-basis.md` | H06 Pi oracle、场景格式、比较器、负例与限制 |
| `docs/migration/t10-admission-package.md` | H07 首批模块、允许边、第一条构建组合与策略变更候选 |
| `docs/migration/precore-handoff-report.md` | 本报告 |
| `tools/migration/oracle_pi.mjs` | 脚本化模型驱动的 Pi 事件轨迹采集器（开发工具） |
| `tools/migration/compare_trace.py` | 轨迹比较器与证据校验器 |
| `tools/migration/test_compare_trace.py` | 比较器负例测试（12 项） |
| `tools/migration/t09-scenarios.json` | 五个确定性场景（候选控制文件） |

修改文件：

| 文件 | 修改 | 原因 |
|---|---|---|
| `governance/bootstrap-policy.json` | 登记上述新文件 | G00 默认拒绝，新增文件必须登记 |
| `tools/gates/check.py` | 允许 `gate` 类型使用 `.mjs`，其他类型不变；新增 `SUFFIXES` | Oracle 必须用 JS 导入冻结 Pi 源码；这是**控制面候选变更**，需独立审核 |
| `tools/gates/test_check.py` | 增加 2 项测试：`.mjs` 仅可作为 gate；不得为 document/control/gate-test/instructions | 证明新后缀没有被放宽成通用豁免 |
| `docs/migration/t05-progress.md` | 增加证据代际表与候选准入说明 | 区分 146/179 项历史与当前，修正“尚无候选入口”的过时描述 |
| `docs/migration/t06-toolchain-review.md` | 增加 2026-10-20 复验章节 | 记录本轮真实重跑与复用范围 |
| `docs/migration/task-ledger.json` | 更新 T05/T06 技术里程碑与阻塞项；T07～T09 改为 IN_PROGRESS 并登记证据 | 技术验证与独立审批分开 |
| `docs/migration/t05-admission-evidence.json` | 由脚本按当前树重新生成 | 绑定当前文件摘要，不手写哈希 |

用户未提交工作：交接开始时所有项目文件均未提交（`git status` 全为 `??`，无 HEAD）。本轮未执行任何 `git add/commit/reset/clean/checkout`，未删除或覆盖既有文件。`references/` 未修改；Pi 只在项目外副本安装。

## 2. H00～H08 状态

| 项 | 状态 | 实际结果 | 证据 |
|---|---|---|---|
| H00 接收核实 | 完成 | 无提交、origin 已连接；G00 PASS；181 项门禁测试与 58 项迁移测试通过；vcs/remote 均 BLOCKED | 见第 4 节日志 |
| H01 证据与文档一致性 | 完成（候选） | 当前树 76 个登记文件、75 条源摘要；三次隔离运行分别标注为“当前实现旧策略摘要”“夹具”“项目 NOT_RUN”；历史快照未回写 | `t05-admission-evidence.json` |
| H02 可信远端闭环 | **BLOCKED** | 交付权限矩阵、检查来源绑定与引导候选；未安装 App、未改规则、未推送、未运行真实拒绝用例 | `t05-trusted-remote-closure.md` |
| H03 工具链收尾 | 完成（候选） | 冻结镜像 `verify` 通过；重新确认 Clang `-MD` 记录 C 头、GNU as `--MD` 记录 `.include`/`.incbin`；Clang 拒绝 `--MD`，两阶段设计必要 | `h03-toolchain-1792427856/reproduce.json` |
| H04 依赖决策 | 完成（候选） | TLS/HTTP/JSON/Schema/allocator/file/process/terminal/search/dlopen 均有归属；未批准任何第三方库 | `t07-dependency-decisions.md` |
| H05 最小契约与互操作 | 完成（局部） | C/Assembly 消费者实测 sizeof/offsetof、Buffer 归属、回滚顺序、错误拒绝；发现并修正 3 个自身错误 | `experiment.json` |
| H06 对照基础 | 完成（oracle 侧） | Pi 在工作副本运行；5 场景产出 11/23/23/21/8 条事件；比较器 12 项负例通过；native 侧 5/5 BLOCKED | `t09-comparison-basis.md` |
| H07 首批准入包 | 完成（候选） | 6 模块、1 构建组合、服务定义与策略变更候选；未实现 T10 | `t10-admission-package.md` |
| H08 复验与交还 | 完成 | 见第 4 节最终隔离回执 | 本报告附录 |

`docs/前期工作交接TODO.md` 共 51 个可执行子项：49 项已勾选并附交付位置或证据；2 项保持未勾选，原因是真实未完成——H02 的“真实部署与回读”（缺 App 安装与管理员权限变更授权）与 H05 的“实验接入项目布局/符号门禁”（生产头文件尚未准入，实验只在项目外）。两项均已注明已完成部分和剩余条件，未写成通过。

对应主任务真实状态：

| 任务 | 状态 | 未闭合项 |
|---|---|---|
| T05 | IN_PROGRESS | 独立控制面批准、检查发布 App、去管理权限、首次引导、真实拒绝证据 |
| T06 | IN_PROGRESS | T05 前置；跨机器/跨平台；生产提升 |
| T07 | IN_PROGRESS | 无实测体积/性能；终端等延期项；无独立批准 |
| T08 | IN_PROGRESS | 生产头文件未准入；线程/取消竞争/DSO 未验证 |
| T09 | IN_PROGRESS | 无原生实现可比；协议与文件副作用未比较 |

## 3. 参考依据与采用/未采用

| 参考 | 提交/位置 | 采用 | 未采用及理由 |
|---|---|---|---|
| Pi Agent Loop | `2ec55d808d455b1a64a2bf7888198207a096120a`，`packages/agent/src/agent-loop.ts` | 事件顺序、工具批次、取消硬退出、并行工具 end 顺序 | 不翻译 JS 对象系统、不复制 typebox |
| Pi 类型 | 同提交，`packages/agent/src/types.ts` | 事件字段与 stopReason 语义 | 不实现 declaration merging |
| Cordis | `references/cordis/packages/core/src/{fiber,registry,service}.ts` | 注册即 effect、反向清理、依赖缺失不激活 | 不引入 Cordis/Node 运行时 |
| Harness AgentLoop | `packages/core/agent-loop/src/index.ts` | 定义/提供/消费分离、`ctx.effect` 清理 | 不复制其包结构 |
| Pi 文档与依赖清单 | `docs/migration/dependency-inventory.json` | 识别能力边界 | 不把 npm 树带入产品 |

## 4. 实际命令、退出码与证据

最终验证（原始日志在 `/home/xu-yuanhao/Documents/PanGu-artifacts/handoff-h00-1792423753/logs`）：

| 命令 | 退出 | 结果 |
|---|---|---|
| `python3 -B tools/gates/check.py` | 0 | G00 PASS，生产准入 BLOCKED |
| `python3 -B -m unittest discover -s tools/gates -p 'test_*.py'` | 0 | 181 项通过 |
| `python3 -B tools/migration/test_reference_bundle.py` | 0 | 16 项 |
| `python3 -B tools/migration/test_surface_inventory.py --typescript <...>` | 0 | 16 项 |
| `python3 -B tools/migration/test_surface_review.py` | 0 | 14 项 |
| `python3 -B tools/migration/test_compare_trace.py` | 0 | 12 项 |
| `python3 -B tools/gates/vcs.py --root .` | 2 | BLOCKED：无提交，66/76 个登记文件未入索引 |
| `python3 -B tools/gates/remote.py` | 2 | BLOCKED：规则存在，检查 App 未绑定，自动化仍为管理员 |
| `toolchain_image.py verify` | 2 | 镜像摘要重算 PASS；BLOCKED 表示未获外部批准 |
| `toolchain_image.py run --job self-tests` | 2 | 181 项在断网只读命名空间通过 |
| `toolchain_image.py run --job native-admission` | 2 | `native_matrix=NOT_RUN`、`build_count=0` |

计数说明：本轮新增 2 项 G00 测试、12 项比较器测试，门禁测试由 179 增至 181。迁移测试由 46 增至 58（新增比较器 12 项，单独套件）。没有为保持旧计数删除或合并测试。

外部产物与摘要：

| 产物 | 路径 | 摘要 |
|---|---|---|
| 当前树隔离自测 | `h08-isolated-1792428058/output/run.json` | 见附录 |
| 当前树空矩阵 | `h08-isolated-1792428058/output-native/native/admission.json` | 见附录 |
| H03 复验 | `h03-toolchain-1792427856/reproduce.json` | 见附录 |
| H05 互操作 | `handoff-h00-1792423753/experiment/experiment.json` | `c1cae95209902f11952e0f0e29a643f834bafb3f4522c6572155b998b7fbe051` |
| T09 oracle | `t09-oracle-run-1792427602/` | 5 case，事件 11/23/23/21/8 |
| T09 比较器自比 | `t09-compare-1792427659/summary.json` | PASS 5/5 |
| T09 无 native | `t09-compare-oracle-only-1792427659/summary.json` | BLOCKED 5/5 |
| 冻结镜像 | `toolchain/frozen-gas-9sf3ohft/image/image.json` | `9765bb42…3c1dc4` |
| Pi oracle 工作副本 | `pi-oracle-t09h06/` | lockfile `5c7ef167…4e9647` |

旧证据复用范围：debug/release 重复构建、C/Assembly 行号、双帧回溯、PIC 共享链接复用 `native-gas-3gfpabe5`；146 项与 179 项隔离自测保留原回执，不冒充当前运行。

## 5. 差异、风险与回退

| 变更 | 风险 | 回退 |
|---|---|---|
| `.mjs` 可作为 `gate` 登记 | 后缀规则扩大；若审核不通过，oracle 无法入库 | 撤回 `check.py` 的 `SUFFIXES` 与策略中的 `.mjs` 条目；runner 移回项目外 |
| 新增比较器与场景 | 尚未接入受保护 case 集；可能被误当成兼容通过 | 保持 `IN_PROGRESS`；未接入 G06 |
| T09 场景控制文件 | 候选控制面，未独立批准 | 删除登记，恢复为项目外实验 |
| t05-progress 证据代际说明 | 无运行时影响 | 文档回退 |

回退动作只能由维护者执行；本任务未做任何控制面生效动作。

## 6. 下一位主审的最短复验

1. `python3 -B tools/gates/check.py`（0）。
2. `python3 -B -m unittest discover -s tools/gates -p 'test_*.py'`（181）。
3. `python3 -B tools/migration/test_compare_trace.py`（12）。
4. 用附录命令重跑一次隔离自测与空矩阵作业，核对摘要。
5. 审阅 `t05-trusted-remote-closure.md` 的 R1～R5 并按第 6 节决定。

进入 T10 仍缺：控制面独立批准（含 `.mjs` 准入与工具镜像摘要）、T05 远端权限分离、T07 候选依赖批准、T08 头文件准入。缺一项都不能开始 T10 生产代码。

## 7. 控制面变更声明

本次对 `tools/gates/check.py`、`governance/bootstrap-policy.json`、`tools/migration/t09-scenarios.json` 的修改均属控制面候选。执行 Agent 不能自批；在本报告提交后须由维护者独立审查。审查通过前，G00 的 PASS 只表示当前本地策略自洽，不表示控制面已被批准生效。

## 附录：最终隔离回执

在最终树上重新运行（2026-10-20）：

```sh
python3 -B tools/gates/toolchain_image.py run \
  --image /home/xu-yuanhao/Documents/PanGu-artifacts/toolchain/frozen-gas-9sf3ohft/image \
  --image-sha256 9765bb42bfca5bce3787c83801b9201563d59f758b61a327bfb644c3737c1dc4 \
  --controller <snapshot>/controller --candidate <snapshot>/candidate \
  --policy-sha256 2fe2decfb75853a09374553d0a7884176e2df216ef2c537d6ca4c1c8032a6a3e \
  --controller-sha256 7c7c7bac30cac71f8f7f56a2055365c0f7aadd2407911749400a0749e959d3f6 \
  --job self-tests --output <new-dir>
```

| 作业 | 退出 | 结果 | 回执摘要 |
|---|---|---|---|
| self-tests | 2 | 181 项通过；`status=BLOCKED` | `isolation.json` `c83bf8a2…d2a0`；`run.json` `cad9f183…b4ec` |
| native-admission | 2 | `native_matrix=NOT_RUN`、`build_count=0`、`status=BLOCKED` | `isolation.json` `2d7e735e…4913`；`admission.json` `3b789b7b…fbe5` |

运行目录：`/home/xu-yuanhao/Documents/PanGu-artifacts/h08-isolated-1792428058`。

树编辑差量（必须明示）：隔离运行时控制器树含 76 个文件，此后登记文件变为 77 个。此后所有改动都是文档或证据文本：本报告附录、`t05-admission-evidence.json` 的重新生成、`t05-progress.md` 的计数说明、`task-ledger.json` 的状态同步、`前期工作交接TODO.md` 的勾选。`tools/` 下无任何文件在隔离运行之后被修改。原生登记仍为空，因此矩阵结论不变。复核者重跑时应得到自己的新摘要，不应把本报告的摘要当作当前树摘要。

当前树绑定（不含本证据文件自身）：

| 项 | 值 |
|---|---|
| 策略摘要 | `2fe2decfb75853a09374553d0a7884176e2df216ef2c537d6ca4c1c8032a6a3e`（隔离运行与当前登记一致） |
| 树摘要 | 自引用，不在文档中固定；以 `t05-admission-evidence.json` 的 `controller_tree_sha256` 为准（该文件排除自身） |
| 登记文件 | 77（含证据文件自身） |
| 源摘要条目 | 76 |

`t05-admission-evidence.json` 自身不含自己的摘要；其记录的树摘要按“其他 76 个登记文件”计算。本报告、`task-ledger.json`、`t05-progress.md` 与交接 TODO 的勾选全部完成后，证据文件最后一次性重新生成，生成脚本与 `receipt.capture` 交叉核对得到 `policy match True`、`tree match True`、77 个登记文件、76 条源摘要。由于本报告自身参与该摘要，树摘要值不在文档中固定，只以证据文件字段为准。复核命令：

```sh
python3 -B tools/gates/receipt.py capture --candidate . --trusted-root . \
  --policy-sha256 2fe2decfb75853a09374553d0a7884176e2df216ef2c537d6ca4c1c8032a6a3e \
  --receipt <new-external-path>
```


## 附录：本轮自身的实现错误（保留记录）

| 错误 | 发现方式 | 处理 |
|---|---|---|
| T08 实验首版断言 allocator 48 字节、instance 偏移 56 | 汇编检查退出 1 | 改为 40/88/8/64；修正断言与文档，不改实现 |
| 回滚顺序期望 `ABab` | 检查失败 | 实际为 `ABba`（激活 A,B；撤销 B,A）；修正期望 |
| T09 runner 调错 `runAgentLoop` 参数顺序 | 0 事件、`emit is not a function` | 按源码签名改为 `(prompts, context, config, emit, signal, streamFn)` |
| `agentLoop` 返回流未被消费 | 0 事件 | 改用 `runAgentLoop` |
| H03 汇编夹具缺 `.intel_syntax noprefix` | GNU as 报 operand mismatch | 修正夹具，不是工具链问题 |
| 证据文件曾包含自身摘要 | 重新生成后树摘要不自洽 | 生成器排除自身，并记录理由 |

这些错误均在校验阶段暴露，未写进通过证据。
