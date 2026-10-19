# T12 有界 UTF-8 字符串序列化预览

状态：IN_PROGRESS，未获独立批准。基于已批准 reader，现有 JSON 模块新增 quote.c 和一个 quote_utf8 导出；旧 parser、reader 实现与 ABI 不变。没有新增分配器、模块或构建组合。

候选与完整回执位于 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-quote-032yqsn2`：最终输入 request-v4.json / candidate-v4，控制器 controller-v2，产物 build-v4。evidence.json 绑定源码、归档、语料和运行回执摘要。固定控制树 `9e179618f81e66a6694f34e8a06d25d15ce1fe7eabed4c335704165ae353c14a` 尚待独立审核。

接口只把合法 UTF-8 编码为包含双引号的 JSON 字符串字面量，没有尾部 NUL。先完整校验和测长，再写输出；容量不足报告所需长度且不改输出。拒绝输入与输出/长度存储重叠，保留嵌入 NUL，非法 UTF-8 不做 replacement。控制字符使用短转义或小写十六进制转义，反斜线与双引号转义，其余合法 UTF-8 字节保留。

实际验证：28 项冻结构建及断网正常执行通过；4 次继承保护页正对照均返回预期 139。273 条 Python json.dumps 与 Node JSON.stringify 的精确字节共识语料覆盖 ASCII、Unicode 边界、BOM、非字符及固定随机种子，逐条检查四种容量、测长、输出哨兵，并经已批准 parser/reader 往返比对原字节。另验证非法 UTF-8、存储重叠拒绝、输入及输出紧贴保护页边界。

两次失败原样保留：build-v2 的测试映射类型名错误；build-v3 的测试聚合零初始化触发未准入 memset。candidate-v4 用真实类型和显式字段赋值修复，无 libc 或门禁放宽。

归档恢复后输入一致，28 项保留构建产物绑定复验通过。补充 UBSan 的 28 项构建/正常执行、4 次保护页对照（139）与 6 次违规移位对照（132）均符合预期；与普通候选相比仅测试正对照源文件不同。正式工具链/门禁/构建参数未修改，不代表正式 sanitizer 准入或 ASan/TSan/汇编全覆盖。

[固定候选登记](t12-quote-candidate.json) · [源码和回执摘要](t12-quote-evidence.json)。待独立批准。尚未完成对象/数组与数值序列化、数值读取、Schema、Pi 适配、完整 T12 或生产准入；不勾选这些任务。
