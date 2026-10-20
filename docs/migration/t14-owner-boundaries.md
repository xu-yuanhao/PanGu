# T14 owner 生命周期启动草稿

2026-10-21，外部宿主实验，未批准原生ABI/模块或T14阶段输入。使用已固定T13规划接口作为设计背景，不宣称完整T13已验收。参考Cordis fiber卸载等待dependent的语义，以及Harness vendored硬化：setup前登记所有权、重入停止等待setup/清理、停止后禁止新effect。

## 当前草稿

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t14-owner-wip-5stexo34`，owner.h/owner.c及evidence.json。调用者独占owner、资源栈和操作槽，无堆分配/产品全局状态，单线程操作。首次owner必须完全零初始化；通过API重用仅限Disposed、无资源/在途且epoch递增。

状态Loading→Active→Quiescing→Disposed。Loading/Active可登记同步disposer，停止先关闭新调用/注册入口；在途操作未退出返回BUSY并保留资源。资源逐项LIFO、调用前从待清理栈移出，busy标记保证重入stop不会重复释放；记录首个清理错误但继续全部disposer。重复stop不再执行清理并返回已有错误。Disposed只证明受管清理已调用完成，不保证失败回调的外部资源物理释放。

setup用显式操作票据在执行前登记；重入stop等待该票据退出，迟到加载成功被取消，不会恢复Active。完成票据检查owner地址、epoch及槽generation；复制票据重复完成、旧槽完成、新epoch的旧加载完成均拒绝。代际饱和槽退休，不回绕。finish_load明确传捕获的epoch，避免旧完成误改新加载；取消后完成不再改终态。

操作退出不会在回调栈内自动执行清理，调用者需再次stop或由未来调度器驱动；异步disposer、跨线程和计划级逆拓扑停止未实现。必须让owner存储活到票据生命周期结束，不能手动清零或释放后复用旧票据；此处不是指针来源认证，未来运行时协调器还须约束owner分配/epoch和调用入口。

## 实际宿主验证

6组场景验证LIFO、清理错误继续、重入stop BUSY/注册拒绝、setup取消与迟到成功、重复/过期/代际票据、加载失败与清理错误分别保留、在途等待/槽耗尽/代际饱和、基础别名与非零初始帧拒绝。O0/O2无libc依赖链接通过。这些是宿主场景，不是Cordis运行兼容或原生门禁通过。

## 必须继续完成

从T13计划建立实例owner，Pending不激活，提供者先启动/消费者先停止；每个setup调用前登记操作并在所有退出路径释放；部分失败按实际已启动序列回滚；setup重入停止、异步完成/取消、异步清理必须保持owner可见至静默；清理异常不得饿死其他owner。最后登记结构/ABI、加入故障注入轨迹与原生冻结构建/guard/sanitizer/恢复证据，再判断G04/T14范围，不能据当前草稿勾选完成。
