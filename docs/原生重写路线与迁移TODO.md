# 给核心 Agent：完成 Pi Durable 原生移植

你是 PanGu 的核心实现 Agent。工作目录 `/home/xu-yuanhao/Documents/PanGu`。请按本文执行批次成组移植、接线、验证并更新进度。实现语言按**手写 Assembly 第一、C 第二、C++ 第三**。对应 TUI Agent 使用 [TUI TODO](TUI移植TODO.md)。

## 完成目标

完成 Linux x86-64 上可实际使用的原生 Pi Durable TUI：Durable 的会话/事务/文档、持久任务/调度、输入队列、模型生成、工具、扩展、分叉、压缩和观察能力都能通过原生服务工作；Pi 原 Coding Agent 提供项目资源与编码工作流；deepseek-harness/Cordis 的组合与生命周期语义用于原生插件宿主。

当前任务是完整移植Pi，必须对照Pi Durable、Pi Coding Agent及deepseek-harness/Cordis的对应实现开发。每组功能沿现有算法、调用链和用例移植，原生重组服务于内存、ABI、平台与插件组合需要。下面每一步都完成、覆盖表全部有实现与验证、TUI 最终工作流通过，才勾选 K15。每项实现从已有代码继续，复用已验证的部分。

产品交互优先级按 2026-10-28 用户澄清执行：Pi 式简洁交互 TUI 为主，Grok Build 布局/输入/历史体验为参考，Codex/Claude Code 兜底。TUI U02 原完成判定撤回，下一交付先替换滚动调试壳为完整主界面，具体调研和验收见 [TUI TODO](TUI移植TODO.md)。核心保持真实服务接线，不用可运行 REPL 或 headless 替代 TUI 验收。

K14 的命令行范围仅取固定 Pi `cli/args.ts` 与 main 分派：install/remove（uninstall）/update/list/config/auth/mcp，以及帮助/版本、模型和会话启动参数、资源/工具开关、export/list-models、print、text/json/rpc 等已有入口；逐项沿对应模块移植并适配 PanGu 身份。默认 `pangu` 进入交互式 TUI，不建设独立 CLI 产品或另一套会话；`config` 的交互选择也接同一 TUI。命令清单和实际支持状态须分别记录。


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

- [x] 默认model.metadata接通原生内置聊天模型元数据、用户agent目录models.json的容量/输出覆盖与自定义模型元数据；按provider/model或endpoint/model选择，只输出公开字段。默认prepare按canonical provider/model逐字段应用compaction.modelOverrides，刷新、隔离所有权和真实自动/后台压缩通过。
- [x] models.catalog/TUI可选入口提供公开模型目录，支持provider筛选、内置/用户models.json同名覆盖去重，复用describe输出能力/容量，结果排除私有认证；与已有Agent模型选择共同提供模型面板Runtime入口。
- [x] OpenAI编码器采纳modelInfo.compat的supportsUsageInStreaming与maxTokensField，按模型移除不支持的用量选项并选择输出字段，显式options覆盖模型默认；思考参数按DeepSeek/Zai/Qwen/OpenRouter格式和supportsReasoningEffort映射，普通端点不带提供商专用thinking字段；公开reasoning/thinkingLevelMap贯通目录、覆盖和请求，支持级别/关闭映射及off:null省略；实际编码与模型传输/ABI专项通过。
- [x] OpenAI Responses基础请求/流接入同一模型服务：按api/modelInfo.api或端点选择，投影input、扁平函数声明/调用/结果、图片结果、max_output_tokens与reasoning；解析文本/思考、稀疏工具索引、最终参数替换、用量及完成/长度/失败终态。实际curl服务回放覆盖跨块/CRLF、工具调用、提前EOF和失败；O0/O2/UBSan、ABI及分配失败检查通过。完整Responses行为仍按下项覆盖。
- [x] Responses最终item按输出槽保存，正文/思考采用done事实，终态补齐加密思考；canonical消息保存textSignature、thinkingSignature及独立responseItemId。provider/model/api一致时重放，切换模型/提供者移除相应签名和item身份；SQLite/JSONL重开、真实请求投影、跨协议投影、乱序done、分配失败与ASan/UBSan通过。
- [x] Responses终态结算与失败接通产品：全部function item.done确认后交付调用，已关闭槽忽略迟到参数；incomplete保留长度终态与rawStopReason。结构化提供者message/error body进入既有重试和上下文超限分类；并行部分完成、实际模型服务拒绝未完成调用、HTTP 200错误分类、关闭释放及O0/O2/ABI/ASan/UBSan通过。
- [x] Responses最终参数接通修复、错误元数据与Tool结算：空白为{}，修复字符串控制字符/非法转义，失败保留原文并生成argumentsError；模型调用先交付准备后的对象，complete保存最终元数据，Tool在效果前返回错误。416组固定Pi源码差分、构造失败释放、SQLite/JSONL无写入副作用与NUL错误文本、真实DeepSeek错误后重试写入通过。
- [x] Anthropic Messages基础链接入原生模型服务：system/文本/图片、签名思考、tool_use/tool_result分组、工具声明及API-key/version头；有序块关闭后解析最终参数，message_stop确认流终态，输入/缓存用量按增量合并。实际模型服务回放覆盖文本/工具、错误参数、长度终态、结构化错误与提前EOF；canonical历史按模型身份重放思考，预算/自适应思考编码及分配失败清理通过。
- [x] PanGu会话身份接通OpenAI Chat/Responses请求缓存：按上游选择prompt_cache_key、按64个Unicode字符截取，短/长/关闭及Responses显式缓存模式映射；模型O0/O2/UBSan、传输与所有权专项通过。
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

- [ ] 在剩余 read/write/edit/bash 行为上补齐上游输入准备、完整文件、UTF-8/二进制、并发同文件操作、超时/取消、截断/溢出文件和错误语义。
- [x] 工具参数贯通Mimo SSE、Durable Generation与工具分派，按增量动态保存到128 KiB上限；write分配并写入完整内容，64 KiB实际文件字节回读一致，文本显示预算保持原值。
- [x] Edit完整读改写接入同一调用链：内部完整快照按需扩容，Assembly文本工作区及Pi diff/patch按文件大小分配，内部write解析支持大文件；196 KiB文件端到端编辑后逐字节回读一致；替换向量按实际数量扩容，128项及逆序/BOM/CRLF批量编辑对照通过，NUL正文从参数、文本变换、实际写入到diff/patch按显式长度保留，模型可见参数边界保持128 KiB。
- [x] read普通UTF-8分页接入Pi Durable的50 KiB/2000行预算和续读offset，移除固定1 MiB扫描上限；多页重组与大于1 MiB的行偏移通过。模型可见读取剥离文件开头UTF-8 BOM，保留NUL并按TextDecoder规则替换非法UTF-8；Number型offset/limit按slice语义选择行，超长首行返回码点完整前缀，续读提示进入诊断元数据。1600组固定Pi正文差分及读取改写/追加行为通过；Edit内部完整快照保留BOM。
- [x] read默认图片路径接入Durable结果与请求：PNG/JPEG/GIF/WebP头部尺寸、默认inline限制、base64、读取稳定性重试；afterTool保留图片，真实read Task持久结果可投影为模型附件。
- [x] read按模型inputLimits与视觉能力运行，支持注入image.processor；prepared结果保留转换、缩放和坐标映射说明，配置失败不替换现有选项。
- [x] 原生图片处理器接入应用：真实解码、八种EXIF转正、Lanczos缩放、PNG/JPEG候选与质量梯度、字节预算降采样、BMP转换及损坏图片拒绝；实际read持久结果覆盖缩放/坐标说明。
- [x] 完成 edit 的 diff/patch/首行结果与工具结构化结果。真实参数随tool_started事件送至TUI；结果Entry原子保存参数、明确状态、details和diagnostics；edit提供diff/patch/firstChangedLine；TUI桥接提供独立的持久工具结果查询；bash结构化结果与诊断已投影。
- [x] Bash采用50 KiB/2000行UTF-8尾部预算，完整保留结果通过snapshot进入Tool Entry、重开和动态模型请求；超过预算保留完整原始输出文件。60 KB字节回读、预算内10 KB无溢出、Unicode、取消/超时及真实DeepSeek调用→长输出→回答通过。
- [x] 将工作区、文件和进程能力组合为注入的 ExecutionEnv 宿主服务。BashPrepare式回调可在启动前调整命令、工作目录和显式环境；拒绝调用时不创建输出文件也不启动进程。
- [ ] 验收：固定工具语料与上游实际函数比较，核对文件字节、目录、命令退出及清理；真实模型完成 read→edit→write→bash→回读。

### K09 — 完整原生 Extension 能力

来源：Durable `harness/{define,registry,agent,prompt,tool}.ts`；`harness-registry`、`harness-lifecycle`、`harness-ownership`、`harness-tools` 测试。

- [x] 自定义Task接入Host task.definitions：汇编校验/查找、内置与扩展重名拒绝、真实Scheduler执行/暂停重开；动态库经公开任务提交ABI保存checkpoint与终态，消费者清理后卸载。
- [x] Agent配置服务接入pangu.agent文档：字段保留/替换/null清除、无变化不写入、回滚/重开；instructions进入真实请求且保留资源提示，tools/modelTools控制有序模型声明与实际执行入口。
- [x] 扩展快照接通实际请求/工具链：稳定名称默认/显式/add/remove选择、工具和提示段同名覆盖保序、同步wrapTool/wrapSection、改名/错误移除并报告、标签渲染及请求内固定选择；未改动的基础工具仍走注入提供者。
- [x] beforeTool/afterTool进入真实Tool阶段：意图前校验/参数链/首个block、最终参数checkpoint、结果替换链与元数据、pending/取消/重开、异常处理；实际DeepSeek写入路径由hook改写，原assistant记录保持。
- [x] Generation四阶段hooks进入产品：beforeRequest临时消息链、afterResponse/afterTools观察、onYield首个续接；phase固定选择、挂起/取消、回答与续接/队列边界原子提交。新请求保存offeredTools并随并行/串行子任务传递，未提供的工具在效果前拒绝。
- [x] 可变注册表接入实际消费者：同名替换保序、完整候选验证后发布、安装/撤销及独立所有者引用；运行中phase固定快照，旧快照归还后释放，Host关闭等待外借快照。
- [x] beforeCompact接入同一扩展选择/注册表：首个decline/summary、挂起/取消、阶段快照及替换后下一任务采用新hook；摘要进入原有Task/Submission持久放置链。
- [x] 自定义Task接入可变注册表和实际Scheduler：全安装集合按稳定名字解析、跨扩展/内置冲突拒绝、phase边界接手、撤销或不兼容候选保留旧invocation；调度器重开后缺定义等待安装，迁移失败身份缓存及替换后恢复，任务图同步blocked原因，代码引用在清理后释放。
- [x] 同步section失败保留已展示文本；section/tool声明按上游计划持久pangu.system：无变化不写、值/增删最小patch、顺序重排、head后的完整基线/旧delta omit，工具变化附最后一条；与request checkpoint/cutoff同事务提交，新计划计入压缩阈值。
- [x] 异步section接入Generation prepare：固定输入/扩展快照的挂起续接、包装后渲染、取消先清理回调再释放引用；挂起期间不写SystemEntry，调度器重开重新准备，完成后复用原有提示计划和原子提交。入队/压缩只读同步配置，避免触发挂起renderer。
- [x] 工具按Task保持扩展快照、声明gate、hook与执行租约；扩展父工具挂起时，owned子工具可执行并回传结果，父子取消分别释放。基础提供者与共享异步上下文保持互斥；产品ASan/UBSan及真实DeepSeek扩展工作流通过。
- [x] 原生工具执行回调接通executeTool的start/poll：调用级实例工厂、显式键/自动序号、madeNestedCalls memo、持久owned子工具与脱离存储的结构化返回；真实write→read、重开复用两条子任务、结果隔离及级联取消通过。实例close后dispose，未绑定工厂拒绝执行。
- [x] 嵌套工具Live槽接入持久创建/意图/结果事务：pending→running→done、调用/父调用/任务身份、最终arguments与isError摘要，父工具结算清除直属子槽；原生观察读取与safe重开/取消通过。
- [x] 嵌套工具开始事件改为已提交execute意图，执行前取消只结束；开始参数取最终checkpoint。OUTPUT先提交Live的4096字节UTF-8尾窗口/droppedBytes再发布，done清除临时进度；Task输入保存parentCallId。真实父子工具、两段输出/多字节截断、重开与取消通过。
- [x] executeTool按父工具replay策略登记abandonOnRestart；Scheduler重开先标记放弃，派生取消保留既有意图边界并向普通后代传递restart。已执行/未执行工具分别返回interrupted/abandoned诊断，safe重跑先清除旧Live进度；实际父子孙任务及无副作用重开通过。
- [x] 中断/abort结果读取已提交Live输出与元数据后再结算清理；实际3000字节输出跨重开保留。缺定义的restart任务等待安装并执行abort，普通取消可使缺定义任务以orphaned结束；Client取消清除restart原因，派生取消识别显式祖先意图。
- [x] executeTool在缺少存储结果时按真实Task outcome返回faulted/orphaned及具体原因，客户端也发布失败结束。模型工具父调用接通结算清理，Live按完整后代树移除并保留无关分支；注入终态记录、实际write/read和跨级清理通过。
- [x] 工具执行API接通details/diagnostic持久进度：输入复制、details null、诊断形状校验、快照观察、最终结果fallback/诊断顺序与中断恢复。模型/嵌套工具共用Live槽及进度提交，Generation响应结算清理上一轮模型工具槽；模型直接调用和子调用专项通过。
- [x] 输出预算采用Chord完整行/UTF-8 head/tail算法，现有4096字节缓冲接通2000行进度和最终结果预算；累计droppedBytes/droppedLines、最终截断诊断和中断恢复诊断进入实际工具链。2408组固定上游源码对照、持续输出及最终/中断专项通过。
- [x] 工具定义携带固定output_limits并随选择/wrap/phase快照保留；默认50KiB/2000行/head，bash显式tail；Tool缓冲按预算分配，配置70KiB和零预算、旧4KiB-tail恢复/取消及非法配置拒绝通过。完整工具/注册表/扩展/模型声明布局同步迁移。
- [x] Chord流文本清洗接入Live窗口和输出事件，保留制表/换行及普通Unicode；诊断渲染按实际长度分配，7000字节诊断进入最终模型文本。单调时钟durationMs写入结果/嵌套返回/摘要，2464组固定上游对照和产品场景通过。
- [x] 原生流buffer保留清洗前原始margin、累计字节/换行与末尾状态；head在窗口固定后只计数，tail快照保留上游所需的前一字符/换行。Tool已原位替换旧窗口拼接；220组逐块快照直接对照、实际大输出/清洗/恢复/取消通过。
- [x] 工具调用API接入output、retainedOutput和outputWindow：报告与异步OUTPUT共用调用级buffer/节流/事件，owned快照含尚未刷新的输出，tail接受OutputSkip并保留全流计数；head拒绝skip，执行前/释放后拒绝访问。固定Chord 260组多块快照对照、实际工具/中断/清洗/跳过输出、原生ABI和真实模型工作流通过。最终OK/ERROR可显式省略output并采用报告窗口，显式空输出保持独立；afterTool改写文本时去掉原窗口截断提示。工具返回后撤销执行API，duration在返回时固定，200ms后置hook等待不计入执行时间。省略输出且窗口为空时保存空Content；嵌套默认程序值在诊断渲染之外由实际输出确定。失败Live摘要优先拼接error诊断，否则取输出，最多500 UTF-16单元并保留原生UTF-8字符边界；成功或空错误省略error字段。
- [x] ToolControl接入真实Generation工具轮次：模型工具终态receipt保存control，Generation按调用顺序归并addTools、最后一个handoff与全体terminate；配置增量、上下文reset和队列边界同事务提交。数组/排除表/未设置选择、去重、终止true/false/混合与取消结果、最后handoff和终止后不建后继通过；两后端工具选择回滚/重开、真实DeepSeek写文件后直接结束通过。
- [x] Live文本更新采用追加或有界重叠后的裁头/追加，操作量供Tool进度按原生Op payload计费；details/diagnostic确认提交后更新同一输出暂停期限。模型/嵌套槽的10KB追加、Unicode尾窗口、无变化零提交、计量和原有暂停/取消场景通过，保持同步元数据API。
- [x] 结构化工具结果接入固定调用声明与外部Draft-7强类型校验：bash声明、嵌套合法/缺失/不匹配结果、错误文本诊断与模型非致命报告进入真实链；DSO ABI同步更新，产品ASan/UBSan及组合/$ref/regex/数组/null专项通过。完整TypeBox差异和定义前置准入见交接区。
- [x] afterTool多内容块结果接入预算、Entry、嵌套返回和错误摘要：text/image数组无截断保序，整体text裁切时只合并保留位置的文本并保留图片；程序值不含渲染诊断，空/单text/单image/多块、head/tail与错误摘要产品ASan/UBSan场景通过。
- [ ] 实现 extension注册/选择、工具增减、section/wrapSection、wrapTool、generation/tool/compaction hooks和自定义task。
- [ ] 将本地原生插件装载、更新、依赖变化与宿主作用域生命周期连接；状态保存稳定名字/数据，运行时解析服务。
- [ ] 验收：两个原生扩展可组合、替换、撤销并正确清理；提交中的配置变化、工具声明与实际可执行集合一致；hook错误/取消与上游顺序对应。

### K10 — 输入队列、多会话与分叉

来源：Durable `harness/{harness,submissions,inbox}.ts`、`session/forks.ts`；`harness-inbox`、`harness-host-inputs`、`harness-conversations`、`harness-nested-tools*`、`session-forks` 测试。

- [x] 文本Submission接入真实产品：requestId去重、输入/写入admission、状态轮询/撤回、steer/follow-up的one/all边界、忙时reject、展示写入、reset/stale、失败keep/continue/withdraw及持久结算。
- [x] 根/独立/分叉会话通过统一Session创建入口原子提交；创建hook的Entry和Agent文档同批保存，已有根会话不重复初始化，hook失败整体撤销。产品根启动接入；SQLite/JSONL分叉历史与文档继承、隔离及重开通过。
- [x] 前台空闲选择会话保留共享Scheduler，失败保持旧绑定；当前会话历史经同一canonical投影读取并直接绑定TUI历史行，父/分叉内容隔离、两后端重开通过。
- [x] Harness创建服务接入Live/Inbox/Usage/Provider/Agent默认文档、独立UUIDv7身份、分叉点/任务owner配置继承及同事务agent/init；产品conversation.client支持独立/分叉创建后选择，真实DeepSeek完成各自对话与分叉写文件，两后端回滚/重开通过。
- [x] conversation.catalog提供升降序游标分页、owner筛选、记录查询和名称读写；Coding Agent式元数据Entry规范CR/LF与Unicode首尾空白，名称变化进入会话观察，分叉继承/清空隔离、跨页读取与两后端重开通过。
- [x] conversation.actions.reset接入真实Submission/队列边界，空上下文和显式handoff消息共用head重置；公开接口保留Submission ID用于状态查询，忙时reset/后续队列顺序及两后端专项通过。
- [x] 任务能力接口通过runner受保护事务创建/分叉会话，复用Harness默认文档/配置/init；产品Host真实任务验证owner归属、分叉配置、init失败整体回滚与过期能力拒绝。
- [x] task access通过runner受保护事务向子会话提交Submission，确认后返回receipt，复用现有requestId去重、Entry/任务准入与状态查询。
- [x] task.control与TUI可选入口支持指定任务持久取消；重复取消不重复提交，父取消沿owned子会话级联，调度器重开后子Generation aborted与父终态收束通过产品Host专项。
- [x] 产品独立seed/resume进程验证owned子会话与Generation持久恢复：直接退出后重读JSONL、保存配置进入真实DeepSeek请求、父等待完成；真实子模型流已开始时持久父取消并退出，新进程恢复后子Generation aborted且不再发模型请求。
- [ ] 完成 requestId去重、输入/写入submission、wait/settlement、steer/follow-up队列与忙时策略。
- [x] 会话Agent的ModelRef与thinkingLevel进入实际准备配置：provider/modelId映射modelProvider/model，思考级别映射reasoningEffort，off清除参数；配置准入拒绝错误ModelRef/空身份/NUL/未知级别且保留原配置，恢复默认配置、实际请求编码及产品Host专项通过。
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
- [x] 摘要输入移除4096字节复制边界：任务持有完整JSON文本，通过model.provider ABI3的显式长度借用行进入同一模型编码器；1 MiB/NUL/页尾编码及19,301字节真实请求、两后端恢复与产品Host继续对话通过。
- [x] 自动阻塞压缩接入Generation：按模型容量、最近有效usage与尾部估算选择切点，原子创建owned子任务并等待；摘要Entry与子任务终态同批提交，父任务重开后继续生成。产品读取合并compaction设置和可选model.metadata；真实模型两后端阶段重开与产品Host闭环通过。
- [x] 溢出恢复接入Generation：沿pi-ai错误分类和Durable当前策略，原子记录失败assistant/owned压缩/父等待；摘要成功后保持attempt继续请求，保留compacted防重复，失败或无摘要保留原溢出原因。真实模型摘要/恢复及两后端阶段重开通过。
- [x] 后台阈值压缩接通Generation/Client：同事务检查已有压缩并创建conversation-owned后台任务，前台请求独立推进；前台空闲/取消隔离后台，空闲poll继续调度，摘要经Submission放置。真实模型两后端去重/Esc/两次重开及实际产品Host通过。
- [x] Generation按固定cutoff持有动态canonical context，经model.provider ABI4的PG_CONTEXT_VIEW进入同一OpenAI编码器；模型请求解除64行/单文本4096字节复制边界，system/section、工具配对和动态启动历史共用投影。两后端长文本/重开、70条未压缩消息真实产品、模型工具与deferred恢复通过。
- [x] Generation正文改为动态printbuf累积，Live追加和最终assistant同源；摘要流式累积、持久summary与包装后的Entry去除4096字节边界。真实产品超过12 KiB回答和超过14 KiB摘要放置通过，旧内联正文数组原地删除。
- [x] 压缩Live生命周期接入创建/阶段/终态事务：taskId/reason/blocking/attempt列表、取消/完成移除、失败保留compactionFailure，成功放置清除旧失败；同任务阶段更新保持列表顺序。两后端创建同序列、重开/取消/失败与真实自动/后台压缩通过。
- [x] 压缩失败按当前retry策略进入持久retry/sleep：保留范围和请求配置、指数退避/上限/预算、提前唤醒重睡、attempt递增及Live.retry更新/清除。两后端预算耗尽专项及一次503→退避重开→真实摘要/放置/回忆通过。
- [x] 后台摘要延迟放置与新摘要竞争：真实摘要保存后挂起，后续范围的新摘要先提交；旧摘要重开后经同一Submission路径结算stale、不追加Entry且保持新head。SQLite/JSONL真实摘要/回忆及原子写入检查通过；手动与自动任务共用一个构造函数。
- [x] beforeCompact接入原生Host可替换compaction.hooks服务：按序回调、首个决定、错误报告后跳过、decline/summary、协作挂起/续接与取消；固定旧范围/配置的hook摘要进入同一place/Submission链。两后端专项与产品Host插件替换通过。
- [x] 压缩模型与预算对齐Durable：固定当前Agent模型/配置，摘要预算独立于普通对话maxTokens；无usage阈值基线沿上游消息估算。双后端真实摘要、重开、放置及回忆通过。
- [x] Generation-owned阻塞压缩与后台摘要竞争：阈值创建owned任务、父Generation等待/恢复，较新摘要落库后旧后台摘要重开结算stale，保持新head与真实回忆；SQLite/JSONL通过。
- [ ] 完成 reset/compaction/fork后的上下文投影、token估算、保留边界、自动/手动压缩、后台压缩、摘要模型与hook。
- [ ] 压缩任务接入Scheduler/Storage，运行中的取消、失败、工具调用配对与重开保持一致。
- [ ] 验收：超过当前有界会话规模的长工作流可通过压缩持续运行；原始记录、摘要与模型可见上下文能从存储重构；压缩中断后恢复。

### K13 — 观察、Usage与任务图

来源：Durable `harness/{view,events,task-graph,usage,telemetry}.ts`、`session/`；`harness-view`、`harness-events`、`harness-task-graph`、`session-watches`、`spec-usage` 测试。

- [x] UsageDoc接入Generation响应、工具结果可选usage和摘要尝试的同批事务；按模型/工具累计计数及已报告cost，可选reasoning/cacheWrite1h保留，Host usage服务读取PanGu会话账本；两后端累计/回滚/重开与真实摘要/生成用量通过。
- [x] 按 Durable `Harness.usage()` / `addUsageState` 接入跨会话 `usage.total`：汇总各会话自身已提交模型/工具账本，通过 Host 与 TUI 公共接口读取；产品 ASan/UBSan 专项及 Assembly、登记检查通过。
- [x] Live/usage只读快照提供Host observation服务与可选TUI绑定：无调度/写入、缺省空状态、压缩终态、存储重开累计用量及解绑边界通过。
- [x] Host/TUI observation提供拉取式状态观察：初始快照、已提交Live/usage变化合并、无变化跳过、返回值隔离及幂等退订；两后端用量事务与实际压缩开始/结束通过。
- [x] Durable Client在提交发布后按账本实际变化排队PG_USAGE_CHANGED；缓存读取不访问Storage，动态事件节点保留完整JSON，退订/暂停释放基线。真实摘要/生成用量事件及16 KiB队列所有权通过。
- [x] Client按已采纳Live中taskId差异发布压缩开始/结束：结束→用量→开始，失败匹配对应error；正常/空范围/连接失败/取消均一对通知，TUI执行适配保持兼容。
- [x] Generation重试/deferred状态与Task checkpoint同事务写Live，事件覆盖retry开始/结束、pollAt变化与faulted/orphaned任务失败；两后端ASan/UBSan、真实重试/工具/回答及deferred重开通过。
- [x] Host task.graph与TUI可选绑定提供只读活任务图：五种live状态分页、父子owner/等待/睡眠/completing投影、所属会话分页、注册元数据阻塞分类、终态移除与重开查询；读取占用Session只读事务。
- [x] 任务图多观察者支持attach初始帧、独立读取/合并变化、注册元数据刷新、幂等退订与Host统一关闭；迁移失败按定义身份缓存，替换后恢复，图读取不执行迁移。
- [x] conversation.view接通会话记录、原始有效Entry范围、已实现Live/Usage文档与排队Submission视图；Host/TUI公开只读入口同一readOnLine读取，分页、撤回、压缩head及重开通过。
- [x] conversation.view多观察者接通初始历史、后续Entry/文档/队列变化、合并读取与Host关闭释放；任务图/会话视图共用一份原生Watch生命周期，真实摘要/回答观察通过。
- [x] Client接通message_start/message_end/entry_appended原生事件：首文本前开始、流式assistant提交不重复开始、普通消息与无模型Entry边界、同批ID顺序和会话过滤；真实重试/工具/后续回答配对通过。
- [x] Submission提交记录接入Client事件：按ID排序、会话过滤、排队/放置/撤回/结算字段直接取PanGu记录，位于任务结束之后、用量之前；产品排序夹具与真实follow-up流程验证。
- [x] Generation回合开始/结束接入提交事件，completing提前结束后terminal去重，按结束→Submission/用量→开始排列；流式消息以Entry.byTaskId关联，其他任务提交不结束当前消息。
- [x] Client队列刷新接通启动分页基线与Submission实际提交：入队/撤回/结算后发布完整ID有序队列，内容不变跳过，暂停释放；重开恢复大队列与真实follow-up事件通过。
- [x] conversation.history提供fork-aware Entry范围/游标分页，默认最新优先、升降序续读与包含边界；与会话目录复用同一原生分页/所有权路径，产品Host及TUI可选入口接通。
- [ ] 完成只读snapshot、watch/退订、增量消息/工具/任务事件、会话视图、任务图、usage/cost与report/trace上下文。
- [ ] 为TUI提供公开查询/操作函数表，覆盖会话/模型选择、历史分页、队列、压缩、任务和配置状态。
- [ ] 验收：观察者初始快照与增量衔接正确，退订/关闭释放；多次重开保留累计用量，TUI显示来自实际Runtime事实。

### K14 — 完成本地 Agent 应用组合

来源：Durable `Harness` 与 Coding Agent `core/agent-session.ts` 的模型、资源、会话操作；现有 `src/composition/pangu.c`。

- [x] 产品前台接入Durable Client/Session/Scheduler/Generation/Tool：默认`.pangu` JSONL、模型/工具事件、取消、暂停与已提交历史。核心专项与真实模型编码工作流通过；TUI候选整合见交接区。
- [x] 基础启动参数接入--session-dir/--session-id、help/version及工作区；Assembly解析后直接打开选定会话，配置/历史与恢复任务使用同一Runtime，目录参数优先于环境设置。
- [ ] 将 K02–K13 的服务组合成实际程序：工作区、模型/凭据、资源、存储、宿主与TUI桥接；完善启动参数、配置加载与关闭顺序。
- [ ] 对接TUI TODO的所有操作请求，按已实现头文件逐项联调；完成会话选择、导入/导出、模型/思考配置与扩展刷新所需Runtime服务。
- [ ] 验收：从干净临时工作区启动，真实模型完成编码工作流、会话管理、压缩、扩展调用、暂停/恢复，操作数据逐项可核对。

### K15 — 完整移植核对

- [ ] 逐行核对覆盖表和对应入口中的public行为，检查实现、原生差异和实际验证；把发现的缺口补回所属K步骤并完成。
- [ ] 运行受影响全部模块检查及产品构建，配合TUI的U08执行最终用户工作流。
- [ ] 与 U02/U08 核对实际终端截图/录屏和可见帧回放：独立输入/会话/状态、流输出、原位工具展开/收起、阅读焦点和面板、resize，无提示符堆积；完整主界面通过后再验收以下真实服务流程。
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

- 来源：Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283` 的 `api/{anthropic-messages,transform-messages,simple-options,constrained-sampling}.ts` 与 streaming-tool-arguments、anthropic-sse-parsing、strict-tool-fallback 用例。
- 实际调用链：Messages已接入原生模型服务、Generation/Tool和PanGu存储。`mimo_projection.S`处理文本/图片/工具与配置，`mimo_stream.S`处理动态块、最终参数、签名和用量，`mimo.S`处理传输、beta与会话亲和头；`request_context.S`按模型身份重放历史。公开model ABI保持。OpenAI编码器共用PanGu sessionId生成上游缓存键与保留配置；Chat成功消息保存原始停止原因，content_filter/network_error等失败携带明确错误及原始原因。
- 验证：模型O0/O2/UBSan、ABI、curl入口协议回放、构造/插入失败清理通过；Chat原始停止/错误与固定Pi raw-stop用例一致；SQLite/JSONL验证流解析→Entry提交→重开→后继请求。本次集成`build_test.py`全套、产品构建及`test_product_resources.py --sanitize --live-agent`通过，真实DeepSeek完成声明收窄、工具写入及后继回答；Assembly与登记快照G00通过。Anthropic使用协议回放。
- 具体缺口：Messages的managed effort、constrainedSampling声明/严格转换/失败回退、native tools、OAuth和网关完整覆盖，以及后续协议。现有`pg_tool_definition`及提示目录只传名称/说明/schema；严格模式需将声明元数据、插件快照、模型请求和恢复一起贯通。
- 下一交付物：按K05继续成组完成提供者行为；严格工具从上述声明调用链实施，其他协议按覆盖表顺序移植。接口或构建调整随同一批集成。
