# T12 有界对象/数组转换本地实验

2026-10-20，T12 IN_PROGRESS。实施与审查均为同一Agent，按用户授权自审自批，仅批准固定包用于本地实验，不代表独立审核或生产放行。

[登记](t12-tree-candidate.json) · [证据](t12-tree-evidence.json) · [源码发布](t12-tree-source-publication.json)。实验目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-pi-tree-v2-wl9o2lyy`。控制树SHA256 `5757e70720c733a84229b875d3ebe6170996a3044d705104bb144c292e984f6c`，候选树SHA256 `d8e8a861cba35dac997e6a4a02e9c8f81ec274ef379c708353b574061feeea63`。

## 实现与边界

既有pi-arguments模块新增coerce.c及tree_convert导出，48字节tree_schema描述承载JSON视图和调用者持有的token索引Kind表。旧接口布局和实现不变，没有新模块、服务、构建模式或运行时依赖。探针因直接调用最终Schema验证，明确增加schema依赖与foundation.schema服务消费；不是修改检查器豁免符号。

TypeBox的隐藏Kind不能从Schema JSON恢复：Type.Literal("")与Type.String({const:""})的JSON相同，内部null输入的公共接受结果不同。调用方必须保存真实来源，映射1～5基础原语、6 Array、7 Object、8 Literal、9 Enum，0为不触发typed转换；当前不接受其他typed Kind。Kind表不提供来源认证，借用生命周期由调用者保证。

转换按typed与plain分两次调用，每次输出后重新严格解析。typed数组可把非数组包装为单元素数组，plain数组不包装；typed literal/enum先约束转换候选，普通const仍由后续验证处理。plain多类型按Pi的既有类型匹配和首个发生变化的原语候选规则执行。对象使用decoded-key last-wins，生成字段只保留最后一个逻辑名称，未匹配子树保留原始JSON。原始孤立UTF-16转义和非有限数字词法可保留；替换数值有限，负零跨阶段用-0传输。

固定64层迭代栈、调用者scratch、测量后写出，无堆分配、产品递归或libc。原文、结果、tokens、视图、Kind表与所有可写范围检查分离；输出/长度失败原子，scratch和运行时report可变。work为确定性操作/检查字节配额，不是CPU性能预算。selected Schema子视图只检查其子树的Kind；宿主额外验证无关兄弟元数据不会阻断。

typed对象属性名实际是未转义正则，当前遇正则元字符显式返回9；plain属性仍按名称查找。union/高级Schema由既有严格Schema服务返回9，不计作支持成功。Null服务Kind受支持；手工Literal/Enum null等不在该typed候选范围内。整个primitive/array根时Pi丢弃Value.Convert返回值，本阶段返回转换结果，不能直接宣称整个公共函数兼容。

## 实际验证

- 606条真实参考用例，保留optional-null、typed原地修改/返回和plain结果及公共接受/拒绝，各阶段在后续变更前快照。1212次阶段检查中1128次成功结果一致、84次按预先限定的union/typed正则名称范围拒绝；没有把拒绝算作成功转换。
- 四工具read/write/edit/bash的Schema从冻结Pi源码原样抽取并核对摘要；286条原生解析→optional-null→typed→plain→严格Schema链路与公共Pi接受/拒绝及成功语义一致。此组合在测试消费者中，没有执行工具，也没有新增生产工具调用协调器。
- 宿主22230次不足容量检查、13项work/depth/nodes/别名/Kind表/未发布结果/子视图检查通过。原生同语料逐容量循环、12项拒绝控制、额外escaped重复字段last-wins及原文/scratch/output/Kind表/report/written六个页尾槽通过。
- 普通与补充UBSan各36项构建、48次正常执行；原有JSON四分片全保留，4项保护页故障对照139、12项无效探针参数247及补充6项移位对照132通过。本包最终运行无超时；所有旧探针函数体及语料前缀保留。
- 59份原生源码归档恢复，preflight输入一致并核对保留产物绑定；恢复不是新构建。UBSan仅补充实验，不覆盖全部汇编，也不是正式sanitizer配置。

第一轮宿主有24次quote容量探测接线失败，旧报告/源码保留；修正后重跑通过。首个冻结候选因探针缺少pangu_schema_validate直接依赖被门禁拒绝；原失败目录保留，补齐声明后使用新固定树重建。检查器、预算、30秒时限和基线未放宽。

## 未完成

union/高级Schema、typed正则属性、公共函数全部根值/错误文本、Provider最终事件、wire/session、正式资源预算/完整G07、完整T12及受保护发布仍未完成。native_matrix=PASS与release=BLOCKED同时成立。

源码发布至 [`codex/t12-pi-tree-conversion`](https://github.com/xu-yuanhao/PanGu/tree/887e3edc5a8d930f01270543c9afe9ad2b29583d)，提交 `887e3edc5a8d930f01270543c9afe9ad2b29583d`；发布副本全部preflight输入与固定候选一致，远端ref回读一致，不代表生产准入。

## 统一校验入口草稿（后续实验，未准入）

目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-validation-wip-wiskk69h` 从上述固定包扩展validate.c和头文件，60份源码；新文件、导出和96字节storage/112字节result描述尚未登记，不属于上方已批准包。`wip-evidence.json`绑定源清单和验证脚本/结果。

入口组合最终对象解析、optional-null、typed/plain树转换和严格Schema验证，只有最后验证成功才发布借用结果。非final清空结果且不碰工作区；运行时失败不发布input/tokens，非法参数/坏Schema结果头保留调用者原result。JSON字节/深度与跨四个Schema阶段的总work分别受限；调用者提供互斥的repair、双字节缓冲、tokens、scalar、drops存储，需遵守返回状态和借用生命周期。Typed Array根仍显式不支持，不能冒充完整Pi根值语义或工具执行授权。

实际宿主验证：286条四工具公共Pi对照和286次不修改工作区的非final检查；全部3341个不足总work配额返回2且不发布，精确3342配额成功；94组双缓冲容量、15组token容量、原始字节配额及别名/元数据拒绝通过。另1570条真实Pi最终文本解析+验证对照，196次接受、其中56次修复成功，拒绝调用不发布结果。宿主O0/O2无未解析符号链接通过。尚未完成原生探针、冻结构建、保护页、UBSan、归档恢复或控制面自审，不新增完成勾选。
