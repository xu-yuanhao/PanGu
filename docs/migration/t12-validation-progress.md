# T12 统一参数校验入口本地实验

2026-10-20，T12 IN_PROGRESS。实施与审查为同一Agent，按用户授权自审自批，仅批准固定包用于本地实验；不是独立审核或生产放行。

[登记](t12-validation-candidate.json) · [证据](t12-validation-evidence.json) · [源码发布](t12-validation-source-publication.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-pi-validation-adsngan8`。控制树SHA256 `a6ebf03c4735be1a83fd981bf4e656790be170afb5797d3d0a4c9685cb4e0070`，候选树SHA256 `bbebcafb0efd84f7077e934bf112ca3214292824a2aad63ccd5391546e8a6ef5`。

## 实现

既有pi-arguments增加validate.c与arguments_validate导出，新增storage96/result112字节描述；无新模块、服务、依赖、构建模式，旧接口布局/实现不变。调用者提供repair、双字节Buffer、tokens、scalar及drops存储，全部可写区互斥且与原始输入、Schema视图/JSON/tokens/Kind表和storage描述分离。

入口执行最终对象解析→optional-null→typed树转换→plain树转换→严格Schema验证，只有最后成功进入READY才发布借用JSON/tokens。非final先检查参数几何再清空result，不触碰工作区；运行时失败清空发布指针/长度/count，前置参数或坏结果头错误保留原result。调用者必须检查返回值，并在每次复用存储前使旧借用视图失效；保留前置错误result不是允许忽略失败继续使用的授权。

JSON原始/中间字节、深度、token容量与跨归一化/转换/验证阶段的总Schema work分别受限；每个阶段从剩余额度扣减实际report.work，不能每阶段重置配额。work是确定性操作/字节计数，不是CPU时间或正式性能预算。失败诊断阶段保留，不复刻JS错误文本。

输入范围为最终Object文本，保留已有严格优先/词法修复规则；没有salvage补全。Typed Array根显式返回9，避免把Pi丢弃的Value.Convert返回值错误接管。union、typed正则名称及其他既有Schema范围限制仍在；不是完整公共Pi根值行为、Provider事件接线、argumentsError传递或执行授权。

## 实际验证

- 286条四工具公共Pi对照；286次非final检查逐字节确认repair/双Buffer/tokens/scalar/drops未改变，结果不发布。
- 1570条真实Pi最终解析+验证对照，196次接受、56次修复成功；成功语义、解析拒绝/Schema拒绝阶段及失败不发布均验证。工具Schema原样抽取并绑定参考摘要，没有执行工具。
- 宿主对bash样例的全部3341个不足总work配额拒绝，精确3342配额成功；94组双缓冲容量、15组token容量、原始字节配额和别名/元数据拒绝通过。原生另对选定简单样例穷举全部不足work配额，验证容量、token、原文配额、结果前置错误保留及非法UTF8不发布。
- 九处原生页尾：storage描述、result、repair、双Buffer、tokens、scalar、drops、输入；成功与中途容量失败均验证。所有既有探针和JSON四分片保留。
- 普通与补充UBSan各36项构建、48次正常执行；4项保护页故障对照139、12项无效探针参数247、补充6项移位对照132通过。普通执行因桌面中断停于14条已完成记录；确认旧句柄不存在且无对应活进程后，仅补跑缺失项，保留中断回执和恢复记录，均绑定原固定ELF摘要，没有扩大30秒时限。本包没有记录到测试失败或超时。
- 60份源码归档恢复，preflight输入一致并复验保留产物绑定；恢复不是新构建。UBSan为补充实验，不代表正式sanitizer准入或全部汇编证明。

完整Pi根值/错误文本、union等高级Schema、Provider/wire/session、工具执行授权、正式预算/完整G07、完整T12及受保护发布未完成。native_matrix=PASS与release=BLOCKED同时成立。

源码发布至 [`codex/t12-unified-validation`](https://github.com/xu-yuanhao/PanGu/tree/37dd2ba50b03c13ba94d2747f110cae2f6c92b1e)，固定提交 `37dd2ba50b03c13ba94d2747f110cae2f6c92b1e`；发布副本preflight输入及远端ref回读均匹配固定候选，不代表生产准入。
