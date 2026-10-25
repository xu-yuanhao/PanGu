# 盘古 Pangu

盘古是以**手写汇编为主、C 第二、C++ 第三**，参考移植 Pi Durable 和 Pi 原 Coding Agent、采用 deepseek-harness 式插件组合的最小原生 Agent。

长期目标是通过架构和平台适配，理论上运行在任何具备所需资源的设备上。当前先实现 Linux x86-64，不宣称已经支持所有设备。

## 参考什么

| 参考 | 用途 |
|---|---|
| [Pi Durable](references/pi/packages/durable/) | 主要行为基座：Agent 运行、提交、恢复、取消 |
| [Pi 原 Coding Agent](references/pi/packages/coding-agent/) | 辅助参考：编码工具和交互工作流 |
| [Pi 共享基础库](references/pi/packages/) | pi-ai、tui 等对应基础模块 |
| [deepseek-harness](references/deepseek-harness/) / [Cordis](references/cordis/) | 微内核式服务组合、依赖、资源所有权和生命周期 |

开发前阅读对应源码、直接调用和测试，优先采用已有成熟行为与库，避免重复造轮子。参考项目的语言不决定盘古的实现语言；需要写原生实现时，按汇编优先推进。定期检查 Pi/Harness 更新，按实际需要吸收修复。

## 怎么推进

- **保持最小**：实现当前需要的可用闭环，不合并参考项目的全部功能，不预建额外框架。
- **一切皆插件**：Agent Loop、Model、Tools、Session、Storage 和基础服务可通过明确接口替换；宿主只负责必要引导、组合与生命周期。允许静态组合，不把每个内部函数拆成插件。
- **汇编优先**：核心状态机、协议、工具和 TUI 纯逻辑优先手写汇编；C 用于必要平台或外部库胶水，C++ 用于确有需要的桥接。
- **原地替换**：完成重构即删除旧实现和失效构建项，不保留对照或备用版本；回溯用 Git。
- **验证相关行为**：检查实际变更涉及的行为、ABI、内存、资源释放和工具安全；真实模型验证真实闭环。门禁服务开发，不为未来认证阻挡当前实现，也不放宽检查掩盖失败。

详细行为在开发时查参考，不在本页提前规定实现。旧计划中的额外体系不自动成为需求；没有当前需求或参考依据的功能，不因曾写进文档就继续实现。

## 交互与进度

TUI 是主要入口，简单易上手；保留必要启动参数和自动化接口，不建设全功能 CLI。GUI 以后再考虑。

TUI 主要参考 Grok Build 的 UI/UX、Pi 的简洁性，并结合 Codex、Claude Code 的交互优点；只参考体验，仍用盘古自己的技术栈。具体见 [TUI 参考](docs/TUI体验参考与设计原则.md)。

当前已有手写汇编的 Agent/Session、MiMo 协议、事务日志、read/write/edit 和薄 TUI。实际能力、运行方法与限制见 [核心进展](docs/migration/runtime-core-progress.md)；下一步按 [迁移 TODO 的近期队列](docs/原生重写路线与迁移TODO.md#近期执行队列2026-10-24) 推进。开发约束见 [AGENTS](AGENTS.md) 和 [工程守卫](docs/工程门禁与架构守卫.md)，历史阶段清单不作为全部功能的串行前置。
