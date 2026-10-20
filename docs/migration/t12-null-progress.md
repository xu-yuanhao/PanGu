# T12 Pi optional-null 归一化本地实验

2026-10-20，T12 IN_PROGRESS。实施 Agent 根据用户授权自审自批固定包，仅用于本地实验；同一实施者与审查者，不作为独立审核或生产放行。

[候选登记](t12-null-candidate.json) · [实现与证据](t12-null-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-pi-null-0er55qjr`，最终 request-v2.json / controller-v2 / candidate-v2 / build-v2。控制树 SHA256 `7463d318fdb1cc17aab5333963e8fa37c4df2c3a4874f54fbb421e4e21c4dc07`。56个原生文件已归档恢复并复验保留产物绑定；恢复不是重新构建。

## 实现与用途

在既有 pi-arguments 服务增加 normalize.c 与 optional_nulls 导出，使用既有 Schema 服务；原有两种 repair 组合新增 Schema 绑定，没有新服务、模块、构建模式或运行库。所有旧基础、词法修复和最终解析实现不变，仅公共头与原探针扩展。

这是 normalizeOptionalNulls 阶段：只删除可选、自有且其可用子验证器拒绝null的字段；必需和允许null的字段保留。按properties/items递归，不因声明type与值类型不符而提前阻止归一化。支持范围沿用现有严格Schema子集；未知/未支持定义明确拒绝，不能忽略后通过。

**阶段成功不代表参数有效。** 必需null、错误类型、额外字段及其他Schema不匹配可能仍存在，随后还要类型转换和最终校验。输出需重新解析后用于后续阶段，不能执行工具。

重复名称按解码后的UTF-16单元last-wins。删除逻辑字段时删除其全部原始重复项，避免较早的值重新出现；未改动子树可以保留原始词法。escaped孤立surrogate、NUL、负零、溢出数值和超大整数词法不被替换。输出只承诺JSON逻辑值，不承诺与JS stringify的字节、空白或键序相同。

## 实测发现的 Pi 缓存边界

Pi 用WeakMap缓存验证器。布尔子Schema不是可用的WeakMap键，缓存失败被getSubSchemaValidator捕获；因此即使Compile(false).Check(null)为false，可选false子Schema的null仍被保留。

初版原生实现按直接Schema拒绝结果删除它，499条对照中的28个输出容量分支暴露差异。已修正为保留布尔子Schema的null，预期与语料未改；失败报告、初版实现和实际缓存反例保留。这很关键：错误删除可能让本应在后续校验失败的字段变成缺失。

oracle为冻结validation.ts的完整字节前缀，加一条export以访问私有normalizeOptionalNulls；函数实现没有改动。这是明确记录的访问插桩，不冒称未修改模块的公开导出。原文件/副本/后缀及TypeBox包文件摘要已记录，references未修改。完整validateToolArguments结果另作参考记录，不能计作当前原生转换兼容通过。

## 资源和失败契约

调用者提供成功且匹配的不可变Schema/value视图、u64删除计划暂存区、输出容量、written和report。四种可写范围两两不重叠，并与两侧视图/结果/输入/公开token范围分离。参数或结果头拒绝保持report、输出、written不变；运行失败可改变计划暂存区和report，但保持输出字节与written不变。NULL/零输出支持测量，无隐式NUL，无分配、产品递归或I/O。

固定64帧计划及发射栈；节点/深度限制约束Schema与访问路径，未改变子树可直接复制而不访问其后代。总work累计Schema预检、nullable子校验、名称/删除计划查找、遍历和复制字节；子校验仅获得剩余work。宽对象及计划查找可能二次复杂度，但比较前消耗配额。这是抽象工作量，不是CPU、栈、RSS、延迟或体积预算。

先完成全部可失败的计划和测量，再发射输出；发射在源和工作区独占不变的契约下重放已验证路径，不再次扣除配额。调用者仍负责存储、对齐、生命周期及借用失效；范围检查不认证指针，也不能机械撤销复制出去的旧视图。

登记曾沿用词法阶段“失败时所有输出不变”的过宽概述。controller-v2细化了已有各接口的实际契约：repair字节/长度原子性、final解析清空发布结果并使用可丢弃暂存、当前归一化输出原子性但计划/report可变。原生源码与第一冻结候选一致，没有改变旧实现、测试预期、工具链、门禁或预算。

## 实际验证

- 499条插桩Pi归一化对照，覆盖必需/可选/nullable/enum/const/boolean、嵌套properties/items、四工具、重复/Unicode等价名称、NUL/孤立UTF-16名称、额外字段与固定随机组合。
- 宿主1980种容量；原生每条遍历零至所需容量+1，验证失败原子性、尾部哨兵与重新解析后的逻辑值。语义比较包含精确binary64位、UTF-16单元和自有成员last-wins；参考输出保留负零和Infinity，不依赖有损stringify。
- 所有正work配额至成功需求+1、删除计划不足、布尔缓存边界、未知Schema、别名/失败结果头拒绝，以及四片保护页末端的Schema/value/输出/report通过。
- 普通36项冻结构建、48次断网正常执行，4项保护页正对照139、12项私有参数拒绝247通过；旧JSON/Schema/词法/最终解析及数字格式化全部四分片保留。
- 补充UBSan同样36项构建、48次正常执行，4项保护页和6项违规移位对照符合0/139/132；只有text-probe正对照分支与普通源不同。不是正式sanitizer或汇编全覆盖。

## 后续

下一步实现TypeBox/plain Schema转换，必须区分模式：TypeBox integer会截断成功转换的number，TryArray会包装非数组，TryString不把任意对象/数组直接String化；不能用自定的“严格转换”替代。实际转换须另建完整Pi差异证据。

完整validateToolArguments、Provider完成事件接线、流式临时展示、JS stringify/wire字节、错误文案、原始孤立UTF-16输入、完整T12/G07和受保护服务器准入仍未完成。native_matrix=PASS与release=BLOCKED同时保留。

源码已公开至 [`codex/t12-pi-optional-null`](https://github.com/xu-yuanhao/PanGu/tree/50914fc8f7fbcca9fbff1287b2661913cd373b85)，固定提交 `50914fc8f7fbcca9fbff1287b2661913cd373b85`。发布副本全部preflight输入与固定候选一致，远端ref已回读核对；[发布记录](t12-null-source-publication.json)不代表生产准入。
