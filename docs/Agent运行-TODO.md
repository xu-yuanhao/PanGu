# Agent 运行与持久化：执行任务

你负责 PanGu Agent 的会话、持久执行、产品操作和应用生命周期。先读根文档与 AGENTS.md；默认用户工作流参考 Pi Coding Agent；持久执行插件参考 Pi Durable，服务组合参考 Harness/Cordis。按完整行为组连续完成实现、接线和验证。

**文件归属**：`include/pangu/agent/`、`src/agent/`、`tests/agent/`、`examples/agent/` 和本 TODO。Core 与服务接口由其所有者维护。

首轮连续交付 R01/R02 的保存重开与真实会话，再接 R05 的默认会话树/压缩与 R06 的组合操作；R03/R04 交付可选持久执行插件。专项入口：`python3 tools/integration/check.py agent`，接入 `pangu_check_agent` 目标与 `agent` 标签。

## 执行清单

- [x] **R01 命令/事件契约与存储重开。** 阅读 Durable `src/types.ts`、`entries.ts`、`documents.ts`、`session/`、`storage/` 和 `packages/chord/` 的实际事务调用。交付会话创建/打开、提交、观察、取消与关闭的公开接口，提供 TUI 可编译的订阅示例；交付默认 JSONL 存储插件及原子提交、只读、损坏诊断和重开；Memory 与 SQLite 的完整存储实现随 R03 的持久执行插件接入。验收：独立专项运行存储一致性场景，已提交消息和文档重开后保持一致，未提交状态不对外可见。
- [ ] **R02 通用 Agent 可用闭环。** 对照 Coding Agent `src/core/{sdk,agent-session,model-runtime}.ts` 将 Core 驱动注册为执行插件；从配置加载随产品提供的默认插件组合，通过服务层选择模型、工具、提示资源，支持输入、流式事件、取消、排队、保存与重开。提供 `src/agent/app.c` 的应用生命周期和 TUI 启动接线，与 TUI 负责人共同运行。验收：DeepSeek 真模型使用注入的通用工具完成一轮任务，用户能在 TUI 中看到结果，重开保持会话。
- [ ] **R03 Durable 持久执行插件。** 成组移植 Durable `src/tasks.ts`、`harness/{scheduler,generation,tool,submissions,inbox,live,provider}.ts`：输入接纳、requestId 去重、Generation/Tool 阶段、检查点、队列、并发、嵌套调用、输出提交与订阅。通过宿主注册可选择的持久执行驱动，复用 Core/服务层公共执行能力，持久调度遵循 Durable 的阶段语义。验收：在接纳、模型、工具及结果提交边界终止进程并重开，对照生成、工具恢复用例验证；外部副作用的重试行为按来源表达。
- [ ] **R04 任务图与恢复控制。** 对照 Durable 任务及 `harness/task-graph.ts` 在持久执行插件中提供父子任务、子会话执行原语、等待、睡眠/唤醒、重试/退避、取消传播、关闭、崩溃计数、park/retry、缺失定义与版本迁移、错误报告和用量。验收：任务关系和阻断原因可查询，重开保持检查点，取消等待与取消任务的行为区分正确；单个任务失败后的队列策略符合来源。
- [ ] **R05 会话树与压缩插件。** 对照 Durable `harness/{harness,context,prompt,view,compaction,events,usage}.ts`、`session/forks.ts`，以及 Coding Agent `session-manager.ts`、`agent-session.ts`、`compaction/`，以默认插件提供多会话、树形导航、分支/切换、reset/handoff、摘要压缩、手动/自动压缩、重试、模型切换、会话属性与用量统计。验收：在 TUI 及独立宿主可执行这些操作，重开后上下文、分支、队列和费用保持一致。
- [ ] **R06 插件运行接线与产品操作。** 将服务层提供的注册表、环境、提示片段、工具包装、hooks、任务定义及 reload 接入真实会话；统一用户命令、审批请求/答复、工具展示信息和任务观察。对照 Coding Agent SDK、会话操作、扩展事件，Durable registry 与 Harness，以及 Harness agent-loop 生命周期。验收：运行中更换组合、重新加载资源和关闭时在途工作/事件的顺序正确，工具与提示词实际请求可核对。
- [ ] **R07 行为覆盖与平台验收。** 逐项核对 Durable README 的公共能力及 `src/index.ts` 导出、`test/` 与 Coding Agent 会话/压缩相关测试，将各项归入 R01～R06 并补齐具体缺口。移植存储、环境和恢复的一致性用例。桌面三平台实跑会话、工具取消与恢复，移动库使用宿主存储/执行能力运行嵌入场景。验收：会话产品操作、持久任务、Core 驱动和 TUI 接线完整，资源随实例关闭释放。

## 开始与反馈

先交付 R01 的接口；存储专项可以独立推进。R02 接通默认可用产品，R05 的会话树与压缩随默认组合交付；R03/R04 的持久执行组合和 R06 的公共接线按各自接口并入同一产品。确定性模型/工具及可控时钟运行恢复测试，真模型验证完整调用链。向集成负责人交接 CMake 目标、依赖和受影响场景。

当前顺序：完成 C04 presence 跨层收尾后，成组闭合 R02/R05/R06 的默认配置、模型切换、会话树/压缩、扩展与 MCP 实际用户流程，再交付 R03/R04。单个字段和存储编解码变化在消费组内完成；平台结果单列到 R07。

## 当前交接

- 当前行为组：R01/R02 配合 C04 的消息 presence 真正保存/重开；入口为 `pg_session_append/commit/open`，JSONL 与 SQLite 两后端。此次子组完成，不改变 R02/R05/R06 整项验收状态。
- 来源：Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283`（MIT），阅读 `packages/ai/src/types.ts` 的 System/User/Assistant/ToolResult 消息联合及 optional 字段，`packages/coding-agent/src/core/session-manager.ts:parseSessionEntries/_persist` 的 JSON 保存/重开。原生沿用 PanGu version 1 内部 `data` 格式、原子提交与严格损坏诊断，不改参考文件。
- 接口/版本：直接消费父方 `pangu_message.presence` 尾成员及 six flags，未新增运行侧 ABI/依赖。新 writer 每条消息必写 uint32 presence（含 0），以成员存在作为新格式 marker，无需升级外层 session version。必须与此次 Core 头/库一起重新编译。
- 实现：`src/agent/message.c:pg_encode_message/pg_decode_message` 服务实际 session、tree、持久队列等现有调用者。指定 content signature/tool_namespace/arguments_error 与 message response_model/response_id/raw_stop_reason/provider_thinking_level/error，NULL 省略、非 NULL 零长度保存空串；新记录按 key 缺省/空串还原，非法字段类型报 CORRUPT。旧无 presence 记录的 optional 空串归 absent，assistant 加 HAS_THINKING_LEVEL，toolResult 加 HAS_USAGE，保持原全字段投影。其余 required 内部字段和 `data` 不变。presence 非整数/负数/超 uint32 拒绝，角色位校验沿 Core append；编码分配失败及 Core NOMEM 沿原错误路径传播。
- 实际消费者/回归：新增 `tests/agent/presence.c`、`agent_presence` 与 `pangu_check_agent` 依赖。JSONL/SQLite 12 类实际消息保存、重开与 Core `pangu_proxy_request_json` 字节比较，覆盖 presence=0、OFF/零 usage 显式存在、非 OFF/非零 usage 兼容推断、string content（0/1 text）、空文本与空数组、system 三个空容器、指定 optional 缺省与显式空串。原 `agent_rich` 保留非空/二进制内容覆盖。旧格式通过真实存储写入、open、commit 迁移、再次 open 并比对 proxy JSON；旧 content/message 空 optional 均覆盖。NOMEM 注入 yyjson encoder 与 Core decode 深拷贝（无半条消息且解除失败后重试成功），负数/布尔/溢出 marker、optional null 与非法角色位返回 CORRUPT。
- 验证：Linux x86-64/GCC Debug，`python3 tools/integration/check.py agent` 构建成功，**17/17，17.69s**。随后仅增强新用例旧格式真实迁移与失败注入，`cmake --build build/agent --target pangu_agent_presence_test -j 2` 成功；`ctest --test-dir build/agent -R '^agent_presence$' --output-on-failure` **1/1，0.13s**。其余执行源码不变，复用已通过结果。未运行 all、真模型、ASan、安装或其他平台。
- 差异/阻断：无接口阻断。低层旧记录未存的信息无法恢复：旧 optional 空串不能区分原缺省/显式空，旧 system 空容器存在性与 user/system string-vs-array 形状不能推断；按旧投影兼容。旧 assistant OFF 和 toolResult 全零 usage 使用固定兼容位，不宣称还原来源原始存在性。无新增依赖，仍链接 Core 自带 yyjson、原 SQLite/libuv。
- 下一交付：父方集中执行 C04 全组集成/安装/内存验证；本运行侧子组已完成实际消费者，无 root/Core/services/TUI/references 改动。后续运行工作继续 R06 MCP/OAuth/资源发现接口及产品闭环，不在本次扩展范围。
