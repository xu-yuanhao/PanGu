# 给核心 Agent：完成 Pi Durable 原生移植

你是 PanGu 的核心实现 Agent。工作目录 `/home/xu-yuanhao/Documents/PanGu`。请按本文从上到下移植、构建、验证并更新进度。实现语言按**手写 Assembly 第一、C 第二、C++ 第三**。对应 TUI Agent 使用 [TUI TODO](TUI移植TODO.md)。

## 完成目标

完成 Linux x86-64 上可实际使用的原生 Pi Durable TUI：Durable 的会话/事务/文档、持久任务/调度、输入队列、模型生成、工具、扩展、分叉、压缩和观察能力都能通过原生服务工作；Pi 原 Coding Agent 提供项目资源与编码工作流；deepseek-harness/Cordis 的组合与生命周期语义用于原生插件宿主。

下面每一步都完成、覆盖表全部有实现与验证、TUI 最终工作流通过，才勾选 K15。每项实现从已有代码继续，复用已验证的部分。

## 并行职责

你维护 `src/runtime/`、`src/contracts/`、`src/composition/`、原生基础/宿主模块、`tests/runtime/`、`tools/runtime/`、`tools/migration/`、`tools/gates/`、`governance/`，以及 `src/client/tui/runtime_bridge*` 和 `tui_runtime_adapter.h`。你更新本文与当前实现记录。

TUI Agent 维护其余 `src/client/tui/`、`tests/client/tui/`、`tools/client/tui/` 和 TUI TODO。每个集成批次先查看 TUI TODO 的接口/登记请求，处理到自己归属的头文件与清单中，再在本交接区记录已经提供的入口。公共接口变更保持现有消费者可构建，TUI 按已到位的接口接入。 并行批次内固定既有函数表字段、枚举值和尺寸，新增能力通过独立公开接口提供；既有布局的迁移在双方交接就绪后由核心统一集成。

每次只暂存核对过的具体路径。TUI 候选交接后由你集中做组合、真实模型和终端验证。

## 每一步怎么做

1. 阅读下面对应的固定上游源码、直接调用与测试，确定行为和错误/取消路径。
2. 在本步骤“原生落点”中维护唯一实现，先明确 ABI、状态、内存所有者和借用有效期。`Promise/AbortSignal` 对应可恢复操作、有限 poll、取消和清理；字符串使用 UTF-8 slice；动态对象按明确字段/tag/函数表表达。
3. 将上游行为用例带到原生边界，验证数据、事件顺序和实际副作用。JSON/TLS/Unicode/SQLite/JS 引擎等外部能力复用成熟库，依赖选择随该步骤登记。
4. 跑本步骤专项与需要的集成检查，记录结果，完成后勾选。一次推进一个完整用户行为或完整源模块。

## 固定参考与覆盖表

本次主参考 [Pi](https://github.com/earendil-works/pi) `d5b3de22d0e24a3e9c38a9410d3329b099717283`；[Harness](https://github.com/deepseek-ai/deepseek-harness) `22ddee3792402c3e70ec5f86a63c3e3e895a2f60`；[Cordis](https://github.com/cordiverse/cordis) `df4431eb43086bd1f8263a0e8c03e9b7c5233b68`。Pi 路径均相对 `references/pi/`。`packages/durable/src/index.ts`、`package.json` 的本地运行入口与 `docs/spec.md` 是覆盖核对入口。下面的组覆盖每类导出；每步完成时在其交接记录附具体函数/类型到公开原生头的映射。

| 上游行为组 | 源码阅读入口 | 本次任务 |
|---|---|---|
| 原生基础与外部接口 | pi-ai/types、Durable/types，现有基础库头 | K01 |
| 服务依赖、作用域、启动/释放 | Harness `vendor/cordis/src/{context,registry,service,fiber}.ts`；Cordis `packages/core/src/` | K02 |
| Entry、事务、Session、Cursor、分页、所有权 | Durable `entries.ts`、`types.ts`、`session/` | K03 |
| Document/family、table、revision、checkpoint、迁移 | Durable `documents.ts`、`session/transaction.ts` | K03 |
| Memory/JSONL/SQLite、读写失败、只读打开 | Durable `storage/`、`errors.ts` | K04 |
| Models、流式内容、thinking、tool、usage、认证 | pi-ai `types.ts`、`models.ts`、`auth/`、`api/` | K05 |
| Task、phase、checkpoint、sleep/wake、owner、join | Durable `tasks.ts`、`timers.ts`、`harness/scheduler.ts` | K06 |
| Generation、partial、deferred、重试、工具轮次 | Durable `harness/{generation,partial-changes,provider,live}.ts` | K07 |
| 编码工具、执行环境、输出截断、图片 | Durable `tools/`、`env/`、`truncate.ts`、`images/` | K08 |
| Extension、registry、section、hook、wrap、reload | Durable `harness/{define,registry,agent,prompt,tool}.ts` | K09 |
| Submission、inbox、steer、follow-up、settlement | Durable `harness/{submissions,inbox}.ts` | K10 |
| Conversation、fork、reset、handoff、嵌套工具/子任务 | Durable `harness/harness.ts`、`session/forks.ts`、`harness/tool.ts` | K10 |
| 项目配置、AGENTS、skills、templates、MCP | Coding Agent `core/{resource-loader,settings-manager,skills,prompt-templates}.ts`；`packages/mcp/src/` | K11 |
| Code mode、工具搜索与持久嵌套调用 | Durable `codemode/`、`tool-search/`；`packages/codemode/src/` | K11 |
| Context、compaction、分叉摘要 | Durable `harness/{context,compaction}.ts`；Coding Agent `core/compaction/` | K12 |
| View/watch、events、task graph、usage、report/telemetry | Durable `harness/{view,events,task-graph,usage,telemetry}.ts`；`session/` | K13 |
| 本地应用、模型/会话操作、TUI 服务接口 | Coding Agent `core/agent-session.ts` 与 `modes/interactive/`；Durable `harness/harness.ts` | K14–K15 |

## 实施顺序

### K01 — 整理可直接复用的原生基座

- [x] 核对当前 `src/runtime/`、`src/bootstrap-api/`、`src/platform-*`、`src/arena/`、`src/buffer/`、`src/text/`、`src/json/`、`src/schema/` 和产品实际构建清单，为上表每组列出当前原生入口与所需补齐点，记在本文交接区。
- [ ] 对照当前实际消费者统一内存、Buffer、字符串与文件/进程/时钟接口，复用已接入的json-c/libcurl/utf8proc。后续模块需要的基础能力随对应K步骤实现，纯逻辑手写汇编化。
- [ ] 验收：所用模块可构建、替换、释放；上游数据样例、容量边界和 ABI 对应通过。

### K02 — 将实际产品接入原生插件宿主

来源：Harness 的 Cordis `Context/RegistryService/Service/Fiber`；`references/cordis/packages/core/tests/{plugin,service,service-dispose}.spec.ts` 的对应测试。

- [x] 将静态服务注册、依赖排序、隔离根作用域、激活、失败清理和协作释放接入实际 `src/composition/`。
- [x] 完成父子作用域的服务继承、局部提供者替换和子级先释放。
- [ ] 完成服务变化、单插件/绑定撤销与重新激活的生命周期。
- [ ] Loop、Session、Task/Scheduler、Storage、Model、Tools和基础服务均按服务契约注入。
- [x] 原生插件使用服务描述符与版本化清单，提供静态/本地共享库入口；库代码在宿主协作清理完成后卸载。
- [ ] 验收：替换提供者后消费者行为成立；缺依赖、重复服务、初始化失败和释放时订阅/异步资源的处理与参考对照。

### K03 — 完整的 Session、事务和状态记录

来源：Durable `types.ts`、`entries.ts`、`documents.ts`、`session/{session,transaction,forks}.ts`；`session-*.test.ts`。

- [x] 移植Document地址身份、作用域/owner/family-key解析和上游JSON tuple编码，作为事务/存储查询共用基础。
- [x] 移植通用准备批次的Storage提交边界：脱离调用者输入、同步adopt、成功结算后发布、只读/空批次及失败锁存。
- [x] 完成conversation/entry/task/submission四类记录的内存批次准备、原子应用、全局ID分配与独立点读；接通通用提交入口。
- [x] 完成四类表过滤/分页/游标顺序、fork-aware Entry可见范围、提交序号及latest head marker读取。
- [x] 接通私有StorageWrite事务批次、普通表read-after-write屏障、seal/commit/discard与有序submission候选结算，提交复用现有Storage/adopt契约。
- [ ] 原生落点：现有 `session.S/h` 和明确归属的 documents/entries 模块。补齐 Entry、Conversation、Document/family、Task、Submission 与表查询的事务级存取、ID/版本、游标分页。
- [x] 完成文档值的scope/history/fork兼容检查、版本拒绝/迁移要求与隔离物化；迁移回调和JSON返回值所有权明确。
- [x] 完成已提交revision的current/as-of快照读取、生命周期截点、最近base选择与同版本delta尾重放。
- [x] 将Document create/change/retire与其余四类表接入同一原子内存批次，维护全局ID、live地址唯一性及revision历史，提供current/as-of物化读取。
- [x] 完成Document逻辑地址查找、精确scope/kind过滤与升序分页；current及历史成员关系返回独立生命周期record。
- [x] 完成definition-free Document.copy：已提交源快照转为目标base，核对kind/key/history/fork并拒绝同批变更复制源。
- [x] 接通已存在Doc的事务内编辑获取/地址复用、统一checkpoint提交与loaded adoption；包含迁移版本写回和失败清理。
- [x] 完成新Doc的缓存安装候选、索引容量预留和Storage确认后的createdAt/loaded安装；批量安装后直接命中缓存。
- [ ] 完成文档生命周期、所有权、revision、checkpoint、迁移提交、延迟提交、只读视图和一致的查询语义。
- [ ] 验收：一批同时修改条目/文档/任务原子成功或失败；提交确认后通知观察者；失败会话停止；分页、版本和生命周期与对应上游用例比较。

### K04 — 完整存储与重开

来源：Durable `storage/memory.ts`、`storage/jsonl/`、`storage/sqlite/`；`memory-storage`、`jsonl-storage`、`sqlite-storage`、`sqlite-migrations` 测试。

- [ ] 在 Storage 接口下完成内存、JSONL与SQLite提供者，复用SQLite成熟库；维护当前原生日志及其显式数据转换入口。
- [ ] 将 K03 的记录、查询、事务与版本迁移接入持久提供者；完成锁、尾部恢复、失败关闭与只读打开。
- [ ] 验收：同一 Session 场景在各后端重开得到相同语义；完整批次、损坏/截断和 IO 失败有明确结果；原生文件与导入文件字节可核对。

### K05 — 完成 pi-ai 对话模型层

来源：pi-ai `types.ts`、`models.ts`、`api/`、`auth/`、`utils/provider-retry.ts` 及对应 provider 测试。

- [ ] 将模型/协议、端点、目录、能力、认证和凭据配置变为可注入提供者；完成 text/thinking/image/tool blocks、usage、stop reason、流终态、取消、超时、错误分类、重试与缓存身份。
- [ ] 顺序移植对话协议：OpenAI Chat Completions → Responses → Anthropic Messages → Gemini/Vertex → Bedrock → Azure → Codex/Copilot与其上游网关适配。按相同协议复用编码和解析模块。
- [ ] 验收：上游请求/响应样例与原生字段、事件、错误一致；每个协议有确定性流回放；现有MiMo真实对话、工具、取消、恢复继续成立。

### K06 — 持久 Task 与可替换 Scheduler

来源：Durable `tasks.ts`、`timers.ts`、`harness/scheduler.ts`；`harness-tasks*`、`harness-sleep`、`harness-blocked`、`harness-cancellation-barrier` 测试。

- [ ] 原生落点：任务定义/注册、phase/checkpoint、任务记录和调度服务；接入 K03/K04。
- [ ] 完成 pending/running/waiting/sleeping/completing/terminal、owner/child、join/allSettled、timer/wake、blocked/restart、abort barrier与关闭恢复。
- [ ] 验收：每个可恢复边界退出重开；等待取消与任务取消分别验证；父子结束、失败和释放顺序按上游任务场景成立。

### K07 — 把 Generation 和 Tool 作为持久任务运行

来源：Durable `harness/{generation,tool,partial-changes,provider,live}.ts`；`harness-generation*`、`harness-tools-recovery`、`harness-deferred-progress` 测试。

- [ ] 在现有 `agent.S/h` 上完成 generation/tool任务、turn/run/settled、草稿与提交事实、流进度、批量调用、顺序/并行工具轮次和结果关联。
- [ ] 完成可恢复模型重试、deferred生成、partial变化、工具中断结算和副作用恢复策略；模型请求沿用已提交配置与身份。
- [ ] 验收：无工具、单/多工具、错误、取消、暂停与进程恢复的状态/事件/副作用逐项对照；达到消息/输出预算前给出可处理结果。

### K08 — 完成编码工具与环境适配

来源：Durable `tools/{read,write,edit,bash,edit-diff,image-processor}.ts`、`env/`、`images/`、`truncate.ts`；`tools.test.ts`、`tools-read-differential.test.ts`、`tools-images.test.ts`、`env-node.test.ts`。

- [ ] 在现有 read/write/edit/bash 服务上补齐上游输入准备、路径/目录、分页/完整文件、UTF-8/二进制/图片、并发同文件操作、超时/取消、截断/溢出文件和错误语义。
- [ ] 完成 edit 的 diff/patch/首行结果与工具结构化结果；将工作区、文件和进程能力作为注入的 ExecutionEnv 服务。
- [ ] 验收：固定工具语料与上游实际函数比较，核对文件字节、目录、命令退出及清理；真实模型完成 read→edit→write→bash→回读。

### K09 — 完整原生 Extension 能力

来源：Durable `harness/{define,registry,agent,prompt,tool}.ts`；`harness-registry`、`harness-lifecycle`、`harness-ownership`、`harness-tools` 测试。

- [ ] 实现 extension注册/选择、工具增减、section/wrapSection、wrapTool、generation/tool/compaction hooks和自定义task。
- [ ] 将本地原生插件装载、更新、依赖变化与宿主作用域生命周期连接；状态保存稳定名字/数据，运行时解析服务。
- [ ] 验收：两个原生扩展可组合、替换、撤销并正确清理；提交中的配置变化、工具声明与实际可执行集合一致；hook错误/取消与上游顺序对应。

### K10 — 输入队列、多会话与分叉

来源：Durable `harness/{harness,submissions,inbox}.ts`、`session/forks.ts`；`harness-inbox`、`harness-host-inputs`、`harness-conversations`、`harness-nested-tools*`、`session-forks` 测试。

- [ ] 完成 requestId去重、输入/写入submission、wait/settlement、steer/follow-up队列与忙时策略。
- [ ] 完成多个conversation、查询/命名、fork/clone、reset/handoff、父子任务、嵌套工具、所有权与级联取消；每个conversation保存独立provider身份。
- [ ] 验收：队列和分叉在重开后保持顺序/归属，已结算输入可再次查询；取消等待、取消当前任务与关闭宿主的语义可区分。

### K11 — 项目资源、MCP和代码工具组合

来源：Coding Agent `core/{resource-loader,settings-manager,settings-schema,skills,prompt-templates,system-prompt,auth-storage}.ts`、`packages/mcp/src/`；Durable `codemode/`、`tool-search/` 及其测试。

- [ ] 完成项目/用户配置优先级、AGENTS上下文、system/append/section、skills、prompt templates和资源刷新；配置解析与资源加载成为服务。
- [ ] 完成 MCP stdio/HTTP客户端的发现、调用、取消、认证与清理，并转成现有Tools接口。
- [ ] 对照 Pi codemode 的工具能力隔离、声明生成和持久嵌套调用，通过QuickJS的C接口完成原生桥接；完成tool search后动态工具声明与实际执行同步。
- [ ] 验收：同名资源覆盖、刷新、重开、模板参数、MCP错误/取消、代码模式恢复和动态工具选择均有对应参考用例。

### K12 — 上下文投影与压缩

来源：Durable `harness/{context,compaction}.ts`、`harness-compaction.test.ts`；Coding Agent `core/compaction/` 的分叉摘要。

- [ ] 完成 reset/compaction/fork后的上下文投影、token估算、保留边界、自动/手动压缩、后台压缩、摘要模型与hook。
- [ ] 压缩任务接入Scheduler/Storage，运行中的取消、失败、工具调用配对与重开保持一致。
- [ ] 验收：超过当前有界会话规模的长工作流可通过压缩持续运行；原始记录、摘要与模型可见上下文能从存储重构；压缩中断后恢复。

### K13 — 观察、Usage与任务图

来源：Durable `harness/{view,events,task-graph,usage,telemetry}.ts`、`session/`；`harness-view`、`harness-events`、`harness-task-graph`、`session-watches`、`spec-usage` 测试。

- [ ] 完成只读snapshot、watch/退订、增量消息/工具/任务事件、会话视图、任务图、usage/cost与report/trace上下文。
- [ ] 为TUI提供公开查询/操作函数表，覆盖会话/模型选择、历史分页、队列、压缩、任务和配置状态。
- [ ] 验收：观察者初始快照与增量衔接正确，退订/关闭释放；多次重开保留累计用量，TUI显示来自实际Runtime事实。

### K14 — 完成本地 Agent 应用组合

来源：Durable `Harness` 与 Coding Agent `core/agent-session.ts` 的模型、资源、会话操作；现有 `src/composition/pangu.c`。

- [ ] 将 K02–K13 的服务组合成实际程序：工作区、模型/凭据、资源、存储、宿主与TUI桥接；完善启动参数、配置加载与关闭顺序。
- [ ] 对接TUI TODO的所有操作请求，按已实现头文件逐项联调；完成会话选择、导入/导出、模型/思考配置与扩展刷新所需Runtime服务。
- [ ] 验收：从干净临时工作区启动，真实模型完成编码工作流、会话管理、压缩、扩展调用、暂停/恢复，操作数据逐项可核对。

### K15 — 完整移植核对

- [ ] 逐行核对覆盖表和对应入口中的public行为，检查实现、原生差异和实际验证；把发现的缺口补回所属K步骤并完成。
- [ ] 运行受影响全部模块检查及产品构建，配合TUI的U08执行最终用户工作流。
- [ ] 从真实TUI执行：配置模型→加载项目资源→连续对话→读改写运行→steer/follow-up→压缩→新会话/分叉→扩展/MCP/代码工具→任务中断恢复→查看历史/用量→正常退出。每个结果以实际状态、存储和副作用核对。
- [ ] 本文K01–K15与TUI U01–U08均满足验收，在主文档记录当前参考提交、构建命令和完整移植结论。

## 已有基线，直接接着做

现有Loop/Session、MiMo流、read/write/edit/bash及TUI纯逻辑已为手写汇编。已验证的实现和命令见 [当前实现记录](migration/runtime-core-progress.md)。K03–K08可复用这些实现；完整任务仍按各步骤全部验收条件判断。

下一批从K01核对现有落点开始，随后进入K02的实际宿主组合，按依赖完成各步。

## 移植方法来源

| 实际材料 | 用到本文的方法 |
|---|---|
| [yannelli/pi-rust parity playbook](https://github.com/yannelli/pi-rust/blob/66ea9a61ac5dfa828aa216ab84cfbd45106cb2b0/docs/parity/README.md) | 上游模块到目标模块映射、固定同步提交、逐个功能补差异 |
| [Bun PORTING](https://github.com/oven-sh/bun/blob/46d3bc29f270fa881dd5730ef1549e88407701a5/docs/PORTING.md) 与 [移植工作流](https://github.com/oven-sh/bun/blob/23427dbc12fdcff30c23a96a3d6a66d62fdc091d/.claude/workflows/phase-e-proper-port.workflow.js) | 类型/所有权/目标文件预先明确，沿原始源码修复真实函数 |
| [Copilot迁移复盘](https://github.blog/ai-and-ml/generative-ai/migrating-the-github-copilot-runtime-to-rust-using-copilot/) | 依赖顺序、逐组件替换、状态/生命周期核对、集成职责集中 |
| [pie模型选择任务](https://github.com/c4pt0r/pie/blob/b72579680bdfe616edbc05808fb1395a1be3660f/docs/superpowers/plans/2026-06-11-model-picker.md) | 每步给出文件、动作和可运行验收 |
| [nktkt路线](https://github.com/nktkt/pi/blob/0808c756fa1991940def7f0f9837464417149419/ROADMAP.md)、[bigfish路线](https://github.com/bigfish1913/pi-rust/blob/fcfc9314304f53662127782f551c589b0919965a/ROADMAP.md)、[vamsi模块布局](https://github.com/vamsi/pi/blob/dc6b092508ce471a4f57064f3d4700dc2ab78b83/README.md) | 按行为成组、共享基础库分层、运行时与应用组合分开 |

对应资料已保存到 `references/pi-rust-yannelli`、`pie`、`pi-rust-nktkt`、`pi-rust-bigfish`、`pi-python-vamsi`、`bun-porting`。功能与行为的实现依据为上表中的Pi和Harness源码。

## 核心交接区

- K03 新Doc安装：固定Pi d5b3de22 的 Transaction.adopt(created 分支)→Session.host.install/touchDocument；原生 documents.h/.S 的 install_prepare/reserve/apply/discard 对应准备、预留、提交后采纳与放弃。提前复制record/value并分配stamp/node，采用json-c公开resize/insert接口。32个真实MemoryStorage批量创建后缓存直接命中；重复/过期/关闭拒绝及故障注入通过。missing Doc 的自动初始化/创建由后续 document_tx 获取流程串接。


每批更新以下字段；接口请求从TUI交接区读取。

- 当前步骤：K03 Document与事务；K01/K02的实际基座可供使用，其余子项按当前勾选状态继续。
- 上游同步：2026-10-26 fetch Pi/Harness，工作树干净，固定提交与远端均仍为上文版本；本批无新增上游修复。
- 本批来源：Harness `vendor/cordis/src/context.ts` 的 Context/作用域查询，`registry.ts` 的 Inject.resolve/plugin/inject，`service.ts` 的服务发布，`fiber.ts` 的 effect/释放，`reflect.ts` 的声明注入与父级查找；Cordis `packages/core/tests/plugin.spec.ts` 的注册/无效插件/生命周期，`service-dispose.spec.ts` 的消费者释放期间保留提供者。原生使用同步激活和可重复调用 close 表达协作释放。
- 公开接口：`src/runtime/host.h` 的 `pg_host_init/init_child/start/provide/get/find/close`；`host.S` 手写依赖检查、拓扑排序、发布和逆序释放；`src/composition/pangu.c` 以七个静态插件组合实际程序。
- 所有权：宿主由调用者提供1120字节存储；描述符只读借用，服务由提供者持有。每作用域最多32插件/解析服务、32子作用域、每插件32依赖、名字最多63字节；层级最多32条子级边。激活成功且声明导出齐全后才可见；失败清理包含部分初始化插件；pending/error清理保留所有权和依赖。子作用域继承父服务，局部导出遮蔽同名父服务；父关闭先驱动全部子级释放，pending/error期间保留父资源。
- 实际验证：宿主 O0/O2 的 ASan/UBSan 专项通过，覆盖依赖乱序、缺失/重复/环、发布权限、初始化失败、递归调用拒绝、pending/error清理、独立/子级提供者替换、嵌套关闭、父级等待子级错误/异步清理、32项/32层边界和页尾保护。Runtime 全专项通过。固定1092bdc的TUI加本批核心，在隔离临时树的真实MiMo `--live-workflow` 与 `--live-prompt-snapshot` 退出0；新增工作区打开失败PTY用例确认退出2并精确恢复termios。
- 实际失败：同一隔离组合的 `--live-bash-recovery` 退出1，模型仅叙述执行而没有调用bash，缺少应产生的spill路径；该场景未通过。共享树整机验证等待TUI当前渲染批次完成，未修改其源码。
- 已处理TUI登记请求：`tools/client/tui/test_tui_pty.py` 登记为限定在 `tools/client/tui/` 的Python客户端工具；检查器新增接受/拒绝用例，17项自测通过。共享client/adapter函数表保持原布局。
- Session/Storage装配：`session.h` 的 `pg_session_source` 提供一次加载/回放入口；实际入口将内存与Journal源作为服务注入，Agent声明依赖，存储提供者负责打开失败清理并在Agent释放后关闭。按固定Pi的 `session/session.ts:createSession(storage)`、`harness/harness.ts:Harness.open` 及 `memory-storage.test.ts` 读取采用存储与执行者分离的组合语义；完整Session/Storage能力仍由K03/K04实现。
- 原生共享库入口：`native_plugin.h/.S` 的 `pg_native_open/close`，使用导出对象 `pangu_plugin_v1`（ABI/清单大小/描述符大小、插件数组）；显式绝对路径装载，内部宿主按现有注入与父子作用域工作。对照Harness固定提交 `vendor/loader/src/config/entry.ts:Entry._init`、`resolve.ts:createResolve` 与 `fiber.ts:restart`；原生差异为POSIX动态链接和明确借用的C ABI。共享库本身的静态/实例状态由插件定义，宿主不改写其描述符。
- 原生装载验证：五种真实共享库（正常/ABI错误/缺入口/缺依赖/初始化失败）与宿主进行实际链接和装载。O0/O2专项通过；pending保留代码映射、完整清理后RTLD_NOLOAD确认已卸载、父作用域关闭后由库所有者完成卸载、替换上游seed服务后新实例得到新值；4096字节非终止路径拒绝。完整Runtime专项退出0。固定TUI组合产品构建通过，动态符号表确认七个公开Host ABI函数；实际扩展配置加载由K09/K11接入。
- K03地址实现：`documents.h/.S` 对应Pi固定提交 `documents.ts:resolveAddress/ownerId/addressId`、`ids.ts:idFromNumber`；并阅读 `transaction.ts:appendEntry` 和文档暂存路径、`session-documents.test.ts` 的family与语义/版本拒绝用例。`pg_document_address` 明确scope、signed-safe-integer owner、可选key和借用UTF-8 slice；json-c承担JSON转义，纯逻辑与所有权编排手写汇编。
- K03实际验证：`tools/runtime/test_documents.py` 直接导入固定Pi原函数，通过Node模块解析hook指向同版本实际Chord/json.ts，未替换被测函数；792条地址逐字节一致。O2 ASan/UBSan测试通过，覆盖空kind/空key/缺key、嵌入NUL、负owner边界、页尾slice、输出别名/容量及逐个JSON分配/插入/序列化故障，输出在失败时保持不变。地址模块尚待下一批事务调用，K03整体仍未验收。
- K03物化实现：`pg_document_check/materialize` 对应 `documents.ts:checkRecordScope/checkRecordVersion/materializeDocumentValue`。90条有效定义/记录组合直接与Pi函数比对结果一致；原生枚举边界补充校验。输入JSON只读，迁移在私有副本执行，回调返回owned引用，最终再复制以隔离回调保留引用；ASan/UBSan覆盖成功、错误和两次deep-copy失败清理。
- K03值准入：对照同版本 `packages/chord/src/json.ts:copy/isJsonValue`，新增 `pg_document_value_valid`，在输入复制和迁移结果复制前检查有限数值与祖先环；允许共享子对象。C ABI限定256层容器，避免原生栈无限递归，超过边界返回拒绝。实际Chord函数对照finite/nonfinite/cycle/DAG六种图一致；ASan/UBSan验证256层接受、257层拒绝，非法输入/迁移返回被拒绝，调用者保留输入图所有权。
- K03批次入口：`records.h/.S:pg_records_commit` 对应 `types.ts:Storage.commit`、`session/session.ts` 的提交/GuardedStorage.#call、`transaction.ts:settleSuccess/discard`；阅读并移植 `session-adoption.test.ts` 的同步采纳、采纳前/后错误、漏采纳及采纳错误场景。原生使用同步C ABI回调：backend持久化后调用adopt，返回时核对唯一采纳与相同序号，再发布；已采纳的状态在后端后续失败时保留，Session停止继续提交。
- K03批次验证：O0/O2 ASan/UBSan专项通过；混合Entry/Document准备批次用确定性Storage边界验证整批脱离、adopt-before-resolve/publication、空/只读、重复/缺失/错误序号、回调内重入拒绝、采纳失败不会逃逸到Storage、后端失败锁存，以及复制失败/部分复制清理和页尾Session布局。完整记录约束由Storage/Tx实现，当前模块是提交契约，尚未接通通用记录持久提供者。
- K03内存表提供者：`record_memory.h/.S` 对应Pi固定提交 `storage/memory.ts:prepareCommit/checkGlobalIds/applyPreparedCommit/mintId/conversation/entry/task/submission`。四类记录先复制/校验并预留容量，再无分配地应用与同步adopt；原生用有序行数组和libc qsort维护索引，避免每次克隆全部已存状态。ID全局共享，conversation/entry不可覆盖，task/submission仅原类型更新且同批最后值生效；读返回owned副本。
- K03提供者验证：`test_record_memory.py` 直接导入实际MemoryStorage与实际Chord/delta，430次混合批次/读取/mint操作一致，含整数/整数浮点ID；O0/O2专项验证乱序/重复/跨表ID、失败无半批、替换/读取隔离、容量增长、提交序号、释放/重开、与pg_records_commit组合。ASan/UBSan及分配失败检查通过，故障覆盖深复制的部分结果、暂存分配、已有行扩容失败、读取复制失败和页尾提供者对象。当前提供者仍待Document revision与扫描/查询接入。
- K03表查询：`record_memory.h/.S` 的 `pg_record_memory_scan/entry/head` 对应Pi `storage/scan.ts:scanStart/nextCursor`、MemoryStorage的四类scan、`visibleEntries/visibleEntriesAscending`、`entry/findLatestHeadMarker`。原生Query/Page使用明确C布局，过滤字段采用presence bits，状态/顺序用enum；limit为正整数，游标after为signed-safe整数，旧游标保留表默认顺序。Entry按fork段及各层at截点遍历，降序按需访问祖先，升序先收集可见段；Entry/head取首项即结束，cursor通过二分定位起点；读取返回owned快照。
- K03查询验证：实际Pi累计1324次批次/点读/分页/Entry/head/ID操作一致，覆盖六种task状态、四种submission状态、owner/kind/bool过滤、NUL/Unicode、旧/新游标、非法顺序/非safe游标、七层fork、inclusive bounds及head截点，含短页/点读在访问缺失祖先前成功的惰性边界。O0/O2 ASan/UBSan验证Query/Page页尾、关闭后快照、adopt期间查询、部分分页分配失败和错误输出归零。当前按有序全局行过滤，各fork段按引用范围扫描；Document查询及Tx暂存继续接入。
- K03 revision重放候选：`delta.h/.S` 对照Chord固定提交的 `assertValidOp/assertSafePath/applyOps/applyImmutable/applyImmutableBatches/resolve/copyContainers`，以及MemoryStorage的 `documentDeltaBatches/materializeDocument`；采用decoded r/s/d/a/t/p/m、稠密数组、reserved path拒绝、UTF-16计数及最终值重放。原生在私有JSON副本应用操作，输入与ops不变，单次多批重放复用同一工作根；对照pending.ts:merge的追加合并语义，同路径连续a一次拼接，ASCII截断使用直接计数。
- K03 delta验证：449个单/多批实际Chord结果一致；C专项覆盖失败原子性和每个直接分配/插入/复制故障。三条已识别ABI差异单独验证：非BMP字符内部UTF-16截点、含NUL属性路径、超safe整数数字路径会被当前候选拒绝，仍纳入K03/K15兼容核对；候选用于Document值还原，Storage revision接入继续推进。
- K03 revision读取：`delta.h/.S:pg_document_revision_read` 对照Pi `MemoryStorage.materializeDocument/isAliveAt/isCurrentOnly/documentDeltaBatches` 和 `storage-conformance.ts` 的历史/长尾/拷贝场景，原生view借用已提交revision数组，snapshot拥有独立JSON值。historical current-only先拒绝，再检查[createdAt,retiredAt)；current只读未退役，最近base后的delta版本必须一致。96条实际MemoryStorage读取对照一致，含退役/未退役、Session/Task/Conversation、latest/rewindable与base版本切换。O0/O2 ASan/UBSan覆盖逐个直接分配/插入/部分复制故障，失败输出清空。接口暂供Storage集成，未声明Document写入完成。
- K03 Document提供者：`record_memory.S` 对照同版Pi `prepareDocumentActions/checkDocumentActions/applyDocumentActions/materializeDocument`，Document与四类表共用全局行索引、预留后无分配应用及adopt；同批content/retire先按ID合并并检查，live地址在批次最终状态中唯一。latest新base替换历史、退役清空；rewindable保留revision。`pg_record_memory_document` 返回独立StoredDocument JSON。公开Memory结构及既有表查询ABI不变。
- K03 Document验证：累计2048条实际MemoryStorage操作对照一致，增加四类scope/history组合、连续delta与版本base、退役/重建、重复命令、混合表失败、历史截点读取。O0/O2内存专项验证创建、已有值更新、退役+同址重建、读取时逐直接分配/插入/部分复制失败；失败不改变已存记录/ID/序号，不采纳，输出归零。完整Runtime检查通过，新增故障场景后的全量复验见当前实现记录。
- K03 Document查询：`pg_record_memory_find_document/scan_documents` 对照Pi `MemoryStorage.findDocument/scanDocuments/isAliveAt`、`addressKey`、`scopeKey`与`page`，阅读 `storage-conformance.ts` 的退役重建和身份字段场景。Native地址复用既有48字节类型；DocumentQuery 64字节，Page沿用24字节。成员关系可查询current-only的历史，区别于content历史拒绝；at截点按[createdAt,retiredAt)判断。当前基于provider有序全局行过滤，不新增平行存储。
- K03查询验证：4922条累计实际MemoryStorage对照一致；新增singleton/空key、NUL kind/key、同址新旧实例、scope/kind过滤、0～120截点、负/正after及分页。O0/O2 ASan/UBSan验证输出隔离、缺失、无效limit、复制部分结果/分配/插入失败清理。完整Runtime复验结果见当前实现记录。K03仍待copy及完整Tx。
- K03 Document复制：`record_memory.S:.Ld_resolve_copies` 对照Pi固定版 `MemoryStorage.resolveDocumentCopies` 与 `storage-conformance.ts` 的独立base/ambiguous source场景。先在私有输入中解析所有copy，拒绝源ID在本批任何document命令中变化，源按current/as-of物化；Conversation owner可变而kind/key/history/fork保持一致，生成create/base后复用同一原子准备路径。公开读取仍拒绝prepare期间重入，内部只读路径仅用于解析已提交源。
- K03复制验证：累计5254条实际MemoryStorage操作一致，含历史/current、latest/rewindable、NUL family key、定义不符、退役目标/源、源后续变化、两种顺序的同批修改及链式copy拒绝；O0/O2 ASan/UBSan验证复制独立、输入不变与每个直接分配/插入/部分复制失败的源状态/ID/序号不变。完整Runtime结果见当前实现记录。
- K03事务submission候选：`records.S/h:pg_records_submission_change` 对照 `transaction.ts:applySubmissionChange`，阅读Transaction的submissionChanges暂存、`settleSuccess/discard`和`#read/#write`调用规则。手写转换保留终态、区分input/write放置、要求done从placed结算；输出独立拥有，输入不变。24个组合与固定上游实际函数体直接对照一致（仅剥离TS类型后执行原函数体），O0/O2 ASan/UBSan覆盖两段复制/部分复制故障。该入口等待完整事务组装调用，尚未提供完整Tx。
- K03事务批次：`records.h/.S:pg_record_tx_begin/read/stage/commit/discard/submission_change` 对照Transaction构造、`#read/#write`、`settleSuccess/discard`及Session提交调用，采用私有候选、写后禁止公开表读、结算先seal与最终Storage/adopt。原生48字节Tx借用Session和注入的committed-read服务；同步回调期间拒绝Tx重入，整个Session mutation line由调用者串行持有。stage接收已构造的StorageWrite；typed创建/ID/文档tracker及异步drain继续由后续事务层实现。
- K03事务验证：与真实MemoryStorage组合验证暂存不可见、混合批次提交、丢弃/重用、已seal拒绝、非法/失败表写后的read barrier、Document暂存后仍可读表、readonly和Storage失败、Session锁存、queued→placed→done连续候选及终态保持。O0/O2内存专项枚举暂存与submission转换的直接复制/插入/分配故障；读回调错误的partial结果释放及重入拒绝专项通过。原24条上游submission函数和5254条Storage对照继续通过。完整Runtime结果见当前实现记录。
- K03 typed Entry：`pg_record_tx_append_entry` 对照固定Pi `Transaction.#appendEntry/#requireConversation`，检查同批新Conversation或committed Conversation，调用注入Storage mint，覆盖id/conversationId/byTaskId并将head=self转为新ID；候选及返回记录分别持有，失败不插入半条Entry，mint后的错误不回退ID。10条构造语料执行实际上游方法体与实际Chord copyJson（固定mint/read host，仅核对构造）一致；真实MemoryStorage组合及O0/O2内存/逐分配插入故障通过。该入口尚待应用消息投影使用。
- K03 typed Conversation：`pg_record_tx_create_conversation` 对照Pi `#stageConversation/#currentTask`，提供保留root ID、ownerless和task-owned记录构造；普通创建先mint再查同批最新task候选/committed task，归属从task.conversationId派生。返回值与暂存候选隔离；mint后错误保留ID消耗。4条构造语料使用实际上游方法体（无parent，固定mint/task/creation-hook host）直接对照一致；真实Storage验证同批/已提交owner和候选覆盖优先级。O0/O2内存专项枚举创建/owner读取/复制/插入故障，失败不追加记录。完整Runtime结果见当前实现记录。
- K03 fork文档选择：`pg_records_prepare_fork` 对照Pi `session/forks.ts:prepareForkDocumentCopies/collectCopies` 及 `session-forks.test.ts` 的继承Entry/重复策略场景，注入entry/scan/mint服务，先按可见Entry实际owner与commitSeq选择asOf，再按请求父会话current选择current；initial跳过，分页256，去除生命周期stamp并替换目标scope/ID，重复kind/key目标地址拒绝。输出拥有独立copy plans；mint后错误保持ID消耗。
- K03 fork验证：5262条累计实际MemoryStorage操作对照含直接调用实际prepareForkDocumentCopies，600个文档跨页、祖先Entry提交截点、NUL family key、initial跳过、缺失/不可见Entry、asOf/current冲突与后续mint一致。实际Storage将plans作为document.copy提交，读得分叉时旧值。O0/O2 ASan/UBSan及逐直接分配/插入/复制/读错误清理通过。完整Runtime结果见当前实现记录。
- K03 fork事务装配：`pg_record_tx_fork_conversation` 对照Pi `#stageConversation/#assemble/#rejectForkSourceWrites`，在独立候选数组中准备owner、parent与全部copy，成功才替换原Tx批次；保留原候选引用，失败仅消耗已分配ID。Tx committed-read与fork Storage可用不同context，局部桥接维持原服务注入。commit在Storage前拒绝copy源的同批变更及父会话新增current-policy文档，避免预备错误锁存Session。
- K03 fork事务验证：实际Storage验证parent/文档同批落盘、历史值还原、task owner与不同回调context、返回parent隔离；选择失败/逐分配插入复制故障保留原候选，fork前/后修改源及新增父级current文档均在admission前拒绝且Session健康。O0/O2 ASan/UBSan、原5262条Storage/选择对照与完整Runtime通过；最后补充返回parent隔离专项在最终源O2通过。
- K03 Tx查询服务：`pg_record_tx_scan/latest_head` 对照Transaction `scanConversations/scanEntries/scanTasks/latestHeadMarker` 共用 `#read` 规则；query/page原有112/24字节布局移入共用records契约，Memory通过24字节descriptor提供现有scan/head实现。普通表写尝试后拒绝，Document暂存不设barrier，sealed后拒绝，callback期间防重入。服务descriptor使用GOT兼容动态链接。
- K03 Tx查询验证：真实Memory组合通过fork分页/过滤/祖先head、无head与未知Conversation区分、各表查询、写后读/sealed拒绝；O0/O2 ASan/UBSan验证partial失败结果释放、输出归零、回调重入与逐直接分配复制插入故障。共享库对照初次因PC32重定位链接失败，改用GOT后原5262条Storage对照通过；完整Runtime结果见当前实现记录。
- K03 Conversation host：`pg_record_tx_create` 对照Session/Transaction `conversationCreated` 调用，统一root/new/fork后调用注入host；callback在候选已暂存后运行，可追加记录/嵌套创建，活跃callback期间拒绝commit/discard。Tx原44字节offset预留字段改为creation_depth，整体48字节不变；回调错误返回CREATION_FAILED，由外层失败callback丢弃候选，尚未存储。实际Memory组合验证回调追加Entry、嵌套、fork、空hook、失败丢弃及逐分配插入复制故障；O0/O2内存专项通过，完整Runtime结果见当前实现记录。Harness内建Live/Inbox/Usage/Provider初始化继续按对应K步骤接入该host。
- 已处理TUI登记请求：共享工作树登记tui_sink.h、tui_md.h/.S与test_tui_md.c，使用既有client-source/client-test范围，G00通过。相关登记（含之前PTY工具）暂留工作树，随TUI交接一并提交；未将未交接TUI文件混入本核心候选。
- K03最终owner校验：对照Transaction `#validateOwners/#currentTask/#assemble` 与 `session-tables.test.ts`，在Storage准入前检查新Conversation/Task owned work：owner取最新暂存候选，拒绝missing/terminal/completing/abort；已有任务保留旧owner不当作新建work。原生Task新建按未提交ID识别，typed创建应使用mint新ID。48组实际Storage顺序/状态/abort组合及同批新owner结束场景通过；27组执行实际Pi校验方法体一致。owner读取分配失败/partial清理不锁存Session；O0/O2内存专项、原5262条Storage对照及完整Runtime通过，最后新增同批新owner专项在最终源O2通过。
- K03文档编辑内核：`delta.h/.S:pg_document_change_open/append/snapshot/prepare/discard` 对照Chord `ChangeImpl.prepare/abort`、Tracker `beginChange` 及Transaction `#acquire/planDocument/settleSuccess`。原生显式操作日志取代JS proxy：open/append脱离输入，snapshot/prepare才重放并检查路径/操作语义，snapshot独立拥有；prepare成功或失败均封闭句柄，成功转交value/ops，discard不释放已转交Prepared。追加只复制新批次，避免每次流式追加重放全文；prepare复用已有合并追加重放。
- K03编辑验证：448条操作日志结果与实际Chord一致，3条既有ABI拒绝边界单列；O0/O2 ASan/UBSan覆盖open/append/prepare逐分配/插入/部分复制故障，追加失败回退本批、prepare失败清理/封闭、输入与快照隔离。实际MemoryStorage验证编辑→Prepared→Tx document.change→提交，旧快照稳定。此API不声称JS proxy或ops规范化兼容，原生操作/错误时点已写明；完整Runtime结果见当前实现记录。
- K03 loaded文档写入规划：`pg_document_plan_change` 对照Transaction `planDocument` loaded分支及`#assemble` checkpoint选择；迁移写base（含空ops），同版非空ops按提供的checkpoint决策选base/delta，同版空ops不写，拒绝newer stored version。planner为纯构造，不执行predicate；完整Doc结算须在所有验证后计算决策。72条执行实际Pi原函数/分支一致，输入/payload隔离与逐分配插入复制故障通过；实际Storage验证迁移materialize→空编辑→base、checkpoint base、普通delta与空编辑不提交。O0/O2内存专项及完整Runtime结果见当前实现记录。
- K03文档载入句柄：documents.h/.S的record_decode/loaded_open/loaded_close与loaded_begin对照Session #loadDocument未命中缓存分支及Transaction #acquire的loaded分支，注入find/read，检查current实例身份/scope/version并物化迁移，保留stored/value version与deltasSinceBase。句柄独立拥有record/value；Memory提供24字节绑定descriptor。编辑复制当前物化值，关闭/失败清理metadata。
- K03载入验证：128条新鲜载入执行实际Pi原方法体及实际materializeDocument/track，匹配Storage调用顺序，含scope/history/fork/family NUL、版本/迁移与缺失读。真实Memory验证迁移载入→空编辑→base提交、版本拒绝、Storage关闭后独立持有与逐分配/读取/部分复制故障；O0/O2内存专项通过。缓存命中/驱逐、holds和adoption版本协调尚待控制层；完整Runtime见当前实现记录。
- K03文档缓存：documents.h/.S的cache接口对照Session #loadDocument/#touchDocument/#dropDocument/#sweepDocuments/#pinDocument；完整地址（含NUL family key）使用json-c linkhash，手写节点/idle链与生命周期。相同valueVersion复用并检查definition语义，不同version先held flush再drop/reload，失败保留旧句柄；pin由观察/holds服务提供，变更后touch，sweep由Session执行线显式调用。缓存借用Definition，句柄只读借用期限明确。
- K03缓存验证：11步复用/版本切换/hold flush执行实际Pi原方法体一致；160个NUL family地址跨哈希扩容，重复获取无额外find/read。触摸粒度、101ms retention、pin/unpin、零retention、条件驱逐、close/reopen及直接分配/插入/读取失败O0/O2通过。完整Runtime通过，最后新增命中definition不符专项在最终源O2通过。统一Session mutation/adoption、holds与自动timer调度尚待接入。
- K03 loaded adoption：delta.h/.S的adoption_prepare/apply/discard对照Transaction.adopt与Chord Tracker.adopt，Storage前复制候选并保留预期base引用/版本/计数/ID，确认后无分配交换；空ops保留当前物化值，迁移更新storedVersion，base重置deltas、delta递增。过期或重复使用拒绝，discard释放未发布候选。目标句柄在apply/discard前保持有效并由Session执行线串行持有。
- K03 adoption验证：12个场景执行实际Pi adopt与实际Chord tracker一致；O0/O2验证竞争候选失效、重复采纳、输入隔离和直接分配/部分复制失败。真实Storage/adopt/publish组合覆盖成功、采纳前失败、采纳后后端报错：后者保留已采纳值且锁存Session；apply期间故障注入计数为0。完整Runtime结果见当前实现记录。此处为loaded incarnation能力，统一Doc事务/创建安装/退役驱逐继续接入。
- K03多loaded文档统一结算：document_tx.h/.S的pg_document_tx_commit接收同一mutation line上的已载入句柄与编辑集合，统一prepare、生成写入、校验后checkpoint、准备adoption及Storage确认后的采纳，最后调用原Session adopt/publish并恢复其context/hooks。records增加sealed finalizer与批次验证入口，避免checkpoint早于生成计划验证。空编辑不产生Storage提交；重复incarnation/edit拒绝。
- K03统一结算验证：实际MemoryStorage验证双文档base、delta与普通表混合、空编辑保持identity、checkpoint失败、owner/复制源错误不调用predicate、回调恢复及每个直接分配/插入/复制故障；旧值在失败时保持，编辑与Prepared全部清理。O0/O2专项通过，完整Runtime见当前实现记录。该入口处理loaded文档，Doc自动获取/创建/退役/holds仍待上层接入。
- K03 loaded自动获取：document_tx的open/get/finish/abort管理48字节Doc scope，按当前事务地址复用首次编辑/definition/checkpoint，调用cache载入已有实例，独立分配稳定编辑句柄、动态扩展结算数组，finish复用统一提交并释放编辑，abort丢弃整个record Tx。普通表写后仍可获取Doc；任务候选terminal先拒绝，包含已获取地址的再次访问。
- K03自动获取验证：16个family实例跨结算数组扩容，原句柄稳定，同址不同version token仍复用首次draft（对应Pi memoization）；重复获取无额外find，批量提交更新各loaded值。验证abort、missing、表写后获取、terminal候选及逐直接分配/读取/扩容失败清理。O0/O2内存专项通过；完整Runtime见当前实现记录。当前get对缺失地址返回MISSING，初始化创建/retirement/holds继续接入；缓存和loaded借用受同一Session执行线保护。
- K03新Doc构造：documents的record_create对照Pi documentCreate，按definition/address生成owned DocumentCreate而不含生命周期stamp，验证identity/原生ID及scope/history/fork。initialize对照Transaction.doc/acquire：family seed先复制，initializer返回owned object经严格JSON校验与再次复制，singleton忽略seed，callback partial返回均释放；callback保留引用不影响最终候选。
- K03创建基础验证：189条记录执行实际Pi documentCreate一致，含singleton/空family key/NUL、三类scope和所有history/fork组合；initializer输入/返回隔离、callback失败与逐直接分配/插入/复制故障O0/O2通过。真实Storage完成初始化→mint→记录构造→Tx document.create→重新loaded载入，生命周期stamp由Storage产生，原seed后续变化不影响内容。自动获取missing分支仍待接入这些服务。
- 下一完整行为：接通missing Doc初始化创建、缓存安装及退役驱逐与延迟提交，接入当前消息投影。

### K01 当前落点与补齐映射

| 行为组 | 已有原生入口 | 所属步骤补齐点 |
|---|---|---|
| 基础内存/文本/外部接口 | `bootstrap-api/{memory,region,arena,buffer,text,io}.h`、对应platform/arena/buffer/text；产品通过libc与utf8proc适配 | 按实际消费者复用；早期C基础实验未进入当前产品构建，不作为汇编完成证明 |
| JSON/Schema | 产品模型/工具调用json-c；`bootstrap-api/{json,schema}.h`是已有实验入口 | 协议与工具字段随K05/K08验证；复用成熟库 |
| 服务宿主 | `runtime/host.h`、`host.S` | K02绑定撤销/重新激活与全部服务注入 |
| Entry/Session/Document | `runtime/session.h/.S` | K03通用记录、文档/表、分页/版本；当前为有界消息事务 |
| Storage | `runtime/session.h`的pg_storage、`records.h`/`record_memory.h`、`journal.h/.S`、`journal_posix.c` | K04内存/JSONL/SQLite与迁移 |
| Model | `runtime/agent.h`的pg_model、`mimo.h`、`mimo*.S` | K05目录/认证、多协议与完整内容块 |
| Task/调度 | `session.h`的pg_run_checkpoint、`agent.h/.S` | K06通用持久任务、timer/join/owner服务 |
| Generation/工具轮次 | `agent.h/.S`、`session.h/.S` | K07持久generation/tool、deferred与完整恢复 |
| 编码工具/环境 | `tools.h`、`read_tool.h`、`write_tool.h`、`edit_tool.h`、`bash_tool.h`、`process.h`及手写实现 | K08完整分页/图片/diff与环境契约 |
| Extension | `tools.h`的工具注册、`host.h`的静态插件 | K09扩展/hook/动态装载 |
| 输入/会话/分叉 | `submissions.h/.S`、`agent.h/.S` | K10队列、多conversation与fork |
| 项目资源/MCP/代码工具 | `prompt.h/.S`、`profile.h/.S`的当前配置身份与system快照 | K11资源加载与MCP/codemode |
| Context/压缩 | `session.h`消息与`mimo_projection.S` | K12投影/压缩任务与摘要 |
| View/事件/usage | `contracts/client.h`、`client/tui/runtime_bridge.h` | K13完整snapshot/watch/usage与任务图 |
| 应用组合/TUI入口 | `composition/pangu.c`、`tui_runtime_adapter.h` | K14/K15完整操作与联调 |

实际构建输入以 `governance/assembly-first.json` 为准；`tools/runtime/build_test.py` 验证当前Runtime及提供者替换，基础实验不自动成为产品依赖。K01其余项仍按全部消费者的ABI与释放验证判断。
