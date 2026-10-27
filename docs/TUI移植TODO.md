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

- [x] 接续当前工作区的输入候选实现，完成UTF-8边界上的行内插入/删除、Left/Right、Home/End、Backspace/Delete、清草稿。
- [x] 完成多行编辑、Alt+Enter换行、普通Enter提交、CRLF去重、bracketed paste标记任意切分、控制字节过滤、超限后继续消费到结束标记。
- [x] 验收：中文/emoji、首尾/中间编辑、1023有效字节边界、粘贴中的Enter/Ctrl-C/Ctrl-D/ESC、未完成标记与结束后正常编辑，均在实际输入/PTY路径验证。
  - 实现：`tui_input.S`（码点边界编辑、逐字节 feed 状态机、分片粘贴匹配）；`tui_input.h` 契约（1023 有效字节、整字符拒绝、无超时解析）。
  - 单元：`test_tui_input.c` 12 组（含 `paste_all_splits` 全拆分点）；ABI/保护页：`test_tui_abi.c`（guard_input_full 按 1023 契约修正）。
  - PTY：`tools/client/tui/test_tui_pty.py` multiline/paste_safety/unfinished_paste_recovery/capacity_limit。

### U02 — 稳定布局、渲染与终端生命周期

- [x] 会话、输入与状态区布局清晰，光标位置跟随实际草稿；支持多行、窄终端、自动折行和resize。
- [x] 流式输出/工具输出更新时保留草稿、逻辑光标和焦点；使用现有Unicode宽度能力完成中文/emoji显示。
- [x] 验收：正常退出、EOF、信号和失败路径恢复termios/光标/粘贴模式；长输出、resize和流式重绘PTY检查通过。
  - 实现：`tui_view.S`（多行重绘、CJK/emoji 宽度表换行、CUU/CUD/CHA 光标定位、editor_break 收束、resize 换列）；`tui_term.S`（进出 raw 模式开关 bracketed paste，修复序列长度 6→8）；`tui_app.S`（SIGWINCH 重绘、完整码点才重绘、未完成输入不提交）。
  - PTY：startup_and_exit_paths/signal_exit_restores_terminal/poll_error_failure_path/streaming_keeps_draft/resize_keeps_draft/narrow_autowrap/capacity_limit（1023 字节长草稿重绘）。
  - 已知限制：宽度表为 code point 粒度近似（非完整字素簇/East Asian Width 全表），组合字符按零宽处理；见 `tui_view.S` 注释。

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

已有薄TUI具备输入/流式回答/工具状态/取消/退出、终端恢复与持久历史接入。U01/U02 候选改动已于本批接续验证并完成（见交接区），含 `tools/client/tui/test_tui_pty.py` 真 PTY 回放。

优先完成U01–U04；同时把U05/U06所需Runtime能力写入下面请求表，核心可按其归属并行补齐。

## TUI交接区

- 当前步骤：U01、U02 已完成并验收；U03 进行中——Markdown 流式渲染（`tui_md`）已落地并通过单元/PTY 验收，工具/变更展示（渐进展开、diff/patch、图片）待做。
- 参考提交、函数/测试（本批实际核对）：
  - Pi `d5b3de2`（与固定快照一致）：`packages/tui/src/components/markdown.ts` 165–180（`trimPartialClosingFences` 流式部分闭栏防闪烁 → `tui_md` 的 `hold` 暂扣机制）、327–328（TAB→3 空格 → `md_validate` `.Lv_tab`）、320–345（流式增量渲染管线）。
  - 行为差异（有意，均记录于 `tui_md.h`）：
    - Pi 整段重排（每 delta 重渲活动块）；本实现滚动回显不可重写，按完整源行独立折行输出。
    - Pi 孤儿 CR 归一为 LF（editor.ts:1145–1149 同源正则）；本实现 `tui_md_text` 直接丢弃 CR（`\r\n` 经 LF 正常成行，孤儿 CR 不成行）。
    - 表格渲染为自绘管道表（box-drawing 边框、单元格 24 列封顶截断加 `…`、过窄整表回退原文）；GFM 分隔行判定含尾管道（本批修复 `.Ldl_sep` 行尾判定）。
  - CJK 折行语义修正：宽字符仅在跨界（起始列+宽 > 终端列）时折行，恰好占满不折——与 ASCII 满行同一规则，对齐真实终端 auto-wrap 行为。
- 修改文件、接口/布局变化：
  - 新增 `tui_sink.h`（输出目的地函数表，视图与渲染共享）、`tui_md.h/.S`（流式行级 Markdown：段落/标题/列表/引用/围栏/分隔线/管道表格/内联样式/链接、转义安全、流式未闭合块与表格缓冲）、`tests/client/tui/test_tui_md.c`（14 组快照，含任意字节拆分等价、行满拆分）。
  - `tui_view.h/.S`：view 内嵌 `tui_md`；TEXT_DELTA 改经 md 流式渲染（行完整才输出；工具/状态事件先 `md_break` 冲出未完行；RUN_FINISHED/终态先 `tui_md_flush`）；busy/submit/limit/cancel 提示前 `md_break`，poll_error/protocol_violation/exit_note 终态前 `md_flush`+carry 冲刷（失败退出不丢已收文本）。
  - `tui_app.S`：主循环栈帧偏移改为从 `tui_layout.h` 结构体尺寸派生（修复 view 内嵌 md 后仍按旧 64 字节预留、`tui_view_init` 越界打穿调用者栈帧清空 `cfg.ops` 的启动崩溃）。
  - `tui_md.S` 本批修复四处：`.Ldl_sep` 尾管道判定、`.Lpl_cand` 的 `CAND_LEN` qword 加载带上相邻 `TABLE_ON` 致钳制写坏 `TABLE_LENS`（dword 零扩展修正）、`md_render_table` 三处边框循环在 `call` 后从被覆盖的 `rax` 存回列计数器（先存后调）、`md_row_line` 的 `colw` 指针 32 位截断加载、`tui_md_text` 行满拆分 `k` 与 `md_cell_emit` 的 `cons/w` 跨调用使用 caller-saved 寄存器（改栈槽）+ 回扫缺 `LINE` 偏移、`md_row_parse` 计数上限与存储上限一致（8）。
  - 测试更新至 md 流语义：`test_tui_view.c`（TEXT_DELTA 缓冲至行完整/事件收束、CR 丢弃、拆分 UTF-8 由 md 行缓冲拼合）、`test_tui_abi.c`（guard_view_text 同步）；`test_tui_md.c` 修正三处与实现/终端语义矛盾的期望（CJK 恰满不折、表格 `…` 需真超 24 列封顶才触发、行满拆分计数 511+89=600）。
- 实际命令、退出码与PTY观察：
  - `bash tools/client/tui/build.sh run-tests` → 7 个测试程序全部 all passed，退出码 0。
  - `python3 -B tools/client/tui/test_tui_pty.py` → 10 场景 all passed，连跑 3 次稳定；`poll_error` 场景现核对"失败退出前已收文本可见"（本批新增 `md_flush` 收束），退出码 2、不补造终态、termios 精确还原。
  - `python3 -B tools/gates/assembly_first.py` → PASS。
  - `python3 -B tools/gates/check.py` → G00 FAIL 仅因本批 4 个新文件未登记（见下登记请求）；其余通过。
- 已完成行为/下一行为：U03 第一项（Markdown 必要行为）完成；剩余：工具执行/输出/错误/取消的渐进展开、edit diff/patch 与首个变更行、图片的终端呈现或附件信息（对应 Pi `components/` 工具组件与 Coding Agent `modes/interactive/components/`），完成后整步验收勾选 U03。

新增文件登记请求（请核心登记到 `governance/bootstrap-policy.json`）：
- `src/client/tui/tui_sink.h` — 输出目的地函数表（视图与渲染模块共享的 sink 契约）。
- `src/client/tui/tui_md.h` / `src/client/tui/tui_md.S` — 流式行级 Markdown 渲染（纯状态无 I/O，经 sink 输出；产品构建已加入 `tools/client/tui/build.sh` 的 ASM_SRC 与各测试链接）。
- `tests/client/tui/test_tui_md.c` — md 快照/拆分等价/行满拆分测试。

| 请求 | 操作、参数与结果/事件 | 对应U步骤 | 核心处理结果 |
|---|---|---|---|
| 当前接口 | submit/cancel/poll/history/shutdown，见 `tui_runtime_adapter.h` | U01–U04 | 已提供 |
| 会话操作 | list/open/new/name/fork/clone/reset/import/export及历史分页 | U05 | 待核心提供 |
| 模型与资源 | catalog/select/thinking/auth/settings/reload | U05 | 待核心提供 |
| 运行操作 | steer/follow-up/list/withdraw/compact、task/view/watch、usage | U06 | 待核心提供 |

既有登记请求：
- `tools/client/tui/test_tui_pty.py` — TUI PTY 回放工具（已登记，本批 G00 不再报该文件）。

### 2026-10-27 接续验证记录（U03 未整步验收）

- 来源：`references/pi` HEAD 为固定 `d5b3de22d0e24a3e9c38a9410d3329b099717283`；核对 `tool-execution.ts` 预览/展开及 `diff.ts` 的解析、分组和行内高亮。无参考目录修改。
- 实际调用链：`tui_view_event` → `tui_tool_started/output/finished`，输入 Ctrl-O → `tui_view_tool_expand`。前十源行预览、尾部保留和展开已接入。`tui_safe` 为视图/工具共享安全输出。`tui_diff` 和图片 note 目前仅独立入口，尚无真实事件消费者。
- 修复：安全输出长度栈槽重叠；tool 初始化公开头与五参数 Assembly ABI 不一致；started 缺右括号；finished 查找 ID 的错误栈偏移；尾部淘汰 lossy 标记；ANSI 提示行计数跨回调丢失；diff 输出参数搬运覆盖数据指针、SGR 跨调用指针保存、rbp 保存、行内长度跨 sink 回调保存及非法 diff 行退回上下文。diff 保留输入内部空行。
- 测试维护：tool/diff 已加入统一 run-tests；修正测试语料自身栈越界、错误字节长度、隐藏尾行与返回值期望；diff 用符合上游格式的行号夹具，按实际行内高亮核对；删除不符合无状态完整块接口的任意分片调用等价测试。diff sink 主动破坏 r9/r10/r11，验证长度跨回调保存；state/view/PTY 同步真实 ANSI 工具格式。
- 验证：`bash tools/client/tui/build.sh run-tests` 全部九程序退出 0；`python3 -B tools/client/tui/test_tui_pty.py` 十场景退出 0；`python3 -B tools/gates/assembly_first.py` PASS；G00 仍报告下列八个候选文件未登记。未运行整机/真实模型验收。
- 取消边界：已核对 Pi `interactive-mode.ts` 的 tool_execution_start/update/end 与 agent_end；原生 `RUN_FINISHED` → `tui_tool_flush` 冲出仍缓冲的工具尾行，超预览尾部给出 Ctrl-O 提示并可展开；不补造工具成功/失败事件或状态。新增取消场景覆盖半个 UTF-8、ESC 转义、超十行尾部展开及重复 flush 无输出；客户端九程序与 PTY 十场景再次退出 0。
- 工具原文空行修复：再次对照 Pi `tool-execution.ts:createResultFallback`（output.split/newline join），修复 `.Lo_nl` 使用“非空尾行冲刷”而吞掉显式空行的问题。显式 LF 总是提交一条源行，完成/取消冲刷仍只提交非空缓冲；前导、连续、尾部空行同时进入预览计数/隐藏尾部。`tt_line_flush` 调 `tt_line_emit` 前补齐 SysV 栈对齐。新增任意字节拆分输出等价及隐藏空行展开用例。九客户端程序与十六组 PTY 退出 0，Assembly PASS，所属文件 diff 空白检查通过。未以本项修复替代 U03 剩余结构化事件和全文行为验收。
- 运行中工具展开：对照 Pi `ToolExecutionComponent.setExpanded/updateResult` 的进行中展示，Ctrl-O 优先选择最近活动 pending 调用，否则回退最近完成调用。可在输出到来前启用；隐藏尾部输出后清理保留缓冲并同步 shown 计数，后续完整源行直接输出，完成时不再重复提示已显示内容。新增进行中展开、预先展开、后续流输出、重复操作和完成提示专项；九客户端程序退出 0，既有十六组 PTY 与 Assembly 检查通过。现有滚动回显仍不支持上游全局双向折叠，不据此勾选完整展开行为。
- 工具身份与实际桥接对齐：`runtime_bridge.h` 的 id/name 各 64 字节，原 tool 各 48 字节把有效 48–63 字节 ID 截断，`tt_find` 又以全长查找，造成后续输出丢失。tool 自有字段改为各 64，保留桥接内完整身份；record 4776、tool 19144、view 24960 字节，公开结构与 Assembly 常量逐项断言，app 自动派生帧随布局更新。两个共享前 62 字节的最大长度 ID 专项核对输出/终态/展开分别关联。GCC/Clang 九客户端程序、十六组 PTY、Assembly 检查通过；产品重新构建及既有离线持久目录/恢复/终端专项通过。此上限来自现有桥接消费者，不新增 Runtime 能力。
- 全部保留工具展开：对照 Pi `interactive-mode.ts:setToolsExpanded` 4553–4568 遍历组件的行为，Ctrl-O 现遍历当前四个保留槽，对完成且有隐藏行/进行中的调用统一展开；运行中后续行继续直出。共享单槽实现，不改布局和 Runtime 接口；新增完成/运行中两个调用同时展开、重复无输出及后续行用例。GCC/Clang 九客户端程序通过。仍缺全局双向折叠与新调用继承展开偏好，滚动回显不支持撤回已打印行，U03/U04 未整步验收。
- 剩余与下一交付物：U03 参数展示、结构化工具取消结果、diff/首个变更行及图片附件的真实事件接线；diff 当前公共前后缀算法仍不等同上游 diffWords，长行存在有界截断，不能据独立测试宣称完整移植。U04–U08 保持未完成。

当前新增文件登记请求（核心归属）：
- `src/client/tui/tui_safe.h`、`src/client/tui/tui_safe.S`：共享安全输出。
- `src/client/tui/tui_tool.h`、`src/client/tui/tui_tool.S`：工具预览/尾部/展开状态机。
- `src/client/tui/tui_diff.h`、`src/client/tui/tui_diff.S`：diff 渲染候选。
- `tests/client/tui/test_tui_tool.c`、`tests/client/tui/test_tui_diff.c`：对应专项。

接口请求补充：工具 started 需要有界参数摘要；finished 需要结构化 diff、首个变更行、图片 MIME/尺寸及可访问附件路径；run 取消时需要明确仍在执行的调用终态。当前 `tui_event` 仅 name/id/text/detail/tool_ok，不能可靠区分普通输出、结构化 patch 与图片附件。请核心通过独立公开接口提供这些语义，TUI 消费，不推断普通文本为结构化内容。

### U04 当前输入历史与视觉导航交付（整步仍未完成）

- 来源：Pi 固定 HEAD `d5b3de2` 的 `editor.ts:addToHistory/navigateHistory/exitHistoryBrowsing`（425–500）、箭头分派（925–950）、`keybindings.ts`，以及 `test/editor.test.ts` 的历史去重/100 条上限/恢复草稿/多行历史反向移动用例。本地参考树保持固定快照；已知本地 origin/main 前进 20 提交，不在本批更新。
- 调用链：`tui_app` 仅在 `tui_state_submit` 接受后调用 `tui_input_remember`；BUSY/ERROR 不记录、不清草稿。`tui_input_feed` CSI/SS3 Up/Down → Assembly 视觉行布局与历史导航。保留最近 100 条、ECMAScript String.trim 空白集合裁剪、连续去重；浏览恢复原草稿及字节光标；输入/删除/清空退出浏览。
- 视觉导航：输入与 view 共用迁至 `tui_utf8.S` 的唯一 code-point 宽度实现；view init/resize 同步输入列数。多行/自动折行的 Up/Down 保留期望正文列，短行不丢期望列；首行提示符两个单元格仅参与绘制和折行，不计入正文列。第一行 Up 在正文非行首时先移至起点，再浏览历史；最后行 Down 在历史浏览中返回较新项/草稿。完整字素簇及上游词边界折行仍待对齐。
- 布局与所有权：`tui_input` 104936 字节，内存自有 100 项历史、草稿备份、列数与期望列；布局逐项静态断言。视觉行边界在有界栈帧内临时构建，最多 1024 项，无堆分配；app 栈帧按结构尺寸派生。无 Runtime 函数表或公共契约修改。保护页分配按真实尺寸取整；满 100 项移位、1023 字节窄列导航、remember/方向分派寄存器保存均通过。修复 view init 调用 md/tool 初始化时缺失的 SysV 栈对齐。
- 验证：`bash tools/client/tui/build.sh run-tests` 九程序退出 0；`python3 -B tools/client/tui/test_tui_pty.py` 十二场景退出 0，含实际中文/emoji历史→恢复草稿→原光标插入及六列视觉行 Up→插入→提交；`python3 -B tools/gates/assembly_first.py` PASS。新增全部 ECMAScript trim 字符及三个非空白反例、显式多行/短行/CJK/组合字符、resize 与 view 光标一致性用例。仅假 Runtime PTY，未宣称真实模型集成。
- 当前可用帮助：对照 Pi `interactive-mode.ts` 的 hotkeys 文本生成/调用入口（3225、6849–6917），将当前已实现按键适配成 PanGu 帮助。`tui_input_feed` SS3 F1/CSI 11~ → `TUI_KEY_HELP` → app → `tui_view_help`；不经过 submit，不清草稿、不移动逻辑光标，收束活动 Markdown 行后输出固定帮助并恢复编辑区。首屏提示 F1；粘贴期间不触发。`/hotkeys` 精确命令现已在 Enter→Runtime submit 之前本地分派，与 Pi 3225 的处理后清编辑器行为一致；记入本进程输入历史。F1 保留草稿，命令提交则消费自身。未宣称完整上游热键集合。
- 帮助验证：九个客户端程序、十四组 PTY 场景退出 0（hotkeys 组含闲置、本地命令后普通提交、近似命令、多行文本、活动工具流程五种回放），含 F1 后原位置插入、纯文本与 ANSI 输出、粘贴安全；Assembly 和 G00 PASS。新增帮助例程曾误置于非执行节，调试定位后显式切回 `.text`，上述结果均为修复后运行。无新增文件或 Runtime ABI 更改。`/hotkeys` 不调用 submit；活动运行中查看后原流程仍产生唯一终态，近似与多行内容不被当作命令。
- 当前退出命令交付：核对 Pi `interactive-mode.ts` 3297–3300（精确 `/quit` → 清编辑器 → shutdown）；`tui_app` 在 submit 前精确分派 `/quit`，进入既有 `.Ldone` → shutdown 重试 → exit note → termios/粘贴模式恢复路径。新增内部退出原因 `TUI_EXIT_COMMAND=4`，既有值不变，布局静态断言；demo 与产品消费者均按 POLL_ERROR/负值判断失败，可消费此成功原因。未修改核心文件。
- 退出验证：九客户端程序、十五组 PTY 退出 0，quit 组覆盖闲置、活动运行退出及 `/quitX` 正常提交；活动退出不补造成功终态，正常退出检查沿用精确 termios 恢复断言。Assembly 与 G00 PASS。假 Runtime 不证明持久任务恢复，真实 Runtime shutdown/恢复仍需 U06/U08 联调；未据此勾选 U05。
- 当前命令补全交付：定位 Pi `tui/src/autocomplete.ts` 的命令识别/前缀与替换语义；在既有 `tui_input.S` 内用有界 Assembly 实现当前实际支持的 `/hotkeys`、`/quit` 前缀补全，Tab → 唯一候选替换光标前文本并保留后缀；`/` 多候选 → app → view 显示当前可执行列表并恢复草稿。没有列出尚无消费者的 Runtime 命令。输入结构/Runtime ABI 不变；新增内部 key 枚举静态断言，容量不足整次拒绝，粘贴模式不分派 Tab。
- 补全验证：九客户端程序、十六组 PTY 退出 0；专项覆盖中文后缀、满容量不改原文、未知前缀、已补全重按 Tab 与粘贴安全；PTY 实际执行 `/`+Tab→候选→`h`+Tab→帮助→`/q`+Tab→退出，未发起模型运行。Assembly/G00 PASS。此为当前两个本地命令的前缀适配，尚非上游完整模糊命令/路径补全和选择控件。
- 启动历史输入恢复：对照 Pi `interactive-mode.ts` 3924–3940 的历史 user 文本加入 editor.addToHistory，`tui_state_history` 对已验证的 kind=0 快照调用 `tui_input_history_add`，在借用切片失效前复制；assistant/tool 不加入输入历史，重复加载沿既有 loaded 状态禁止。新 Assembly 入口复用 remember 的裁剪/去重/容量算法，不改当前草稿/光标/解析器；超过 1023 字节、控制字符或非法 UTF-8 的记录不作为可编辑候选，完整历史仍走安全展示路径。此为编辑容量与终端安全的原生适配，不截断原记录。
- 恢复验证：新增借用缓冲每次查询清空的 state 消费者测试，确认最新→较旧顺序、排除 assistant、草稿保留、无 submit/poll 执行；输入专项覆盖中文/emoji、控制字节/过长编码/代理区/越界码点/半字符拒绝及返回草稿。九客户端程序、既有十六组 PTY、Assembly 检查通过。持久存储写入/重开仍由 Runtime 提供，本项未新增客户端存储协议。
- 实际产品联调：`python3 -B tools/runtime/build_pangu.py --output /tmp/pangu-native/pangu` 退出 0；既有 `tools/runtime/test_pangu_pty.py --binary /tmp/pangu-native/pangu` 的默认持久目录/空闲重开/损坏与断尾/终端恢复通过。新增客户端 PTY 可选入口 `PANGU_TUI_PRODUCT=/tmp/pangu-native/pangu python3 -B tools/client/tui/test_tui_pty.py`：真实产品中文/emoji 提示→本地连接拒绝的模型失败→/quit→重开→Up 找回已提交提示→清草稿退出，存储字节不变、重开无 RUN_STARTED，十六组原场景与此产品场景全部通过。该用例验证失败运行的持久用户记录，不宣称成功模型或工具闭环。
- 产品差异修复：产品 Clang 构建的中文输入暴露 `tui_utf8_cp_width` 把宽度表 `.set` 计数解析为绝对地址 6/30，coredump/GDB/objdump 已定位；比较操作改用显式 OFFSET 立即数；随后 Clang 工具专项暴露同类计算字符串长度的立即数歧义，`tui_tool.S` 的对应 mov 同步显式 OFFSET。GCC 客户端与 Clang 客户端各九测试程序通过，产品回放修复后通过。最初本地 HTTP/SSE 夹具因产品仅允许 HTTPS 与独立 CA 信任未通过，已移除该夹具，保留有明确目标的连接拒绝恢复用例；没有放宽产品 TLS。
- 当前命令选择焦点：核对 Pi `tui/src/components/select-list.ts` 的 selectedIndex、方向导航、onSelect/onCancel，再接入现有两条命令的实际候选消费者；`/`+Tab 打开候选，Up/Down/Tab 循环选择，Enter 只填入并关闭，下一次 Enter 才执行。Ctrl-C 只关闭候选，释放后才恢复运行取消/闲置退出语义；普通编辑和粘贴关闭候选，草稿后缀保留。选择状态归 input，增加两个 unsigned 字段，input 104944 字节，静态布局断言与 app 帧同步。视图使用固定安全文案标示选中项；仍为滚动回显，尚无重绘原位候选面板/搜索/通用选项列表。
- 选择验证：GCC/Clang 九客户端程序、十七组 PTY 退出 0；新增 Enter 不误退出、Ctrl-C 不丢草稿、CRLF 不误提交及活动工具运行时 Ctrl-C 关闭候选而不取消 run。Assembly/G00 PASS；产品重新构建与既有离线持久目录/终端专项通过。U04 的完整选择/搜索/确认/文本输入控件与历史阅读锚点仍未验收。
- 输入历史搜索当前接线：核对固定 Codex `8dd6099` 的 `bottom_pane/chat_composer/history_search.rs` 搜索会话/独立查询/预览/接受/取消生命周期，调用者 `bottom_pane/mod.rs` 1035、3068 取消测试。Ctrl-R 先保存原稿与光标，打开空查询，不自动采用最新条目；正文输入区在搜索期间持有独立查询，历史条目仅作为只读预览。文字编辑/粘贴后重新从最新查找；Ctrl-R/Up 较早，Ctrl-S/Down 较新；Enter 将匹配项作为可编辑草稿而不提交；无匹配时 Enter 保持搜索；Ctrl-C/Ctrl-D 恢复原稿，不传播运行取消/退出。预览与空查询/无匹配状态经安全文案展示，查询按既有 UTF-8 编辑状态机处理。
- 搜索验证与剩余：GCC/Clang 九客户端程序与十八组 PTY 通过，覆盖中文独立查询、双向导航、无匹配不提交、粘贴查询、接受后 CRLF 不误提交、原稿恢复后继续编辑；Assembly/G00 PASS。重复结果已按完整文本保留最新一次，双向扫描均跳过较旧重复项；另核对 Codex `chat_composer_history.rs:search_result_is_unique/search_matches`（788–811），现已接入完整字符串 lowercase 后的 memmem，避免用 ASCII 或 casefold 替代参考语义。单 ESC 控件取消已接入；还缺独立底部查询焦点/布局和搜索期间历史分页；目前滚动回显展示预览，不声明完整参考搜索 UI 或 U04 已完成。搜索状态归 input 自有字段，104952 字节，静态布局断言与 app 自动派生帧同步。
- 搜索本轮边界验证：新增 A/B/A/C/B 非相邻重复历史的唯一结果与双向回溯、查询编辑重启、中文/emoji 原稿光标恢复；不完整 UTF-8 时 Ctrl-R 忽略而非触发半字符重绘，字节补齐后可进入。GCC/Clang 九客户端程序、十八组 PTY、Assembly 检查通过；产品重建和既有离线持久目录/终端专项通过。去重仅遍历当前至多 100 条自有历史，无额外分配，不改变 Runtime 分页范围。
- Unicode 大小写搜索：核对参考 `chat_composer_history.rs:search_matches` 的 text.to_lowercase().contains(query_lower)，使用本机 libunistring `u8_tolower`（`/usr/include/unicase.h`，无语言/无规范化）执行完整上下文 lowercase。`tui_term.c` 只承担动态库 ABI/句柄/分配释放胶水，`tui_input.S:li_case_contains` 用两个各 4096 字节有界栈缓冲转换并匹配，状态/遍历/去重仍 Assembly。每次调用释放库句柄与库返回的替换分配；库缺失/转换失败单独标记并显示，不静默退回大小写敏感搜索。
- Unicode 验证：GCC/Clang 九客户端程序通过，含 É、希腊 final sigma、İ→i+组合点、ß 不扩展为 ss，以及小输出容量/非法 UTF-8 失败。十八组 PTY 和实际产品持久重开场景通过，新增产品 `PERSIST` 匹配已存 `persist中🙂`，取消搜索退出后存储字节不变，无新增 run。产品重新构建、Assembly/G00 PASS；未据此宣称 U04/全机验收。
- 核心登记请求（新外部运行依赖）：请将 `libunistring.so.5`、开发头 `unicase.h` 与 `tui_term.c` 的 Unicode 外部 ABI 胶水职责纳入实际模块/依赖说明；现无新增源文件。客户端专项已链接既有平台胶水对象，产品经既有 libdl 调用，无额外强链接符号。运行环境缺该库时搜索显示不可用，输入编辑/取消仍可用。完整生产准入仍由核心核对。
- 单独 Escape 控件取消：核对 Pi `tui/src/stdin-buffer.ts` 的默认 10ms escape timeout。app 用 CLOCK_MONOTONIC 截止时间，仅当 input 为 ESC state=1 且搜索/候选控件活动时限制 poll 等待；事件已就绪时优先读取，截止到期才调用 `tui_input_escape_timeout`。该入口复用控件取消路径，恢复搜索原稿或关闭候选，不发起运行取消/退出；CSI、SS3、粘贴与普通编辑的无时钟字节解析保持。app 截止时间栈槽按派生帧扩展，无公共 Runtime ABI 改动。
- Escape 验证：GCC/Clang 九客户端程序通过；十九组 PTY 通过，包含独立 Escape→恢复原稿/候选后继续输入，以及活动工具运行+resize+Escape→恢复草稿→run 正常结束。专项核对 CSI 分片/粘贴/普通 ESC 不被 scoped timeout 取消。产品重建与离线持久目录/终端专项、Assembly/G00 通过。
- 输入 UTF-8 边界修复：`tui_input_incomplete` 现在包含 utf8_pending，防止 Enter 将半字符交给 Runtime；光标移动和重新开始 ASCII 字符前移除光标前的未完成码点，保留后缀。键入/粘贴共同校验 UTF-8 首字节、首连续字节的 E0/ED/F0/F4 标量上下界与编码 C1 控制字符，不将非法序列留入可重绘草稿。原生过滤是终端输入适配，参考 JS 字符串层不直接暴露非法字节，未声称其无效字节替换行为相同。
- 边界验证：GCC/Clang 九客户端程序通过，覆盖过长编码/代理区/超过 U+10FFFF/非法首字节/C1、普通与粘贴两路径、中间编辑保留后缀。新增 PTY 半字符+Enter 不启动 run，退格修复后正常提交；产品重建及离线持久目录/终端检查、Assembly/G00 通过。Escape PTY 曾把旧输出中的提示符当作截止时间完成，提前发字符导致误消耗；回放现明确等待 50ms 并持续读输出，针对宿主 10ms Escape 判定同步，不修改产品解析语义。
- 流式重绘半字符修复：按 U01/U02 的完整字符绘制要求，`tui_view_redraw_editor` 在已知 pending 区间只跳过未完成码点，仍绘制后方完整后缀；原稿字节/逻辑光标/pending 保留，字符补齐后正常显示。对不带 pending 标记但源末尾不完整的防御分支也不再把剩余字节当完整字符输出。该逻辑覆盖所有调用重绘的事件/resize/粘贴路径，不只按键分派。
- 验证：新增 view 单元核对中间半字符→流输出→resize→补齐，以及粘贴结束仍半字符；新增真实 PTY 工具运行中保留半个中文→流式重绘不泄露非法字节→终态后补齐为 a中b。GCC/Clang 九客户端程序、二十一组 PTY、Assembly/G00 通过；产品重建与既有离线持久目录/终端专项通过。未改 Runtime 接口或添加文件。
- 会话阅读当前交付：核对 Pi `tui.ts` 的逻辑行滚动及输入焦点隔离语义，接入 `PageUp/PageDown` → app → `tui_view_page`。view 自有 64KiB 原始源文本缓冲，记录接受的用户提示、启动历史、模型 delta、工具源输出/摘要及运行终态；借用切片在返回前复制。PageUp 临时进入 alt-screen，sink 代理在阅读期间抑制现场重绘，事件仍推进和入缓冲，阅读锚点保持；PageDown 向较新源行移动，到末尾或 Enter/Ctrl-C/Ctrl-D/Escape 返回主屏，回显阅读期间新内容并恢复草稿。退出说明前强制离开阅读屏。resize 按终端高度限制页源行数并重绘原锚点。
- 布局/边界：view 90552 字节，新增物理 sink/锚点/缓冲字段逐项 ABI 断言；app 帧仍派生。缓冲超限按旧源行淘汰，同步调整 read_anchor/read_start，视图显式提示淘汰；超大分片保留 UTF-8 边界后的尾部。阅读回放统一安全转义，不执行原文 ESC。返回前在抑制输出状态收束 md/tool 的活动缓冲，避免随后重复冲刷旧片段。无 Runtime 接口变更/新增文件。
- 阅读验证：GCC/Clang 九客户端程序通过；新增锚点冻结/恶意控制转义/退出恢复、64KiB 淘汰与分页返回最新单元；二十二组 PTY 通过，含工具输出中阅读→resize→Ctrl-D 回主屏（不删除非空草稿）→继续编辑、阅读屏 SIGTERM 正常恢复终端。Assembly/G00、产品重建与既有离线持久目录/终端专项通过。
- 阅读仍有明确差异：当前读的是源文本而非保留 Markdown/工具控件的完整视图树，现已按视觉行分页（1 列起）；完整字素簇宽度、超缓冲持久历史分页、鼠标与选择仍待补齐。此为实际可用闭环，不作为 U04/U07 整步验收。下一交付物集中于阅读的视觉行分页与控件布局，而非继续增加无消费者辅助层。
- 阅读视觉分页当前实现：`tui_view_page` 通过两次有界流式扫描定位视觉行，再用同一 token 迭代器绘制目标页；分页临时空间降为常量（96 字节主帧及单 token 胶水），不再建立整段视觉行表。阅读锚点为“源字节偏移 <<5 | 安全显示展开内偏移”，控制转义可逐格跨行/跨页恢复，源缓冲淘汰按相同缩放调整锚点，read_start 继续使用原始字节边界。标题裁剪为单行，正文每视觉行显式换行，不依赖终端的隐式自动折行时机。
- 极窄终端适配：已移除至少 8 列限制。正常宽度保留 CJK/emoji；字形本身宽于窗口时显示该 UTF-8 字节的安全十六进制转义，分片位置可继续翻页，不省略原内容。Tab 在源阅读视图采用 Pi 显示策略展开三空格，ESC/C1/非法字节保持安全可访问文本。safe writer 与迭代器共用 `tui_safe_seq_len` 校验完整标量，不执行原文控制序列。正常主屏绘制和存储字节不受此阅读适配影响。
- 当前验证：GCC/Clang 九客户端程序通过；新增 1 列阅读“中”的每个转义字符逐页连续核对、长 ASCII/CJK/emoji 跨页、ESC 展开内锚点与重复翻页、缓冲淘汰/返回最新。二十三组 PTY 通过，含 8 列 6 行长提示的实际可见后缀；产品重建与离线持久目录/终端、Assembly/G00 通过。仍缺完整字素簇、原位 Markdown/工具树与超缓冲持久历史分页，U04/U07 不勾选。
- 极小高度阅读：复用 view 预留 unsigned 保存真实 terminal_rows（默认 24，app 启动/resize 同步）；对象尺寸不变并增加偏移断言。页容量同时受真实高度与页偏好限制；1 行隐藏状态标题，2 行保留一行标题，3 行且发生淘汰时保留两行状态，其余优先正文。抵达物理最后一行时不再输出导致滚屏的 LF，分页位置仍按逻辑/视觉行推进，避免刚显示字符被换行滚走。
- 高度验证：GCC/Clang 九客户端程序通过，1–3 行快照逐字节核对标题/淘汰状态预算与末格无 LF；二十四组 PTY 通过，新增阅读中 resize 至 1×1 不滚走唯一行、Enter 返回主屏、正常退出精确还原 termios。产品重建与离线持久目录/终端、Assembly/G00 通过。完整字素簇与原位 Markdown/控件布局仍为实际缺口，不据本项勾选 U04/U07。
- 下一可运行交付物：原位 Markdown/控件、完整展开折叠、路径补全及复用选择/搜索/确认控件。U04 不勾选。U03 结构化事件消费仍待核心接口；本轮 G00 已 PASS（核心在共享树完成登记）；工程检查仍明确标记生产架构/构建/ABI/性能准入 BLOCKED，候选专项通过不代表整机准入。
