# T11 最小 UTF-8 与保护页候选

日期：2026-10-20。T11 IN_PROGRESS；本固定包已获用户独立审核批准，仅用于本地实验。

[源码与证据](t11-text-evidence.json) · [候选登记](t11-text-candidate.json)。原始目录：`/home/xu-yuanhao/Documents/PanGu-artifacts/t11-text-kno9rwbx`；最终控制器 controller-v2、源码 candidate-v3、构建 build-v3、请求 request-v3.json。source.tar 包含全部 30 个原生源文件，摘要见证据。生产源码仍未迁入根项目。

## 接口与范围

新增 foundation.text v1 的 16 字节 borrowed UTF-8 slice；提供 next、validate 和校验后 Buffer append。无独立字符串分配器或重复 StringBuilder，持有内容继续使用已批准的有界 Buffer。文本实现只依赖接口与 Buffer，不依赖 arena 或 Linux。

next 使用字节游标，成功输出 Unicode 标量值；结束返回 3，非法参数返回 1，畸形或截断 UTF-8 返回 12。失败不推进游标或改写 scalar。validate 输出完整合法前缀的标量数及错误序列首字节位置；成功时错误位置等于字节长度。调用者提供的输出存储必须独立，源切片必须在调用期间可读且不被修改。

先校验整个输入再追加，非法后缀不会导致部分追加；合法输入的容量、OOM、自引用与生命周期检查继续由 Buffer 负责。raw slice 本身不跟踪所有者，不能把失效的 raw slice 传入解码器；Buffer checked view 的调用者仍须先验证 view，并确保 owner/allocator 存储有效。

保留嵌入 NUL、BOM 和合法非字符码点；拒绝 overlong、孤立 continuation、surrogate、超出 U+10FFFF 和截断序列。编码规则按 [RFC 3629 第 3～4 节](https://www.rfc-editor.org/rfc/rfc3629) 核对。此处严格拒绝的策略不代表 Pi 所有 JS 输入入口都采取相同错误处理；后续 wire/session 适配须另做兼容差异验证。

字节长度、标量数、JS UTF-16 code unit、字形簇数量和终端列宽是不同概念。当前没有 UTF-16 索引转换、流式 decoder、自动 replacement、规范化、搜索替换、字形切分、终端列宽或 SIMD 扩张。

## 参考与取舍

参考 Pi 原项目的 provider/流边界，以及 deepseek-harness 的 `packages/session/session-title/src/normalize.ts`：其标题截断先计算 UTF-8 字节预算，按 code point 累加而非截断任意字节；`packages/ptc-runtime/ptc-runtime-node/src/channel.ts` 的协议入口使用 fatal TextDecoder。保留这些边界意识，不引入 Node、JS 字符串存储或参考项目依赖。参考版本沿用项目冻结清单。

## 实际验证

十个组合 × debug/release，共二十项冻结镜像构建、保留产物复验与断网运行通过。此前七个组合继续通过，旧原生源文件与固定 Buffer 候选逐字节一致；新增 text 的真实 mmap、静态页替代提供者、guard page 提供者三个组合，消费者源码相同。

六次 text 运行各覆盖：

- 全部 1,112,064 个 Unicode 标量值，由独立算术编码器生成字节，校验解码结果；多字节编码的非空截断前缀必须拒绝。
- 2,408 条固定语料，期望由 Python 严格 UTF-8 codec 独立生成，包括空输入、全部单字节、边界组合、带合法前缀的随机后缀。Python 仅用于开发语料，不进入原生运行时。
- NUL、中文、emoji 混合输入、自引用追加、非法后缀追加不改变长度/容量/revision/arena 用量及既有 view；arena reset 后拒绝向失效 Buffer 追加。
- 十万次 clear/append 循环，在预热容量后 arena.used 与容量不再增长。这是局部资源不变量测试，不是吞吐性能或长期泄漏结论。
- 页尾全部单字节输入、四字节上界序列及截断、位于保护边界的空切片。

test-guard 是测试专用 platform.memory 提供者，仅接受至多一页的申请；映射三页，首尾 mprotect 为不可访问，仅交付中间页。Linux syscall 只在该 platform 模块，未向消费者开放。四次正对照（左右保护页 × 两种模式）主动越界读取均返回预期 139；正常 text 用例返回 0。正对照证明保护页实际生效，不宣称任意 native 指针均安全。

测试 ELF 的 release 大小约 46 KB，其中包含固定语料；不能把测试体积当作 Agent 大小或批准性能预算。所有预算仍为 UNBASELINED。

## 失败与复验

保留早期失败：首个草稿控制器被 Python 自动生成 __pycache__ 污染，库存门禁拒绝；重建并使用 -B。首次构建还发现测试局部聚合零初始化引入未准入 memset，改为显式字段赋值，没有补 libc shim、放宽链接规则或修改编译参数。首次预检发现候选仍持有旧 registry，随后同步固定控制器的登记。失败不计为通过证据。

最短复验：用 request-v3.json 中的固定路径调用冻结 controller 的 admission.verify，传入保留 admission.json 的 SHA256；随后按 executions.json 的 command，在新建工作目录中复跑 ELF 副本。原 ELF 保持只读，副本须与其摘要一致。正常返回 0，标记 guard-positive-control 的四项返回 139；core limit 为 0。完整重新构建应使用 run_image 与新的空 output 目录，禁止覆盖 build-v3 证据。

setup.py/revise-v2.py/revise-v3.py 记录候选生成过程；execute.py 是本轮一次性执行脚本，工作目录已存在时会拒绝，重跑时须使用新工作目录。脚本、请求、原始回执和源归档均有摘要绑定。固定语料可从 setup.py 的独立 codec 分类段重新生成核对。

## 审核记录与下一步

固定控制树 SHA256：`71d141c484f7dde084991e2d11f931abdacb8a17219cbd4d46e5d6bb7d223b75`。

相对已批准 Buffer 控制器，仅扩展既有候选 registry：新增 text 接口/实现、测试消费者与测试专用 guard 提供者，以及三个测试组合。门禁、策略、工具链、镜像、原有源文件及旧 ABI 不变。根项目新增的文档/证据登记仅用于追踪，不等于生产准入。

根 AGENTS 要求「控制面变更须独立审核，不能自批」。用户已明确批准本固定包用于本地实验；不包含完整 G07、T11 整体验收、未来模块或生产发布。尚需资源/故障覆盖审计、适用 sanitizer 的实施决策及证据、边界大输入与性能基线审查，再判断 T11 技术退出条件，不能直接宣布进入 JSON/TUI。
