# 给 TUI Agent：完成原生 Pi Durable 终端客户端

你是 PanGu 的TUI实现Agent。工作目录 `/home/xu-yuanhao/Documents/PanGu`。请按本文顺序开发、验证和勾选，使用**手写 Assembly 第一、C 第二、C++ 第三**。运行时由 [核心 Agent](原生重写路线与迁移TODO.md) 负责，你将其能力做成简单易上手的终端交互。

## 完成目标

用户默认启动进入真正的交互式 Agent TUI，在同一界面完成模型配置、连续对话、编码工具、队列、会话/分叉、压缩、扩展、任务恢复及历史/用量查看。保持 Pi 的简洁性：以对话和输入为中心，少量必要状态、按需出现的控件，不堆常驻仪表盘或装饰。会话区、独立多行输入区与必要状态有稳定的布局、焦点和滚动关系，基本操作均可用键盘完成。持续打印日志加 `>` 的 REPL/调试壳不构成产品 TUI，也不能作为布局验收依据。

### 参考优先级与采用边界（2026-10-28 用户澄清）

1. **Pi 是主基准**：简洁性、默认交互、命令和工具行为；以固定快照的实际交互实现为依据，不能将简洁误解为缺少布局。
2. **Grok Build 是体验参考**：研究其输入/历史分区、焦点、滚动、块级浏览、展开/搜索/复制和终端稳定性为何顺手，将可核对行为用于改善 Pi 式简洁界面。参考体验，不要求复刻其完整仪表盘、功能范围或快捷键。
3. **Codex 与 Claude Code 是兜底参考**：Pi/Grok 对具体控件或状态反馈没有合适解法时再取底部面板、工具摘要、确认/选择及反馈方式。冲突时先保持 Pi 的一致性；Claude 社区还原源码不能冒充官方规格。

### Grok Build 体验调研与落地要求

2026-10-28 联网调研；“众多 TUI 中最好”是用户口碑，不作为已证实排名。社区反馈称其日常操作更像完整 TUI，并提到闪烁体验和模型响应速度；前者需实际终端验证，后者属于模型/服务性能，不能归功于布局或作为 PanGu TUI 验收替身。

| 可核对体验依据 | PanGu 采用的要求 | 验证 |
|---|---|---|
| 全屏交互界面，输入与历史有各自焦点；本地 prompt_widget、scrollback 实现 | 默认交互式 TUI，独立输入区、可阅读会话区；切焦点/关闭面板保留草稿和阅读位置 | 启动、输入、流输出、阅读、返回的实际画面与按键回放 |
| 历史按条目选择、折叠/展开、搜索、复制及全屏查看 | 将消息/工具作为可交互内容块，摘要和全文可切换；无需从滚动日志中找工具结果 | 长工具结果、diff、多轮历史中的选择/展开/返回 |
| 多行编辑、草稿暂存、上下文快捷键说明 | 输入反馈直接，补全/选择/搜索按需出现；提示当前可用操作，不常驻铺满帮助 | 中文/粘贴/候选/取消/resize，实际可见焦点 |
| 可选状态行、终端差异和输入协议适配 | 简短真实状态，低高度优先正文与输入；减少闪烁、跳动、重复打印，正确恢复终端 | 窄窗口、连续流、工具并发、SSH/tmux 能力范围和退出检查 |

来源：[官方概览](https://docs.x.ai/build/overview)、[官方快捷键](https://docs.x.ai/build/keyboard-shortcuts)、[官方终端支持](https://docs.x.ai/build/cli/terminal-support)、[社区体验讨论](https://www.reddit.com/r/LocalLLaMA/comments/1uxi5mf/grok_build_open_sourced_under_apache_20_license/)。网络最新资料用于调研，不自动更新实现快照；本地 Grok 固定 `2bdd1d6a` 的 `xai-grok-pager/docs/user-guide/{03-keyboard-shortcuts,21-terminal-support,25-status-line}.md`、prompt_widget 与 scrollback 是移植核对入口。具体键位以 Pi/PanGu 一致性为准，不混拼四家的键位。

### TUI 与基础命令行的边界

不开发独立 CLI 产品或另一套 CLI 会话体验。`pangu` 默认进入上述 TUI；命令行只承担 Pi 已有的启动参数、一次性操作与集成入口，共用同一 Runtime、配置、身份和存储。TUI 内 `/model` 等 slash 命令与 shell 子命令必须分开核对。

固定 Pi `d5b3de22` 的 `packages/coding-agent/src/cli/args.ts:printHelp/parseArgs` 和 main 分派是范围清单：

- 基础子命令：`install`、`remove`（`uninstall` 别名）、`update`、`list`、`config`（进入资源选择 TUI）、`auth`、`mcp`；子命令参数/子操作继续按其实际解析与对应模块逐项移植，产品名、更新目标和路径适配 PanGu。
- 启动与一次性入口：`--help/-h`、`--version/-v`、模型/provider/thinking、提示词和 `@files`、继续/恢复/指定/分叉/命名会话、工具开关、扩展/技能/模板/主题与上下文发现、信任和离线参数；`--export`、`--list-models`。
- Pi 已有 `--print/-p` 与 `--mode text/json/rpc` 属于同一 Runtime 的非交互/集成入口，纳入来源覆盖核对，不另做 CLI 产品；不能只完成这些入口而跳过 TUI。Pi 的 `--tui-mode fullscreen/regular` 也是同一交互式 TUI 的显示方式，regular 不得退化为裸 REPL。

上述是待实现范围而非当前支持声明。禁止新增无 Pi 来源的便利子命令；既有调试/demo 入口只作测试。具体实现和命令帮助由核心 K14 维护，TUI 消费接口，完整范围在 K15/U08 联合验收。

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

### U02 — 交互式 TUI 主界面、稳定布局与终端生命周期（重新验收）

原完成判定撤回：现有滚动回显和局部 editor 重绘只证明部分底层能力，未证明参考级交互式 TUI 布局。保留已实现代码和有效用例，不能继续宣称 U02 完成。

- [ ] 默认启动呈现 Pi 式简洁交互界面：简洁首屏、会话内容区、独立多行输入区、必要状态/按键提示；与 Grok 固定布局源码核对空间和焦点关系。空输入不会不断堆积 `>`。
- [ ] 建立统一的布局/渲染与原位更新路径；流式消息、工具和状态更新不挤走输入，不重复打印整份提示；选择/搜索/确认在同一界面按需出现。
- [ ] 会话滚动与编辑焦点独立；向上阅读时新流输出不拉走位置，返回最新可继续修改原草稿；工具块支持原位展开/收起。
- [ ] 多行、窄终端、低高度、resize、CJK/emoji/组合字符的实际光标/可见内容正确；正常退出、EOF、信号、失败路径恢复 termios/光标/粘贴/鼠标模式。
- [ ] 验收同时提交实际终端截图或录屏、对应源提交及 PTY 可见帧/交互回放：启动→编辑/粘贴→流输出→工具展开/收起→阅读/返回→选择/取消→resize→退出。逐项对照参考；单元通过、ANSI 字节出现、demo 能启动或 raw mode 正常均不能代替界面验收。

下一交付物优先完成该主界面闭环。已有输入/安全输出/工具状态可复用；除主界面直接依赖或阻断修复外，暂停继续细化局部 diff/辅助层。主界面验收后再沿 U03–U08 集成交互，不缩减其完整范围。

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

### 中央几何 Logo：正式功能批次（2026-10-29）

上一版手工半块字符常量已被替换，不能作为本批验收。SVG `assets/branding/pangu-logo.svg` 的两条原始路径不变；JSON 保留 V2 多边形/18°/9 单位法向裂隙并改记渲染规范，删除旧 terminal_art 字符阵列。

**来源与技术取舍。** 已阅读 Codex 固定 `8dd6099e265f14b80cb1b3fcf10c29a12bb6b23d` 的 `empty_state_animation/{geometry,paths,renderer,lighting,sequence,policy}.rs`、调用者 `empty_state_animation.rs`：路径展开/内外判定、带符号距离缓存、2×4 DOT_BITS、主题混色、时间插值和可见性/终止调度。PanGu 采用同一类点阵/时序原理；两块凸多边形直接用半平面判定，避免无消费者的 3D 投影、SDF 大网格和随机纹理。没有复制整套 Rust 实现，没有 PNG/字符画转换依赖。

**分层和调用链。** `tui_logo.S/h` 是独立原生几何/Braille rasterizer：SVG 16.16 坐标、64 位中间量，最大 40×20 cell；每 cell 为 2×4 dots，每 dot 四个确定性面积样本，覆盖至少一半点亮，覆盖率参与边缘亮度。标准位序 `[1,8,2,16,4,32,64,128]` 编码 U+2800+mask。两块原始多边形在闭合状态沿相反法线各移动半个 9 单位间距，smoothstep 进度趋近 1 时刚性回到原 SVG，不压扁/重画裂缝。

`li_logo_draw` 只管理主内容区域中央布局、颜色合成和变化 cell 输出；点阵和动画没有耦合输入解析。区域上限 40×20，按实际内容尺寸缩小，不覆盖顶部标题、底部编辑/提示；字符宽高比优先取 winsize 像素/行列，未知时 2:1，可 `PANGU_TUI_CELL_ASPECT=2.0` 覆盖。`tui_term.c:tui_logo_environment` 只承担平台 ioctl、locale/环境配置发现。RGB 用已知前景/背景混合模拟透明度，静止态约 22% 不透明度、闭合态约 41%；没有真实终端 alpha 或噪声。未知 RGB/无真彩色使用终端自身 dim 前景；`NO_COLOR` 禁用 RGB SGR。主题可由 `COLORFGBG`、`PANGU_TUI_THEME=light|dark` 或六位 RGB `PANGU_TUI_FOREGROUND/BACKGROUND` 提供。

`app poll → tui_view_logo_tick(now_ms)` 使用已有 CLOCK_MONOTONIC，2 秒一次性开启，50ms 目标帧距；poll 取运行/Escape/Logo 的最近期限。启动历史和 attach 之后才判断空会话资格；首次输入令图形直接收束到标准形，提交清除欢迎内容；有历史、运行中、阅读屏或区域不可见不安排 Logo 帧。完成返回 -1，空闲 poll 无限等待；resize 重新测量/布局，保留动画时间，不重启已结束动画。view 拥有 1600+1600 字节点阵/影子缓冲，几何无堆分配；每帧使用有界 40000 字节栈暂存，合并为一次物理写入，只更新变化 cell，不清屏播放。

**减少动态和兼容。** `PANGU_REDUCE_MOTION=1`、`REDUCE_MOTION=1` 或 `GTK_ENABLE_ANIMATIONS=0` 启动即显示标准静态形，不启动帧定时器。此为平台配置输入，尚未自动读取所有桌面系统/Runtime settings 的“减少动态”服务，核心提供该设置接口后应直接注入；不能宣称已经自动探测所有系统。UTF-8/字体支持不足、TERM=dumb/Linux console 或显式 `PANGU_TUI_BRAILLE=0` 使用中央 PanGu 文本，不输出损坏字形。当前宿主仍是项目 Linux x86-64 平台，ANSI/VT+Braille 渲染不依赖某一终端品牌，不宣称已验证其他 OS 的宿主实现。

**一致性与验证。** 新 `test_tui_logo.c` 从唯一几何 JSON 读取顶点/角度/间距，并核对 SVG 的实际 d 属性；独立双精度 ray-crossing oracle 验证三种 cell 比例、闭合/中间/标准三阶段的全部 Braille masks，当前零 cell 差异，允许的固定点边界误差上限为四个 cell。测试含 caller 容量/哨兵、裂缝、时间进度、结束/阅读/历史/减少动态无额外写入；SVG/JSON 改动与原生常量不同步会在该几何检查失败。当前 GCC/Clang 客户端十程序通过；100 次最大点阵计算约 17–18ms（不是完整终端帧耗时）。真实 PTY 已验证中央位置、输入避让、完整动画仅启动一次全屏清理、结束无输出、深浅主题、减少动态、降级、历史不闪出 Logo；像素尺寸 resize（cell 比例改为 2.5:1 后重新居中）、静止 250ms 零新增输出/最多一 CPU tick，以及完整动画 256KB 输出上限专项通过；真实一次动画观测约 79–93KB、仅一次启动清屏。终端 QA 的 pyte/Pillow 仅装于 `/tmp/pangu-logo-qa`，不属于产品/构建依赖。

**集成登记与剩余。** view 增长到 93824 字节，Logo 字段从 90568 起，transcript 旧偏移不变；静态布局断言和 app 派生帧同步。新增 `src/client/tui/tui_logo.S`、`src/client/tui/tui_logo.h`、`tests/client/tui/test_tui_logo.c` 需核心登记；客户端 build 已接实际 rasterizer 和测试，term/view/app 使用该路径，无静态 Logo 字符串后门。核心还需登记 `assets/branding/pangu-logo.svg`、`assets/branding/pangu-logo-geometry.json`，为 gate 增加品牌资源 SVG 类型/后缀及自测，把 logo.S 和既有缺失的 diff.S 加入实际产品构建及 view→logo/term 依赖，记录 term.c 新平台能力胶水。已运行 G00，明确 FAIL 于这些未登记文件/目录；Assembly inventory PASS 不能替代产品源准入。未越界编辑治理清单。2026-10-29 实际运行标准 `tools/runtime/build_pangu.py`，因 `tui_diff_render`、`tui_logo_raster` 未链接而失败；构建脚本要求已登记清单为唯一产品输入，因此未绕过清单打包。客户端同一 view/app 的十程序和真实 PTY 已通过，但不宣称整机产品构建成功，U02 仍不勾选。

### 当前优先批次：交互式主界面纠偏（2026-10-28）

- 来源：Pi tui/interactive 模式的独立编辑和内容区域；Grok 固定 `2bdd1d6a` 的 pager prompt_widget/scrollback 分区与焦点关系；优先级按上文用户澄清。源码仍沿固定快照，本轮未更改 references。
- 已交付入口：默认 ANSI+editor 的 `tui_view_banner` 启用主界面（至少 8 行）；顶部 PanGu 与真实 idle/running 状态、会话独立滚动区、底部输入区和操作提示。`li_editor_break` 不再将草稿结束并打印入会话，而是恢复会话光标；提交用户消息在 `tui_view_user` 明确写入会话。原 Markdown/tool/diff 继续使用同一 sink。多行编辑使用独立输入滚动范围，围绕光标裁剪后方内容；app resize 调 `tui_view_relayout` 更新范围并按安全源缓冲回放。阅读屏重置范围，返回恢复会话范围；退出恢复默认滚动范围。
- ABI/归属交接：view 新增 shell_active/body_end 两 unsigned，transcript 偏移 25032、view 90568 字节，C/Assembly 静态断言同步，app 帧继续从布局派生。新增 `tui_view_relayout` 为实际 app 消费者；请核心核对组合与模块入口。未改 Runtime 接口/治理清单，也未提交共享树。
- 验证：本批 GCC/Clang 客户端九程序通过；既有 28 组 PTY 在新默认布局通过。新增可选 `PANGU_TUI_VISUAL=1` 用 pyte 解码真实 PTY 帧（依赖仅临时安装 `/tmp/pangu-tui-visual-env`），检查固定 header/input/footer、提交用户消息可见、草稿唯一、800 字节粘贴后输入光标仍在区域。本轮可选可见帧检查已退出 0，不以截图替代模型联调。Assembly inventory 与 diff 空白检查通过；实际产品 diff 源登记缺口仍存在，未宣称产品构建通过。
- 预算变化自审：PTY 捕获上限从 256KiB 改 1MiB，以容纳当前逐键完整输入重绘的额外控制序列；这不是性能优化或预算验收通过。长草稿仍会重复绘制，可见更新批处理/差量绘制是后续必修项。启动等待由旧日志标记改为支持真实产品标题，光标仍显式定位（绝对行列及 CHA）。
- 明确未完成：小于 8 行仍为旧降级路径；resize 回放为安全源文本而非完整 Markdown/工具视图树；工具块原位双向折叠、面板布局/焦点、模型/目录真实状态、Unicode 字素簇、终端性能仍未完成。首版不能当作参考级主界面已验收，U02 保持全部未勾选。临时截图字体无 CJK 字形，不能用其方框判断产品中文表现；以真实终端/解码字节进一步验收。
- 下一可运行交付物：先补小窗口分区和选择/搜索面板的稳定布局，再统一块级视图与双向折叠、渲染差量和真实状态。暂停非主界面依赖的局部 diff 细化；主界面完整验收后继续 U03–U08。下列历史记录只作为可复用能力证据，不覆盖当前优先级。

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
- 工具真实参数消费者：核心现已更新公开桥接契约，TOOL_STARTED 的 detail 为 canonical JSON arguments，并提供独立 `tui_tool_observer`（owned JSON read/release）。本轮先消费实际 started.detail：主屏 `[arguments]` 经既有 code-point 有界/安全输出显示 160 字节预览，超限明确截断；完整源参数在借用失效前复制进入阅读缓冲，可 PageUp 查看。与 Pi `ToolExecutionComponent.createCallFallback/formatToolCallWithArgs` 采用真实 args 的用户行为对应；当前未实现工具专用参数格式化/折叠树。
- 参数验证：GCC/Clang 九客户端程序通过，新增长参数预览截断但阅读缓冲保留尾部、OSC/控制字节安全用例；二十五组 PTY 通过，tool_roundtrip 实际核对 `[arguments] {"path":"example.txt"}`。产品重建与离线持久目录/终端、Assembly/G00 通过。接下来通过新 observer 消费已提交 details 的 diff/首个变更行/图片，不再把这些项目当作接口未到位阻断。
- 结构化 edit 结果当前接线：app 经弱可选 `pg_tui_runtime_tool_observer` 查询并借用核心 observer，view 初始化为 NULL；工具完成事件 read owned `{entry.model[0].details}`，消费 firstChangedLine 与 string diff，显示首个变更行并调用既有 `tui_diff_render`，源 diff 复制入阅读缓冲后释放观察对象。成功、缺失及错误返回均按 owned 指针配对 release；观察错误显示明确提示，不推进执行。view 增 observer 指针后 90560 字节，公开布局断言同步。
- 本轮验证：GCC/Clang 九客户端程序、二十五组 PTY 通过；新增 observer read/release 配对、首变更行与 diff 原文留存用例。另修复 `tui_view_history` 在 transcript_append 后未重装 rdi=view 导致的跨回调崩溃。客户端专项链接 json-c，产品已有该依赖。
- observer 失败与类型边界：新增 owned 返回对象在成功/缺失/错误状态全部释放的独立用例，成功却无对象显示不可用；entry/model/message/details 逐层核对 JSON 对象/数组类型，firstChangedLine 仅正整数，diff 仅字符串。没有 diff 时可消费同一 committed details 的字符串 patch，并保留全文至阅读缓冲；普通文本不推断为 patch。GCC/Clang 九客户端程序通过，覆盖 malformed shape、错误带对象、缺失带对象、patch-only 等失败风险。产品 sources 缺 diff 登记仍为具体集成缺口，不能据专项通过宣称产品构建已恢复。
- 图片 committed 内容元数据消费者：核对 Pi `tool-execution.ts` 350–390 从 result.content 过滤 type=image、使用 data/mimeType 的入口。observer 在 details 之前遍历已提交 message.content，逐层类型检查，图片显示安全有界 MIME（缺失则 image/unknown）与 base64 字节数，显式说明数据位于 committed tool result；不输出或复制 base64 本体，不生成虚假附件路径/尺寸。所有借用 JSON 使用完成后由既有 release 收束。GCC/Clang 九客户端程序通过，新增无 details 图片内容、编码本体不泄露、释放配对专项。
- 图片剩余：当前仅元数据提示，不等同可访问附件交付。仍需真实路径/尺寸或终端图片呈现与恢复场景；source read 原图能力和 U03 完整验收未完成。产品 diff 源登记缺口仍需核心处理，不能以元数据专项替代产品构建/真实模型验证。
- 工具 committed 状态：核对 `durable_tool.S` 423–441 的 entry.data.status（completed/error/interrupted/aborted）及 637 的真实 abort 路径；observer 在 model/details 前独立消费 data.status string，显示 `[tool committed status]` 并复制到阅读缓冲，保留旧 tool_ok 事件摘要而不猜测取消。无 data/状态时静默沿原事件，无 observer 仍保持旧接口；未知将来状态按安全元数据展示。GCC/Clang 九客户端程序通过，含四种真实状态、未来状态中恶意 ESC 转义、owned 释放与无 model 的 receipt 场景。产品源登记仍待核心集成，未宣称整机完成。
- 重开工具结果消费者：参考 Pi 历史 ToolExecutionComponent 使用已保存 result 的行为，`tui_view_history` kind=3 后接同一 committed observer，恢复首变更行/diff/状态/图片元数据而不 submit/poll 工具。历史 ID 为显式长度借用切片，先验证 1–63 字节并复制到临时 NUL 缓冲，再调用观察入口；不依赖切片后的字节。新增非 NUL 四字节 ID 的历史恢复专项，核对只增加一次 observer read/release、不出现 run started。observer 三字段函数表布局补静态断言。GCC/Clang 九客户端程序、二十五组 PTY、Assembly/G00 通过；产品 diff 源登记仍待核心，不宣称重开产品联调已通过。
- diff 长源行保真：对照 Pi `components/diff.ts:renderDiff` 保留原内容与 replaceTabs 行为，长于 255 字节的源行现在先收束挂起组，再完整逐行着色，避免进入有界行内对比缓冲；通用行输出改成流式 TAB span→三空格→安全输出，不再使用 768 字节内容展开缓冲截断。短行 1:1 的公共前后缀高亮仍保留，尚不同于上游 diffWords，未因保真修复宣称完整行内算法移植。
- 长行验证：GCC/Clang 九客户端程序通过，新增各 1500 字节删除/新增行、中文与 emoji 尾部、TAB 展开后准确字节长度及 ANSI 颜色/结束标记专项；Assembly PASS。产品源登记未变，链接缺口仍待核心；不复用旧产品成功结果证明新 observer 调用链。
- diff 行号解析修复：再次核对固定 Pi `components/diff.ts:parseDiffLine` 的 `whitespace* digits* separator content` 顺序，原生不再把内容开头数字继续吸收为行号；数字后无分隔符时回退到先前空白边界，TAB 可作强制分隔符。公共前缀到达较短内容末尾时直接完成边界判定，不读取临时缓冲的下一未初始化字节。新增数字正文、空白行号、TAB 分隔、数字紧接正文、ASCII/CJK 前缀延长用例。GCC/Clang 九客户端程序、Assembly 检查与所属文件 diff 空白检查通过。解析目前支持 ASCII 空格/TAB，尚未完整实现 JS `\s` 的 Unicode 集合；词级 diffWords 与完整产品构建仍未验收。
- diff Unicode framing：继续使用 Pi `d5b3de22` 固定快照（当前 origin/main 已领先 29 提交，本批不切换）；核对 `diff.ts:parseDiffLine`、直接消费者 `core/tools/renderers/edit.ts:110/148` 及 `test/tools.test.ts` 的 details.diff/patch 用例。`d_space_width` 按完整 UTF-8 标量识别 ECMAScript 空白集合，`d_parse` 按标量回退最后一个空白作为分隔符，行号字段保留原始字节；上下文前缀统一输出空格。内容中的 CR/LF/U+2028/U+2029 按上游无 dotAll 正则拒绝解析，沿既有原文安全上下文路径显示。新增 24 种可独立成行的空白在前缀/数字分隔/回退中的循环用例、非空白 U+200B/U+0085、截断 UTF-8 和正文分隔符；Node 运行原正则交叉核对 framing fixtures。GCC/Clang 九客户端程序、Assembly 及 diff 空白检查通过。此项取代上一条 ASCII 空白限制，词级 diffWords、组容量与末尾空行差异仍未解决，产品登记缺口仍保留。
- diff 完整连续组：复核 Pi `diff.ts:renderDiff` 先收集全部删除行/新增行、仅准确 1:1 才调用行内比较的规则；本轮原地替换按 16 行缓存收束的主循环，改为对整个借用源区间计数，再完整流式绘制多行组。超长行也参与同一连续组，避免长行或 16 行边界后的短尾部误成 1:1。仅真实短 1:1 复制两条源行进入既有比较器，栈帧由 9944 降至 2200 字节，移除无消费者的 TUI_DIFF_RUN_LINES 容量声明和旧分组路径。GCC/Clang 九客户端程序、Assembly 与所属 diff 空白检查通过；新增 33:1、1:33、长行+短尾部、上下文后恢复准确 1:1 及纯文本源保真用例。此项消除组容量差异；仍有短行公共前后缀近似、长 1:1 不做词级比较及尾部空行差异。
- 下一段词级算法来源已定位：Pi package.json 固定 `diff:8.0.4`；从 npm 官方 tarball 读取 `libcjs/diff/word.js`、`base.js`，临时只读副本位于 `/tmp/pangu-jsdiff-reference/package/`，没有改 references。WordDiff 使用扩展 Latin 词字符/单标点/包围空白 token，trim equality、编辑路径和 postProcess 空白归并；不能用简单字节 LCS 宣称等价。下一交付物仍为接入实际 1:1 消费者的完整词级比较；尚未复制该第三方实现，采用时须保留真实 BSD-3-Clause 来源。
- 完整 1:1 内容消费者：移除 255 字节旁路和两条固定源行副本，真实单行组将完整借用源行传给 `d_render_pair`；经溢出校验分配一次容量为源内容总长三倍的缓冲，保存两侧完整 TAB 展开结果，完成绘制后释放。非 ANSI 路径不分配；溢出或分配失败完整逐行着色，不截断。主循环帧降为 72 字节，pair 自有 120 字节临时帧；移除不再适用的 TUI_DIFF_LINE_MAX 声明。测试用链接器 malloc/free wrap 注入失败并核对零存活分配，1500 字节中文/emoji 长行已进入现有比较路径。GCC/Clang 九客户端程序、Assembly 与 diff 空白检查通过。本项消除长行旁路，但行内算法仍为公共前后缀近似，不能勾选词级 diffWords。
- 空片段高亮：对照 Pi `renderIntraLineDiff` 的 `if(value)`，现有比较器只在变更片段非空时发 inverse/on-off，避免相同行或纯插入/纯删除的未变侧出现空反色。新增相同中文/emoji、空内容及两种单侧变化用例，沿上述分配释放断言核对。GCC/Clang 九客户端程序及 Assembly 通过。下一交付物仍是 WordDiff 分词、编辑路径与空白归并接入该完整内容消费者；完整产品登记和真实工具联调仍待核心集成。
- observer 完整客户端 PTY 接线：在既有 fake_runtime 原地增加显式 `committed_edit` 场景，脚本使用真实 edit 名称、参数和工具终态；独立可选 observer 仅对该测试适配器/场景返回 owned committed JSON，终态前不可读，其他场景仍返回 NULL。PTY 从 app 的弱扩展查询进入 view，核对 committed completed、firstChangedLine=7、数字正文双侧 ANSI diff，阅读页完整原文及返回后草稿可编辑，最后 /quit 精确恢复终端。这是注入的数据契约回放，不执行文件修改或冒充真实 Runtime；JSON 释放仍由 observer release，view 单元另有配对计数。GCC/Clang 九客户端程序和二十六组 PTY 全部退出 0，所属文件 diff 空白检查通过。未新增产品能力/源文件，产品 diff 登记缺口仍需核心集成，真实 edit 保存/恢复联调未验收。下一交付物继续词级算法和原位工具控件，不以本轮回放替代 U03/U08。
- committed 错误结果分支：核对 Pi `core/tools/renderers/edit.ts:formatEditResult` 的 isError 先返回错误内容、成功才显示 diff 的规则，并核对核心 `durable_tool.S` 提交 message.isError 的真实布尔来源。observer 消费 model[0] 后仅在 `isError` 为 JSON boolean true 时跳过 firstChangedLine/diff/patch；既有工具错误原文、committed 状态与图片元数据路径保留，owned 对象正常释放。false、缺失或非布尔值不推断为错误。新增包含 diff 的失败结果、false/字符串/数字边界，核对屏幕和阅读缓冲均无失败修改摘要且错误原文仍在。GCC/Clang 九客户端程序、二十六组 PTY、Assembly 及所属 diff 空白检查通过。真实 Runtime edit 保存/重开仍未验收，词级算法仍未替换。
- committed 错误全文消费者：沿 Pi `formatEditResult` 的 content.filter(type=text).map(text).join(newline) 接入 `li_observe_error_text`；仅 committed isError=true 分支调用，在 JSON owner 释放前遍历 typed content 数组，逐块安全输出并复制完整源字节入阅读缓冲。事件错误摘要继续保留；图片/base64 和畸形 text 值不混入正文。长度来自 json-c，内嵌 NUL 后文字不丢失；换行在文本块之间及终端行尾显式写出。新增 1500 字节尾部中文/emoji、两文本块、NUL/ESC 安全输出、图片数据不泄露、阅读源全文与 read/release 配对专项。GCC/Clang 九客户端程序、二十六组 PTY、Assembly 检查通过；无新源文件/布局/Runtime 接口。完整词级算法、原位控件及真实 Runtime edit 联调仍待完成，不勾选 U03。
- WordDiff 零编辑路径实际接入：采用 Pi 固定依赖 jsdiff 8.0.4 的 extendedWordChars/trim equality 与 base.buildValues keep 取 newTokens 的语义，在 `d_render_pair` 的完整 TAB 内容缓冲之后调用 `d_words_equal`。`d_next_word_core` 按完整安全 UTF-8 标量跳过包围空白，将扩展 Latin 连续串聚为词、其余每标量独立；两侧 token 顺序和核心字节相同时，ANSI/纯文本两种输出均使用新内容空白且不产生 inverse。`a b` 与 `ab` 不会因去空白被合并，非法 UTF-8 保留安全回退；无额外分配，借用范围随既有 pair owner。源码内保留 jsdiff 完整 BSD-3-Clause 通知。新增 Latin/扩展 Latin、CJK、emoji/标点、NBSP/U+2003、全空白、词边界和非法字节用例；8 组零编辑 fixtures 用 npm 8.0.4 原 diffWords 交叉核对。GCC/Clang 九客户端程序、二十六组 PTY、Assembly 与 diff 空白检查通过。此为完整词级算法的已接入零编辑分支，不替代尚缺的编辑路径和 postProcess 空白归并；有词增删仍用公共前后缀近似，U03/U08 不勾选。上一条“非 ANSI 不分配”已由此取代，以保持两种输出相同语义。下一交付物为相同 token 入口上的准确编辑路径及结果段归并。
- WordDiff 无共同 token 分支：在相同的 `d_next_word_core` 消费者上接入 `d_words_disjoint`，完整两侧 token 无任何核心相等时，直接采用唯一的全删除/全新增结果，不再按字节公共前后缀把词内相同字符留作 keep。`d_render_pair` 分别记录两侧首个变更段的前导空白字节数，按照 Pi renderIntraLineDiff 将其留在 inverse 之外；正文与尾部空白保持完整。pair 帧从 120 改为 136 字节以保存独立 new-prefix，所有权/失败回退不变。新增 foobar→foobaz、élan→éclat、不同缩进/NBSP、尾部空白和单侧空内容；旧 aa→ab、12foo→13foo 和 a→ab 的字节近似断言改为上游整词高亮。8 组由 jsdiff 8.0.4 原实现核对确无 keep。GCC/Clang 九客户端程序、二十六组 PTY、Assembly 及 diff 空白检查通过。当前扫描复用源 token、常量空间，最坏两侧 token 数乘积时间；尚未采用通用编辑路径，包含共同 token 的增删仍为近似，不能勾选完整词级算法。下一交付物是共同 token 间的最短编辑路径与空白归并。
- 共同上下文的词边界：实际 pair 路径新增 `d_word_boundary`，复用已接入的 jsdiff token-core 分词；公共字节前缀若终止于词内部，退到该 token 起点，公共后缀若开始于词内部，前移变更终点至 token 尾部，两侧取相容的共同长度。精确 token 边缘和空白不移动，借用源文本无新增分配。修复 const foobar→foobaz、call(fooTail)→call(barTail)、扩展 Latin élan→éclat 及中文后缀中的整词高亮，避免把同一个词的共享字节误成 keep。四组已用 jsdiff 8.0.4 原结果核对；GCC/Clang 九客户端程序、二十六组 PTY、Assembly 与 diff 空白检查通过。此项仍使用一个变更区间，多个词变更之间的 keep 和 postProcess 空白归并尚未等价，完整词级移植未验收；下一交付物保持为通用编辑路径/多段渲染，不以边界修正替代完整算法。
- 全局展开偏好继承：复核 Pi `interactive-mode.ts:setToolsExpanded` 的 toolOutputExpanded 与新组件 `component.setExpanded(this.toolOutputExpanded)`（3502/3581 等）；`tui_tool_expand` 除展开保留槽外保存 expanded_preference，`tui_tool_started` 在重置槽后将其继承到新调用。偏好保存在 tool 原尾部填充偏移 19140，tool 仍 19144 字节，rec/view/app 尺寸和原偏移不变，新增公开字段及逐项静态断言；init 既有清零覆盖偏好，跨运行/槽轮转保持，重新 init 恢复默认。帮助文案同步提示新调用继承。新增空工具时先展开、六个新 ID 轮转、尾部直接输出、重新初始化恢复预览单元；PTY 真实按键验证 Ctrl-O→两轮运行的新工具均显示超十行尾部。GCC/Clang 九客户端程序、二十七组 PTY、Assembly 及 diff 空白检查通过。此项消除“新调用不继承”缺口，仍无原位双向收起，不能据此勾选 U03/U04。下一交付物继续原位工具状态视图与通用词级多段 diff，产品 sources 登记仍待核心。
- 成功 committed 结果文本接线：核对 Pi `ToolExecutionComponent.getTextOutput/createResultFallback` 的 typed text 内容入口；将原错误专用 helper 原地改为 `li_observe_result_text`，成功和错误共用 JSON 完整长度、文本块顺序、安全输出、阅读源复制和 owned 生命周期。成功使用 `[tool committed output]` 标记，之后继续 details/diff；只有真实 isError=true 跳过 diff。新增相同 1500 字节 payload 在 true/false 两路径的尾部中文/emoji、内嵌 NUL/ESC 与源缓冲核对，成功 observer PTY 检查 canonical 正文再显示首变更行/diff。GCC/Clang 九客户端程序、二十七组 PTY、Assembly 与所属 diff 空白检查通过。当前 canonical 成功文本直接输出全文，尚未接入 createResultFallback 的十行预览/展开状态，且滚动模式可能保留事件摘要和最终正文两份显示；本项只完成 canonical 数据消费，不作为渐进结果控件验收。下一交付物需将该正文统一纳入工具预览/展开控件，连同原位去重处理，不能把本路径当完整 U03。产品 diff 构建源仍缺登记，公开 TUI 操作接口仍只有 submit/cancel/poll/history/shutdown 加 tool observer，U05/U06 不勾选。
- committed 成功结果预览消费者：沿 Pi `createResultFallback` 的十行预览/expanded 全文规则，新增实际调用的 `tui_tool_result_begin/end`（同既有 tui_tool.S，无新源）：begin 仅重置目标调用的内容计数/行/隐藏尾部，保留身份、真实终态与 expanded；正文块和块间 LF 经既有 tui_tool_output，end 仅冲刷该调用并给隐藏尾部 Ctrl-O 提示。view 的成功 canonical 内容进入此生命周期，原始完整字节仍逐块复制阅读缓冲，JSON release 在消费后。错误 renderer 保持上游错误全文路径；无保留槽（如当前历史回显）保留安全全文回退。新增 begin 缺 ID、旧流内容替换、终态保留、十行预览、尾部展开、expanded 继承及重复 end 无输出单元；observer PTY 现在含 12 行成功正文，确认 CANONICAL_TAIL 默认不可见、Ctrl-O 后可见，再阅读 diff/恢复草稿。GCC/Clang 九客户端程序、二十七组 PTY、Assembly 及 diff 空白检查通过。未改变布局/Runtime 函数表。请核心将 tui_tool_result_begin/end 的实际 view→tool 依赖纳入模块入口登记。此项替代上一轮成功正文一律全文直出的临时路径，仍缺历史完整工具组件、原位去重/双向收起和精确视觉行计数，不勾选 U03/U04。下一交付物继续完整工具视图及词级多段编辑算法。
- 当前产品集成缺口（需核心按文件归属处理）：`tools/runtime/build_pangu.py` 本轮链接失败：undefined reference to tui_diff_render。此前 diff 无实际产品消费者，未进入 `governance/assembly-first.json` 实际产品 sources；现请把 `src/client/tui/tui_diff.S` 加入产品源及模块依赖，核对 `tui_view`→diff/json-c/observer 实际调用链。G00/Assembly 检查仍 PASS，不能据此宣称产品构建通过。未修改治理文件或以弱空实现掩盖缺失；完整目标保持未完成。
- 剩余与下一交付物：U03 专用参数格式化/折叠、结构化工具取消结果、diff/首个变更行及图片附件的真实事件接线；diff 当前公共前后缀算法仍不等同上游 diffWords，长行已改为完整流式输出，不能据独立测试宣称完整移植。U04–U08 保持未完成。

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
- 阅读鼠标滚轮：核对 Pi `tui-alt-screen.ts` 67–69 的 SGR 鼠标模式启停及 `tui.ts` wheel 事件契约；临时阅读屏开启 1000/1006，返回/信号退出关闭后离开 1049。Assembly 字节状态机消费 SGR button/X/Y（数字上限、正坐标、完整终结符），仅滚轮 press 映射为专用 key；app 仅 reading 时消费为前后翻页，主屏报告不启动阅读、不改变草稿。畸形坐标/溢出报告消费至终结符后丢弃，release/普通点击忽略；此为分页滚轮适配，未实现点击选择或鼠标拖拽。
- 鼠标验证：GCC/Clang 九客户端程序通过，含修饰滚轮、release、普通点击、无效坐标与报告后继续输入；二十五组 PTY 通过，新增逐字节 SGR wheel→长文本前后页→关闭鼠标报告→草稿继续编辑。产品重建/离线持久目录终端、Assembly/G00 通过。已有阅读信号清理路径共同关闭鼠标模式，不据本项宣称完整鼠标交互验收。
- 下一可运行交付物：原位 Markdown/控件、完整展开折叠、路径补全及复用选择/搜索/确认控件。U04 不勾选。U03 结构化事件消费仍待核心接口；本轮 G00 已 PASS（核心在共享树完成登记）；工程检查仍明确标记生产架构/构建/ABI/性能准入 BLOCKED，候选专项通过不代表整机准入。
