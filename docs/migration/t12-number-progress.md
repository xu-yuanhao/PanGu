# T12 JSON 数值读取本地实验

2026-10-20，T12 IN_PROGRESS。本包按用户「以后改为Agent自批自审」的决定，由实施 Agent 自审并批准用于本地实验，不是独立审查或生产准入。

[固定候选](t12-number-candidate.json) · [源码与证据](t12-number-evidence.json)。外部目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-number-duzp1i5c`，最终输入 request-v2.json / candidate-v2，controller / build-v2，源码 source.tar；源码恢复及 28 项保留产物绑定复验通过。控制树 SHA256 `190782406bce0d9fc5b08561ce363f171845c16247845f3c6abccfdcf264b056`。

## 接口与范围

现有 foundation.json 模块增加 number.c 和 number_bits 导出。只接受成功 parser result、有效 NUMBER token 和对应完整不可变原输入，返回 IEEE-754 binary64 原始位模式。结果不是整数截断值，也不是经过本机浮点环境计算的 double；使用方须按明确格式解释。

遵循 JSON.parse 的最近值、相等取偶数舍入，保留负零，溢出为带符号无穷；合法 JSON 不产生 NaN。后续 Schema 可以拒绝无穷，不能把语法合法或转换成功直接视作工具参数有效。Number(string) 对十六进制、空白等强制转换、布尔/null 转数值及数值格式化均不在本包内。

数值字面量上限 1 MiB，与 parser 文档字节上限分开。超限返回 2；错误 token 类型 9；无效结果 10；词法不符 14；参数/存储重叠 1。所有失败保持输出位模式不变。源、result、token、输出的生命周期及有效存储由调用者保证，不声称认证伪造元数据或任意指针安全。

旧 parser、reader、quote 实现逐字节不变，只在 json.h 追加声明和契约；原 parser/token/limits/result 的 1144/48/16/32 字节断言继续编译通过。没有新模块、服务 ID、堆、libc、浮点运行时或第三方数值库，没有改门禁或构建参数。

## 算法与资源

用固定 160 个 32 位 limb 的私有整数计算十进制有理数，再比较 binary64 相邻值的精确中点。二分搜索最多 63 次，不逐个遍历浮点值，不做递归。

保留 800 位有效十进制数字及被舍弃尾部是否非零。binary64 中点的有限十进制表示至多 768 位有效数字，因此被截断区间内不会隐藏一个中点；恰好落在中点时用尾部标记决定是否超过它。指数先与完整尾数/小数位数合并再作数量级判断，避免百万位尾数与指数抵消时误判。指数饱和值大于允许的任何尾数抵消范围。

四个私有 bigint 数据合计 2576 字节，转换时不分配与输入长度成比例的存储。可行数量级内分母至多 10^1123，比较临时值小于 4755 bit，低于 5120 bit 容量。符号、carry、shift 和指数范围推导见外部 algorithm-review.md。保留反汇编入口：debug 栈预留 0xac0，release 0xa38，另有寄存器保存及被调用函数；这不是完整调用链栈预算或 G07 验收。基线与性能预算仍 UNBASELINED，未建立 Agent 吞吐/RSS 结论。

## 实际验证

28 项冻结构建和断网正常运行通过；继承的 4 次保护页正对照为预期 139。JSON mmap/guard 两后端各跑 debug/release，保留此前语法、全部分片、reader、quote 测试，并增加：

- 1315 条 Node JSON.parse / Python float binary64 位级共识：负零、整数精度、溢出、次正规数、最小正规数、最大有限值边界，以及随机中点的精确相等与上下邻近十进制值。
- 原 547 条基线加固定种子 120065 的 128 组随机 binary64 中点；每组正负号与三种位置，共增加 768 条。Decimal 精度 2400，邻近差值 10^-1800，最长输入 2111 字节；逐条比对全部 64 位，不只比打印出来的字符串。
- 百万位尾数及大指数抵消、百万位前导小数零抵消、超长正负指数饱和、超过 800 位后才出现的非零尾部、恰好 1 MiB 与超限一字节；配额失败保留输出。
- null/数组拒绝、数组内数值读取、非法索引、结果未成功、输出与源/结果/token 重叠拒绝；数值输入贴住保护页末端。

补充 UBSan 位于 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-number-ubsan-khw58na6`：28 项构建/正常运行、4 次保护页对照和 6 次违规移位对照分别符合 0/139/132。生产源码与普通候选相同，仅 text-probe 的正对照分支不同。插桩不是正式 sanitizer 配置、ASan/TSan 或汇编全覆盖。

候选初版已通过构建/运行；最终 v2 仅扩展大输入边界测试，重新完成全部矩阵。初版及其回执保留，未删除失败用例或缩减测试。make-number-tests.py 保存初版生成器，v2 增补测试的最终权威为 source.tar / candidate-v2，不应把生成器草稿等同于最终树。

## 审查与下一步

approval.json 记录同一 Agent 的自审身份、固定控制树/候选摘要、算法与依赖审查和结论；evidence 绑定该记录及所有回执。没有独立审核保证，也没有受保护控制版本晋升、远端权限或生产发布。

接下来推进当前数据模型所需的结构化输出、受支持 Schema 和 Pi 参数转换适配。数值序列化、完整 TypeBox/Number(string) 语义、完整 T12、生产 G07 与实际 Agent 功能仍未完成。

源码快照已公开到 [`experiments/t12-json-number`](https://github.com/xu-yuanhao/PanGu/tree/c8b7d04aa0029a8cac1eef03430c34fa711ac1e1)，固定提交 `c8b7d04aa0029a8cac1eef03430c34fa711ac1e1`。发布副本的全部 preflight 输入与自审候选一致；远端 ref 回读相符。
