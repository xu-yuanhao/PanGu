# T11 有界 arena 首个候选

日期：2026-10-20。T11 IN_PROGRESS，固定 arena 登记已获用户批准，仅限本地实验；仅 scoped region allocator 子项完成技术验证。

[源码与证据](t11-arena-evidence.json) · [候选登记](t11-arena-candidate.json)。原始目录：`/home/xu-yuanhao/Documents/PanGu-artifacts/t11-arena-gt_0ytu8`，最终控制器 controller-v2、源码 candidate-v3、构建 build-v3。source.tar 保留可恢复源文件；源码、命令、回执均绑定摘要。

## 实现

通过注入 platform.memory 初始化固定容量 arena，公开逻辑容量与实际映射长度分别记录；分配按真实地址对齐，拒绝零长度、非二次幂对齐、超过 4096 的对齐及超额申请，不增长、不调用通用 malloc。失败输出为空且位置不推进。

56 字节 arena 状态由调用方持有，40 字节 region allocator 表返回 borrowed context、allocate 与 epoch。只支持整个 scope 的 reset/dispose，没有单块 free；分配块借用 arena 的映射。这样可供后续 Buffer 消费接口，而不绑定 Linux 或引入通用堆算法。

reset 将位置归零并递增 epoch，原数据不清零，所有旧借用失效。dispose 通过原内存提供者释放；失败保留所有权与活动状态，不能 re-init 丢弃旧范围；成功后 epoch 查询为零。重新初始化保留单调代数，不回到 1；代数达到最大值时拒绝重置/重新初始化，不回绕。

借用状态存储必须持续有效。epoch 只提供检查依据，不自动阻止原生悬空指针；后续 view 必须比较创建时捕获的 epoch。原始指针、伪造字段、并发和回调重入没有安全保证。

## 验证

五组合 × debug/release，共十项在冻结镜像/只读输入/断网环境构建、复验并运行，全部退出 0。旧 T10 的退出、内存与文件/时钟 ELF 两模式均字节不变。

arena-probe 使用真实 mmap；arena-alternate 选择独立登记的 test-memory 静态页提供者，allocator 源码与 C 消费者保持相同。静态提供者仅测试，保留明确状态并校验一次映射/一次对应释放，不进入产品组合。

覆盖逻辑容量、实际映射长度、对齐、溢出输入、容量不足不推进、重置/释放/重新初始化、epoch 不回绕。每轮还执行 32×16 次不同大小/对齐分配，检查范围、非重叠和先前写入哨兵；失败保持 used 不变。该检查验证性质，不通过复制分配算法计算预期偏移。

另外对真实 mmap 后端的 map/unmap 分别注入 EPERM，两模式四次运行确认：初始化失败不发布任何持有状态；释放失败保留映射、已分配数据、原提供者及 epoch，重新初始化被拒绝。故意拒绝释放的夹具最后由进程退出回收，不当作正常释放完成。

新增替代组合时，第一版登记错误地把某个具体提供者设为必需模块，依赖闭包检查拒绝。已修正为“必需服务、允许两个实现、组合显式选择一个”，没有改门禁规则，也没有同时链接两个同名实现。

## 范围与下一步

Buffer、字符串、一般 realloc、guard page、借用 view 检查器、sanitizer、压力与性能预算尚未完成。T11 不整体划线，不宣称 G07 通过。接下来应在此接口上实现最小 Buffer，明确增长失败、容量上限与 borrowed view 生命周期；arena 的累积分配只能在 reset/dispose 时回收，不能隐藏旧块空间成本。

## 待审核

控制树 SHA256：`aac168cf8100f64084946c024354f0b02a7442690209e426355c85cbf1c04bce`。

相对已批准 T10 文件/时钟包，只改变既有候选 registry：增加 arena 契约、实现、测试消费者及独立测试内存提供者和两个测试组合。既有门禁、工具链、镜像和 T10 源码/ABI 不变。用户已明确审核批准该固定包，仅限本地实验，不覆盖将来的 Buffer 等新模块或生产发布。
