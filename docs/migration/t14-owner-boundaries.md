# T14 owner 生命周期启动草稿

当前最新宿主候选在版本化host-control函数表之上接入了静态 entry 到 owner-aware setup 的桥，见末节；前文小节保留各历史候选的实际范围。T14 整体与原生准入仍未完成。

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
