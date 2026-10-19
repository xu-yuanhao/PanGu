# T12 有界结构化 JSON writer 本地实验

2026-10-20，T12 IN_PROGRESS。实施 Agent 按用户「以后改为Agent自批自审」完成自审，批准本固定包用于本地实验；不声称独立审核或生产准入。

[固定候选](t12-writer-candidate.json) · [源码与证据](t12-writer-evidence.json)。外部目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-writer-fblocegt`。最终 request-v6.json / controller-v4 / candidate-v6 / build-v6；source-v6.tar 包含 44 个原生源文件，恢复后的输入库存与保留产物已核对。固定控制树 `74020d49c1b57e7f5351086984c3798b1c7953ca160b968671a85905bab97757`。

## 能力与 ABI

现有 foundation.json 增加 write.c，新增 writer_init / writer_emit / writer_finish 三个导出，没有新模块、服务 ID、依赖、分配器或构建组合。原 reader、quote、number 及 parser 扫描逻辑未改动；parser init 新增了对既有首次完整清零契约的拒绝检查，旧结构尺寸断言继续通过。新增 writer 固定 560 字节，包含 64 个八字节容器帧及元数据；调用者提供互不重叠的 writer 与固定输出区。

操作支持对象/数组开始与结束、UTF-8 字段名/字符串、严格数字字面量、true/false/null。除字段名、字符串、数字外的操作必须传 NULL/零长度参数。自动处理逗号、冒号及字符串转义；不提供任意 JSON 片段注入。数字由现有严格 parser 在固定局部存储内验证，必须是覆盖整个参数的 NUMBER token，拒绝空白包裹、截断或拼接多个值。

对象保留显式写入顺序及重复字段；数字保留原词法，例如 -0、9007199254740993、1e400。这个接口不等同于 JSON.stringify(Number(...)) 的浮点格式化，也不模仿 JS 对象的整数 key 排序或属性覆盖。消费者按已实现 reader 的 decoded-key last-wins 语义读取重复字段；完整 Pi session/wire 兼容尚未完成。

## 状态、存储与失败

只能写一个根值。对象必须先写 key 再写 value，数组只接受 value；有待写 value 的对象不能关闭，不匹配的结束类型被拒绝。深度上限由调用者设为 1..64。finish 只在根完整且所有容器关闭时发布显式长度借用切片，并封住后续写入；未完成返回 13，输出 view 不变。

每次调用先验证参数、活动状态、输入存储不重叠、深度与容量，再写入数据并提交状态。容量/深度/数字配额返回 2；非法参数/重叠 1，状态或顺序错误 10，非数字 literal 9，UTF-8 12，数字语法 14；底层 quote 的算术溢出 11 可向上传递。被拒绝的调用保持所有 writer 元数据与输出字节不变，随后仍可执行合法操作。

这是逐调用失败原子性，不会撤回此前成功写入的半份文档，也不会自动预留所有结束括号。输出区固定，不暗中增长或分配临时整串。调用者仍须保证存储有效、输入在调用期间不可变并串行修改同一 writer。init 重置元数据、使旧借用失效，不清空旧字节；不能把半成品或失效 view 当作已提交结果，也不声称能认证伪造指针/元数据或实现原生隔离。

## 实际验证

最终 28 项冻结构建、保留产物复验和断网正常运行通过；继承的 4 次保护页正对照均为预期 139。JSON 两种内存后端各跑 debug/release，保留此前所有语法/分片/reader/quote/数值测试，并新增：

- 140 组 Python json.dumps / Node JSON.stringify 精确字节共识，1,389 个写入操作，遍历每组输出容量 0..n+1，共 6,416 个容量变体。失败核对全部 writer 字节、完整容量内输出和尾部哨兵，成功核对字节、长度、解析结果及封住后的拒绝行为。
- 全部 1,315 条数值对照分别检查少一字节容量时原子拒绝、逐字节保留原词法及重新读取的 binary64 位模式。
- 错误顺序、未知操作、错误关闭、未写完 key/value、非法 UTF-8、数字注入/空白/截断、存储重叠、数字超额；拒绝后仍可合法完成。
- 重复 key 的实际 last-wins 读取、64 层成功/第 65 层拒绝、1 MiB 字符串与数字成功、数字超限一字节拒绝。
- writer 的 560 字节状态和两字节字符串输出分别紧贴保护页末端，少一字节容量时不写输出。

source-v6.tar 恢复后的所有 preflight 输入与最终候选一致，28 个保留产物绑定复验通过。生成器记录初稿；数字扩展及生命周期修正记录于 final-test-extension.json / ownership-review.json。最终权威输入是 candidate-v6 和归档，复验应使用新目录。

补充 UBSan：`/home/xu-yuanhao/Documents/PanGu-artifacts/t12-writer-ubsan-1ussm9nm`，28 项构建/正常执行、4 次保护页对照和 6 次违规移位对照符合 0/139/132。普通/插桩候选的生产源相同，仅 text-probe 正对照分支不同；正式工具链/门禁未变，不代表正式 sanitizer、ASan/TSan 或汇编全覆盖。

当前 JSON 探针 ELF 约 4.49～4.56 MB，含大量只用于测试的固定语料，不是 Agent 体积。无正式吞吐、RSS、完整调用链栈预算或 G07 验收结论。

## 控制面审查与剩余工作

自审核对了每个固定输入/回执摘要、状态机、容量算术、不可变源和借用边界。修正 JSON registry 中从 Buffer 沿用的分配器生命周期措辞。准备过程中，清单曾拒绝新控制器副本里意外生成的 Python 缓存，线程枚举也拒绝自由文本描述；保留原 single-thread 枚举，只删除生成缓存后冻结新候选，没有放宽检查。生命周期说明修正时生产实现未改动；这些准备拒绝不计入通过证据。随后初始化契约复核的代码修正另见下文。

approval-v6.json 明确记录实施者与审查者为同一 Agent。完整 T12、binary64 数字格式化、孤立 UTF-16 surrogate 输出策略、Schema、Pi 参数修复/强制转换、受保护控制版本部署与生产发布仍未完成。

源码快照已发布至 [`experiments/t12-json-writer`](https://github.com/xu-yuanhao/PanGu/tree/d163091ad21fa4f8098d98604615844c5078417f)，固定提交 `d163091ad21fa4f8098d98604615844c5078417f`。发布副本全部 preflight 输入与自审候选一致，远端 ref 已回读核对；[发布记录](t12-writer-source-publication.json)不等同于受保护主线或生产发布准入。

## 初始化契约复核与最终版本

初版实验提交 `5ec146ba25cd5b04479931a7ee1428ea2a51ab1b` 已被最终快照取代。初版在 writer 的局部 parser 中只清零头部；虽然旧运行时接受且测试通过，却违反已有接口的首次完整清零约定。最终 v6 改为完整字节清零，并在 parser init 的空头部首次使用路径检查剩余字节，发现脏存储则返回 1 且不初始化；合法 parser 重置不要求清除旧帧。新增测试覆盖脏 reserved 字节拒绝、干净首次初始化和带旧帧的有效重置。它仍不认证伪造的有效头部或指针。

最终 28 项构建/运行、全部 writer 及旧 JSON 测试、归档恢复、UBSan 均在 v6 源码上重新完成。控制树摘要相同不意味着可以沿用旧实现产物；最终候选源码摘要、approval-v6.json 及回执均见 evidence。旧 approval.json / evidence.json 与发布记录原样保留，但 v4-superseded.json 已标明失效，不作为当前批准依据。
