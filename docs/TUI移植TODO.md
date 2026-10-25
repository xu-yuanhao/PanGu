# 给 TUI Agent：完成原生 Pi Durable 终端客户端

你是 PanGu 的TUI实现Agent。工作目录 `/home/xu-yuanhao/Documents/PanGu`。请按本文顺序开发、验证和勾选，使用**手写 Assembly 第一、C 第二、C++ 第三**。运行时由 [核心 Agent](原生重写路线与迁移TODO.md) 负责，你将其能力做成简单易上手的终端交互。

## 完成目标

用户从终端完成模型配置、连续对话、编码工具、队列、会话/分叉、压缩、扩展、任务恢复及历史/用量查看。会话区、输入区与必要状态清晰；基本任务用键盘完成。Pi提供交互行为基线，Grok Build提供布局/输入体验参考，Codex补充底部面板与状态反馈，Claude Code提供工具摘要等通用体验参考。

## 文件归属与接口

你维护：

- `src/client/tui/` 的输入、UTF-8、状态、视图、应用和终端文件。
- `tests/client/tui/`、`tools/client/tui/`。
- 本文的勾选、来源和交接区。

核心Agent维护 `src/contracts/`、`src/runtime/`、`src/composition/`、`src/client/tui/runtime_bridge*`、`src/client/tui/tui_runtime_adapter.h`、`tools/runtime/`、`tests/runtime/`及`governance/`。既有函数表字段、枚举值与尺寸在并行批次内固定；新增Runtime能力通过核心提供的独立公开接口消费。TUI从这两个公开接口入口消费Runtime：`src/contracts/client.h`、`src/client/tui/tui_runtime_adapter.h`。

新增源文件在交接区列出路径和职责，由核心登记。需要新Runtime操作时，在交接区写清操作、参数、结果/事件和使用步骤；核心提供入口后接入。接口未到位期间继续可独立验证的输入、渲染、控件和状态映射。产品会话与执行状态由Runtime提供，测试数据在客户端测试中注入。

保留共享树中他人的更改。你完成后更新本交接区，核心统一核对和集成提交。

## 每一步怎么做

1. 阅读下面对应参考函数、调用者与测试；记录实际SHA和采用的用户行为。
2. 沿现有TUI文件原地实现；输入/布局/绘制/交互算法为手写汇编，termios/ioctl等平台结构使用现有薄胶水。
3. 专项验证输入字节、可见状态、焦点和光标、事件映射与资源释放。每步以可实际操作的用户任务结束。
4. 运行 `bash tools/client/tui/build.sh run-tests` 和相关PTY回放，满足整步验收后勾选；全程序/真实模型集成由核心统一执行。

## 固定参考与行为映射

固定快照：Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283`；Grok Build `2bdd1d6a6369de0e8c68132ea4539e9abd9e14a8`；Codex `8dd6099e265f14b80cb1b3fcf10c29a12bb6b23d`；Claude-Code `b78dd22a091b717c8938ab98c736bc04825a8ee8`。每批阅读核对实际提交，采用的更新记入交接区。

| 用户行为 | 参考源码入口 | 原生落点 |
|---|---|---|
| 输入、光标、多行、粘贴、历史、补全 | Pi `packages/tui/src/components/editor.ts`、键盘/宽度实现及 `test/`；Grok `crates/codegen/xai-grok-pager/src/views/prompt_widget/{mod.rs,tests.rs}` | `tui_input*`、`tui_utf8*`、`tui_layout.h` |
| 布局、重绘、光标、终端清理 | Pi `packages/tui/src/tui.ts`、`terminal.ts`；Grok pager视图；Codex `codex-rs/tui/src/tui.rs` | `tui_view*`、`tui_term*`、`tui_app*` |
| Markdown、工具、diff、图片、状态 | Pi `packages/tui/src/components/`、Coding Agent `modes/interactive/components/` | 视图/组件模块、`tui_state*` |
| 历史浏览、输入区、选择/确认控件 | Grok pager视图；Codex `codex-rs/tui/src/bottom_pane/`及相关snapshots | 视图/控件模块、`tui_app*` |
| 模型/会话/压缩/资源命令 | Pi Coding Agent `modes/interactive/interactive-mode.ts`、`core/slash-commands.ts` | 命令/选择模块，通过Runtime操作接口执行 |
| 持久快照、事件、队列、任务 | Pi Durable `harness/{view,events,task-graph}.ts` | `tui_state*`和只读Runtime接口 |

Claude-Code快照位于 `references/Claude-Code/`，源码为社区还原；采用通用交互方式，代码实现依据Pi/Grok/Codex的可核对源码。

## 实施顺序

### U01 — 输入编辑与安全粘贴

- [ ] 接续当前工作区的输入候选实现，完成UTF-8边界上的行内插入/删除、Left/Right、Home/End、Backspace/Delete、清草稿。
- [ ] 完成多行编辑、Alt+Enter换行、普通Enter提交、CRLF去重、bracketed paste标记任意切分、控制字节过滤、超限后继续消费到结束标记。
- [ ] 验收：中文/emoji、首尾/中间编辑、1023有效字节边界、粘贴中的Enter/Ctrl-C/Ctrl-D/ESC、未完成标记与结束后正常编辑，均在实际输入/PTY路径验证。

### U02 — 稳定布局、渲染与终端生命周期

- [ ] 会话、输入与状态区布局清晰，光标位置跟随实际草稿；支持多行、窄终端、自动折行和resize。
- [ ] 流式输出/工具输出更新时保留草稿、逻辑光标和焦点；使用现有Unicode宽度能力完成中文/emoji显示。
- [ ] 验收：正常退出、EOF、信号和失败路径恢复termios/光标/粘贴模式；长输出、resize和流式重绘PTY检查通过。

### U03 — 消息、工具与变更展示

- [ ] 移植Pi的必要Markdown行为：段落、标题、列表、引用、代码块、链接/表格和流式未闭合块；沿成熟解析/宽度能力组织渲染。
- [ ] 完成工具执行/输出/错误/取消展示、参数和结果的渐进展开、edit diff/patch与首个变更行；显示图片内容对应的终端呈现或可访问附件信息。
- [ ] 验收：模型文本与工具原文正确呈现；控制转义安全；长行、未完成UTF-8/Markdown片段、错误与展开状态经快照和PTY核对。

### U04 — 历史、草稿与常用控件

- [ ] 完成会话历史浏览/滚动、回到最新、展开折叠、输入历史、草稿保存、路径/命令补全和帮助提示。
- [ ] 实现复用的选择、搜索、确认、文本输入控件；焦点与返回操作保持草稿和阅读位置。
- [ ] 验收：向上阅读时新的流输出保持阅读位置；返回输入可继续修改；窄窗口和resize后选择/焦点正确；键盘可完成全部基本操作。

### U05 — 接入完整 Agent 操作

对应核心K10–K14；从已提供的公开Runtime入口接入。

- [ ] 完成模型、thinking、登录/退出认证、配置、项目资源和扩展刷新控件；对应Pi的`/model`、`/thinking`、`/login`、`/logout`、`/settings`、`/reload`。
- [ ] 完成新会话、选择/恢复、命名、fork/clone/tree、导入/导出、手动压缩；对应`/new`、`/resume`、`/name`、`/fork`、`/clone`、`/tree`、`/import`、`/export`、`/compact`。
- [ ] 完成会话信息、复制、帮助/热键与退出入口，对应`/session`、`/copy`、`/hotkeys`、`/quit`。每个命令显示真实成功/失败结果。
- [ ] 验收：从界面完成上述操作，重开核对实际模型/会话/资源状态；无效输入与取消不丢草稿。

### U06 — 运行中的队列、任务与恢复

对应核心K06/K07/K10/K13。

- [ ] 完成steer/follow-up输入、队列查看/撤回，区分取消当前运行、暂停退出和仅关闭控件。
- [ ] 接入持久snapshot与增量事件，展示子任务/工具进度、等待/恢复/失败状态、usage/cost；订阅在页面/会话切换时正确释放。
- [ ] 验收：工具运行中排队、取消、退出、重开恢复；任务与会话切换后历史和终态一致，累计用量来自Runtime。

### U07 — 易用性与终端兼容收尾

- [ ] 整理首屏提示、默认按键、错误和帮助，用户能直接找到输入、当前状态和下一步操作。
- [ ] 完成不同窗口宽高、CJK/emoji/组合字符、长会话与大工具输出、键盘/可用鼠标能力的实际操作检查。
- [ ] 验收：统一回放首次启动、输入修正、多行粘贴、编码工具、阅读历史、选择控件、错误与退出；记录并修复实际闪烁、跳动、焦点或字符宽度问题。

### U08 — 完整 TUI 移植核对

- [ ] 逐行核对上面的行为映射，每一项具有实际源码、实现和通过用例，补齐所属U步骤的缺口。
- [ ] 运行客户端专项与实际PTY场景，向核心交接文件、ABI/接口需求和结果。
- [ ] 与核心K15联调：真实模型配置→项目资源→连续对话→读改写运行→队列→压缩→会话/分叉→扩展与任务→中断恢复→历史/用量→退出。核对文件、存储、界面和终端模式。
- [ ] U01–U08全部验收，本文交接记录完整，作为最终原生Pi Durable TUI的一部分合入。

## 当前起点

已有薄TUI具备输入/流式回答/工具状态/取消/退出、终端恢复与持久历史接入。当前共享树包含U01/U02的候选改动，先检查 `git status --short` 和相关实现，接续验证后完善。

优先完成U01–U04；同时把U05/U06所需Runtime能力写入下面请求表，核心可按其归属并行补齐。

## TUI交接区

- 当前步骤：U01。
- 参考提交、函数/测试：待本批填写。
- 修改文件、接口/布局变化：待填写。
- 实际命令、退出码与PTY观察：待填写。
- 已完成行为/下一行为：待填写。

| 请求 | 操作、参数与结果/事件 | 对应U步骤 | 核心处理结果 |
|---|---|---|---|
| 当前接口 | submit/cancel/poll/history/shutdown，见 `tui_runtime_adapter.h` | U01–U04 | 已提供 |
| 会话操作 | list/open/new/name/fork/clone/reset/import/export及历史分页 | U05 | 待核心提供 |
| 模型与资源 | catalog/select/thinking/auth/settings/reload | U05 | 待核心提供 |
| 运行操作 | steer/follow-up/list/withdraw/compact、task/view/watch、usage | U06 | 待核心提供 |

新增文件登记请求：待填写。
