> 2026-10-24 收尾说明：本文为历史前期交接记录，不再是当前开发队列。保留未完成项，远端部署等转入发布准备；后续执行以《原生重写路线与迁移TODO》的近期执行队列为准，不继续按本文扩展前期工程。

# 盘古前期工作交接 TODO（历史记录，不再派发）

日期：2026-10-19。工作目录：`/home/xu-yuanhao/Documents/PanGu`。

目标：用现有实现收尾 T05～T09，使原生核心开发具有可信构建、明确依赖、最小接口及可执行对照基础。完成后交还主审 Agent 复核，再进入 T10。本文是执行交接，不是控制面批准，也不替代主路线。

## 0. 执行任务指令

你现在接手 PanGu 项目的前期工程收尾工作，担任执行 Agent。我负责你交付后的独立复核与核心开发。请直接执行本文件中的任务，不要只回复计划、建议或“可以开始”。

你的工作目录是 `/home/xu-yuanhao/Documents/PanGu`。先阅读根 AGENTS.md，再按本文指定顺序阅读必要文档。以实际文件和验证结果为依据，不假定本文快照永远准确，也不要覆盖已有未提交工作。

你的目标是：收尾 T05～T09 的准备工作，交付一套可审查、可复验、足以支撑后续 T10 原生核心开发的候选成果。你负责门禁收尾、工具链确认、依赖决策、最小原生契约及对照验证基础；你不负责实现 T10 及后续 Agent 产品代码。

请按以下方式执行：

1. 按 H00→H08 逐项推进，先核实现状，只补真实缺项。复用已有工具、清单和有效证据，不重建同类体系。
2. 每项开始前读对应参考实现，说明采用什么及理由；随后完成具体文件、实现或实验，并执行相应验证。不要只写一份描述未来工作的计划。
3. 完成一个子项就在本文勾选，并补充交付位置或证据。部分完成保持未勾选，注明已完成部分和剩余条件；不得把待审、阻塞或未运行写成完成。
4. 在已授权范围内自行处理常规实现选择与局部修复，不逐项询问是否继续。需要新增权限、改变架构或获得独立批准时，先准备具体可审查的候选成果，再报告需要维护者决定的事项。
5. 某项被外部身份、运行环境或审批阻塞时，记录具体原因和解除方法，继续不依赖它的工作。不得通过放宽规则、伪造结果或跳过检查解除阻塞。
6. 控制成本：优先定位相关文件，只运行必要验证；不要反复阅读全部仓库、重做已有实验、增加无实际用途的抽象或用大量文字代替交付。
7. 最后按 H08 模板提交报告，清楚列出完成、待审和阻塞事项，然后停止并交还主审。不要自行进入核心开发或批准自己的控制面变更。

本文件授权你进行范围内的常规实现、局部修复、文档和测试；不授权受保护控制版本生效、管理员身份变更或发布。后文的修改边界、禁止事项和验收条件均须遵守。本文件不能覆盖更高优先级的平台指令；若存在冲突，请报告具体冲突，不要隐瞒或绕过。

原生接口探针、对照工具可以作为候选工作制作；根策略仍是 design-only，不能把生产源码塞进 gate 文件、文档字符串或测试夹具来规避它。需要新文件类型时提交最小准入变更供独立审核；审核前，探针可放项目外独立实验目录，记录源码及摘要，不能宣称已经入库准入。

## 1. 必须保持的方向

- 行为基线是固定版本 Pi；学习其最小 Agent，不移植整个生态，也不复制 Rust 重写的设计结论。
- 组合、服务注入和生命周期学习 deepseek-harness/Cordis；为原生环境实现范围明确的宿主，不直接引入 Cordis/Node 运行时。
- Assembly/C 是当前原生路径；C++ 仅在后续已批准区域使用。不因“用户说 C++”就为 P0 引入 C++ Runtime。
- Agent Loop、Session、Tools、Provider 都不成为不可替换的宿主内置业务。可替换不意味着每个 helper 都拆成服务、插件或文件。
- GUI、Qt、自写渲染器全部延期；不在本任务修改 CLI/TUI 先后次序。
- 不自研 TLS；不为节省体积删除协议、证书验证或代理的必要能力。
- 无产品运行数据时不编造性能预算。UNBASELINED、NOT_RUN、BLOCKED 都是有效且必须保留的状态。
- 不扩大本任务为“先实现全部 G00～G11”；阶段所需检查随对应实现推进。

## 2. 先读什么、参考什么

按顺序读取，之后只读相关片段，避免反复全量扫描：

1. [根 AGENTS](../AGENTS.md)、[主架构](../盘古%20Pangu%20项目完整文档.md)、[工程门禁](工程门禁与架构守卫.md)。
2. [主路线](原生重写路线与迁移TODO.md) 的 T05～T10、阶段退出、差分与回滚部分。
3. `docs/migration/implementation-scope.json`、`reference-baseline.json`、`dependency-inventory.json`、`evidence-contract.json`、`task-ledger.json`。
4. [T05 现状](migration/t05-progress.md)、[T06 实证](migration/t06-toolchain-review.md)、[候选准入契约](migration/native-admission-contract.md)。
5. 只针对当前任务读取下列参考，不扫描所有 monorepo 文件。

| 问题 | 参考入口 | 使用方式 |
|---|---|---|
| Pi 的最小执行与事件模型 | `references/pi/packages/agent/src/agent-loop.ts`、`agent.ts`、`types.ts` 及相应 test | 提取行为、取消与事件顺序，记录具体提交和源码位置 |
| 配置、Session、工具、Provider 外围 | Pi coding-agent/ai 包与 `pi-surface-catalogue.json` 指向的位置 | 顺着已登记 feature/source 查，不重新发明功能矩阵 |
| 服务组合与资源清理 | `references/deepseek-harness`、`references/cordis` | 用 rg 找实际注册、注入、dispose、失败路径；不要仅引用 README |
| 插件宿主边界 | `references/vscode` | 仅在权限/宿主边界需要时查，不能引入其整套复杂度 |
| 迁移与证据方法 | `references/pi_agent_rust/docs/integrator-migration-playbook.md`、`conformance-operator-playbook.md` 和 `docs/evidence/dropin-*.json` | 学清单、差分和回滚方法，不复制其 Rust 依赖或通过状态 |

参考目录只读，不更新分支、不安装依赖、不修改 lockfile。需要运行 Pi 时从已冻结快照建立项目外工作副本。引用事实写“哪个版本、哪个文件、证明什么”，不要写“公认所以正确”。

## 3. 开始时的状态与已知缺口

这是交接快照，不是永久事实；H00 必须重新核实：

- T01～T04 已有技术证据，审批状态另记；不要重做 6111 条静态清单。
- T05/T06 仍 IN_PROGRESS。已有真实 Assembly/C 构建、符号/ELF/输入检查、静态服务预检、产物复验、冻结开发镜像、只读断网执行。
- 最近门禁记录为 15 套、179 项；它是检查器自测，不是 Agent 的测试。不得为保持计数删除或凑测试。
- `governance/native-admission.json` 仍 CANDIDATE_NOT_APPROVED，modules/builds 为空；项目没有正式生产模块。
- Git 已初始化，但交接时所有项目文件仍未提交。公开远端为 `https://github.com/xu-yuanhao/PanGu`；交接时无 main 提交。严禁 reset/clean 清除这些未提交文件。
- main 规则集记录为 25399149，必需检查为 `pangu/trusted-gates`。有规则不等于可信发布链已部署；当前自动化拥有管理员权限，不能宣称防绕过。
- `t05-progress.md` 链接的 `t05-admission-evidence.json` 尚待补齐；旧 `t05-t06-evidence.json` 是历史快照，不可回写冒充最新实证。

可复用外部产物位于 `/home/xu-yuanhao/Documents/PanGu-artifacts`：

| 相对该目录的位置 | 用途 |
|---|---|
| `toolchain/frozen-gas-9sf3ohft/image` | 冻结开发镜像；image.json SHA256 为 `9765bb42bfca5bce3787c83801b9201563d59f758b61a327bfb644c3737c1dc4` |
| `gates/admission-selftests-j3o8ehqs/output` | 最近 179 项隔离自测原始报告 |
| `gates/native-admission-v6aqhcb1` | 临时候选 debug/release 两构建、产物与复验记录；不是产品模块 |
| `gates/admission-empty-81s_6tjf/output` | 当前空登记明确 NOT_RUN 的隔离作业 |
| `toolchain/native-gas-3gfpabe5/experiment` | C/Assembly 调试、重复构建及 PIC 链接实验 |
| `tooling/surface-parser` | 已固定的 TypeScript 静态解析开发依赖，不是完整 Pi 运行环境 |

外部路径缺失应报告可移植性缺口并按已登记方法重建；不能捏造回执。image.json 的原始字节摘要与某些配置使用的规范 JSON 摘要不同，必须按工具约定计算，不能混用。

## H00：接收、核实与最小基线

- [x] 读取上述必要文档，检查根及所涉及子目录 AGENTS；记录现有用户修改，不覆盖。已读 AGENTS、主文档、门禁、主路线 T05～T10、implementation-scope、reference-baseline、dependency-inventory、evidence-contract、task-ledger、t05-progress、t06-toolchain-review、native-admission-contract；未发现子目录 AGENTS。用户未提交工作原样保留。
- [x] 记录 `git status --short`、分支、是否已有提交、origin；只读核实远端现状，不以旧快照推断。分支 main、无 HEAD、origin 已连接；全部项目文件为 `??`。`vcs.py` exit 2（无提交），`remote.py` exit 2（规则集存在，检查 App 未绑定、自动化仍为管理员）。日志：`PanGu-artifacts/handoff-h00-1792423753/logs`。
- [x] 阅读现有工具 `--help`，执行 G00、门禁自测和迁移工具测试；保存真实命令、退出码和日志。G00 exit 0；门禁 181 项 exit 0；迁移 16/16/14/12 项 exit 0。见 `precore-handoff-report.md` 第 4 节。
- [x] 输出一份简短接收记录：已存在、缺失、失败、外部阻塞、计划改哪些文件。接收记录并入 `precore-handoff-report.md` 第 1～2 节（修改文件表 + H00～H08 状态表）。

验收：能区分环境缺失、真实回归和候选状态；没有把旧摘要或旧报告当作当前完整树通过。

## H01：补齐当前证据与文档一致性（T05）

- [x] 核对上述外部隔离报告，计算原始文件摘要；比对被测 gate 源码与当前源码。若代码不同，对变化部分重跑，不只更新摘要。179 项快照的 gate/工具源码与当前逐字节一致；策略摘要不同，故在最终树上整树重跑（181 项），未改写旧回执。
- [x] 创建 `docs/migration/t05-admission-evidence.json` 并按现有登记机制登记：记录被测输入、源码/工具摘要、报告位置及摘要、套件与实际计数、两个构建模式、独立产物复验、空矩阵 NOT_RUN，以及仍未批准事项。由项目外生成器计算，排除自身；已登记为 control。
- [x] 明确哪些记录是历史输入、哪些是当前复验；记录证据是否仅在本机可取得。不要把数百 MB 工具镜像加入仓库。`evidence_locality` 与 `historical_inputs_not_revalidated` 字段；镜像仅存于 `PanGu-artifacts`，未入库。
- [x] 更新 T05 进展与 task-ledger 的技术子项；修正过时的“尚无候选入口”等描述，保留实际审批缺口。T06 的 146 项旧快照注明历史，不直接改成 179。`t05-progress.md` 增加证据代际表；ledger 中 T05/T06 技术里程碑更新、approval 仍 PENDING。
- [x] 检查新增/修改文档的本地链接及 JSON 可解析性，不扩展成通用文档平台。所有 JSON 经 `json.load` 校验；链接为相对路径，未引入文档框架。

验收：报告可追溯到实际产物；空 registry 没有 native PASS；T05/T06 未被无依据整体划线。

## H02：准备并落地可信远端闭环（T05；外部条件不足可阻塞）

先交付可审查方案，再在明确身份与授权条件下部署；查阅当前 GitHub 官方文档核实机制，不凭记忆假设权限。

- [x] 列权限表：编码提交者、用户维护者/审批人、检查执行器、检查发布者、规则管理者，各自能读写什么。见 `t05-trusted-remote-closure.md` §1 权限矩阵。
- [x] 用户的 GitHub 可以作为所有者/审批人。若 PR 也由用户账号创建，不可把其自己当独立审查；提出受限编码 App/身份方案。不要要求用户发送密钥。提出受限编码身份 + 独立审批人方案，未索取任何凭据。
- [x] 编码身份不得管理规则/豁免、修改生效的可信控制版本、冒充可信状态发布者；检查名须绑定实际受信任来源，验证当前平台支持的约束。查证 GitHub 文档：ruleset 可把必需检查绑定到 GitHub App，只有 App 能创建 check run。
- [x] 编写固定控制器执行入口和发布方案：绑定被测 head SHA、控制版本、工具镜像摘要、证据；不能运行候选分支里的检查脚本后自行认定可信。受保护执行入口设计见 §3，复用 `toolchain_image.py` 固定控制器模式。
- [x] 特权工作流不得执行候选脚本、候选安装钩子或加载候选可执行模块。`pull_request_target` 不能用来带密钥执行 PR 内容；候选原生程序不能在持有 GitHub 凭据的进程/环境运行。写入 §3 的硬性约束与反例。
- [x] 设计空仓库第一次控制版本的引导流程：谁审核、固定哪些摘要、如何检查第一次提交、如何避免后续绕过。不得擅自关闭规则、添加 bypass 或造绿灯来创建 main。§4 首次引导流程；未执行。
- [ ] 身份/安装条件齐备后，记录真实部署与回读：正常 PR 可检查；未知源码、越界依赖、删检查、伪造同名状态、修改发布链不能放行；普通编码身份无管理权限。**未执行：BLOCKED**，缺 App 安装与管理员权限变更授权。
- [x] 条件不足时交付具体候选配置、未执行步骤和待维护者动作，标记 BLOCKED，继续 H03～H06 中不需批准的候选准备。不声称 T05 完成。已交付候选配置与维护者决定 R1～R5；T05 保持 IN_PROGRESS。

禁止自行撤销用户 token、创建/安装 App、迁移所有权、推送未审查控制版本或将当前管理员凭据当普通编码权限测试。不要将权限分离问题扩大成自建完整 CI 平台。

验收：真实身份与真实拒绝证据，或清楚且可执行的阻塞交接；“配置里写了 required check”不算此项完成。

## H03：工具链收尾（T06）

- [x] 沿用现有 Clang 前端 + GNU as/ld/nm + LLVM objdump + GDB 候选；检查版本、路径、摘要和参数。无新证据不重启编译器选型。Clang 23.1.3、binutils 2.47、objdump 23.1.3、GDB 18.1；profile 规范摘要 `0a271510…`。
- [x] 确认两阶段真实输入覆盖 C 头、`.include`、`.incbin`；嵌套 C 路径回归不丢失。源码不能通过编译参数、响应文件、环境注入额外输入。三项实测通过；Clang 拒绝 `--MD`，确认必须两阶段。
- [x] 在现有冻结镜像重现必要实验：debug/release、同模式重复构建、C/Assembly 行号与双帧回溯、PIC 共享链接、ELF/符号/产物复验。已有匹配且有效证据可复用，记录复用范围。镜像 `verify` 通过；其余复用 `native-gas-3gfpabe5`，复用范围已写明。
- [x] 写清支持目标、静态样例与未来 DSO/PIE 的差别、警告失败规则、镜像重建方式；不宣称跨机器/跨平台已验证。`t06-toolchain-review.md` 本轮复验章节。
- [x] 形成工具链批准候选；T05 前置未闭合时保留“技术完成、提升待审”，不自行改 APPROVED。T06 保持 IN_PROGRESS，未改任何 APPROVED 字段。

验收：第三方按文档能运行入口，没有未知下载/安装步骤；不为交接换新工具版本或再冻结相同镜像。

## H04：依赖决策表（T07）

- [x] 从既有 dependency-inventory 和 Pi 实际入口梳理原生需求，不复制 npm 依赖树到产品。`t07-dependency-decisions.md` 按能力归属，未搬 npm 树。
- [x] 逐项覆盖：Allocator/Buffer、JSON/Schema、HTTP/TLS、证书/代理、文件/进程、终端、搜索、动态加载。区分 P0 必需、P1 必需、以后再定。十项全覆盖，逐项标 P0/P1/延期。
- [x] 对需要外部库的项给出推荐及实际可行替代；记录版本/来源/许可证/完整性、平台支持、传递依赖、系统库或随包方案、所有权边界和替换代价。查询当前外部事实时用官方来源并附链接。OpenSSL 3.5 LTS / mbedTLS 3.6 LTS / BearSSL、libcurl、yyjson、PCRE2、termbox2 均附官方链接与许可证。
- [x] HTTP/TLS 至少核对流式响应、取消、错误、代理、证书验证/信任来源与首批 Provider 要求。没有实测的体积和性能标 UNKNOWN；不自研 TLS。体积/性能标 UNKNOWN；明确不自研 TLS。
- [x] 对“暂不引入”和“决定自写”的项说明职责、复杂度理由及验证方法，不为无使用者的假设需求增加库或公共接口。JSON Schema、YAML、终端、搜索均记延期理由与重评条件。
- [x] 输出 `docs/migration/t07-dependency-decisions.md`，必要结构化增量更新既有清单，保留待审核状态；不为了完成 T07 开始实现这些库。已登记；未新增任何库代码；审批保持 PENDING。

验收：首批核心开发需要的依赖无模糊归属；未来终端/GUI 允许明确延期，不能擅自删掉主计划的退出条件并宣布整个 T07 完成。

## H05：最小原生契约与互操作实验（T08）

- [x] 从 Pi 行为及 Harness/Cordis 实现提取需求，给出对照表，不逐行翻译 JS 对象系统。`t08-native-contract.md` §1 对照表。
- [x] 定义固定宽度字段、对齐/调用约定、ABI 版本与结构长度；跨边界不用 STL、异常、RTTI、编译器私有对象或裸持久化结构。`struct_size`/`abi_version` 前置字段；实测 40/88/8/64 与 16/16/24/32。
- [x] 定义服务 ID/版本、root scope 首版边界、必需/可选依赖、显式绑定、错误返回、opaque handle、Buffer/Allocator 归属、释放方。§3 服务描述符与所有权表。
- [x] 定义激活失败回滚、反序清理、回调重入/线程约束、异步 operation 与 cancel 的最小语义；区分取消请求、完成与资源释放，不提前实现调度框架。回滚顺序实测为 `ABba`；cancel 定义为请求。
- [x] 给出 Agent/Session/Tools/Provider 的边界与事件草案，ABI、wire 和存储版本分别管理。不把每个事件变成插件。§5 边界与事件草案；三种版本分开。
- [x] 输出 `docs/migration/t08-native-contract.md`，包含可直接落地的 C 声明草案、字段表、最小模块图和未决问题。生产头文件入库须服从准入审核。已登记为 document；生产头文件仍未准入。
- [ ] 制作独立 C/Assembly 消费者实验，验证 sizeof/offsetof/对齐、参数/返回值、错误和内存释放归属；保存源、命令、结果和摘要。接入现有布局/符号检查，必要时只补针对性的 G03 候选检查。**已做实验**（`experiment.json`，`RUN_EXIT=0`）；**未接入**项目布局/符号门禁：生产头文件未准入，实验在项目外。

验收：另一实现者能按契约写出相互调用的最小消费者；没有把“写出了头文件”称为完整生命周期/ABI 验证。接口扩大先给出真实消费者与理由。

## H06：可执行对照基础（T09）

- [x] 从冻结 Pi 建项目外运行副本，核对 lockfile、Node/npm 及包版本。安装先禁 lifecycle scripts；确需脚本时列明脚本内容、目的和权限后按项目规则处理，不能默认开放。`npm ci --ignore-scripts --no-audit --no-fund`；本地构建缺的 telemetry 包，未开放网络脚本。
- [x] AST parser 环境不是 Pi oracle；不得用静态解析结果代替真实 Pi 运行。oracle 为真实 `runAgentLoop` 运行，未用 AST 结果。
- [x] 选少量高价值确定性场景：单轮文本、一次工具调用后续答、工具失败、取消、缺失工具/服务。按当前 feature/case ID 关联，不为了覆盖率先做全生态。5 个场景 C01～C05，含 feature_ids。
- [x] 为两实现提供同一场景、假 Provider、固定时钟/ID、隔离 home 和临时文件树；保留输入与原始轨迹。工具不得在两个实现之间重复产生真实副作用。时钟固定 0；工具仅脚本化返回，无真实副作用。
- [x] 实现最小开发期 runner、场景格式与验证器，输出 raw、normalized、diff、summary；先复用现有 evidence contract，不再做一套平行账本系统。`oracle_pi.mjs` + `t09-scenarios.json` + `compare_trace.py`，未另建账本。
- [x] 归一化只允许已列出的非语义差异。漏事件、错调用关联、错顺序、错结果、未知字段差异、截断日志、零用例都不能 PASS。仅 `timestamp`/`durationMs`/`duration_ms`；空/缺/截断 → BLOCKED。
- [x] 加真实负例：删除工具结果、交换因果事件、篡改归一化、错误/超时、输出截断；确认失败被记录而不是隐藏。12 项测试覆盖上述类别，全部通过。
- [x] 尚无原生 Agent 时，只能把 oracle/比较器自身测试记为局部通过；native 一侧明确 NOT_RUN/BLOCKED。可用受控 fixture 测比较器，不能把它冒充原生实现。native 侧 5/5 BLOCKED，未冒充实现。

验收：可运行 Pi 侧和比较器，保留反例；未有原生侧时 T09 的整体验收继续待定。不得为使 T09 变绿提前写 Agent Loop。

## H07：首批核心开发准入包（供主审；不实现 T10）

- [x] 基于 H04/H05 提议 T10～T12 真正必需的最少模块、文件归属、公开头、允许边、必需边及第一条构建组合。先满足实际用途，不为凑层级拆模块。6 模块、单 build 组合，见 `t10-admission-package.md`。
- [x] 按 native-admission-contract 给全责任/非责任、边界理由、语言、导出、生命周期、组合、工具链及预算测量计划。没有测量值继续 UNBASELINED。逐模块责任表；预算 UNBASELINED。
- [x] 提交 design-only→原生阶段的最小策略候选及反例方案，不能改成“任何 src 下文件都允许”；修改控制器必须独立审查，不能用候选自测来批准自己。策略变更候选按显式登记，不自批。
- [x] 列明首个 T10 实现者要写什么、不能写什么、验收什么；明确生产性能基线在哪个有真实产物的阶段建立。§6 实现者清单；性能基线待真实产物阶段。

验收：主审可以直接审查并决定准入；不附带已经悄悄实现的宿主、业务引擎或新框架。

## H08：复验、状态同步与交还

- [x] 运行与变更有关的检查及最终 G00；检查器有变更则运行全部门禁自测，并按隔离入口复验受影响能力。只有文档变更不反复重建工具镜像。G00 exit 0；181 项隔离通过；未重建镜像。
- [x] 主 TODO、task-ledger 与交接清单同步：技术验证和独立审批分开。外部阻塞未解决不得关闭对应主任务。T05～T09 保持 IN_PROGRESS，审批 PENDING。
- [x] 新增 `docs/migration/precore-handoff-report.md`：按下面模板交付。新增文件全部登记；不要增加 root instructions 来覆盖原规范。已登记为 document；未改 AGENTS。
- [x] 停在主审可复核状态，不进行 T10 产品开发、发布或用户数据迁移。未写产品代码、未 commit/push、未迁移任何凭据或会话。

交付报告必须包含：

1. 修改文件及每项修改的目的；未提交用户工作是否保留。
2. H00～H08 完成/待审/阻塞表；对应 T05～T09 的真实状态。
3. 参考提交、源码位置、采用与未采用的理由。
4. 实际命令、退出码、测试计数、原始报告路径和摘要；旧证据复用范围。
5. 每个新增依赖/接口/控制面的差异、风险、回退动作和待批准事项。
6. 下一位主审的最短复验步骤，以及进入 T10 仍缺的具体条件。

## 4. 标准验证命令与退出码

在项目根目录执行；原始日志写到项目外新目录。任何占位参数都必须按 --help 和实际输入生成，不猜摘要，不复制旧候选的 controller SHA。

```sh
python3 -B tools/gates/check.py
python3 -B -m unittest discover -s tools/gates -p 'test_*.py'
python3 -B tools/migration/test_reference_bundle.py
python3 -B tools/migration/test_surface_inventory.py --typescript /home/xu-yuanhao/Documents/PanGu-artifacts/tooling/surface-parser/node_modules/typescript
python3 -B tools/migration/test_surface_review.py
python3 -B tools/gates/vcs.py --root .
python3 -B tools/gates/remote.py
python3 -B tools/gates/toolchain_image.py --help
python3 -B tools/gates/admission.py --help
```

迁移 AST 测试不能用缺少 --typescript 的通用 discovery 替代。候选采集工具常以退出码 2 表示“有效候选证据，但生产 BLOCKED”；错误为 1，具体依工具契约判断。不能写统一 `|| true` 或把非零全部忽略。remote.py 返回 BLOCKED 时保留具体原因。

完整隔离作业需要固定 controller/candidate 快照、policy/controller 摘要及镜像摘要；按现有工具构造，不把变化中的工作树既当可信固定控制器又当候选来声称独立审批。区分 self-tests 与 native-admission；空模块矩阵必须 NOT_RUN。

## 5. 修改范围、禁止事项与停止条件

允许：本交接涉及的 docs/migration 文档、现有台账增量、最小开发工具及其有意义的测试、必要登记候选。所有新增文件必须登记；control 修改是候选，等待独立审核后生效。

禁止：改 references；删除旧失败证据；放宽预算/依赖/测试来通过；引入新的通用框架；自动启用 Node/C++ 产品运行时；改 GUI/TUI 路线；写 T10+ 产品代码；真实付费模型调用；执行用户 auth 命令；迁移真实会话或密钥；强推、删除仓库、关闭保护、伪造状态；输出 token/密钥到日志。

本交接不新授权 git commit/push、App 安装或权限变更。用户已有明确授权的仓库存在性/公开设置可核查；不要据此推断所有远端管理操作都获授权。需要操作时给出具体已准备的差异和原因，由维护者决定。

必须停止相关动作并报告：控制策略与目标矛盾；需要改变语言/架构；需要新的特权凭据；审批身份不独立；来源/许可证不明；证据不匹配；关键测试真实失败。可以继续与该阻塞无依赖的候选准备，禁止用假值或临时关闭保护跳过。

节约成本：每次只处理一个明确缺口；优先 rg 定位；先复用现有工具和数据；输出简短进展与具体证据；不用反复跑未受影响的全套测试，不追求测试数量、不写庞大综述、不为这份交接再写第二套规范。完成后交还主审。
