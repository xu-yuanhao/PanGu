# T13 作用域与服务解析启动边界

2026-10-20。设计及外部宿主实验，不是原生准入或T13验收。T12整体仍IN_PROGRESS，T13任务仍PLANNED；本记录不将未完成的阶段输入冒充通过。当前可用输入是已固定的T10～T12本地接口/实验包，尚未批准新的插件宿主模块。

## 参考与范围

已核对Cordis context.ts的isolate、service.ts的filter、reflect.ts的locate/provide和Harness core/scope的ScopedLayers。参考摘要记录在实验evidence.json，没有修改references或将其编入产品。

Cordis服务按隔离标签选择实现，locate还检查fiber的依赖声明和隔离屏障；不能只实现一个全局名字map。Harness的NamedEntries/ScopedLayers提供registry内部的global与scope条目合并、exact-entry undo及资源归属，是另一层机制。盘古分别实现服务解析与具体registry语义，不把两者混成通用覆盖规则。

T08的root-only条款现修正为T10历史范围；主架构应用/Session作用域要求保留。回调级资源归属、dispose、异步刷新和热重载不由静态解析器假装完成。

## 首个解析内核草稿

外部目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t13-scope-wip-eopzg20g`。scope.h/scope.c接受不可变的scope树、服务目录、显式binding、提供者与查询；索引仅属于同一计划，不是稳定服务名、插件实例ID或wire标识。服务/版本的稳定映射和真实静态插件清单尚未实现。

- Root与子作用域显式登记，每个非root parent必须在前，拒绝环形/越界树；完整依赖图的环仍待实现。
- binding逐服务选择ISOLATE或INHERIT；缺省不继承。ISOLATE在本scope建立域，INHERIT逐父声明解析；任何缺口或隔离屏障都停止，不能继续猜测全局服务。
- 提供者必须登记在自己显式ISOLATE的域，不能在继承域中无声明覆盖父服务；单提供者在服务/精确版本/域内唯一，服务实例标识独立于未来插件owner标识。
- 多提供者按不可变清单顺序返回全部匹配项，不暗选首项。隔离域内不自动叠加父提供者；Harness工具等registry需要global/scoped合并时由其接口单独定义。
- 必需服务无匹配返回NOT_FOUND，可选服务返回空集合；精确版本不匹配不自动回退。此处查询是可信计划器输入，尚不是面向任意插件的安全get接口；消费者依赖声明校验还须在完整图中实现。
- 调用者提供结果空间，总扫描fuel预收费；任何配额/容量/定义错误保留输出indices，report可变。可写区与所有读区分离；无堆分配、回调或插件执行。

scope.c是外部草稿，未登记生产文件或新服务，不属于既有固定候选。evidence.json固定源和结果摘要。

## 实际验证与缺口

宿主C17无libc依赖共享库编译/链接通过；独立Python规则模型对照120组固定种子图、7806次查询，32个随机图不足fuel检查；另9条明确继承/屏障/兄弟隔离/精确版本/多提供者顺序例子、158个不足fuel检查和2个别名拒绝通过。模型只验证盘古明示的草稿规则，没有运行Cordis，不声称Cordis兼容。

接下来必须完成：静态插件清单及catalogue身份、定义/提供者/消费者分离、required/optional边、循环/冲突诊断、Pending与激活顺序、两种最小提供者、G05组合矩阵；随后按新边界登记ABI/模块/依赖，补原生冻结构建、保护页、补充UBSan和恢复验证。T14再验证owner/失败回滚/停止和清理顺序。不得因当前解析草稿通过就把T13或生命周期勾选完成。
