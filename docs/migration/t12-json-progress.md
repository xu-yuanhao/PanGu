# T12 有界增量 JSON token 候选

日期：2026-10-20。T12 IN_PROGRESS；本固定候选已获用户独立审核批准，仅用于本地实验。仅作为本地原型，不代表完整 T12 或生产准入。

[候选登记](t12-json-candidate.json) · [源码与证据](t12-json-evidence.json) · [Pi 适配边界](t12-protocol-boundaries.md)。最终原始目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-json-6w0v_ftw`；控制器 controller-v2、源码 candidate-v2、构建 build-v2、请求 request-v2.json、执行 executions-v2.json。source.tar 包含全部 40 个原生文件，恢复后已逐项匹配原输入并复验保留构建产物。

## 实现与状态契约

新增 foundation.json v1：init、feed、get_result。调用者提供 1144 字节 parser 和 token 空间；每个 token 48 字节，记录类型、key/escape 标志、起止字节位置、parent、跳过子树的 next、数组值数或对象成员数。根 parent 使用 UINT64_MAX，重复 key 保持源顺序。字符串范围包含引号，容器范围包含括号。

不自行分配、复制完整消息、构建通用容器或进行递归调用。固定最多 64 个容器 frame，调用者另设更小深度、总字节和 token 容量。只向前处理分片；数字终止符最多再经一次结构处理，UTF-8 仅保留最多四字节局部状态，不重解析已消费的前缀。实际 JSON 对象文件的外部调用只有 pangu_text_next；未引入 libc、浮点转换或第三方 JSON 运行时。当前 text 服务的组合依赖继续带入已有 Buffer/region，不新增一层纯转发抽象。

非 final feed 一律返回 13，get_result 暴露 NULL/count=0，即使当前输入已是完整 `{}`。只有 final 且整个文档合法才返回 0 并公开 token。语法错误 14、字符串 UTF-8 错误 12、配额 2；最终截断 13 为终态，后续 feed 返回 10，必须 init 后才能再次解析。失败/未完成的 token 空间可能含脏数据，不允许消费；进行中的 error_offset 不作为最终错误位置使用。

这是 API 状态契约，不能隔离恶意原生消费者或阻止调用者直接读取自己提供的存储。parser/token/result 和输入的可写区域须不重叠；调用者不能改写内部状态。工具执行门控仍须在后续阶段单独实现并验证，不能把本轮当作 T24 执行安全证明。

## Unicode、数字与所有权

语法按 [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259) 核对。原始字符串字节按严格 UTF-8 校验，支持跨 chunk 多字节；转义序列逐段处理，包含跨 chunk 的 `\uXXXX`。转义中的孤立 UTF-16 surrogate 按 JSON 词法保留，未尝试无损转换为 UTF-8。BOM 不被隐式剥离，原始控制字符与非法转义拒绝。

数字验证词法并保留原始范围，支持负零、大整数和大指数的合法文本；没有宣称得到与 JS Number 一样的数值。字符串未解码、key 未规范化或合并、重复 key 未执行 last-key-wins；这些属于后续 reader/Provider/Schema 层的工作。

feed 不保留分片指针，调用期间输入必须可读；token 的字节位置引用调用者保留的整条逻辑消息。重置 parser、复用 token 空间或修改原始消息都会使旧 raw 结果失效，当前没有 checked document owner 或任意 native 指针保护。

## 验证

十四个组合 × debug/release，共 28 项冻结镜像构建、保留产物复验、断网正常执行通过；原有 4 项保护页正对照按预期返回 139。此前 16 个生产实现/接口文件未改，新增 json.h 与 JSON 实现及测试消费者。

JSON 在 mmap 和 guard 提供者下各跑两种模式，四次运行分别覆盖：

- 636 条 Python JSON 与 Node JSON.parse 独立达成一致的语法语料：219 合法、417 非法。原生未调用两种运行时。
- 每条语料遍历全部双分片切分点，共 7853 次；另逐字节输入 7217 个分片。最终 token 全字段、状态和错误字节位置必须与一次输入相同；失败时不得公开部分结果。
- 手工独立指定的嵌套对象/数组 token 拓扑；空输入、空白、各原语、重复 key、转义/NUL/非 ASCII、畸形/截断 UTF-8 与精确错误位置。
- 64 层容器，深度/字节/token 配额拒绝、非法参数不推进、结束后拒绝继续 feed；合法前缀后尾随垃圾不能成功提交。
- 1 MiB JSON 字符串一次输入与每片 257 字节输入结果一致，两个 token 空间即可运行，不按字符分配。
- 输入和 token 配额空间都放在页尾；容量 0/1/2/3 遍历切分点，验证少一个 token 时在写入前拒绝、恰好够用时成功，前置哨兵不变；页尾未闭合字符串/截断多字节不越界。

补充 UBSan 位于 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-ubsan-h1huradh`：28 项构建/正常运行通过，4 项保护页对照返回 139，6 项已记录的运行期非法移位对照返回 132。额外 C 参数仅在补充实验使用：`-fsanitize=undefined -fsanitize-trap=all -fno-sanitize-recover=all`；正式检查器、参数集合、镜像均不变。该实验未成为正式准入模式，也不覆盖 Assembly 的所有访存或并发。

测试 release ELF 约 123 KB，包含 636 条固定语料与测试代码，不能当作最小 Agent 体积。预算仍为 UNBASELINED，没有吞吐/RSS/启动性能验收结论。

## Pi 真参考结果

本轮实际在既有独立 oracle 工作副本导入冻结 Pi 的 json-parse.ts，确认该源文件与 references/ 逐字节一致，执行 11 条最终参数/流式边界样例。回执保存 Node、源码、runner 和 partial-json 0.1.7 文件摘要；references/ 未修改或作为产品构建输入。

实际结果确认：空白按空对象处理，非法反斜杠及 raw control 有限修复；截断最终参数拒绝；数组和 null 拒绝作为工具参数对象。重复 key、孤立 surrogate、大指数的 JS 可观察值也单独记录（非有限数使用带类型标记的开发回执表示，避免 JSON.stringify 静默转 null）。

这是 Pi 基线回执，尚未实现原生 Provider 修复或 Schema 转换，不能声称两者与基础 token 层全面等价。Python/Node 语法共识与 Pi 参数容错是两套不同证据。

## 审核记录与未完成

固定控制树 SHA256：`8df9fa8e0d15c6e81c51d54632a2f2e8e671fdbc77555c3146f244a68ae641f3`。

相对已批准 T11 资源控制器，只扩展既有 registry：新增 JSON 接口、foundation.json 实现、测试消费者及两个测试组合。既有门禁、策略、镜像、工具链和旧 ABI 未变。根 AGENTS 要求「控制面变更须独立审核，不能自批」，用户已明确批准本固定包用于后续本地实验。

后续仍须实现 token reader/字符串解码与序列化、字段数值与缺失/null 语义、有支持范围的 Schema、Pi 修复/转换适配、针对协议的持续 fuzz 与完成门控。当前增量 token 基础不是 SSE/MCP 帧解码器。整体 T12、完整 G07、正式 sanitizer 配置、性能预算和生产发布均未批准。

复验时使用 request-v2.json 的固定路径、控制树/镜像摘要和原始回执；新的构建与执行必须另开目录，禁止覆盖 build-v2 或已有 work/exec 证据。源码恢复复验回执见证据文件的 restored_verify。开发生成脚本记录了从旧包派生的过程，重新生成须使用新的外部工作目录。

## 源码发布记录

完整固定候选已推送至 [实验分支固定提交](https://github.com/xu-yuanhao/PanGu/tree/c015384e9dc1b3a518a607f23b22e9b0035b73c3)。119 个输入（79 个控制库存文件与 40 个原生文件）在推送前通过固定控制器 preflight 与原候选逐项比对，远端 SHA 已回读一致。[发布回执](t12-source-publication.json)。
