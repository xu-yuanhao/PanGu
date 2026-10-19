# 盘古 Pangu

> Assembly-first, ultra-small, fully plugin-driven AI Agent Runtime.

盘古是一个以汇编语言为核心、追求极致性能、极小体积、极快启动、完全插件化和跨设备运行的通用 AI Agent Runtime。

它不是一个单纯的 CLI，也不是一个模型 API 封装器。

盘古以 Pi 的最小 Agent 行为为基线，使用原生语言重写，并把功能实现拆成可组合、可替换的插件：

- 核心极小
- 运行极快
- 一切皆插件
- 所有核心能力可替换、可组合、可配置
- UI 与 Runtime 完全分离
- 同一套 Agent Runtime 运行于不同设备和操作系统
- 能力上可以承载 Claude Code、Codex 级别的 Coding Agent
- 同时可以扩展到移动端、服务器、机器人、嵌入式设备和未来未知设备

核心理念：

```text
Small Core
Infinite Capability
```

---

# 1. 项目定位与参考基线

盘古是一个使用 Assembly、C 和最少量 C++ 实现的原生 AI Agent Runtime。

首个产品目标：**以 Pi 为最小 Agent 的功能与行为基线，将其重写为一切皆插件、服务可替换的原生实现。**

## 1.1 三类参考各司其职

| 参考 | 采用的内容 | 不直接继承的内容 |
|---|---|---|
| Pi | 最小 Agent 的能力范围、循环行为、工具调用、流式输出、上下文、会话、取消和扩展体验 | TypeScript 实现结构、固定 Agent 宿主边界、Node.js 依赖 |
| deepseek-harness | Agent Loop、Session、工具注册表等自身作为插件；服务定义、提供者、消费者分离；配置组合应用 | 全量产品功能、全部包划分和运行环境 |
| Cordis | 服务依赖、作用域、组合、资源所有权、异步生命周期与有序清理语义 | JS Proxy、动态对象、Promise API 及 npm 插件兼容性 |
| VS Code | 插件清单、声明式贡献、按需激活、扩展宿主与客户端 API 边界 | 整套编辑器架构和 Extension Host 实现 |

Pi 是最小 Agent 的设计基线；“最小”由盘古自身的构建与运行测量验证，不以项目声誉代替量化结果。插件化的目标是在 deepseek-harness 的服务组合基础上，进一步允许替换内存分配、解析、事件后端、传输等底层实现。

## 1.2 移植范围

行为复现以固定提交的 Pi 为依据。开始实现前记录参考提交、选定模块、输入输出、事件顺序、取消、错误恢复和会话语义，建立差异清单和验收样例。

不逐句翻译源码，不要求目录、类和语言技巧一比一对应。Pi 的固定流程需要拆成服务接口和默认插件实现；有意差异必须记录原因和行为影响。未完成范围审查前，不宣称完整兼容 Pi。

不要求直接运行 Pi 或 Cordis 的现有 JS/TS 插件。此类兼容由未来可选桥接宿主提供，其运行环境与体积单独计算。

本轮设计参考入口：[Pi Agent Loop](references/pi/packages/agent/src/agent-loop.ts)、[Pi 扩展机制](references/pi/packages/coding-agent/docs/extensions.md)、[Harness 架构](references/deepseek-harness/docs/architecture.md)、[Harness vendored 修改](references/deepseek-harness/vendor/README.md)、[Cordis 生命周期](references/cordis/packages/core/src/fiber.ts)、[VS Code 扩展宿主](references/vscode/src/vs/workbench/api/common/extHostExtensionService.ts)。T01～T04 已固定参考提交、迁移范围与静态清单，见 [参考基线](docs/migration/reference-baseline.json) 和 [迁移 TODO](docs/原生重写路线与迁移TODO.md)；固定与盘点不等于运行兼容性已经通过。

## 1.3 产品边界

首个可用产品围绕最小 Coding Agent 闭环构建。多端、机器人、嵌入式和本地推理保留为架构扩展方向，不进入首版必需依赖。

项目不绑定模型、UI、具体操作系统或单一 Agent 实现。

---

# 2. 核心目标

## 2.1 极致性能

核心及真正高频的 Runtime 路径优先使用汇编实现；具体优化由测量决定。以下模块可以位于默认插件中，使用汇编不改变其可替换边界。

包括：

```text
Memory
Allocator
String
Buffer
Parser
JSON
Hash
Search
Agent State Machine
Tool Dispatch
Event Loop
IPC
Protocol
Plugin Dispatch
SIMD
部分数学运算
部分推理算子
```

盘古自身不应该成为 Agent 的明显性能瓶颈。

目标是：

```text
用户输入
    ↓
本地 Runtime 处理
    ↓
几乎立即完成
    ↓
等待模型 / 网络 / 外部工具
```

本地框架开销必须尽可能接近不可感知。

---

# 3. 极小体积

盘古的体积设计遵循：

```text
Core 极小
能力外置
UI 独立
模型独立
```

以下为待验证的目标量级，尚无实现或实测结果，不构成已达成指标：

```text
Assembly Core
≈ 100 KB ～ 1 MB

完整 Runtime
≈ 1 ～ 5 MB

完整 TUI Coding Agent
≈ 5 ～ 20 MB

GUI 安装体积
≈ 50 ～ 150 MB
```

其中 GUI 的主要体积来自：

```text
Qt Runtime
字体
图形资源
平台组件
```

而不是 Agent Core。

模型权重不计入 Runtime 体积，但发布包必须分别报告权重和推理后端。

测量对象区分：最小 Bootstrap/Host、可调用模型的最小 Agent、TUI Coding Agent，以及完整 GUI 产品。最小 Agent 必须包含其实际使用的 TLS、解析器、模型适配、工具与服务依赖，不能只报告宿主文件。

每次结果注明平台/架构、编译模式、静态/动态链接、启用插件、压缩前后体积、安装后体积，以及系统共享库是否另计。冷启动明确计时起止点、缓存状态和依赖准备情况；空闲与峰值内存按组合测量。插件化和汇编本身不保证体积或启动目标。

---

# 4. 语言组成

整个项目遵循：

```text
Assembly > C >>> C++
```

其他语言仅用于对应生态的 UI 或宿主环境。

完整语言结构：

```text
Assembly
├── Agent Core
├── Runtime
├── Parser
├── Protocol
├── JSON
├── Memory
├── Algorithms
├── SIMD
├── 部分平台适配
└── 部分推理算子

C
├── Platform Abstraction
├── TLS 接口
├── 系统 API 封装
├── Socket
├── Dynamic Library
└── Stable ABI

C++
└── Qt Bridge

QML
└── Desktop GUI

TypeScript
├── Web UI
└── VS Code Extension

Kotlin
└── Android UI
```

盘古不追求“100% Assembly”。

盘古追求：

> Assembly-first。

Assembly 决定项目核心身份。

---

# 5. 总体架构与可替换边界

```text
Clients：TUI / Desktop / Web / VS Code / Android / API
                         │
          客户端协议与会话访问接口
                         │
          按配置组装的原生服务插件图
  ┌───────────────────────────────────────────┐
  │ Agent Loop / Session / Tools / Context     │
  │ Model / Prompt / Scheduler / Permission    │
  │ Filesystem / Shell / Storage / Transport   │
  │ Allocator / JSON / Event Backend / PAL     │
  └───────────────────────────────────────────┘
                         │
    同进程 C ABI；跨进程与网络使用版本化协议
                         │
              最小 Bootstrap / Plugin Host
   引导、接口解析、插件归属、生命周期与清理规则
                         │
              架构及平台启动适配
```

图中的服务是依赖图，不是必须层层穿过的固定调用栈。服务可直接调用其显式声明的依赖；模型工具只是服务的消费者之一。

## 5.1 固定规则与可替换实现

“一切皆插件”指所有可插拔功能实现，包括 Agent Loop、Session、工具注册、权限策略以及可选的底层基础服务。默认插件可以用 Assembly 编写；编写语言不决定它是否属于固定宿主。

最小宿主只保留建立插件系统所需的引导机制，以及接口校验、资源归属、生命周期执行等不可绕过的规则。不能要求插件加载器先加载自己才能运行。

Bootstrap 包含装载初始插件所需的最少分配、平台和调用支持。完整 Allocator、JSON、事件后端、PAL 等实现通过版本化接口替换；首次启动仍须有一个可用实现。

替换分为构建时选择、启动时选择和运行中切换。底层分配器、PAL、事件循环等首版允许仅在构建或启动时选择，不承诺运行中热替换。仍存活的内存必须由原分配器释放；不得切换后交给新实现。

## 5.2 Runtime 的含义

本文 Runtime 指宿主与已组装服务的整体。Session Runtime、Agent Runtime、Tool Runtime 等是服务接口及默认插件集合，不代表固化在宿主中的业务模块。

客户端只依赖公开服务或协议。会话的权威状态属于 Runtime 中的 Session 服务，不属于 UI；服务可替换不等于允许多个实现同时无协调地写同一会话。

---

# 6. Assembly Core 与原生基础服务

Assembly Core 是原生基础实现集合，不是把所有基础能力永久固化的单体。

固定宿主只理解插件实例、服务标识、接口版本、作用域、资源归属和生命周期。Agent、Tool、Session、Model 等领域对象属于对应服务接口。

Memory、String、Buffer、Collections、JSON、Event、IPC、Protocol 等提供精简默认实现。按需以库、静态注册插件或动态插件交付；链接方式不改变服务接口与生命周期要求。没有实际替换需求的内部辅助函数不强制增加动态分发。

Core 不包含具体模型厂商、Coding、Git、Browser、Qt 或 React。业务服务不因使用汇编实现而进入固定宿主。

---

# 7. Memory System

盘古拥有自己的基础内存系统。

包括：

```text
Allocator
Arena
Pool
Buffer
Ring Buffer
Stack
Memory Map
Object Lifetime
```

设计目标：

```text
低分配开销
低碎片
明确生命周期
低峰值内存
可预测行为
```

---

# 8. 数据结构

Core 提供最必要的数据结构：

```text
Array
Vector
String
StringView
HashMap
HashSet
Queue
Ring Buffer
Linked List
Tree
Heap
Bitset
Arena
```

原则：

> 只实现 Runtime 真正需要的基础结构。

不构建庞大的通用标准库。

---

# 9. 字符串系统

默认内部编码：

```text
UTF-8
```

提供：

```text
String
StringView
StringBuilder
Search
Replace
Split
Join
Compare
UTF-8 Decode
UTF-8 Validate
ASCII Fast Path
```

字符串系统大量使用：

```text
SIMD
Zero-copy
View
Arena
```

减少复制。

---

# 10. JSON Runtime

JSON 是盘古的基础协议格式之一。

支持：

```text
Parse
Serialize
Streaming Parse
Incremental Parse
Zero-copy Parse
Token Iterator
```

用于：

```text
LLM API
Tool Calling
MCP
IPC
Plugin Metadata
Config
Remote Protocol
```

高频路径使用 SIMD 加速。

---

# 11. Agent Runtime

默认 Agent Runtime 以 Pi 为行为基线，由 Agent Loop 等服务插件负责完整 Agent 生命周期。循环实现可替换，宿主不内置固定 Agent 算法。

基本循环：

```text
Input
 ↓
Context Build
 ↓
Model Request
 ↓
Streaming Response
 ↓
Tool Call
 ↓
Tool Execution
 ↓
Tool Result
 ↓
Model Continue
 ↓
Finish
```

模型、工具和策略全部通过插件注入。

Agent Runtime 本身不绑定任何特定模型。

---

# 12. Agent State Machine

公开 Agent 接口提供统一状态投影；默认循环将内部状态映射为以下状态。替代循环遵守公开状态语义，不必采用相同内部状态机：

```text
Idle
Preparing
CallingModel
Streaming
WaitingTool
RunningTool
WaitingInput
Paused
Cancelled
Completed
Failed
```

所有 UI 通过事件读取状态。

UI 不直接控制 Agent 内部实现。

---

# 13. Tool Runtime

Tool Runtime 是可替换的工具注册与执行服务插件，管理工具生命周期。业务工具通过服务注册，不能绕过必需的授权和执行校验。

每个 Tool 至少包含：

```text
ID
Name
Description
Input Schema
Output Schema
Capability Requirements
Permission Requirements
Execute
Cancel
Timeout
```

Tool Runtime 负责：

```text
Register
Discover
Dispatch
Validate
Execute
Cancel
Timeout
Return Result
```

具体 Tool 全部由插件提供。

---

# 14. Capability 与信任边界

盘古区分三种机制：

| 机制 | 职责 | 不能替代的机制 |
|---|---|---|
| 服务作用域 | 决定消费者经宿主可解析哪些服务实例 | OS 安全隔离 |
| Capability 授权 | 对经受控接口发起的资源操作授予或拒绝权限 | 对同进程恶意机器码的约束 |
| OS 沙箱 | 强制限制进程的文件、网络、进程及资源访问 | 应用层的会话授权与审计 |

Capability 示例包括 `filesystem.read`、`filesystem.write`、`network.http`、`process.spawn`、`terminal.execute`、`clipboard.read` 等。

授权必须结合调用者身份、Session、资源范围和操作参数；服务存在不代表调用被授权。插件清单声明权限需求，受控执行路径检查实际授权。路径、网络目标等范围由资源服务规范化与检查，不能只比较未经解析的字符串。

同进程 Native 插件属于可信代码，通常具有宿主进程的 OS 权限，并可绕过宿主接口访问系统或内存。Capability 不能把普通动态库变成安全沙箱。

不可信代码通过受限进程或其他受支持的隔离后端运行。独立进程本身不构成权限隔离；必须实际配置 OS 限制。远程服务还需要认证、请求授权和传输保护。

权限策略可替换，但受控调用的检查位置和缺失必需策略时拒绝执行的规则不能被配置静默绕过。显式的全权限配置仍需进入同一检查路径。

---

# 15. Plugin Runtime 与服务组合

盘古参考 deepseek-harness/Cordis 的组合与生命周期语义，实现适合原生环境的插件宿主，不将现成 Cordis 或 Node.js 作为必需依赖。

## 15.1 插件提供服务

插件可以提供或消费多个服务。模型、文件、Shell、存储、上下文、Agent Loop、Session、Tools、Scheduler、Permission Policy、Transport，以及底层基础服务均可提供替换实现。

每个服务拆分为：

- Service Definition：服务 ID、版本、方法、数据结构及行为要求。
- Service Provider：实现该接口的插件实例。
- Consumer：声明依赖并使用服务的插件；模型工具是消费者的一种。

例如文件服务可同时供文件工具、Shell 环境适配和 LSP 使用，不要求内部服务协作走模型工具分发。

## 15.2 清单与依赖

插件清单在执行插件代码前可读取，至少声明插件 ID、版本、入口、宿主 ABI 范围、提供与依赖的服务及版本范围、配置 Schema、激活条件、支持平台与架构，以及请求的权限。

必需依赖未满足时保持 Pending 并报告原因；可选依赖须有明确缺失行为。循环必需依赖、版本冲突和重复 ID 必须在激活前诊断，不能仅靠加载顺序偶然解决。

单提供者服务在每个作用域只允许一个有效实现；多提供者注册表必须在接口中明确声明及定义选择顺序。禁止无声明的“后加载覆盖”。

插件实例、插件模块和服务实例分别标识：同一模块可以在不同作用域创建多个实例。

## 15.3 作用域与配置组合

作用域至少支持应用、Session/Agent 和调用级资源归属。子作用域按声明继承服务，并可显式绑定不同提供者；该可见性隔离不是 OS 沙箱。

配置描述插件实例与服务绑定。最小 Agent、TUI Coding Agent 等以可复用 Profile/Bundle 组装，禁用某个能力应移除其非必需依赖。

首版支持启动时组合和校验。运行中配置变更须声明是否重启实例、延迟生效或拒绝；不承诺整个插件图更新具有事务性。失败必须报告实际生效状态，不得伪装已全部回滚。

## 15.4 生命周期与资源所有权

```text
Discovered → Pending → Loading → Active → Quiescing → Disposed
                         │                     │
                         └── Failed → Cleanup ─┘
```

插件注册的服务、事件监听器、工具、定时器、子插件和异步任务必须有所有者及可追踪的释放操作。初始化失败撤销已创建的注册，并等待必要清理；取消与释放应可重复调用而不重复释放资源。

停止时先关闭新调用入口，再按声明等待或取消在途调用。消费者先于其提供者完成清理；提供者资源在消费者退出前保持可用于清理。单个清理失败不能阻止其他资源清理，最终错误须可观察。

依赖消失时停止接收新工作并使消费者进入停用流程。首版不要求依赖恢复后自动激活；启动重组或显式重启行为必须可预测。

所有权只能撤销受宿主管理的注册与资源，不声称能自动撤销插件已经写入的文件或外部副作用。

## 15.5 替换与卸载

服务替换默认通过停止受影响实例、完成清理、绑定新实现和重新启动完成。活动 Session 是否继续使用旧实现、迁移或要求重启，由对应服务定义，不能假设状态自动转移。

原生动态库释放前，必须证明没有在途调用、回调、线程或仍引用其代码和数据的资源。首版允许停止实例后保留模块映射至进程退出，不把 `stop()` 等同于 `dlclose()`。

参考版本需固定，并同时审查 deepseek-harness 的 `vendor/` 生命周期修正。参考机制以原生测试验证，不宣称完整移植 Cordis。

---

# 16. 插件角色与执行位置

功能角色与执行位置是两个独立维度。同一个 Model、Storage 或 Tool 服务可以由同进程、子进程或远程提供者实现。

| 执行位置 | 调用边界 | 适用情况 |
|---|---|---|
| Native | 同进程 C ABI；静态注册或 `.so` / `.dll` / `.dylib` | 可信原生能力、低开销基础服务 |
| Process | 版本化 IPC 协议及宿主代理 | 不同语言、故障隔离、配合沙箱运行不可信插件 |
| Remote | 版本化网络协议及宿主代理 | 远程模型、工具和资源 |

三者复用服务定义、身份、权限和生命周期概念，不要求调用成本、可用性与故障语义完全一致。仅声明可远程化的接口才可由代理提供；不得跨边界传递函数指针或进程内地址。

跨边界接口须定义请求 ID、结构化错误、截止时间、取消、流控、连接失效和资源释放。取消请求不代表远端副作用已撤销；重试必须依据幂等规则，不能自动重放任意写操作。

首版先打通可信 Native 插件与本地最小 Agent；Process 和 Remote 使用同一服务设计逐步增加，不为首版强制加载网络服务器或额外语言宿主。

---

# 17. 原生 C ABI 与协议版本

Native 插件使用 C ABI，不暴露 C++ ABI、STL 类型、语言异常或跨边界的隐式对象生命周期。

以下是设计要求，具体头文件需经 ABI 设计评审后确定；此处不冻结函数签名。

- 单一查询入口返回带 `abi_version`、`struct_size` 的插件描述和函数表。
- 宿主 API 通过版本化函数表注入；服务使用稳定 ID、接口版本和显式句柄解析。
- 所有结构体定义字段宽度、对齐、可选字段与扩展规则；ABI 兼容限定于明确的平台、架构和调用约定。
- 字符串与 Buffer 明确长度、编码、借用或转移关系；谁分配谁释放，或显式指定共享分配器。
- 服务引用绑定拥有者和有效期。停用后拒绝新调用，句柄失效可检测，不把裸指针当作跨生命周期引用。
- 同步返回与异步操作分开定义；异步完成、取消竞争、回调执行线程、重入和最终资源释放必须有明确规则。
- 错误以稳定错误码和可选诊断数据返回；不得让异常穿越 ABI。

宿主引导 ABI、服务接口版本、跨进程协议版本和 Session 持久化格式版本分别管理。使用 C ABI 不等于接口自动稳定；早期允许显式破坏性变更并更新消费者，稳定承诺须在验证后发布。

Process / Remote 插件使用可序列化的版本化协议，通过代理映射服务接口，不直接使用本地 C ABI。

Assembly、C、C++ 以及其他能满足 C ABI 的语言均可编写原生插件。加载其他语言插件所需的额外运行环境必须计入对应组合的体积和内存。

---

# 18. 配置系统

盘古几乎所有行为都可以配置。

包括：

```text
Models
Plugins
Tools
Permissions
Prompt
Context
Memory
Agent Strategy
Scheduler
UI
Network
Cache
Logging
Performance
Secrets
```

原则：

> 通过配置选择插件、服务绑定和策略，不修改宿主。新增算法仍须实现对应插件；配置不能替代尚不存在的能力，也不能跳过必需校验。

---

# 19. Model Provider

所有模型都是 Provider Plugin。

例如：

```text
OpenAI
Anthropic
Gemini
DeepSeek
GLM
Ollama
Local Inference
Custom Endpoint
```

统一接口：

```text
Create Request
Stream
Tool Call
Cancel
Embedding
Vision
Audio
Model Metadata
```

Agent Runtime 不需要知道模型厂商。

---

# 20. Context Engine

Context Engine 统一管理：

```text
Conversation
Files
Code
Memory
Tool Results
System Instructions
Skills
Prompt Components
Token Budget
Compression
Summarization
```

Context Strategy 本身也是插件。

---

# 21. Prompt Compiler

Prompt 不依赖一个巨大的固定模板。

默认 Prompt 组装先参考 Pi 的实际行为，保持简单、可测试。更复杂的算法驱动 Prompt Compiler 作为可选替代插件：

```text
Request
 ↓
Context
 ↓
Capabilities
 ↓
Rules
 ↓
Relevant Prompt Components
 ↓
Final Prompt
```

目标：

```text
低 Token
低重复
可计算
可测试
可配置
可替换
```

Prompt Compiler 作为插件运行。

---

# 22. Coding Agent 能力

完整 Coding Agent 可以具备：

```text
Read
Write
Patch
Diff
Search
Grep
Glob
Shell
Git
LSP
Diagnostics
Build
Test
Browser
MCP
Subagents
Planning
Memory
Skills
Permissions
Project Index
Symbol Search
Code Navigation
```

这些能力全部建立在盘古 Runtime 和插件系统之上。

盘古核心本身不包含“Coding”概念。

---

# 23. Subagent Runtime

盘古支持多个 Agent 实例同时工作。

例如：

```text
Main Agent
├── Research
├── Coding
├── Test
├── Review
└── Browser
```

Runtime 支持：

```text
Spawn
Message
Cancel
Join
Priority
Dependency
Resource Limit
Timeout
```

Subagent 仍然运行在统一 Runtime 中。

---

# 24. 事件、执行钩子与持久化事实

事件分发本身由稳定接口和可替换后端实现。事件定义必须注明作用域、数据版本、执行顺序、错误处理和是否允许重入。

| 类别 | 语义 | 示例 |
|---|---|---|
| 通知 | 观察事实，不通过返回值改变已发生操作 | 模型开始、工具完成、插件状态变化 |
| 变换链 | 按确定顺序传递变换结果，等待完成后继续 | 上下文构造、模型请求准备 |
| 门控/中间件 | 可委托、改写或拒绝操作；必需检查失败时拒绝 | 工具授权、执行前校验 |
| 持久化事实 | 按会话顺序提交，用于恢复与回放 | 用户消息、最终模型消息、工具调用与结果 |

通知监听器异常的隔离或传播规则由事件接口声明；不能让普通观察者意外决定工具执行是否成功。流式通知不能阻塞在无界慢消费者上。

模型调用前须完成必需的上下文和权限钩子；`permission.requested` 通知不等于权限已授予。中间件顺序、短路规则、重复委托和超时行为必须可测试。

瞬时流片段与已提交 Session 事实分离。默认 Session 实现定义消息何时提交、顺序号、断线重连游标及中断恢复规则；不默认把每个 token 同步写盘。

流通道定义背压、缓冲上限和慢客户端处置。允许合并的 UI 更新不能造成最终消息或工具结果丢失。监听器注册归属插件实例，卸载时撤销，并明确当前分发快照是否仍可完成。

---

# 25. Session Runtime

Session 服务以插件提供，独立于 UI；默认格式与行为参考选定的 Pi 基线。存储后端可替换，领域状态规则与物理存储接口分离。

Session 接口按职责拆分；以下是可组合的信息类别，Tasks、Memory 等可选功能不作为最小 Session 的强制字段或依赖：

```text
Session ID
Messages
Context
Tasks
Tools
Agent State
Memory
Streams
Events
Permissions
```

因此同一个 Session 可以通过公开接口被：

```text
TUI
GUI
Web
VS Code
Android
```

同时访问。默认每个 Session 只有一个权威写入者；多个客户端的命令由服务排序处理。替换 Session 实现须明确格式兼容、迁移和在途任务处理，不自动承诺跨实现恢复。

---

# 26. 多端同步

因为 Session 属于 Runtime，而不属于 UI，所以可以做到：

```text
电脑 TUI 创建任务
        ↓
Android 查看进度
        ↓
Web 查看输出
        ↓
VS Code 接管代码编辑
        ↓
Desktop GUI 继续对话
```

Agent 不会因为 UI 切换而重新启动。

---

# 27. Daemon

盘古支持独立 Runtime Daemon；最小 CLI 可在同一进程内组装 Runtime，不强制启动后台进程或服务器。

命令：

```text
pangu
```

用于 CLI / TUI。

后台 Runtime：

```text
pangud
```

负责：

```text
Sessions
Agent Runtime
Plugin Runtime
IPC
Network
Authentication
Remote Clients
```

---

# 28. IPC

本机通信：

Linux / macOS：

```text
Unix Domain Socket
```

Windows：

```text
Named Pipe
```

Android：

```text
Native IPC / local socket
```

远程：

```text
WebSocket
HTTP
RPC
```

协议定义独立版本化；默认编解码和传输实现优先使用 Assembly/C，以服务插件提供。客户端与宿主通过公开协议连接，不把所有传输固化进宿主。

---

# 29. TUI

TUI 本身尽可能由 Assembly 实现。

技术：

```text
Assembly
+
ANSI Escape Sequences
```

不依赖大型 TUI Framework。

支持：

```text
Input
Streaming Output
Markdown
Tool Status
Diff
Task Status
History
Search
Completion
Multiple Sessions
Panels
Keyboard Shortcuts
```

TUI 是完整产品界面，不是调试界面。

---

# 30. Desktop GUI

桌面 GUI：

```text
Qt 6
QML
minimal C++
```

架构：

```text
QML
 ↓
C++ Bridge
 ↓
C ABI
 ↓
Pangu Runtime
```

支持：

```text
Linux
Windows
macOS
```

GUI 不复制 Agent Runtime。

GUI 只是客户端。

---

# 31. Web UI

技术：

```text
TypeScript
React
Vite
```

架构：

```text
Browser
 ↓
WebSocket / HTTP
 ↓
pangud
 ↓
Pangu Runtime
```

Web UI 不包含 Agent Core。

---

# 32. VS Code Extension

技术：

```text
TypeScript
VS Code Extension API
```

架构：

```text
VS Code
 ↓
Extension
 ↓
IPC
 ↓
pangud
 ↓
Pangu Runtime
```

Extension 负责：

```text
Editor Context
Diff
Diagnostics
Commands
Panels
Terminal Integration
Selections
Workspace Events
```

Agent 本体依然属于 Runtime。

---

# 33. Android

Android 使用：

```text
Kotlin
Jetpack Compose
JNI
NDK
ARM64 Assembly
```

结构：

```text
Compose
 ↓
Kotlin
 ↓
JNI
 ↓
C ABI
 ↓
Pangu ARM64 Runtime
```

Android 不是远程壳。

它可以本地运行 Pangu Runtime。

---

# 34. iOS

iOS 不作为主要运行平台。

原因：

```text
动态插件限制
后台限制
进程限制
文件系统限制
动态代码限制
跨 App 控制限制
```

iOS 形态定义为：

```text
Remote Pangu Client
```

连接其他设备上的 Pangu Runtime。

---

# 35. CPU 架构

盘古支持多 CPU 架构实现。

目录：

```text
arch/
├── x86_64/
├── arm64/
└── riscv64/
```

主要目标：

```text
x86-64
ARM64
RISC-V 64
```

架构无关逻辑和架构相关实现严格分离。

---

# 36. Platform Abstraction Layer

所有 OS 差异通过 PAL 隔离。

统一接口：

```text
memory_map
memory_unmap

file_open
file_read
file_write
file_close

net_open
net_send
net_recv

thread_create

process_spawn

time_now

terminal_read
terminal_write

dynamic_library_load
```

对应实现：

```text
Linux PAL
Windows PAL
macOS PAL
Android PAL
Embedded PAL
```

除最小启动代码与 PAL 实现外，业务服务通过声明的 PAL 接口访问 OS。PAL 可按构建或启动配置选择；不承诺进程运行中切换操作系统后端。

---

# 37. 网络系统

网络栈能力：

```text
Socket
DNS
HTTP
HTTPS
WebSocket
Streaming
Proxy
Connection Pool
Timeout
Cancellation
```

网络实现位于 Platform Layer 和网络插件中。

---

# 38. TLS

TLS 是 HTTPS 的加密层。

负责：

```text
Encryption
Server Authentication
Integrity
```

HTTPS：

```text
HTTP + TLS
```

盘古使用成熟 TLS 实现。

密码学算法中的高性能热点可以使用 Assembly 优化。

TLS 协议本身不为了“纯汇编”重新实现。

---

# 39. SIMD

SIMD 是盘古的核心性能技术之一。

x86：

```text
SSE
AVX
AVX2
AVX-512
```

ARM：

```text
NEON
SVE
```

应用于：

```text
JSON
String
Search
Hash
Encoding
Compression
Math
Tokenizer
Model Operators
```

Runtime 根据 CPU Capability 自动选择实现。

---

# 40. Local Inference

本地模型通过插件提供。

```text
Pangu Runtime
 ↓
Inference Provider
 ↓
Backend
```

Backend 可以是：

```text
llama.cpp
自定义 CPU Engine
GPU Backend
NPU Backend
远程 Runtime
```

模型权重和推理引擎不进入 Core。

---

# 41. Storage

Storage 也是插件。

支持：

```text
Memory Storage
File Storage
SQLite
Remote Database
Vector Database
Custom Storage
```

Storage Interface 由独立服务定义模块提供，宿主不内置具体存储或存储领域规则。

---

# 42. Memory

Agent Memory 与内存管理不是一个概念。

Agent Memory 作为插件存在：

```text
Short-term Memory
Long-term Memory
Semantic Memory
Project Memory
User Memory
```

存储方式可替换。

---

# 43. Search

Search 是插件体系。

可能包括：

```text
Filesystem Search
Code Search
Symbol Search
Web Search
Semantic Search
Vector Search
Project Search
```

Agent 通过统一 Search Capability 调用。

---

# 44. Browser

Browser 不进入核心。

Browser Plugin 可以：

```text
Navigate
Click
Type
Read DOM
Screenshot
Download
Network Inspect
DevTools
Visual Control
```

底层实现可以连接：

```text
Chromium
CEF
CDP
系统浏览器
Remote Browser
```

---

# 45. LSP

LSP 作为 Coding Plugin。

提供：

```text
Definition
References
Symbols
Diagnostics
Hover
Completion
Rename
Workspace Symbols
```

Agent 不直接依赖任何具体语言服务器。

---

# 46. MCP

MCP 是一个 Protocol Plugin。

可以：

```text
Discover Server
Connect
Read Tools
Read Resources
Execute Tools
Subscribe
```

MCP 不是核心依赖。

---

# 47. Git

Git 是插件。

Agent 使用统一能力：

```text
status
diff
log
branch
commit
checkout
merge
blame
```

底层可以使用：

```text
libgit2
git CLI
自定义实现
```

---

# 48. Shell

Shell 是插件。

统一能力：

```text
Spawn
Input
Output
Exit
Signal
Resize
Environment
Working Directory
```

不同平台使用不同实现。

---

# 49. Permissions

权限系统独立于 UI。

权限可以配置：

```text
Allow
Deny
Ask
Scoped Allow
Temporary Allow
Session Allow
```

例如：

```text
filesystem.read:/project

filesystem.write:/project/src

network:https://api.example.com

process:git
```

---

# 50. Secrets

Secrets 不进入普通配置。

统一通过 Secret Provider：

```text
Environment
OS Keychain
Encrypted File
Hardware Security
Remote Secret Manager
```

插件只能获取被授权的 Secret。

---

# 51. Sandbox 与强制执行

沙箱能力由服务插件提供，按平台实际支持文件、网络、进程隔离以及 CPU、内存、超时等限制。

Process 插件和危险工具只有进入实际启用的隔离后端后，才具有其声明的 OS 限制。后端必须报告可强制执行的能力；要求的限制不受支持时明确拒绝，不静默降级成无限制执行。

同进程 Native 插件不受插件宿主的内存安全隔离，默认只加载可信代码。Cordis 风格作用域、服务依赖和 Capability 授权不能替代 OS 沙箱。

权限策略和沙箱后端可替换，但受控执行路径的必需校验不可绕过。运行在沙箱内的插件也不能自动获得其他 Session 的服务、凭据或网络权限。

安全边界由执行机制保证，不能依赖 Agent 或插件自觉遵守。

---

# 52. Config

配置采用统一数据模型。

配置内容：

```text
Runtime
Plugins
Models
Tools
Permissions
UI
Prompt
Context
Memory
Network
Logging
Performance
Sessions
```

配置格式可以有多个前端表示。

内部统一解析为 Pangu Config Model。

---

# 53. Logging

日志级别：

```text
error
warn
info
debug
trace
```

Release 模式支持：

```text
完全关闭
最小日志
结构化日志
```

日志系统不能成为高频性能瓶颈。

---

# 54. Metrics

Runtime 可以暴露：

```text
Startup Time
Memory Usage
Plugin Load Time
Tool Dispatch Latency
IPC Latency
Context Build Time
JSON Throughput
Search Throughput
Streaming Latency
CPU Usage
```

所有性能优化以 Benchmark 为依据。

---

# 55. 性能设计

核心原则：

```text
Zero-copy where possible

Arena allocation

Batch operations

SIMD fast path

Avoid unnecessary abstraction

Avoid GC

Avoid heavyweight runtime

Avoid repeated serialization

Streaming first

Lazy loading
```

---

# 56. 插件发现与按需激活

插件清单可在执行代码前读取；发现、校验、加载模块、创建实例和激活分别计时。

激活条件可以是 Profile 显式启用、服务依赖、工具调用或客户端功能请求。宿主先解析必需依赖，再激活提供者；只读取工具声明不应启动浏览器或语言服务器。

启动最小 Agent 只加载该组合的必需服务。Browser、LSP、Git、GUI 等可选能力在未启用且没有必需消费者时不进入运行图。

按需激活不等于所有服务都延迟到首次调用：Agent Loop 运行前必须满足其必需依赖。失败应报告对应插件、依赖链和版本冲突。

默认原生插件可静态链接并按需初始化，也可动态加载。静态链接插件的代码仍计入可执行文件体积；不初始化只能降低初始化开销和部分内存成本。

---

# 57. 万物可运行

盘古的“万物可运行”定义为：

> 同一套 Agent 架构可以针对不同 CPU 和操作系统构建对应 Runtime。

不是：

```text
一个 binary 跑所有设备
```

而是：

```text
同一个 Core Architecture
+
不同 Architecture Backend
+
不同 Platform Layer
+
不同 Plugin Set
```

---

# 58. 不同设备的形态

开发电脑：

```text
Pangu
+ Coding
+ Git
+ Browser
+ LSP
+ MCP
+ Search
+ Subagents
+ GUI
```

服务器：

```text
Pangu
+ Network
+ API
+ Storage
+ Multi-agent
```

Android：

```text
Pangu
+ Mobile UI
+ Files
+ Camera
+ Network
+ Local Tools
```

机器人：

```text
Pangu
+ Vision
+ Audio
+ GPIO
+ Motion
+ Sensor
```

路由器：

```text
Pangu
+ Network
+ Diagnostics
+ Configuration
```

嵌入式设备：

```text
Pangu
+ Minimal Runtime
+ Network
+ Hardware Plugins
```

---

# 59. UI 原则

所有 UI 都是 Pangu Client。

```text
TUI
Desktop
Web
VS Code
Android
```

它们共享：

```text
Session
Events
Agent State
Tools
Permissions
History
```

不会各自重新实现 Agent。

---

# 60. Runtime 原则

Runtime 是唯一真实状态来源。

```text
使用 daemon 或独立宿主时，UI 可以关闭

Agent 可以继续运行
```

重新打开任何 UI 后：

```text
重新连接 Session
```

即可读取当前状态。进程内模式下退出客户端通常也会停止 Runtime；进程重启后的恢复依赖已启用的持久化服务，不承诺未提交流片段或外部操作自动恢复。

---

# 61. 项目目录

以下为规划目录，尚未实现。目录位置不决定链接方式或是否可替换；默认实现通过服务接口组装。`core/plugin` 承载可复用管理实现，最小引导和生命周期约束由 `bootstrap` 持有。

```text
pangu/
├── bootstrap/             # 最小引导与宿主规则
├── core/                  # 可复用原生基础实现
│   ├── memory/
│   ├── string/
│   ├── buffer/
│   ├── collections/
│   ├── json/
│   ├── parser/
│   ├── event/
│   ├── protocol/
│   ├── capability/
│   ├── plugin/
│   └── ipc/
│
├── runtime/               # 服务定义与默认 Runtime 插件
│   ├── agent/
│   ├── session/
│   ├── tool/
│   ├── context/
│   ├── scheduler/
│   └── permission/
│
├── arch/
│   ├── x86_64/
│   ├── arm64/
│   └── riscv64/
│
├── platform/
│   ├── linux/
│   ├── windows/
│   ├── macos/
│   ├── android/
│   └── embedded/
│
├── plugins/
│   ├── model/
│   ├── filesystem/
│   ├── shell/
│   ├── git/
│   ├── search/
│   ├── browser/
│   ├── lsp/
│   ├── mcp/
│   ├── memory/
│   ├── storage/
│   ├── coding/
│   └── inference/
│
├── clients/
│   ├── tui/
│   ├── desktop/
│   ├── web/
│   ├── vscode/
│   └── android/
│
├── daemon/
│
├── include/
│   ├── pangu.h            # 引导 ABI
│   └── services/          # 版本化服务接口
│
├── protocol/
│
├── tests/
│
├── benchmarks/
│
├── docs/
│
└── examples/
```

---

# 62. 核心设计约束

## Core 不绑定模型

模型是插件。

## Core 不绑定 UI

UI 是客户端。

## Core 不绑定 Coding

Coding 是插件集合。

## Core 不绑定系统

系统通过 PAL。

## Core 不绑定 CPU

架构通过 Architecture Backend。

## Core 不绑定存储

Storage 是插件。

## Core 不绑定浏览器

Browser 是插件。

## Core 不绑定权限策略

Permission Policy 可配置。

---

# 63. 不允许进入 Core 的东西

以下组件不得直接进入 Assembly Core：

```text
Qt
React
Node.js
Chromium
CEF
SQLite
libgit2
具体 LLM SDK
具体浏览器 SDK
具体数据库
具体 GUI Framework
```

Core 只保留抽象和运行机制。

---

# 64. 项目原则

本章的工程约束由[《工程门禁与架构守卫》](docs/工程门禁与架构守卫.md)落实。文档和 Agent 自述不能替代检查；门禁控制面必须独立审核，正式验收使用受保护规则。当前仅实现 G00 文件登记准入，其余门禁按阶段建设，未实现不算通过。

### 1. Assembly-first

核心和高频路径优先汇编。

### 2. Small Core

核心必须保持极小。

### 3. Everything is a Plugin

从 Agent Loop 到可替换的底层基础服务均由插件提供。宿主只保留引导和不可绕过的接口、所有权、生命周期规则；静态注册与动态插件遵守同一套规则。

### 4. Stable ABI

明确管理宿主 ABI、服务接口、协议和持久化格式版本。稳定性优先于内部实现，早期版本不作未经验证的永久兼容承诺。

### 5. Runtime Owns State

UI 不拥有 Agent 状态。

### 6. Capability Security

受控服务调用没有隐式授权；可信 Native 插件具有宿主进程权限，强隔离依赖实际启用的 OS 沙箱。

### 7. Lazy Load

未使用能力不加载。

### 8. Zero-copy First

减少无意义的数据复制。

### 9. Benchmark Driven

没有 Benchmark，不做所谓“性能优化”。

### 10. Cross-platform by Architecture

跨平台通过明确的 PAL 和 Architecture Backend 实现。

---

# 65. 能力目标

盘古本身可以支撑完整 Coding Agent，包括：

```text
Repository Understanding
Code Search
File Editing
Patch
Diff
Build
Test
Git
Shell
LSP
Browser
MCP
Web Search
Subagents
Planning
Memory
Skills
Permissions
Context Management
Long-running Tasks
Multi-session
Remote Runtime
```

因此：

```text
极小
```

不等于：

```text
功能少
```

完整能力来自 Plugin Universe。

---

# 66. 性能目标

盘古追求：

```text
毫秒级冷启动

极低 Idle Memory

极低 Tool Dispatch 开销

极低 IPC 延迟

高吞吐 Parser

高吞吐 Search

按需插件加载

无 GC

无大型语言 Runtime
```

实际用户等待时间应主要来自：

```text
LLM
Network
External Tool
Disk
Build
```

而不是 Agent Framework。

---

# 67. 最终产品形态

盘古不是一个单独软件。

它是一套 Runtime。

同一个 Runtime 可以形成：

```text
Pangu TUI

Pangu Desktop

Pangu Web

Pangu VS Code

Pangu Android

Pangu Server

Pangu Embedded

Pangu Robot
```

它们使用同一个核心协议、插件系统、Session Runtime 和 Agent Runtime。

---

# 68. 开发实现顺序与阶段验收

详细顺序、技术栈转换、依赖清单、双运行差异验证、上线门禁与回滚策略见[《原生重写路线与迁移 TODO》](docs/原生重写路线与迁移TODO.md)。该计划以 Pi 为行为基线、Harness/Cordis 为组合参考；Rust 重写项目仅提供迁移组织方法。T01～T48 与功能/依赖/证据账本逐项追踪，未验证任务不能勾选完成。

实现顺序服从同一最终架构，允许通过测试和实测修正规格；不承诺未经验证的设计永远不变。

## 阶段 A：固定参考与定义接口

前置要求：建立文件登记门禁与拒绝测试。第一份生产代码进入前必须具备模块清单、受保护的规则执行方式、实际依赖检查 G01 及构建检查 G02；不能先累积实现再补架构约束。具体时点见[工程门禁目录](docs/工程门禁与架构守卫.md)。

1. 固定 Pi、deepseek-harness 和 Cordis 的参考提交，记录 Harness vendored 修改。
2. 以 Pi 梳理最小 Agent 所需功能和行为，区分首版必需能力与后续可选能力。
3. 定义宿主与插件边界、服务 ID/版本/作用域、资源所有权、异步调用和取消语义。
4. 定义 C ABI 草案、清单格式、配置组合和信任模型；明确哪些替换仅在启动时发生。

验收：参考范围和差异可追踪；两种测试提供者可替换同一服务；缺失依赖、版本冲突和初始化失败有确定结果。

## 阶段 B：最小原生宿主

1. 构建系统、x86-64/Linux 启动及最小 PAL。
2. 引导分配支持、基础 String/Buffer、必需 Collections。
3. 服务注册、作用域、插件生命周期、资源追踪与默认事件后端。
4. 静态注册及 Native 插件装载；优先保证停止和清理，不强求动态库热卸载。

验收：依赖有序启动/停止、部分初始化回滚、在途调用与取消竞争、重复释放和失效句柄均有针对性测试。以不同提供者验证真正解耦。

## 阶段 C：Pi 基线的最小 Agent 闭环

1. 以服务插件实现 Session、Tools、Context/Prompt、Agent Loop 和权限策略。
2. JSON、HTTP/TLS 与一个模型提供者；使用成熟 TLS 实现。
3. 参考 Pi 实现流式响应、工具参数校验、工具调用及结果续接、取消和错误终结。
4. 接入文件与 Shell 默认服务和工具，提供最小 CLI 客户端。
5. 完成必要的会话存储与恢复，随后接入 TUI。

验收：使用确定性假模型回放 Pi 参考用例，明确有意差异；完成“读取项目 → 修改 → 构建/测试 → 返回结果”。替换 Agent Loop、模型、文件服务或存储实现时，不修改宿主代码。通过独立配置关闭可选能力。

## 阶段 D：产品体验与隔离

按 Pi 基线范围补齐上下文管理、会话体验和扩展钩子；完成 TUI 产品能力。增加 daemon/IPC、客户端重连、Process 插件和实际 OS 沙箱，随后加入按需激活及配置管理。

验收：权限拒绝不能执行工具；中断与重连结果一致；慢客户端不会无限占用内存；故障插件的影响符合所选执行位置。

## 阶段 E：扩展能力与平台

按实际需要增加 Git、Search、LSP、MCP、Subagents、Remote Plugin，以及 Windows、ARM64、Android、Desktop、Web、VS Code 和其他平台。

每个阶段记录产物体积、冷启动、空闲/峰值内存和关键路径开销。扩展能力以独立组合交付，不自动成为最小 Agent 的必需依赖。

---

# 69. 项目验证标准

行为验收与工程门禁同时满足才可放行。适用检查必须提供绑定源码、策略、工具链和产物的验证结果；性能预算未建立、必需门禁未实现或执行失败时不能宣称阶段验收完成。

盘古最终应该能够完成这样的任务：

```text
用户：
这个项目编译失败，帮我修好。
```

Runtime：

```text
读取项目
 ↓
理解代码
 ↓
搜索相关文件
 ↓
调用 LSP
 ↓
运行 Build
 ↓
分析错误
 ↓
修改代码
 ↓
查看 Diff
 ↓
再次 Build
 ↓
运行 Tests
 ↓
Git 状态检查
 ↓
返回结果
```

同时：

```text
Agent Core
```

仍然保持极小。

---

# 70. 项目愿景

盘古最终不是：

> 一个用汇编写出来的 Claude Code。

它是：

> 一个足够小，可以运行在极低资源设备；又足够通用，可以承载 Claude Code、Codex、通用 Agent、机器人 Agent、移动 Agent 和未来未知 Agent 形态的基础运行时。

盘古希望证明：

```text
Powerful ≠ Heavy

Cross-platform ≠ Heavy Runtime

Plugin-driven ≠ Slow

Low-level ≠ Inflexible

Small ≠ Limited
```

最终目标：

> **One Core. Any Agent. Any Device.**

---

# Pangu

**Assembly-first AI Agent Runtime**

```text
Small enough to run anywhere.
Powerful enough to become anything.
```
