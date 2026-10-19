# T03/T04：公开表面盘点与迁移差异追踪

本记录验收的是固定参考版本的静态盘点、差异分类和任务关联。没有运行 Pi、没有构建原生 Agent，也不签发兼容性或控制面升级批准。

## T03 的输入与盘点结果

入口依据：Pi 的 `coding-agent`、`agent`、`ai` 三个 package manifest，原计划的显式入口，以及实际处理命令的 CLI/MCP/package-manager 源码。TypeScript 7.0.2 AST 只解析，不执行源码。

| 项目 | 结果 |
|---|---:|
| package exports/bin 声明（条件出口合并后） | 26 |
| 已追踪的相对重导出边 | 194 |
| 外部包重导出边 | 2，均为 TypeBox，转交 T07 |
| 源码文件 | 322 |
| AST 声明、字段、参数比较和选项表记录 | 5,393 |
| coding-agent 文档 | 42 |
| 文档表格、选项与相关内联值记录 | 718 |
| 可追踪表面记录合计 | 6,111 |
| CLI/子命令/别名的源码选项 token 候选 | 81 |
| SettingsSchema 根字段 | 59 |
| RpcCommand 带标签分支 | 33 |

这些数字是输入盘点数量，不能作为已通过测试数或兼容率。根入口的 named exports 可能只暴露目标文件的一部分，因此目录保留“公开或支持性声明”标签，不把所有 helper 都错误承诺成公开 API。JSON metadata 出口单独登记，不尝试当 TypeScript 解析。

每个源码记录保存路径、行号、声明类别、原始契约和稳定于本次固定参考的 ID。可选字段按 TypeScript 7 的 `postfixToken` 读取；联合类型的命令分支与输入字段分别保留。Schema 的原始校验表达式、默认值引用及 SETTINGS_DEFAULTS 对象保持原文，不用猜测值替代计算表达式。

每个文档记录保存所在页、行号及整行契约，全部 42 个文档的摘要同时记录。选项、配置键、RPC、工具参数、SDK 方法和扩展接口可以回到源码或文档定位。环境变量、斜杠命令、会话格式和资源规则也有对应文档记录；动态注册名称通过注册 API 及参数契约登记，不假设其值域有限。

这里的 ID 与固定提交绑定；升级参考时必须生成差异并保留旧证据，不能承诺源码偏移生成的证据 ID 在任意上游编辑后仍相同。长期功能 ID 与 Txx 任务 ID 不随源码偏移变化。

## 来源反向核对与 30 条发现差异

盘点器将源端 option/schema 字段与文档提示反向比较。所有发现保留在 catalogue，不能为了“零差异”删除原始记录。

| 类别 | 判定 | 后续验证 |
|---|---|---|
| TypeBox 两条重导出 | 外部 Schema/SDK 类型依赖 | T07 依赖审查，T12 Schema 子集，T39 SDK 迁移；尚未兼容 |
| `--models-dir`、`--ctx-size`、`-hf` 等 | llama.cpp 服务的参数 | LOCAL-01 / T47；不伪造为 Pi 的顶层选项 |
| `-write`、`-bash`、`-name` | 工具选择列表的值 | CLI-05 / CFG-02；T17/T24/T28 验证值语法及混用限制 |
| `-builtin`、`-path` | 资源排除语法的片段 | RESOURCE-04 / EXT-01；保留完整表达式，T31/T32/T40 验证 |
| `${1:-default}` 中的片段 | 模板默认参数语法 | RESOURCE-03 / T31；不当作 CLI flag |
| `ls -la` 中的 `-la` | Shell 子命令参数 | TOOL-05 / TUI-04；验证透传，不当作 Pi 参数 |
| `queueMode`、`websockets` | 旧配置输入 | Pi settings-manager 分别迁移为 steeringMode/transport，现代字段优先；T17/T37 保留迁移义务 |
| `$schema`、deviceId、trackingId、lastChangelogVersion | Schema 提示或持久化产品元数据 | CFG-02 / OPS-01；导入规则显式决定，不导入为盘古的遥测同意或后台网络许可 |

全部 30 条已经分类并分配后续任务；这表示来源已解释，不表示实现缺口已关闭。新出现的未知项不能套用上述分类自动通过：分类器遇到未识别差异会拒绝生成 T04 账本。

此前 REF-001 曾把 `cacheWarming`、`showCacheMissNotices` 误记为未被文档表格覆盖，本轮已经纠正。当前固定 Pi 文档明确包含这两个字段；实际 Schema 表面与“用户可配置表格”的差异为上表列出的元数据/旧键。未修改参考文档来消除差异。

## 默认值、优先级及行为要求

关键来源：`settings-schema.ts`、`settings-defaults.ts`、`settings-manager.ts`、`config.ts`、`main.ts`、`cli/args.ts`、`trust-manager.ts` 及对应文档。

- 设置按字段和具体服务合并，不能用一个全局优先级口号替代。普通嵌套对象与 defaultTools 列表的合并不同；CLI 的混合工具列表规则也不同于 settings。
- sessionDir 在信任决策前参与会话定位；项目可执行资源在信任之后加载。全局专属的 defaultProjectTrust、httpProxy、cacheWarming 不应变成普通项目覆盖。
- `SYSTEM.md`、`APPEND_SYSTEM.md` 按各自的项目覆盖规则处理；AGENTS.override 只覆盖同目录的候选文件。
- 用户路径、项目资源路径和 cwd 的相对基准分别保存；不要在 C 实现里统一相对于进程启动目录解析。
- 缓存预热在固定 Pi 的默认值为 streaming，并有成本和取消语义。原总账遗漏了此能力，本轮增加 LLM-07；P1 仍只承诺已明确的请求行为，不隐式增加预热请求。
- print、JSON、RPC 的退出/接受/settled 语义分开登记。print 的模型错误、abort 和 length 与 JSON 模式的状态处理不同；RPC prompt 成功响应仅表示接受或处理，不表示模型工作完成。
- 工具参数准备、Schema 校验、模型可见工具声明和执行授权分别处理。特别是 edit 的多项替换与截断模型参数，不能用“JSON 能解析”作为执行许可。

上述规则转为 T09/T17 及各服务的行为 fixtures 后才能验证。原始契约里未求值的引用不当作已解释的常量；需由对应场景确认。这是测试工作的输入，不是用静态 AST 证明运行语义。

## T04 的差异与范围分母

`surface-gap-ledger.json` 为 6,111 个表面记录分别关联 feature ID、候选 case ID、实现步骤、风险、责任角色和兼容策略。策略来自所属功能的 EXACT/SEMANTIC/INTENTIONAL_CHANGE/DEFERRED 登记；跨功能记录保留各自策略，不把它们压成一个模糊 PASS。

所有记录当前均为 OPEN_IMPLEMENTATION / NOT_RUN。这些是追踪条目和候选验证入口，不要求为每个 helper 生造一个单独测试文件；将来允许一个有意义的场景覆盖多个字段，但必须明确其断言覆盖范围。

P0～P3 当前共登记 103 条显式范围要求，涵盖必需功能、CLI 子集、设置、环境变量、模型协议、工具、会话与权限。每条都有独立 requirement ID 和待实现 case ID。验收分母包括选定组合及其祖先的全部要求，不能因某要求未实现就排除。

P3 真实使用方尚不存在；实际工作流的 OAuth、SDK 或 JS 扩展需求一旦出现就必须追加到必需范围，不能用当前自用结果替代其验收。P4 尚为未来组合，不能凭空范围或零用例签发通过。

T04 的完成表示缺口、风险、实现顺序、责任及验收分母可追踪；新增行为依旧需要独立审批，运行兼容性、原生实现和上线全部未完成。外部 TypeBox、旧配置迁移、SDK/扩展差异及性能预算继续在对应 Txx 作为阻断条件，未予豁免。

## 已执行验证与后续

AST 提取器 16 项测试、盘点/分类器 14 项测试通过。覆盖相对重导出与循环、遗漏/逃逸入口、包出口变化后的陈旧证据、可选参数、局部 named export、选项字典、别名表、源码不执行、语法错误、缺失来源、重复 ID、未知参数不能自动分类和未实现要求不能算 PASS。

本轮源码和文档摘要、工具摘要及生成物摘要见 `t03-t04-evidence.json`。G00 与原有 11 项门禁自测已通过；控制面独立审核尚未执行，不声称已建立服务器保护。

下一项 T05 建立实际架构/构建准入和受保护执行方式。在该项通过前不加入生产源码。T06 工具链、T07 原生依赖、T08 ABI 和 T09 对照执行仍未完成。
