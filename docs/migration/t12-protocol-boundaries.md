# T12 开工边界：严格 JSON 与 Pi 兼容适配

日期：2026-10-20。状态：参考源码审查与 11 条 Pi 真基线已记录；增量 token 局部候选见 [实现与证据](t12-json-progress.md)，完整 T12／Provider／Schema 兼容仍未完成。T11 固定资源包已经获批用于本地实验，生产阶段批准仍未完成。

## 依据

Pi 冻结提交 `2ec55d808d455b1a64a2bf7888198207a096120a`，实际阅读：

- `packages/ai/src/utils/json-parse.ts` 的 repairJson、parseJsonWithRepair、parseToolCallArguments 和 parseStreamingJson。
- `packages/ai/src/utils/validation.ts` 的 validateToolArguments：argumentsError 先行阻断，随后 clone、optional-null 规范化、Value.Convert、Schema 校验，部分 JSON Schema 走自己的 coercion。
- `packages/ai/src/types.ts` 的 ToolCall.arguments 与 argumentsError。
- `packages/ai/test/json-parse.test.ts` 的完整参数与截断行为用例，后续须从冻结测试中提取对应 oracle case，不能用本审查文档代替执行证据。

## 三条不同的路径

| 路径 | 原生边界 | Pi 的具体行为及迁移要求 |
|---|---|---|
| 基础 JSON | 完整输入、严格语法、显式 UTF-8 字节范围、深度/字节/token 容量限制 | 基础解析器不自动调用修复；不能以更宽松的 partial parse 返回“已完成” |
| Provider 参数适配 | 明确维护原始参数文本与最终状态；必要修复必须可追踪 | Pi 最终参数先做普通 parse，再尝试有限 repair：字符串内 raw control 转义，非法反斜杠变成字面反斜杠；空/纯空白按空对象处理；最终值必须是非 null、非数组对象 |
| 工具 Schema 边界 | 完整参数通过语法与适用 Schema 后才允许调用工具 | Pi 检测 argumentsError 后才转换/校验，存在 optional null 规范化和类型转换；不能默认“全部严格拒绝不转换”就兼容 Pi |

UI 部分参数展示是独立的临时状态。Pi 的 parseStreamingJson 会尝试 partial-json，得到部分对象供流式消费；这个对象不能作为执行授权。原生实现必须保留明确的 incomplete/final/error 状态，完成前不出具可执行参数，不把 `{}` 同时用作“合法空对象”和“解析失败”唯一标志。

## 实现顺序与约束

1. 先实现有界严格 JSON token/读取基础：类型、字节范围与借用生命周期明确，输入/深度/token 空间均由调用者显式限制。只覆盖当前 wire/session 和工具模型实际需要的结构，避免先造容器框架。
2. 保留数字词法信息，先验证语法，再在具体字段处决定数值转换与范围；不靠未登记 libc/通用浮点库隐式引入依赖。JS Number 舍入、超大整数、指数溢出与负零均须进入差异清单，未验证前不声称等价。
3. 字符串转义与 UTF-8 区分：原始非 ASCII 字节必须合法；`\uXXXX` 的 surrogate pair、单独 surrogate 和 NUL 必须有明确表示/错误策略。JS 可以保存孤立 UTF-16 surrogate，UTF-8 不能无损直接表示，不能默默丢失后声称一比一兼容。
4. 对象重复 key、缺失 key、显式 null、数组/原语作为顶层、BOM、空输入分别有用例。基础 token 层保留原始信息，进入对象适配时明确 Pi 的 last-key-wins 等可观察语义，不通过宿主语言 map 的偶然行为决定。
5. 在严格基础上实现逐帧输入和最终提交。跨 chunk 的 UTF-8 多字节、`\u`、字符串转义、数字指数和容器闭合必须与一次输入结果一致；不能每收到一个字节就全量重解析，形成无记录的平方复杂度。
6. Provider 修复适配与 Schema 子集按真实 Pi 参考分开实现和登记，修复后的对象仍需完整语法/对象类型/Schema 校验。修复不得补齐未结束工具参数并执行。
7. 不支持的 Schema 关键词须显式拒绝或报告 unsupported；不能忽略后返回校验成功。最初子集以四工具和最小 Agent 所需字段为准，逐项登记类型/必需项/枚举/数组/附加字段/嵌套/数值及字符串约束，尚未验证的转换列为待办。
8. 用冻结 Pi 真 oracle 与独立严格 JSON oracle 分别建立语料，按预期支持路径对比；语法层与 Pi 容错适配层不能混成一张“全兼容”成绩单。

## 开工前必须固定的验证集

正常：空对象、嵌套/数组、各原语、NUL/Unicode/转义、非 ASCII key、重复 key 和有序观察、受控边界数字。

拒绝/限制：缺少结束括号/引号/小数或指数尾部、非法转义、raw control、非法 UTF-8、尾随垃圾/多文档、深度/token/字节超限、最终工具参数非对象、未支持 Schema。

Pi 适配：空/纯空白参数、修复非法反斜杠、转义 raw control、截断参数保留错误、argumentsError 阻断调用、optional-null 与实际 TypeBox/JSON Schema 转换差异。

分片：每一条固定边界语料遍历全部切分点；多切分点随机种子固定；必须证明最终结果和错误种类与一次解析一致，失败后不能消费部分 token 或执行工具。

资源：提供者替换、OOM、失败原子性、scope 失效、guard page 与已实现的 sanitizer 补充检查；避免以固定小输入正常就声称大输入/性能合格。

## 状态边界

本文件是实现依据与待测清单。T12 仅勾选有证据的局部预览，不勾选整体完成；不据此批准生产模块、新依赖或兼容差异。新 JSON 服务与构建组合须在实际候选、证据就绪后按根 AGENTS 当前规则记录审查批准；用户已授权 Agent 自审自批，不能伪称独立审核。T11 正式 G07/预算和全阶段验收保持原状态。

## 四工具 Schema 的源码核对

同一冻结提交中，`packages/coding-agent/src/core/tools/` 的输入声明：

| 工具 | 必需字段 | 可选字段／嵌套 |
|---|---|---|
| read | path: string | offset、limit: number；声明并不是 integer |
| write | path、content: string | 无其他输入字段声明 |
| edit | path: string，edits: array | edits 每项 oldText、newText 均为 string |
| bash | command: string | timeout: number |

当前 edit 声明没有 minItems，工具说明中的“One or more”不能自动当作 Schema 已编码的限制；执行前另调用 validateEditInput，且存在 prepareArguments 处理，后续须分别追踪。additionalProperties 没有统一显式设置为 false，不能直接给原生四工具全部加上该规则并声称兼容。

输出结构还包含 read 的 string/image union、bash 的 boolean/number/可选 string 等，输入最小子集不能冒充全部输入输出 Schema 支持。数值下游也有 JS truthiness/Math/slice 等可观察行为，整数收紧和超限策略须以相应工具 oracle 验证，不能由字段描述自行推断。

## 真实参数转换基线（2026-10-20）

另实际执行冻结 Pi 的 validateToolArguments，共 25 条用例，Schema 夹具按上述四工具的输入字段/可选性/嵌套声明构造，使用该工作副本的 TypeBox。没有实例化工具工厂或执行工具，因此只证明参数校验函数和这些夹具的行为，不证明完整工具调用链。原始输入、结果、错误及源码/运行时/TypeBox 文件摘要记录于 [reader 证据](t12-reader-evidence.json) 的 pi_schema_boundary_baseline。

确认的边界：optional null 被删除；数字字符串转 number，boolean 可转 0/1；必需 string 字段的 number/null 可转为 `"7"`/`"null"`；read 的小数/负数和额外字段在该 Schema 下可以通过；空 edits 数组通过 Schema，但不代表工具级业务校验已通过。孤立 surrogate 和 NUL 仍可通过 string 类型检查，不能把原生无损 UTF-8 reader 的 unsupported 当作 Pi 校验拒绝。

大指数 Infinity 被 Number Schema 拒绝；`1.7976931348623158e308` 在 JS 中舍入到有限的最大 double 并通过。这进一步表明，原生 number 语义不能以十进制字面大小或简单整数解析替代。该 25 条基线建立时尚未实现数值/Schema；现已完成后述数值读取实验，Schema 与这些完整转换路径仍未实现，不能把参考基线写成原生兼容通过。

## 数值读取历史基线（早于原生转换实现）

2026-10-20 建立 547 条 JSON 数值字面量语料，保存于 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-number-baseline-i5z0ch26`，oracle.json SHA256 `ac4f9bd9695f725a69f66fdcff09fdf227d4d02c7a212902d2ff68e3abb35822`。Node JSON.parse 结果以 binary64 原始位记录，Python float 的位结果逐条相同。包含负零、整数精度边界、溢出为无穷、次正规数、正规/次正规边界及精确 midpoint 的上下邻近十进制值；midpoint 用 Decimal 精度 2200 构造，随机种子 120064。

这是下一步原生转换的参考基线，不是原生兼容性通过。严格 JSON 数值语法、JSON.parse 的 binary64 转换、Number(string) 强制转换以及 TypeBox 有限数校验必须分开处理；例如合法 `1e400` 可解析成 Infinity，但工具 Number Schema 可拒绝它。当前不实现后两类转换，也没有数值序列化结论。

原生数值读取现已完成局部实现与位级验证，见 [T12 数值读取实验](t12-number-progress.md)：1315 条对照通过，转换为 binary64 位模式。前述 547 条为历史参考基线，不再代表当前完全没有转换实现；Schema、强制转换和数值序列化仍未实现。

## 结构化输出本地实验

[有界 writer](t12-writer-progress.md) 已验证显式调用顺序的对象/数组、UTF-8 字符串、布尔/null 和严格数字词法组装。只在完整根及容器闭合后发布结果，逐调用失败保持状态与字节不变。数字词法保留不等同于 JSON.stringify 的 binary64 格式化，对象调用顺序也不等同于 JS 属性枚举；这两类可观察差异仍须单独追踪，不能据此宣称 Pi wire/session 兼容。
