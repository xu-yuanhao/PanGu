# 盘古原生重写路线与迁移 TODO

状态：T01～T04 已完成技术验收；T05～T09 与生产准入尚待闭环，T10～T14 原生本地实验已有记录，T19～T28 的核心子集推进中，首个真实模型只读 TUI 已接通。勾选和划线仅表示对应技术条件已有证据，不表示整阶段或上线通过。控制面按根 AGENTS 当前规则记录审查批准（用户已授权 Agent 自审自批）。参考仓库已存在，不等于新实现已经兼容。

## 1. 这次重写的准确含义

盘古是“Pi 行为基线 + deepseek-harness 式服务组合 + Assembly/C 原生基础 + 最少量 C++ 桥接”的新实现。Pi 决定要复现哪些用户行为；deepseek-harness 与 Cordis 决定如何拆开和组装服务；盘古架构与门禁决定实现边界。`pi_agent_rust` 用于学习重写的组织、功能盘点、差异证据与迁移方法，不作为盘古功能真相，也不把它的完成声明转移给盘古。

两项工作分别进行：

- **实现迁移**：把 TypeScript 提供的行为变成原生服务；不把 JS 语法、对象模型、GC、动态导入逐句翻译成汇编。
- **使用方迁移**：让 CLI、RPC 客户端、配置、会话和已有工作流逐步改用盘古；这需要独立兼容适配，不能因为算法重写完成就直接切换。

没有已经部署的盘古 TS 生产版本。这里的“旧系统”指固定版本的参考 Pi 或未来试用方正在使用的 Pi；不擅自替换其安装、凭据或数据。正式开始迁移时还要登记该使用方的实际版本，不能假设等于本地参考版本。

## 2. 文档、清单与权威

- [主架构](../盘古%20Pangu%20项目完整文档.md)：产品目标、宿主边界、服务及执行位置。
- [工程门禁](工程门禁与架构守卫.md)：G00～G11、控制面审查记录和阶段准入。
- [参考快照](migration/reference-baseline.json)：本轮本地提交、关键文件摘要、工作树情况；不承诺任意未来 main。
- [功能迁移清单](migration/feature-inventory.json)：带 ID 的初始功能族、兼容策略、任务、用例与当前状态。
- [依赖清单](migration/dependency-inventory.json)：Pi 直接依赖快照、语言替换方向及原生候选评审事项。
- [差异证据规范](migration/evidence-contract.json)：每次运行需要的元数据、判定及证据完整性规则。它是待实现验证器的输入规范，不是已运行的证据套件。
- [任务追踪](migration/task-ledger.json)：T01～T48 的前置、状态和功能关联；默认串行，是保守调度，不要求为每一步建立独立代码模块。
- [差异与阻塞账本](migration/gap-ledger.json)：当前已知的设计、验证和使用方迁移缺口；后续缺口追加 ID，不覆盖历史。

这些机器可读文件是登记起点，需在 T03/T04 展开到具体 flag、配置键、事件、方法和工具参数。功能族一行不代表全族已穷举。任何新增公开表面必须新增清单项或子项，并链接实现、用例和门禁。

参考优先级：盘古明确批准的有意差异 → 固定 Pi 源码和可重复运行结果 → 同版本 Pi 文档。三者冲突时记录差异并审查，不自动把上游 bug 当作盘古必须保留的功能。deepseek-harness 作为组合语义来源，不覆盖 Pi 的会话/CLI 行为。

### 2.1 从 Rust 重写项目吸收的方法与保留意见

本轮克隆位置为 `references/pi_agent_rust`，固定提交 `9fd02e42438fb01ca7766764a36bceb0330b9a87`。没有安装依赖、运行其测试或修改参考源码。

| 指定材料 | 采用的方法 | 盘古的调整 |
|---|---|---|
| [迁移手册](../references/pi_agent_rust/docs/integrator-migration-playbook.md) | 先盘点使用方表面，再迁移配置/认证、逐表面验证、签核及回滚 | 首先双入口并存，禁止默认接管 pi；增加数据隔离、Session 导入和副作用结算 |
| [功能矩阵](../references/pi_agent_rust/docs/evidence/dropin-feature-inventory-matrix.json) | 稳定行 ID、elements、来源证据、未知项与责任关联 | 重新从本地当前 Pi 提取，策略/审批/实现/验证四者分离，不复制旧矩阵的状态 |
| [差异证据套件](../references/pi_agent_rust/docs/evidence/dropin-differential-evidence-suite.json) | 将不同表面的差异执行和证据聚合 | 要求原始产物、摘要、范围分母与独立重算，不能由手写总 PASS 代替 |
| [Conformance 操作手册](../references/pi_agent_rust/docs/conformance-operator-playbook.md) | 确定性 oracle、场景/corpus 分层、逐字段 diff、失败分类与工件位置 | 不继承 Rust 命令、绝对机器路径、旧 CI 阈值、自动接受基线或 warn-only rollback 模式 |

需注意这批材料的时间差：

- 当前 Rust [项目方向](../references/pi_agent_rust/AGENTS.md)已将旧 strict drop-in 计划列为历史，不能作为其当前发布权威。相关 [slash 差异测试](../references/pi_agent_rust/tests/dropin_slash_differential.rs)也存在明确的 retired/ignored 标记。
- 其历史功能矩阵绑定 Pi 提交 `10bca71fcfc2c4cb7bbbdb4110644a8c5aaef768`；本地盘古参考 Pi 是 `2ec55d808d455b1a64a2bf7888198207a096120a`。例如当前 Pi 已有 `agent_settled`、新的配置/事件和 edit 参数行为，不能直接复制旧元素清单。
- 迁移手册引用的 `docs/dropin-upstream-baseline.json` 在本轮 Rust checkout 中不存在；盘古快照用缺失记录表示，不伪造哈希或当作已验证依赖。
- 历史差异套件有总 PASS/100% 字段，同时某组件描述 28/29；它们的范围可能不同，未重跑与解释分母前不能推导全表面兼容。盘古必须从实际必需 case 集重算，不照抄总结。

参考方法用于盘古自己的验收体系；不把 Rust 的 36 个工具、QuickJS、SQLite 默认配置或 Rust 运行库顺带移植进最小 Agent。

## 3. 兼容等级与发布组合

### 3.1 每个功能都要有结论

| 策略 | 适用表面 | 通过条件 |
|---|---|---|
| EXACT | 声称兼容的 flag、JSON/RPC 字段、退出码、选定工具参数 | 固定用例按字段、顺序、错误与副作用严格比较 |
| SEMANTIC | 调度实现、内存布局、UI 绘制细节等 | 明确可观察不变量，允许的差异列入规则 |
| INTENTIONAL_CHANGE | 权限隔离、新插件 ABI、品牌/目录等 | 有明确理由、用户影响、迁移方法和批准记录 |
| DEFERRED | 尚未实现的 Provider、JS 扩展等 | 保留清单和后续任务；对需要它的发布组合仍是阻塞项 |

支持状态另记为 `PLANNED / IMPLEMENTED_UNVERIFIED / VERIFIED / BLOCKED`，不能把“有意不兼容”写成“严格兼容已通过”。`VERIFIED` 必须链接有效证据；审批状态与验证状态分开。

### 3.2 发布组合

| 组合 | 必须具备 | 明确不隐含的声明 |
|---|---|---|
| P0 原生宿主实验 | 服务依赖、资源所有权、替换与故障测试 | 不可用作日常 Agent |
| P1 最小 TUI Agent | 一个明确的模型协议适配、read/bash/edit/write、流、取消、配置、持久化、最小安全策略，以及可对话的薄 TUI | 不宣称完整 Pi 替代、完整终端编辑器或 JS 扩展兼容 |
| P2 日常 TUI Agent | P1 + 队列/恢复/压缩、资源加载、会话管理和可用 TUI | 只覆盖已批准功能集合 |
| P3 选定工作流迁移 | 指定 CLI/JSON/RPC、配置、会话导入、该工作流全部必需依赖 | 不自动兼容 TS 进程内 SDK 和任意 npm 扩展 |
| P4 扩展与多端 | 经登记的桥接、MCP、daemon、远程、其他平台 | 独立认证、预算和发布矩阵 |

不允许用 P1 的通过率为 P3 签发证书。首版只选 x86-64/Linux；其他目标保留在路线里，在对应矩阵通过前标为未支持。

## 4. TypeScript 到原生技术栈的转换规则

| TypeScript 概念/依赖 | 原生表达 | 转换时必验行为 |
|---|---|---|
| interface、类型联合 | 版本化 C 结构、tagged union、服务函数表 | 明确宽度、对齐、unknown 字段和版本处理 |
| Promise、async/await | operation 句柄、完成回调或 poll、事件循环 | exactly-once 终结、取消/完成竞争、回调执行上下文 |
| AbortSignal | 所属操作的取消令牌与原因 | 传播、幂等、资源清理、在途结果处理 |
| EventEmitter/回调闭包 | 带 owner 的订阅、函数指针 + user_data | 捕获数据有效期、分发快照、退订和卸载顺序 |
| GC 对象、Array、Map | 显式分配器、arena、vector、hash table、句柄 | 生命周期、容量上限、整数溢出、失效引用 |
| string/Buffer | UTF-8 slice、owned buffer、显式长度 | JS UTF-16 与 UTF-8 索引差异、NUL、边界和复制规则 |
| number / undefined / null | 字段级整数/浮点类型、presence bit、tag | 超过 2^53、负零、缺失与 null、时间/usage 精度 |
| throw/catch | 稳定错误码、诊断结构、清理路径 | 错误分类、调用者可见信息、异常不跨 ABI |
| jiti/动态 import | 插件清单、原生入口、静态/动态注册 | 发现与执行分离，不能原样加载 JS |
| TypeBox/Schema 验证 | 选定 JSON Schema 子集与校验服务 | 默认值/coercion/额外字段的明确支持范围 |
| Node fs/process/net | PAL 与文件/进程/网络服务 | cwd、路径、权限、信号、句柄、代理、证书 |
| npm 包安装 | 原生清单、包来源校验与受控安装 | 传递依赖、许可证、构建脚本、离线与回滚 |

实现顺序为接口与行为用例 → 最小原生默认实现 → 正确性/生命周期 → 实测热点汇编优化。关键底层原语优先 Assembly；复杂平台胶水可先用精简 C。不得把生产原生版本长期变成“全部 C 的临时实现”，也不能为语言比例写无收益汇编。每个模块登记语言决定；无阶段性百分比配额。

C++ 首先只用于 Qt 桥接等批准区域，P0～P3 默认不依赖 C++ Runtime。参考仓库里的 simdjson、llama.cpp、Qt 等不因存在就自动进入依赖。TLS 不自研；具体库在 T07 比较协议、平台与部署成本后确定。

## 5. 顺序总览与里程碑

T01～T48 保留为功能追踪编号与整体验收清单，不再表示必须逐项全量验收的串行开发顺序。当前执行以下纵向队列；各步骤旧输入列表用于完整阶段验收，局部实现只要求实际使用的依赖和直接安全条件已具备。未完成的阶段不勾选，不把局部可用描述成全量兼容或生产放行。

| 里程碑 | 步骤 | 对应主文档阶段 | 退出结果 |
|---|---|---|---|
| M0 设计与准入 | T01～T09 | A | 固定范围、工具链、依赖与硬门禁 |
| M1 原生宿主 | T10～T18 | B | P0，生命周期与替换得到验证 |
| M2 最小闭环 | T19～T28 | C | P1，无真实网络也可重复验收 |
| M3 日常 Agent | T29～T35 | C/D | P2，核心 Pi 工作体验可用 |
| M4 工作流兼容 | T36～T40 | D | P3 的指定表面达到候选状态 |
| M5 试用与切换 | T41～T46 | D | 有证据、可回退的受控发布 |
| M6 后续组合 | T47～T48 | E | 分别认证平台与扩展，复盘基线 |

### 近期执行队列（2026-10-24）

并行分工：主 Agent 负责 Runtime/会话/模型/Loop/Tools/统一构建和共享控制文件；第二位 Agent 按 [TUI 任务提示词](并行开发任务提示词-TUI.md) 在独占目录开发薄客户端与终端恢复。双方先用假源解耦，主 Agent 最后固定 C 接口、登记并集成；不同时修改共享文件。

本队列优先于各步骤旧的串行前置措辞；机器账本的依赖保留为整体验收依赖，不作为本地子集实现的全局锁。

- [x] 收拢路线和门禁范围，停止以测试数量和报告数量作为进度；保留历史验收状态。
- [ ] A：整理现有 T10～T14 可复用源码，确定第一闭环实际模块/依赖/构建入口，登记最小组合；将需复用的实现放入可追踪源码，不继续扩展宿主验证框架。仅补妨碍编译或直接危及当前调用路径的缺口。
- [x] B 的连续会话子集：系统提示、消息投影、空闲时清空会话、工具关联校验；真实MiMo多轮回忆/清空/取消后继续验证通过。未实现完整Session树、持久化、资源发现与预算投影。见 [核心进展](migration/runtime-core-progress.md)。
- [ ] B：实现内存会话、最小消息/事件和假 Provider（T19/T21/T22 子集）；读取 Pi agent/types、agent-loop 与测试，固定文本/工具调用/错误/取消用例。不等会话树、JSONL恢复、完整配置。
- [x] C 的首个局部切片：原生内存会话、注入模型/工具接口、有界 poll Loop、单工具往返、拒绝/错误/取消及 debug/release UBSan 验证；见 [核心进展](migration/runtime-core-progress.md)。不代表完整 B/C、Pi 兼容或 T23/T24 完成。
- [ ] C：实现 Agent Loop 与 Tools 注册/调用（T23/T24 子集）：输入→流式响应→工具请求→结果回填→继续响应；含轮次上限、未知工具、失败、取消及资源清理。以假 Provider 验证完整往返与替换，不把业务写进 Bootstrap。
- [x] D 的首个真实子集：MiMo Token Plan CN + mimo-v2.6-pro，经原生HTTPS适配与C Loop完成一次真实工具请求/执行/回填/续答；见 [真实验证](migration/mimo-live-progress.md)。真实SSE文本分片、工具参数组装及流中取消现已验证；生产依赖认证及完整T27尚未完成。
- [ ] D：接入一个真实 Provider（T27），登记 HTTP/TLS 依赖、超时、取消、响应/输出上限与凭据读取；使用本地配置或环境凭据，不打印密钥。先流式对话再验证一次真实工具往返，不等待多厂商兼容。
- [x] E 的只读子集：显式工具注册/参数语义校验/授权/执行链，工作目录内小型UTF-8普通文件read；真实MiMo读取临时sample.c并正确回填标记。路径/符号链接/输出上限拒绝已测，不代表Shell/write/edit或完整T24/T25。见 [核心进展](migration/runtime-core-progress.md)。
- [ ] E：接入最小 read 和受控执行工具（T25/T26 子集），明确工作目录、写入/命令授权与取消、超时、输出上限；在临时工作区验证成功和拒绝路径。完整工具族随后补齐。
- [x] F 的首个只读体验：薄TUI已桥接真实Runtime/MiMo/SSE/read，PTY实测工具续答、取消与终端属性恢复；构建入口 `tools/runtime/build_pangu.py`。尚不代表完整P1或T28，write/edit/bash、持久化与配置界面仍待完成。见 [核心进展](migration/runtime-core-progress.md)。
- [x] F：~~交付首个简单 TUI~~（T28），输入编辑、流式展示、工具状态、取消、退出与终端恢复；只消费 Runtime 接口。可在 C 阶段提前搭薄客户端，真实模型/工具就绪后完成体验验收。内部测试入口不扩成全功能 CLI。
- [ ] G：验收可运行的原生纵向切片：可重复构建、假模型确定性往返、真实模型对话/工具往返、错误与取消、TUI终端恢复，并记录运行命令和限制。然后按真实使用反馈推进持久化、配置、重试及完整生命周期。

第一闭环暂缓：T14 新增穷举专项、全 G04/G07 认证、远端 App/管理员分离、动态加载/热替换、多平台、全量 Schema/数值能力、Pi 全功能兼容。出现当前路径的真实缺陷仍须修复，不以暂缓掩盖失败。

“一切皆插件”在首个闭环的含义：Loop、Model、Tools、Session、终端和所用基础服务有清晰接口与显式组合，默认实现可静态注册，至少对模型/工具及复用基础服务验证替换；不是每个 helper 都做插件，也不要求首版任意热卸载。汇编用于适合的底层原语与实测热点，C承担主要流程，C++使用须有明确边界，不为语言比例阻碍闭环。

<a id="T01"></a>

### T01：~~冻结参考和本地状态~~

- [x] 输入：本轮参考快照及主架构。保存 Pi、Harness、独立 Cordis、Harness vendored Cordis、VS Code 和 Rust 项目的提交与关键文件摘要。
- [x] 操作：记录跟踪文件修改、lockfile、许可证；脏工作树不能仅用 HEAD 标识。参考更新另开差异审查，禁止自动跟随 main。
- [x] 产物：可复现 reference bundle、来源说明、Pi 文档/实现冲突记录。
- [x] 退出：另一环境能定位相同内容；关键文件摘要一致。失败停止 M0，不改参考树来凑基线。

技术验收证据：[T01/T02 回执](migration/t01-t02-evidence.json)、[参考审查记录](migration/reference-audit.md)。控制面独立审批尚未进行。

<a id="T02"></a>

### T02：~~定义迁移对象与可声称范围~~

- [x] 输入：P0～P4 和试用者实际使用情况。没有使用方时先采用项目自用验收集。
- [x] 操作：明确必需 Provider、执行模式、扩展、Session 版本和目标平台；为有意差异登记审批状态。
- [x] 产物：每个组合的必需 feature ID、排除项、风险等级、负责人角色与 go/no-go 规则。
- [x] 退出：没有“全兼容但以后再补”的模糊声明；尚未批准范围不能用于上线。

技术验收证据：[T01/T02 回执](migration/t01-t02-evidence.json)、[参考审查记录](migration/reference-audit.md)。控制面独立审批尚未进行。

<a id="T03"></a>

### T03：~~穷举 Pi 的外部表面~~

已完成静态盘点：322 个源码文件、42 份文档、6,111 条记录；30 条来源差异已分类并保留实现义务。见[盘点目录](migration/pi-surface-catalogue.json)、[审查记录](migration/t03-t04-review.md)。这不是运行兼容性认证。

- [x] 输入：固定 Pi 的 CLI、settings、env、slash、JSON/RPC、SDK、session、extensions 文档和源文件。
- [x] 操作：把功能族展开成 flag/别名/互斥组合、配置键/默认值/优先级、事件字段、RPC 命令、SDK 方法、扩展 API、工具输入输出、文件格式及异常行为子项。
- [x] 产物：子项清单，逐项关联源符号、文档和候选测试；统计未知项。
- [x] 退出：文档与导出/解析器反向核对，新增公开项不能不入账；族级清单本身不能算穷举完成。

技术验收：[T03/T04 审查与证据](migration/t03-t04-review.md)、[差异账本](migration/surface-gap-ledger.json)。独立控制面审批和运行验证未完成。

<a id="T04"></a>

### T04：~~建立差异账本和任务追踪~~

- [x] 输入：T03 清单。逐项分配 EXACT/SEMANTIC/INTENTIONAL_CHANGE/DEFERRED、优先级、目标组合、实现步骤和测试 ID。
- [x] 操作：每个不兼容项记录影响、适配方法和审批；严重程度依据数据/权限/协议风险，不依据开发方便程度。
- [x] 产物：gap ledger 与范围分母；无证据项保持 PLANNED。
- [x] 退出：所有选定组合的必需项可追踪；未解决的数据损坏、权限和关键协议问题阻断，不用平均通过率掩盖。

技术验收：[T03/T04 审查与证据](migration/t03-t04-review.md)、[差异账本](migration/surface-gap-ledger.json)。独立控制面审批和运行验证未完成。

<a id="T05"></a>

### T05：先搭受保护门禁和构建控制面

进行中：[实施记录](migration/t05-progress.md)。已实现 Assembly/C 实际编译输入、符号/最终 ELF、服务组合预检、回执、分离控制器与冻结镜像隔离复现；生产源码准入、原生绑定轨迹和外部受保护状态发布链尚未完成，生产准入仍为 BLOCKED。

- [x] 输入/本地版本管理：G00、工程门禁规范已具备；已初始化本地 Git `main`，排除参考仓库并通过 Git 索引边界检查，未暂存或提交。
- [x] 本地候选检查：模块、C/Assembly 两阶段依赖、公开头/符号、最终 ELF、服务组合与产物复验正反例已验证；设计阶段 G00 仍禁止生产源码。
- [x] 隔离演练：固定开发工具镜像、只读控制器/候选、断网且不挂载用户目录，十五套 179 项门禁自测通过；另有五种候选篡改的真实拒绝演练。
- [x] 候选源码准入：完整模块登记、全树默认拒绝、全部组合/模式真实构建、对象/ELF 字节预算及整矩阵复验已实现；正例与空登记分别验证 PASS 与 NOT_RUN，生产批准仍关闭。
- [x] 远端规则配置：按用户要求建立公开 GitHub 仓库，main PR/必需检查/防强推与删除规则已回读核实；仓库仍无提交，权限分离未建立。
- [ ] 远端保护：配置托管仓库、独立审批和外部受保护执行器，验证普通编码权限不能绕过。
- [ ] 操作：建立模块登记、允许依赖、工具链和受信任执行入口，G01/G02 及拒绝测试；区分产品、测试、开发期依赖。
- [ ] 产物：实际可执行的架构/构建检查、控制面审批规则、源码摘要回执。
- [ ] 退出：故意添加越界依赖、未知源码和删检查都不能获放行；运行环境未就绪时 BLOCKED。

<a id="T06"></a>

### T06：选择并固定原生构建与调试链

技术预研：[工具链比较与实证](migration/t06-toolchain-review.md)。候选为 Clang 前端 + GNU as/ld，已完成 Assembly/C debug/release、最终 ELF、重复构建、C/Assembly 调试回溯、PIC 共享链接及冻结工具镜像隔离复现；T05 前置与独立控制面批准仍待完成。

- [ ] 输入：x86-64/Linux 优先、Assembly-first、跨平台目标。
- [x] 本地实验：比较 fasm 文档与实际工具链能力，固定单一 GAS Intel `.S`/C17 候选；真实验证汇编输入、静态 ELF、调试、重复构建和 PIC 共享链接。
- [x] 隔离复现：工具及运行数据闭包已冻结，镜像摘要绑定；断网只读环境下重复构建和调试自测通过。开发镜像不进入 Agent 依赖。
- [ ] 操作：比较 fasm 与工具链兼容方案、C 标准/编译器、链接器、调试符号、位置无关代码、静态注册与 DSO 构建；决定一个首版方案，不能混用未登记方言。
- [ ] 产物：toolchain manifest、可重复的 debug/release 构建入口、反汇编/符号/size 检查流程。
- [ ] 退出：干净环境产生可解释产物；构建过程不执行未批准下载或安装脚本，告警处理规则明确。

<a id="T07"></a>

### T07：审查依赖与替换可行性

- [ ] 输入：依赖清单及 Pi lockfile。展开实际使用入口的传递依赖，而不是复制整个 monorepo。
- [ ] 操作：选定 TLS/HTTP/Schema/终端/搜索方案；分别比较系统库与随包交付、协议功能、证书/代理、平台、许可证和体积。BearSSL 等只是候选，不能因小就忽略 API 所需 TLS 能力。
- [ ] 产物：批准依赖、拒绝依赖、版本/哈希、构建来源和每项替代策略。
- [ ] 退出：无未登记运行时；需要 JS 的能力从最小原生组合剥离；不能把依赖迁移问题留给上线阶段。

<a id="T08"></a>

### T08：定义服务、ABI 与数据模型

- [ ] 输入：Pi 类型、Harness 服务划分、Cordis 生命周期及原生规则。
- [ ] 操作：定义引导 API、服务 ID/版本、作用域、句柄、Buffer/Allocator、operation/cancel、错误码、事件与 Session 接口；区分 ABI/协议/存储版本。
- [ ] 产物：头文件草案与布局探针、服务依赖图；Agent Loop/Session/Tools 不能进入固定宿主。
- [ ] 退出：独立 C/Assembly 消费者互操作；宽度/对齐/所有权可验证，接口变化接入 G03。

<a id="T09"></a>

### T09：先实现对照执行与证据骨架

- [ ] 输入：T01～T04 与 evidence contract。在工作副本/隔离构建目录运行固定 TS Pi，不修改只读 references。
- [ ] 操作：建立假模型/假时钟/固定 ID/受控文件树；两边接受同一场景、各自输出原始轨迹。协议、语义和性能比较器分别实现。
- [ ] 产物：可运行 runner、case manifest、raw/normalized/diff/run-summary；验证器与比较器负例。
- [ ] 退出：故意改变事件顺序、漏结果和不合法归一化会 FAIL；新实现尚无产物时 BLOCKED，不能套用参考测试的 PASS。

<a id="T10"></a>

### T10：引导、PAL 和最小进程

进行中：[首个原生实现与证据](migration/t10-bootstrap-progress.md)。

- [x] ~~文件/时钟 PAL 技术预览：普通文件打开/独占创建/读写/关闭及单调时钟；六项构建复验、文件效果检查及四类 OS 拒绝注入通过。~~ [候选与证据](migration/t10-io-progress.md)，固定登记已获用户本地实验批准，不声称路径沙箱或持久化已实现。

- [x] ~~内存 PAL 技术预览：匿名 RW 映射/释放、溢出/错误/所有权检查，两模式隔离运行、OS 拒绝注入及释放后失效验证通过。~~ [候选与证据](migration/t10-memory-progress.md)；扩展控制包已获用户固定包本地实验批准，不代表 T10 整体验收。

- [x] ~~本地候选退出路径：平台接口、通用 Bootstrap、静态组合及 Assembly 入口；两模式构建/复验/隔离执行、参数拒绝、替换提供者与三类门禁反例通过。~~ 仅此子项完成，完整 PAL 和生产准入未完成。

- [ ] 输入：T05～T08。实现入口、内存映射、文件/时间/退出的最少平台支持。
- [ ] 操作：所有 OS 差异限定在批准区域；建立系统句柄生命周期与错误转换。
- [ ] 产物：无业务依赖的宿主骨架、PAL 服务表和首个产物基线。
- [ ] 退出：符号和链接图无 Node/模型/UI；最小宿主退出无泄漏；G01/G02/G09 可检查该目标。

<a id="T11"></a>

### T11：Allocator、Buffer 和字符串

进行中：[有界 arena 与证据](migration/t11-arena-progress.md)。

- [x] ~~最小 Buffer 技术预览：有界字节、增长失败原子性、自引用追加与 checked view 生命周期，两后端十四项构建/复验/执行通过。~~ [候选与证据](migration/t11-buffer-progress.md)，固定 Buffer 包已获用户本地实验批准；字符串、guard page 与完整 G07 未完成。

- [x] ~~有界 region allocator 技术预览：容量/对齐/失败不推进、scope 重置/释放/代数、两提供者替换及 OS 拒绝注入通过。~~ 固定 arena 包已获用户本地实验批准；Buffer、字符串及完整 G07 未完成。

- [x] ~~最小 UTF-8 技术预览：严格标量解码、校验后 Buffer 追加、独立编码语料、三后端及 guard page 正对照，二十项构建/运行和四项预期故障通过。~~ [候选与证据](migration/t11-text-progress.md)，固定包已获用户本地实验批准，完整 G07/T11 未完成。

- [x] ~~本地资源与替换预览：独立固定槽 allocator 与 arena 链接相同消费者对象，1 MiB 文本／整页 Buffer／原提供者计数，24 项常规构建运行通过；UBSan 补充实验及违规对照通过。~~ [候选与证据](migration/t11-resource-progress.md)，固定资源包已获用户本地实验批准；正式插桩配置、预算与阶段整体批准未完成。

- [ ] 输入：T10。实现引导分配与可选择默认 allocator、显式长度 UTF-8/byte buffer 和容量限制。
- [ ] 操作：C/Assembly 交叉调用；故障注入 OOM、长度溢出、guard page、借用失效和原分配器释放。
- [ ] 产物：基础服务与边界语料，G07 原生资源探针。
- [ ] 退出：换分配器不修改消费者；零长度、NUL、大输入与非 ASCII 正确；尚不做无基准 SIMD 扩张。

<a id="T12"></a>

### T12：必需数据结构与 JSON

开工依据：[严格 JSON／Pi 参数修复／Schema 转换边界](migration/t12-protocol-boundaries.md)。已核对冻结参考源码；增量 token 局部实现与语法验证见下文，Provider/Schema 兼容未完成。

- [x] ~~有界增量 JSON token 本地预览：636 条双运行时语法共识、全部切分点／逐字节、1 MiB、配额与页尾边界，28 项构建运行及 UBSan 补充实验通过。~~ [候选与证据](migration/t12-json-progress.md)，固定包已获用户本地实验批准；Schema、Provider 修复及完整 T12 未完成。

- [x] ~~JSON reader 本地预览：无损 UTF-8 解码、失败原子性与 decoded-key last-wins 查询，458 条字符串／33 条查找双运行时语料、全 UTF-16 单元与 pair 边界、28 项构建运行及补充 UBSan 通过。~~ [候选与证据](migration/t12-reader-progress.md)，固定 reader 包已获用户本地实验批准；不代表完整 JSON/Schema 或 Pi 兼容验收。

- [x] ~~有界 UTF-8 字符串序列化本地预览：273 条 Python/Node 字节共识、reader 往返、容量与保护页、28 项构建运行及补充 UBSan 验证。~~ [候选与证据](migration/t12-quote-progress.md)，固定包按用户新规则已由 Agent 自审自批用于本地实验；不代表独立审核、完整序列化或 T12 验收。

- [x] ~~JSON 数值读取本地实验：1315 条 binary64 位级共识、精确舍入与负零/溢出、百万位抵消/配额、28 项构建运行、归档恢复及补充 UBSan 通过。~~ [候选与证据](migration/t12-number-progress.md)，固定包已由 Agent 自审自批；不含数值序列化、Schema、强制转换或完整 T12。

- [x] ~~有界结构化 JSON writer 本地实验：140 组字节共识、6416 种容量、1315 条数字词法往返、深度/页尾/1 MiB 边界、parser 首次完整清零检查、28 项构建运行、归档恢复与补充 UBSan 通过。~~ [候选与证据](migration/t12-writer-progress.md)，固定包已由 Agent 自审自批；不代表 binary64 格式化、Schema、Pi 适配或完整 T12。

- [x] ~~binary64 数值格式化本地实验：16500 条双模式字节对照、有限值往返/负零归一化、容量/页尾/组合、28 项构建与40次正常执行、归档恢复及补充 UBSan 通过。~~ [候选与证据](migration/t12-format-progress.md)，固定包已由 Agent 自审自批；不代表通用 JS 转换、Schema 或完整 T12。

- [x] ~~JSON UTF-16 字符串相等判断本地实验：2060 组双运行时对照、全 UTF-16 单元、pair 边界、1 MiB/页尾/借用、28 项构建与40次正常执行、恢复复验及补充 UBSan 通过。~~ [候选与证据](migration/t12-equal-progress.md)，固定包已由 Agent 自审自批；不代表 Schema、UTF-16 输出或完整 T12。

- [x] ~~有界严格 Schema 子集本地实验：可替换服务、640 条实际 TypeBox 对照与16条范围拒绝、32项构建/44次正常执行、配额/页尾/恢复和补充 UBSan 通过。~~ [候选与证据](migration/t12-schema-progress.md)，固定包已由 Agent 自审自批；不代表完整 Schema、Pi 参数修复或 T12 验收。

- [x] ~~Pi repairJson 词法修复本地实验：3364 条真实 Pi 字节对照、306573 种原生容量、1 MiB/页尾/失败原子性、36项构建及48次正常执行、恢复和补充 UBSan 通过。~~ [候选与证据](migration/t12-repair-progress.md)，不代表最终参数解析、Schema 转换或完整 T12。

- [x] ~~Pi 最终参数对象解析本地实验：3443条真实Pi接受/拒绝和语义对照、逐条nonfinal拒绝、严格优先/有界修复/空白及根对象、配额/页尾、36项构建与48次正常执行及补充UBSan通过。~~ [候选与证据](migration/t12-final-progress.md)，不代表Schema转换、Provider事件接线、工具执行或完整T12。

- [x] ~~Pi optional-null归一化本地实验：499条函数体未改动的访问插桩对照、required/nullable/布尔缓存边界、重复名称删除、逐容量/配额/页尾、36项构建和48次正常执行及补充UBSan通过。~~ [候选与证据](migration/t12-null-progress.md)，不代表类型转换、最终校验或完整T12。

- [x] ~~Number(string)基础本地实验：2987条Node位对照、9978种宿主容量、1760条Python共识，进制/规范化/MiB/页尾、36项构建及48次正常执行和补充复验通过。~~ [候选与证据](migration/t12-number-string-progress.md)。一次旧分片超时及原时限复验保留；另2250条Pi转换仅是参考，尚无完整coercion通过结论。

- [x] ~~基础标量规则本地实验：2562条三模式/五目标语义对照，typed fallback/安全BigInt/整数截断、原值保留、零workspace/别名/页尾，普通与UBSan各36构建和48次正常执行通过。~~ [候选与证据](migration/t12-scalar-progress.md)，未完成树遍历、约束/union或完整validateToolArguments。

- [x] ~~有界对象/数组转换本地实验：606条参考、1212次阶段检查（84次显式范围拒绝）、286条四工具完整参数链路，逐容量/重复字段/六个页尾、普通和UBSan各36项构建及48次正常执行通过。~~ [候选与证据](migration/t12-tree-progress.md)，未完成union、typed正则属性及整个公共函数根值语义，不代表完整T12。

- [x] ~~统一Object参数入口本地实验：286条四工具对照/非final工作区不变、1570条最终文本、总work/容量/失败不发布/九处页尾，普通及UBSan各36构建和48次正常执行通过。~~ [候选与证据](migration/t12-validation-progress.md)，不含Provider事件或执行授权，完整T12未验收。

- [ ] 输入：T11、Pi wire/session schema。只实现当前数据模型需要的结构。
- [ ] 操作：严格 JSON、增量帧解码、转义/Unicode、深度/长度限制、数值与缺失字段语义；有支持范围的 Schema 验证。
- [ ] 产物：JSON/Schema 服务、协议边界 fuzz/差分语料和准确的限制错误。
- [ ] 退出：分片输入与一次输入结果一致；拒绝不完整最终工具参数，不能因 salvage parse 成功就执行。

<a id="T13"></a>

### T13：插件发现、服务解析与作用域

- [x] ~~240组依赖图原生语料：95计划/145循环拒绝，含14无自环多节点循环与69间接Pending，完整公开入口、四profile及普通/UBSan矩阵通过。~~ [证据](migration/t13-corpus-progress.md)，首次构建超时与原时限复验保留；不代表完整T13。

- [x] ~~原生组合增量：多提供者正/逆清单顺序、必需/optional自依赖循环拒绝及失败不发布，普通/guard两模式及UBSan验证通过，规划器/ABI/控制面不变。~~ [证据](migration/t13-combinations-progress.md)，不代表完整循环/组合矩阵或T13。

- [x] ~~原生宿主规划器本地实验：单导出host.plan、独立消费者与普通/guard两组合，五阶段总配额/失败不发布/入口映射/三处页尾，普通及UBSan各40构建和52次正常执行通过。~~ [候选与证据](migration/t13-host-plan-progress.md)，完整配置/替换矩阵、原生循环/多提供者语料及生命周期尚未完成。

- [ ] 输入：T08、T10～T12。实现可静态读取的插件清单及静态提供者注册。
- [ ] 操作：单/多提供者规则、必需/可选依赖、循环/冲突诊断、应用与 Session 子作用域。
- [ ] 产物：可组合宿主、两种最小测试提供者、G05 配置矩阵。
- [ ] 退出：依赖满足才激活、跨作用域不能意外解析；不把作用域测试当 OS 隔离测试。

<a id="T14"></a>

### T14：资源所有权与生命周期

- [x] 七组协调器公开ABI迁移与轨迹守卫：最终8普通/UBSan构建/执行，56组次/1096事件、223隔离门禁自测通过。真实调用票据保留双方owner，原私有导出不恢复；两组持有及重复owner较早拒绝的适配已记录。源码恢复、8份保留构建核对和实验源码远端回读完成，仅此范围，见 [契约/证据](migration/t14-coordinator-trace-evidence.json)。

- [x] 五套公开ABI专项进入冻结原生矩阵：facade/entry/route/interop/parent在同一树下44普通/UBSan构建及44运行通过，原输出和284断言保持；72私有链接拒绝/8公开query链接通过，源码恢复及44保留构建核对、实验源码远端回读完成。只完成此范围，见 [证据](migration/t14-focused-native-evidence.json) 与 [19项盘点](migration/t14-focused-inventory.json)。

- [x] 当前登记矩阵的UBSan trap：60冻结构建/98目标执行，22真实SIGILL、四项信号监视器自测、430份C/C++源码审计及215隔离门禁自测通过；SDK1/SDK2原场景/ABI守卫计数保持。测试两路并行和套件拆分保留原180/30秒限制，超时/执行器失败原记录保留。源码归档恢复、保留产物核对及实验源码远端回读通过；仅此登记矩阵，不代替其余T14专项，见 [证据](migration/t14-ubsan-native-evidence.json)。

- [x] SDK1有效路径原生回归：56冻结构建/72目标执行，SDK1八次各824场景/1794租约消费、四次guard各122627调用；bind/enter及七SDK方法成功/拒绝覆盖，原全部计数保持。原汇总失败保留，独立最终校验完成；完整G03/T14仍未完成。

- [x] SDK2原生ABI守卫：48冻结构建/64隔离执行；四次guard各146729调用、5工厂/52目标，10真实破坏负例与2转发自测通过，原1196场景/3242方法调用及计数保持。固定SDK/实现不改；SDK1有效路径已由后续专项补齐，完整G03仍待验收。

- [x] 独立 C++ 测试准入前置：新增 v2 test-only C++20 配置，保留 v1；197 项门禁自测（含真实两模式 C++ admission/复核）及新镜像载荷一致性通过。完整 SDK2 矩阵待接入。
- [x] SDK2 测试观察函数改为按实例 config 回调注入：仅三个测试文件变化，O0/O2 原 1196 场景/3242 调用及完整输出不变，四个 C/C++ 对象不再导入观察函数；完整原生模块图与组合验证待接入。

- [x] SDK2 冻结静态补充：只读原场景在 Clang→GAS→ld 无 libc 链路下 O0/O2/Oz 各 1196 场景、3242 调用通过；静态 ELF/无未定义符号与断言失败负例通过。仅测试适配器，未登记原生矩阵，见 T14 草稿。
- [x] 明确 compiler.memory v1 构建/链接替换边界：GAS 与测试 C 提供者通过原 native_build debug/release 共四组、页边界/重叠/寄存器/DF、8 项依赖拒绝与四组产物复核；原 SDK2 冻结对象不变、两提供者 O0/Oz 四次完整场景通过。仅候选，详见 t14-compiler-memory-candidate.json。
- [x] SDK2原生模块/组合登记：compiler.memory、宿主、调用管理器、独立C/C++插件进入45模块/22组合（保留T13原20组合），44冻结构建、60隔离执行通过；新增T14实现和stdio/PAL仍为test范围，不构成生产准入。
- [ ] 完成T14整体验收：最新19项focused入口已有范围内证据；仍需早期语料发现完整性/私有API差异结论、完整G04与对应ABI/G07验收分母及统一受控组合。局部证据不能代替阶段验收，见 [当前审核](migration/t14-acceptance-review.json)。

- [x] SDK2本地端到端：版本化方法显式父参数，C/C++插件与双runtime完成真实调用；O0/O2各1196场景/3242方法调用，本地SDK2准入、8项失败关闭自测、18份回归及宿主UBSan通过。GAP-010仅部分解决，冻结原生/G04/G07待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部显式父子调用后端：旧ABI1不变、新扩展核对真实父/根票据，受管根作用域与父子引用；O0/O2各809场景/2248调用、4层调用、取消/完成/预算/别名与宿主UBSan通过。尚未接入版本化插件方法及SDK，GAP-010与原生准入仍未完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部插件SDK基础候选：独立固定消费头、不透明租约/完成、自动实例作用域与声明清理；C/C++插件O0/O2各824场景、1794次服务调用、预算/别名及补充宿主UBSan通过。GAP-010嵌套setup/cleanup准入实际BLOCKED，完整SDK与原生准入未完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [ ] 完成GAP-010剩余验收：固定模块/组合和冻结clang/GAS集成已通过；SDK1有效路径也已通过；最新focused入口迁移已完成；仍需早期发现完整性/私有API差异、对应ABI与完整G04/G07验收，之后才判断关闭；不能只按inflight计数放宽停止门闩。

- [x] 局部宿主服务跨语言消费：独立固定契约下C/汇编/C++消费两种C提供者及C++提供者，O0/O2各1188场景/34128调用，9组合各3792次；callee-saved、错误边界、补充宿主UBSan与源归档恢复通过。完整SDK与冻结原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主真实服务值路由：稳定名称/版本/尺寸、setup发布与双端租约，MANY逐边、app/session隔离和两种实现替换；O0/O2各390场景/888实际调用、16518不足fuel、4884别名拒绝及补充宿主UBSan通过。完整SDK、服务汇编/C++消费与冻结原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主静态 entry 桥：T13 installed/map 对应验证、owner/epoch/票据检查及一次性进入；O0/O2 各 4108 场景、24 次汇编入口、90346 不足fuel和6555别名拒绝通过，既有套件与补充宿主UBSan复验通过。真实服务值路由、插件SDK及冻结原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主 ABI 门面候选：17 方法/152 字节版本化表、单工厂导出、固定消费头，O0/O2 各 1152 场景/5772 登记拒绝、18 私有符号链接拒绝及宿主 UBSan 通过。插件 entry 桥、服务值路由和冻结原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主通用调用预算：两端预检/槽扫描/提交计费，失败不持有操作或消耗代际；O0/O2 各 98 组合、276 实际调用、1769 不足预算拒绝、138 满额拒绝通过，补充宿主 UBSan 通过。采用原子重试，正式 ABI 与原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主启动预算：校验/槽查找/setup 与失败回滚共享剩余 fuel；完成通知触发有预算重扫，修复扫描中途完成的等待缺口。O0/O2 各 6056 场景、103692 步、285 次调用前取消、114 次回调前拒绝通过；正式 ABI/原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主适配器事务：T13 列表/停用边/结果合并准备，源与输出别名/溢出拒绝，失败不部分发布；O0/O2 各 98 次准备启动停用、145 循环图拒绝、6432 别名拒绝、12127 不足预算通过，补充宿主 UBSan 通过。正式 ABI、启动预算及原生门禁待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主运行时几何：启动前绑定全生命周期元数据区间索引，票据/报告别名、对齐与溢出拒绝；O0/O2 各 98 场景、77611 次拒绝逐字节验证通过，补充宿主 UBSan 通过。T13 适配器写出边界、正式 ABI 和原生门禁待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主停用预算：owner/runtime 可暂停继续，保存节点/边游标，区分预算耗尽/等待/终态；O0/O2 各 3172 场景、47798 次有界推进通过，量子 1～4。回调执行时间、启动预算、完整几何及原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主调用准入：区分业务/setup/清理作用域，同时保留调用两端 owner；停止后业务拒绝，清理回调返回后仍等待在途调用。O0/O2 各 207 场景/1776 次测试提供者调用通过；完整服务 ABI、边界约束和原生准入待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主分支停用：按 T13 真实绑定保留提供者闭包，等待 setup/disposer/调用时仍清理无关分支；O0/O2 各 1010 场景通过，含 548 次独立推进和 83 次等待 setup 时独立退出。正式 ABI、调度预算和原生 G04 待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主异步 setup：defer/finish/pump、停止后迟到成功不激活、setup 完成后仍等待异步清理；O0/O2 各 1414 场景通过，原 675 生命周期与 8 组清理场景复验通过。仍需正式 ABI、原生准入和完整 G04，见 [T14 草稿](migration/t14-owner-boundaries.md)。
- [x] 异步 setup 核心公开 ABI 原生专项：初版覆盖旧 `setup-test.c` 的 1414 个异步 setup 场景，七模式各202例；随后通过合并候选补测其 675 个 `integration_main()` 场景。当前范围与 API 差异见 [合并证据](migration/t14-adapter-setup-native-evidence.json)；此行保留初版专项记录。
- [x] 旧 `adapter-test.c` / `setup-test.c` 当前公开 ABI 合并专项：675 个适配器集成场景与 1414 个异步 setup 场景，普通/UBSan × 汇编/C内存实现 × debug/release 共8次构建和执行，全部结果一致。86个有真实依赖边的调用等待通过公开租约覆盖；106个无边 owner-only 等待是已移除的私有语义，明确记录为差异，未重新导出 helper。F01/F16 按带 API 适配通过，完整 T14 仍待完成，见 [证据](migration/t14-adapter-setup-native-evidence.json)。
- [x] 旧 `async-test.c` 当前公开 ABI 清理专项：追加7组公开票据、重复/迟到拒绝、内联/同步失败、epoch重用、依赖调用排空和provider保活轨迹；同一8构建矩阵全部通过，并复验上方675个 adapter/1414个 setup 场景。F02 按带 API 适配通过；无边任意owner操作未伪称覆盖，完整T14/G04/G07仍待完成，见 [F02证据](migration/t14-async-disposer-native-evidence.json)。
- [x] 旧 `budget-test.c` 当前公开 ABI 预算专项：2,736个场景/44,904次有界推进，量子1–4、零预算逐字节不变、清理失败/异步等待/终态重停；普通/UBSan × 双内存实现 × debug/release共8次构建执行一致。436个无公开依赖call映射的私有owner hold明确跳过；F03按API适配通过，完整T14/G04/G07仍待完成，见 [F03证据](migration/t14-budget-native-evidence.json)。
- [x] 旧 `call-budget-test.c` 当前公开 ABI 调用预算专项：207个公开调用场景、1,191次fuel不足重试，验证配额拒绝、无效binding、输出/scope重叠拒绝以及同步setup/cleanup scope；普通/UBSan × 双内存实现 × debug/release共8次构建执行一致。公开依赖call一次占用consumer/provider两端，无法分别模拟旧私有owner单端配额耗尽，差异已记录；F04按API适配通过，完整T14/G04/G07仍待完成，见 [F04证据](migration/t14-call-budget-native-evidence.json)。
- [x] 旧 `call-test.c` 当前公开 ABI 生命周期专项：每个实际binding覆盖直接call、setup_call、cleanup_call三种模式，共207个场景；验证停止期间拒绝新调用、scope过期、异步清理期间保活consumer/provider及票据拒绝。普通/UBSan × 双内存实现 × debug/release共8次构建执行一致。旧私有helper单独耗尽provider槽位的quota断言没有公开单端等价物，差异明确保留；F05按API适配通过，完整T14/G04/G07仍待完成，见 [F05证据](migration/t14-call-lifecycle-native-evidence.json)。
- [x] 旧 `geometry-test.c` 当前公开 ABI 几何专项：98个计划、600轮protected-span矩阵、143,961次拒绝验证；普通/UBSan × 双内存实现 × debug/release共8次构建执行一致。standalone owner私有函数无公开等价入口；fuel不足时bind_geometry只保证runtime未提交，span scratch可能已有中间写入，差异与实现契约均记录，F08按API适配通过，完整T14/G04/G07仍待完成，见 [F08证据](migration/t14-geometry-native-evidence.json)。
- [x] 旧 `graph-test.c` 当前公开 ABI 图停止专项：98个边构建计划、516个停止场景；普通/UBSan × 双内存实现 × debug/release共8次构建执行一致。覆盖独立分支、祖先保活、异步setup/cleanup和错误传播；292个无公开依赖边的旧私有owner hold明确跳过，完整T14/G04/G07仍待完成，见 [F09证据](migration/t14-graph-native-evidence.json)。
- [x] 旧 `start-test.c` 当前公开 ABI 启动预算专项：6056个场景、103692次有界推进、285次setup前取消、114次回调前拒绝；量子1–4，普通/UBSan × 双内存实现 × debug/release共8次构建执行一致。保留同步/异步失败取消、槽退休、重入停止与graph/prefix断言，完整T14/G04/G07待完成，见 [F17证据](migration/t14-start-native-evidence.json)。
- [x] 旧 `transaction-test.c` 当前公开 ABI 准备事务专项：98次成功事务、145个失败计划、6432次别名拒绝、12127次不足预算拒绝；输入快照/输出毒值与尾部边界验证，普通/UBSan × 双内存实现 × debug/release共8次构建执行一致。最新快照入口迁移无剩余，但更早owner审计及完整T14/G04/G07未完成，见 [F19证据](migration/t14-transaction-native-evidence.json)。
- [x] 最早owner六组断言首轮审计及4组公开ABI补测：四资源LIFO、加载/清理双错误、setup取消迟到完成、跨epoch旧票据及初始化拒绝；8个普通/UBSan构建运行一致。旧任意单端hold差异和30份早期测试发现清单明确保留，更早语料审计仍未完成，见 [证据](migration/t14-earliest-owner-native-evidence.json)。
- [x] 早期owner固定发现清单30份不同内容逐份静态审计：全部路径摘要复核，18份与focused源码相同、11份历史变体完整差异及最早probe六组边界均记录；这不是新增运行证据，发现完整性与私有API差异仍待处理，见 [审计](migration/t14-earlier-owner-content-audit.json)。
- [x] 固定44份早期Python运行器/生成器审计：全部assert节点与源码摘要记录，唯一嵌入测试已在30份内容清单中；外部副本重跑8项SDK2本地回执检查通过。未重跑会改写源文件的旧生成器，尚不关闭完整发现/ABI/G04/G07，见 [审计](migration/t14-earlier-runner-audit.json)。
- [x] G04局部异步setup/cleanup轨迹：3组50事件，8个普通/UBSan构建运行逐事件一致；伪造/重复票据、取消迟到成功、加载/清理双错误及终态排空覆盖。局部参考检查器5组自测（含700个字段变异）通过；多owner/分支/预算完整矩阵仍未验收，见 [证据](migration/t14-g04-async-trace-evidence.json)。
- [x] G04固定分支图小预算轨迹：8场景/228事件/148次推进，量子1–4、公开依赖call双端保活、无关分支独立清理与清理错误保留；8个普通/UBSan构建运行一致，分支检查器8组自测通过。尚未覆盖planner组合及完整异步交错/故障矩阵，见 [证据](migration/t14-g04-branch-trace-evidence.json)。
- [x] G04分支检查器失败关闭加固：非推进事件拒绝预算报告、等待/终态须有驱动事件、end身份和终态快照核对；12组检查器测试通过，保存的8份原生输出1824事件重放通过（没有新增原生构建/执行），见 [证据](migration/t14-g04-branch-checker-hardening.json)。
- [x] G04 planner→公开prepare→分支停止组合：两子作用域四实例，真实解析2条依赖边及激活顺序映射；8个普通/UBSan构建运行通过，1824分支事件通过加固检查器，轨迹与固定图夹具逐字节一致。只覆盖固定scoped拓扑，Pending/MANY与完整异步矩阵仍待验证，见 [证据](migration/t14-g04-planner-branch-evidence.json)。
- [x] G04 Pending/MANY阶段组合：真实discovery/prepare，Pending不激活，MANY双票据及逐个释放后的provider闭包保活；量子1–4共8场景/24阶段快照，8个普通/UBSan构建运行通过，局部检查器7组自测通过。不是逐tick完整轨迹，异步分支交错/故障矩阵仍待完成，见 [证据](migration/t14-g04-pending-many-evidence.json)。
- [x] G04公开call与双分支异步清理完成交错：ABC/ACB/BAC三种合法顺序 × 量子1–4共12场景/72阶段快照，首个清理错误/提供者保活/一次性begin及重复完成拒绝；8个普通/UBSan构建运行、检查器7组自测通过。不是所有异步故障空间，见 [证据](migration/t14-g04-interleave-evidence.json)。
- [x] G04局部统一检查入口：四必需套件/374条记录及固定回归摘要完整核验，缺失/调序/额外日志失败关闭；6组自测通过，新包装器对原冻结二进制8次隔离重执行共2992记录通过。无新编译，尚不代表生产/远端必需G04，见 [证据](migration/t14-g04-bundle-evidence.json)。
- [x] T14测试产物资源观察：8产物×3重复共24次隔离执行，bundle8976记录通过；ELF/生命周期对象节大小与8项readelf交叉核对、直接/proc RSS/HWM采样。仅MEASURED_UNBASELINED，无预算批准或G07/G10通过，见 [观察](migration/t14-resource-observations.json)。
- [x] T14 guard存储压力：runtime/owner/资源池/operation池页首与页尾，256场景/2048哨兵检查，成功/双错误/取消与128 epoch重用、8映射释放；8普通/UBSan构建及最终8次controller-bundle隔离执行通过。新故障正对照与完整G07仍待完成，见 [证据](migration/t14-owner-guard-evidence.json)。
- [x] T14 guard新故障正对照：同一提供者，成功映射/可访问端点写入/就绪标记后读取左右保护页；普通/UBSan × debug/release × 两侧共8构建执行均为预期SIGSEGV139。补齐局部保护页生效证据，不关闭完整G07，见 [证据](migration/t14-guard-fault-evidence.json)。
- [x] 同步成功start ABI补测：每次6个受守卫调用（2个零返回），RBX/DF/栈哨兵3个真实破坏正对照；8普通/UBSan构建运行通过，修复既有SDK守卫没有同步start零返回覆盖的缺口，见 [证据](migration/t14-start-abi-evidence.json)。

- [x] 局部宿主异步清理：完成票据、等待期间 owner 可见、重复/迟到拒绝与消费者清理期间保留 provider；O0/O2 各 8 组补充场景及原 675 生命周期场景通过。完整异步事件循环及原生准入仍待完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主接入：T13 固定 240 图经真实 planner 输出进入 owner 协调器；O0/O2 各 675 场景验证 Pending、解析绑定顺序及逐实例失败/取消/在途等待。静态 entry ABI、异步与原生准入仍未完成，见 [T14 草稿](migration/t14-owner-boundaries.md)。

- [x] 局部宿主实验：同步 owner 与预验证列表协调器已验证 LIFO、setup 重入取消、部分失败逆序回滚和在途消费者保留提供者；见 [T14 草稿](migration/t14-owner-boundaries.md)。未接入 T13 计划、异步 disposer 或原生门禁，不代表阶段完成。

- [ ] 输入：T13 和 Harness vendored 修正记录。实现 Loading/Active/Quiescing/Disposed 及失败清理。
- [ ] 操作：注册 disposer、部分失败回滚、重入停止、异步 setup 后取消、消费者先于提供者清理、重复 dispose。
- [ ] 产物：G04 状态迁移与故障注入套件，生命周期事件轨迹。
- [ ] 退出：所有在途 owner 均可追踪；错误不饿死其他清理；停止后不能新注册逃逸资源。

<a id="T15"></a>

### T15：事件循环、异步操作和流控

- [ ] 输入：T14。实现选定事件后端、操作完成与取消、定时器和受限队列。
- [ ] 操作：明确线程、重入、通知/变换/门控；处理慢消费者、取消竞争和消息背压。
- [ ] 产物：可替换 event backend、确定性调度测试及队列/分配预算。
- [ ] 退出：exactly-once 终结与有限内存得到验证；不能阻塞整个循环等待 UI 或外部网络。

<a id="T16"></a>

### T16：原生动态模块装载

- [ ] 输入：T13～T15。实现版本查询、函数表验证、平台/CPU 检查与可信库装载。
- [ ] 操作：静态/动态提供者跑同一生命周期套件，注册与模块映射生命周期分离。
- [ ] 产物：Native 插件 SDK 最小示例；失败模块诊断。
- [ ] 退出：损坏描述符/ABI 不匹配拒绝；首版停止实例但保留代码映射，不承诺随意 dlclose 或热替换 allocator。

<a id="T17"></a>

### T17：配置与 Profile 组合

- [ ] 输入：T03、T13～T16。定义盘古自有配置与 Pi 映射，不要求执行 Cordis 的 JS/YAML 表达式。
- [ ] 操作：配置 Schema、默认/用户/项目/环境/CLI 优先级、信任时序、离线模式、未知字段策略、最小 Profile。
- [ ] 产物：配置服务与组合 dump、来源诊断、信任前读取项测试。
- [ ] 退出：未信任项目不会加载可执行插件；Pi 的 sessionDir 等前置信息处理有对应决定，不能泛化成“不读任何项目文件”。

<a id="T18"></a>

### T18：完成 P0 与首次资源预算

- [ ] 输入：T10～T17。独立构建宿主及测试组合，运行 G01～G05/G07/G09/G10 的适用部分。
- [ ] 操作：冻结可测目标的首个基线，验证退出/失败/替换；把待实现性能项目保留 UNBASELINED。
- [ ] 产物：P0 证据包、依赖图、size/RSS/启动与分配计数。
- [ ] 退出：控制规则有显式审查批准记录（用户已授权 Agent 自审自批）；没有 Agent 能力也不能拿 P0 宣称 Pi 重写完成。

<a id="T19"></a>

### T19：Session 树和消息投影

- [ ] 输入：Pi session-manager/messages/types 与 T08、T12～T18。
- [ ] 操作：先内存 Session 树、活动叶、system/tool 声明、消息角色、model/thinking 变更与上下文投影；Session 提供者独立于存储。
- [ ] 产物：纯数据回放/分支对照用例，未知 entry 的保留/拒绝规则。
- [ ] 退出：相同历史构造相同模型上下文；不能直接套用 Harness 的线性日志替代 Pi 树语义。

<a id="T20"></a>

### T20：JSONL 持久化与崩溃恢复

- [ ] 输入：T19、Pi 格式 v1/v2/v3 清单。先写盘古自有版本化数据，不覆盖 Pi 源文件。
- [ ] 操作：追加/刷盘策略、单写者、文件锁、截断尾、损坏中间记录、事务/落盘失败；支持范围内版本迁移。
- [ ] 产物：File Storage 插件、重启恢复与故障注入测试、Session 格式说明。
- [ ] 退出：错误可定位，未知新版本拒绝；恢复不假装重做已发生工具副作用；新旧格式迁移另由 T38 负责。

<a id="T21"></a>

### T21：Prompt、资源与 Context 基础

- [ ] 输入：Pi system-prompt/resource-loader/context 文件语义、T17/T19。
- [ ] 操作：base/system/append、AGENTS 层级与覆盖、工具声明、技能摘要、token budget 接口；先不用复杂 Prompt Compiler 算法。
- [ ] 产物：可替换 Prompt/Context 服务及请求快照用例。
- [ ] 退出：模型实际可见输入能从受控 Session 和资源快照重构；工具可执行集合与声明一致。

<a id="T22"></a>

### T22：Model 服务与假 Provider

- [ ] 输入：Pi ai/types、agent/types 与 T15/T19/T21。
- [ ] 操作：text/thinking/tool blocks、usage、stop reason、流开始/完成/错误、上下文能力；先实现确定性假 Provider。
- [ ] 产物：模型接口、脚本流回放插件、Provider 能力验证。
- [ ] 退出：无需凭据可复现每个成功/失败路径；模型服务不 import 特定厂商。

<a id="T23"></a>

### T23：Agent Loop 原生服务

- [ ] 输入：Pi agent-loop/agent、T19～T22。Loop 作为服务消费者和插件，不写入 bootstrap。
- [ ] 操作：输入→请求→流→工具→结果→续接、取消、状态投影、请求前钩子；保留 Pi turn/run/settled 层次。
- [ ] 产物：第一批 G06 对照轨迹；可替换 Loop 测试提供者。
- [ ] 退出：无工具/工具续接/错误/取消都有最终状态；低级 agent_end 不误当整个工作流结束。

<a id="T24"></a>

### T24：Tools 服务和受控执行管线

- [ ] 输入：Pi tools/extensions wrapper、Harness tools 服务与 T23。
- [ ] 操作：注册、校验、参数准备、门控、超时、结果、进度、串行/并行与 terminate；动态工具暴露和权限分开。
- [ ] 产物：Tools 服务、假工具及 schema/截断参数/缺工具/拒绝/取消用例。
- [ ] 退出：每个被接受调用都有明确结果；授权拒绝不能触发工具；并发结果关联 ID 正确。

<a id="T25"></a>

### T25：文件服务与 read/write/edit

- [ ] 输入：Pi read/write/edit、edit-diff、file-mutation-queue/truncate/path-utils 与 T24。
- [ ] 操作：路径/cwd、UTF-8/BOM/CRLF、行/字节截断；edit 多修改基于原文件、唯一匹配/不重叠、错误不半写；按受控资源服务执行。
- [ ] 产物：文件插件和工具，对照文件内容/结果/diff 的隔离 fixtures。
- [ ] 退出：读写权限失败、符号链接和并发写场景有界；差异依接口登记，不偷偷把 edit 换成另一种 patch 协议。

<a id="T26"></a>

### T26：进程服务与 bash

- [ ] 输入：Pi bash/shell/child-process、T15/T24/T25，G08 初版必须就绪。
- [ ] 操作：cwd/env、stdout/stderr、退出码、输出截断/落盘、timeout、进程组取消；默认非持久 Shell，不能误抄 Harness 持久 PTY 行为。
- [ ] 产物：Process/Shell 服务、bash 工具、拒绝执行/孙进程/信号/大输出用例。
- [ ] 退出：超时/取消能正确收尾；会话环境变量映射明确；Windows PowerShell 后续单独实现。

<a id="T27"></a>

### T27：HTTP/TLS 和首个真实 Provider

- [ ] 输入：T07 批准的网络库、T22 和首个工作流确定的协议。
- [ ] 操作：原生 HTTP/SSE、代理、证书/主机名校验、超时、取消、429/5xx；选一个明确协议和版本，不把“OpenAI-compatible”当所有端点完全一致。
- [ ] 产物：网络插件、Provider、受控本地服务测试及凭据脱敏记录。
- [ ] 退出：本地可重复套件先通过，再使用明确凭据做受控真实端点测试；不能以一个成功请求证明整个 Provider 兼容。

<a id="T28"></a>

### T28：最小 TUI 与 P1 首次体验验收

UI/UX 遵循 [TUI 体验参考与设计原则](TUI体验参考与设计原则.md)：Grok Build 为主参考，结合 Pi、Codex、Claude Code；只学体验，沿用盘古原生栈。

- [ ] 输入：T17、T20～T27，以及 Pi 终端输入/恢复、流式展示的最小相关实现。TUI 是首个正式交互入口，不等待 T33/T34 完整交互能力。
- [ ] 操作：实现可输入并提交提示词、追加显示模型流文本、显示工具执行状态/结果、取消当前请求、退出恢复终端的最薄 TUI；支持基本中文输入/粘贴与 resize。UI 只消费 Runtime 接口，不复制 Agent Loop 或 Session 状态。
- [ ] 保留最小启动入口：help/version、模型和配置/Session 路径；确定性 headless 驱动供自动化测试。print/stdin 可按实际自动化需求实现，不先复制 Pi 全套 flags、命令或 CLI 交互产品。
- [ ] 产物：可上手的 P1 TUI、必要终端 PAL、PTY 回放/恢复用例、用法与不支持项；无网络和本地 HTTP 的完整“读→改→构建/测试→结果”证据。
- [ ] 退出：G01～G10 所有适用检查通过；TUI 可完成对话→工具→续答，取消/错误/退出恢复终端且最终文本不丢失；测量完整 P1 依赖与成本。仅宣称薄 TUI 和列明的自动化子集可用，不宣称完整 Pi CLI/TUI 兼容。

<a id="T29"></a>

### T29：队列、重试与终结语义

- [ ] 输入：Pi AgentSession 与 T23/T28。
- [ ] 操作：steering/follow-up 的注入边界、模式、clear_queue、abort、自动重试、agent_settled、运行结束后输入；禁止工具副作用盲重试。
- [ ] 产物：时序 fixtures 与控制钩子；模拟输入恰好到达边界。
- [ ] 退出：队列不丢不重、取消与恢复确定；对照多轮结果而非只比最终一段文本。

<a id="T30"></a>

### T30：Compaction、恢复和树操作

- [ ] 输入：Pi compaction/session runtime/branch summary、T19/T20/T29。
- [ ] 操作：手动/阈值/overflow 压缩、保留边界、summary usage、branch/fork/clone/tree/context_edit、模型切换；原始历史保留。
- [ ] 产物：Session 高级功能、压缩假模型和崩溃恢复语料。
- [ ] 退出：原始历史、活动上下文和 accounting 三者不混淆；操作中断无部分状态冒充成功。

<a id="T31"></a>

### T31：Skills、模板、资源发现

- [ ] 输入：Pi skills/prompts/packages/resource-loader、T17/T21。
- [ ] 操作：资源路径/优先级、frontmatter、按需技能正文、模板参数、命名冲突/忽略规则、重载、项目迁移后 cwd。
- [ ] 产物：资源服务和纯文件 fixtures；npm 包安装尚未完成则报告不支持。
- [ ] 退出：只看摘要不执行技能；不可信可执行资源在信任决策前不能激活；用户/项目覆盖可追踪。

<a id="T32"></a>

### T32：原生扩展 API 与钩子

- [ ] 输入：Pi 扩展清单、Harness/Cordis 服务生命周期、T13～T16/T24/T29。
- [ ] 操作：原生工具/命令/Provider/钩子/renderer 注册、owner 清理、reload 与旧 ctx 失效；设计 UI 服务分离。
- [ ] 产物：原生扩展示例及 Pi→原生 API 对照，不承诺 jiti 模块可直接装载。
- [ ] 退出：通知/变换/阻断顺序和返回值可测试；不加载 TUI 的组合仍可运行核心扩展。

<a id="T33"></a>

### T33：完善 TUI 输入与终端能力

- [ ] 输入：T28 已有薄 TUI/终端 PAL 与 T29～T32，参考 Pi tui/keybindings/terminal/setup 及上述四家 UI/UX 清单；复用已有实现，不另起一套客户端。
- [ ] 操作：raw mode、Unicode 宽度、光标/resize、bracketed paste、ESC/组合键、信号恢复、pipe/TTY 检测与可配置快捷键。
- [ ] 产物：终端 PAL、编辑器输入、受控 PTY 回放与终端状态恢复测试。
- [ ] 退出：退出/崩溃处理不遗留终端模式；实现不依赖完整 GUI/Node，CJK/emoji 与多行输入有验收用例。

<a id="T34"></a>

### T34：完善 TUI 展示与日常交互

- [ ] 输入：T33、四家 UI/UX 参考清单、Pi renderer/themes/slash commands 与资源清单；Grok 为主，简单易上手优先，不复制功能并集。
- [ ] 操作：流式文本/Markdown、工具状态/diff、history、模型/Session 选择、命令、主题和已选扩展 UI；按语义验收视觉布局。
- [ ] 产物：P2 TUI 用例、截图/PTY 轨迹和慢渲染资源数据。
- [ ] 退出：UI 不拥有 Agent 业务状态；最终文本不丢失；像素不同不自动 FAIL，语义缺失不能被截图相似掩盖。

<a id="T35"></a>

### T35：模型、认证与图像范围补齐

- [ ] 输入：T02 必需矩阵、Pi providers/models/auth/images/virtual-models。
- [ ] 操作：按 ID 增加协议和凭据类型、模型目录/metadata、reasoning、图像尺寸/预算/格式；OAuth 只在对应工作流必需时纳入，但未实现就阻断该工作流。
- [ ] 产物：Provider/auth capability matrix、每路独立 fixtures、P2 完整证据。
- [ ] 退出：凭据不进日志/普通配置；测试按 provider×protocol×auth×model 能力分类，不能拿厂商数量作覆盖率。

<a id="T36"></a>

### T36：JSON/RPC 的 Pi 兼容适配

- [ ] 输入：Pi json/rpc/rpc-types 与 T29～T35；pangu 原生 API 保持独立版本。
- [ ] 操作：兼容层支持选定 flags/命令、ID、状态/错误、delta-only wire、header 差异、LF/CRLF、U+2028/U+2029、stdout 专用与背压。
- [ ] 产物：P3 协议适配器及逐命令/事件子项测试。
- [ ] 退出：现有解析器不改即可通过承诺范围；prompt 接受响应不等于运行完成；错误命令和并发响应均对照。

<a id="T37"></a>

### T37：配置与凭据迁移工具

- [ ] 输入：T03/T17/T35、试用者配置副本。
- [ ] 操作：只读盘点、dry-run、键映射/默认差异/未支持报告、目标独立目录、显式选择凭据来源。不得自动执行旧 auth 文件中的命令或扩展代码。
- [ ] 产物：migration report、独立盘古配置、凭据引用和失败恢复说明。
- [ ] 退出：未知关键配置不静默忽略；旧目录无修改；凭据不进入证据包，恢复旧版本无需转换回 auth 数据。

<a id="T38"></a>

### T38：Session 导入、导出与一致性

- [ ] 输入：T19/T20/T30、Pi v1/v2/v3 测试文件和真实脱敏副本。
- [ ] 操作：从备份导入到独立命名空间，保留 parentId/分支/自定义项/压缩/模型与usage；校验 orphan/未知类型与版本。
- [ ] 产物：原文件哈希、导入映射、拒绝/保留报告、可选明确范围的 Pi 导出器。
- [ ] 退出：至少模型上下文、会话树和统计三类对照；默认单向导入，不假设可反向转换。导出失败时仅回滚到已验证检查点，保留新会话供人工恢复。

<a id="T39"></a>

### T39：SDK 和现有扩展迁移路线

- [ ] 输入：T02 的实际 TS SDK/扩展使用清单与 T32/T36。
- [ ] 操作：优先让调用方通过 RPC 使用原生进程；选择“重写原生扩展”或“可选 JS sidecar”。逐方法/扩展声明支持，UI renderer、闭包/对象身份、同步 API 不能自动远程化。
- [ ] 产物：调用方改造指南、API crosswalk、桥接候选方案与额外体积/安全预算。
- [ ] 退出：不会宣称 C ABI 等于 TS SDK drop-in；必需扩展无法迁移则阻断其 P3，sidecar 未完成不能当功能已支持。

<a id="T40"></a>

### T40：包、安装、卸载与支持边界

- [ ] 输入：Pi install/remove/update/list、P3 范围和原生交付方案。
- [ ] 操作：原生插件来源/完整性校验、版本锁、离线安装、失败恢复、精确卸载；npm/git 资源兼容若延期须明确阻断受影响工作流。
- [ ] 产物：可安装包、SBOM/许可证、命令/脚本兼容说明和 P3 候选证据。
- [ ] 退出：安装失败不破坏旧包；路径命名 pangu-native 与 pi-ts 并存，不能抢占用户 pi 命令；更新必须可恢复旧产物。

<a id="T41"></a>

### T41：离线双运行认证

- [ ] 输入：T09 runner、P3 候选、当前范围与 gap ledger。
- [ ] 操作：完整必需套件在固定环境重跑，两边使用相同假模型输入和独立工具沙箱；产物/源码/策略摘要绑定，审计归一化规则。
- [ ] 产物：完整 conformance bundle、逐项 verdict、未覆盖和已批准差异。
- [ ] 退出：必需项全部有效 PASS 或可接受的已批准差异；关键风险不豁免；缺 case、oracle 构建失败或证据缺失为 BLOCKED。

<a id="T42"></a>

### T42：受控双轨试用

- [ ] 输入：T41 和独立 Pi/盘古 home、工作树及明确测试任务。
- [ ] 操作：先观察/只读 shadow，再在工作区副本执行相同任务。参考模型响应可回放时才做严格对照；两次真实模型响应不同不直接判兼容失败。
- [ ] 产物：按 feature ID 分类的问题、资源指标、试用矩阵。
- [ ] 退出：绝不让两套工具同时修改同一真实项目或发送相同外部副作用；不把同一付费调用不受控地双发。修复后重新认证受影响项。

<a id="T43"></a>

### T43：发布前门禁与回滚演练

- [ ] 输入：最终包、T41/T42、G00～G11 和批准预算。
- [ ] 操作：冻结发布物、签发绑定摘要的证据；检查依赖/安全/行为/性能；在真实安装布局演练切回 Pi、恢复配置与会话检查点。
- [ ] 产物：go/no-go 表、签核、旧版本保留位置、备份校验和回滚记录。
- [ ] 退出：必需检查无 FAIL/BLOCKED/UNBASELINED；回滚演练成功；候选包变化全部重新核验，不能拿 debug 结果认证另一 release 包。

<a id="T44"></a>

### T44：小范围 Canary

- [ ] 输入：T43、批准的用户/工作流/平台/观察窗口。
- [ ] 操作：使用明确的 pangu-native 入口试用，记录成功任务、错误、取消、恢复、资源、协议和功能缺口；灰度单位是会话/工作流，不在同一会话中随机混用两个引擎。
- [ ] 产物：脱敏 canary 记录、按观察窗口统计的判定。
- [ ] 退出：达到事先批准的样本量与窗口；严重问题立即停止扩大。当前不编造已批准天数或成功率阈值。

<a id="T45"></a>

### T45：切换选定调用入口

- [ ] 输入：T44 通过与维护者对具体范围的上线批准。
- [ ] 操作：仅切换已认证的 wrapper/脚本/客户端配置，保留 pi-ts 与旧 home；新任务用原生，新旧在途任务各自结束，转移会话必须先建立检查点。
- [ ] 产物：切换记录、回执、用户可见差异和恢复入口。
- [ ] 退出：入口解析/版本/数据目录已核对；未迁移 SDK、Provider 或扩展的工作流继续使用旧实现，不能全局覆盖 PATH 掩盖缺口。

<a id="T46"></a>

### T46：回滚或稳定退出

- [ ] 输入：T45 后监控与第 9 节触发条件。
- [ ] 操作：有问题时停止新原生任务、结算/取消在途、保存新数据，恢复旧入口和独立旧数据；无问题时在批准窗口结束后签稳定声明。
- [ ] 产物：回滚/稳定判定、未能自动恢复的数据和后续问题记录。
- [ ] 退出：旧系统用新的烟测验证可用，外部副作用人工核对；不删除新版本证据，不因回滚掩盖故障。未满足保留策略前不移除旧系统。

<a id="T47"></a>

### T47：扩展执行位置与平台

- [ ] 输入：P3 稳定、真实需求、独立预算。按需求建立后续子路线。
- [ ] 操作：MCP、Search/LSP/Git、Subagents、daemon/IPC、Process 沙箱、Remote，及 Windows/ARM64/Android/GUI/Web/VS Code。每个方向先定义 seam 和认证矩阵。
- [ ] 产物：独立 Profile/Bundle 与平台产物；Process/Remote 采用版本化协议，不传裸指针。
- [ ] 退出：该组合自己的 G00～G11 通过；不反向拉高最小 Agent 的必需依赖，平台 CPU 回退与调用约定分别测试。

<a id="T48"></a>

### T48：参考更新与技术债复盘

- [ ] 输入：稳定里程碑、功能账本、预算变化和上游新版本。
- [ ] 操作：审查新增能力、依赖和未使用层；删除无消费者的非必需抽象。更新参考先生成 surface diff，再批准新基线。
- [ ] 产物：版本化范围、新任务和例外到期处理，不直接重置所有基线。
- [ ] 退出：未关闭差异仍可追踪；过期例外阻断相应组合；新上游特性不能被静默算入既有兼容声明。

## 6. 双运行与渐进替换设计

### 6.1 默认用进程边界

```text
同一 case manifest / 固定模型脚本 / 相同资源快照
      ├── pi-ts（固定 TS 参考，独立 home/workspace）
      └── pangu-native（候选原生，独立 home/workspace）
                  ↓
          原始轨迹 + 文件差异 + 退出状态
                  ↓
          版本化比较器 → 差异 → 证据包
```

参考依赖和 Node.js 只进入对照工具环境，不计作原生运行时依赖，也不能隐藏在原生发布包里。参考安装使用独立工作副本、固定 lockfile 和受控安装步骤，不在 `references/` 内安装并改写文件。准备参考运行环境的权限、密钥和生命周期脚本要独立处理。

默认不把原生代码作为 Node addon 嵌入 Pi。若为了某个纯函数试验采用可选 FFI 对照工具，必须独立登记 ABI、所有权和测试依赖；不得让这种实验成为产品主架构。

### 6.2 四级验证

1. **纯函数差分**：JSON、编辑、路径、Session 投影、schema。完全确定，可比较精确输出。
2. **脚本化运行差分**：回放模型块、工具结果、时钟和输入时序，比较事件/最终状态。
3. **本地协议集成**：受控 HTTP/SSE、子进程、PTY、RPC，验证真实 I/O、背压和取消。
4. **真实端点与试用**：按工作流验证质量、认证与运维，不用随机模型文本作字节级 oracle。

### 6.3 不要双写

读写工具各自在内容相同的临时目录运行；两个模型运行不共享进程、端口、锁或会话文件。涉及外部 API、Git push、安装和付款等副作用时，使用 stub/记录回放或显式独立测试环境。shadow 不执行真实写操作。新旧运行的请求 ID 可映射，工具 ID 关联与因果关系不能归一化掉。

## 7. 差异测试与证据如何执行

以下是未来 runner 的操作流程，尚无可复制执行的产品命令。当前仅有 G00 检查；T09 实现后须补充并验证真实 CLI，不能让伪命令进入验收脚本。

1. 校验 reference/candidate 摘要、Profile 和 case 列表；拒绝脏输入或未说明的本地 patch。
2. 建立隔离 home、workspace、env、假时钟和模型响应；记录初始文件哈希、终端尺寸与 locale。
3. 运行两边并持续消费 stdout/stderr，施加超时与资源上限；保留原始字节流。
4. 按明确协议解析，再用版本化规则归一化。只允许固定字段映射：临时根目录、固定 ID、时钟和已声明无语义差异；原始记录必须保留。
5. 依用例比较精确字段、允许的偏序、最终上下文、文件副作用、退出码、授权和资源生命周期。
6. 生成 diff 与逐项判定；验证证据完整性、摘要和必需项覆盖后才能汇总。
7. 审查差异、gap 与例外（允许 Agent 自审自批，须标注身份与证据）；签发的是某组合/某版本结论，不是仓库永久兼容证书。

不能删除错误消息、事件、usage、tool result 或整个字段来“归一化通过”。对并行工具仅允许声明过的独立事件交换次序，仍需满足每次调用 start→update→end 与结果消费因果。未知差异默认 FAIL。

最小场景集：无工具；四工具成功/失败；截断工具参数；多工具串行/并行；steering/follow-up；取消各阶段；超时与重试；慢 stdout；LF/CRLF/Unicode 分隔符；UTF-8/BOM/换行编辑；配置优先级与项目不信任；会话树/压缩/导入；未知版本；错误凭据/证书/代理；插件加载失败/重入清理/依赖消失；资源上限。

复测规则：失败修复后重跑受影响集及共有核心回归；修改归一化器/接口/参考基线须扩大到完整必需集。环境错误是 BLOCKED；同一个失败反复运行到偶然 PASS 不能放行。

## 8. 上线 Go/No-Go 表

| 维度 | GO 所需证据 | NO-GO |
|---|---|---|
| 范围 | 必需功能子项已登记且证据有效 | 缺配置/CLI/Session/扩展项，或用覆盖百分比代替必需项 |
| 架构 | G01～G05 的适用检查通过 | 私有依赖、替换失败、生命周期缺口 |
| 行为 | G06，指定 CLI/JSON/RPC/工具场景一致 | 关键事件/结果/错误不一致 |
| 安全/内存 | G07/G08，无关键越权、泄漏或破坏 | 任意已知严重内存/权限/数据问题 |
| 依赖/性能 | G09/G10，最终包及批准预算 | 禁止依赖、超预算或必需指标 UNBASELINED |
| 数据 | 导入校验、备份可读、回滚演练 | 新旧混写、原文件被覆盖、无法恢复检查点 |
| 交付 | G11、最终源码/策略/工具链/包摘要绑定 | 缺必需检查、旧回执或包不一致 |
| 试用 | 预先批准的样本与窗口满足 | 只试一个 happy path，或缺必要使用方验收 |

维护者批准具体上线是最后一步。此计划的编写不授权自动发布、替换系统 `pi`、迁移真实密钥或删除旧数据。本文与门禁中的角色表示职责，不假定已有账号、CI 或远端保护部署。

## 9. 回滚策略与演练脚本要求

### 9.1 触发条件

越权或凭据泄漏、会话/文件损坏、重复工具副作用、协议破坏、频繁崩溃/卡死、经复测确认的资源超限、关键工作流失败，均应停止扩大；严重项立即停止新任务。普通缺功能按范围判定回退受影响工作流，不强制破坏其他稳定会话。

### 9.2 切换前准备

保留原 Pi 的确切版本/依赖、独立 home、加密或受限访问的备份、已验证的 Session 检查点和 CLI/RPC 烟测。记录新旧入口绝对位置、wrapper 内容和恢复动作。备份可读性和恢复必须演练，只有存在压缩包不算可回滚。

### 9.3 回滚步骤

1. 停止给原生实例分配新任务，记录受影响 Session 与工具调用。
2. 请求在途操作结算或取消；超过批准期限后按策略终止，记录状态未知的外部副作用。
3. 保存候选二进制、日志、证据和盘古数据快照，不删除失败现场；凭据隔离保存。
4. 恢复旧 wrapper/客户端入口和原 Pi 配置目录，核对版本及二进制摘要。
5. 从旧系统最后可读检查点恢复。盘古新会话只有通过已验证的导出器才能回输；不能直接让 Pi 打开盘古私有格式。
6. 运行配置、print、RPC、会话恢复及只读工具烟测；核对真实工作树和工具副作用，避免再次执行已成功操作。
7. 报告已恢复任务、保留但未导回的数据、状态未知操作和修复后重试条件。

程序回滚不能撤销写文件、Git 提交、外部请求等已发生动作。工作树备份/补丁恢复也必须先区分用户后续修改，不能使用全局 hard reset 清理现场。

### 9.4 数据前向变化

盘古状态格式升级采用新路径或明确的版本化副本，迁移源保留。若不可逆变化不可避免，则上线前明确可回退的最后检查点和新数据保留方式；不许承诺无损降级。灰度阶段默认不共享可写 Session 目录，避免制造必须逆迁移的数据链。

## 10. 完成定义与当前未决项

每个任务关闭须记录：实现提交/内容摘要、feature/case ID、具体证据、Gxx 结果、已批准差异、预算影响和下一步。README、TODO 勾选或 Agent 的“完成”声明不能取代证据。

当前必须在实施前解决：

- T02：首个实际工作流与完整兼容范围；本计划只是范围草案。
- T05/T06：版本管理/受保护执行器、汇编方言、编译/链接工具链。
- T07：TLS/HTTP/Schema/终端等原生依赖最终选型与供应链复核。
- T08：服务 ABI 与异步模型的具体签名。
- T09：参考 Pi 运行环境、可执行差分 runner 与格式验证器。
- T18/T28/T43：实测预算、试用窗口及发布阈值。

这些未决项有明确所属步骤，不意味着先跳过它们写业务代码。当前已完成 T01～T04 的参考冻结、范围、静态盘点和差异追踪；Pi/Rust 运行测试、原生产物、兼容认证和上线均未执行。

## 2026-10-20 主审收尾的阶段解释

T05/T06 的远端部署与独立批准保持未完成；T07 后续 HTTP/TLS 等选型不阻塞无第三方库的 T10。T08 的 DSO/取消竞争验证在对应运行能力引入前实施。T09 原生差分整体验收必须等原生实现存在，不能循环要求 T10 之前已有 Agent。

T10 可执行候选包见 migration/t10-admission-package.md 及 t10-native-candidate.json。经独立批准固定控制器/镜像/候选登记后，可在外部候选源码树开展本地隔离核心实验；根目录仍保持 design-only。合并/发布之前必须完成远端可信身份及状态发布，不把该里程碑写成已经通过。该说明不授予编写控制修改的 Agent 自批权。

## 2026-10-20 交互入口调整

按用户明确偏好，TUI 为主要产品入口，T28 提前交付最薄可用体验；T33/T34 完善日常体验。CLI 仅保留启动与必要自动化接口，不建设全功能 CLI 产品。T36 等 Pi 协议适配按所选迁移工作流推进，不成为 P1 体验前置。本次修改是计划范围调整，不是兼容验证通过；旧静态清单、审批快照和历史证据保留原范围，不回写为当前通过。固定控制包及 T10 本地实验批准不因本文更新而自动扩大。
