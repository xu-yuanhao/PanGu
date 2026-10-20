# T13 原生依赖图语料

2026-10-21，T13 IN_PROGRESS。同一实施Agent自审自批，仅批准固定本地实验，不是独立审核或生产放行。

[登记](t13-corpus-candidate.json) · [证据](t13-corpus-evidence.json) · [发布](t13-corpus-source-publication.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t13-host-corpus-native-9yjyg49n`，候选树SHA256 `855d3a803afe6b92d405f6dc494eb84c79c4553a9799b71bbe527bc9bcea5830`，控制树SHA256 `d22b6f03eb83bebbb83539885ee0354a320cc0e4a0f43a77ed9318e3de01cd53`。

只增加消费者私有graph-cases.h及其登记/c-header语言，修改消费者调用语料；规划器、公开ABI及其余63份旧源码不变。删除新增include、语料块与调用后，旧消费者逐字节恢复一致；没有移除旧用例或更改基线。

## 语料与实际验证

固定种子0x13a6重建原240组宿主图，输入规模、预期状态/Pending/顺序与旧记录核对一致。独立Python模型给出完整状态、顺序、绑定；生成静态POD模块/实例/依赖及真实C入口表，经唯一公开准备入口验证，未绕过规范模板或调用私有graph helper。

每个原生profile均得到95组有效计划、145组循环拒绝；含14组没有自环的多节点循环、69组间接Pending传播。有效计划逐项比较状态、顺序、绑定、入口映射；循环要求阶段5/错误17/reason8，图视图不发布、所有最终数组保持毒化字节，回调数0。范围是root域、最多9个实例/10个提供记录/24个依赖，不宣称随机跨scope覆盖。

随机有效图中没有两个同时合格的提供者。初次覆盖审查对此有错误预期，实际计数0已保留；正向MANY仍由原有正/逆清单两用例验证，未改种子、语料或期望来制造随机覆盖。

普通及补充UBSan各40项构建、52次正常执行；原有JSON四分片、4项保护页故障139、16项无效参数247及补充6项移位132均符合预期。四种host profile执行全部240图。本轮运行没有失败/超时。

首次普通构建在固定180秒隔离时限终止，原请求、日志和部分产物保留；确认进程终止后，仅变更输出目录，以完全相同的源码/控制器/镜像在原180秒重新完整构建通过，没有并行补充构建。超时原因未确定，不能据此断言是并行负载。补充构建成功，180秒与运行30秒时限均未放宽。

65份源码归档恢复，核对preflight与保留产物绑定，不是新构建。旧40个ELF中36个字节完全相同。UBSan是补充模式，不是完整汇编或正式资源预算证明。

仍缺完整任意跨scope/畸形清单语料、G05全部组合/替换矩阵、Pending/循环路径诊断、配置Schema/权限、T14激活/owner/回滚及完整G07/性能预算、完整T13和受保护生产发布。native_matrix=PASS与release=BLOCKED同时保留。

源码分支 [`codex/t13-native-graph-corpus`](https://github.com/xu-yuanhao/PanGu/tree/9217a7a1b349e7604dd970343b8670f6c94a835d)，提交 `9217a7a1b349e7604dd970343b8670f6c94a835d`；发布副本全部preflight输入与固定候选一致，远端ref已回读。
