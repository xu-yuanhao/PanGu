# T12 基础标量转换规则本地实验

2026-10-20，T12 IN_PROGRESS。固定包由实施Agent按用户授权自审自批，仅用于本地实验；同一实施者与审查者，不作为独立审核或生产放行。

[登记](t12-scalar-candidate.json) · [实现与证据](t12-scalar-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-pi-scalar-b0_majy9`，request.json / controller / candidate / build。控制树SHA256 `385179b0166a4fd3f9f5cbaa8b109f92e2bf00363d3c6e1d53740eb5a3d293a5`。58个原生文件已归档恢复并复验保留产物绑定；恢复不是重新构建。

## 已实现

既有pi-arguments模块增加scalar.c、scalar_convert导出及32字节结果，无新模块、服务、依赖或构建模式。旧基础和兼容实现不变，所有旧探针保留。

五类目标number/integer/string/boolean/null，三种模式：TypeBox1.3.27单条规则、冻结Pi plain单条规则、无literal/enum/union约束的typed基础规则后接plain规则。调用者显式选择来源，不能从序列化JSON猜测隐藏Kind。

接口处理内部字段的单个值，不等同于整个ToolCall或Schema转换。结果kind0表示保留原token，包括容器和原始STRING；其他结果使用JSON原语kind及替换表示。替换NUMBER保证有限binary64；替换STRING借用静态ASCII或workspace，须在重用前消费。跨阶段传输NUMBER时保留负零，不能盲用JSON.stringify模式抹掉它。

## 关键行为

- typed数值字符串先使用Number(string)，只接受有限值，再尝试大小写无关true/false、严格十进制安全BigInt后缀n。安全范围为±9007199254740991，拒绝加号/前导零/空白/大写N，-0n变成正零。失败保留原字符串，不生成NaN替换值。
- plain数值字符串要求trim非空和有限结果，没有typed fallback；plain integer要求舍入后Number.isInteger，typed integer按位实现Math.trunc，保留符号零。
- typed/plain的null转string分别为"null"/空串；typed布尔和null词语规则比plain更宽。typed基础组合保留typed已成功结果；string遇非有限原始number时，typed失败后仍由plain转换为"Infinity"等。
- 类型已满足、转换不适用或不成功时可保留原值。原STRING直接保留，包括孤立escaped UTF-16；这类单元不能构成数字或ASCII识别词，不替换也不错误报告UTF-8 unsupported。

上述组合化简只适用于当前无约束基础原语。literal/enum可能先限制typed候选，union要选择分支，数组可能包装，不能直接把本接口当作这些路径。

## 内存与失败

输入必须是匹配的成功JSON视图和值token，key不能传入转换；检查结果头、索引、范围和selected lexeme字节配额。workspace容量、结果结构与输入视图/结果/token/原文范围分离。任何失败完整保留32字节结果，workspace可丢弃；状态0也可能保留一个不符合Schema的原值，仍需最终验证。

调用者拥有存储、对齐、不可变源和借用生命周期。workspace用于解码UTF-8与独立数值规范化区或数字格式化；max(25,2*lexeme字节+2)为保守容量，已有1MiB数值限制不变。没有堆分配、产品递归、libc、持久可变状态或指针认证。

整数截断覆盖符号、指数与所有低fraction位宽；BigInt安全累积先检查再乘加，不依赖浮点。所有Unicode码点128～10FFFF的Node审计中，只有Kelvin符号完整小写为ASCII k；识别词true/false/null/undefined不含k，因此无需引入Unicode大小写框架。

## 实际验证

- 2,562条语义对照，三模式、五目标，涵盖原语/不适用容器、空白、NUL/孤立UTF-16、BigInt安全边界，全部-2～53整数截断指数、三种mantissa边界和正负号，另含次正规数/最大有限值。
- typed参考调用实际Value.Convert的内部字段；plain参考为完整冻结validation.ts加一条export访问私有coercePrimitiveByType，函数体未改。plain/组合模式的参考结果还与Compile及公共validateToolArguments的接受/拒绝和输出核对。原生证明范围仍是标量阶段，不是完整ToolCall兼容。
- 宿主与原生每条检查零workspace；原生还比较精确bits/UTF-16/逻辑JSON、有限替换与保留字段、所有安全后缀workspace容量、非根值/key拒绝、配额、模式/别名/失败结果头及三片源/workspace/结果页尾。
- 普通及补充UBSan各36项构建、48次正常执行，4项保护页对照139、12项私有参数拒绝247、补充6项移位对照132通过。本轮无超时；保留以前全部JSON/Schema/兼容探针和数字格式化四分片。

源归档恢复核对的是保留产物绑定；UBSan不是正式sanitizer或汇编全覆盖。未建立正式性能/栈/RSS/体积预算，不代表完整G07或受保护服务器保证。

## 接下来的工作

接入对象/数组遍历、typed数组包装、literal/enum/union候选限制、optional-null前置及最终Schema校验，建立完整validateToolArguments对照。处理原始整个primitive-root时，Pi丢弃Value.Convert返回值的边界也须单独追踪；当前接口只承诺内部字段。

完整转换、Provider/wire/session、完整T12及生产准入未完成；native_matrix=PASS和release=BLOCKED同时保留。

源码已公开至 [`codex/t12-pi-scalar-rules`](https://github.com/xu-yuanhao/PanGu/tree/e7a794157a85cbfba78fe8c9016e6acdbab5ee84)，固定提交 `e7a794157a85cbfba78fe8c9016e6acdbab5ee84`。发布副本全部preflight输入与固定候选一致，远端ref已回读核对；[发布记录](t12-scalar-source-publication.json)不代表生产准入。

## 对象转换接线前的参考复验（2026-10-20）

已执行320条冻结Pi/TypeBox参考观察，位于 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-tree-oracle-20261020`。`oracle.mjs` 使用完整原始validation.ts加私有helper导出，逐条保留optional-null、typed原地结果、typed返回结果、plain结果和公共接受/拒绝；各阶段在后续原地修改前立即快照。`reference-binding.json`记录源文件与执行脚本/结果摘要。此为参考证据，不是原生对象转换通过。

三条执行断言确认：Type.Literal("")与Type.String({const:""})序列化JSON相同，但内部字段null的公共接受结果不同；typed数组会把字符串"1.8"包装并转换为[1]；typed属性名a.b按未转义正则匹配，会转换名为axb的值。因此后续转换描述必须显式保留Kind来源，不能从JSON猜测。对象名称的正则语义须显式实现或返回不支持，不能静默改成精确名称匹配。另保存primitive/array根的返回值丢弃观察，接线时区分返回值与原地修改。

下一实现仍是有界对象/数组转换及四工具完整链路对照；本次没有新增原生实现，也没有勾选转换完成项。
