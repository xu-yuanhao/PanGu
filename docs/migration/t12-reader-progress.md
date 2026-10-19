# T12 JSON 字符串与对象字段读取候选

日期：2026-10-20。T12 IN_PROGRESS，固定候选已获用户本地实验批准；尚未完成序列化、数值转换、Schema 或 Pi 工具参数适配。

[候选登记](t12-reader-candidate.json) · [源码和证据](t12-reader-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-reader-wvl3c9k0`，controller、candidate、build、request.json、executions.json。source.tar 包含全部 41 个原生源文件，恢复后的输入库存与保留 ELF 已复验。

## 接口及模块边界

在原 foundation.json 模块增加 read.c，新增两个导出，未增加服务 ID、分配器或通用容器层：

- string_decode：把一个成功结果中的 string token 无损解码为调用者提供的 UTF-8 字节区。
- object_find：按解码后的 UTF-8 key 查询对象自身成员，返回 value token 索引，后出现的同名 key 覆盖先出现者，跳过嵌套子树。

原增量 parser C 源码逐字节未变；parser/token/limits/result 尺寸仍为 1144/48/16/32 字节，旧导出和签名不变。旧生产文件仅 json.h 追加声明/契约，其余 17 个旧生产实现/接口文件保持一致。read.c 的实际外部调用只有 text_next/text_validate；没有平台、libc、堆或浮点依赖。

两个接口只接受成功的 get_result 及对应的完整、未修改原输入。现有 raw result/token 仍由调用者持有，重置 parser、修改原输入或复用 token 存储都会使旧借用失效；不声称认证伪造元数据或自动发现 stale raw pointer。对结果头部、索引、范围、子树和存储重叠的检查仅为 API 合法使用提供诊断，不能取代原生隔离。

## 字符串与失败语义

解码分两遍：先校验并计算长度，再确认容量后写入，不分配临时整串。输出没有尾部 NUL，嵌入 NUL 按正常字节保留；调用者须使用长度，不能直接把结果视作 C 字符串。

支持短转义、大小写 hex、BMP、合法 surrogate pair、原始 UTF-8、NUL 和非字符码点，不做 BOM 剥离或 Unicode 规范化。合法 JSON 中的孤立 escaped UTF-16 surrogate 不可无损表示为 UTF-8，返回 9 unsupported，原始 token 保留；没有静默 replacement 或丢弃。

在有效、不可变源和互不重叠的调用者存储契约内，失败不修改任何输出字节。容量不足返回 2 并报告所需长度；转换失败在进入读取后将长度置零；参数/结果头部检查失败时输出保持原样。调用者始终先检查返回状态，不消费失败时的字节。空串允许 NULL/零容量成功，非空串可用 NULL/零容量获取所需长度。

输出区、长度/索引输出不能覆盖源、result 或 token；字段查找的索引输出也不能覆盖 query key。范围和重叠检查不依赖未定义的跨对象指针比较，目标仍仅为已登记 Linux x86_64。

## 字段语义

按解码后的 key 比较，因此 `"a"` 和 `"\u0061"` 可命中同一查询，重复成员取最后一个。空 key、NUL key、非 ASCII、emoji 和 `__proto__` 都作为普通自身成员处理，不创建宿主语言对象。

缺失返回 3，索引为 UINT64_MAX；显式 null 返回 0，索引指向 NULL-kind token。查找不递归搜索嵌套对象，利用 next 跳过子树，不为一次查找建立 hash map。query 必须是合法 UTF-8；含孤立 surrogate 的合法 JSON key 无法等于 UTF-8 query，不影响查找对象中的其他可表示 key。raw token 接口仍可观察这类成员。

## 实际验证

28 项冻结构建、保留产物复验和断网正常执行通过，继承的 4 次保护页正对照返回预期 139。JSON 的 mmap/guard 两个后端各跑 debug/release，四次均覆盖此前 636 条语法、全部切分点、1 MiB 与配额用例，并增加：

- 458 条 Python/Node 共同核对的字符串样例：444 可无损 UTF-8 解码，14 为非字符串或不可无损表示的 UTF-16 值；容量为 0、少一字节、恰好够用、多一字节以及测长调用均核对状态/长度/完整输出与哨兵。
- 33 条去重后的字段查找样例：解码等价重复 key、嵌套跳过、缺失/null、空/NUL/Unicode/`__proto__` key，以及对象类型拒绝。
- 全部 65536 个 UTF-16 单元，包括 2048 个孤立 surrogate；3072 个 pair 边界向量，覆盖每个 high unit 的 low 两端及全部 low unit。预期 UTF-8 使用独立算术编码器核对；没有声称遍历所有非 BMP pair。
- 合法前缀后遇到不支持 surrogate 时失败原子性；非 final 结果、越界索引、源/结果/token 重叠拒绝；非法 UTF-8 query。
- 将 escaped pair 输入放到页尾，输出四字节 emoji 恰好贴住下一保护页；少一个字节时整片哨兵保持不变。

Python 使用 strict UTF-8 编码；Node 使用 JSON.parse 的字符串值、isWellFormed 与 Buffer UTF-8 编码建立相同的无损表示范围。两种 JSON 实现本身仍接受孤立 surrogate，unsupported 是本原生 UTF-8 reader 的明确范围，不能把它写成“Pi 也拒绝”。字段查找参考 Node Object.hasOwn/实际值及 Python 字典的 last-key-wins 结果。回执保存原始输入、期望字节、运行时路径/版本/摘要与生成器。

UBSan 补充目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-reader-ubsan-c24yx5s3`：28 项构建/正常运行、4 项保护页对照、6 项违规移位对照均符合预期。正式 gate/profile/工具链/镜像没有修改；插桩只作补充实验，不代表 ASan、TSan、Assembly 全覆盖或正式 sanitizer 准入。

ELF 体积见证据，包含大批测试语料，不作为 Agent 体积。预算仍 UNBASELINED，没有吞吐、RSS 或完整 G07 验收结论。

## 审核与下一步

固定控制树 SHA256：`3b8e916d94590852dc15a6ed891ea8a3cb4abd70e06a411a1e77be36ae778da0`。

相对已批准增量 token 控制器，仅更新现有 registry 中 JSON 模块的新文件、导出和职责说明；没有增加模块/服务/构建组合，正式门禁参数和旧 ABI 不变。根 AGENTS 要求「控制面变更须独立审核，不能自批」，本次功能不在此前 token 包的批准范围内，用户已明确批准本固定 reader 包用于本地实验。

下一步推进有界字符串序列化、完整字段数值策略、受支持 Schema 和 Pi 修复/转换适配。当前 reader 不等于 TypeBox 校验器，也不等于 Pi fs.writeFile 对孤立 surrogate 的替换行为；这类兼容选择必须有单独证据和范围。完整 T12、生产 G07、正式 sanitizer、未来模块和发布不在本包内。

源码归档及恢复复验为最终权威输入；setup.py 记录最初派生过程，初次编译前另做过输出语义/索引检查审查，记录在 initial-review.json。测试模板和语料生成器一并绑定，不能把生成器草稿当作最终源码。复验须开新目录，不覆盖 build、已有 exec/work 或冻结控制器。
