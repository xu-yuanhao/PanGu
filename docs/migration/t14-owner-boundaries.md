# T14 owner 生命周期启动草稿

2026-10-21，外部宿主实验，未批准原生ABI/模块或T14阶段输入。使用已固定T13规划接口作为设计背景，不宣称完整T13已验收。参考Cordis fiber卸载等待dependent的语义，以及Harness vendored硬化：setup前登记所有权、重入停止等待setup/清理、停止后禁止新effect。

## 当前草稿

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-owner-wip-5stexo34`，owner.h/owner.c及evidence.json。调用者独占owner、资源栈和操作槽，无堆分配/产品全局状态，单线程操作。首次owner必须完全零初始化；通过API重用仅限Disposed、无资源/在途且epoch递增。

状态Loading→Active→Quiescing→Disposed。Loading/Active可登记同步disposer，停止先关闭新调用/注册入口；在途操作未退出返回BUSY并保留资源。资源逐项LIFO、调用前从待清理栈移出，busy标记保证重入stop不会重复释放；记录首个清理错误但继续全部disposer。重复stop不再执行清理并返回已有错误。Disposed只证明受管清理已调用完成，不保证失败回调的外部资源物理释放。

setup用显式操作票据在执行前登记；重入stop等待该票据退出，迟到加载成功被取消，不会恢复Active。完成票据检查owner地址、epoch及槽generation；复制票据重复完成、旧槽完成、新epoch的旧加载完成均拒绝。代际饱和槽退休，不回绕。finish_load明确传捕获的epoch，避免旧完成误改新加载；取消后完成不再改终态。

操作退出不会在回调栈内自动执行清理，调用者需再次stop或由未来调度器驱动；异步disposer和跨线程未实现；下述协调器仅支持预先验证的线性启动列表，尚未接入T13计划。必须让owner存储活到票据生命周期结束，不能手动清零或释放后复用旧票据；此处不是指针来源认证，未来运行时协调器还须约束owner分配/epoch和调用入口。

## 实际宿主验证

6组场景验证LIFO、清理错误继续、重入stop BUSY/注册拒绝、setup取消与迟到成功、重复/过期/代际票据、加载失败与清理错误分别保留、在途等待/槽耗尽/代际饱和、基础别名与非零初始帧拒绝。O0/O2无libc依赖链接通过。这些是宿主场景，不是Cordis运行兼容或原生门禁通过。

## 必须继续完成

从T13计划建立实例owner，Pending不激活，提供者先启动/消费者先停止；每个setup调用前登记操作并在所有退出路径释放；部分失败按实际已启动序列回滚；setup重入停止、异步完成/取消、异步清理必须保持owner可见至静默；清理异常不得饿死其他owner。最后登记结构/ABI、加入故障注入轨迹与原生冻结构建/guard/sanitizer/恢复证据，再判断G04/T14范围，不能据当前草稿勾选完成。

## 计划级协调器宿主草稿（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-coordinator-wip-hx57uc73`。复用上版owner源码，新增coordinator.h/coordinator.c及宿主测试test.c；固定源码摘要与完整编译执行记录见该目录evidence.json，其SHA256为 `77ce08feb7c908d172c54d096c1bb7a6e2ee4ae23e30b943ee22bbe4465d9690`。同一Agent实施和自审，结论仅允许继续宿主实验，不构成原生模块/ABI准入。

调用者提供已验证的提供者优先启动列表；Pending不加入此列表，但尚未实现T13输出到列表的适配验证。所有owner预初始化Loading，启动前检查重复owner/空setup/初态，每个setup调用前取得操作票据并记录已启动前缀，返回后释放票据。失败保留首个加载错误、逆序清理实际已启动实例（包括失败实例）；清理错误单独保留且不阻止其他已静默实例退出。setup重入stop只设置停止请求并返回BUSY，回调返回后取消后续启动并回滚。

停止从末尾消费者开始；未静默时保留其在已启动列表中，提供者仍Active供清理使用，操作退出后再次stop推进。协调器停止标记是新业务入口的阻断依据，后续服务调用分派必须接入该标记；目前没有服务分派入口，因此不声称全局新调用已被机器阻断。owner级入口在轮到该owner停止后关闭。回调不能修改列表、owner元数据或释放其存储；这是可信原生代码契约，不是沙箱。

实际验证：O0/O2各7组场景执行通过，覆盖正常逆序/重复停止、部分setup失败及清理失败继续、setup重入停止、消费者在途阻止提供者过早释放、失败setup遗留操作后继续回滚、重复owner在回调前拒绝、空列表。清理回调逐一断言更早提供者仍Active；owner/coordinator对象以freestanding/nostdinc编译，宿主测试程序使用libc断言。不是冻结工具链或原生门禁验证。

未完成：真实T13计划/依赖映射、分支图与Pending适配、公共调用分派阻断、异步setup完成协议、异步disposer及完整资源静默、恶意元数据/别名检查、工作量预算、原生冻结构建与故障注入。当前线性停止遇到BUSY会阻塞更早的无关实例；后续图协调器需要明确推进策略，不能据此声称完整图生命周期语义已实现。

## T13 真实准备结果到生命周期列表（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-plan-adapter-wip-a97379e9`，证据evidence.json SHA256 `5f860d650695f42e95751474cae02f664df86e001497bec2f2b22bc39be415e6`。新增adapter.h/adapter.c，将stage6准备结果、graph_output和按实例索引的可信setup/owner映射转换为激活顺序列表；复用上一版owner/coordinator源码。adapter核对Pending数量、顺序唯一性、所有解析绑定的索引和提供者先于消费者，包括optional-present和MANY；只发布非Pending实例。所有验证和整体提交预算预留成功后才写列表/数量；不足预算和拒绝保持输出。尚未把静态entry(void*)与owner-aware setup的ABI统一，不能宣称任意已绑定插件已能运行。

实际宿主执行：原封不动链接已登记T13 planner源码，直接包含其固定240图测试数据，95有效图、145循环拒绝；各有效图正常启动/停止，并在每个激活位置注入setup失败、setup重入取消、额外在途操作；另验证app/session隔离、Pending跳过和两个MANY提供者正反顺序。O0/O2各675个生命周期场景通过，消费者清理回调沿真实resolved bindings断言提供者仍Active；Pending保持未启动。每个有效图遍历从1到首个可用预算，检查不足预算时输出完整不变；额外拒绝非成功stage、越界绑定、反转依赖顺序、矛盾Pending、容量不足及重复owner。准备阶段未执行安装表的entry回调。

同一Agent实施和自审：继续宿主实验，不批准ABI或生产准入。首次测试编译的const描述符参数错误已修正，未修改警告级别或planner基线。当前借用的准备视图/源owner映射/目标列表须由可信调用者保证有效、互不重叠和存活；全面几何检查、服务分派入口、静态entry ABI、异步setup/disposer及原生验证仍待实现。当前逆序前缀停止保守保留所有更早实例，尚不能让无关分支在消费者BUSY时独立推进。
