# T12 Pi 最终参数对象解析本地实验

2026-10-20，T12 IN_PROGRESS。实施 Agent 根据用户授权完成固定包自审自批，仅用于本地实验；同一实施者与审查者，不伪称独立审核或生产通过。

[登记](t12-final-candidate.json) · [源码与证据](t12-final-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-pi-final-s1catmg1`，最终 request-v2.json / controller / candidate-v2 / build-v2。控制树 SHA256 `1fe00238d5fba69ddb6c628f968ba9b66b66c90370fc3e29c8264cc307127380`。55 个原生文件已归档恢复并复验保留产物绑定；不是重新构建。

## 行为与接口

现有 pi-arguments 兼容模块新增 final.c、parse 导出及 64 字节结果，使用既有 JSON 服务；两种原有 repair 测试组合增加 JSON 绑定，没有新服务、模块或构建模式。旧 repair_utf8 实现和签名不变，所有基础实现不变；仅公共头及原有两个探针文件扩展，新增一个实现文件。

final=0 对任何文本返回 incomplete 13，清空已发布 input/token/count，不解析临时对象。final=1 先限制原始字节并检查 UTF-8/ECMAScript 空白；全空白返回静态借用的 {}，非空白则严格解析原文。只有严格语法错误或 incomplete 才考虑词法修复；配额与 UTF-8 错误直接失败。变化后的文本必须符合 scratch 和规范化字节上限，再完整严格解析。只有非 null、非数组 OBJECT 成功；不补齐截断引号、容器和数字，不采用 partial parse 前缀。

空白字符表为 TAB～CR、space、A0、1680、2000～200A、2028/2029、202F、205F、3000、FEFF；排除85/180E/200B。仅用于判定“全空白”，不为非空白 JSON trim；BOM+{} 仍应失败。数字词法、负零、溢出、重复成员和 escaped 孤立 surrogate 由既有 JSON token 保留。

调用者传入 final 标志不是 Provider 完成事件的证明；Provider 流程的实际接线仍待实现。成功也不代表 Schema 校验、参数转换或工具执行已获授权。

## 资源、错误与借用

源、整个 scratch 容量、token 容量及64字节结果须两两不重叠；容量乘法和指针范围先检查。参数错误1保持结果不变，调用者必须检查返回值。运行时错误则清空公开 input/token/count，并写入错误状态。scratch/token 是可丢弃暂存区，运行失败可修改它们；任何调用前都应失效旧借用，原始指针不能机械撤销已复制的旧视图。

成功 input 借用原文、scratch 或静态 {}，token 借用调用者数组。调用者负责存储有效性、对齐、生命周期及源不可变；不把范围检查当指针认证。独立固定栈 parser 完整清零后使用，首次清零与 reset 契约未放宽。无分配、递归、libc 或 I/O。

0为完整对象；2为原文/规范化字节、深度、token 或 scratch 配额；9为非对象；12为UTF-8；13为非最终/不完整；14为语法。path 记录严格/修复/空白分支，包括失败时所选分支；original_error_offset 保留考虑修复前的严格错误偏移，JSON偏移对应所选文本。错误字符串未仿制 JS 文案。max_bytes 同时约束原始与规范化文本，设置为1时空白映射 {} 也会超限。

## 实际验证

- 3,443 条实际冻结 Pi parseToolCallArguments 对照：1,190接受、2,253拒绝；每条另检查 nonfinal 不发布。包括此前全部词法修复语料、全部 ECMAScript 空白和反例、文档前缀截断、Unicode/孤立 escaped surrogate、重复key、原型名称自有成员及边界数值。
- 宿主逐条比较选择的源字节/分支；接受结果按 binary64 位、UTF-16 单元、数组及自有成员 last-wins 对象做语义对照。不是依赖 JSON.stringify 把 Infinity/负零等细节抹掉。Node 另断言所选源 JSON.parse 与真实函数结果一致。
- 原生逐条比较接受/拒绝、源字节/分支及新鲜严格解析的完整 token；失败和nonfinal无公开 token。另测规范化字节/scratch/token限额、64/65层、空/数组根、非法UTF-8、别名、参数拒绝结果不变，以及三片保护页末端的源/scratch/结果。
- 普通36项冻结构建、48次断网正常执行、4项保护页对照139、12项私有参数拒绝247通过；旧3364条词法/306573种容量及所有JSON/Schema探针、数值格式化全部四分片继续运行。
- 补充 UBSan 同样36项构建、48次正常执行，4项保护页和6项违规移位对照符合0/139/132；仅 text-probe 正对照分支与普通源码不同。它不是正式 sanitizer 或汇编全覆盖。

首次普通/UBSan debug构建发现测试的64字节哨兵结构体复制被编译器转换为memcpy，门禁拒绝；candidate-v2 改为分别显式填充两个哨兵。解析实现和测试预期不变，原日志保留，没有放宽门禁或增加libc依赖。

## 继续推进

下一步是 optional-null 规范化和 TypeBox/plain JSON Schema 的转换；还需实际 Provider final事件接线、流式临时展示、错误文案、完整wire/session与系统化fuzz。原始孤立UTF-16输入不能无损表示为本接口UTF-8，仍不支持。完整T12、正式性能/栈/RSS/体积预算、完整G07和受保护服务器准入未完成；native_matrix=PASS 与 release=BLOCKED 同时保留。

源码已公开至 [`codex/t12-pi-final`](https://github.com/xu-yuanhao/PanGu/tree/2d78d22fe2ad2791ba714efa37e55df3c44a39ea)，固定提交 `2d78d22fe2ad2791ba714efa37e55df3c44a39ea`。发布副本全部 preflight 输入与固定候选一致，远端 ref 已回读核对；[发布记录](t12-final-source-publication.json)不代表生产准入。
