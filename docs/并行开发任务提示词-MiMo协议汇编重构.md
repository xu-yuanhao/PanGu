# 给第二位 Agent：原地重构 MiMo 协议适配，Assembly 第一

状态（2026-10-25）：已交接并由主Agent审核修复、验证及集成，不再执行本任务。结果见 docs/migration/mimo-assembly-handoff.md 与 docs/migration/runtime-core-progress.md。下文保留原任务范围。

你是 PanGu 的并行实现 Agent。请直接完成 MiMo 请求投影、SSE 解码和请求生命周期的手写汇编重构，实际编译验证后交接，不停留在建议或计划。主 Agent 同时推进 Durable 配置身份、持久会话入口与恢复；共用工作树，必须遵守下面的文件归属。

本任务替代已完成的 TUI/工具重构派发。TUI 与 tools/read 已经是手写汇编，不要重复重构它们。当前剩余待迁移的 C 核心是 `src/runtime/mimo.c`。本任务不增加工具、不扩展 TUI，不重写整个模型生态。

## 先读项目与当前状态

工作目录：`/home/xu-yuanhao/Documents/PanGu`。先读根 `AGENTS.md`、`盘古 Pangu 项目完整文档.md` 当前执行决定、`docs/工程门禁与架构守卫.md`、`docs/原生重写路线与迁移TODO.md` 近期队列，以及 `docs/migration/runtime-core-progress.md` 最近的日志/检查点/工具汇编集成记录。

主要行为基座是 Pi Durable，旧 Coding Agent 为辅助；pi-ai 是共享模型协议参考；服务组合与生命周期参考 deepseek-harness/Cordis。当前原生模型接口是可替换 `pg_model` 函数表。模型适配器只负责投影、传输和协议事件，不持有 Session 提交、任务恢复、工具执行、TUI 状态或宿主调度。

语言固定为 **Assembly 第一、C 第二、C++ 第三**：请求投影、字符串/容量检查、SSE 状态推进、工具片段累积、请求开始/轮询/取消/关闭都优先手写 `.S`。可直接调用现有 libcurl/json-c/libc，不增加依赖，不另造 JSON 库。C 只允许确有必要的外部平台结构适配，不能把循环、条件分派和所有权流程搬入 C 后称为胶水；不引入 C++。

原地单版本替换：交付时删除 `mimo.c`，不保留旧 C 对照、备用、兼容路径或测试内副本。回退用 Git 历史。禁止编译器生成汇编、机械逐行转换、汇编壳调用旧 C 核心。注释只解释必要 ABI、所有权和边界。

## 文件归属与并发约束

你独占，可以修改：

- `src/runtime/mimo.c`（完成后删除）、`mimo.h`（保持下述公开契约），新增 `mimo.S`、`mimo_layout.h`。
- 允许按职责拆成最多三份实际需要的汇编源，例如 `mimo.S`、`mimo_projection.S`、`mimo_stream.S`；不要按小函数拆文件。内部符号隐藏，公开导出保持原集合。
- 仅必要时新增 `src/runtime/mimo_curl.c/.h`：只适配公开 `CURLMsg` 等外部结构或确实无法直接表达的接口。逐个列出导出、为何不能直接调用以及它不承载算法的证据。不能把整个 start/next/receive/close 或 JSON 投影留在 C。
- `tests/runtime/test_mimo_stream.c`、`test_projection.c`；按需要新增 `test_mimo_asm.c`、`mimo_abi.S`、`test_mimo_transport.c`。测试名并非要求凑齐文件。
- 新建独占驱动 `tools/runtime/test_mimo_asm.py`，固定实际源列表，产物放临时目录自动清理。
- 唯一交接报告 `docs/migration/mimo-assembly-handoff.md`。

主 Agent 独占，只读不得修改：

- `agent.*`、`agent_layout.h`、`session.*`、`session_layout.h`、`journal*`、`profile*`、全部 `src/contracts/`、`src/composition/`。
- `tools*`、`read_tool*`、全部 `src/client/`、`tests/client/` 及其他运行时测试/真实测试入口。
- `tools/runtime/build_test.py`、`build_pangu.py`、`test_pangu_pty.py`、`tools/client/`。
- AGENTS、主文档、总 TODO、本提示词、`governance/`、`tools/gates/` 与账本。

主 Agent 不会同时修改你的独占文件；它正在修改 Agent/Session 控制结构与桥接生命周期。**不要冻结或硬编码整个 pg_agent/pg_session_control 的旧尺寸。** 本模块只消费 `pg_session` 的消息/系统视图、`pg_model`/event/slice、工具定义列表；根据公开头文件为实际使用的字段做 `offsetof/sizeof` 断言，不复制无关结构。发现消费的公开字段确实变化时报告，不能覆盖主 Agent 的头文件。

冻结 `pg_mimo` 的公开布局和四个公开导出：`pg_mimo_model`、`pg_mimo_bind_tools`、`pg_mimo_encode_request`、`pg_mimo_decode_line`（共四个具名函数；model 返回的 start/next/cancel/close 为回调，不新增公开 API）。保持 `pg_model` 聚合返回、事件/回调 ABI、工具 registry 布局。不要添加环境配置入口、模型选择服务或端点注入产品开关。

保持当前 endpoint、`MIMO_MODEL` 缺省值、请求字段和认证来源。主 Agent 将按这些配置绑定持久身份；本任务不能静默改模型、max_tokens、thinking、工具 schema 或工作目录策略。确需改变契约时先报告具体原因，不越界实现。

先看 `git status --short`；工作区里主 Agent 未提交的 profile/Session/日志/bridge 改动是正常并行状态，不能撤销、暂存或复制覆盖。不建分支、不提交/推送、不执行 reset/clean/stash、不全仓格式化、不修改参考仓库。独占文件出现未知并发修改时停该文件并报告。

共享登记由主 Agent 集成。新文件必须在交接报告列出准确类型、依赖、导出、源列表和删除项。未登记阶段产品主构建/G00可能失败，如实记录，不改检查器或伪装产品源码为测试。

## 先阅读真实参考源码

先记录参考实际提交。本任务选定 Pi `38ff99177fba21e9c57a670f4e95adcf407c99c7`、Harness `22ddee3792402c3e70ec5f86a63c3e3e895a2f60`。如共享 HEAD 移动，用 `git show <提交>:<路径>` 读固定版本，不自行 fetch/checkout。

按具体函数、直接调用和相关测试阅读，不能只看 README 或旧 C 然后自行猜协议：

1. Pi `packages/durable/src/harness/generation.ts` 及 `test/harness-generation.test.ts`、`harness-generation-recovery.test.ts`：模型请求准备、流事件与结束/错误归属；采用边界，不将 Durable 事务/重试复制进适配器。
2. Pi `packages/ai/src/api/openai-completions.ts`：追踪 `stream`、`buildParams`、实际消息/工具转换函数及调用；同时读 `transform-messages.ts` 和相关工具历史测试。函数以固定版本实际代码为准。
3. Pi `packages/ai/test/openai-completions-provider-stream-event.test.ts`、`streaming-tool-arguments.test.ts`、`openai-completions-tool-choice.test.ts` 与 `src/utils/event-stream.ts`。核对片段、终态、工具参数、错误/取消。上游 SDK 隐藏的 HTTP/SSE 细节不等于我们已有实现；缺同构代码的地方明确说明，并查本机公开 libcurl/json-c 头文件/文档，不猜结构/常量。
4. Harness 定位实际模型服务注册、调用及 dispose 代码，只采用服务可替换/生命周期边界。不要用 subagent 模型选择 UI 代替适配器阅读，不为参考新增配置框架。
5. 原生 `mimo.c/.h`、`agent.h` 中 `pg_model`/事件契约、`session.h` 视图、`tools.h`，以及现有两个协议测试。现有实现不自动等于正确标准；确定的缺陷按最小修复处理，保留拒绝约束并补回归。

报告简写“提交/文件/函数及测试 → 采用语义 → 原生差异 → 验证”。当前原生仅支持单 choice/单工具调用、固定容量、非完整 Pi provider 兼容；不为追齐上游引入多工具、图片、thinking、自动重试等功能。

## 按顺序完成，报告中逐项打勾

- [ ] 检查工作树和文件归属，读上述源码/契约，记录采用语义与差异。
- [ ] 用布局头和静态断言固定本模块实际字段、枚举与 ABI；核对公开 curl/json-c 常量，不手抄私有结构。
- [ ] 手写请求投影：system/user/assistant/tool_call/tool_result，文本+call 合并、call/result 配对，完整 registry 工具定义与 schema。禁止把未结束草稿送回模型。保持原配额和非法历史拒绝，生成的请求归本实例所有，失败不泄漏临时 JSON 对象或旧 request。
- [ ] 手写 SSE 单行解码及片段累积：WAIT/TEXT/CALL/DONE、CRLF/空行/注释、JSON 解析完整性、单 choice/index0、有界 id/name/arguments/text、finish_reason 与 [DONE] 次序。decoded NUL、尾垃圾、非法类型、重复结束、结束后数据及未知结束理由按当前有界协议拒绝。
- [ ] 手写接收回调、start/next/cancel/close 生命周期：乘法/容量溢出、跨接收块换行、未完成末行、HTTP 非200、curl 失败、正常流结束、工具参数最终 object 校验。取消/关闭释放 easy/multi/header/request，部分初始化失败也可安全关闭；重复关闭不双重释放，不在活动请求借用已释放缓冲。
- [ ] 工具 CALL 只在完整协议确认后交给 Loop，不因片段 name/id 提前执行；transport 错误不能伪造 DONE 或成功。借用事件缓冲的有效期写清楚；不在 provider 内执行工具或提交 Session。
- [ ] 删除旧 `mimo.c`，独占驱动用唯一实际汇编实现构建，补必要 ABI/边界/传输故障测试；列出 C 胶水（若有）的逐导出理由。
- [ ] 运行专项、原有协议/投影测试、对象导入导出检查；写交接报告并停止扩大范围，等待主 Agent 集成。

接收总量当前是有界整流缓冲，不顺手做无界队列/自动增长/新环形框架；若发现真实容量或解析缺陷，先以当前上限内最小修复和明确失败收尾。不要以“兼容上游”放宽已有安全拒绝或改变正常事件顺序。

## 验证：结果必须来自实际运行

使用当前 Linux x86-64、clang-23、GAS Intel `.S`、SysV ABI、PIE/RIP 相对、非可执行栈；不增加未检测 CPU 指令要求。覆盖聚合返回、栈上传参、callee-saved、每次外部调用前栈对齐、DF、回调破坏 caller-saved。特别核对 curl 的变参调用 ABI，不能凭普通三参数函数调用猜测。

独占驱动至少运行 O0/O2；C 夹具用 UBSan，但明确它不检查汇编访存。容量边界用哨兵/保护页等直接证据。产物与夹具全部临时目录，不读写用户会话。

必须覆盖：

- 原两个测试全部通过；正常请求的 JSON 结构、角色、文本+call、工具 schema 和调用 ID 配对不变；非法历史与长度边界不回退。不要求 JSON 对象键顺序字节一致，但不得忽略字段/类型差异。
- SSE 正常文本、text+tool、工具参数分片、空 usage chunk、合法结束；全部关键输入切分与边界。长度用 JSON 解码后的字节检查，不能用 strcmp 前缀接受嵌入 NUL。
- HTTP/curl 完成、错误和取消、重复关闭、半初始化与分配失败的必要路径；接收乘法溢出、容量0/满、换行跨块、最后一行不完整、重复终态拒绝。
- 使用链接包装器或测试进程中的可控 curl 替身制造运输故障，不修改产品端点/新增跳过 TLS 的测试开关。没有条件覆盖的路径明确 NOT_RUN，不把纯解析测试算真实网络通过。
- ABI 探针自检，故意破坏 caller-saved 的受控回调，页尾输入/输出；`nm/readelf` 确认无旧 C 算法依赖，只有预期外部符号。

执行 `python3 -B tools/runtime/test_mimo_asm.py` 和 `python3 -B tools/gates/check.py`，记录真实命令、退出码和限制。主 Agent 负责最终共享登记、完整核心/TUI/PTY/真实 MiMo 回归。不要为绿色结果放宽断言、基线或门禁。

有预先提供的 `MIMO_API_KEY` 环境时，可运行已有最小真实测试；没有时标 NOT_RUN，主 Agent 集成时补测。不要从聊天、其他进程、shell 历史寻找密钥；不要索取或写入代码、报告、参数、夹具。真实请求限制在必要的单轮/工具往返，不做压测。协议确定性测试用于故障覆盖，不能冒充真实模型验收。

## 交接报告与停止条件

只新建 `docs/migration/mimo-assembly-handoff.md`：

1. 逐项勾选实际完成项，参考阅读、采用语义与差异。
2. 新增/修改/删除文件、准确导出与依赖、公开布局是否保持、C 胶水理由及剩余限制。
3. 给主 Agent 精确登记差异：bootstrap、assembly-first、runtime-local-modules、主构建与专项测试源/链接输入；明确删旧 C 登记，不提供双版本选项。
4. 实际测试命令、退出码、编译器、失败后修复与复验；未运行的单列，不声称完整 Durable/Pi 兼容或生产放行。
5. 待集成事项。完成后停止写文件，不提交、不继续改主 Agent 的持久化或 TUI。

本次交付是可替换 MiMo provider 的原地汇编实现；共享控制面自审、产品组合、真实恢复验证与统一提交由主 Agent 负责。
