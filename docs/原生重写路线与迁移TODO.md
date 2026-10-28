# 给核心 Agent：完成 Pi Durable 原生移植

你是 PanGu 的核心实现 Agent。工作目录 `/home/xu-yuanhao/Documents/PanGu`。请按本文执行批次成组移植、接线、验证并更新进度。实现语言按**手写 Assembly 第一、C 第二、C++ 第三**。对应 TUI Agent 使用 [TUI TODO](TUI移植TODO.md)。

## 完成目标

完成 Linux x86-64 上可实际使用的原生 Pi Durable TUI：Durable 的会话/事务/文档、持久任务/调度、输入队列、模型生成、工具、扩展、分叉、压缩和观察能力都能通过原生服务工作；Pi 原 Coding Agent 提供项目资源与编码工作流；deepseek-harness/Cordis 的组合与生命周期语义用于原生插件宿主。

当前任务是完整移植Pi，必须对照Pi Durable、Pi Coding Agent及deepseek-harness/Cordis的对应实现开发。每组功能沿现有算法、调用链和用例移植，原生重组服务于内存、ABI、平台与插件组合需要。下面每一步都完成、覆盖表全部有实现与验证、TUI 最终工作流通过，才勾选 K15。每项实现从已有代码继续，复用已验证的部分。

## 并行职责

你维护 `src/runtime/`、`src/contracts/`、`src/composition/`、原生基础/宿主模块、`tests/runtime/`、`tools/runtime/`、`tools/migration/`、`tools/gates/`、`governance/`，以及 `src/client/tui/runtime_bridge*` 和 `tui_runtime_adapter.h`。你更新本文与当前实现记录。

TUI Agent 维护其余 `src/client/tui/`、`tests/client/tui/`、`tools/client/tui/` 和 TUI TODO。每个集成批次先查看 TUI TODO 的接口/登记请求，处理到自己归属的头文件与清单中，再在本交接区记录已经提供的入口。公共接口变更保持现有消费者可构建，TUI 按已到位的接口接入。 并行批次内固定既有函数表字段、枚举值和尺寸，新增能力通过独立公开接口提供；既有布局的迁移在双方交接就绪后由核心统一集成。

每次只暂存核对过的具体路径。TUI 候选交接后由你集中做组合、真实模型和终端验证。

## 执行批次与完成方式

K01–K15是完整功能清单，B01–B05是实际执行顺序。跨K步骤完成调用链，满足所属项验收时勾选；全部功能最终在B05核对完成。已有可用实现直接复用。

| 批次 | 完整交付物 | 对应清单 | 批次集成验收 |
|---|---|---|---|
| B01 存储与Session | 将现有事务、记录、Document和Session接入Memory/JSONL/SQLite，完成打开、提交、关闭、重开与失败处理 | K03、K04；随消费者完成K01/K02相关接口 | 同一混合记录/文档事务在各后端提交后重开；历史、版本、只读、损坏/截断与失败首因按上游场景成立 |
| B02 Durable执行链 | 持久Task/Scheduler接通Generation/Tool、现有模型/编码工具、应用组合和TUI桥接 | K06、K07；接通K05/K08/K10/K13/K14的实际调用路径 | 真实模型完成读改写运行；执行中退出，重开后按持久事实继续；工具副作用和任务终态可核对 |
| B03 模型、工具与插件完整覆盖 | 完成全部模型协议、编码工具行为、Extension与宿主生命周期和服务替换 | K01、K02、K05、K08、K09及B02实际差异 | 上游协议/工具场景对照；扩展组合、替换、撤销与任务执行接通 |
| B04 完整工作流与TUI联调 | 队列、多会话/分叉、项目资源、MCP/代码工具、压缩、观察/用量/任务图和全部客户端操作 | K10、K11、K12、K13、K14；TUI U05/U06联调 | 从TUI执行对应操作，重开后查询状态、历史、用量和任务事实一致 |
| B05 全覆盖收尾 | 按来源表核对每个公开行为，补齐所属步骤的剩余差异，与TUI完成全部验收 | K01–K15、U01–U08 | K15/U08完整工作流及覆盖表通过 |

### 批次内的工作循环

1. 按表定位整组上游模块、直接调用者和相关测试，固定提交；结合现有实现一次确定当前调用链的文件、接口与所有权。
2. 成组实现参考行为并接入实际消费者，内部模块按原生所有权和服务边界组织。普通`Promise/AbortSignal`使用续接、协作推进、取消与清理；Durable可恢复任务使用持久状态。UTF-8 slice、明确tag/函数表和成熟外部库承担原生数据表达。
3. 将对应上游用例移到原生边界，具备运行条件时以相同输入差异对照结果、状态、错误和副作用；开发中运行受影响专项，失败按实际差异修复。批次集成时运行完整回归与产品工作流。
4. 通过后覆盖更新交接区：来源、实现入口、结果、具体剩余差异、下一交付物。完成所属K项即勾选，继续本批调用链。

批次完成以表中集成行为为准。先选择该批最短的可运行接线路径，其余行为仍保留在所属K项并在本批或覆盖收尾完成。基础模块的细节随调用者和上游用例集中补齐；每次交接写明产品能力的实际变化，重复辅助层工作按AGENTS推进规则调整。

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

## 完整功能清单

### K01 — 整理可直接复用的原生基座

- [x] 核对当前 `src/runtime/`、`src/bootstrap-api/`、`src/platform-*`、`src/arena/`、`src/buffer/`、`src/text/`、`src/json/`、`src/schema/` 和产品实际构建清单，为上表每组列出当前原生入口与所需补齐点，记在本文交接区。
- [ ] 对照当前实际消费者统一内存、Buffer、字符串与文件/进程/时钟接口，复用已接入的json-c/libcurl/utf8proc。后续模块需要的基础能力随对应K步骤实现，纯逻辑手写汇编化。
- [ ] 验收：所用模块可构建、替换、释放；上游数据样例、容量边界和 ABI 对应通过。

### K02 — 将实际产品接入原生插件宿主

来源：Harness 的 Cordis `Context/RegistryService/Service/Fiber`；`references/cordis/packages/core/tests/{plugin,service,service-dispose}.spec.ts` 的对应测试。

- [x] 将静态服务注册、依赖排序、隔离根作用域、激活、失败清理和协作释放接入实际 `src/composition/`。
- [x] 完成父子作用域的服务继承、局部提供者替换和子级先释放。
- [ ] 完成服务变化、单插件/绑定撤销与重新激活的生命周期。
- [ ] Loop、Session、Task/Scheduler、Inputs、Storage、Model、Tools和基础服务均按服务契约注入。
- [x] 原生插件使用服务描述符与版本化清单，提供静态/本地共享库入口；库代码在宿主协作清理完成后卸载。
- [ ] 验收：替换提供者后消费者行为成立；缺依赖、重复服务、初始化失败和释放时订阅/异步资源的处理与参考对照。

### K03 — 完整的 Session、事务和状态记录

来源：Durable `types.ts`、`entries.ts`、`documents.ts`、`session/{session,transaction,forks}.ts`；`session-*.test.ts`。

已有：Document地址/物化/revision、四类记录原子内存提交、Tx候选与查询、文档编辑/退休/fork-copy、held提交与定时器、Session关闭/观察/Watch/readOnLine已成组实现。具体入口见`src/runtime/{records,record_memory,document_tx,document_reader,durable_session,holds,hold_timer}.*`；已有细项与验证历史可查Git。

- [ ] 完成Entry、Conversation、Document/family、Task、Submission与表查询的事务级存取、ID/版本、游标分页，覆盖上游Session/Transaction公开入口。
- [ ] 完成文档生命周期、所有权、revision、checkpoint、迁移提交、延迟提交、只读视图、观察和一致查询语义，接通K04持久后端与K06调度调用者。
- [ ] 验收：一批同时修改条目/文档/任务原子成功或失败；提交确认后通知观察者；失败会话停止；分页、版本、取消与生命周期通过对应上游用例及重开场景。

### K04 — 完整存储与重开

来源：Durable `storage/memory.ts`、`storage/jsonl/`、`storage/sqlite/`；`memory-storage`、`jsonl-storage`、`sqlite-storage`、`sqlite-migrations` 测试。

- [x] 原生SQLite提供者接入Storage/Session提交：准备后事务持久化、确认后采纳、混合记录/文档跨进程重开、只读打开、过期写入者拒绝、失败锁存及损坏拒绝；持久化采用PanGu原生批次日志。
- [x] 原生JSONL提供者接入Storage/Session：main marker与task/doc sidecar编码、准备后写入、确认后采纳、独占写租约、跨进程重开、撕裂/未确认尾部恢复及提交后及恢复时终态任务/当前checkpoint清理，维护失败保留已提交事实并在恢复时重试。
- [x] 在Storage接口下完成内存、JSONL与SQLite提供者，复用SQLite成熟库；维护PanGu自身的存储版本与原生日志。
- [ ] 将 K03 的记录、查询、事务与版本迁移接入持久提供者；完成锁、尾部恢复、失败关闭与只读打开。
- [ ] 验收：同一 Session 场景在各后端重开得到相同语义；完整批次、损坏/截断和 IO 失败有明确结果；PanGu存储版本、提交边界与重开后的逻辑事实可核对。

### K05 — 完成 pi-ai 对话模型层

来源：pi-ai `types.ts`、`models.ts`、`api/`、`auth/`、`utils/provider-retry.ts` 及对应 provider 测试。

- [ ] 将模型/协议、端点、目录、能力、认证和凭据配置变为可注入提供者；完成 text/thinking/image/tool blocks、usage、stop reason、流终态、取消、超时、错误分类、重试与缓存身份。
- [ ] 顺序移植对话协议：OpenAI Chat Completions → Responses → Anthropic Messages → Gemini/Vertex → Bedrock → Azure → Codex/Copilot与其上游网关适配。按相同协议复用编码和解析模块。
- [ ] 验收：上游请求/响应样例与原生字段、事件、错误一致；每个协议有确定性流回放；现有MiMo真实对话、工具、取消、恢复继续成立。

### K06 — 持久 Task 与可替换 Scheduler

来源：Durable `tasks.ts`、`timers.ts`、`harness/scheduler.ts`；`harness-tasks*`、`harness-sleep`、`harness-blocked`、`harness-cancellation-barrier` 测试。

- [x] 单任务phase执行器接入实际Durable Session及SQLite/JSONL：定义解析、checkpoint推进、异步续接、sleep/wake、迁移、fault/abort、owned-child completing、正常关闭与进程中断恢复。入口`task_runner.S/h`。
- [x] 全局协作调度发现已提交活跃任务，打开时先恢复所有running记录；按任务ID逐轮公平推进，动态发现新任务、重试缺失定义、报告sleep截止时间，并在关闭时统一清理。入口`pg_task_scheduler_*`。
- [x] 持久waiting接入phase提交与恢复：allSettled/failFast、成员存在与直接所有权校验、等待/普通所有权环检测、后台分支隔离；失败成员与取消意图派生同一事务内的标记，子级先于父级abort。
- [ ] 原生落点：任务定义/注册、phase/checkpoint、任务记录和调度服务；接入 K03/K04。
- [ ] 完成 pending/running/waiting/sleeping/completing/terminal、owner/child、join/allSettled、timer/wake、blocked/restart、abort barrier与关闭恢复。
- [ ] 验收：每个可恢复边界退出重开；等待取消与任务取消分别验证；父子结束、失败和释放顺序按上游任务场景成立。

### K07 — 把 Generation 和 Tool 作为持久任务运行

来源：Durable `harness/{generation,tool,partial-changes,provider,live}.ts`；`harness-generation*`、`harness-tools-recovery`、`harness-deferred-progress` 测试。

- [x] 接通Generation/Tool共用的原子phase事务：Entry、Document、工具子任务及waiting/checkpoint同批提交；等待和所有权检查使用最新候选，回调/环/最终owner校验失败整体撤销。入口`pg_task_runner_commit_with`。
- [x] model-issued `pangu.tool`接入真实read/write/bash服务与Task/Scheduler：call/execute意图先存、双safe恢复、异步输出/关闭、abort、暂停排空及原子结果Entry/终态。入口`durable_tool.S/h`；Generation和嵌套工具按下面剩余项继续。
- [x] Generation prepare/request/tools接通真实模型→assistant/owned工具任务→waiting→结果入库→后继Generation；请求配置和cutoff持久化，提供者从已提交配置恢复模型/端点及maxTokens/temperature；SQLite/JSONL上的真实DeepSeek Flash闭环与重开通过。入口`durable_generation.S/h`。
- [ ] 在原生Generation/Tool与Client调用链完成turn/run/settled、草稿与提交事实、流进度、批量调用、顺序/并行工具轮次和结果关联。
- [x] 接通模型错误分类、失败assistant/Task状态原子结算、当前重试策略、持久指数退避/截止时间、提前唤醒、重开续试及取消；真实模型在一次故障注入后完成工具闭环。
- [ ] 完成deferred生成、partial变化、完整工具中断结算和副作用恢复策略；模型请求沿用已提交配置与身份。
- [ ] 验收：无工具、单/多工具、错误、取消、暂停与进程恢复的状态/事件/副作用逐项对照；达到消息/输出预算前给出可处理结果。

### K08 — 完成编码工具与环境适配

来源：Durable `tools/{read,write,edit,bash,edit-diff,image-processor}.ts`、`env/`、`images/`、`truncate.ts`；`tools.test.ts`、`tools-read-differential.test.ts`、`tools-images.test.ts`、`env-node.test.ts`。

- [ ] 在现有 read/write/edit/bash 服务上补齐上游输入准备、路径/目录、分页/完整文件、UTF-8/二进制/图片、并发同文件操作、超时/取消、截断/溢出文件和错误语义。
- [ ] 完成 edit 的 diff/patch/首行结果与工具结构化结果；将工作区、文件和进程能力作为注入的 ExecutionEnv 服务。已有：真实参数随tool_started事件送至TUI；结果Entry原子保存参数、明确状态、details和diagnostics；edit提供diff/patch/firstChangedLine；TUI桥接提供独立的持久工具结果查询。已有诊断文本渲染、bash结构化结果与单条工具消息投影；附件与完整ExecutionEnv待补齐。
- [ ] 验收：固定工具语料与上游实际函数比较，核对文件字节、目录、命令退出及清理；真实模型完成 read→edit→write→bash→回读。

### K09 — 完整原生 Extension 能力

来源：Durable `harness/{define,registry,agent,prompt,tool}.ts`；`harness-registry`、`harness-lifecycle`、`harness-ownership`、`harness-tools` 测试。

- [ ] 实现 extension注册/选择、工具增减、section/wrapSection、wrapTool、generation/tool/compaction hooks和自定义task。
- [ ] 将本地原生插件装载、更新、依赖变化与宿主作用域生命周期连接；状态保存稳定名字/数据，运行时解析服务。
- [ ] 验收：两个原生扩展可组合、替换、撤销并正确清理；提交中的配置变化、工具声明与实际可执行集合一致；hook错误/取消与上游顺序对应。

### K10 — 输入队列、多会话与分叉

来源：Durable `harness/{harness,submissions,inbox}.ts`、`session/forks.ts`；`harness-inbox`、`harness-host-inputs`、`harness-conversations`、`harness-nested-tools*`、`session-forks` 测试。

- [x] 文本Submission接入真实产品：requestId去重、输入/写入admission、状态轮询/撤回、steer/follow-up的one/all边界、忙时reject、展示写入、reset/stale、失败keep/continue/withdraw及持久结算。
- [ ] 完成 requestId去重、输入/写入submission、wait/settlement、steer/follow-up队列与忙时策略。
- [ ] 完成多个conversation、查询/命名、fork/clone、reset/handoff、父子任务、嵌套工具、所有权与级联取消；每个conversation保存独立provider身份。
- [ ] 验收：队列和分叉在重开后保持顺序/归属，已结算输入可再次查询；取消等待、取消当前任务与关闭宿主的语义可区分。

### K11 — 项目资源、MCP和代码工具组合

来源：Coding Agent `core/{resource-loader,settings-manager,settings-schema,skills,prompt-templates,system-prompt,auth-storage}.ts`、`packages/mcp/src/`；Durable `codemode/`、`tool-search/` 及其测试。

- [x] 用户与项目默认skills接入模型请求：目录SKILL.md优先、根目录Markdown与嵌套发现、名称/描述校验、同名先加载优先、规范路径去重、禁用自动调用过滤和XML描述；真实模型按需读取技能/相对资源并完成写入。
- [x] 用户/项目默认prompt templates与显式技能命令接入前台提交：引号参数、位置/全部参数、默认值和切片单次展开；/skill:name读取当前正文（含禁用自动调用的技能），展开后的实际输入进入Durable持久执行。
- [x] 用户/项目settings.json接入资源调用链：保留两层及递归覆盖结果，BOM/缺失/错误诊断，skills/prompts中的普通文件与目录路径按所属作用域解析，复用实际技能发现和模板提交。
- [x] 核心resources服务接通配置/技能/模板/系统提示的空闲整体刷新、失败保留、独立快照与前台展开；提供Host服务和可选TUI资源操作表，真实模型验证刷新前后及失败后的继续执行。
- [x] 应用资源选择接入默认/配置目录：`!`/`+`/`-`优先级、显式资源优先和同层项目优先；筛选先于同名采纳，记录已排除路径避免自动发现重新加入，模板按应用名称去重；常见glob、brace/range与1978组固定上游匹配对照通过。
- [ ] 完成项目/用户配置优先级、AGENTS上下文、system/append/section、skills、prompt templates和资源刷新；配置解析与资源加载成为服务；内置提示词保留参考机制并适配PanGu自称、`.pangu`目录、配置与实际入口。
- [ ] 完成 MCP stdio/HTTP客户端的发现、调用、取消、认证与清理，并转成现有Tools接口。
- [ ] 对照 Pi codemode 的工具能力隔离、声明生成和持久嵌套调用，通过QuickJS的C接口完成原生桥接；完成tool search后动态工具声明与实际执行同步。
- [ ] 验收：同名资源覆盖、刷新、重开、模板参数、MCP错误/取消、代码模式恢复和动态工具选择均有对应参考用例。

### K12 — 上下文投影与压缩

来源：Durable `harness/{context,compaction}.ts`、`harness-compaction.test.ts`；Coding Agent `core/compaction/` 的分叉摘要。

- [x] Generation与历史恢复共用最新head优先投影；保留范围内包含旧head的编辑，omit/replace按最后一项生效；分页读取超过256条持久记录，SQLite/JSONL重开保持结果。
- [x] 模型可见上下文按调用顺序整理工具结果：下一assistant前的首个同ID结果、跨用户消息匹配、排除错误/取消/deferred assistant、孤立/重复结果过滤及缺失结果临时回执；实际请求编码与两后端重开通过。
- [x] 当前OpenAI请求路径按pi-ai折叠system正文和命名section：内容顺序追加、覆盖保序、null删除、重新加入移到末尾；接入Generation/历史/实际编码，容量边界与两后端重开通过。
- [x] 核心手动压缩服务接通动态Entry/贡献视图、UTF-16估算与合法保留边界、固定配置/范围的摘要请求、持久select/summarize/place及原子Submission/任务终态；真实模型验证70条消息、两次阶段重开、原记录保留和摘要后继续对话，提供独立TUI操作表。
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

- [x] 产品前台接入Durable Client/Session/Scheduler/Generation/Tool：默认`.pangu` JSONL、模型/工具事件、取消、暂停与已提交历史。核心专项与真实模型编码工作流通过；TUI候选整合见交接区。
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

存储重开、持久Task/Scheduler和模型/工具执行链已接通，作为B03/B04后续完整覆盖与工作流的现有基线。具体当前批次和剩余行为见交接区。

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

- 来源：Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283` 的 Durable harness/{context,compaction}.ts deriveRange/orderToolResults/selectCut/isCandidate/serializeConversation/CompactionTask，以及pi-ai utils/estimate.ts；摘要提示词与MIT来源保留。固定快照沿本批使用，缓存上游restartLimit修复通过原生Task cutoff接入。
- 实际调用链：Host compaction服务/可选TUI表 → 手写Assembly创建conversation-owned Task → 动态Entry/贡献视图和UTF-16估算选择保留边界 → 固定tail/firstKept/模型配置 → 空工具目录摘要请求 → 持久保存摘要 → 同一事务admit write Submission并完成Task → 最新head上下文 → 后继模型对话。Generation/历史共用同一动态视图，原始Entry继续保留。
- 验证：保留边界、工具配对、摘要格式/焦点、2000 UTF-16工具结果截断、跨surrogate替换、容量与ABI专项ASan/UBSan通过；真实DeepSeek在SQLite/JSONL各完成70条消息摘要、select后重开、保存摘要后重开、原记录cutoff回读及继续回答旧标记。发送入口核对摘要system、空tools和512输出token预算，发布回调验证摘要Entry/Submission/Task终态同批。Runtime整批回归、核心产品构建、实际Host服务/可选TUI表绑定与资源集成通过；隔离核心G00和Assembly通过。
- 接口与自审：新增task.compaction.local.v0、pg_durable_context_read/order和同一admission实现的外层事务入口；模型/客户端既有布局保持。产品注入同一选定Model提供者的空工具目录请求，提供pg_compaction_ops及pg_tui_runtime_compaction；TUI可弱链接获取后start/poll/release。原生将摘要保存和最终放置分为持久阶段，最终admission与终态原子提交。登记/依赖/许可证自审通过，并行TUI、assets与其规划文档保持原位。
- 剩余与下一交付物：继续K12自动阈值/overflow/background、generation-owned阻塞压缩、重试/取消与Live/hook、完整大文本摘要请求及system基线；当前请求/摘要仍受模型ABI容量约束。TUI的/compact交互消费已提供的独立表，完整K12与最终TUI验收继续按原覆盖表。其他K项范围保持。
