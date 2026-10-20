# T13 原生循环与多提供者组合实验

实验日期2026-10-20，记录收尾2026-10-21。T13 IN_PROGRESS。同一实施Agent自审自批，仅固定本地实验，不是独立审核或生产放行。

[证据](t13-combinations-evidence.json) · [既有登记](t13-host-plan-candidate.json) · [发布](t13-combinations-source-publication.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t13-host-combinations-8yz3n3i9`。候选树SHA256 `1738e1e51db53139dd6394ee6def0c3c978e7077f34a751727d9931d56becfa6`，控制树仍为 `838a3a4a181b6e8e618d32564eeebec1ca5d51791dafc6e0b5dd8a07693048bd`。

本轮只扩展host-plan-probe/main.c；其余63份源码、公开ABI、规划器实现、控制器/模块登记、预算及30秒时限均不变。旧语义/配额/页尾检查保留，新增测试继续从唯一pangu_discover_prepare入口调用，不暴露内部graph helper。

## 新增原生用例

- 声明cardinality=MANY的测试接口，两个提供者均返回；将provision清单倒序后，绑定仍按清单顺序，服务记录身份/owner映射随之变化，插件拓扑顺序保持稳定。
- 安装夹具显式声明并提供自身必需依赖，规划返回17、阶段5、循环reason8，入口索引和计划数组不变，图视图不发布。
- 将同样存在的自依赖声明为optional，仍形成顺序边并拒绝循环；没有为了可启动而静默丢边。静态自依赖策略不冒充Cordis动态self-provision行为。
- 所有组合规划中回调计数为0，随后恢复原配置复验；测试最后的显式C夹具调用只证明入口映射，不是生命周期。

## 实际验证

普通与补充UBSan各40项构建、52次正常执行；原JSON四分片、4项保护页故障对照139、16项非法参数247及补充6项移位132均符合预期。新增组合在普通/guard × debug/release四种profile执行。本轮无失败或超时。

64份源码归档恢复并核对保留产物绑定，不是重新构建。旧40个ELF中36个字节不变。UBSan仍为补充模式，未证明全部汇编/资源安全。

更大的多节点/随机循环与多提供者语料、完整G05配置/替换矩阵、Pending/循环明细、配置Schema/权限、T14生命周期、正式资源预算/完整G07及完整T13未完成；生产发布保持BLOCKED。安装表/服务定义均为可信测试夹具，未建立针对任意Native输入的元数据来源安全保证。

源码分支 [`codex/t13-native-combinations`](https://github.com/xu-yuanhao/PanGu/tree/bfb6bc85b925f1be63cec477f41fd98a2e8e185c)，提交 `bfb6bc85b925f1be63cec477f41fd98a2e8e185c`；发布副本输入与固定候选一致，远端ref已回读。

后续：[240组原生图语料](t13-corpus-progress.md)已补充无自环多节点循环和间接Pending；随机有效图不含多活跃提供者，仍保留本页正/逆序MANY用例，完整T13未完成。
