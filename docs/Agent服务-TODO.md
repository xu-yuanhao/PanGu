# Agent 模型、工具与插件服务：执行任务

你负责 PanGu Agent 的模型、工具、资源和插件宿主。先读根文档与 AGENTS.md，采用 Pi 的现有能力和 Harness/Cordis 的组合语义。服务用于通用 Agent，通过可选择的组合提供编码等能力。

**文件归属**：`include/pangu/services/`、`src/services/`、`tests/services/`、`examples/services/` 和本 TODO；依赖接入交给集成负责人，产品会话接线交给运行负责人。

首轮连续交付 S01～S03 的可运行宿主、DeepSeek 和工具组合，随后按协议族或扩展行为完成其余组。专项入口：`python3 tools/integration/check.py services`，接入 `pangu_check_services` 目标与 `services` 标签。

## 执行清单

- [ ] **S01 插件宿主与公共服务。** 阅读 Harness `docs/architecture.zh.md`、`packages/core/{scope,tools,agent,agent-loop,session,system-prompt}/` 和 Cordis `packages/core/src/{context,registry,fiber,service,events}.ts`、对应测试。交付上下文、服务函数表、依赖、作用域、事件、注册/释放、错误清理和 C 插件描述符；提供运行层可消费的服务查询接口。验收：默认插件与外部插件走同一注册路径，两个提供者可替换同一能力；Core 驱动和持久执行驱动按组合安装，卸载撤销其注册并安全处理在途调用。
- [x] **S02 真模型与工具最小组合。** 对照 Pi pi-ai 的 DeepSeek 提供者、流式事件、工具 Schema 及参数处理，使用 libcurl 接入真实 DeepSeek；工具从 Core 接口注入。完成文本/思考/工具增量、usage、结束原因、HTTP 错误与取消。验收：在服务专项及运行层调用中真实完成工具往返，凭据只通过环境注入，协议记录脱敏。把可运行接口及时交接 R02。
- [ ] **S03 原生工具与执行环境。** 成组移植 Durable `src/tools/`、`src/env/` 和 Coding Agent `src/core/tools/` 的 read/write/edit、bash/PowerShell、grep/find/ls、截断/溢出、图片、差异展示数据、路径与文件修改顺序。使用 libuv 和成熟搜索/Unicode/图片组件提供环境能力；将能力及工具注册为可选插件。验收：工具行为对照来源用例，三桌面平台运行含空格/Unicode 的路径、进程树取消、并发修改及大输出；移动宿主能够注入允许的执行能力。
- [ ] **S04 模型与认证覆盖。** 核对 pi-ai `src/index.ts`、`models.ts`、`models-store.ts`、`providers/`、`auth/`、`utils/` 及相关测试；按协议族组织提供者插件，成组移植模型目录与动态提供者、请求选项、跨模型内容转换、思考/图片/结构化内容、图像生成/分类、用量成本、代理、重试和凭据解析。移植 Coding Agent 模型解析、自定义配置与虚拟模型的实际行为。认证包含 API key、环境/配置解析与来源采用的 OAuth 刷新。验收：每个采用快照的提供者工厂与公开能力映射到实现和协议用例；缺少账号的路径使用确定性录制/协议测试，真模型结果单独记录。
- [ ] **S05 配置与提示资源。** 对照 Coding Agent `settings-*`、`resource-loader.ts`、`skills.ts`、`prompt-templates.ts`、`system-prompt.ts`、`auth-storage.ts`、`project-trust.ts` 及实际调用者，移植用户/项目设置、覆盖优先级、模型选择、项目上下文/记忆文件、skills、模板、资源发现、信任、提示片段和重载。配置、认证和提示资源通过插件注册，公共能力给运行层和 TUI 使用。验收：PanGu `.pangu` 数据能写入、读取与恢复，模型请求的身份/提示词正确；资源增删与设置变更在实际会话生效。
- [ ] **S06 扩展组合与生命周期。** 对照 Durable `harness/{define,registry,prompt}.ts`、Coding Agent `src/core/extensions/`、`package-manager.ts` 与 Harness 配置/组合入口，完成具名扩展、tools/sections/hooks/wrappers/tasks、加载配置、发现、安装/移除、启停、重载、依赖变化与错误回滚；面向用户提供 TUI 可调用的扩展管理接口，分发包匹配平台/架构和 ABI，安装时校验元数据及来源。原生扩展通过版本化 C ABI 注册，静态与动态加载共用生命周期。验收：外部示例扩展可以贡献工具/提示/命令/任务，替换模型、存储或执行驱动；关闭/卸载后资源和事件监听正确释放。
- [ ] **S07 MCP、Code Mode 与工具发现。** 移植 Pi `packages/mcp/`、`packages/codemode/`，Durable `src/codemode/`、`src/tool-search/` 和 Coding Agent 的对应入口；MCP 随默认组合提供，支持参考的连接、认证、工具发现/调用、取消、输出及错误映射；Code Mode 与工具发现模块按配置组合。Code Mode 采用成熟 JS 引擎，调用走同一工具钩子和取消；持久调用通过运行层注入的回调接入检查点。验收：实际 MCP 服务和 Code Mode 调用工具，嵌套调用在重开后按来源恢复，正常与错误结果可供模型/TUI 消费。
- [ ] **S08 服务覆盖、多平台与资源验收。** 核对以上参考入口的导出、配置与关联测试，将具体能力归入 S01～S07，补齐缺口。三桌面平台运行模型协议、工具、资源与插件加载/卸载专项；Android/iOS 构建并验证宿主注入与静态插件组合。测空闲连接、工具并发、重载和多实例后的资源释放，向集成负责人交接实际依赖、许可、结果和差异。验收：默认能力由插件提供且可替换，停用后撤销注册；可选插件能独立安装/启用，其依赖按组合链接或加载。全部服务能被产品真实消费，Core 独立构建保持最小依赖。

## 开始与反馈

S01 的公共接口先交接；S02、S03 优先接通可用产品。协议族和相邻调用者作为一个实现批次。依赖选择在实际消费者处完成，版本及许可交给集成负责人统一固定。测试按服务目标选择，重型构建与整机回归集中执行。

## 当前能力与执行顺序

- 已有：插件宿主/lease/作用域、DeepSeek、文件/命令/搜索工具、配置/skills/模板、原生动态加载、MCP stdio/HTTP；实现入口见 `include/pangu/services/` 与 `examples/services/README.md`。
- 待闭合：S03 文件顺序、路径/图片来源差异及 Windows 执行；S04 多模型与认证/OAuth；S05 资源发现/设置完整语义；S06 扩展发现/安装/配置/依赖生命周期；S07 Code Mode 与剩余协议行为。每组直接接产品消费。
- S01 宿主先按现有 Core 驱动与真实插件验收；Durable 驱动组合由 R03 集成时验收。功能完成与平台验证分别记录，所有完整要求仍由 S08 汇总。
- 顺序：先完成当前 ABI/Schema 跨层收尾，随后闭合默认资源/扩展/MCP 用户流程与多模型协议组；运行驱动接线由运行所有者处理。可选 Code Mode 作为完整模块交付。

## 当前交接

- 当前行为组：C04 消息 presence 的 Services 动态 ABI 配套（2026-11-19）ready；仅本角色归属，未扩展 Schema 全范围或 OAuth。
- 来源：父 C04 约定在 `pangu_message` 尾部追加 `uint32_t presence`（六个 bits），不改变 `pangu_tool`；原生加载继续沿用既有 Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283` extensions/loader.ts 工厂入口与 Cordis `df4431eb43086bd1f8263a0e8c03e9b7c5233b68` scope 生命周期原生适配。实际检查入口为 `src/services/dynamic.c:pg_dynamic_plugin_load` 的 descriptor 校验，先于 factory。
- 实现：`include/pangu/services/dynamic.h` 的 `PG_DYNAMIC_ABI` 从3升4，记录旧 message 布局不可传入当前消费者、tool布局不变、ABI1/2/3均需重编；`tests/services/dynamic-bad.c` 将旧版夹具固定为字面量3（不随宏漂移），factory保留abort；`tests/services/dynamic.c` 验证 PG_ABI/descriptor阶段/out=NULL，并检查清理计数和原静态服务绑定未变。ABI4正常MODULE仍通过服务lease→实际Core工具调用→卸载。`examples/services/README.md` 更新版本、重编和presence使用边界；无新字段、无服务消息生产逻辑变化、无新增依赖。
- 只读消息审查：`deepseek.c` 的 stream_state/request_events 只生成 assistant并填usage，普通assistant usage必有无需HAS_USAGE，thinking由Core final标；`tools.c` 文件/图片、`execution.c` 命令/搜索、`approval.c` 拒绝/取消、`mcp-client.c` stdio/HTTP共用结果均未主动生成usage，故不把零初始化冒充显式全零用量。审批允许路径原样转发sink，aggregate只复制pangu_tool而非message，resources/schema不复制或生成消息，无presence丢失点。外部native示例工具亦无usage。若今后某能力明确输出全零tool-result usage，应标Core HAS_USAGE；非零保持兼容推断。本次各能力均无实际新增标记需要。
- 验证：Linux x86-64，独立Debug `build/services`；`cmake --build build/services --target pangu_services_dynamic_test pangu_services_dynamic_example pangu_services_schema_test pangu_services_tools_test -j2` 成功；`ctest --test-dir build/services -R '^services_(dynamic(_example)?|schema(_source)?|tools)$' --output-on-failure` 5/5通过（动态拒绝/成功调用与示例、Schema及既有来源差分、文件工具消费者）。只跑此快速组，未跑all/product-asan/真模型/终端，多平台未验证。测试时父Core公共头尚无message presence，不能据此声称已验证追加后的Core布局；本次ABI3预factory拒绝与ABI4消费者已实际通过。
- 差异/阻断：本服务子交付无阻断；安装消费者旧 fixture 当前为 `.abi=2`，同样属于必须拒绝的旧 ABI；根 deps 说明已同步 ABI4，本角色未改Core/Agent/TUI/root/integration/references。S06全组、Schema全范围与OAuth不因此勾选。
- 下一交付：父落C04消息presence后集中接线/集成；服务后续完整行为组另领，本轮不扩展范围，无后台构建/测试。
