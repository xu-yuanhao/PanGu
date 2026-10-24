# TUI 汇编重构交接报告（第二位 Agent → 主 Agent）

日期：2026-10-25。任务：`docs/并行开发任务提示词-汇编优先TUI重构.md`。
状态：客户端纯逻辑五个模块已全部原地替换为手写 x86-64 汇编并删除旧 C，
独占构建/测试/PTY 验证全部通过；共享登记与产品构建切换留给主 Agent（差异见下）。

## 1. 完成项

- [x] 读取 Pi 参考：`references/pi/packages/tui/src/terminal.ts`（raw 模式生命周期、
  退出前排空输入防泄漏、SIGWINCH 刷新）、`stdin-buffer.ts`（序列完整性/ESC 重组成/
  Meta+键吞掉）、`keys.ts`；Pi 快照提交 `2ec55d808d455b1a64a2bf7888198207a096120a`。
  行为以现有头文件/测试为准（Pi 的 kitty 协议协商、括号粘贴等不在本轮范围）。
- [x] `tui_utf8.c` → `tui_utf8.S`：四个导出（seq_len/last_seq_start/incomplete_tail/
  truncate_len）。
- [x] `tui_input.c` → `tui_input.S`：init/reset/feed + 头内 static inline 查询 helper
  转为汇编导出（text/len/empty）。ESC 状态机、整字符拒绝、节流、上限不变。
- [x] `tui_state.c` → `tui_state.S`：init/submit/cancel/poll + running/fatal 转汇编导出。
  busy 拒绝、取消仅请求、唯一终态、有限 poll(16)、fatal 传播均保持。
- [x] `tui_view.c` → `tui_view.S`：全部 12 个导出 + 渲染内部流程（vput 换行翻译、
  挂起/重绘、有界截断标记、carry 拼接）。libc 仅用 strlen/memcpy/snprintf。
- [x] `tui_app.c` → `tui_app.S`：主循环/poll/输入派发/生命周期/信号安装恢复/
  exit_reason_name 全部汇编；sigaction 布局经 tui_layout.h 常量 + ABI 测试
  offsetof 断言；errno 经公开 `__errno_location()`，不碰 TLS 私有布局。
- [x] `tui_term` 拆分：init/enter_interactive/restore/write/is_interactive 迁
  `tui_term.S`；`tui_term.c` 仅保留 termios/winsize 平台结构胶水（见 §3）。
- [x] 每模块迁移同批删除旧 C；无兼容层/双后端/旧实现挪移。
- [x] 新增 `tests/client/tui/test_tui_abi.c`（ABI/边界专项）与 `test_tui_view.c`
  的 carry 回归用例；`tools/client/tui/build.sh` 已切汇编输入。

## 2. 删除/新增与导出清单

| 删除 | 新增 | 导出符号（均为原头文件契约，无新增公共接口） |
|---|---|---|
| `src/client/tui/tui_utf8.c` | `tui_utf8.S` | tui_utf8_seq_len / last_seq_start / incomplete_tail / truncate_len |
| `src/client/tui/tui_input.c` | `tui_input.S` | tui_input_init / reset / feed / **text / len / empty**（由头内 inline 转出） |
| `src/client/tui/tui_state.c` | `tui_state.S` | tui_state_init / submit / cancel / poll / **running / fatal**（同上） |
| `src/client/tui/tui_view.c` | `tui_view.S` | tui_view_init / redraw_editor / finalize_editor / banner / exit_note / busy_rejected / submit_error / input_limit / cancel_requested / poll_error / protocol_violation / event |
| `src/client/tui/tui_app.c` | `tui_app.S` | tui_app_run / tui_exit_reason_name |
| （无删除） | `tui_term.S`（新增，拆自 tui_term.c） | tui_term_init / enter_interactive / restore / write / is_interactive |
| （新文件） | `src/client/tui/tui_layout.h` | 无符号；结构偏移/枚举/平台常量的单一定义 + C 侧编译期断言 |
| （新文件） | `tests/client/tui/test_tui_abi.c` | 仅测试 |

修改：`tui_input.h`/`tui_state.h`（inline helper 改声明）；`tui_term.c`（缩至平台胶水，
导出 `tui_term_linux_platform` 平台表）；`tests/client/tui/test_tui_view.c`（新增用例）；
`tools/client/tui/build.sh`。

汇编约定：GAS Intel 语法、`.cfi`、`.type/.size`、`.note.GNU-stack`、RIP 相对访存、
外部调用一律 `call sym@PLT`、跨文件数据（平台表）经 `@GOTPCREL`（PIE 安全）、
栈对齐在所有调用点保持 16 字节、方向标志仅在 `cld` 后使用串指令。

## 3. 保留 C 胶水的逐文件理由

- `src/client/tui/tui_term.c`（登记 c-bridge/platform-layout）：仅
  `linux_enter_raw`/`linux_restore`/`linux_query_size` 与平台表。理由：
  `struct termios`/`struct winsize` 字段布局、c_cc 下标（VMIN/VTIME）与
  IGNBRK/IXON/CS8 等标志值由 `<termios.h>`/`<sys/ioctl.h>` 派生，属平台结构适配，
  保留 C 由编译器维持布局契约；不含任何控制流状态机（分派/写循环在 .S）。
- 其余 TUI 文件无 C 实现。`runtime_bridge.c`（主 Agent 维护）未动，仍为待迁移 C。

## 4. 主 Agent 需要的登记/构建调整（差异，未由我执行）

`governance/bootstrap-policy.json`（G00）：

- 移除登记：`src/client/tui/{tui_utf8,tui_input,tui_view,tui_state,tui_app}.c`
- 新增登记（client-source：`tui_layout.h`、六个 `.S`；client-test：`test_tui_abi.c`）
- 本报告 `docs/migration/tui-assembly-handoff.md` 本身如需入库按 document 登记

`governance/assembly-first.json`：

- 删除 5 个 `pending-inplace-assembly` 条目（上列 .c）
- 新增：六个 `.S` → `assembly-core`；`tui_layout.h` 如需登记可并入（纯头，无实现）
- `tui_term.c` 保持 `c-bridge/platform-layout`（职责已收窄，理由仍成立）

`tools/runtime/build_pangu.py` / 产品 SOURCES（现为读登记表，登记更新后自动生效）：
TUI 输入应为 `tui_utf8.S tui_term.c tui_term.S tui_input.S tui_view.S tui_state.S
tui_app.S runtime_bridge.c`。

另：G00 当前还报 `tests/runtime/test_session_commit.c` 缺失/未登记与
`tools/runtime/build_test.py` 等改动，属主 Agent 并行工作，与本任务无关，未触碰。

## 5. 验证证据（实际命令与结果）

独占构建与测试（`bash tools/client/tui/build.sh run-tests`，gcc 16.2.1，退出码 0）：

- test_tui_utf8 / test_tui_input / test_tui_view / test_tui_state / test_fake_runtime /
  test_tui_abi：`all passed`（原五组用例未删改期望，仅新增）。
- ABI 专项内容：sigaction/全部结构偏移 offsetof+sizeof 编译期断言；
  调用前主动破坏全部 caller-saved、callee-saved 哨兵核对（rbx/r12-r15；
  rbp 不可测，说明见测试注释）；栈金丝雀；汇编→C 回调入口 rsp≡8 (mod 16)；
  保护页（PROT_NONE 紧贴缓冲尾）覆盖 utf8 回扫/截断、tui_input 满 1024 拒绝/
  退格/清行、view text_delta 边界读；零长度/满容量用例。
- demo 五场景（含 ^C 取消、忙时二次提交）非交互模式退出码全 0。

PTY 套件（`python3 /tmp/pangu-tui-probe/pty_test.py`，独占构建产物，5/5 PASS）：

1. interactive_roundtrip：UTF-8 输入、code point 退格、整行重绘、完整 run、^D 退出；
2. ctrl_c_idle：闲置 ^C 退出；
3. sigterm_during_run：run 中 SIGTERM 优雅退出；
4. sigwinch_during_run：run 中改窗口尺寸不崩溃、随后取消+退出；
5. busy_then_cancel：忙拒绝、取消提示、终态、^U+^D 退出。
   每用例均做退出前后 PTY termios 全字段（含 cc 数组）相等比较，全部恢复。

工具链与集成：

- clang-23 组装六个 `.S` + 编译 `tui_term.c`/`runtime_bridge.c`：通过
  （`-Wall -Wextra -Werror -pedantic -O2`）。
- 完整产品组合手工编译（不改共享脚本，/tmp 内按替换后源单）：
  `composition/pangu.c + runtime/{agent.S,session.S,mimo.c,tools.c,read_tool.c} +
  TUI 汇编 + runtime_bridge.c` + libcurl/json-c → 链接成功（PIE ELF）。
- `python3 tools/gates/check.py`：FAIL（预期中间态）——仅上列 §4 登记差异 +
  主 Agent 侧文件；无其他违规。
- `python3 -B tools/runtime/test_pangu_pty.py`：**NOT_RUN**（在
  assembly-first admission 处被阻：登记表仍列已删除的 5 个 .c；按任务不改共享
  脚本绕过）。登记切换后应可直接运行。
- 真实 MiMo 工具往返：**NOT_RUN**（本会话环境无 MIMO_API_KEY；按任务该最终
  验证由主 Agent 在其凭据环境执行）。产品二进制在无凭据下按契约退出码 2
  （"MIMO_API_KEY is required"）。

## 6. 与被替换 C 版的已知行为差异（均为修正 C 缺陷，附证据）

1. **text_delta carry“吸收”路径**：C 版在本段整体并入 carry 时仍会把已并入的
   字节提前写给终端并重复一次（4 字节序列按 1+2+1 拆分可复现：C 输出
   `9F 98 F0 9F 98 80`，违反 tui_view.h 声明的“绝不向终端写出半个序列”）。
   汇编版不提前写出（输出恰为 `F0 9F 98 80` 一次）。探针记录于本报告随附
   /tmp/pangu-tui-probe/carry_probe*.c（已本地验证，未入仓库）；回归用例
   `test_utf8_absorb_split` 固定该契约。
2. **banner/截断标记 snprintf 返回值截断**：C 版以 snprintf 返回长度直接作为
   写出长度，超长 runtime 名会越过 64/128/160 字节栈缓冲读。汇编版将长度
   钳制到缓冲区内（正常路径字节输出与 C 完全一致，原测试全过）。

## 7. 已知问题与限制

- 本轮既有限制不变（头文件已记录）：仅行尾编辑、无字素簇/多行/括号粘贴/主题；
  paste 换行视同提交；poll_tick 定时推进为 v0 临时方案。
- rbp 的 callee-saved 保存无法用 C 内联手段破坏性测试（编译器拒绝），已用
  rbx/r12-r15 五寄存器覆盖；tui 汇编无一处读写 rbp。
- 信号处理器栈对齐采用“保存 rsp + and -16”模式，不依赖内核入口对齐假设。
- G00/装配门禁在主 Agent 完成登记切换前保持 FAIL（§4），非实现缺陷。

## 8. 未完成/不由本任务完成

- `runtime_bridge.c` 迁汇编（主 Agent 待迁移清单内）。
- 完整 P1/T28、真实 MiMo 往返、`test_pangu_pty.py` 复跑、生产认证：主 Agent
  集成后执行；本报告不声称完成。

## 9. 主 Agent 集成回执（2026-10-25）

已完成五个旧C删除/六个手写汇编及布局头/ABI测试/本报告的共享登记，产品构建由语言清单直接使用新实现。runtime-local-modules同步更新；runtime_bridge.c仍为主Agent待迁移项。独占构建脚本已改为对象直接写OUT，不在仓库内生成后搬走，避免失败残留。termios胶水仅精简说明注释，逻辑未调整。

实际复验：bash tools/client/tui/build.sh run-tests六组全部通过；核心八组O0/O2 UBSan全部通过；完整工作区G00通过（生产准入仍BLOCKED）；无网络PTY Ctrl-D/SIGTERM/resize及termios恢复通过；真实MiMo PTY read工具/模型续答、流中取消与精确termios恢复全部通过。以上是登记切换后的完整产品组合验证，补齐原报告NOT_RUN项的本轮范围，不表示完整P1/T28或生产认证。

控制面由主Agent按用户授权自审：只更新实际源码/语言/构建登记，不放宽检查器、预算或断言；风险仍为Linux x86-64平台常量、现有限制以及桥接/模型/工具尚待迁移。固定摘要见本次核心进展记录。
