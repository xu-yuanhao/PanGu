# 给并行 Agent：完善 TUI 输入编辑与安全粘贴

状态：待开始，2026-10-26。工作目录 `/home/xu-yuanhao/Documents/PanGu`。本任务是当前有效派发，旧 MiMo 重构任务已结束，不要重新执行。

你是 PanGu 的并行实现 Agent。请直接实现下面的输入体验，实际构建、验证后交接，不停留在方案或文档。主 Agent 同时推进 Runtime、read 分页和工具语义；你们共用工作树，必须遵守文件归属，不能清理或提交对方的更改。

## 目标与范围

把目前只能行尾编辑、吞掉方向键的薄输入框，完善为可日常使用的原生输入框：能移动光标修改文字，能安全粘贴多行内容。保持界面简单，不重写另一套 TUI。

先读根 AGENTS.md、项目总文档、工程守卫、迁移 TODO 近期队列，以及 `docs/TUI体验参考与设计原则.md`。主要 Agent 基座是 Pi Durable，TUI 简洁性参考 Pi，交互另参考 Grok Build/Codex；技术栈固定为手写 Assembly 第一、C 第二、C++ 第三。

## 文件归属：这是并行工作的硬边界

你可以修改以下现有文件；只改实现本任务需要的部分：

- `src/client/tui/tui_input.S`、`tui_input.h`。
- `src/client/tui/tui_view.S`、`tui_view.h`。
- `src/client/tui/tui_app.S`、`tui_app.h`。
- `src/client/tui/tui_term.S`、`tui_term.h`；`tui_term.c` 仅在确需调整 termios/ioctl 等平台胶水时修改，不能承载输入或渲染算法。
- `src/client/tui/tui_layout.h` 中上述类型/枚举的布局和静态断言；不得修改 state、Runtime adapter 或 bridge 的契约/布局。
- `tests/client/tui/test_tui_input.c`、`test_tui_view.c`、`test_tui_abi.c`、`demo_main.c` 及 `test_util.h` 的相关测试辅助代码。
- `tools/client/tui/build.sh`：只扩充本任务实际需要的测试/构建，不改全局产品构建。
- 本文末尾的“交付记录”：用作唯一简短交接记录，不另建一套报告。

其他文件均只读，尤其禁止修改：

- `src/runtime/`、`src/contracts/`、`src/composition/`，包括 read/Tools/Agent 的正在开发的更改。
- `src/client/tui/runtime_bridge*`、`tui_runtime_adapter.h`、`tui_state*`、`tui_utf8*`。
- `tools/runtime/`、`tests/runtime/`、其他 TUI 测试及共享登记/门禁文件。
- 根 AGENTS.md、项目总文档、主 TODO、`governance/`、`references/`、Git/CI 配置。

默认在现有模块和测试里完成，不新增源文件、依赖或模块。确实必须越界时，先在交付记录写明具体文件、最小接口需求与理由，继续完成不依赖它的部分；不得自行改共享文件。主 Agent 负责最终登记、全局集成与真实模型复验。不要把同一文件复制成另一套实现来绕开归属。

开工先保存 `git status --short` 和 HEAD，区分已有脏文件；不要 stash/reset/checkout/restore/clean 他人的改动，不创建分支或 worktree，不执行 git add/commit/push。主 Agent 最后统一审查提交。

## 先读实际参考，再实现

阅读以下本地源码的对应函数、调用者与测试，记录实际 SHA、文件/函数、采用行为和差异；具体文件不存在时用 rg 定位，不凭名称猜测已读过。

- `references/pi/packages/tui/src/components/editor.ts`：输入处理、光标、插入/删除、粘贴及渲染；阅读对应 editor/键盘/粘贴测试和调用入口。
- `references/pi/packages/tui/src/` 的键盘解析与显示宽度实现：理解分片转义序列、bracketed paste、终端字符宽度，不只看 README。
- `references/grok-build/crates/codegen/xai-grok-pager/src/views/prompt_widget/mod.rs`、`tests.rs`：输入与可见光标/换行的关系；只取本任务需要的 UX。
- 如需补充，再读 `references/codex/codex-rs/tui/src/bottom_pane/` 的输入区与相关测试。Claude-Code 快照不是官方发布源码，不复制其代码。

参考源码不修改、不参与产品构建；不要为阅读安装整套上游运行时。学习行为与边界，用盘古自己的技术栈实现，不照抄 TS/Rust 的类型或组件体系。

## 必须实现的行为

1. **行内编辑**：Left/Right 在 UTF-8 code point 边界移动；Home/End 到当前逻辑行首尾；Backspace 删除前一个字符，Delete 删除后一个字符；普通输入在光标处插入。不能拆坏 UTF-8、越过边界或漏掉末尾字节。现有 Ctrl-U 清草稿行为保留。
2. **多行草稿**：Enter 只在普通输入模式请求提交；提供 Alt+Enter 插入换行，并在简短输入提示中说明。向 Runtime 提交实际文本和显式长度，不把换行展平成空格。普通 Enter 与 CRLF 不得触发两次提交。
3. **安全 bracketed paste**：启用终端 bracketed paste 模式，识别 `ESC[200~` / `ESC[201~`；开始/结束标记允许在任意读取边界拆分。粘贴中的 CRLF/CR 规范化为 LF，保留多行内容，期间不产生提交、取消、退出或光标控制动作。控制字节安全过滤；未知 ESC 不得直接写到终端执行。未完成粘贴不能提交半截内容，明确规定 Ctrl-C/退出的收尾规则并测试。
4. **容量**：继续使用现有 `TUI_INPUT_MAX_BYTES=1024`，有效文本不超过 1023 字节，不扩大预算。超限按完整字符拒绝并提示；即使溢出也继续消费到粘贴结束标记，不能把剩余粘贴误当快捷键或命令。测试粘贴结束后可以继续正常编辑与提交。
5. **显示与真实光标一致**：支持行内光标、多行、窄终端和自动换行。CJK/emoji 的列宽与现有 UTF-8 helper 保持一致；当前未提供完整字素簇编辑，不伪称已实现。resize、工具/模型流式输出后草稿字节和逻辑光标不丢失，屏幕光标仍对应编辑位置，不残留旧输入行。
6. **生命周期**：正常退出、Ctrl-C、EOF/信号及失败退出都恢复已启用的终端模式，包括 bracketed paste。忙时取消、持久退出暂停等 Runtime 语义保持原样；TUI 不能自行补造运行终态。

若未声明 bracketed paste 的普通终端输入到来，保留现有普通按键语义；不要声称能可靠识别所有未括号的多行粘贴。未知转义序列安全消费，不能吞掉其后的正常文本。

## 明确不做

不做会话选择、消息历史滚动/搜索、输入历史持久化、自动补全、Vim、主题/鼠标系统、完整 Markdown、工具折叠或全屏重构。不要增加 Runtime 队列、模型重试、会话状态或另一套命令行。也不要顺手整理所有文档、格式化整树或修改门禁。

输入状态机、UTF-8 编辑、按键分派、换行与渲染逻辑均手写 `.S`；C 只用于必要平台结构胶水及测试。禁止编译器生成汇编、汇编壳调用 C 核心、保留新旧双实现。优先复用已有 UTF-8/显示宽度 helper，不自写第二套 Unicode 库，不引入依赖。注释只解释必要 ABI、状态、所有权与边界。

## 怎么验证

- 先运行 `bash tools/client/tui/build.sh run-tests`，保留实际退出结果，既有失败不得删用例或放宽断言掩盖。
- 在现有测试中验证：空/首尾/中间编辑；中文和 emoji；1023 字节边界；Home/End/Delete；多行及 CRLF；粘贴标记所有切分点；粘贴内容中的 Enter/Ctrl-C/Ctrl-D/ESC；超限后的结束标记与正常下一次输入；未完成标记、重复标记、未知 CSI/SS3；resize 和流式重绘保持草稿/光标。
- 更新受影响结构的 sizeof/offsetof 与汇编常量断言。使用现有 ABI/保护页测试检查保存寄存器、栈对齐、输入输出边界；不要只让 C 测试通过而遗漏实际汇编路径。
- 进行真实 PTY 输入回放：可在现有 `build.sh` 中加入小型 Python 测试入口，驱动实际编译出的 TUI 演示程序。检查提交字节与次数、取消/退出次数、窄窗口/resize、终端属性和 bracketed paste 开关的恢复；离线输入回放不能写成真实模型验证。
- 本模块测试确有必要时运行 ASan/UBSan；它们主要覆盖 C/库边界，汇编内存访问仍靠保护页/长度与 ABI 检查。不要仅以 sanitizer 无报错宣称汇编安全。
- 执行 `python3 -B tools/gates/check.py`；出现共享树中的主 Agent 未完成项，如实区分，不擅自修门禁或触碰他人文件。真实 MiMo/完整产品集成由主 Agent 收尾，不索取或记录凭据。

## 完成交接

做完即停在可审查状态，在下面逐项填写，不提交整个共享工作区：

- [ ] 参考：实际 SHA、函数/测试、采用行为与原生差异。
- [ ] 实现：上述六项逐项说明完成部分，未完成的保持未勾选。
- [ ] 文件：只列你修改的路径，以及结构/符号变化；注明没有新增依赖。
- [ ] 验证：实际命令、退出码、PTY 观察和失败修复；没有执行的不能记通过。
- [ ] 风险：尤其是字素簇、未括号粘贴、窄窗口/终端差异与取消语义。
- [ ] 主 Agent 集成事项：最小登记差异及真正需要其处理的接口；无需求写“无”。

## 交付记录

待并行 Agent 开始后填写。
