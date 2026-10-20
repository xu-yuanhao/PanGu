# T12 有界严格 Schema 子集本地实验

2026-10-20，T12 IN_PROGRESS。实施 Agent 根据用户「以后改为Agent自批自审」完成固定包自审，批准本地实验；同一实施者与审查者，不作为独立审核或生产放行。

[候选登记](t12-schema-candidate.json) · [源码及证据](t12-schema-evidence.json)。外部目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-schema-b299q60c`，最终 request-v3.json / controller-v2 / candidate-v3 / build-v3。控制树 SHA256 `96aebd55b8f965b82280e0a8555c3e18fee41f58f049db7b191f712d73593af4`。49 个原生文件的 source.tar 已恢复并核对保留产物绑定；恢复核对不是重新构建。

## 服务与支持范围

新增可替换 foundation.schema/1，两个导出 check/validate，一个 C 实现文件；接口在 bootstrap-api，依赖已登记的 JSON 服务。新增两个测试组合，原 28 项构建扩为 32 项；既有 44 个原生文件逐字节不变，没有新增运行库、分配器、I/O 或工具执行。check 独立检查所有逻辑子 Schema，即使待验证值未包含该路径。

当前支持 boolean Schema；七种 type 名或非空 type 数组；properties、required、单个 items Schema；boolean additionalProperties；基本类型 enum/const；title、description、$comment 注解。对象成员按解码后的名称 last-wins；字符串按 UTF-16 单元比较，保留 escaped 孤立 surrogate/NUL。number 要求有限 binary64，integer 按转换后的 Number.isInteger 语义判断，数值 const 的负零等于零。

未知关键词、对象/数组 enum/const、非有限 Schema 常量、tuple items、Schema 形式的 additionalProperties、引用/方言、正则/边界约束/组合器等明确返回 unsupported 9；无效定义 16、值不匹配 15、配额 2。不得把未支持约束当成通过。这不是完整 JSON Schema 实现。

参考实际 TypeBox 1.3.27 Compile.Check；四个工具参数夹具按冻结 Pi 的 read/write/edit/bash 结构生成，省略不影响校验的 description 注解，没有执行工具。640 条实际 TypeBox 严格检查对照，另有 16 条原生子集拒绝检查；后者不能计作 TypeBox 兼容通过。

TypeBox 对 JS 原型属性有继承查找行为，例如 required:["toString"] 可让空对象通过；原生当前按 JSON 自有成员处理。因此 properties/required 中的原型敏感名称显式返回 unsupported，保留真实 TypeBox 反例记录 prototype-gap.json，不伪称等价。这一差异与未知关键词、没有 repair/coercion 一并留作迁移缺口。

## 资源与借用契约

调用者提供不可变的成功 JSON input/result/token 视图，以及 limits/report。接口结构尺寸 32/16/32 字节。固定 64 帧迭代遍历，无递归和堆分配；max_depth 1～64，max_nodes/max_work 为正。节点配额计逻辑 Schema，值访问和比较计工作量。work 是操作与检查的原始字节计数，不是 CPU 周期或正式性能预算；宽对象查找仍可能二次复杂度，但每次比较先消耗配额。

32 字节 report 必须与两组视图、输入、结果、token 数组及 limits 不重叠。参数/结果前置拒绝保持 report 不变；进入遍历后写入状态、原因、Schema/value token 与实际工作量。基本范围检查不能认证任意指针，调用者仍负责存储有效性、对齐、生命周期与解析后不可变性。

## 实际验证

- 32 项冻结构建及保留产物复验通过；44 次断网正常执行、4 次继承保护页对照（139）、8 次私有参数/分片拒绝对照（247）均符合预期。
- 原生逐条执行 640 条 TypeBox 对照及 16 条子集拒绝；检查输入与 token 不被修改。TypeBox 依赖文件清单、Node 版本、Pi 参考源码摘要已绑定证据。
- 逐个测试从 1 到成功所需 work+1 的工作配额；64 层/64 节点成功及 63 层/节点拒绝；1 MiB 字符串、非根 token、空参数/失败结果/两侧别名拒绝，两个输入及 report 分别贴住三片保护页末端。
- 所有旧 JSON 测试保留；16500 条数字格式化的四个分片按每种后端/模式完整运行并核对集合。
- 补充 UBSan：32 项构建、44 次正常执行、4 次保护页对照、6 次非法移位对照符合 0/139/132；仅 text-probe 对照分支与普通源不同。它不是正式 sanitizer 准入，也不代表汇编全覆盖。

初稿严格宿主语法审查提示相邻 for 缩进歧义，已拆行。首轮冻结构建拒绝测试映射数组初始化产生的 memset；candidate-v2 改为逐字段初始化。随后原生准入拒绝漏登公开头文件 schema.h；controller-v2/candidate-v3 补正登记。失败目录与日志保留；没有引入 libc、修改检查器、降低预算或改测试预期。最终源码归档/发布为权威，早期生成器是准备记录。

## 剩余工作

完整 Schema/TypeBox、Pi 参数 repair/coercion、provider/wire/session 兼容、系统化 fuzz、完整 T12、正式性能/栈/RSS/体积预算、完整 G07 和受保护服务器准入仍未完成。普通回执的 native_matrix=PASS 与 release=BLOCKED 同时保留；自审不把它改写为生产通过。

源码已公开至 [`codex/t12-schema`](https://github.com/xu-yuanhao/PanGu/tree/892c5a8ec2ee0974c39447e27f33e71d86950b23)，固定提交 `892c5a8ec2ee0974c39447e27f33e71d86950b23`。发布副本 preflight 输入与获批固定候选一致，远端 ref 已回读核对；[发布记录](t12-schema-source-publication.json)仅记录本地实验源码，不表示生产准入。
