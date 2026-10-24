# 工具核心汇编重构交接报告（第二位 Agent → 主 Agent）

日期：2026-10-25。任务：`docs/并行开发任务提示词-工具核心汇编重构.md`。
状态：工具注册表与工作目录只读 read 已全部原地替换为手写 x86-64 汇编并删除
旧 C；公开布局/契约未动；独占驱动 12 组运行（O0/O0+UBSan/O2/O2+UBSan × 三组
测试）全部通过。共享登记与产品构建切换留给主 Agent（差异见 §3）。

## 1. 实际参考与采用/差异（提交/路径/函数 → 语义 → 原生差异）

固定参考：Pi `38ff99177fba21e9c57a670f4e95adcf407c99c7`（HEAD 与任务指定一致，
未移动）、Harness `22ddee3792402c3e70ec5f86a63c3e3e895a2f60`。

- Pi `packages/durable/src/harness/tool.ts`（ToolTask.phases.call：readCall →
  resolveTool → prepare/validate → beforeTool → execute → afterTool → settle，
  单个 handler 内完成，错误 settle 为 invalid/unavailable 结果）与
  `registry.ts`（RegistryState 不可变快照；扩展重名/任务重名抛错）、
  `define.ts`（defineTool 恒等定型）；测试 `test/harness-tools.test.ts`
  （describe "tool round"/"tool results"/"tool execution api"）、
  `harness-read-only.test.ts`（只读会话拒绝写入且保持可用）。
  采用：解析→校验→授权→执行次序、失败不再触发后续回调、重名拒绝、注册表
  不可变（初始化后无增删）。差异：Durable 的 ToolTask/检查点/重放/
  nested calls/live slot 属任务与恢复边界，未搬入注册表（任务明确禁止）。
- Pi `src/harness/generation.ts`（工具轮次/结果提交归 Loop/任务层）：仅确认
  边界，未复制任何内容到工具注册表。
- Pi `src/tools/read.ts` + `truncate.ts` + `path-utils.ts`（readHead 分块、
  DEFAULT_MAX_BYTES/LINES 截断、路径变体解析、图像）：本子集只保留“读一个小
  普通文本文件”；offset/limit/行扫描/图像/路径模糊变体均不采用（现有头文件
  契约即如此，任务禁止扩大）。
- Harness `packages/core/tools/src/index.ts` + `docs/tool-execution-pipeline.md`
  （ToolView.visible 与 restrictions 分离、可见性来自注册、执行经 scoped
  executor）：采用“可见性≠授权”的对应关系——`pg_tools.authorize`（原 C 的
  available）只做存在性查询，策略校验在每工具 authorize 回调。差异：Harness
  的 scope 层/allow-deny 编译/激活条件不在本子集。
- 旧 Coding Agent `packages/coding-agent/src/core/tools/read.ts`：仅浏览确认
  无额外必要行为；未采用图片/行区间/分页。
- 平台核对：`<fcntl.h>`/`<sys/stat.h>`/json-c 枚举全部经编译期
  `_Static_assert`（read_tool_layout.h/tools_layout.h）与一次性探针程序确认，
  未猜任何值；`struct stat` 仅用 `st_mode`（偏移 24，公开 ABI）。

## 2. 完成项勾选（任务清单）

- [x] 工作树检查（主 Agent 的 agent/journal/session 改动只读未碰）、参考与
  契约阅读（§1）、差异列出。
- [x] 布局断言：`tools_layout.h`（slice/definition/registry/pg_tools/枚举）与
  `read_tool_layout.h`（read_tool、open 标志、stat 模式位），C 侧
  sizeof/offsetof/枚举逐项断言，不修改任何真实布局。
- [x] `tools.c` → `tools.S`：全量验证后提交（事务性）；16 上限、名称字符集、
  重复拒绝、描述长度、必需回调、完整 JSON object schema 且 type 为字符串
  "object"；失败不改原 registry；**支持 defs 与目标 registry 别名/重叠**
  （与被替换 C 的本地 candidate 语义一致，含回归用例；初版汇编曾丢失该语义，
  测试暴露后修复）；count==0 合法清空。
- [x] 查找/服务函数表/执行流程：未知工具拒绝；完整 JSON object 检查在前，
  validate → authorize → execute 次序，失败不触发后续回调（原测试的 order
  断言通过）；available 仅存在性。借用定义/字符串/context 生命周期由调用方
  保证（契约原样）。
- [x] json-c 保持外部调用（tokener/parse_ex/parse_end/get_error/is_type/
  get_ex/get_string/get_string_len/object_length/put/new/free），无第二套
  JSON；完整输入、尾随垃圾、嵌入 NUL（显式长度切片）、非 object、尾随空白
  均与原行为一致。
- [x] `read_tool.c` → `read_tool.S`：根目录 fd 生命周期（open/DUPFD_CLOEXEC/
  close 幂等）、JSON path 语义（恰好一个字符串成员、非空、相对、无 NUL、
  分量非空且非 "."/".."）、逐分量 `openat`（中间 O_DIRECTORY、全程
  O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK）、逐 fd 关闭、fstat+普通文件判定、
  读满 cap-1 后额外一字节超限检测、严格 UTF-8（过短/过长/代理区/连续字节）、
  NUL 二进制拒绝。
- [x] 路径防护未退化：绝对路径/空分量/点段/嵌入 NUL/任意层符号链接（含中间
  目录 symlink）/非普通文件全部拒绝（原测试 + 新增边界）；无字符串拼接 open、
  无 realpath、NOFOLLOW/O_DIRECTORY/非阻塞保留。
- [x] fd 释放：目录 fd/最终 fd 在各错误路径关闭（refused 统一出口）；close
  幂等；容量协议保持（成功长度严格 < capacity；cap<2 → invalid read path，
  与原一致）；零长度、cap 0/1/2、恰好 cap-1、多一字节、已关闭根均有用例。
  EINTR 语义：与被替换 C 一致——`read`/`openat` 返回 -1 不重试、按拒绝处理
  （O_NONBLOCK + 先 fstat 过滤非普通文件，常规文件不会 EAGAIN）；未把错误
  无限重试。
- [x] 旧两份 C 已删；无 C 胶水新增（fstat 直接经公开 libc 调用 + 布局断言，
  未建 read_posix.c）；独占驱动更新并给出主构建精确源列表（§3）。
- [x] 原测试 + 新增边界/ABI 测试真实执行（§4）；导出/导入核对：tools.S 导出
  仅 `pg_tool_registry_init`/`pg_tool_registry_service`，read_tool.S 导出仅
  `pg_read_tool_open/close/definition`；未定义导入仅 json-c 与 libc
  （strlen/memcmp/memcpy/strcmp/memchr、open/openat/close/fcntl/fstat/read），
  无任何指向已删除 C 核心的符号。

## 3. 主 Agent 需要的登记差异（未由我执行）

`governance/bootstrap-policy.json`（G00）：

- 移除登记：`src/runtime/tools.c`、`src/runtime/read_tool.c`
- 新增登记：`src/runtime/tools.S`、`src/runtime/tools_layout.h`、
  `src/runtime/read_tool.S`、`src/runtime/read_tool_layout.h`（native-source）；
  `tests/runtime/test_tools_asm.c`（native-test）；
  `tools/runtime/test_tools_asm.py`（development-tool）；
  本报告如需入库按 document 登记。

`governance/assembly-first.json`：

- 删除 `src/runtime/tools.c`、`src/runtime/read_tool.c` 的
  `pending-inplace-assembly` 条目（连同 sha256）；
- 新增 `src/runtime/tools.S`、`src/runtime/read_tool.S` 为 `assembly-core`；
  两个 `*_layout.h` 为纯常量/断言头，如清单要求登记可并入（无实现）。

`governance/runtime-local-modules.json` 与产品/测试构建输入：

- tools 模块实现文件：`tools.S`（新增 layout 头为 include 依赖）；
- read_tool 模块实现文件：`read_tool.S`；`read_posix.c` 未创建（无需）；
- `tools/runtime/build_test.py` / `build_pangu.py` 经登记表自动生效
  （`implementation('src/runtime/tools')` 将解析到 `.S`），无需改脚本本身。

G00 当前其余失败项（agent/journal/session 等）属主 Agent 并行工作，未触碰。

## 4. 验证证据（实际命令、编译器、退出码）

- `python3 -B tools/runtime/test_tools_asm.py` → 退出码 0。
  clang-23（`/usr/bin/clang-23`）；每组 `-Wall -Wextra`：
  - 汇编对象 O0 与 O2 各一套；
  - `tests/runtime/test_tools.c`（原用例，未改期望）×4 配置 rc=0；
  - `tests/runtime/test_read_tool.c`（原用例，未改期望）×4 配置 rc=0；
  - `tests/runtime/test_tools_asm.c`（新增）×4 配置 rc=0：
    汇编回调（validate/authorize/execute 保存实参后破坏全部 caller-saved；
    execute 验证 rcx/r8/r9 实参与 cap/size 在服务层 [rsp+8]/[rsp+16] 的栈传
    位置、写标记、size 回写）、ABI 探针（callee-saved 哨兵 + caller-saved
    垃圾 + 独立 scratch 区）、sret 聚合（pg_tools 24B、pg_tool_definition
    56B）、注册表边界（名 1/63/64、描述 4095/4096、16/17 项、失败事务性、
    别名/重叠、空 registry、schema 各拒绝分支、尾随空白允许）、参数严格性
    （尾随垃圾/嵌入 NUL/非 object/超长/空 out/空 size*/查找前缀与 NUL 名）、
    read 边界（cap 0/1/2、恰好/超一字节、零字节、8 层深 + UTF-8 组件、空中间
    分量、根目录本体、关闭根、幂等 close）、保护页（输出缓冲尾贴 PROT_NONE
    ×三容量组合、参数切片尾贴保护页）。UBSan 配置仅作用于 C 测试代码（汇编
    访存不被其覆盖，已按任务用保护页/哨兵补充）。
- 导出/导入核对：`nm -g --defined-only` / `nm --undefined-only` 输出记录于
  驱动日志（§2 末条摘要）。
- `python3 tools/gates/check.py`：FAIL（预期中间态）——仅 §3 所列登记差异 +
  主 Agent 并行文件；未修改门禁。
- 共享 `build_test.py`/`test_pangu_pty.py`/真实 MiMo：**NOT_RUN**（登记表仍
  指向已删除的 .c，装配门禁会拒绝；按任务不改共享脚本绕过，集成后由主 Agent
  执行）。

## 5. 与被替换 C 的行为差异

无有意差异。实现过程修复的自有缺陷（均已含回归）：初版丢失 defs/registry
别名支持（candidate 语义已恢复）；`li_available` 少一次对齐压栈（已修）；
read 路径 `openat` 结果存 rcx 被 syscall 摧毁、`parse_path` 的 r14 双用、
`last` 标志存 r8 被调用破坏、`open` flags 误放第三参（均已改栈槽/正确参数）。

## 6. 已知问题与需主 Agent 处理事项

1. **TUI 批次探针潜伏缺陷（移交提醒）**：`tests/client/tui/test_tui_abi.c`
   的 `abi_probe1/2/3` 把参数写到 `[rsp+16]/[rsp+24]`，覆盖压栈保存的
   r15/r14 槽位，返回时破坏调用方 callee-saved（当时仅因编译器未依赖而通过；
   本次同构探针当场崩溃暴露）。该文件现属主 Agent，建议按本批
   `test_tools_asm.c` 的"独立 scratch 区"模式修复。
2. 深目录清理等测试夹具在临时目录（/tmp/pangu-read-*），不触用户项目。
3. 本批不包含：write/edit/bash、动态注册/卸载、异步工具、Schema 执行器、
   Pi read 的行区间/截断分页/图像——均按任务留给后续计划；不声称完整
   T24/T25 或 Durable 工具任务兼容。

完成即止：不再写其他文件，等待主 Agent 复核集成。

## 7. 主Agent集成回执

2026-10-25：已复验独占驱动12次运行并完成文件/语言/模块登记，产品和共享测试实际使用tools.S/read_tool.S，旧C已删除。新增专项纳入主build_test.py（O0/O2 UBSan），没有降低断言或切回旧实现。主Agent仅精简实现中的迁移说明注释，并补一个实际缺陷：schema.type原先经strcmp会把JSON字符串`object\u0000extra`当作object；现在先要求decoded字符串长度恰为6，再比较内容，新增拒绝回归。这是本次明确的语义修复，原普通object与尾空白用例保留。

TUI ABI探针问题已确认并修复：参数使用独立32字节scratch，不覆盖保存的r14/r15。新增外层自检核对探针自身的callee-saved；将旧重叠栈槽作为临时负例编译运行，自检确实拒绝。不是用编译器偶然未使用寄存器来代替ABI证据。

完整工作区G00、客户端六组、核心十一组O0/O2、实际产品无网络PTY与真实MiMo read续答/取消/精确termios恢复已通过。runtime_bridge另在同批由主Agent原地迁汇编并接入初始run_started观察。剩余MiMo/SSE C核心仍明确待迁移，平台胶水保留原理由；不声称完整T24/T25/Durable或生产认证。
