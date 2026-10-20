# T13 原生宿主规划器本地实验

2026-10-20。T13 IN_PROGRESS，仅固定本地实验包。实施与审查是同一Agent，按用户授权自审自批；不是独立审核或生产放行。

[登记](t13-host-plan-candidate.json) · [证据](t13-host-plan-evidence.json) · [发布](t13-host-plan-source-publication.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t13-host-native-v3-3rjcav7f`。控制树SHA256 `838a3a4a181b6e8e618d32564eeebec1ca5d51791dafc6e0b5dd8a07693048bd`，候选树SHA256 `c1900ee46a466ed027d153b763895bdedf9a79b191f23cd37cc564eb23e1db39`。

## 范围与前置

本地输入为已固定的T10 PAL、T11调用者存储及T12原生接口实验；不声称这些阶段的完整生产验收已完成，也不修改全局阶段依赖。当前包用于无配置、无权限请求、无激活回调的静态P0规划预览；配置Schema/权限、生命周期与完整T13仍未完成。

新host.plan v1定义在bootstrap-api，默认实现host-plan只依赖该定义模块。新增host-plan.h、host-plan.c及独立消费者main.c/entry.S共4个文件，旧60份原生源码全部不变。只有pangu_discover_prepare公开，内部阶段函数static；新消费者分别链接普通memory与test-guard提供者。现有共享模块显式补齐两个新组合的覆盖声明，没有新增运行时库或平台调用。

入口统一校验静态C入口元数据、规范模块模板、清单身份、实例声明、作用域及依赖图，共用总work。成功可含Pending；只给可规划实例排序，不执行插件。失败清空图视图并保留最终入口/计划数组；scratch可变。静态表由可信编译组合提供，不证明任意Native指针的来源，不是OS沙箱。自引用result不可移动/裸复制，调用者须检查返回与借用生命周期。

## 实际验证

- 普通及补充UBSan各40项构建、52次正常执行；原JSON四分片全保留，4项保护页故障对照139、16项无效探针参数247、补充6项移位对照132符合预期。本轮无测试失败或超时。
- 新增四个宿主profile覆盖应用/Session、必需/可选、Pending、编译入口映射、五阶段10126种不足配额与精确配额、9项阶段/一致伪造/别名拒绝。准备过程回调数0；测试随后显式调用三个C夹具得到101/202/303，不代表生命周期实现。
- 普通和guard后端在result192、最终入口映射和discovery storage描述的三处页尾验证成功及容量失败；全量工作区页尾、全部host随机scope/graph/cycle/multi-provider语料尚未迁入原生消费者，不能将宿主模型结果改称原生全覆盖。
- 四个新增原生提供者对象的全局定义均仅pangu_discover_prepare；固定门禁也核对符号/依赖。宿主独立负链接测试已证明私有helper不能正常跨模块链接，不等于同进程恶意机器码隔离。
- 64份源码归档恢复，preflight输入一致并核对保留产物绑定；不是重新构建。旧36个ELF中与前包字节完全相同的数量为36。UBSan为补充模式，不是正式sanitizer或全汇编证明。

两次预检拒绝均保留：消费者误登记c-header语言，随后共享模块漏登记新组合覆盖。分别建立新候选修正登记后重跑；检查器、预算、30秒时限、参考基线均未放宽。

完整G05配置/替换矩阵、原生循环/多提供者全面语料、Pending/循环明细、配置Schema/权限、T14激活/owner/回滚、正式资源预算/完整G07、完整T13和服务器受保护发布未完成。native_matrix=PASS与release=BLOCKED同时成立。

源码发布至 [`codex/t13-native-host-plan`](https://github.com/xu-yuanhao/PanGu/tree/9ebd318a388c0c204c48936112364c1b996a4f81)，固定提交 `9ebd318a388c0c204c48936112364c1b996a4f81`，全部preflight输入与固定候选一致，远端ref已回读。

后续测试增量：[原生循环/多提供者组合](t13-combinations-progress.md)补充了正/逆提供者顺序及两类自依赖循环；更大原生语料和完整T13仍未完成。
