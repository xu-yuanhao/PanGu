# TUI：执行任务

你负责 PanGu 的终端交互。先读根文档与 AGENTS.md，以 Pi TUI/Coding Agent 为主要行为来源，结合 Grok Build、Codex、Claude Code 的易用交互。采用 C17，业务逻辑跨平台共享，以呈现插件通过命令/事件接入同一个通用 Agent，默认体验参考 Pi，操作入口随已启用能力呈现。

**文件归属**：`include/pangu/tui/`、`src/tui/`、`tests/tui/`、`examples/tui/` 和本 TODO。运行接口由运行负责人维护，服务/扩展描述由服务负责人维护。

首轮连续交付 U01～U03 的可操作对话界面，接口就绪即接真实 Agent；渲染和输入用例可以独立推进。专项入口：`python3 tools/integration/check.py tui`，接入 `pangu_check_tui` 目标与 `tui` 标签。

## 执行清单

- [ ] **U01 可运行终端与输入输出。** 阅读 Pi `packages/tui/src/{terminal,keys,stdin-buffer,tui,tui-main-screen,tui-alt-screen}.ts` 及测试；建立 CMake 专项、终端进入/恢复、窗口尺寸、输入事件和最小对话界面。共享逻辑与 POSIX/Win32 适配分离。验收：Linux/Windows/macOS 可启动、输入和退出，正常/异常/中断路径恢复终端，重定向与能力不足有可诊断结果。
- [ ] **U02 编辑器与交互反馈。** 成组移植 Pi `components/` 中输入/编辑器、`keybindings.ts`、`undo-stack.ts`、`kill-ring.ts`、`word-navigation.ts`、自动补全和历史相关逻辑。提供多行、粘贴、光标选择、撤销/重做、历史、搜索、命令/路径/资源补全。对照 Grok/Codex 的输入反馈减少操作负担。验收：中文/组合字符/宽字符、跨行编辑、粘贴和终端键盘协议用例通过，输入与模型输出并行稳定。
- [ ] **U03 接通真实 Agent。** 将 TUI 注册为默认呈现插件，消费运行层公共接口完成会话输入、流式文本/思考、工具状态/结果、错误、用量、取消、steer/follow-up 与保存/重开。终端循环和模型/工具执行互不阻塞；与运行负责人接通产品启动入口。验收：用户从 TUI 使用 DeepSeek 完成普通任务和文件工具任务，取消后可继续，关闭重开能看到真实会话结果。
- [ ] **U04 完整显示与浏览。** 成组移植 Pi 布局、差分渲染、Markdown、代码块、表格、列表、图片能力探测、滚动/搜索、复制、折叠、加载状态与覆盖层。结合 `references/grok-build/crates/codegen/` 的 pager/终端实现、`references/codex/codex-rs/tui/src/` 和 `references/Claude-Code/src/components/` 优化工具展示、状态栏与提示。验收：长会话、快速增量、大工具输出和窗口缩放保持正确显示，支持普通与增强终端，重绘量可测。
- [ ] **U05 会话、任务和设置操作。** 对照 Coding Agent `src/modes/interactive/`、会话组件与操作，Durable 的任务图/观察/恢复行为，默认提供会话树与创建/切换/分支/重开、模型及思考设置、压缩、排队输入、工具组合与资源选择、主题/快捷键设置。任务状态/取消/重试和审批答复从对应插件能力接入。验收：每个操作都调用真实运行/服务接口，结果与持久状态一致，用户可以只通过 TUI 完成操作。
- [ ] **U06 扩展交互与易用性。** 对照 Coding Agent 扩展 UI 和组件入口，提供插件发现/安装/移除/启停与配置操作，实现扩展命令、通知、选择/确认/输入、状态/面板及工具自定义展示的原生接口，接入插件生命周期。形成一致的焦点、返回、取消和错误反馈；帮助中可发现实际快捷键和命令。验收：用户通过 TUI 安装并使用一个原生扩展，示例扩展能与用户交互，重载/关闭清理显示与待答复状态；交互无需用户了解内部架构。
- [ ] **U07 覆盖与终端验收。** 核对 Pi TUI `src/index.ts` 的交互能力、Coding Agent 的终端操作及相关测试，将场景归入 U01～U06 并补齐缺口；移植行为而使用原生组件接口。三桌面平台实跑终端测试和完整用户流程，检查输入法、剪贴板、Unicode、粘贴、缩放、中断恢复与文件路径。记录启动、空闲 CPU/内存、流式重绘开销并修复实际问题。验收：与 Core/Agent 联合回归通过，TUI 成为完整可用的产品入口。

## 开始与反馈

U01/U02 可使用本目录内的确定性事件输入开发渲染，U03 接入运行层实际接口。参考用例转换成状态/布局测试，终端协议通过伪终端专项测试；人工验收关注实际键盘和显示体验。快照与断言检查真实行为，交接注明所用终端/系统。

当前顺序：完成当前审批组集成后，优先闭合 U02/U04 的编辑、补全、Markdown、滚动和显示，以及 U05/U06 默认产品操作。已存在的会话/资源/审批接口直接消费；Linux功能与 Windows/macOS验收分别记录到 U07。

## 当前交接

- 当前行为组：U03/U05 默认 app/TUI 显式审批组合。默认策略仍 disabled；宿主在 present 前调用公开 `pg_agent_app_set_approval(app, PG_APP_APPROVAL_ASK)` 即启用。默认呈现插件展示只读 toolName/callId/reason，y 允许一次、n 拒绝、Esc/Ctrl+C 取消，Enter/Tab/粘贴不授权。取消后同一持续会话可以继续输入和执行；审批键不成为 USER 或历史。U03/U05 整项仍不勾选。
- 来源：DeepSeek Harness `22ddee3792402c3e70ec5f86a63c3e3e895a2f60`，`packages/client/ui-approval/src/client/ApprovalPanel.tsx` 的 composer takeover/allowed-once/rejected；`contract/slots.ts:PendingApproval` 的只读字段、answerable、abort/finish 一次性结算；`packages/interaction/user-approval/tests/approval.spec.ts` 的 scope request、asked/decided 配对及撤销失败关闭。原生适配：同步 owner-thread 应答循环，输入 reader 只写加锁的 modal latch，服务 provider ID 与 durable ID 分开；沿用已有 Pi 输入/呈现能力。
- 实现：`src/tui/plugin.c:apply` 在自己的 presentation scope 调用 `pg_approval_provider_register`。`answer_service` 在 durable asked 展示后运行，借用 provider/core/question 仅至返回；owner 读取 modal 选择并通过 `pg_approval_provider_reply` 答复，不调用模型或提交 USER、不把 grant 留到下一请求。等待期间观察 Core cancellation、UI quit/stopped，取消 session 并失败关闭。provider callback 活动时普通 poll 不消费审批，避免误走旧 durable reply 路径；旧 `pg_tui_approval_answer` 独立消费者接口保留。
- 所有权/释放：`stop` 先 revoke provider，再取消/关闭 live durable/modal 请求、join reader 并恢复终端；callback 返回前关闭匹配 modal、清除 answering，无保留的 question/core/provider 指针。scope LIFO cleanup 先执行 presentation cleanup，后释放 provider，避免 revoke 访问已释放 provider。宿主必须等同步 action/answer 返回再 dispose，沿公开 app/services 生命周期。已停呈现不重新打开审批。`include/pangu/tui/plugin.h` 与 `examples/tui/README.md` 记录显式组合、线程、借用与 quiesce 要求。
- 实际调用者：`tests/tui/session.c` 的 approval-jsonl/approval-sqlite 模式通过公开 app setter 启用 ASK，然后走默认 `pg_agent_app_present(...pg_tui_plugin...)`。确定性模型生成普通工具调用；工具只向实际 `.effects` 文件追加内容，不自行 request/answer，也不自行判断授权。门控完全由 app/services wrapper 与呈现 provider 完成。未改 Sagan 运行源码或复制 prepare 包装。
- PTY：`tests/tui/approval.py` 每后端 create + 三次 open，共 8 个真实 PTY 进程。允许一次确有文件追加，拒绝/Ctrl+C/Esc/stop 均无追加；相同 callId 每次新询问，重开不复用授权。取消后立即继续同一 session；Esc 完成后迟到 y 只留作草稿并清掉，下一请求仍需明确选择。每后端断言 7 个唯一 asked ID 与 7 个配对 decided，结果依次 allowed-once/rejected/cancelled/allowed-once/cancelled/rejected/cancelled；审批键与 UI 文本不在 USER，重开搜索审批字段无匹配、浏览不改变保存快照。询问期间 stop 由 owner-thread COMMITTED listener 调用真实 view.stop，验证 revoke/取消、进程正常退出、termios 与启动前完全相同。模态 draft/预存队列隔离继续由已有 editor 用例验证。
- 验证：Linux x86-64，复用 `build/product-asan` Debug/ASan/UBSan。`cmake --build build/product-asan --target pangu_check_tui --parallel 2` 成功；`ctest --test-dir build/product-asan -L tui --output-on-failure` **9/9**（15.57s，含 approval 1.62s，既有 keys/buffer/editor/history/terminal/session/session_history/resources 全过）。随后仅加强 Python 审计断言，再跑 `ctest --test-dir build/product-asan -R '^tui_approval$' --output-on-failure` **1/1**（1.59s）。ASan/UBSan 无报告。首次新增 Esc 场景失败因 PTY 同包 Esc+y 被解析为 Alt+y，改为发独立 Esc、观察取消完成、再发迟到 y；未延长等待。未跑 all、真模型或安装整机；Windows/macOS 未编译或运行。
- 文件：`src/tui/plugin.c`、`include/pangu/tui/plugin.h`、`tests/tui/session.c`、`tests/tui/approval.py`、`examples/tui/README.md`、本 TODO。无新增依赖；继续链接已有 PanGu::services，基础 TUI 闭包不变。仅写 TUI 归属范围；本次开发时共享目录尚无 .git；重装备份时统一初始化仓库，不改变此组源码。
- 差异/阻断：本组无接口阻断，公开 app setter 可在 present 前启用 ASK/NEVER，不需要 presentation 新增策略钩子。产品 CLI 已由运行侧提供 --approval ask/never（见根 README）；本角色未改 app/CLI。运行动态 prepare 工具包装由 Sagan 维护，本轮 TUI PTY 覆盖显式普通工具、未声称覆盖动态工具选择专项。真实模型、其他平台和安装消费者结果沿用各负责人报告，不计作本次验证。
- 下一交付：本组交父集成负责人接集中回归；后续 U02 visual-line/软换行、补全菜单、IME/剪贴板与其余 U04～U07 尚缺，既有原始历史/资源/Tab FIFO 实现与回归保留。
