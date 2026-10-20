# T12 JSON UTF-16 字符串相等判断本地实验

2026-10-20，T12 IN_PROGRESS。实施 Agent 按用户「以后改为Agent自批自审」完成本固定包自审，批准用于本地实验；不代表独立审核、生产准入或完整 Schema 验证。

[候选登记](t12-equal-candidate.json) · [源码与证据](t12-equal-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-equal-b8w43ocv`，最终 request-v2.json / controller / candidate-v2 / build-v2。source.tar 含全部 44 个原生源文件，恢复后的输入库存及 28 个保留产物绑定已核对。控制树 SHA256 `fa0639a356b7c29510178b9669d279de4c84636346d7ee8d6b56b4ea368a9137`。

## 接口与用途

现有 foundation.json 的 read.c 追加 string_equal 一个导出，没有新模块、源文件、服务、依赖或 Unicode 框架。旧 read.c 全文保持为新文件的逐字节前缀；已有 UTF-8 decode/find、parser、writer 和数值实现以及公开结构尺寸不变。

接口接受两组完整且匹配的不可变 JSON input/result/token 索引，比较 STRING token 解码后的 UTF-16 单元序列。结果只在成功时写入 32 位 0/1；错误保持原输出。允许同一输入/结果、自比较及对象 key token；不接受其他 token 类型，也不做通用 JSON 深比较。

这一语义用于后续 Schema 字段名和枚举：转义写法与等价原始 UTF-8 应相等，合法 JSON 中的孤立 escaped surrogate 也必须可比较。已有 UTF-8 解码接口仍对无法无损表示的孤立 surrogate 返回 unsupported，本接口不把它替换成 U+FFFD，也不改变旧接口范围。

参考 [ECMAScript SameValueNonNumber](https://tc39.es/ecma262/multipage/abstract-operations.html#sec-samevaluenonnumber) 和 [String Type](https://tc39.es/ecma262/multipage/ecmascript-data-types-and-values.html#sec-ecmascript-language-types-string-type)，检索日期 2026-10-20。只比较单元及位置，不进行大小写折叠或 Unicode 规范化；例如预组合字符与不同单元序列的分解形式仍可不相等。

## 实现与失败边界

复用已有 string_start、hex4 和 text_next。每个 escaped uXXXX 产生一个 UTF-16 单元；原始 UTF-8 supplementary scalar 产生 high/low 两个单元。私有游标先返回待处理 low 单元，再检查源是否结束，避免遗漏字符串末尾的非 BMP 字符。NUL 是合法值，结束用独立状态表示。

两个私有游标使用固定存储，不分配展开后的字符串；在首次差异或不同长度处返回，合法输入的比较工作随较短序列长度增长。输出存储须与两侧 input、result 及 token 数组全部不重叠。参数/索引/重叠错误 1，类型错误 9，结果或元数据错误 10，UTF-8 错误 12，字符串格式错误 14；失败不写输出。

输入有效性来自成功解析和调用者保持的不可变借用，不重新认证伪造 token，也不保证发现所有解析后修改。原生存储有效性、对齐与生命周期仍是调用者责任；不把范围检查当成原生隔离。

## 实际验证

- 28 项冻结构建、保留产物复验、40 次断网正常执行通过，4 次继承保护页正对照返回 139，4 次越界分片参数控制返回 247。
- 2,060 组 Node JSON.parse 字符串相等结果与 Python UTF-16 surrogatepass 字节比较共识；原生逐条检查两种操作数顺序和自比较。
- 全部 65,536 个 UTF-16 单元：大小写 hex 等价与相邻单元不等；其中 63,454 个可直接出现在 JSON 字符串内的 BMP 单元，额外检查原始 UTF-8 与转义等价。Node 另实际核对全部单元的长度/charCodeAt 和等价/不等预期。
- 3,072 组 pair 边界：每个 high 单元搭配 low 两端，以及固定 high 搭配全部 low；不是穷举全部 1,048,576 个非 BMP pair。
- 孤立 high/low、断开的 pair、替换字符不等、NUL/短转义、不同 Unicode 规范形式、key/nonroot token；非字符串/索引/失败结果/两侧存储重叠拒绝。
- 1 MiB 相等输入与末尾差异；两个输入及四字节结果分别贴住三片保护页的末端。

新测试在每个 JSON 组合的第 0 片执行。原有语法/全部分片/reader/quote/数值读取/writer 测试保留；16,500 条数值格式化的四片仍全部运行并核对片号集合，未用旧版单片运行器漏测。

补充 UBSan：`/home/xu-yuanhao/Documents/PanGu-artifacts/t12-equal-ubsan-yn88esil`，28 项构建、40 次正常执行、4 次保护页对照、6 次违规移位对照均符合 0/139/132。生产源码与普通候选一致，只增加 text-probe 正对照分支；不是正式 sanitizer、ASan/TSan 或汇编全覆盖。

首次准备中，宿主严格语法审查提示相邻 if 的缩进歧义，已拆行。随后我误把日志中的转义显示当成源码转义错误，额外 unicode_escape 转换破坏了正确测试模板；首轮冻结构建拒绝它。candidate-v2 从原始生成器恢复模板，失败候选与日志保留，未调整门禁或预期。源码归档是最终权威；setup.py 记录初稿，initial-review.json 记录修正，复验使用新目录。

## 审查与剩余任务

approval.json 明确记录实施者与审查者为同一 Agent，design-review.md 覆盖 pending/end 顺序、单元范围、NUL、两侧借用及失败原子性；evidence 绑定固定实现与回执。未建立正式性能/栈/RSS/体积预算，不代表完整 G07 或受信任服务器防绕过证明。

下一步实现有明确关键词范围、未知能力显式拒绝的 Schema 服务，并继续 Pi 参数修复/强制转换。UTF-16 存储/输出、任意 token 名查找、完整 JSON 深比较、完整 T12 和 Pi wire/session 兼容仍未完成。

源码已公开至 [`codex/t12-json-equality`](https://github.com/xu-yuanhao/PanGu/tree/509267628524e46cbd41faeb3617f25f122c2a62)，固定提交 `509267628524e46cbd41faeb3617f25f122c2a62`。发布副本全部 preflight 输入与获批候选一致，远端 ref 已回读核对；[发布记录](t12-equal-source-publication.json)不表示生产准入。
