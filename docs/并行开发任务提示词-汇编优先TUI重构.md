# 给第二位 Agent：原地重构 TUI，Assembly 第一

你负责 PanGu 的 TUI 汇编重构。主 Agent 同时负责 Agent Loop、Session、Tools 等核心汇编重构。直接实现并验证，不停在设计报告，不建设新的治理框架。

## 目标和硬边界

工作目录 `/home/xu-yuanhao/Documents/PanGu`。先读根 AGENTS、主文档的语言组成、工程门禁当前范围、迁移 TODO 近期队列，再读本任务。项目语言顺序固定为 **Assembly 第一、C 第二、C++ 第三**；不是“先用C做完整产品，等热点出现再补汇编”。当前真实MiMo/只读TUI已可运行，保持这条链路可用并原地换实现。

只保留一份有效实现。一个模块迁移完成就在同批变更中删除旧C源，更新构建列表，清理旧声明/登记；不挪到tests、legacy、reference、backup或fallback，不留运行时双后端、语言选择开关或C兼容层。回溯靠Git历史。正常的行为测试可以保留，但不能把旧产品实现包装成测试预言机。不得用C编译器生成的汇编冒充手写实现，也不得写一个跳回原C核心的汇编壳。

C ABI指接口约定，不代表实现必须用C。C头文件保留声明和布局契约，避免把被删除的C实现藏成static inline。汇编同样通过显式函数表组合，不破坏插件替换点。C++本任务禁止新增；TypeScript/Python/Rust不能进入产品运行时。

## 不干扰主 Agent

你独占以下文件的原地改动及同名 `.S` 替换：

- `src/client/tui/tui_utf8.c/.h`
- `src/client/tui/tui_input.c/.h`
- `src/client/tui/tui_state.c/.h`
- `src/client/tui/tui_view.c/.h`
- `src/client/tui/tui_app.c/.h`
- `src/client/tui/tui_term.c/.h`：仅处理必要终端胶水或拆出有意义的汇编部分，C保留必须写清具体平台原因。
- 对应 `tests/client/tui/test_tui_*.c`、`test_util.h` 和新增的ABI/边界测试。
- `tools/client/tui/build.sh`：只改你负责实现的构建/测试，不产生checkout内二进制。
- `docs/migration/tui-assembly-handoff.md`：一份交接清单，实际结果和待集成项。

只读且不得改：`runtime_bridge.c/.h`、`tui_runtime_adapter.h`、`src/contracts/`、`src/runtime/`、`src/composition/`、`tests/runtime/`、`tools/runtime/`。它们由主 Agent 维护。尤其不得重写Loop/Provider、复制会话状态、修改事件语义或自行变更凭据读取。

共享控制/文档仅由主 Agent写：AGENTS、主文档、总TODO、governance、tools/gates和本任务。你的新文件/删除文件及语言策略差异列进交接表，由主 Agent集中登记、更新实际产品构建。不要为了门禁通过修改共享规则。

用户通过两个对话协调。不要创建分支、提交/推送共享工作树、reset/clean/stash或全仓格式化。先看git status，保留不属于你的未提交修改；独占文件发生并发修改时停下该文件并报告，不覆盖。产物放/tmp。主 Agent负责提交main。

## 按此顺序做，完成一项在交接报告勾选

- [ ] 读取本地Pi `packages/tui/src/terminal.ts` 及对应输入/UTF-8处理实现；读当前TUI头文件/测试，记录实际参考路径与提交。deepseek-harness只参考服务/生命周期分离，不移植TS代码。
- [ ] `tui_utf8.c` 原地替换为手写 `tui_utf8.S`：实现四个现有导出。覆盖空输入、1～4字节、尾部不完整、退格定位、所有截断边界；按现有契约，不擅自扩大成完整Unicode字素库。
- [ ] `tui_input.c` 原地替换为 `tui_input.S`：单行输入状态、UTF-8 code point退格、长度拒绝、Ctrl-C/D/U与ESC状态；保持上限与返回枚举。不同时扩多行/补全/历史等功能。
- [ ] `tui_state.c` 原地替换为 `tui_state.S`：busy拒绝、取消仅请求、唯一终态、有限poll、fatal传播。不得生成自己的模型/工具事件。头中普通状态查询helper若保留代码，应一起转为汇编导出或明确只保留常量/声明。
- [ ] `tui_view.c` 的渲染流程和有界文本处理迁到 `.S`；可从汇编调用已登记的libc函数，但不能把渲染算法整体回调给遗留C。工具/错误上限、UTF-8 carry、终态展示必须保持。不新增主题/动画/Markdown框架。
- [ ] `tui_app.c` 的事件推进、输入派发与生命周期控制迁到 `.S`；若termios/sigaction等平台结构需要薄C封装，可保留最小封装，清楚列出其导出及职责。主循环/输入/状态机不能藏在封装里。
- [ ] 审视 `tui_term.c`：平台结构适配允许最小C，其余可直接实现的原语优先汇编；不要为了“全汇编”自研TLS或引入脆弱的硬编码glibc私有布局。
- [ ] 每个模块迁移后，旧C源不再存在、不再编译，测试仍通过；同步本地客户端构建脚本。完整产品构建与共享登记留给主 Agent，交接中给出准确差异。
- [ ] 完成ABI、边界、原客户端用例以及终端恢复验证，交付可集成状态，停止扩大范围。

先逐模块保证正确，再推进下一模块。某项没有完成就保持未勾选，不能以汇编入口数量或文件数代替真实迁移。没有第二个实现的切实需求，不建抽象框架。

## 汇编实现要求

目标Linux x86-64，GNU assembler/Clang现有工具链；推荐GAS Intel语法 `.intel_syntax noprefix`。手写 `.S`，C声明、显式整数宽度和C ABI互通。使用RIP-relative访问数据，必要的 `.type/.size` 与 `.note.GNU-stack`，不产生可执行栈。

遵守SysV AMD64参数/返回、callee-saved寄存器、栈对齐和DF清零；不假设回调保留caller-saved寄存器。结构偏移应由公开C布局派生或用编译期sizeof/offsetof断言验证，不能悄悄修改ABI配合硬编码偏移。默认x86-64基础指令，不新增无检测的AVX要求。调用C库只能是必要外部接口，不能委托整段核心算法。

汇编访存不由UBSan/ASan完整覆盖。针对实际缓冲区读写边界补哨兵/保护页和零长/满容量用例；不扩大成无关的通用门禁项目。ABI测试主动破坏caller-saved寄存器，并检查被调用方保存寄存器和栈。不能修改已有期望使错误实现通过。

## 参考与验收

行为来自现有头文件、现有测试和固定Pi；它们冲突时记录具体差异，先保持当前已支持的交互，不照抄上游新增功能。阅读已有 `src/` 中适用的手写汇编可以学习ABI/工具链，但不把旧实验探针整包复制成新的“原生框架”。`references/`不修改、不进产品构建。

至少执行：客户端原五组测试；新增汇编实现测试；ABI/边界专项；有条件时运行主 Agent的 `python3 -B tools/runtime/test_pangu_pty.py`（若共享构建列表尚未切换导致无法编译，交接准确记录，不改共享脚本绕过）。真实测试使用现有凭据环境，不能把key写入仓库或报告，不要求用户重复提供。

主 Agent最终执行真实MiMo TUI工具往返、收到文本后取消、Ctrl-D/SIGTERM/SIGWINCH退出及完整termios比较。不要把纯进程exit0写成终端恢复已验证。若你能用独占构建先跑这些测试，记录实际命令；未执行保持NOT_RUN。

## 交接报告只写一份

列出：完成项；每个 `.c → .S` 的删除/新增和导出；仍保留C胶水的逐文件原因；构建输入和语言策略应如何调整；真实命令/退出码；ABI/边界/PTY证据；已知问题。不要另留源码快照、临时兼容分支或旧C版本。主 Agent统一审查共享控制差异、切换产品构建并提交。完整P1/T28和生产认证未实现的部分不得声称完成。
