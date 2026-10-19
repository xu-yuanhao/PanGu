# T11 分配器替换、资源与大输入复核

日期：2026-10-20。T11 IN_PROGRESS。本轮补齐本地技术验证，固定候选控制登记已获用户本地实验批准；生产阶段和预算验收仍未完成。

[候选登记](t11-resource-candidate.json) · [源码与证据](t11-resource-evidence.json)。最终目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t11-region-swap-kqbd7_13`，控制器 controller、源码 candidate-v2、构建 build-v2、请求 request-v2.json、执行 executions-v2.json。source.tar 保存全部 35 个原生源文件。

## 修复了什么验证缺口

此前的 arena/mmap 与静态页组合替换了 memory 提供者，不能单独证明 allocator 算法可替换。本轮新增四个测试模块：独立固定槽 region 提供者、仅依赖接口/Buffer/text 的消费者、arena 组合、固定槽组合；没有新增生产模块或生产服务 ID。

固定槽提供者独立于 arena 和 mmap，拥有 16 个 256 字节静态槽；整进程作为测试 scope，epoch 在进程结束前固定，逐槽分配且不支持逐块释放或 scope reset。它用于验证 allocator 选择，不作为默认生产 allocator。超过其单槽容量、支持的对齐或槽数量会报告资源不足；具体容量策略允许不同，region 接口语义一致。

相同消费者验证有界 Buffer、UTF-8 校验、自引用扩容、容量失败原子性和一万次容量复用。两个组合的消费者 `.o` 在 debug 和 release 下分别逐字节相同，实际编译依赖仅包含 bootstrap/region/buffer/text 头，不包含 arena 或 memory 头。固定槽 ELF 没有链接 arena/mmap 的实现。

arena 测试组合额外包装实际 PAL，核对申请与释放的 context、地址、长度、次数和未释放字节数：整个消费者运行只 map 一次、由原提供者成功 unmap 一次，各 4096 字节；退出时 ownership 与 arena mapping 状态均清空。这是测试内资源断言，没有引入产品代理层。

## 大输入与页边界

三个 text 后端 × 两种模式均校验 1 MiB 的固定混合文本（ASCII、NUL、二字节字符），预期 786432 个标量值。删去末字节后，预期完整前缀 786431 个标量、错误起始字节 1048574。超出小 Buffer 配额的大输入追加失败，不改变旧内容/长度/版本与 arena 用量。

另一组把 Buffer 填到整页 4096 字节，guard 组合的下一页不可访问。超额单字节追加与满页自追加均拒绝，旧 view 保持有效、内容逐字节不变；clear 后复用整页容量，不增加分配。沿用上一包全部标量值、固定 codec 语料、借用失效和保护页正对照。

最终十二个组合 × debug/release：24 次冻结构建、保留产物复验和正常执行均通过；4 次左右保护页正对照仍按预期返回 139。此前 arena 包的 mmap/munmap 拒绝注入证据保留；本轮未把旧故障执行冒充新 ELF 的故障执行。

## UBSan 补充实验

单独目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t11-ubsan-tracked-94yweid9`。同一冻结镜像，以正式参数为基础给 C 编译增加：

```
-fsanitize=undefined -fsanitize-trap=all -fno-sanitize-recover=all
```

24 项构建及正常执行通过，4 项保护页对照仍为 139；6 次主动执行运行期 `1u << 32` 均触发预期 SIGILL（bwrap 返回 132）。相同违规分支的未插桩对照在两种模式下均到达标记退出 72，证明插桩对照不是普通正常返回。最终补充源与正式候选仅相差测试程序中的违规分支，全部 16 个生产实现/接口文件逐字节相同；静态链接无未解析的 sanitizer runtime 符号。

这是未准入的补充检查配置，不能写成 native_build 新模式、正式 sanitizer 门禁或批准预算。原检查器精确参数集合、镜像和工具链均保持原样。将插桩配置纳入正式必需检查，应另行设计并独立审核配置及违规回归，不在本次实现里放宽参数限制。

UBSan trap 检查不覆盖手写汇编访存，也不是 ASan/TSan。当前服务契约是 single-thread；不能由本轮推导并发安全。保护页、哨兵、分配配额和生命周期断言补充检查已记录的风险，无法保护任意失效 raw pointer。

## 本地退出审计

| T11 条件 | 实际证据 | 状态 |
|---|---|---|
| 显式长度与容量 | Buffer 与 UTF-8 固定接口；容量/溢出/失败原子性 | 本地已验证 |
| 更换 allocator 不改消费者 | arena 与独立槽分配器；相同消费者对象摘要 | 本地已验证，固定控制登记已获本地实验批准 |
| C/Assembly 与边界输入 | 24 项常规矩阵；NUL/Unicode/1 MiB/满页 | 本地已验证 |
| scope 释放与借用失效 | 原提供者计数；epoch/revision；旧包 map/unmap 拒绝 | 本地已验证的范围见各原始回执 |
| 内存错误检查 | guard 正对照；UBSan 补充对照；OOM 与资源不变量 | 局部证据充分，完整生产 G07 未准入 |
| 性能与预算 | ELF/对象字节记录，循环验证资源不再增长 | UNBASELINED，不作 G09/G10 通过结论 |
| 阶段整体批准 | 固定资源候选已获本地实验批准；上游生产准入仍受阻 | T11 保持 IN_PROGRESS |

此处可以作为后续 T12 本地原型的基础审查材料；不能宣称 T11 已满足正式交付，也不能借此绕过 T12 新服务登记审核。尚未实现丰富 String 操作、SIMD、流式 UTF-8 错误策略、UTF-16 索引兼容或 GUI/TUI。

## 控制审核

固定控制树 SHA256：`c835648d00146ec52eb3412ed214f27f85ef62019074568bb9d353e5f65ae00b`。

相对已批准的 UTF-8 控制器，仅扩展既有 registry 的四个测试模块和两个测试组合；同一固定候选内更新 text 测试语料，新增资源计数测试。16 个既有生产实现/接口文件保持一致，门禁、策略、编译参数、镜像及 ABI 未变。所有实际使用的输入摘要由最终 admission 回执绑定。

按根 AGENTS「控制面变更须独立审核，不能自批」，用户已明确批准本固定包用于本地实验。该批准不包含 UBSan 作为正式构建模式、完整 G07、T11 整体验收、未来 JSON 服务或生产发布。

附：本轮修正根目录证据的摘要字段定义：`candidate_tree_sha256` 对齐原始 admission 输入库存，`registry_sha256` 使用规范化 JSON 摘要，原始文件摘要另记 `registry_file_sha256`。原始固定包、approval 和构建回执均未改写。

## 源码发布记录

完整候选已发布到 [实验分支固定提交](https://github.com/xu-yuanhao/PanGu/tree/912c858acdee8edfd5fa18b00f4f1e30ada084af)，共 114 个登记输入（79 个控制库存文件与 35 个原生文件）。推送前固定控制器 preflight 与原候选逐项一致，推送后回读远端 SHA 一致。[发布记录](t11-source-publication.json)。该分支不是生产 main。
