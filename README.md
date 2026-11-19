# PanGu

PanGu 是以 C 为主体、低资源占用、可嵌入、可通过插件组合的通用 AI Agent。

## 产品组成

- **PanGu Core**：可独立链接的最小通用 Agent 库。提供消息状态、模型—工具循环、流式事件、取消、输入队列和执行钩子；模型与工具由使用者注入，供其他产品二次开发。
- **PanGu Agent**：基于 Core、默认精简且开箱可用的通用 Agent 产品。用户体验参考 Pi Coding Agent，能力组合和替换方式参考 DeepSeek Harness。随产品提供的能力也由插件实现，用户可调整模型、工具、提示词和执行方式。
- **PanGu TUI**：Agent 的终端呈现层，提供可以直接上手的完整交互。同一 Core 和 Agent 库也供桌面、手机呈现层调用。

## 默认能力与插件组合

默认组合提供模型接入与切换、会话保存/重开与会话树、上下文压缩、输入编辑、工具执行、扩展管理、skills/提示模板及 MCP。文件与命令工具随产品提供，用户配置模型后即可使用，并可按用途选择工具。

模型提供者与认证、工具与执行环境、配置、会话存储、压缩、提示资源、命令、执行驱动和 TUI 接入均通过插件注册。计划模式、子 Agent 的组织方式和领域工作流由用户选择的插件组合表达。Durable 的检查点、恢复和任务调度作为可选择的持久执行插件交付，同一个 Agent 和 TUI 消费其能力。

构建和安装按组合选择模块；运行时按配置启用。默认组合的依赖闭包、可选插件的安装体积和启用后的资源占用分别测量。

## 实现方式

移植经过验证的参考行为，使用适合原生环境的数据结构和所有权；直接查阅参考实现、调用者与测试。PanGu 使用自己的身份、`.pangu` 配置和数据格式，提示词中的产品名、路径和入口适配 PanGu。

| 来源 | 采用内容 | 本地入口 |
|---|---|---|
| Pi agent-core | 最小循环、状态、事件、队列和工具执行语义 | `references/pi/packages/agent/` |
| Pi pi-ai | 模型协议、流式内容、用量、认证与模型能力 | `references/pi/packages/ai/` |
| Pi Coding Agent | 工具、会话操作、配置、提示资源、扩展和终端工作流 | `references/pi/packages/coding-agent/` |
| Pi Durable | 提交与可见性、存储重开、任务/调度、Generation/Tool、恢复、分支和压缩 | `references/pi/packages/durable/`，依赖语义查 `packages/chord/` |
| DeepSeek Harness / Cordis | 服务注入、作用域、可替换循环、插件组合、生命周期和卸载清理 | `references/deepseek-harness/docs/architecture.zh.md`、`references/cordis/packages/core/` |
| Pi TUI | 渲染、输入、组件和交互的主要行为 | `references/pi/packages/tui/`、`packages/coding-agent/src/modes/` |
| Grok Build / Codex / Claude Code | 简单易上手的布局、输入反馈、工具展示和操作体验 | `references/grok-build/`、`references/codex/codex-rs/tui/`、`references/Claude-Code/src/` |

Core 提供独立循环和可组合的执行能力。Agent 通过插件安装 Core 驱动；持久执行驱动移植 Durable 的阶段、检查点和调度，复用公共消息、模型、工具与事件能力。持久化及恢复属于 Agent。两种驱动共享能力接口，由会话组合选择。

插件宿主保持小型：负责加载、服务注入、作用域、事件与生命周期。Core 库由执行驱动插件调用，Core 的独立嵌入接口保持可用。插件边界按可替换能力与生命周期划分，内部函数按模块组织。原生插件采用版本化 C 接口，静态注册和动态加载采用相同的生命周期；插件分发适配目标平台/架构，扩展 API 面向 PanGu。

## 技术栈与性能

- 业务与公开接口采用 **C17**；C++ 用于实际采用的组件桥接，汇编用于有测量依据的局部优化。公开接口使用 C ABI。
- CMake + CTest 管理构建与测试，依赖固定版本及许可证。依赖按实际模块链接，Core 可单独构建和安装。
- JSON 优先采用 [yyjson](https://github.com/ibireme/yyjson)；HTTP/TLS 采用 [libcurl](https://curl.se/libcurl/)；事件、进程和文件平台能力采用 [libuv](https://docs.libuv.org/)；持久存储采用 [SQLite](https://www.sqlite.org/)。按实际消费者引入。Unicode、Schema、代码执行等能力同样复用成熟实现。
- 性能按完整依赖计算：分发/安装体积、启动时间、空闲与运行内存、空闲 CPU、流式处理和多会话开销。固定场景分别测 Core、Agent、TUI，与采用的 Pi 快照对照；记录工具、输入、平台、编译选项和实测结果。
- 消息与队列按实际数据增长；资源策略由宿主配置。使用事件驱动、增量处理、必要的背压及及时释放，依据分析结果优化热点。

## 多平台

共享 C 源码、C ABI 和同一构建系统，系统差异集中到有实际调用者的平台适配中。

| 目标 | 同步开发与验收内容 |
|---|---|
| Linux x86-64、Windows x86-64、macOS ARM64 | Core、Agent、TUI 的构建与行为测试；终端、路径、进程取消和插件加载实跑 |
| Android ARM64、iOS ARM64 | Core/Agent 的库构建、嵌入示例和宿主能力接入；使用对应 SDK 与运行环境验证 |

移动系统的文件、后台执行和进程权限由宿主提供；插件静态组合适配平台分发规则。平台能力可查询，缺失能力返回明确诊断。每个执行批次维护同一套源码和目标配置，结果区分编译、自动测试与人工交互验证。可移植性通过实际目标证明。

## 开发入口

先读 [AGENTS.md](AGENTS.md)，再领取一份任务：

1. [Core](docs/Core-TODO.md)
2. [Agent 运行与持久化](docs/Agent运行-TODO.md)
3. [Agent 模型、工具与插件服务](docs/Agent服务-TODO.md)
4. [TUI](docs/TUI-TODO.md)

各份清单共同定义交付范围。每项标明来源与验收，全部完成后交付可嵌入 Core、开箱可用的默认 Agent/TUI 组合及可选择的持久执行等插件。

## 参考快照

| 项目 | 初始采用提交 |
|---|---|
| Pi | `d5b3de22d0e24a3e9c38a9410d3329b099717283` |
| DeepSeek Harness | `22ddee3792402c3e70ec5f86a63c3e3e895a2f60` |
| Cordis | `df4431eb43086bd1f8263a0e8c03e9b7c5233b68` |
| Grok Build | `2bdd1d6a6369de0e8c68132ea4539e9abd9e14a8` |
| Codex | `8dd6099e265f14b80cb1b3fcf10c29a12bb6b23d` |
| Claude Code 参考源码 | `b78dd22a091b717c8938ab98c736bc04825a8ee8` |

批次沿采用快照实现，集成时检查上游必要修复并更新采用记录。来源署名和许可证随移植模块保留，第三方许可按实际文件核对。

## Core 嵌入接口交接

当前公共契约与可运行宿主示例见 [Core embedding](examples/core/README.md)。
运行层从 `loop.h` 驱动，服务层注入模型/工具/校验器，呈现层通过 `state.h`
订阅与排队；回调线程、借用数据和销毁屏障以公开头文件为准。
使用 `-DPANGU_BUILD_TUI=OFF -DPANGU_BUILD_PROXY_HTTP=OFF` 可独立构建 Core；
可选 HTTP 代理使用单独的 `PanGuProxyHTTP` 安装包，不进入基础 Core 链接闭包。

## Agent SDK 安装

在根构建中启用 `PANGU_BUILD_AGENT=ON`，构建后使用
`cmake --install build/product --prefix "$PWD/build/sdk"` 安装库、头文件及
`bin/pangu_agent_cli`。外部 C 项目以安装目录设置 `CMAKE_PREFIX_PATH`，调用
`find_package(PanGuAgent CONFIG REQUIRED)`，链接 `PanGu::agent_app`（宿主组合）
或 `PanGu::agent`（会话库）。实际调用示例与安装验收见
[独立 C 消费者](tests/integration/agent-consumer/main.c)。

该包发现 Core、Services、SQLite、libuv 和 utf8proc；Agent app 自动选入服务宿主。
关闭 `PANGU_BUILD_TUI` 后 SDK 与 CLI 的确定性模型保存重开仍可用。
当前 CLI 组合另链接模型、文件工具及 Schema 桥接，构建桥接需要 C++17；
外部 SDK 示例仅启用 C 语言。系统依赖需由目标环境提供，安装不复制系统动态库。

## TUI SDK 安装

启用 `PANGU_BUILD_TUI=ON` 的根构建安装后，外部 C 工程使用
`find_package(PanGuTUI CONFIG REQUIRED)` 并链接 `PanGu::tui`，获得终端、编辑器、
输入缓冲与聊天视图；其链接闭包为 libuv、utf8proc 和平台线程库。
需要会话呈现插件时同时启用 Agent，并使用
`find_package(PanGuTUIPresentation CONFIG REQUIRED)` 与 `PanGu::tui_plugin`。
两种实际调用见 [安装消费者](tests/integration/tui-consumer)，包含真实终端输入、
回复、退出、终端模式恢复和呈现插件的会话重开。当前这些运行验证在 Linux 完成。

## 当前可运行集成入口

Linux 开发组合可通过 `python3 tools/integration/check.py all --build-dir build/product` 配置并验证 Core、服务、Agent、TUI 和跨层场景。`PANGU_BUILD_SERVICES`、`PANGU_BUILD_AGENT` 是独立组合开关；Core 专项关闭它们和 TUI，保持基础依赖闭包。

确定性会话演示：`build/product/src/agent/pangu_agent_cli create build/product/demo.jsonl --demo "hello"`；重开用 `open build/product/demo.jsonl`，终端交互演示用 `open build/product/demo.jsonl --tui-demo`。这些入口明确使用确定性模型，不代表真模型产品验收。跨层 `integration_product` 则通过本地 HTTP 模型协议、实际文件工具和 JSONL/SQLite 完成结果回送、取消复用及保存重开。生产 Schema 能力使用 Valijson 的 C++17 实现与公开 C ABI；依赖版本/许可见 `deps/README.md`。

当前 CLI/TUI 真模型工具闭环已有各角色脱敏验证，会话树/压缩与持久队列已有确定性集成场景；CLI/app 的用户及显式受信项目配置、skill/模板和reload已有离线协议与保存重开验证；交互资源管理、MCP、Durable 和多平台仍按各 TODO 推进。确定性整机与真实模型结果分别记录，不能互相代替。

嵌入宿主可在运行或呈现前调用 `pg_agent_app_set_approval(app, PG_APP_APPROVAL_ASK)` 显式启用工具审批；`PG_APP_APPROVAL_NEVER` 禁止工具执行，默认策略为 disabled。审批经服务插件应答，默认 TUI 提供允许一次、拒绝与取消；会话保存 asked/decided 审计，重开后由宿主重新选择策略。外部 C 调用与两后端验证见 [安装审批消费者](tests/integration/agent-consumer/approval.c)。

CLI 用 `--approval ask` 或 `--approval never` 选择同一策略；`ask` 配合 `--tui` 使用呈现插件应答，没有应答者时工具返回审批 unavailable。安装后的实际 MCP 工具询问、禁止执行及保存重开见 [CLI 安装消费者](tests/integration/agent-consumer/cli.cmake)。

CLI 通过 `--mcp-config file` 加载显式 MCP 配置，可同时组合 stdio 与 Streamable HTTP 服务；HTTP 能力按 `PANGU_SERVICES_MCP_HTTP` 构建选项选择。配置格式、认证头和当前支持边界见 [Agent 示例说明](examples/agent/README.md)。安装 SDK 的 HTTP 工具调用、保存及无服务器重开见 [HTTP 应用消费者](tests/integration/agent-consumer/mcp-http.c)。

## 重装恢复

```sh
git clone https://github.com/xu-yuanhao/PanGu.git
cd PanGu
python3 tools/restore-references.py
```

参考仓库不随主仓库提交；真实来源、固定提交及 Grok `SOURCE_REV` 见
`tools/references.json`。恢复脚本不会覆盖已有不同提交或有修改的参考目录。
Bun 摘录按固定上游文件与 SHA-256 恢复，PR/commit API 元数据重新下载；
原 API 响应的字节格式和 PR 当前状态不属于固定源码。参考工作区未发现本地修改，
Bun 五份源码已与固定上游逐字节核对。

构建需要 C17、CMake、pkg-config；默认服务的 Schema 桥接另需 C++17。
系统开发依赖见 [依赖清单](deps/README.md)（curl、libuv、SQLite、utf8proc、
libgit2、libyaml；图片与 PCRE2 按组合选择）。回归工具需要 Python 3，
Pi 差分另需支持原生 TypeScript 的 Node.js/npm；运行
`python3 tools/integration/prepare_pi_diff.py`（完整入口另加 `--full-runtime`）
重建隔离依赖。Android 库编译另需 NDK，其他平台按 `cmake/` 工具链配置。

凭据不在 Git 中，从独立私密备份恢复 `.private/`，设置目录 700、文件 600，
按原说明注入进程环境。不要将私密备份或真实用户会话加入公开仓库。
`build/` 是可再生成目录；历史测量 JSON 中的构建路径仅记录当时环境，
清理后不保证仍存在。已有测量与各 TODO 验证只对应注明的源码点；
最新 presence/ABI4 跨层组合尚未集中验收，本次备份不推进功能或重跑整机。
