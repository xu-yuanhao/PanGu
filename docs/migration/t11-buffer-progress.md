# T11 最小 Buffer 与 checked view 候选

日期：2026-10-20。T11 IN_PROGRESS；arena 已由用户批准，本次固定 Buffer 服务登记已获用户审核批准，仅限本地实验。

[源码与证据](t11-buffer-evidence.json) · [候选登记](t11-buffer-candidate.json)。原始目录：`/home/xu-yuanhao/Documents/PanGu-artifacts/t11-buffer-e9oj1r42`；最终控制器 controller-v3、源码 candidate-v5、构建 build-v5。source.tar 可恢复完整源文件。

## 实现

Buffer 是有上限的二进制字节容器，不自带 NUL，也不验证 UTF-8。它通过注入的 region allocator 分配；region 契约已从 arena 头中提取到独立 region.h，Buffer 的实际编译依赖集不含 arena 或 memory-provider 头。arena 的 C ABI 类型名、字段和尺寸保持原义，组合代码显式包含其提供者接口。

这不是草案中的通用 alloc/realloc/free 表：同为 40 字节也不能混用，服务 ID 为 foundation.region，语义是整个 scope 拥有分配块。

支持初始化、追加、清空、销毁元数据和只读借用 view。追加支持自引用当前逻辑字节；容量几何增长并受 limit 限制，计算检查溢出。私有 Assembly 字节移动实现不引入 libc 或全局通用内存库。

增长后旧块仍由 arena 持有直到 reset/dispose，Buffer clear 保留容量，Buffer dispose 仅清理元数据；不把它说成独立 free/realloc。累计空间成本可从 arena.used 观察，最终受 arena 总容量约束。

追加失败不改变内容、长度、容量、revision 或已有 view 的有效性。测试通过实际 allocator 上的可控失败适配器触发，不依赖硬编码某次扩容消耗多少字节来凑 OOM。

## 生命周期

64 字节 Buffer 状态、40 字节 borrowed view 均有明确布局。view 捕获 owner、数据指针、长度、epoch 和 revision；验证时比较当前状态。成功修改使旧 view 失效，arena reset/dispose 使 Buffer 及旧 view 失效；失败不更新版本。

Buffer 元数据在 arena 失效后仍可安全清理，不尝试释放旧范围。revision 不回绕；达到最大值时拒绝继续修改或重新初始化。view 的 owner/allocator 状态存储必须持续有效，原始指针本身没有硬件防护，不宣称阻止任意 native UAF。

## 验证

七个组合 × debug/release 共十四项真实构建、保留产物复验和无凭据断网执行全部通过。Buffer 与 C 消费者在 mmap 和独立登记的静态内存测试提供者之间不改源码。

覆盖：空追加、嵌入 NUL、自追加及自追加扩容、非法自引用范围、容量上限、整数溢出、空指针、revision 耗尽、清空保留容量、幂等元数据销毁。失败适配器验证分配失败时数据/长度/容量/revision/arena 用量均不变，旧 view 仍有效。

额外验证 arena 先 reset 或 dispose：view 检查与后续追加返回失效，Buffer 可清理后重新初始化，不读取旧数据。验证在两个后端、两种模式执行。

早期测试源生成器误把文本转义写成实际 NUL，编译器按 Werror 拒绝；失败源与日志保留在 failures/ 与初次 build/，修复夹具而非关闭诊断。历史构建不覆盖成新的通过回执。

## 未完成

UTF-8/字符串接口、guard page、sanitizer、并发与完整 G07、长时间压力/性能基线尚未完成。没有把局部 Buffer 测试当作内存安全或 T11 整体验收。arena reset/Buffer clear 不清零内容，新的原始借用必须按有效长度使用。

后续应先补必要字符串/UTF-8 与边界检查，再做 T11 的资源与故障验收；不直接跳到 JSON/TUI。发布和远端可信检查链保持未完成。

## 审核记录

固定控制树 SHA256：`21e7f8c8827a52bc081ae5ffad38b0f0ac18d9b50bb4623eef5ad24f1679b7f9`。

相对已批准 arena 包仅修改既有候选 registry，新增通用 region/buffer 头、Buffer 实现、测试消费者及两种后端组合。实际 arena 头将通用 typedef 移到 region.h，布局断言和旧消费者继续通过；这项接口整理已在本候选源码摘要内，不冒充从未改过文件。门禁、编译/链接参数、镜像及 T10 ABI 不变。

用户已明确批准本次固定包用于本地实验；批准范围仅限该固定包，不包括字符串、完整 G07 或生产发布。
