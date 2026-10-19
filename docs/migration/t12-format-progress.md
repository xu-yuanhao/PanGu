# T12 binary64 数值格式化本地实验

2026-10-20，T12 IN_PROGRESS。实施 Agent 按用户「以后改为Agent自批自审」完成本固定包自审，批准用于本地实验；不代表独立审核、受保护控制版本生效或生产发布。

[候选登记](t12-format-candidate.json) · [源码与证据](t12-format-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-format-q5nwe7yr`，request.json / controller / candidate / build；source.tar 包含全部 44 个原生源文件，恢复后的输入库存与保留产物绑定已复验。控制树 SHA256 `f164a397053252ffe2310c37c13c36c5db9b8ccc91063c5ba9d0cba1783cc2a9`。

## 接口与参考边界

现有 foundation.json 的 number.c 追加实现，新增 number_text 一个导出，不增加模块、源文件、服务 ID、依赖或构建组合。原 number.c 全文仍是新文件的逐字节前缀；既有 parser/reader/writer/quote 实现及 ABI 尺寸不变，公开头文件只追加模式枚举和声明。

输入为 binary64 原始位模式，不是待数值转换的整数。返回显式长度 ASCII/UTF-8 字节，不写 NUL。两种模式区分：

| 输入 | 数字 JSON 模式 | JS 数字 String 模式 |
|---|---|---|
| 有限非零 | 最近且相等取偶数的最短往返十进制表示 | 同左 |
| +0 / -0 | 0 | 0 |
| +Infinity / -Infinity | null | Infinity / -Infinity |
| 任意 NaN 位模式 | null | NaN |

格式按 radix 10：十进制点位置 -5..21 用普通小数/整数形式，其他用带明确指数符号的科学计数法。边界与非有限值参考 [Number::toString](https://tc39.es/ecma262/multipage/ecmascript-data-types-and-values.html#sec-numeric-types-number-tostring) 和 [SerializeJSONProperty](https://tc39.es/ecma262/multipage/structured-data.html#sec-serializejsonproperty)，检索日期 2026-10-20。最近/偶数选择在前者 Note 2 中属于推荐精度规则；本包以实际 Node 输出为行为基线，不将推荐写成规范唯一允许的结果。

只覆盖数字原语，不处理 boxed object、radix 选项、BigInt、toJSON、replacer 或通用 JS 强制转换。不是完整 JSON.stringify 替代品，也未完成 Pi Schema/参数转换。结构化 writer 原 NUMBER 操作继续保留原数字词法；要序列化 binary64 值，先用本接口，有限值送 NUMBER，非有限值送 NULL；JS 数字字符串则送 STRING，均有组合测试。

## 算法、容量与资源

复用既有私有整数和精确中点比较。先将 mantissa*2^e 化成有限十进制数字与 scale，再从 1～17 位依次测试向下/向上两个十进制候选是否落在原 binary64 的舍入区间。边界相等时按目标尾数奇偶判断，包括 binade 两侧不对称、次正规数和溢出阈值。两候选都可用时，比较精确十进制距离，相等取偶数。没有再写一套近似浮点解析，也没有改动原数值读取实现。

最多 768 位精确十进制数字存于固定 800 字节数组，继续使用每整数 160 个 32 位 limb；候选 scale 有界，临时值低于现有容量。17 位舍入界与范围推导见外部 design-review.md。最终文本写入经边界检查的 32 字节局部数组，再确认容量后写出。测试语料观测长度为 1～25 字节；不为每个数字分配堆或引入通用数学框架。

非法模式、参数、地址范围或输出/长度存储重叠返回 1，输出及长度不变；有效调用中容量不足返回 2 并报告所需长度，内部算术/容量错误 11 则长度为零。所有失败保留输出字节，NULL/零容量可测长。位模式按值传入；调用者仍需保证输出/长度存储有效、对齐与生命周期，不声称任意指针认证或原生隔离。

当前未建立正式吞吐、RSS、调用链栈或 Agent 体积预算，没有完整 G07 结论。测试 ELF 含大批静态语料，不作为 Agent 体积。

## 实际验证

- 28 项冻结构建及保留产物复验通过；40 次断网正常执行覆盖四个 JSON 构建组合的全部四片；4 次继承保护页正对照均为 139。
- 16,500 条位模式逐条核对 JSON/String 两种输出，包括全部 2048 个指数字段与边界尾数/正负号组合、十进制布局阈值邻近值、固定种子 120200 的随机位模式、正负零、无穷、signaling/quiet NaN。实际 Node 输出与独立 Python shortest repr 规范化结果无冲突。
- 16,490 条有限值经原生 parser/reader 后位模式精确往返；另有 1 条负零按约定归一化为正零。9 条非有限样本的 JSON 输出解析为 null。没有将归一化或 NaN payload 丢失冒充位级往返。
- 75 个样本在两模式下额外检查测长和 0/n-1/n/n+1 容量、全部输出哨兵；19 个样本与结构化 writer 组合，并将输出和长度输出分别放到保护页末端。
- 非法模式、NULL/非零容量、缺失长度、重叠存储拒绝；既有语法/全部分片/reader/quote/数值读取/writer 测试仍在各 JSON 组合的第 0 片执行。

分片只改变测试执行组织。测试启动汇编在栈对齐前读取 argc，将 argc-1 作为私有第二参数传给 json_test；第 s 片遍历 s,s+4,...，每片 4125 条。运行器核对每组合的片号集合必须为 0..3，逐次保存回执。另真实执行四个 ELF 的越界片号，均返回约定 247，验证 C/汇编参数传递和拒绝路径；不作为产品 CLI。进程超时仍为 30 秒，未删除语料、缩小测试或放宽时间限制。

补充 UBSan 位于 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-format-ubsan-osa3tt8e`：28 项构建、40 次正常执行、4 次保护页对照和 6 次违规移位对照全部符合 0/139/132；同样核对完整四片集合。生产源码一致，仅 text-probe 插桩正对照分支不同。不是正式 sanitizer、ASan/TSan 或汇编全覆盖。

另宿主 GCC -O0 共享对象探索完成 33,000 次模式调用，无差异，仅作补充。首次宿主链接因其默认 stack protector 的运行时依赖失败；重试采用与冻结 freestanding 配置一致的 fno-stack-protector，未改源实现、冻结门禁或工具链。失败记录保留，不计入通过证据。

## 审查及剩余任务

approval.json 记录同一 Agent 的自审身份、固定控制树及实现候选摘要；design-review.md 覆盖中点成员判定、最短/最近/偶数、容量算术、失败原子性和测试 C/汇编接口。evidence 绑定所有源码、生成器、归档、运行时和执行回执。归档恢复匹配全部 preflight 输入，并复验 28 个保留构建；不等同于独立重新编译或受信任服务器证明。

下一步推进受支持 Schema、Pi 参数修复/强制转换以及当前 wire/session 对象模型的兼容差异。JS 属性枚举、孤立 UTF-16 surrogate 输出、完整 T12、生产门禁与受保护发布仍未完成。

固定源码已公开到 [`codex/t12-json-format`](https://github.com/xu-yuanhao/PanGu/tree/130b857d399a43d7dca5848654e87d81b0263634)，提交 `130b857d399a43d7dca5848654e87d81b0263634`。发布副本全部 preflight 输入与获批候选一致，远端 ref 已回读核对；[发布记录](t12-format-source-publication.json)不表示生产准入。
