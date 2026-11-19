# Core：执行任务

你负责实现最小、可嵌入的 PanGu Core，然后承担整机集成。先读根文档与 AGENTS.md，按下列顺序完成；每组包含实现、调用者、验证和修复。以 Pi agent-core 为主要规格，使用 C17 和公开 C ABI。

**文件归属**：`include/pangu/core/`、`src/core/`、`tests/core/`、`examples/core/`，以及根构建、`cmake/`、`deps/`、`tests/integration/`、`tools/integration/`、根文档和本 TODO。

当前先成组收尾 C02/C04，完成一次 Core 验收；C01/C03 已验收。随后以 C05 默认产品接线为主，C06 在完整组合上验收平台与资源。

## 执行清单

- [x] **C01 公共契约与可嵌入构建。** 阅读 `references/pi/packages/agent/src/{types,agent,agent-loop,stream-fn}.ts`、`packages/ai/src/types.ts` 与直接用例。提供消息/内容、错误、用量、事件、模型流、工具、钩子及所有权的公开头文件；建立 CMake 独立库与嵌入示例。将接口交接给其他三路；同时给 Linux/Windows/macOS、Android/iOS 库目标提供工具链入口。验收：仅配置 Core 即可编译、链接、创建及释放实例，实际依赖清晰。
- [ ] **C02 完整模型—工具循环。** 成组移植 `agent-loop.ts` 与 `agent-loop.test.ts`：输入、上下文转换、流式响应、工具调用及参数验证、结果回送、继续、错误和取消；工具串行/并行、逐工具模式、preflight、before/after、terminate 和结果顺序一起完成。验收：相同脚本模型驱动 Pi 与 Core，比较消息、事件、工具副作用与结束状态；真实模型由服务接口接线验证。
  - 逐来源用例与具体剩余组合见 [C02 coverage audit](Core-C02-coverage.md)；36个声明含两个各两变体的参数化组，不能仅由现有总用例数判定完整覆盖。
- [x] **C03 状态与控制。** 对照 `agent.ts`、`agent.test.ts` 完成 prompt/continue/reset、system 与工具声明更新、自定义消息、状态快照、订阅/退订与等待结束屏障、steer/follow-up 及队列模式、prepareRequest/finishTurn。验收：正常、异常、取消及钩子期间排队行为与参考一致，实例和订阅能重复创建释放。
- [ ] **C04 公共接口覆盖与嵌入验收。** 核对 `src/index.ts` 的公开导出与 `test/` 场景；补齐独立工具调用、低层循环、可注入默认流、`proxy.ts` 的代理流适配。测试覆盖文本、思考、图片、工具及附加 JSON 内容；验证超长消息、多工具、内存失败与回调重入的实际风险。将各导出对应的实现/场景写在本项下，完成可复制的宿主注入示例和安装头文件。验收：Core 的构建、使用和依赖闭包独立，采用快照的公开行为均有对应验证。
  - 公开面、现有消费者与差异核对见 [C04 coverage](Core-C04-coverage.md)，按对应行更新。
- [ ] **C05 产品闭环集成。** 通过宿主的默认组合接入运行、服务和 TUI 插件，沿调用链完成输入→模型流→工具→结果→保存→重开；验证模型切换、会话树、压缩、MCP、扩展入口和可选择的工具组合；另行启用 Durable 插件验证恢复。产品入口由运行负责人维护，本角色负责根构建和集成测试。验收：真模型完成工具任务，TUI 可操作，持久化后的 PanGu 数据可重开；会话取消后可以继续使用。
- [ ] **C06 多平台与资源验收。** 汇总各角色结果，实跑 Linux x86-64、Windows x86-64、macOS ARM64 的构建、Core/Agent 专项及终端交互；使用 Android/iOS SDK 验证 ARM64 库、嵌入和宿主能力。对固定场景测分发及安装体积、启动、空闲/工作内存、空闲 CPU、多会话及流式处理，包含所需依赖，分别测默认组合与启用可选插件的成本；与固定 Pi 基准对照，定位并修复具体回退。验收：全体 TODO 的行为覆盖闭合、平台结果和依赖许可齐全，剩余差异得到实现与验证。

## 快速验证方式

C01 将 `pangu_check_core` 目标与 `core` 测试标签接入 `python3 tools/integration/check.py core`；其余目标随各组件接入，`all` 在整机集成点运行。编译与受影响测试在开发中使用，完整回归在模块接线和最终集成运行。确定性模型验证调度；DeepSeek 真模型验证协议、工具往返和取消。性能以相同输入、相同功能、完整依赖口径比较。

## 当前交接

- 已可用：独立 Core 库/安装消费者、模型—工具循环、串并行、钩子、事件、取消与持久产品接线。C01/C03 已验收；C02/C04 保持未勾选。
- 来源：README 固定 Pi 快照；`packages/agent/src/`、`packages/ai/src/types.ts` 与 validation/transcript/streaming-json。行为映射见 C02/C03/C04 coverage；接口所有权以 `include/pangu/core/` 为准。
- 当前完整组：C02 Schema 来源适配与 C04 消息 wire/保存重开。C02 剩余为 Schema 转换、可选 null、原因/路径诊断与实际工具消费者的对应场景；C04 剩余为消息 optional presence 的完整请求差分、动态 ABI 配套、JSONL/SQLite 重开和已安装消费者。相邻修改一起收尾，依照覆盖表审查实际缺口。
- 跨线接线：Services 已交动态 ABI4 校验与 MODULE 消费者；Agent 已交 presence 编码/读取、旧记录兼容和两后端专项。集成核对当前头/库/ABI 与安装消费者；原生适配的明确差异保留在接口及覆盖表。
- 验证：Core 记录 11月19日 ASan 14/14、C03验收；整机记录 59/59（对应当时源码）。Agent presence 后续 17/17、增强专项1/1；Services ABI配套专项5/5。上述结果对应各自源码点，最新 presence 跨层源码仍需集中验收。具体命令与结果查看现有构建日志和各线当前交接。
- 产品剩余：默认工具/配置/资源/扩展/MCP 的完整体验与 TUI 显示尚需各线闭合；可选 Durable R03/R04 尚缺。JSONL 全快照写入的长期增长、掉电/多写入者行为仍是运行侧缺口。
- 平台：Linux 已有实际运行；Android ARM64 静态/共享 Core+embed 编译成功，未执行；Windows/macOS/iOS 运行环境未验收。功能验收与平台缺口分别跟踪。
- 性能：现有测量见 `measurements/`。11月19日 Linux Release 主程序 strip 后约1.24MB，系统共享库约26.4MB另计；确定性TUI空闲RSS约9.5MB。完整Pi产品/活动插件/其他平台对照仍缺。新的性能工作由实际组合或回退触发。
- 下一交付：闭合上述 C02/C04 有限差异，运行受影响差分及一次跨层集成，更新验收；随后优先默认 Agent/TUI 完整工作流。C03在实际修改时运行受影响回归。
