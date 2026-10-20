# T12 Pi 字符串词法修复本地实验

2026-10-20，T12 IN_PROGRESS。固定包由实施 Agent 根据用户授权自审自批，仅用于本地实验；同一实施者与审查者，不作为独立审核或生产准入。

[候选登记](t12-repair-candidate.json) · [实现与证据](t12-repair-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-pi-repair-a7_egn0h`，最终 request-v2.json / controller / candidate-v2 / build-v2。控制树 SHA256 `8383620bf6b774c1b9db2ef64c618ab8ae5aabdfefd8b92e8ff8074fc2e7f476`。54 个原生文件已归档恢复并核对保留产物绑定；恢复不是重新构建。

## 实现与边界

新增 pi-arguments 兼容模块及 compat.pi-arguments/1，一个 C 文件实现 pangu_pi_repair_utf8。它依赖既有 text 服务；单独保留 Pi 容错边界，严格 JSON 服务不变。原有 49 个原生文件字节一致，新接口、实现及探针共 5 个文件；没有堆分配、libc、I/O 或新框架。

按冻结 Pi 的 repairJson：字符串外原样复制；字符串内 raw control 转短转义或小写 u00XX；合法反斜杠转义及四位 Unicode 转义原样保留；非法或末尾反斜杠加倍，不吞掉后续字符。输入必须是完整合法 UTF-8，escaped 孤立 UTF-16 单元保留其 ASCII 词法；不替换非法 UTF-8，也不规范化 Unicode。

**词法修复成功不表示 JSON 有效或允许执行工具。** 它不补齐引号、容器或数字，不处理空白转空对象、不验证根对象、不做 Schema 转换。截断字符串经此函数仍可截断。后续最终参数适配必须严格解析原文，仅在语法失败时尝试修复，并对结果进行完整解析、非 null 非数组对象检查及 Schema 检查；UI 的 partial parse 不得授权执行。

## 有界与失败契约

调用者提供不可变源、目的容量、输入字节上限和对齐的 u64 written。NULL/零容量目的表示测量模式。先验证 UTF-8并完整测量，再检查容量、输出；没有隐式 NUL。失败保持目的与 written 不变，源/目的容量/长度输出须按接口约定不重叠。0 成功，1 参数/重叠/范围错误，2 输入/容量/大小超限，12 非法或截断 UTF-8。

扫描状态和六字节临时片段为固定空间，输出最多六倍；无递归，工作量随输入和输出线性增长。调用者负责存储有效性、对齐、生命周期和源不可变性；算术检查不认证原生指针。这不是正式性能、栈、RSS 或体积预算。

## 实际验证

- 3,364 条不同输入与实际冻结 Pi repairJson 逐字节对照；包含全部 ASCII 原始/escaped 字符、非法 u 转义、Unicode、固定随机种子和选定文档的全部前缀截断。
- 同时记录 Pi parseToolCallArguments 的 1,156 条接受与 2,208 条拒绝，供下一步最终解析使用；这两组仅是参考运行，当前原生词法函数没有验证这些接受/拒绝结果。
- 宿主补充验证 13,441 种容量和 10 项显式 UTF-8/配额/别名拒绝；原生逐条遍历 306,573 种容量，检查失败原子性、精确输出与尾部哨兵。
- 36 项冻结构建、48 次断网正常运行通过；4 项保护页正对照返回139，12项私有参数拒绝对照返回247；1 MiB 最坏膨胀、页尾输入与输出通过。
- 补充 UBSan 36 项构建、48 次正常运行、4 项保护页和6项违规移位对照符合0/139/132。仅 text-probe 正对照分支与普通源不同；它不是正式 sanitizer 准入或汇编全覆盖。
- 旧 JSON/Schema 探针全部保留，16500条数值格式化的四个分片仍按各后端/模式完整运行。

准备时登记脚本误用 services 字段，在序列化及构建前失败，已改为实际 service_definitions；candidate-v2 为别名测试缓冲区明确添加对齐，修复实现不变。初稿与日志保留。门禁、工具链参数、既有测试和预算均未放宽。

## 待完成

最终工具参数解析、严格优先/失败后修复、ECMAScript 空白语义、根对象限制和明确 final/error 状态，随后 optional-null、TypeBox/plain Schema 转换仍待实现。原始孤立 UTF-16 输入、完整 Provider/wire/session、系统化 fuzz、完整T12/正式G07/受保护服务器准入未完成。native_matrix=PASS 与 release=BLOCKED 同时保留。

源码已公开至 [`codex/t12-pi-repair`](https://github.com/xu-yuanhao/PanGu/tree/7b21806f2a034cd90f4d3537b05c2883e1564077)，固定提交 `7b21806f2a034cd90f4d3537b05c2883e1564077`。发布副本的全部 preflight 输入与固定候选一致，远端 ref 已回读核对；[发布记录](t12-repair-source-publication.json)不代表生产准入。
