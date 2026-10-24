# T14 owner 生命周期启动草稿

协调器七组已完成公开ABI适配与固定轨迹守卫：最终8普通/UBSan构建和执行、56组次/1096事件，223隔离门禁自测通过；停止后的注册/业务拒绝和双方owner在途引用可逐事件核对。最新快照19项入口均已有其范围内证据，更早owner语料仍待审计，完整异步/分支/预算G04轨迹、其余ABI守卫、G07与生产准入仍未完成，GAP-010/T14不关闭。

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

## 异步 disposer 宿主候选（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-async-cleanup-wip-zn5snc8s`；async-evidence.json SHA256 `524b4bcded601442a22d6bcd461829701fcee0c457c86a160e7cf119bd5e6526`，记录全部源码/测试脚本摘要及O0/O2编译执行。基于上一版分叉，未修改历史包。owner宿主草稿从80字节扩至104字节，resource从16扩至24字节；这是尚未准入的草稿结构变更，不更改已批准T10/T11/T12/T13原生ABI。协调器/适配器同步检查新尺寸。

新增register_async和cleanup_finish。异步disposer返回5表示等待，其余返回值表示同步完成状态；若通过票据内联完成，begin仍须返回5，否则记录协议错误10。票据包括owner地址、epoch和本轮清理序号，必须复制后异步持有；等待期间owner留在Quiescing，清理资源已弹出但等待记录仍可见，不得重复调用begin。finish仅记完成状态，不在完成回调栈内推进清理；宿主再次stop消费结果，按LIFO继续。票据重复、前一清理序号、旧epoch、错误owner和元数据别名均有拒绝测试。释放owner存储前必须等所有调用和清理完成，手动清零/释放再复用不受此票据保护。

O0/O2各执行原有675个图生命周期场景，另8个异步场景组通过：两个延迟清理串行推进、内联完成、同步失败、内联完成与返回冲突、在途操作先退出后才发起清理，以及双提供者计划下正常停止/加载失败/重入取消三种异步消费者回滚。清理错误和加载错误分别保留；5既可能表示等待，也可能是已完成回调的错误码，协调器以owner状态判别静默，不能单看返回码判断Disposed。异步完成后仍未再次stop时不声称已完成释放。

同一Agent实施和自审：批准继续宿主实验，尚不批准原生ABI/模块/生产准入。参考Harness vendor第6项owner-visible-until-quiescence与Cordis fiber消费者退出前保留provider的语义；没有声称完整Cordis兼容。未实现异步setup的协调协议、跨线程/事件循环调度、完整别名检查和冻结工具链验证，分支停用当前仍保守阻塞更早无关实例。

## 异步 setup 完成与取消（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-async-setup-wip-wuur71bl`；setup-evidence.json SHA256 `cb7c52f6ae70513cad3110da2145655747aa56c7806991ddf66cfc7af2fac17a`，源码/测试脚本摘要清单的固定SHA256为 `2904952a54507c724a2701887e675cb47b6ac93f80d1c3d3055aadd48bf85594`，同一Agent自审记录见setup-review.json。复用上一版owner104/resource24，扩展未准入的runtime草稿；未修改已批准原生ABI、门禁、预算或planner。新增defer/finish/pump，runtime由零帧进入Starting/Active/Stopping/Disposed；不得在存活票据期间清零或释放runtime。

setup回调内defer取得runtime+owner操作票据后返回5，操作在异步等待期间继续计入owner.inflight；finish可内联或迟到，仅记录状态，pump消费操作并继续激活或回滚。重复defer/finish、错误runtime/epoch/generation均拒绝。defer后回调返回非5记录协议错误10，但仍等待显式完成，不能把实际异步工作当成已经退出。普通同步setup返回5是已完成的错误而非自动延期；调用者须结合phase、setup_waiting和started判断是否仍有工作。

stop在setup执行/等待期间立即将当前owner置Quiescing，拒绝新的资源登记；setup操作尚存时不能清理其资源，也不会过早释放提供者。取消后迟到成功只允许回滚，不能恢复Active。setup失败后若disposer异步等待，协调器仍保留当前实例，清理完成后继续逆序回滚；加载首错与清理首错分别保留。

O0/O2各1414个异步setup场景通过：在95个有效固定图的每个激活位置注入异步成功、失败、外部取消、失败后异步清理、内联完成、defer与返回值冲突、setup内部重入取消，并加app/session与MANY正反提供者顺序。原675图生命周期场景和8组异步清理场景均在新源码上两种优化重新通过。测试验证完成之前无后续setup/cleanup，完成仅记状态不自动释放，Pending不启动，消费者清理时沿真实binding核对提供者仍Active。

首次回归曾发现旧异步清理夹具在stop后登记资源，新规则正确拒绝；已把该夹具改为先登记全部资源再stop，保留原释放顺序和断言，并新增stop后注册/再次defer拒绝断言。完整范围仍待：可信静态entry到setup ABI、服务新调用入口与清理调用区分、全面别名/几何验证、工作量预算、图分支独立停用、冻结原生构建及G04故障注入；单线程显式完成不等于事件循环或跨线程并发已实现。

## 依赖图分支停用（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-graph-stop-wip-240i7pzv`，graph-evidence.json SHA256 `5a121e30b7ebf95b0188422a4fb867bb3bb17d9a9a754c90fcfc6f01d51e1bef`，源码/测试脚本摘要清单SHA256 `381ba64db86820fa1600ecd6243c4554eb100500a21f8e6500f17ee0a46b19ee`；同一Agent自审见graph-review.json，结论仅继续宿主实验。owner/resource不变，runtime草稿增加借用stop graph与已启动前缀长度，另用started记录仍存活实例数；图模式下started不再表示连续前缀。

runtime_prepare_stop_edges从同一stage6计划的resolved bindings映射消费者/提供者的激活位置；必须先以相同不可变准备视图成功构造启动列表。拒绝越界/非拓扑映射，容量/预算不足不写最终边与数量；多个服务映射同一实例对可保留重复边。初始化时检查disposed数组全零、边的provider位置严格小于consumer，避免不可信顺序导致提前释放。全部借用数组必须存活并保持不变，当前仍非元数据真实性认证或完整别名检查。

停止以实际已启动前缀为范围，自后向前扫描。一个owner只有在所有已启动直接消费者Disposed后才进入自己的清理；等待中的消费者保留其传递提供者，但不阻止无关实例。disposed位图逐个记录完成，避免重复调用；清理错误保留且继续扫描。等待异步setup时也能清理已启动的无关分支；setup回调仍在栈内时只关当前owner入口并返回BUSY，回调返回后才能驱动其他回调。尚未启动的消费者不阻止失败回滚，Pending仍未激活。

O0/O2各1010个图停用场景通过（固定95有效图逐激活位置，加app/session与MANY正反顺序），每次沿真实binding计算保留闭包并逐owner对照：548次有无关分支提前完成、224次保留至少一个提供者祖先、83次异步setup未完成时独立分支已退出、155次在另一owner等待时观察清理错误且继续清理。覆盖在途调用、异步disposer、异步setup取消、同步setup失败、清理首错；反复stop和结束后的stop不重复释放。所有消费者清理仍断言其实际提供者Active。

原1414异步setup、675基础生命周期及8组异步disposer在新源码两种优化均复验通过。新边映射在每个有效固定图遍历不足预算验证完整输出不变，并核对每条映射边与原始requirement/provider owner一致。尚未实现每次调度工作量预算、服务调用阻断/清理权限区分、完整几何检查、正式entry ABI、冻结原生构建与完整G04；本地宿主结果不能放行生产。

## 调用入口与清理作用域（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-call-admission-wip-ufub_1qi`；call-evidence.json SHA256 `5579354472e1649f9440ad15469370591d7c0fc8c27fc344f71893ddc197c544`，源码/测试脚本摘要清单SHA256 `fecf6ec73f94c7ecf8efef4a7b00be0f7f308a284f1aeae794e72b762fc07531`，同一Agent自审见call-review.json。新增calls.h/c；仍未准入的owner草稿尺寸104不变，原reserved字转为cleanup_callback标记；不是已批准原生ABI变更。

按已解析binding索引进入调用，同时持有消费者和提供者的操作票据；提供者配额不足时归还已取得的消费者操作，不泄漏在途计数，失败不发布调用票据。正常业务仅在runtime Active且未请求停止时准入。setup调用须有当前callback/未完成异步setup的有效作用域，消费者可为Loading；停止后同样拒绝。清理调用只在Stopping且对应消费者清理作用域仍有效时准入，并沿真实resolved binding保留提供者。完成调用先验证两个操作均存活再释放，复制票据重复完成、篡改其中一个票据不得部分扣减另一个计数。

清理作用域以owner/epoch/每资源清理序号标识：同步回调返回即失效；异步disposer显式完成后即拒绝新清理调用，即使宿主尚未再次stop。已进入的调用仍可正常退出。owner新增cleanup操作入口允许在受管清理中计数；即使同步清理回调已经返回，遗留在途调用也阻止owner继续清理/Disposed，因此提供者仍可用。完成票据不声称能恢复已释放存储，所有runtime/owner/元数据必须存活。

O0/O2各207个调用场景、1776次测试提供者实际调用通过，覆盖固定有效图的全部resolved bindings及app/session/MANY正反顺序；setup调用、停止后业务拒绝、跨消费者清理作用域拒绝、同步清理作用域过期、异步清理显式完成后拒绝新调用、清理回调返回仍保留调用、错误/重复完成以及消费者/提供者配额耗尽和回滚。原1010图停用、1414异步setup、675基础生命周期及8组异步disposer在新源码两种优化复验通过。

这是可信原生宿主的生命周期准入接口，尚非完整服务vtable路由或参数ABI；测试函数在准入成功后才调用，错误分支验证未执行提供者。裸函数指针/owner API仍可被可信原生代码直接调用，不能把这些测试说成沙箱或对恶意插件的权限隔离保证。尚需正式entry/服务ABI、调用入口的模块边界/导出约束、全面别名与几何检查、调度预算和冻结原生验证，T14/G04未整体验收。

## 可暂停的停用预算（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-stop-budget-wip-9mb6a199`；budget-evidence.json SHA256 `df187b83071e5ca8f135eddaa2223ccaa530f30705c32ed536fb9ba7b05d1b18`，源码/测试脚本摘要清单SHA256 `b89759a544099c63741c1110b224b7a51d1372465191e40440b7761866c1f004`，同一Agent自审见budget-review.json。新增owner_stop_budget/runtime_stop_budget，复用owner104；runtime草稿追加节点/边扫描游标。pangu_stop_report24记录work/state/pending/reason，reason0终态、1预算耗尽、2等待在途操作/回调/异步完成。清理错误1/2/5与控制返回码重叠时，必须按report区分，不可单凭整数判等待。

预算覆盖宿主状态迁移、节点/边访问、资源回调发起次数；budget0在尚未完成时不改owner/runtime帧，返回耗尽报告。非零取消门闩是一个常数时间步骤，同时关闭业务入口和当前setup owner注册，之后才允许预算让出。owner逐资源LIFO暂停；runtime保存当前节点与边游标，预算耗尽不重新从头扫描。完成一轮仍有等待返回WAITING，再次调用重试；节点清完但终态提交尚无预算时，下一步直接提交终态，不反复扫描已清理节点。图模式与保守前缀模式均验证可用1单位预算最终推进。

O0/O2各3172个预算场景、47798次有界推进通过，量子1/2/3/4，覆盖固定95有效图及app/session/MANY组合、正常停用、清理错误1/2/5/-94、在途调用、异步disposer与setup取消。每步核对work不超量子、回调增加数不超work，预算0帧不变、终态不重复回调；4资源owner需恰6工作单位（关闭入口、4次回调、Disposed）。报告别名owner帧在修改前拒绝。首次编译出现未使用变量告警，已去掉无用绑定，未放宽警告。

207调用/1776次测试提供者调用、1414异步setup、1010图停用、675基础生命周期、8组异步disposer在最终源码O0/O2均重新通过；五份证据的完整源码/测试摘要清单已逐项核对相同。

限制：工作单位不是微秒或CPU指令预算，不能抢占回调；单个插件回调无限运行仍能阻塞线程，尚不能据此宣称执行时间硬上限。启动pump、操作槽查找等尚未量子化；旧无预算wrapper仅保留宿主回归兼容，不作为已批准生产入口。调用者仍须保证完整几何/别名/存活约束；正式ABI、冻结原生构建、性能基线与完整G04/G07未完成。

## 运行时元数据区间保护（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-runtime-geometry-wip-6uz0z8ah`；geometry-evidence.json SHA256 `8db9d4661d5d45dbfcc8f92f0bda56600ab92417395dbd10909757fd42cfdfbe`，源码/测试摘要清单SHA256 `dca2c1b0771b4bd79ec99c2e0f2c28387fee52415e7ec626065064b4733d137f`，同一Agent自审见geometry-review.json。新增私有geometry.h、runtime_bind_geometry和runtime区间索引字段。启动必须先绑定索引；旧场景夹具统一在真实runtime_start前构造索引，未把保护做成可跳过的成功路径。

构造时在写scratch前检查scratch不覆盖输入，登记runtime帧、实例表、每个owner帧/资源池/操作池、停用图描述符/边/Disposed数组及整个索引存储；预算内排序、拒绝区间重叠，成功后才发布索引，失败时runtime不变、索引scratch可丢弃。布局须在任何runtime API使用期间保持稳定并存活；不能换owner资源池、改指针/长度、释放借用数组后继续使用旧runtime。绑定排序尚非可暂停初始化，预算不足需重试，未宣称整个启动阶段已有调度量子。

运行中票据、scope、调用结果和stop报告在状态修改前检查地址溢出、对齐及与全部已登记区间的重叠；调用scope与输出互相重叠也拒绝。二分查找的区间数量由构造器限制为u32，单次查询最多32轮，不逐owner扫描。stop报告中的work仍计可变停用工作，固定范围/索引查询校验开销不计入该计数，更不代表CPU时间预算。独立owner接口补充帧/池/票据对齐与scope/output重叠检查。

O0/O2各98个几何场景、600次覆盖全保护区间的拒绝矩阵、77611次拒绝通过；测试保存受保护区间列表和逐字节快照，对每次无效接口调用比较全部受保护字节。另遍历构造不足预算，验证runtime不变；覆盖索引scratch覆盖帧/资源池、重复owner池、错位帧/索引/票据、长度溢出、索引自保护和scope/output别名。首次const参数与测试宏命名冲突均在保持-Werror下修正。

207调用/1776次提供者调用、1414异步setup、1010分支停用、3172预算场景/47798步、675基础生命周期及8组异步disposer在新源码O0/O2重新通过。宿主UBSan补充实验也执行同一98几何场景/77611拒绝，退出0且无诊断（receipt SHA256 `75e4473267d3fa5804a24691e38a930215f50deccce158eebe557c2e386d3603`）；六份常规和一份补充证据的完整源码/测试摘要清单逐项相同。不是冻结原生sanitizer/G07验收。

仍需：T13准备适配器写出对源plan/storage的完整别名检查、启动/槽查找预算、正式entry/服务ABI、私有owner入口的模块导出约束及冻结原生构建。有效映射和可写存储仍是C调用方前提，数值区间不能证明映射存在；可信原生代码仍可直接改内存或绕过宿主，未提供沙箱防护或服务器侧防绕过保证。T14/G04/G07整体保持未完成。

## T13 适配器事务写出（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-adapter-geometry-wip-df1z88o2`；transaction-evidence.json SHA256 `6d0ec7dab1cbcf8938d225e9811ee46e66010b73b406eaac099f841f94380c6d`，源码/测试摘要清单SHA256 `20e9107f0c1d80701ab7021ec918e5151bdf374096f6aa39d963487bae073afa`；同一Agent自审见transaction-review.json，仍仅批准继续宿主实验。新增runtime_prepare_storage32、runtime_prepare_info24及runtime_prepare_lifecycle；尚未批准原生公开ABI。

合并接口在提交前检查全部已借用输入/输出的范围、对齐、长度溢出、输出相互重叠和输出覆盖源数据：准备结果、graph输出描述符、registry各数组、lowered实例/需求/provider owner数组、graph输出数组完整声明容量、按实例source表、合格实例owner/资源/操作池及storage描述符。Pending条目只保护source表本身，不解引用其不参与启动的owner指针；未传入的原始manifest、工作区和其他对象不能声称已检查。

几何检查、两阶段语义dry-run和完整提交预留共用剩余fuel；全都成功后一次写启动列表、依赖边和数量/work。任何错误或不足预算保持三个输出完整不变，不出现先发布列表后边失败的半成品。两份旧适配器也补充其实际可见输入的几何检查，但它们无法检查未传入的另一个输出或owner；后续正式宿主应采用合并入口。源视图在验证/提交期间必须单线程不可变。

O0/O2各98次真实T13准备→合并适配→runtime启动/停止通过，包括95个有效固定图、app/session与MANY正反顺序；Pending未启动。另拒绝145张循环图，6432次输出覆盖源或输出相互别名/溢出/错位尝试，遍历12127个不足共享预算值。每次无效输入用源区间逐字节快照及整个目的数组/结果poison检查未修改；成功时核对每条owner与binding映射、未用目的尾部保留和实际setup/cleanup次数。

既有207调用/1776提供者调用、1414异步setup、1010图停用、3172预算/47798步、98几何/77611拒绝、675基础生命周期及8组异步disposer在最终源码O0/O2复验通过。合并适配同样在宿主UBSan退出0、无诊断（receipt SHA256 `84ac25e81ea0257b34a7426c71662a07cf1f4144b5ac3f829edd9c42f5ee638c`）；七份常规加一份补充证据的完整源码/测试摘要一致。另以宿主ld -r合并四个组件对象，nm -u为空，记录host-object-closure.json；仅证明该宿主对象闭合，不代表冻结工具链/原生准入通过。

接下来仍需：启动/槽查找的有界推进、正式entry/服务ABI与内部导出约束、冻结工具链构建/原生guard及完整G04/G07。数值区间不证明映射有效或可写，裸原生内存和回调仍是可信边界；没有对恶意插件的沙箱保证。T14整体仍未验收。

## 启动预算与完成后重扫（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-start-budget-wip-u515221l`；start-evidence.json SHA256 `f2ab832c4beb8ce7b914b84c97f01f79dd30511e5c72ca861473f02ffb1a28aa`，源码/测试摘要清单SHA256 `b1ffe5daeb9f159b90a148c3a76980d1f932e67d6eb54b56328e10c5c319b9b9`，同一Agent自审见start-review.json。新增runtime_start_budget/runtime_pump_budget、选定槽位的owner操作入口和runtime_disposer_finish；runtime草稿保存校验/槽搜索游标。仍不是已批准原生ABI。

预先绑定几何索引后，phase5按预算遍历Disposed数组、依赖边、owner初态和重复owner，全部校验完成才进入Starting；校验失败为phase6，任何setup前拒绝。Starting逐槽搜索并保存候选槽，取得槽位/执行setup前再次检查，setup回调与同步完成占一个调用步骤，异步setup完成另有消费步骤。预算0不改帧，量子1也能从游标继续。失败/取消后用同一剩余fuel进入既有停用推进，不借用无预算回滚；调用前取消不取得操作票据、不执行该setup。

新增测试确实暴露过中途完成的唤醒缺口：停用扫描已越过提供者，setup才完成，旧游标可能又报告WAITING。保留失败回执start-initial-failure.json/start-midpass-wake-failure.json，未放宽done断言；runtime级setup/call/disposer完成现在发出重扫标记，重扫本身消耗预算。当前节点清理回调内已会被观察的完成不额外重扫；终态清除排队标记。调度器管理的完成应走runtime包装，原始owner操作是内部/独立单元接口，不能据它绕过运行时唤醒。内部completion hook尚待正式门面隐藏。

O0/O2各6056个启动预算场景、103692次有界推进通过，量子1/2/3/4，覆盖正常启动、逐实例失败、异步成功/取消、重入取消、失败后异步清理、退休槽跳过及槽全部耗尽，并含图/前缀两种停用模式。另285次在校验期或槽已找到但回调未调用时取消，114次最后owner或边校验失败，均验证相关setup未执行。每步setup/清理回调增加数不超报告work，工作量不超量子，完成前不执行后续setup。

九份常规脚本在最终源码O0/O2通过：675基础、207调用/1776提供者调用、1414异步setup、1010图停用、3172停用预算、98几何、98事务及新启动套件。停用推进实测48234步（历史47798步后新增有预算的完成重扫，不是扩大每步量子）；几何新增三个接口的拒绝矩阵后为102943次拒绝。98事务仍拒绝145循环图、6432别名和12127不足预算值。宿主UBSan执行同一启动套件退出0、无诊断（receipt SHA256 `926e14ceaf04dfd5c33a7d54a65610e27ddbb3ca2823617cbeb86ed6a26ff0ce`）。十份证据源码/测试摘要一致；全部常规脚本逐次比对C/H/Python源码不变。继承的run-host-tests.py会重写夹具，已改为仅编译运行，旧生成器保存于实验目录外。

尚需：正式entry/服务ABI、内部符号约束和冻结工具链/原生guard/G04/G07。索引构建是有fuel的重试式准备，未实现可暂停初始化；普通调用的通用操作槽查找仍是旧接口，启动已单独逐槽计费。预算不是CPU时间承诺，不能抢占单个原生回调；T14整体保持未完成。

## 通用调用槽预算（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-call-budget-wip-n3thrj9c`；call-budget-evidence.json SHA256 `fbbba58d44290087bda7181e32a3f434206bc291c33e0db92ae5e46876ac7ae0`，源码/测试摘要清单SHA256 `aa90a6a339bb525ef862896697e6e743c06c19ba0aa1a58b5dd810d114b9e9ce`，同一Agent自审见call-budget-review.json。新增业务/setup/清理三种budget准入入口，owner只读预检与指定清理槽入口；owner/resource/operation布局不变，均仍为未准入宿主草稿。

有效单线程布局下，先计费预检两端owner，再逐槽寻找可用代际，预留commit步骤后才登记消费者/提供者操作并发布票据；不在预算不足或任一端满额时先占消费者再回退，因此这些失败不会消耗消费者代际。报告复用work/state/pending/reason：reason1/pending1为预算不足，reason0/pending0加返回2为已确认槽配额不足。报告、票据、输入scope必须互不覆盖且在运行时保护区间外，预条件错误不写输出。

这是原子重试接口，不是持有部分资源的异步请求：预算不足没有挂起调用，重新调用需重新检查两端；例如极小固定预算不会跨调用累计扫描进度，应增加预算或放弃请求，不能把pending解释为已有调用。已有setup/清理scope若已过期则必须重新取得合法scope。每次逻辑工作量有界，不抢占提供者回调；源帧与映射有效性仍是可信C调用方前提。

O0/O2各98组合、276次准入/276次实际提供者调用、1769不足预算拒绝、138次消费者或提供者满额拒绝通过。遍历每次成功前的全部不足预算值，以两端所有owner/操作池/资源池、runtime及索引的逐字节快照验证未改动，票据poison保留、提供者未执行。覆盖setup/业务/清理、退休槽跳过、错误票据原子拒绝和scope/报告/票据别名；满额拒绝验证不消耗任一端代际。首次RESULT宏参数误替换reason字段导致严格编译失败，改参数名后-Werror不变。

最终十份常规脚本O0/O2通过且源码不变：6056启动/103692步、3172停用预算/48234步、207既有调用/1776提供者调用、1414异步setup、1010图停用、8组异步disposer、675基础、98事务/6432别名/12127不足预算与新调用预算。几何矩阵加入三种预算入口两个输出后为153607次拒绝。新调用预算宿主UBSan退出0、无诊断（receipt SHA256 `138448777051ab79cdc14ecc1010313e43321437f2f49b9ae3245df30723175a`）；十一份证据源码/测试摘要一致。宿主组件合并对象nm无未解析符号，仍不能替代冻结原生构建。

接下来：正式entry/服务门面ABI、内部导出约束、冻结工具链/原生guard及完整G04/G07。旧MAX包装仅供兼容回归，正式门面不得以其绕过已声明预算；几何初始化仍为预算内重试准备而非可暂停初始化。T14整体未验收。

## 版本化 host-control ABI 候选（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-facade-wip-xgs4cmgp`；facade-evidence.json SHA256 `79848c3aa2029d97febdb335bac201c9d5b3a34bb644d73578a3b98c2f50003f`，源码/测试摘要清单SHA256 `25e6b22a022cd2fdb13f227ecc4e3bacf69975e88694665ce9ed7c6a0c7f59d8`。新增根控制记录 [t14-facade-candidate.json](t14-facade-candidate.json)，只登记ABI候选元数据；bootstrap-policy仅增加该control文件，不新增生产模块、不改门禁/预算。上述JSON与policy固定控制对摘要 `4bcba29febf41712e8d8cc1d2bdca4e4d2e2abf7347f463a8d81607ed872d2cd`；同一Agent实施/自审记录facade-review.json，仅批准继续宿主实验，非独立审核或生产放行。

C17/SysV Linux x86_64候选：ABI1、target1、flags0，函数表152字节/17方法，唯一外部导出pangu_lifecycle_query。版本/尺寸/目标/flags不符返回空。公开类型均有前缀，15个类型尺寸及17个方法偏移编译断言；原内核按记录机械收拢为静态实现，旧MAX驱动/分开的准备接口不进入表。owner初始化先预留操作池数量+1的fuel；资源登记门面只允许当前Loading setup或Active运行期，停止门闩关闭所有登记，即使提供者仍Active供消费者清理。cleanup scope也通过runtime/position检查。

固定消费端client-abi-v1.h SHA256 `c09d775d49f28eebf8965188c1abebabcd0630f684458810fbe577eae6934a07`，及bootstrap/host-plan两份依赖头摘要，运行前核对；provider按当前头编译，C消费者与汇编查询桥按固定消费头编译，避免提供者与消费者同时漂移。生成器不更新固定消费头；任何后续变更须显式控制审查，不能把重新生成消费端当作兼容通过。外部生成器在另一目录重放，lifecycle.c/h逐字节一致，所有父内核输入摘要核对相同。

O0/O2各1152门面场景、1924次提供者调用、5772次登记拒绝通过，覆盖T13固定图、app/session、MANY、正常启动停用、异步setup取消和失败后异步清理。17方法均经函数表使用，C/汇编查询一致；错误握手拒绝和初始化不足预算保持帧/操作池不变。两个优化级别provider对象nm全局仅query且无未解析符号，另18次直接链接私有/旧驱动按预期失败。首次符号检查确实发现单行旧包装未删除，修正生成器并保留initial-export-failure.json，未放宽白名单。初版回放驱动的共享exec变量遮蔽也被拒绝作为证据，改用隔离命名空间与不同路径断言后重验。

十二份常规脚本通过且源码不变，所有既有6056启动、3172停用预算、276预算调用、207旧调用、1414异步setup、1010图停用、98几何/153607拒绝、98事务、675基础和8组异步清理复验通过。门面宿主UBSan执行同一1152场景退出0、无诊断（receipt SHA256 `4a2bb5e3d4ef1c68aec8ff9321bee63706683046f9b361ff870053b01a726ab8`）；十三份证据源码/测试摘要一致。

仍未完成插件静态entry(void*)到owner-aware setup的桥和真实服务值路由；未验证C++互操作，未完成冻结工具链/原生G03/G04/G07。函数表是可信宿主控制ABI，不能阻止同进程原生代码自行读写内存或伪造数据。候选元数据及单一导出不是生产模块准入，T14整体仍未验收。


## 静态 entry 生命周期桥候选（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-entry-bridge-wip-9d3q3ded`；源码/测试清单SHA256 `21205ba9a4e6fdbe1846bfe3b5f975a99983b03a3900de9da2ef29453cd284c8`。根控制记录 [t14-entry-bridge-candidate.json](t14-entry-bridge-candidate.json) 固定每份源码、回执及同一Agent自审记录的摘要。entry-evidence.json SHA256 `dd740568b58cbbc03160b82c848c661ab3d100a7762c910e935aa8a92c075a9e`。新增控制记录与bootstrap-policy登记构成的固定控制对摘要 `471981c8c1a77a95eca1fb2da7b52a69ddb122c9a3f3a4c234e964a03d4418c1`；这是两个文件的摘要，不是整个控制器批准。门禁、工具链、预算、旧生命周期实现/头均未修改，不新增生产模块。同一Agent实施并自审，仅批准后续本地宿主实验；历史批准保留。

新增单工厂 pangu_entry_bridge_query，ABI1/target1/flags0，表24字节、上下文64字节、记录80字节、请求80字节、存储48字节、结果16字节。独立固定消费头client-entry-v1.h SHA256 `43d3d326f5124409622fd019d91c3361cd776ae391ea39fd4e15994fc86e0bc1`，沿用冻结生命周期消费头和T13依赖头；提供者与C/汇编消费者分别编译。两种优化的桥对象全局仅工厂符号、无未解析符号。

准备阶段不执行插件：接收同一次成功T13规划的不可变manifest、installed表、entry_map及图输出，验证阶段/元数据/实例对应关系、Pending与唯一顺序、owner初态及全部输入输出区间，按实例写出记录和setup项。失败保持记录/items/结果不变，module_scratch可改变；预检、扫描和提交共同计费，不足fuel可重试。此层依赖调用者保存真实规划来源，不重复证明完整依赖图，也不认证指针来源。Pending记录不进入启动列表；当前实验要求其owner也初始化。

启动由生命周期协调器先登记setup操作，再调用私有thunk。thunk检查当前owner/epoch/position/回调窗口和真实操作票据，在任何外部函数表调用前置entered标记，拒绝重入，并在setup_scope返回后复查取消/代际。最后调用installed表选中的entry(void*)，返回值交给既有同步/异步协议；返回5但未defer按加载错误处理，避免无限等待。entered代表本次尝试已消费，不保证插件体实际执行。上下文和入口指针生命周期内不可由消费者修改，userdata内容由调用者负责。

O0/O2各4108场景通过：真实T13固定240图及反序安装表、app/session、MANY与Pending；90346不足fuel值、6555别名拒绝和输入逐字节快照；76错误映射、290失败计划、37380早/迟调用拒绝、8424插件体重入拒绝、412次替换setup_scope重入拒绝；异步取消/成功/内联完成、错误代际票据、同步错误及异步清理。每个优化级别24次实际汇编入口调用通过。6688计数是依赖租约内的状态探针，不是服务值查找或路由；前一节门面1924计数也应理解为测试探针。

最终13份常规脚本全部退出0，源码/测试未改变；14份源码绑定回执（含补充宿主UBSan）逐一与最终清单一致。UBSan执行同一4108场景退出0、无诊断，SHA256 `62c72e52ba29c6bfc45add98ef011011e712a62e8f927d6e19f0c78d4891a716`。保留原编译警告/超时约束，未改测试预算。参考Cordis core fiber的setup执行与dependent卸载等待、Harness vendor fiber的旧epoch加载前检查；本地显式票据/调度语义不是其运行时兼容证明，LIFO清理也不是其Promise并行清理的一比一移植。

仍需真实服务值注册/查找/版本校验与替换验证、受限插件SDK边界、C++互操作、冻结原生构建/guard及G03/G04/G07。当前上下文携带完整宿主控制表和可见运行时帧，仅用于可信宿主实验，不是同进程恶意插件沙箱。T14整体保持未验收。


## 真实服务值路由宿主候选（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-service-route-wip-to2c3ddj`，源码/测试清单SHA256 `fd6603392e4fe261cc5026a1d3a821e4177f316638b56e6c58692fabb6178f27`，route-evidence.json SHA256 `312988c2dbdf0c9fc7a59d970c988c1cf36c5fed49ba9f193cfa6197c263e229`。候选记录 [t14-service-route-candidate.json](t14-service-route-candidate.json) 固定证据与同一Agent自审摘要；该JSON和新增bootstrap-policy登记的固定控制对SHA256 `3a6e45fc63d8b939cef1e1d296d572164cc70070bf07f2fd13b4ebe2969463d4`。只增加候选元数据登记，不变更门禁/预算/工具链/基线，不新增生产模块。生命周期和entry提供者源码/头与上两包逐字节一致。实施与审核为同一Agent，仅批准继续本地可信宿主实验，T14整体不勾选。

单查询工厂pangu_service_route_query，ABI1/target1/flags0、40字节表、publish/acquire/release三方法。服务表通用头16字节、slot56、router64、查询64、租约80字节。服务接口版本与宿主ABI分开管理；服务按manifest的32字节稳定名称、准确版本和最小表尺寸核对，不把本地service索引作为外部身份。固定消费头client-route-v1.h SHA256 `fbe61d76765102b608b4c2d78cc3405dcf2ef206eea8e9cc5b5438125b829b71`；运行前同时校验旧消费头/T13头，提供者与消费者分别编译。O0/O2对象各仅导出工厂、无未解析符号。

提供者在当前同步或deferred setup中发布函数表和实例状态，核对owner/epoch、setup操作、provision归属、版本/尺寸/目标及初始全零slot。每个runtime仅发布一次，不在Active或取消后重写表；表须保持不可变，slot不拥有实例状态。调用者遍历已解析绑定，为每条边提供显式binding句柄和consumer身份，路由核对requirement/provider、稳定服务身份、激活位置和epoch，再经原生命周期表取得两端操作票据。只有准入成功才发布函数表/状态租约，实际调用结束后release；不能在release后继续缓存或使用表/状态。MANY每条边独立保留，app/session依据既有T13 realm选择，不默认取第一项。

setup、正常业务和cleanup分别使用对应作用域；停止关闭新业务，cleanup仍可调用保留的Active提供者。测试持有真实服务租约跨过stop开始，确认消费者Quiescing、提供者仍Active，实际函数调用后释放，再推进清理。异步操作可保存租约到真实退出时，但这里没有事件后端或自动取消/超时，调用方负责完成。没有跨线程同步。

几何预检覆盖router/manifest/plan/graph、entry记录、slot、运行时保护区间和Pending owner池；lease/report拒绝覆盖任何已发布表，包括未选中的提供者表。服务表发布也拒绝与宿主元数据/slot重叠。opaque state的大小/映射/生命周期由提供者保证，输出与它的别名不是机器可全面验证的范围。预检和表扫描、名称比较及生命周期准入共用fuel，原子重试失败不持有owner；不声称可暂停查找或性能已达标。

O0/O2各390场景通过：T13固定240图中成功图的正常/反序安装与同步/deferred组合，加app/session/Pending、MANY两种顺序和取消场景。共820次发布、888次实际服务函数调用，两个相同接口实现各444次；178次deferred加载、2次取消后发布拒绝；8605语义/重复/迟到拒绝，16518不足fuel和4884别名拒绝。调用逐项断言实际state/table与绑定提供者对应并验证结果；替换指在停止并排空的独立组合中使用另一实现，不是运行中热替换。错误名称/版本/尺寸、未发布、旧epoch、错误consumer和表目标均有拒绝/输出快照。

首次测试误要求跨图旧租约统一返回10；实际无对应binding可返回3。保留route-initial-failure.json，改为断言跨图必须拒绝且owner/池不变；同图重复完成仍明确要求10，未改实现错误码或放宽门禁。自审另补全“所有已发布表均受输出别名保护”，并验证延后setup可发布、停止后不可发布。

最终14份常规脚本全部退出0且源码未变；15份源码绑定回执（含route UBSan）与同一最终清单逐一匹配。UBSan同一390场景退出0、无诊断，SHA256 `b3595bffe3df3fb4f70d8820932fb22afbd41f4a6e28f785ab923c1dc3b97974`。既有entry4108场景及生命周期/预算/几何/私有链接套件保留。参考Cordis Service构造时reflect.provide以及fiber所保留的依赖实现、停用等待；本实现将TS对象/Promise转换为显式C函数表/状态与票据，不是Cordis API兼容层。

下一步仍需受限插件SDK/组合宿主接线、C++与汇编服务消费验证、冻结原生准入/guard及完整G03/G04/G07。当前仅可信宿主控制ABI，consumer字段与binding不是对恶意同进程插件的权限认证；没有生产放行或服务性能保证。


## C／汇编／C++ 服务互操作候选（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-service-interop-wip-6wdt8lfq`；源码/测试清单SHA256 `2f1b0012456f73b18a7044bab5b31c3824e8a88d67709e77c95d631de9bfa54b`，interop-evidence.json SHA256 `5032cdcf8894dcd6762824dedbaefb6eadfc96c31cf30b729ea78b6b3bee78d1`。根候选 [t14-service-interop-candidate.json](t14-service-interop-candidate.json) 固定证据与同一Agent自审记录。该JSON与bootstrap-policy新增登记的固定控制对SHA256 `b85877eda8999fda579b01f80b62219d5d792b4d1c5edc43247514f2d10ebb2c`；只批准继续本地宿主实验，不代表控制器全树、生产模块或完整T14批准。旧生命周期、entry、router的提供者源码/头与前包逐字节一致。

独立固定消费契约interop-contract-v1.h SHA256 `f2c1f98325967f959e5af7bbfea7db18540df880934a79ed138cbed86faafa0a`，不导入任何宿主控制头；服务头16字节、测试服务表24字节、方法偏移16，固定宽度标量/C ABI，C++使用extern C/noexcept。C++消费端通过对象字节表示读取C所有的表/函数指针，提供者通过字节表示读写实例状态，不把宿主对象假定为C++类。汇编消费端检查版本/最小尺寸/目标/保留字段/方法指针，并尾调用服务方法；额外汇编探针设置并检查rbx/rbp/r12～r15六个callee-saved寄存器，成功与错误出口均检查。

测试使用三种消费端（C、汇编、C++）与三种同接口提供者（两个既有C实现、C++实现），在真实T13输出、entry/setup、路由租约与逆序清理中执行。固定240图的95个成功图遍历提供者/安装顺序/同步与deferred组合，再加app/session、Pending、MANY和反序provisions；O0/O2各1188场景、34128次实际服务调用，9个提供者/消费端组合各3792次。包括546次deferred加载、3312次停止后或重复票据拒绝；持有租约跨过stop开始后仍实际调用受保留提供者，再释放并推进清理。每次核对返回值、实际table/state及绑定提供者；输入包含正负值和合法i32边界。

30次消费端拒绝覆盖错误版本/零版本、短表、目标/保留字段、空方法、空/未对齐表和空输出，验证未执行提供者且输出保持；扩展尺寸表接受。6次提供者错误探针覆盖原样返回错误及C++数值/计数溢出拒绝；服务错误不变成异常或自动重试。此测试服务仅证明边界及生命周期接线，不是Model/Tool等产品服务实现。

C++20对象使用-ffreestanding/-nostdinc/-nostdinc++、禁用异常/RTTI/线程安全静态初始化/unwind；新C++代码保持-Wall/-Wextra/-Werror，未使用缩进告警豁免。最终由cc链接，无-lstdc++；O0/O2 C++对象各只有两个C名称导出，唯一未解析符号为测试观察回调pangu_cpp_provider_observed，无C++运行库引用、.init_array/.eh_frame/.gcc_except_table。实际宿主工具为GCC16.2.1/GNU2.47，不能替代冻结clang/GAS构建或其他平台证明。

首次编译的C++缩进错误以加括号/换行修复，保留interop-initial-compile-failure.json；测试复用的嵌套main宏冲突改为可命名route fixture入口，保留interop-harness-failure.json。旧route独立运行默认main与断言不变。所有外部runner仅扩展源码摘要后缀以覆盖.cpp，未放宽断言/预算/超时；独立消费头固定摘要不由生成器覆盖。

最终15份常规脚本全部退出0且源码未变；16份源码绑定回执（含interop UBSan）逐一匹配同一最终清单。补充UBSan执行同一1188场景退出0、无诊断，SHA256 `a4c932dec331974c077a09bc51063139f3aed9b0c550be6bb2186a1f29f3bdda`；汇编指令本身不受UBSan插桩。既有route390场景/888调用及全部entry/生命周期/预算/几何套件保持通过。54份源码的确定性tar SHA256 `f52e8ea7e7cd94f97160a3f1042288558714efdf47a6fad6f7737913b0017d55`；另目录恢复逐字节一致，只是源文件恢复，不是恢复后重新构建或原生准入。

下一步将插件侧SDK与宿主控制头分离：插件不直接导入owner/runtime/router帧，按自身身份及稳定服务名查询，由宿主选择setup/业务/cleanup作用域，复用现有owner与票据。优先复用不透明票据表示，避免为了SDK再引入一套所有权系统。当前固定契约仍只是服务消费测试，SDK本身尚未实现；旧C17宿主控制头也不据此声称兼容C++。之后仍须冻结原生准入、guard及完整G03/G04/G07。


## 插件 SDK 基础候选与嵌套作用域缺口（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-plugin-sdk-wip-mv4bwxnu`；源码/测试清单SHA256 `930c3e2c22c770b779cc3c71489523f186852100461da65d0bc103ef5b7f830e`，sdk-evidence.json SHA256 `7e8d8ca3a37941b76be26d0df41ff84e356cdba9ca6eb2bf852b3eef64c004ad`。根候选 [t14-plugin-sdk-candidate.json](t14-plugin-sdk-candidate.json) 固定证据与同一Agent自审记录。候选JSON、bootstrap-policy新增登记、GAP-010登记三个控制文件的固定包SHA256 `e9e960b33f12deda44ba66a59535a7aa5f14d72bb835e5fa51fc3512d02f1940`，不是控制器全树批准。只批准继续本地基础实验，不改变门禁/预算/工具链/基线，不新增生产模块；完整SDK/T14及原生准入未批准。

插件头plugin-sdk.h不导入owner/runtime/router/entry控制声明，独立固定消费头client-plugin-sdk-v1.h SHA256 `4074828868833523526476ddb24991ee954c88722daa0f4849a8ca1968868607`。SDK ABI1/target1，表72、上下文32、key40、request48、不透明lease80/completion32字节。provide/count/acquire/release/defer/finish_setup/finish_dispose七方法；借用上下文由宿主按实例与epoch固定。按稳定名称/准确版本/显式ordinal查找自身声明与解析边；可选缺失count0，未声明查找拒绝；MANY保留完整顺序。上下文userdata由插件使用，opaque与完成/调用票据只能复制回传，不暴露可操作的宿主帧类型。不是内存沙箱。

宿主pangu_plugin_host_query为单导出，40字节host表、32字节bank、88字节binding。先做T13/entry/lifecycle准备及几何绑定，再原子bind到每个实例；由公共host trampoline进入对应真实插件entry，SDK一次性标记在任何注册回调前置位，并在外部注册后复核取消。声明的同步或异步disposer在entry前登记，加载失败也执行部分状态清理。当前每实例一个声明disposer，不声称通用effect登记SDK全部完成。 SDK entered标记代表进入尝试已消费；若外部登记过程中发生取消，插件体可能尚未执行，已登记清理仍可能运行，所以userdata和disposer须能处理尚未进入或部分初始化状态。延后setup与异步清理使用原有真实完成票据，不另造owner/操作池；完成要求拷贝的不透明token，旧epoch、错误代际、重复和跨实例票据拒绝。

宿主根据当前实例选择setup/业务/cleanup作用域；SDK输出拒绝覆盖host bank、全部binding、控制表、运行时元数据及所有已发布服务表。数值区间不能证明映射存在，opaque userdata大小与外部状态生命周期仍是调用方责任。key/request按自身自然4字节对齐检查，测试了仅4对齐成员，未强迫插件使用宿主8字节请求对齐。调用、完成与提交预留fuel后才修改状态；计数/查找为预算内原子重试，不能抢占插件函数。

为复用元数据检查，从旧service-route.c逐字节抽出私有route-geometry-private.h；提取体SHA256 `9e7e8908e131772ce5219d39f2dbd58fa367fde59ef1692d8a41c5390935b7b0`。旧路由O0/O2对象与父候选逐字节相同，公开路由头未改。生命周期与entry源码/头也保持原包。SDK对象O0/O2各仅导出query、无未解析符号；C/C++插件对象各四个C名称导出，只有三个测试调用/观察符号依赖，无libstdc++/异常/RTTI/全局构造或unwind要求。C++20插件与C17插件分别编译同一独立SDK消费源，并由cc链接。

O0/O2各824场景通过：T13固定240图中成功图的C/C++插件/安装顺序及四种正常异步组合，加app/session/Pending/MANY、反序provisions与八种故障/完成组合。1794次插件服务调用（setup598、cleanup608、业务588；两种插件语言各897），另168次业务期实际嵌套调用；198次延后setup、432次异步清理。6299语义/旧token/重复/跨身份拒绝、16428别名拒绝；7710次操作不足fuel、388次bind不足fuel，9803次元数据/owner/操作池/资源池/服务表/SDK帧逐字节快照检查。未把opaque配置状态当作宿主可全面保护的内存。

**GAP-010仍OPEN：** 当B在setup或cleanup中调用Active提供者A，A再用自身SDK上下文调用C，当前全局setup/cleanup scope属于B，不能成为A的新调用凭据。真实探针分别复现144次setup拒绝与144次cleanup拒绝；这些是缺口证据，不能计作需求成功。业务期168次嵌套成功不能代替启动/停用期间语义。新增本地check-sdk-nested-admission.py核对源码与可执行文件摘要、重新执行探针；本次实际退出2，sdk-nested-admission.json为BLOCKED，SHA256 `c0f5e1fbe2e65cf92bdbe435d6911727c2991d697e66e35cb9e5cb9bbef08a71`。普通回归退出0只代表已实现基础路径与拒绝不变，绝不放行完整SDK或T14。

下一步优先实现显式父调用凭据与作用域传播：证明父票据的提供者就是当前插件owner，核对runtime/epoch/代际和真实在途状态，传播setup/cleanup根作用域，继续遵守停止门闩及依赖方向；不能仅凭inflight非零或放宽全局phase取得权限。须覆盖多跳真实调用、错误/过期/跨runtime/重复父凭据、取消和失败释放，再让嵌套准入由BLOCKED转为通过。随后才推进完整SDK及冻结原生G03/G04/G07。

首轮别名测试把未发布表当成宿主元数据，count16字节输出在那里合法；保留sdk-initial-test-failure.json，将测试目标改为实际已发布表，未放宽实现。最终16份常规脚本退出0、源码未变；17份源码绑定回执（含SDK UBSan）与同一最终清单一致。UBSan同824场景退出0无诊断，SHA256 `6a25670f6d579d187b44af5f5087ff619fc61186e0cbccab21262ebdcff817dd`；本地嵌套准入仍退出2。64份源码的确定性tar SHA256 `35a6a910a771ac6391b1532b96eae6979cfa146b7ea2493a345b970bab0076c6`，另目录恢复字节一致，不是恢复后重新构建或原生放行。


## 显式父子调用宿主后端候选（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-parent-call-wip-cdogotao`；源码/测试清单SHA256 `be2f73b85116a512f7228beca501c45d92db78fa6fd20ed6bc35156f8d92e01b`，parent-evidence.json SHA256 `511bb883952eecddec43be5422132a027bfe2dda71bcb27cf59716c7934320b2`。根候选 [t14-parent-call-candidate.json](t14-parent-call-candidate.json) 固定证据与同一Agent自审。该候选JSON、policy新增登记及GAP-010剩余要求更新三个控制文件的固定包SHA256 `5fc7c6e6c14e5fea051c06c344eccd42281284b0ea1acf45b2714344a8df275d`，不是控制器全树或完整T14批准。未放宽门禁/预算/基线；只允许继续本地可信宿主实验。

保留生命周期ABI1/152字节表与旧头、17方法原语义，通过同一query增加ABI2/32字节扩展视图，显式转换返回指针到新类型。扩展含原ABI1表指针与enter_child。旧.call在startup或stop后的业务拒绝不变；扩展仅供可信调用管理器，不能直接作为插件SDK。内核核对父/根runtime、绑定、两端活操作票据、父提供者等于当前子调用消费者、根setup/cleanup真实票据、状态与停止门闩；仍要求子调用两端Active，按原有owner槽逻辑原子计费/取得票据，沿真实解析边向提供者走。

新pangu_invocation_query ABI1/48字节表，bind/open_root/open_child/close；caller提供56字节bank、每活调用168字节slot，无堆分配。80字节不透明公开句柄含bank身份/epoch、索引/代际和两端epoch，table/state须与已签发记录一致。根作用域只在原始合法route acquire成功时保存在宿主slot；子调用只能从活父记录继承根，不能由调用方替换mode。不可变parent/root索引与代际在签发时建立链，管理器证明来源，内核再次核对真实owner票据。此注册表保存原有票据及父子关系，不另造一套owner或在途操作。

父节点有子节点时close返回2且不修改状态；子节点排空后才能逐层关闭。子操作取得的原有owner票据继续保留两端资源，停止后的新普通业务、取消setup根、已完成cleanup根不因父票据仍在途而获得权限；先前合法取得的子调用可完成并释放。句柄复制不增加引用，调用者仍须等真实异步工作退出再close，未登记外部任务不是这套接口已管理的操作。host bank首次全零，复用需无活节点且epoch递增；slot代际饱和退休不回绕。

服务名称/版本/尺寸、作用域/MANY及输出几何检查从旧route acquire逐字节抽出为私有route-query-private.h，根与子查询复用；旧route场景/预算计数保持通过。固定client-lifecycle-child-v2.h SHA256 `725b2d8252538bf9526e79689635a38c3d1a72c4e3d73c9409b8365bff4c03d7`、client-invocation-v1.h SHA256 `bf370e97fe050f14c90ca3ead7577ec1798f7e980f164832177ecb829c387380`独立编译。O0/O2 lifecycle/invocation/route对象各只有自己的query导出且无未解析符号。SDK1源码与O0/O2提供者对象逐字节等于上一包，未绕过旧scope检查。

O0/O2各809场景、2248次实际服务函数调用：214次setup、208次cleanup、156次业务嵌套，最大descendant depth3（4层实际服务调用）。固定240图成功图遍历安装顺序及同步/deferred/完成/取消组合，另有app/session/Pending/MANY、反序provision、初始化失败、退休slot与单slot资源拒绝。父/子调用由测试宿主provider包装器显式传入incoming句柄；实际叶服务仍为原测试方法，这不是插件方法ABI已接线的证据。

12234次错误/过期/重复/receiver不符/绑定与runtime字段拒绝，596别名拒绝；609根及399子不足fuel，逐字节验证owner/操作池/资源池/元数据及bank/slots未变。620次父节点尚有子时拒绝提前释放，234次async disposer回调返回后持根调用，14次setup取消后新子拒绝，56次完成作用域后新子拒绝；同时验证已取得子调用在完成后排空、父资源仍被保留。8次代际退休场景、4次操作池或管理器容量耗尽拒绝。并发扇出允许多个子调用，但不超过原3个操作槽；测试保留配额并验证拒绝不消耗代际。

首次测试辅助函数与planner fixture重名，改测试命名并保留parent-initial-compile-failure.json。首轮把所有sibling获取都期望为成功，诊断显示cleanup时consumer已3/3；修正成功路径fixture只同时延后持有一个根，原操作容量不增，并单独强制满额/单slot拒绝及不变快照。保留parent-initial-runtime-failure.json与parent-quota-diagnostic.json，不把失败运行写成通过。

最终17份常规脚本退出0、源码未变；18份源码绑定回执（含parent UBSan）与同一最终清单一致。补充UBSan同809场景退出0、无诊断，SHA256 `d2608c10863fa9141d6c43faed41acb2c30fd12e0819535b79223d202840db5a`。73份源码确定性tar SHA256 `52b32cf7eed7a12eccaddc6d8d9e4ea1966566d368b3670302c4d1a4f9fa1306`，另目录恢复字节一致，没有据此声称重建或冻结原生准入。

**GAP-010继续OPEN：** SDK1没有传入父句柄的参数，仍实际复现setup/cleanup各144次拒绝；check-sdk-nested-admission.py本轮退出2，sdk-nested-admission.json为BLOCKED，SHA256 `38094c2a7c1c649ff1490e0e74a994ed5bbdb67d37ff18f8270eb8cfdef0df2d`。这恰好说明旧业务门闩未被放宽。下一步需保留SDK1兼容面，在新版本插件服务方法中显式传递incoming调用上下文，再由SDK从canonical实例身份调用受管root/child/close。不能靠全局current-parent或inflight计数隐式取得权限。SDK构造与所有输出接口也须保护受管bank/slots，构造失败不得降级到无凭据路径；还需实际双runtime、C/C++插件调用及冻结原生G03/G04/G07，才能判断完整缺口关闭。


## SDK2 版本化插件方法端到端候选（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-sdk2-wip-lyaw4e3h`；源码/测试清单SHA256 `171ccf01ea5e82b252ac79358a436ea8bef8616d4b4549c671f32763887254c3`，sdk2-evidence.json SHA256 `16ebbfff6adfc222a06c06c462051a808d0bd385f2732320271fbdaecd946acf`。根候选 [t14-sdk2-candidate.json](t14-sdk2-candidate.json) 固定证据与同一Agent自审。候选JSON、policy新增登记及GAP-010状态/剩余要求三个控制文件固定包SHA256 `5235ad123f41a7c3c3ee0f0438231fcfbf1baa541c64180e0c0c6269d59642af`，不是控制器全树批准。批准仅限本地候选实验与已实测语义，不改变生产阶段、预算、原门禁或冻结工具链；T14整体仍未验收。

SDK1公开/宿主头及查询布局保持不变，固定消费回归保留。SDK2为88字节表，以真实SDK1表作为首成员，另有acquire_child/accept；canonical上下文仍32字节，header明确ABI2。新增host query ABI2/48字节，96字节bank含受管调用bank；invocation query扩展ABI2/40字节，提供纯prepare_bind和incoming校验。旧查询错误版本/尺寸用例不删除，新旧入口严格匹配上下文版本，实际callback阶段4980次错版本入口均拒绝且状态不变，不能用ABI1入口代替要求SDK2的装载。没有声称任意SDK1/SDK2插件已能在同一组合混装。

公共请求仍只有稳定名称/版本/ordinal/尺寸。普通acquire从自身setup/业务/cleanup取根作用域；acquire_child必须传入被调用方法收到的父句柄，SDK推导自身实例，再由受管后端核对父提供者等于该实例。accept在服务方法效应前核对已签发操作的receiver、根与当前真实owner操作/epoch/代际；它验证已取得的操作，不再次授予新调用。count2只是声明视图，不授予权限；已在途Active提供者不带父凭据的新根请求，在startup/stop仍拒绝。父还有子时release返回2，不能提前释放。未登记外部任务和context/表/opaque状态生命周期仍是可信调用方契约，不是进程内沙箱或用户权限Policy。

构造先校验bank/binding/cap存储与所有输入几何/描述，再调用纯prepare_bind得到实际bind预算，预留完整构造+发布预算后才提交。预算不足、无效descriptor、别名、同epoch重绑、预检少报1单位导致的实际bind拒绝均保持原对象字节不变；不发布半成品SDK2上下文，也没有失败降级到SDK1的代码路径。所有SDK2输出额外保护bank、调用slot、两层SDK表及受管扩展元数据。

独立固定SDK2公开消费头SHA256 `53e8a0e62c670e270974a4707dc1fc9670420b84af49f14e37b50084ef73304e`，宿主消费头SHA256 `31632eba6da861d48dfb60facb42588af1e52047c11e2cf9a03189804b3ac328`；另锁定SDK1/调用扩展消费头及五份T13借用输入，运行前校验。C17与C++20插件分别编译同一源码，仅导入公开SDK头，客户端对象各六个C导出，仅有两个测试观察依赖，无宿主控制声明/查询导入或C++运行库、全局构造、unwind引用。SDK/调用管理器/生命周期对象仍各只有自己的query导出，无未解析依赖；正常测试用cc链接，不要求libstdc++。这是宿主GCC16.2.1/GNU2.47证据，不代替冻结clang构建。

测试方法实际签名为evaluate(state, incoming_lease, input, output)，方法本体使用SDK2接收并转发父参数，已不是宿主包装器模拟子调用。为避免改写旧测试接口，在fixture中独立复制manifest/规范合同/安装合同，将服务版本统一映射为原版本+100，保持名称/作用域/绑定语义；不修改原T13输入或references，不声称这些测试接口已是Model/Tool产品接口。

O0/O2各1196场景、3242次实际方法调用；342 setup、306 cleanup、208业务嵌套，4层调用。C与C++分别171/153/104次对应嵌套，函数指针身份也逐项核对。真实两个runtime可共享不可变T13计划，但owner/操作池/SDK bank/调用存储独立，交错持有活根时130次跨runtime accept/release/child请求拒绝，正常两端仍继续完成。678次错误父/复制上下文拒绝、6202别名拒绝、72次无父新权限拒绝、856次父有子时提前释放拒绝；12次取消后与12次完成清理后新子拒绝，已有操作仍排空。1792次构造不足预算及1200次构造语义拒绝；根897、子687、释放191、incoming校验191个不足fuel值，逐字节检查两runtime所有模型/owner/池/bank未变。188次异步清理根在callback返回后继续由实际插件方法调用。

新增本地check-sdk2-nested-admission.py实际退出0，SDK2语义PASS，SHA256 `1f9b80cb3602377cc22f3ba7ed4e0cbd566bbbabab44818c0b5d7715c03f7b7f`。它核对源码、固定消费头、T13输入、双优化、二进制摘要并重新执行，要求两语言三阶段嵌套、双runtime、停止/完成、构造/预算/版本拒绝等证据；启动时先写BLOCKED，畸形回执不能沿用旧PASS。8项隔离副本自测包括正常、缺回执/源码漂移/产物漂移/缺优化/缺场景/空借用输入及畸形回执残留PASS；未改变SDK1旧检查器，其按旧无父参数契约仍退出2，这是兼容诊断，不能冒充SDK2失败或强行改为通过。新检查仍是同一Agent可审查的本地证据，不宣称服务器侧防绕过或独立认证。

保留sdk2-initial-compile-failure.json（fixture命名冲突/未用字段）、sdk2-initial-runtime-failure.json（故障注入过早在cleanup callback内触发）、sdk2-initial-ubsan-failure.json（空fixture的NULL/零长度memcpy）。分别修正命名、确保post-callback触发并排空已完成但仍持有的操作、跳过空复制；未降低编译/sanitizer约束。另修正了两个printf计数参数顺序并验证错版本入口计数等于实际entry次数两倍。

最终18份常规脚本退出0且源码未变；19份源码绑定回执（含SDK2 UBSan）逐一匹配最终清单。补充UBSan同1196场景退出0无诊断，SHA256 `1d9e49c93ef3c94214dee3bea9e874df285a65a0626d6c68eb624081c4558c44`。86份源码确定性tar SHA256 `fd9fdd0d6c1a705f5b734ce6214d46fe66a288c34d64365fe3f9016d2a7342b1`，另目录恢复字节一致，未据此声称恢复后重建。参考Cordis utils.createTraceable的defSite/useSite区分、reflect.locate的committed依赖视图及退休绑定等待；原生实现把实例解析与调用生命周期凭据分开，不复制JS Proxy/Promise运行时。

GAP-010现为PARTIALLY_RESOLVED：本地实际SDK2方法、C/C++、双runtime和失败门禁已接通，冻结原生模块登记、clang/GAS组合、ABI guard/sanitizer及G04/G07仍待完成；不可关闭缺口或标记T14完成。

## SDK2 冻结静态链路补充验证（2026-10-21）

固定 SDK2 源码和 T13 输入只读挂载，在断网命名空间中使用既有冻结镜像 `9765bb42bfca5bce3787c83801b9201563d59f758b61a327bfb644c3737c1dc4`。先编译 14 个实现/C/C++ 客户端对象，再以 Clang 生成汇编、GAS 汇编、ld 静态链接原 SDK2 场景入口；没有修改任何固定实现或场景源码。补充记录目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-sdk2-frozen-objects-f24d69yv`，summary.json SHA256 `dc170c043248cf975f94301403776ff5d70d21869ee207c08540aa2fc446e088`，含同一 Agent 实施/自审结论、源码/产物/工具链摘要和失败记录，不伪称独立审核。

O0、O2、Oz 各运行 1196 场景、3242 次真实方法调用；完整 stdout 与原宿主 SDK2 证据逐字节相同，包括 C/C++ 的 setup/cleanup/业务嵌套和双 runtime 拒绝。三份产物通过既有 inspect_elf 检查：静态 x86_64 EXEC，无动态解释器/动态段、W+X 段或可执行栈，nm 无未定义符号。另编译 assert(0) 负例，实际退出 125 并输出断言位置，不能把失效断言当成功。

发现此前 GCC 对象证据没有涵盖的移植依赖：冻结 Clang 在 O0 对结构体赋值生成 memcpy/memset 导入。此次用明确标记为测试专用的字节函数、assert/printf 和 Linux write/exit 启动适配器完成静态链接，不链接 libc/C++ 运行库。这不批准把这些符号直接引入生产，也不意味着基础内存服务已解决编译器隐式调用；正式模块登记前须明确编译器内存例程的归属、导入许可与替换边界。

保留两次真实失败：第一版补充脚本未沿用现有 external-as 兼容参数而产生 prefalign/addrsig 拒绝，修正为现有 -fno-integrated-as/-fno-addrsig；第二版在 O0 成功运行后请求镜像未包含的 readelf，改用镜像已有 llvm-objdump 和现有 ELF 解析器。冻结镜像、原门禁和预算未改，没有过滤汇编或放宽告警。

该结果缩小了冻结编译/无 libc 执行的不确定性；尚未执行登记后的 native-admission 矩阵，也未完成新模块 ABI guard、正式 sanitizer、G04/G07。GAP-010 继续 PARTIALLY_RESOLVED，T14 不勾选完成。下一步先登记编译器内存支持与宿主/调用管理器/SDK/测试组合，再以受控原生构建和相同场景建立正式回执。

## 编译器内存支持候选与替换验证（2026-10-21）

编译器生成的结构体复制不经过插件 SDK，也不能依赖尚未启动的宿主解析器。本次明确 compiler.memory v1 为构建/链接时选择的底层模块：SysV x86_64 的 memcpy/memmove/memset，只有三项标准 ABI 导出，无 allocator、OS、业务、持久状态或运行时服务解析；同一组合必须唯一选择一个提供者。公开头只声明三个入口和 64 位 size_t，不带提供者依赖。有效借用区间和长度由调用者保证；memmove 支持重叠，memcpy 遵守标准非重叠条件。编译器标准 ABI 不能改成可失败的 fuel 接口，其工作量按 n 字节有界；受检 Buffer/配额仍由上层接口承担。

候选目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-compiler-memory-v2-t_u_9o41`，独立 GAS 实现和测试用 C volatile 循环实现。两套 manifest/export 策略分别使用相同消费测试，未放宽门禁。既有 native_build 使用原冻结 profile，debug/release 四次闭世界依赖/符号、Clang→GAS→ld、静态 ELF 和平台指令检查通过，四次执行均退出 0。每次覆盖 0～4096 全部 4097 长度、页头/尾复制与填充、65 个偏移 × 257 长度 × 双方向共 33410 个重叠移动场景，以及 5 次返回值/callee-saved/DF ABI 调用。测试 PAL 映射三页，仅中页可写；左右保护页各一次故意读取实际 SIGSEGV（-11），验证 guard 本身有效；正常用例执行后解除映射。

原 SDK2 冻结对象只读、逐文件摘要锁定，不重编宿主/调用管理器/客户端/场景；仅改链接选择的内存提供者和测试输出支持。GAS/C 两提供者在 O0/Oz 共四次执行，各 1196 场景、3242 实际方法调用，完整输出与原 SDK2 证据相同。说明底层提供者可在不改消费者的情况下替换，不声称运行时热替换。

8 项真实产物负例均由未修改的 link_graph 拒绝：两提供者 × 两模式下删除所需直接依赖，或撤回 memcpy 公开导出，分别触发 undeclared/private symbol dependency。四组构建的原产物按登记清单复制到干净目录，verify_build 重新核对源码/策略/所有产物/ELF/闭世界链接与符号边，均通过；这属于保留产物复核，不是源码归档后重新构建。源码归档 `a9f369b21eb0da700a2a7835da2c810e82b70a538706700a247a7e8bd392fc5d` 已逐成员验证。

保留三次失败：初始 C 测试直接取外部函数地址生成 GOT 导入，改为测试汇编显式 RIP-relative ABI 桥；初始执行错误地直接启动不可执行保留 ELF，重放按既有流程执行另存副本；guard 负例重复链接 probe_exit，改为仅引导 main 的独立启动对象。不降低告警、不新增 GOT 豁免、不改变冻结工具链或超时。

固定 summary SHA256 `377862e2096b2c36510bf53d7ff435b73e6db295e8362e4a1a62be0cc4e9019e`；控制包（仅候选登记 JSON 与新增文件登记）SHA256 `fd5befdd358de140f6baa9010f959d8954f3d60ea03b386fa2b544d1a2435fd7`。本次由同一 Agent 实施、自审并批准本地后续登记用途，具体差异/摘要/证据/风险见候选 JSON 与目录内 control-review.json，不伪称独立批准。尚未接入完整 T14 的 native-admission registry 与组合矩阵，旧 ABI/SDK 回归、正式 sanitizer/G04/G07/性能验收仍待完成。GAP-010 继续部分解决；不能据此完成 T14 或生产放行。

## 独立 C++ 测试插件的版本化准入（2026-10-21）

原 native-admission 只实现 C17/GAS，不能把此前手工编译的 C++ 插件算作矩阵准入。本次新增独立 `pangu.native-test-cpp-toolchain.v2` 与 `governance/native-test-cpp-toolchain.json`，保留旧 v1 文件、C17/GAS 参数和 linker 参数。只准许 `.cpp`/c++20 的 test 模块，product/development 仍拒绝；没有放行 Qt、标准库、异常、RTTI、unwind、隐式初始化/TLS或跨模块 C++ mangled global。实际对象节与符号检查还会在保留产物复核时重跑，不能删掉回执里的 C++ 证据冒充通过。

已有编译器输入采集仍检查每一份实际 include 和 GAS 依赖，只将原来处理 C 的精确 STT_FILE 元数据规则同样用于 C++，不丢弃真实依赖。完整门禁自测 197 项通过（原 181 项及新增 13 项 C++、3 项 admission），其中实际执行 C++ debug/release 构建/运行、两模式 admission 矩阵和保留产物复核，并验证旧 v1 仍拒绝 C++、非 test scope、危险参数/节/符号、异常/标准库、声明不符及缺失对象证据。G00 自测另运行 13 项通过。

新冻结镜像 `/home/xu-yuanhao/Documents/PanGu-artifacts/toolchain/frozen-cpp-jyz6w466/image`，SHA256 `23d6086651c009e3863556828331b6d6badb6065a336c90c6cc18fc59b824616`，profile SHA256 `04b3b0bef42d095cdebeff128bf0d4a76b5b2878d15a6e25511ee1f63d54eb8f`；1816 份工具/库载荷和 bwrap 身份与旧镜像逐项完全相同，只有新增配置的 profile 绑定改变，不引入 C++ 运行库。此镜像是本地工具链固定，不等于远端信任根已部署。

最终控制包 SHA256 `dd0e76769f6a1951d16a5353c111e78355a3300d3fa00f5c33b2ef0631559686`，逐文件摘要、差异、测试日志、风险和同一 Agent 自审批准在 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-cpp-admission-e367lwks`；未冒充独立审核。尚未接入完整 SDK2 原生模块矩阵：原测试客户端的两个观察函数反向导入主程序，与主程序导入插件形成环；应改为测试 config 中显式注入回调，保留所有原断言/场景并验证计数一致，不能把环登记为允许。T14/GAP-010 整体状态不变。

测试观察回调已在独立候选目录完成显式注入：sdk2_fixture_config 新增 expected/entered 指针，主程序按实例设置，独立 C/C++ 插件经 config 调用；删除对观察函数的反向外部导入。只改三个测试文件，生产实现/公开 SDK/宿主头不变。原 O0/O2 各1196场景/3242实际方法调用及完整输出逐字节保持一致，四个 C/C++ 对象 nm 确认无 sdk2_fixture_* 导入。此次没有通过允许依赖环来放行；完整原生 fixture 模块布局和声明图仍需接入后检查。

最终自审还补充拒绝 .init/.fini/.preinit_array 节族，并用实际 C++ 对象负例验证 preinit_array；最终完整197项测试通过，已保留先前196项测试/批准记录及最终覆盖它的固定包。不得拿先前摘要批准最终不同的文件。

## SDK2 正式登记的本地原生矩阵（2026-10-21）

候选 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-native-sdk2-uue0e85a`，登记 `t14-native-candidate.json`。保留T13全部20组合与原源码字节，再增加SDK2的GAS/C compiler.memory提供者两组合，共45模块/22组合、112份原生源码/头。原冻结v2配置的Clang→GAS→ld完成44次构建，native_matrix=PASS；隔离控制器按设计仍退出2、status/release=BLOCKED。固定controller `262c4b7675b820827fada3daf0129acd9acdfd7973f2a9225006b0a52ff0702a`，candidate `f042672bb95469ee81e9686cbed4c5d9204c8ee904ca63d7afb0b883b81915bc`，image `23d6086651c009e3863556828331b6d6badb6065a336c90c6cc18fc59b824616`。

真实执行共60次通过：保留旧分片/保护页负例，新增两提供者×debug/release四次各1196场景/3242实际方法调用，342 setup/306 cleanup/208业务嵌套、双runtime及取消/完成拒绝完整输出与原SDK2一致。不能把包含但未调用的旧SDK/route/entry测试函数算成专项回归已执行；这部分仍列为待办。

编译器依赖记录确认，C/C++测试插件各只包含自身TU、固定SDK1/SDK2公开消费头和测试config头。七个独立消费头移位后字节摘要不变；C++对象只有六个C-linkage入口，debug的唯一未解析导入为memcpy，由当前选定compiler.memory模块解析，不依赖观察主程序/宿主控制头/私有实现。route/invocation/SDK共享私有geometry/query辅助代码，保留一个实现模块和三个query导出，没有为拆模块把私有头改公开。启动、PAL和主程序分离；C/C++测试入口取地址使用明确hidden声明，以匹配静态PIC测试交付，不新增GOT豁免或修改生产公开头。

初始product范围实现到test内存替代者的声明被拒绝，新增T14实现现在明确test scope；未来生产提升仍需新审查，不能偷偷把测试提供者改成product。冻结控制器运行12项登记检查（1项正例、11项负例）：缺模式/缺C++插件/缺内存提供者、双提供者、绑定未选提供者、丢失SDK依赖、C++错误scope、SDK版本错误、未登记审查材料，以及客户端导入宿主/私有头均拒绝。

44组保留产物由admission.verify重跑绑定、ELF与闭世界符号核对；完整301文件源码归档 `24909a9935444acbf1945c3fcc121b5048db34fba1a24e730a01fd42d394afcb` 恢复到新目录后，字节/mode及preflight输入一致。未声称归档恢复后重新构建。原生源码已发布实验分支 `codex/t14-native-sdk2-matrix`，提交 `0d045531e284eb74abbe3111b9965f8084951c58`；Git树清单与每份blob匹配固定候选，远端回读一致。初次Git行模式引用中文路径导致清单比较拒绝，改为NUL机器格式后通过，保留失败记录；不降低文件核对。

结果见 `t14-native-evidence.json`；测试前固定控制器审查和测试后元数据包分开记录，同一Agent实施、自审。最终元数据控制包 `95c05a069af6a5ad2a5595ea830f2631145c4da7ab688dcf0247a884dceb521d` 不冒充原controller树pin。尚缺各宿主工厂/表的正式原生ABI guard、sanitizer、完整G04轨迹/故障矩阵与G07测量，以及旧SDK1/其他T14专项的原生回归。GAP-010保持PARTIALLY_RESOLVED，T14不勾选，生产/远端受保护放行继续BLOCKED。

## SDK2 原生 SysV ABI 守卫（2026-10-21）

固定候选 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-native-abi-guard-v3-6lq3cxme`，48模块/24组合/122原生文件，保留先前组合并增加两种内存提供者的guard组合。冻结controller `6a1ffa103d5ae1c7b423d507ef075e1daf227741d4d73e07075ef3da7e0e6bc0`，candidate `b61576173b18d755b1f32d3f7fab7e1a6876dc8ecb09a617aa7b58012e8db064`，image `23d6086651c009e3863556828331b6d6badb6065a336c90c6cc18fc59b824616`。48构建和64隔离执行通过；四次guard各保留原1196场景/3242服务方法调用完整输出，另检查146729次ABI调用、5工厂和52登记目标。32个宿主方法表槽以固定消费声明转发真实函数；SDK2的9方法及服务evaluate、两语言5个公开回调经测试config/独立适配器进入守卫，不替换canonical SDK表，不改变真实context。生产实现/公开及独立固定消费头逐字节未改。

守卫只适用于本阶段SysV x86_64整数/指针参数、最多13参数；保留完整RAX，再由i32适配器按低32位解释。不声称支持浮点/向量/variadic/聚合返回。汇编用完整64位模式核对rbx/rbp/r12～r15、DF、RSP恢复及六个栈哨兵；发现错位栈时在目标调用之前拒绝。每次实际调用都有独立显式frame；测试专用active-frame链支持同步重入恢复，单线程，不是运行时父调用权限或线程安全沙箱。

10个真实破坏自测：六寄存器分别破坏、DF置位、RSP改变、写栈哨兵、错误栈对齐（确认目标未调用）；两项转发自测验证目标入口RSP%16=8以及13个不同64位参数的正确传递/求和。工厂覆盖ABI0～3、尺寸0～159及支持元组的错误target/flags；并非枚举全部32位输入。每次运行57个报告槽（5工厂+52目标）均非零调用，详细零/非零返回数在证据JSON；非零不等于错误，例如life.start的量子1推进正常返回未完成。SDK1 host.bind仅空参拒绝、host.enter只涵盖错误版本；不能把这些当SDK1有效路径已验收。

首次debug探针在原别名负例实际退出125：新转发表被误用作原host只读表。修正为通过getter继续使用原工厂canonical host descriptor，不跳过断言或改拒绝状态；正常调用仍使用转发表。新/旧main均93个assert，原全部计数一致。后续debug/release与GAS/C提供者四次均通过。固定前两版目录和失败/成功探针回执保留。未改门禁、工具链、预算或超时。

12项登记检查（1正/11拒绝）通过；48组保留产物绑定/符号/ELF复核通过；完整314文件归档 `6090efb3ef41d9a027e7089aa00c756f9d82027be986bbd3b2c006db2c3070bf` 恢复后字节/mode与preflight一致，未据此宣称归档后重建。源码已发布 `codex/t14-native-sdk2-abi-guard`，提交 `731b316d8054635374d96128648c66951f5f13c9`，Git树每份blob与固定候选一致、远端回读一致。源码分支为实验快照，不是生产放行。

最终本地元数据控制包 `260a4e3c142dfd47e0c4ca5543f981985003e4fa1e612a694fba0970b24f5878`、原固定controller、源摘要、逐槽覆盖和同一Agent自审在 `t14-abi-guard-evidence.json` 与目录内final-review.json中分别记录。SDK1有效路径和其他T14专项原生回归、正式sanitizer、完整G04轨迹/故障矩阵、G07资源/体积/性能仍未完成；本结果不关闭完整G03、GAP-010或T14。

## SDK1 有效路径与原生互操作回归（2026-10-21）

SDK1指盘古早期原生ABI1，不代表Pi/JS插件兼容。本次沿用原824场景，保留所有断言及既有能力差异。SDK1客户端通过config显式注入budget/expected回调，纯assert(opaque)观察函数移至既有测试断言支持，既保留检查又消除反向依赖；C++ interop提供者只通过测试setter注入ABI守卫。public SDK1/fixed interop头与生产宿主实现逐字节不变，canonical SDK1表/上下文未替换。`host.plugin-sdk1` v1只登记原来已有的ABI1工厂能力，未新增生产实现。

固定候选 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-native-sdk1-und1qxzx`，58模块/28组合/144原生文件；controller `c6bcc201a0954c630e85a826ec9a25faa3b8a02034038d580095e11c2474b3b5`、candidate `06fdeea90c951736594c538a960bbfdb2634509c8902b858eda9390f837f8214`、原冻结image `23d6086651c009e3863556828331b6d6badb6065a336c90c6cc18fc59b824616`。56构建、72次目标隔离执行通过。两内存提供者×普通/guard×debug/release共八次SDK1，各824场景，setup598/cleanup608/business588租约消费（合计1794，C/C++各897），另有168次嵌套业务调用；guard的service.evaluate槽实际1962次。普通四次完整stdout逐字节匹配原SDK1证据，guard前缀行同样匹配；SDK2所有原计数保持。

guard四次各122627调用，5工厂/32宿主槽及新增18目标均覆盖。SDK1 host.bind每次824个成功返回，host.enter成功/拒绝均有；7公开SDK方法都有成功/非零返回记录。两语言entry/load/dispose/use、C++invoke/evaluate与嵌套服务均进入守卫，10真实破坏自测与2转发自测沿用并通过。ABI1 setup/cleanup无父作用域时仍各144次拒绝；不按inflight/全局phase放宽来“消除”差异。仍未执行其他早期T14独立专项，不能因此宣布完整G03/G04。

执行脚本在72个目标完成后因漏带外部SDK2 slot-map.json报错。原executions.json状态RUNNING及退出1日志未改；补齐固定映射，以verify-executions.py复核56产物及全部72回执/ELF摘要、原最终断言和额外的SDK1普通stdout精确比较，独立execution-verification.json为COMPLETE。没有重跑或覆盖原记录，也未把原汇总脚本伪称成功。原main的纯观察assert只移动到支持模块，余下两个main各68处，原main共69处。

12项登记检查（1正/11拒绝）、339文件归档恢复/字节/mode/preflight和56保留产物复核通过，未宣称归档后重建。源码实验分支 `codex/t14-native-sdk1-interop`，提交 `a73eeba68ae867a039f4b8da60774cbf4fff7d3b`，所有Git blob与固定候选匹配，远端回读一致。源归档 `513d5b92099642ce26fe1f6a74cdfb912a0c2caff55d9f8dd5d3edc747fffbd7`。

最终元数据包 `aabd037e8a1963ed7ff122e147da94b9ab6e8979b88a2e24d61a2b25f9e89cd3` 与同一Agent自审在目录内final-review.json及t14-sdk1-native-evidence.json；它不替代原测试controller的固定pin。下一步为其他专项原生回归、正式sanitizer与自测、完整G04事件轨迹/故障矩阵及G07测量；T14不勾选、生产与受保护远端放行继续BLOCKED。


## 当前登记矩阵的 UBSan trap 验证（2026-10-21）

外部目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-native-ubsan-parallel-rjctw9_3`；登记与证据见 [t14-ubsan-native-candidate.json](t14-ubsan-native-candidate.json) 和 [t14-ubsan-native-evidence.json](t14-ubsan-native-evidence.json)。60模块/30测试组合、148份原生文件；原144份逐字节不变，只增加C/C++故障自测及其入口四文件。新增v3测试配置使用 `-fsanitize=undefined -fsanitize-trap=all -fno-sanitize-recover=all`，拒绝非test组合；v1/v2配置不改。新镜像载荷沿用固定工具链，绑定v3 profile，SHA256 `4b6b9758fe337a4c57a4bc904bba23c2903855deb7aa50fa9b4271d2f4f41b41`。

60份debug/release冻结构建、每份保留产物复核通过；430份C/C++翻译单元审计（400 C、30 C++）与源码摘要匹配。审计完整预处理后的Clang AST，拒绝 `NoSanitizeAttr` / `DisableSanitizerInstrumentationAttr`，包括宏展开；字符串内同名文本不误判。进程内缓存仅复用固定编译器、完整选项及实际预处理内容相同的AST审计，每次仍重新预处理；头文件变化及 `__has_include` 存在性变化都会重新审计。缓存不跨进程持久化，不依据源码本身摘要跳过检查。

串行完整任务三次在原180秒限制内分别留下24/42/51份回执后超时，均不计通过。剖析后对v3测试组合使用固定两路并行、私有暂存与产物目录、登记顺序汇总；每组仍完整编译/审计/即时复核，任一线程异常不写矩阵回执，v1/v2保持串行。实际60份矩阵随后在原180秒限制内完成，单工具30秒限制不变。新增自测验证并发上限、全部六份构建与复核、稳定顺序和失败关闭。

98次目标执行及执行后全产物/源码/控制摘要核对通过：原72次基础/SDK运行计数保持，四次健康自探针退出0，22次真实UB探针触发SIGILL。两语言debug/release均覆盖有符号溢出、移位、越界、普通指针误对齐、空指针访问；C++另有错误间接函数类型。原操作器把PID命名空间启动器的退出132当成直接子进程-4而失败，原74条记录及FAILED状态保留。后续只复用73条实际成功记录，重测故障并完成其余自探针；只读Python监视器使用内核waitpid状态确认信号。四项监视器自测区分真实SIGILL、主动exit132、正常exit0及SIGSEGV，不能把数值132作为检测成功。最终核对全部98条预期组合、22份真实信号状态及每个ELF摘要。

SDK1八次各824场景/1794租约消费，guard四次各122627调用；SDK2八次各1196场景/3242方法调用，guard四次各146729调用。所有原输出和守卫槽集合保持；ABI1 setup/cleanup无父授权仍各144次拒绝，不用全局phase或inflight放宽规则。

隔离自测入口补齐原先遗漏的C++/UBSan套件，并拒绝已登记测试未调度或重复调度。首次整套admission在原30秒限制内超时，保留日志；将新增四项UBSan准入测试按职责移入单独登记套件，未删改用例/断言或扩大限制。最终冻结镜像内全部215项通过，admission / native UBSan / UBSan admission分别23.517 / 18.153 / 12.906秒。该自测控制树 `0ca8f813953efa0799d8d85ee62662b8baf23753c4f6bb0feedbdea7befa6cae` 与原native矩阵控制树 `97f47f7bf91f3d85c5a3271c97009f966a971810d8a8e6f6bc91ec435693705d` 分开固定；后续差异仅套件调度/测试迁移及其登记，native构建、准入、源码审计与profile字节相同，不伪称两棵全树相同。

源码归档SHA256 `ca8ac9e5da305eabcbd7463f3d782f99be24d90b3dd97167ab28840d9dd92955`，349份登记文件恢复字节/清单匹配、60份保留产物复核通过；不是恢复后重新构建。固定原native候选源码已推送实验分支 `codex/t14-native-ubsan-trap`，commit `83e18f3434caec96b361c3df86724e7c63e40868`，Git清单/逐blob和远端ref回读匹配；此快照先于自测调度后续修复，根分支另行交付后续控制面，不将新控制树的证据冒充原树证据。

同一Agent实施、自审并批准本地test用途；固定控制树、差异、日志摘要、风险与最终元数据审查在目录内 control-review.json / selftest-control-v2-review.json / final-review.json。UBSan undefined组不是完整内存安全证明，汇编未插桩；不声称覆盖volatile目标访问、C++vptr、ASan/泄漏/数据竞争或任意第三方插件。早期T14独立专项及对应ABI/sanitizer、完整G04轨迹/故障矩阵、G07资源/体积/性能仍待完成，完整G03、GAP-010、T14与生产/远端受保护放行仍未通过。


## 公开 ABI 专项原生迁移（2026-10-21）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-focused-native-v4-79sc9bn4`，两份登记 [normal](t14-focused-normal-candidate.json) / [UBSan](t14-focused-ubsan-candidate.json)，结果见 [专项证据](t14-focused-native-evidence.json)，剩余入口见 [固定盘点](t14-focused-inventory.json)。SDK入口之前只包含entry/route夹具而未执行专项main，本轮单独执行facade、entry、route、interop和parent，不以被编译函数代替运行覆盖。

23模块/11组合、83份原生文件；47份复用文件与已验证候选逐字节相同，36份新增文件全为测试适配。五套专项分别用GAS/C两种compiler.memory、debug/release、普通v2/UBSan v3组合，每套实际8次；另有四次格式器边界自测。两模式同一源码树 `a9a5ac9fe6791582863c7aaa17ea45d4ad0a585529371e010144ddc6e376ba38`、控制树 `0c06561628462d8e9ee661b3186c9220f1edd8347526a14d55c8f2fafc0e6575`，合计44冻结构建/44隔离执行。150份UBSan C/C++源码审计覆盖全部相应对象；所有运行退出0且无stderr。每份专项stdout逐字节等于原宿主O0/O2共识，不从新实现输出生成期望。

每次facade为1152场景/1924提供者调用/5772登记拒绝；entry为4108场景/6688依赖检查/90346不足预算/6555别名拒绝及24汇编入口；route为390场景/888调用、两实现各444调用；interop为1188场景/34128调用，九矩阵单元各3792，C++观察计数11376；parent为809场景/2248调用，setup/cleanup/业务嵌套分别214/208/156，深度3。五份直接测试文件的284个原始assert表达式逐条文本匹配，实际差异与摘要在 assertion-retention.json 和逐套test-port.diff；这不是全生命周期语义的形式化证明。

新测试支持库只承担固定%d/%s/puts输出、memcmp与断言；边界自测验证INT_MIN/MAX/零、空字符串、换行与返回长度，不进入产品。C++测试提供者的观察函数由测试main显式设置，避免插件反向导入client形成依赖环；没有把原11376观察计数改成空回调。此setter是进程内可信测试桩，不作为生产服务或线程安全机制。

旧私有链接夹具的九个符号在普通/UBSan、两内存提供者、debug/release中共72次被真实ld拒绝；八次同环境公开pangu_lifecycle_query链接成功。所有被检lifecycle对象的全局定义仅此一个工厂；链接出的额外测试程序未执行，不能计为生命周期运行测试。输入/对象/工具链只读挂载且断网，检查源与结果摘要保留在 private-check-*/ 下，仍不宣称服务器侧防绕过。

失败历史完整保留：第一次被Clang拒绝旧interop_async只写不读；随后发现parent_budgets同类死计数，移除它们而保留真正异步控制、根/子预算计数和所有断言。第二次封闭链接审计拒绝外部函数地址引入的_GLOBAL_OFFSET_TABLE_，按既有SDK1惯例明确静态测试导入hidden，不放宽链接器。第三次普通22构建/执行通过，UBSan预处理AST因for闭块后贴着assert的排版产生误导缩进诊断；只调整换行后，最终同一v4源码重新完成两模式。各失败/部分回执未改为PASS；外部生成器bindings键顺序差异也已记录并改为排序JSON，未覆写运行中的控制目录。

289份文件源码归档SHA256 `eba1345295bee5e0dd7d37ae40d875af64bfd0b71f9066a428355c0d1d95b384`，恢复字节/登记匹配；使用恢复出的源码再次核对两模式全部44份保留构建，未执行恢复后的重新构建。实验源码分支 `codex/t14-native-focused-public-abi` commit `42bd58cfe1fa1f9243f79d8f737c4345b8b4322d` 已逐blob核对并远端ref回读匹配。

门禁、测试和instructions字节与前次215项隔离自测的固定控制器一致，本轮没有冒称又执行了215项；本轮新矩阵、运行/负链接及G00等收尾检查单独记录。同一Agent实施、自审批准本地测试登记与证据，固定摘要/差异/风险在 control-review-*.json / final-review.json。参考Cordis等待dependent再清理，以及Harness vendor旧epoch/完成期间等待的代码，本轮是原生行为验证，未宣称JS框架兼容。

盘点覆盖最新宿主快照18份C测试入口及private-link夹具，共19项：5项本轮运行通过、2项SDK已有固定原生证据、1项仅链接边界通过、11项当前ABI未运行。还须审计更早独立owner语料。旧私有helper部分已从公开面删除，后续应通过现有公开ABI重表达约束并记录差异，不能重新导出私有函数以迁就测试。对应专项ABI守卫、完整G04事件轨迹/故障注入、G07资源/体积/性能和生产准入仍待完成，T14/GAP-010保持未关闭。


## 七组协调器公开 ABI 轨迹（2026-10-22）

旧 `test.c` 的七组同步协调器约束经现有公开ABI重表达：正常逆序/重复停止、部分setup失败与cleanup错误继续、setup重入取消、在途等待、失败setup带在途操作、重复owner拒绝、空列表。规范在首次原生捕获前固定，见 [契约](t14-coordinator-trace-contract.json)；验证器 [coordinator_trace.py](../../tools/gates/coordinator_trace.py) 按固定动作语法及单独状态模型核对顺序、phase、started/launched、owner状态、在途引用、资源行计数、错误与报告。结果见 [证据](t14-coordinator-trace-evidence.json)。

两组持有场景改用真实 `setup_call` / `call_finish` 票据，在显式链边上同时保留consumer/provider，不恢复私有owner_enter/leave。此处验证调用准入与生命周期引用，不调用提供者业务方法。重复owner先在bind_geometry拒绝，未绑定start也拒绝，均无回调；这是明确记录的较早拒绝阶段。映射见目录内 migration-map.json，保留七组语义约束，不声称旧assert文本逐字节不变。

最终目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-coordinator-trace-qbufh1wh/final-bounded`；12模块/2组合、38原生文件，37份复用源码字节不变，只新增一个测试main。普通v2/UBSan v3、GAS/C两种内存提供者、debug/release合计8冻结构建与8目标执行；每次七组/137事件，56组次/1096事件，八份完整轨迹逐字节相同。14份UBSan源码审计覆盖全部相应对象，既有native和单工具180/30秒限制不变。预览阶段也通过8次，但最终结果不把预览重复运行累加为独立新增场景。

先固定预览检查器及契约摘要再运行；正式检查器仅加强最大1MiB读取、畸形JSON异常处理及契约标量类型，动作语法/状态转换不变，随后在包含正式检查器的最终控制树重新完成8份原生构建/执行。同一控制树 `23e5fcc013654fc0c35481d1447890fc8134c099c845af49f826164441cabd4c` 的冻结镜像全部223项门禁自测通过，包括新增8项轨迹自测。反例覆盖漏/多/乱序事件、提供者提前释放、在途引用丢失、吞加载/清理错误、停止后登记成功、资源行残留、停止后启动成功、错误等待报告、工作量超限、字段/类型/数组/JSON畸形、重复JSON键、契约删改/布尔浮点冒充整数、错误契约摘要以及读取上限。合法样本来自观察，仅作为反例测试输入，期望由先固定的语义契约/状态模型决定，不从样本生成期望。

最终候选树 `22c38306b68898b195c709e26e0c5910d3ec93095a3ca8eefdd1d9bca2d12209`；253份文件归档SHA256 `d62ec75b0c1e0f4cfb38ac177c6e5562b14d2257d3c359abdbb06c1251ee9075`，恢复字节/登记匹配，并使用恢复出的源码复核两模式全部8份保留构建；不是恢复后重新构建。实验源码分支 `codex/t14-coordinator-trace` commit `32dafac4001e5f8410abfdcc414c2c8c11f17064` 已核对清单/逐blob并远端回读。最终本地控制/元数据由同一Agent实施、自审，固定摘要、差异、证据和风险在 final-review.json；不伪称独立审核或服务器侧防绕过。

盘点中的旧协调器、F01适配器测试与F16 setup测试现均有带API适配的原生证据；剩余8项当前ABI端口待完成，更早owner独立语料仍待审计。本守卫只保证上述固定七组捕获的约束；异步 disposer 完整矩阵、分支、量子推进/故障矩阵、完整G03/G04/G07与T14尚未完成。逻辑work上限不是回调墙钟/峰值内存证明，资源行已调用清理也不是外部副作用必然物理释放。不得据本结果宣布完整G04、插件沙箱或生产放行。

观测器收尾另补转换前计数断言：拒绝runtime count超过4、started/保存的启动前缀不合法、owner在途超过3或资源行超过2，避免64位高位被截断为合法int。沿用相同223项已验证控制树，全部8份原生构建/执行重新完成，轨迹字节仍相同；原预览与首版成功记录保留，最终计数不累加重跑。


### 2026-10-24：异步 setup 核心公开 ABI 原生专项

针对旧 `setup-test.c` 的 1,414 个异步 setup 场景，新增 test-scope 公开 ABI 候选；七种模式各202例，包含 defer/finish/pump、失败回滚、停止时等待、异步 disposer、inline finish、返回协议冲突、重入停止和迟到 setup。普通与 UBSan、汇编/C 内存提供者、debug/release 共8次冻结构建与隔离执行全部通过，摘要逐字节一致。源码摘要、矩阵、逐次 ELF/输出摘要及失败尝试见 [专项证据](t14-async-setup-core-evidence.json)；测试源与双配置已发布到 `codex/t14-async-setup-core`（`f7a35a2bbf853c41018a615b31bbd6c830bbcff7`），见 [源码发布记录](t14-async-setup-core-source-publication.json)。

初版候选只覆盖 `setup-test.c` 的异步子集；后续合并候选已补入其675个 `integration_main()` 场景，见下方更新。初版 evidence 文件保留为历史记录。


### 2026-10-24：适配器集成与异步 setup 合并复验

将旧 `adapter-test.c` 的 `integration_main()` 与 `setup-test.c` 的 1,414 个异步 setup 用例合并到同一当前公开 ABI 候选。普通/UBSan × 两个内存提供者 × debug/release 共8次构建和隔离执行全部通过；每次输出均为675个 adapter 场景、1414个 async 场景，七个 async 模式各202例。86个真实依赖边调用等待通过公开 call ticket 覆盖。

旧 helper 还能在没有服务依赖边时单独 enter owner；当前插件公开 API 没有该操作。106个这类用例执行了正常 stop，但没有伪造 in-flight lease；这是明确记录的 API 差异，也没有重新导出旧私有 helper。F01/F16 标记为 `PASS_CURRENT_NATIVE_WITH_API_ADAPTATIONS`，证据见 [合并测试结果](t14-adapter-setup-native-evidence.json)，测试源码与登记见 [发布记录](t14-adapter-setup-source-publication.json)。完整T14、G04/G07、生产准入仍未通过；隔离控制器external trust approval仍待定。


### 2026-10-24：旧 async-test.c 当前公开 ABI 原生专项

将旧 `async-test.c` 的异步清理行为映射到现有公开 `pangu_lifecycle_api`，未重新导出 `pangu_owner_cleanup_finish`、`pangu_owner_enter/leave` 或 runtime 私有helper。新增7组补充轨迹：两个异步disposer的延迟完成/重复停止/票据拒绝与错误继续；待完成期间拒绝 owner 重初始化；内联成功、同步失败和完成/返回值冲突；旧 epoch 票据在重用后拒绝；公开依赖调用排空后才触发异步清理；以及真实多 provider 图中 consumer 等待清理时保留 providers。

普通/UBSan × 汇编/C内存提供者 × debug/release 共8次冻结构建和隔离执行全部通过，输出摘要一致；同一候选还复验675个 adapter 与1414个 async setup 场景。8次运行均为7个清理组、8次坏/重复/迟到票据拒绝、2次内联finish。旧测试依赖的无依赖边任意owner操作没有公开ABI等价物，本次不伪造覆盖；完整 T14、G04/G07、生产准入及隔离控制器 external trust approval 仍未通过。固定树、构建/执行回执、风险和同一 Agent 自审记录见 [F02 原生证据](t14-async-disposer-native-evidence.json)。


### 2026-10-24：旧 budget-test.c 当前公开 ABI 原生专项

将私有 `runtime_stop_budget` / `pangu_owner_stop_budget` 断言改为公开 `start`、`stop` 与生命周期报告。普通/UBSan × 汇编/C内存提供者 × debug/release 共8次冻结构建和隔离执行全部通过，输出摘要一致；每次跑2,736个预算场景、44,904个有界 stop 推进，涵盖固定240图的有效组合、四档 quantum（1–4）、同步清理错误码、异步 disposer、异步 setup、prefix teardown，以及零预算状态快照和终态零工作量重停。单 owner 释放经公开 runtime 路径在9个工作单位内完成4个disposer；所有 stop 回调增量均不超过本次报告work。

旧测试在任意位置直接 enter owner；公开 API 只允许依赖边上的 call ticket。本次对无可用依赖边或prefix上下文的436个场景跳过这项持有断言，不记作通过；有边场景用公开call/call_finish保留consumer/provider并等待。源码与回执固定在 [F03原生证据](t14-budget-native-evidence.json)。随后对固定请求及镜像清单复核时发现该证据中的 UBSan image SHA256 曾误抄为62位；已更正为请求与冻结镜像清单一致的64位摘要，构建和执行回执未变化。这只关闭F03当前公开ABI迁移证据，不代表完整T14/G04/G07，也不解除隔离控制器 external trust approval 与生产准入阻塞。


### 2026-10-24：旧 call-budget-test.c 当前公开 ABI 原生专项

将旧预算调用测试的三个私有入口分别映射到公开 `call`、`setup_call`、`cleanup_call` 和 `call_finish`。普通/UBSan × 汇编/C内存提供者 × debug/release 共8次冻结构建和隔离执行全部通过且输出摘要一致；每次覆盖207个有真实解析依赖边的调用场景，1,191次逐fuel不足重试，30次配额拒绝、30次无效binding拒绝、345次输出或scope重叠拒绝，并在202个setup及202个cleanup上下文里验证scope阶段约束。

预算不足时逐字节核对runtime、owner、资源、operation、span和edge存储及输出票据；配额测试通过三个公开依赖票据同时占满两端operation槽位，不能模拟旧私有接口分别耗尽单个owner槽位的情况，该差异明确保留。F03的零预算测试在合并候选中也补充了资源和operation数组快照。F04源码、冻结输入、运行摘要和同一Agent自审记录见 [F04原生证据](t14-call-budget-native-evidence.json)。F03的UBSan镜像摘要转录更正也记录在该证据及[F03证据](t14-budget-native-evidence.json)。这只关闭F04当前公开ABI迁移证据，不代表完整T14/G04/G07，也不解除外部信任审批和生产准入阻塞。


### 2026-10-24：旧 call-test.c 当前公开 ABI 原生专项

将旧 call/setup/cleanup 生命周期测试迁移到公开 `call`、`setup_call`、`cleanup_call`、`call_finish` 与 `disposer_finish`。普通/UBSan × 汇编/C内存提供者 × debug/release 共8次冻结构建和隔离执行全部通过且输出摘要一致；每次执行207个生命周期场景：69个实际binding分别跑直接调用、setup callback调用和cleanup callback调用。合并轨迹累计819次setup_call、819次cleanup_call、1,776次provider调用及7,212次预期拒绝；69次异步清理会持有调用票据直到清理完成，再排空调用后释放provider。

轨迹还验证停止开始后拒绝新业务调用、setup scope过期、cleanup callback外scope失效、伪造和重复finish拒绝以及consumer清理期间provider保持活动。旧 `quotas()` 通过私有owner helper只耗尽provider槽位的场景不能由公开call单端复刻；公开ticket始终同时保活两端，F04/F05均把这一差异明示而不声称兼容。固定候选、矩阵回执和同一Agent自审记录见 [F05原生证据](t14-call-lifecycle-native-evidence.json)。这只关闭F05当前公开ABI迁移证据，不代表完整T14/G04/G07，也不解除外部信任审批与生产准入阻塞。


### 2026-10-24：旧 geometry-test.c 当前公开 ABI 原生专项

将旧 geometry-test.c 中 runtime 外部指针检查、protected-span 守卫与几何绑定构建测试迁移到公开生命周期表。普通/UBSan × 汇编/C内存提供者 × debug/release 共8次冻结构建和隔离执行全部通过且摘要一致；每次覆盖98个有效计划，600轮逐span拒绝矩阵，累计143,961次预期几何拒绝。矩阵覆盖生命周期报告、setup/cleanup票据、call票据、finish票据和错位输出指针；builder覆盖runtime/resource别名、错位span、重叠owner资源、错位owner、容量溢出、fuel不足及重复绑定，并检查拒绝时protected内存及runtime状态。

明确保留两项API差异：standalone owner enter/stop私有函数没有公开表项，不能假称已测试；fuel不足的 `bind_geometry` 会保持runtime未提交，但调用者span scratch可含部分中间数据，旧测试没有要求其回滚，本专项也不扩大为事务性保证。固定候选和同一Agent自审证据见 [F08原生证据](t14-geometry-native-evidence.json)。F08只代表当前公开ABI专项通过，不代表完整T14/G04/G07，也不解除外部信任审批与生产准入阻塞。

## F09 图停止公开 ABI 迁移（2026-10-24）

普通/UBSan × 汇编/C内存提供者 × debug/release 共8次冻结构建与隔离执行通过，输出逐字节一致。每次98个图的边构建与fuel拒绝检查、516个公开停止场景，含326个独立分支、224个祖先保活、83个setup等待时独立清理、44个等待期间错误传播及112个真实依赖call保活场景。

使用现有公开prepare/start/stop、setup_defer/setup_finish、异步disposer与dependency call票据。292个旧任意owner hold缺少公开依赖边，明确跳过，不计作通过。测试不恢复私有helper；候选和同一Agent自审见 [F09原生证据](t14-graph-native-evidence.json)。F17/F19、更早owner语料、完整T14/G04/G07仍待完成；生产准入和外部信任审批保持阻塞。

## F17 启动预算公开 ABI 迁移（2026-10-24）

普通/UBSan × 汇编/C内存提供者 × debug/release共8次冻结构建与隔离运行通过，输出逐字节一致。每次6056个启动场景、103692次有界推进、285次setup前取消、114次回调前拒绝；量子1–4，推进数与历史测试一致。同步失败、异步setup完成/取消、两种槽退休、重入停止、异步清理错误、graph/prefix模式及零预算runtime快照均保留。回调执行时间不受fuel度量保证。

旧私有prepare拆分接口合并为公开prepare事务，其余驱动/完成/资源注册通过当前公开生命周期表；未新增私有导出或修改生产实现。固定源码、失败迭代及同一Agent自审见 [F17原生证据](t14-start-native-evidence.json)。F19、更早owner审计及完整T14/G04/G07仍未完成；生产准入、外部信任和发布保持阻塞。

## F19 准备事务公开 ABI 迁移（2026-10-24）

普通/UBSan × 汇编/C内存提供者 × debug/release共8次冻结构建和隔离运行通过且输出一致。每次98次成功准备/启动/停止、145个失败计划、6432次别名拒绝和12127次不足fuel拒绝，与历史计数一致。快照覆盖plan、registry、graph输入/输出、storage/source items及有效owner/资源/operation池；拒绝后输入不变，激活项/边/结果仍为毒值，成功后输出尾部不被覆盖。保留容量溢出、结果错位和输入/输出交叉重叠断言。

使用公开prepare、bind_geometry、start/stop及登记方法，无生产源码、公开ABI、预算或门禁变化；[F19原生证据](t14-transaction-native-evidence.json)记录固定输入和同一Agent自审。最新快照19项入口均已建立各自范围内证据，其中F12仅链接边界、SDK1/SDK2为既有原生矩阵，不能将pending_current_abi_ports=0解释为完整T14完成。更早standalone owner语料审计、其余ABI/故障轨迹及完整G04/G07仍待完成。

## 最早 standalone owner 语料补充（2026-10-24）

逐组读最早owner实验probe.py六组断言，新增4组公开runtime测试：四资源LIFO与满额/重入停止/登记拒绝、加载错误-7与清理错误-9分别保留、setup取消后的迟到完成与重复票据、跨epoch旧票据及同epoch重用/资源别名/非零初始帧拒绝。start返回加载错误，后续stop返回清理错误，与旧语义一致。普通/UBSan × 双内存提供者 × debug/release共8构建运行通过且输出一致。

旧任意owner enter/leave已移除，不能声称逐条等价；实际依赖调用的槽位/代际/排空由F04/F05覆盖，但双端保活不等价于旧单端任意hold。六组映射与明确差异见 [最早owner证据](t14-earliest-owner-native-evidence.json)。[发现清单](t14-earlier-owner-discovery.json)按内容摘要整理含owner.c实验目录中的30份不同测试，属于发现结果，尚未逐份证明覆盖；审计仅部分推进。完整T14/G04/G07与生产准入仍未完成。

## 早期30份内容的来源审计（2026-10-24）

固定发现清单内所有路径均重新计算摘要，30份不同内容逐份分类并保存完整差异。18份与focused盘点逐字节一致；其余11份是几何前置接入/矩阵扩展、异步取消登记顺序修正、测试入口命名及SDK2观察回调注入版本；最早probe.py另有六组部分映射证据。详见 [逐份审计](t14-earlier-owner-content-audit.json)。

这是静态来源与断言审计，没有新执行旧变体。特别保留异步取消登记顺序的有意变化、SDK2组合证据边界及旧私有owner API缺口。发现过滤范围尚未证明穷尽，故不关闭更早语料审计或T14；下一步先补全发现边界，再处理ABI/G04/G07剩余项。

## 早期44份运行器/生成器审计（2026-10-24）

44份Python内容按AST记录全部assert节点并复核所有副本摘要。唯一超过200字符的嵌入源码正是已发现的早期adapter-test.c（摘要f56c8c1d78ae…）；其余断言涉及编译/运行退出码、固定头摘要、导出/未解析符号及SDK2回执。未运行会重写源码的历史生成器，也不把静态断言清单视为当前ABI验证。

在外部副本重跑SDK2本地8项失败关闭检查：有效、缺回执、额外源码、二进制漂移、缺O0、缺场景、残留PASS加畸形回执、空借用输入，退出码与回读状态全部符合预期；副本源码与原目录一致。[运行器审计](t14-earlier-runner-audit.json)固定源码、断言和回执摘要。这仅证明本地回执检查，不能代替原生重构建或远端受信执行。更广发现边界、私有API缺口、完整T14/G04/G07仍待完成。

## G04异步setup/cleanup事件补充（2026-10-24）

新增3组真实公开ABI轨迹：异步setup成功再停止、setup等待时取消后迟到成功、setup失败与异步cleanup错误同时保留。50个有序事件记录phase、owner状态、started、停止标记、在途引用、资源、cleanup等待与回调次数；包含伪造/重复票据拒绝、等待时不重复回调及终态排空。普通/UBSan × 双内存实现 × debug/release共8次构建和运行一致，注册参考模型逐事件验证400事件次。

局部检查器tools/migration/g04_async_trace.py与执行时外部检查器字节一致，5组自测包括700个逐字段变异、事件删除/交换、重复键与非法输入拒绝。它是局部实验检查器，尚未接入必需G04门禁；三组单owner轨迹不能证明分支/多owner/全预算/全故障矩阵完成。pump在停止清理完成后返回6（失败场景保留加载错误），随后stop返回cleanup错误；固定证据见 [异步轨迹证据](t14-g04-async-trace-evidence.json)。完整T14/G04/G07与生产准入仍未完成。

## G04固定分支图与小预算事件补充（2026-10-24）

四owner、两条独立依赖边的固定公开生命周期图，量子1–4各两种清理状态（成功/独立消费者清理-33），共8场景。公开call票据同时保留consumer1/provider0，验证分支3→2在等待期间独立退出，持有分支1→0在finish前不释放；随后重复finish拒绝并有界排空。零fuel核对runtime/owner/resource/operation/disposed存储不变。每次228事件、148次有界推进，普通/UBSan × 双内存实现 × debug/release共8构建运行一致，1824个分支事件实例逐项验证，并复验此前50事件异步轨迹及全部计数套件。

局部参考检查器验证预算、事件阶段、引用保留、消费者先于提供者、独立分支完成、错误来源和终态排空，8组变异自测通过。图直接来自固定生命周期测试输入，不宣称此次已覆盖planner解析到运行时的完整组合，也不覆盖所有异步/分支交错。新增检查器尚未接入必需G04，完整G04/G07/T14和生产准入仍未完成；见 [分支轨迹证据](t14-g04-branch-trace-evidence.json)。

## G04分支检查器失败关闭加固（2026-10-24）

反例复验发现旧局部检查器会接受end仍携pending/reason、finish等非推进事件携work，以及未核对end owner等不一致记录。已要求非推进事件报告三元组全零，非cleanup事件owner=-1；independent-drained前必须已有tick-held返回等待，end前必须已有终态tick-drain且owner/runtime快照相同。检查器收紧，原生实现未改。

12组检查器测试及13项G00自测通过；保存的普通/UBSan八份原生输出用新检查器重放，1824事件通过。这是已有输出重新核验，不是新构建或新原生执行。原分支证据保留当时检查器摘要，当前补充证据明确旧反例及收紧后的范围，见 [检查器加固](t14-g04-branch-checker-hardening.json)。尚未将局部检查器接入必需G04，完整T14/G04/G07仍未完成。
