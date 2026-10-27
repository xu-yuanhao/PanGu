# 盘古开发入口

首次接手阅读 `盘古 Pangu 项目完整文档.md`、`docs/工程门禁与架构守卫.md` 和自己负责的 TODO；接续工作读取当前批次、实际接口和相关参考源码：

- 核心 Agent：`docs/原生重写路线与迁移TODO.md`。
- TUI Agent：`docs/TUI移植TODO.md`。

## 共同实现规则

1. 目标是手写 Assembly 优先的 Pi Durable 原生 Agent TUI；Pi 原 Coding Agent 补充编码工作流，deepseek-harness/Cordis 提供插件组合和生命周期语义。
2. 语言顺序固定为 Assembly、C、C++。Loop、Session、任务/调度、工具状态机、缓冲和 TUI 纯逻辑使用手写汇编；C 承担必要平台/外部库胶水；C++ 用于有实际桥接需求的模块。类型布局、函数表和所有权在公开头文件中说明。
3. 按核心 TODO 的执行批次和 TUI TODO 的完整交互组推进。一次阅读上游模块、直接调用者与相关测试，成组移植真实函数；参考实现是行为规格。复用已有读取结果，记录提交、模块和关键入口。
4. 原地维护一个实现；替换完成时一并删除被替代源码、构建项和登记，历史通过 Git 查询。注释解释必要的 ABI、状态和所有权。
5. 复用成熟库和已验证实现。批次的交付物是接入实际调用链的模块或用户行为；基础能力随实际消费者补齐。批次内连续完成实现、接线和修复，以可运行结果交接。
6. `references/` 为只读参考。每个执行批次开始时核对 Pi/Harness 上游差异并固定采用提交；工作树干净时可快进更新。批次内沿固定快照完成，下一批统一评估新增修复。
7. 文件归属按下表执行；每个 Agent 只编辑自己归属的文件。接口与登记需求写进自己的 TODO 交接区，核心 Agent 在每次集成时处理。
8. 新文件登记到 `governance/bootstrap-policy.json`，产品源同时更新实际模块、依赖和构建输入。登记变化时运行 G00，语言职责/产品源变化时运行 Assembly 检查；检查器修改时运行对应自测。验证层级按工程守卫执行。
9. 保持参考用例和现有预算的真实性；结果按实际运行记录。控制变更由实施 Agent 自审，记录实际差异、受影响检查与结论；Git diff/提交标识固定内容。
10. 步骤满足验收后勾选 TODO。交接区覆盖更新当前批次：来源、实现入口、验证结果、具体缺口和下一交付物；历史查 Git。核心 Agent 负责整机集成与经核对文件的提交。

验证时机与交接格式统一按本文件和工程守卫执行；两份 TODO 提供功能范围及来源。当前核心执行批次为 B01；TUI继续既有交互组，公共接口交接按下表处理。

## 并行文件归属

| 归属 | 可写文件 |
|---|---|
| 核心 Agent | `src/runtime/`、`src/contracts/`、`src/composition/`、`src/client/tui/runtime_bridge*`、`src/client/tui/tui_runtime_adapter.h`；其余原生基础/宿主源码；`tests/runtime/`、`tools/runtime/`、`tools/migration/`、`tools/gates/`、`governance/`；主文档、本文件、工程守卫、核心 TODO、当前实现记录 |
| TUI Agent | `src/client/tui/` 中上行接口/桥接之外的文件；`tests/client/tui/`、`tools/client/tui/`；`docs/TUI移植TODO.md` |

`src/contracts/client.h` 和 `tui_runtime_adapter.h` 是两条线的接口入口，核心维护、TUI 消费。共享树中保留他人的未提交工作；暂存和提交使用明确文件列表。
