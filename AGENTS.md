# 盘古开发入口

先读 `盘古 Pangu 项目完整文档.md` 和 `docs/工程门禁与架构守卫.md`，再读自己负责的 TODO：

- 核心 Agent：`docs/原生重写路线与迁移TODO.md`。
- TUI Agent：`docs/TUI移植TODO.md`。

## 共同实现规则

1. 目标是手写 Assembly 优先的 Pi Durable 原生 Agent TUI；Pi 原 Coding Agent 补充编码工作流，deepseek-harness/Cordis 提供插件组合和生命周期语义。
2. 语言顺序固定为 Assembly、C、C++。Loop、Session、任务/调度、工具状态机、缓冲和 TUI 纯逻辑使用手写汇编；C 承担必要平台/外部库胶水；C++ 用于有实际桥接需求的模块。类型布局、函数表和所有权在公开头文件中说明。
3. 每批先读参考函数、调用者和相关测试，按 TODO 的来源映射移植。采用语义、原生表达和行为用例应一一对应。源码阅读记录写在自己 TODO 的交接区，用具体提交和函数定位。
4. 原地维护一个实现；替换完成时一并删除被替代源码、构建项和登记，历史通过 Git 查询。注释解释必要的 ABI、状态和所有权。
5. 复用成熟库和已验证实现。每批围绕当前步骤的完整行为推进，专项检查与该行为的风险相匹配。
6. `references/` 为只读参考。实现批次前检查 Pi/Harness 的上游差异；工作树干净时可快进更新。将旧/新提交和采用的修复记入对应 TODO；本批对照使用固定提交。
7. 文件归属按下表执行；每个 Agent 只编辑自己归属的文件。接口与登记需求写进自己的 TODO 交接区，核心 Agent 在每次集成时处理。
8. 新文件登记到 `governance/bootstrap-policy.json`，产品源同时更新实际模块、依赖和构建输入。运行 `python3 -B tools/gates/check.py`；改检查器时运行其自测。
9. 保持参考用例和现有预算的真实性；结果按实际运行记录。控制面变更可由同一 Agent 自审，记录差异、固定摘要、验证和结论。
10. 每个步骤完成后勾选自己的 TODO，并写实现路径、验证命令/结果和仍待处理的该步骤子项。核心 Agent 负责整机集成与经核对文件的提交。

## 并行文件归属

| 归属 | 可写文件 |
|---|---|
| 核心 Agent | `src/runtime/`、`src/contracts/`、`src/composition/`、`src/client/tui/runtime_bridge*`、`src/client/tui/tui_runtime_adapter.h`；其余原生基础/宿主源码；`tests/runtime/`、`tools/runtime/`、`tools/migration/`、`tools/gates/`、`governance/`；主文档、本文件、工程守卫、核心 TODO、当前实现记录 |
| TUI Agent | `src/client/tui/` 中上行接口/桥接之外的文件；`tests/client/tui/`、`tools/client/tui/`；`docs/TUI移植TODO.md` |

`src/contracts/client.h` 和 `tui_runtime_adapter.h` 是两条线的接口入口，核心维护、TUI 消费。共享树中保留他人的未提交工作；暂存和提交使用明确文件列表。
