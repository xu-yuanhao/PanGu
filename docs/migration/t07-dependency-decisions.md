# T07 原生依赖决策与替换方案

状态：候选决策 `CANDIDATE_NOT_APPROVED`。本文给出 P0/P1 首批核心开发所需的依赖归属、候选版本与替代方案；不批准任何第三方库进入产品构建，也不开始实现这些库。技术事实按 2026-10-19/20 查询的官方来源记录；无法核实的内容标 `UNKNOWN`，不做推测。

机器可读对应项见 [dependency-inventory.json](dependency-inventory.json)（Pi/Node 侧快照）与本文末尾的决策表增量。产品原生依赖批准清单仍为空。

## 1. 决策方法

判断顺序：

1. 该能力是否进入 P0/P1 必需路径（见 [implementation-scope.json](implementation-scope.json)）。
2. 是否存在已维护、许可证许可、可在 x86_64 Linux 静态/随包交付的实现。
3. 系统库与随包交付的差异：系统库减少随包字节但引入发行版差异；随包交付固定行为但增加体积与供应链责任。
4. 替换代价：接口是否被限制在一个已登记服务后面，替换是否只需改组合。

体积和性能在有真实产物前一律 `UNKNOWN`，不以估计值充当预算。没有使用者的假设需求不引入库或公共接口。

## 2. 首批原生需求的来源

| 来源 | 证据位置 | 原生含义 |
|---|---|---|
| Pi Agent Loop 与事件 | `references/pi/packages/agent/src/agent-loop.ts`、`types.ts`（提交 `2ec55d808d455b1a64a2bf7888198207a096120a`） | 需要事件顺序、取消、工具批次与结果归并，不需要 JS 对象系统 |
| Pi 工具与 Session | `packages/coding-agent/src/core/agent-session.ts`、`session-manager.ts` | 文本工具、JSONL 会话、最终记录恢复 |
| Pi Provider | `packages/ai` 包及 `pi-surface-catalogue.json` 的 LLM 条目 | P1 只需 OpenAI Chat Completions 兼容子集 + SSE + tool calls |
| Harness/Cordis 服务组合 | `references/cordis/packages/core/src/fiber.ts`、`registry.ts`、`service.ts`；`references/deepseek-harness/packages/core/*/src/index.ts` | 定义/提供/消费分离、effect 注册、反向清理、依赖缺失时不激活 |
| Pi 依赖清单 | `docs/migration/dependency-inventory.json` | 仅用于识别能力，不把 npm 树复制到产品 |

Pi 的 npm 依赖（undici、typebox、yaml、jiti、quickjs-wasi 等）不是原生依赖候选。对应能力需要重新选型。

## 3. 能力逐项决策

### 3.1 TLS

| 候选 | 版本/来源 | 许可证 | TLS 1.3 | 证书验证/信任源 | 结论 |
|---|---|---|---|---|---|
| OpenSSL（系统 libssl） | 3.5 LTS，2025-04-08 发布，支持至 2030-04-08 | Apache-2.0（3.0 起） | 是 | 完整 X.509 store、SNI、ALPN | P1 首选候选（系统库或随包二选一） |
| mbedTLS | 3.6 LTS（支持至 ≈2027-03）或 4.1.x | Apache-2.0 OR GPL-2.0-or-later | 3.6 起默认启用 | 提供 X.509 解析与验证回调 | 备选（随包） |
| wolfSSL | 未在本次核实 | 未核实 | 未核实 | 未核实 | 暂不评估（UNKNOWN） |
| BearSSL | 0.6（beta 质量） | MIT 风格 | 否（仅 RFC 5246/1.2） | 需自行接信任源 | 拒绝作为首批方案 |
| BoringSSL / AWS-LC | 未在本次核实 | 未核实 | 未核实 | 未核实 | 无明确理由不引入 |

决定：

- 不自研 TLS。首批 Provider 需要 TLS 1.2+、证书验证、SNI/ALPN 与代理。
- P1 推荐 **系统 libssl（OpenSSL 3.x）**：发行版负责安全更新，随包字节不包含系统库文件，但必须计入最小部署依赖清单与完整部署成本。风险是发行版版本差异——必须在依赖清单登记最低版本并在启动时检查。
- 若后续需要单文件随包交付，首选 **mbedTLS 3.6 LTS**（TLS 1.3 默认启用、许可证宽松）；切换只改 TLS 服务提供者，不改消费者。
- BearSSL 只满足 TLS 1.2，且为 beta 质量；不作为首批候选。它可作为“极小体积”场景的第二提供者实验，但不能以体积为由接受协议缺失。

代理：`HTTPS_PROXY`/`HTTP_PROXY` 的 CONNECT 隧道由 HTTP 服务实现，不由 TLS 库隐式处理。证书/代理的错误必须显式上报，不静默降级。

### 3.2 HTTP

| 候选 | 版本/来源 | 许可证 | 说明 | 结论 |
|---|---|---|---|---|
| libcurl | 8.22–8.23 区间（2026-09/10），curl.se | curl license（MIT/X 风格，非完全相同） | HTTP/1.1、h2、h3、代理、取消、流式回调 | P1 首选候选 |
| 自写 HTTP/1.1 + SSE | 无 | 自有 | 工作量大，需自己处理分块、超时、代理、TLS 交互 | 仅在 curl 不可随包时评估 |
| llhttp | 未核实版本 | 未核实 | 仅解析器，不含连接/TLS/代理 | 不支持作为完整方案 |
| nghttp2 | 未核实版本 | 未核实 | 仅 h2 | 若自行实现 HTTP 才需要 |

决定：

- P1 首选 **libcurl**：流式响应、取消、代理、证书验证与 HTTP/2 都由同一服务处理，避免自写协议栈。必须约束：只启用 HTTPS、禁用不必要的协议、关闭重定向自动跟随或显式限制、固定超时与最大响应体。
- libcurl 的 TLS 后端必须与 3.1 的决策一致（OpenSSL 或 mbedTLS），并在构建回执中记录实际后端。
- 流式与取消语义必须在 T27 用本地受控服务验证：分块到达、慢消费者、半途取消、错误码、超时、代理 CONNECT。当前全部 `NOT_RUN`。

### 3.3 JSON 与 Schema

| 候选 | 版本/来源 | 许可证 | 能力 | 结论 |
|---|---|---|---|---|
| yyjson | 0.13.0，github.com/ibireme/yyjson | MIT | RFC 8259、int64/uint64/double、JSON Pointer/Patch | 候选（解析与序列化） |
| jansson / json-c | 未核实 | 未核实 | 通用 JSON | 系统库备选 |
| simdjson | 参考目录已有 | 未核实（C++） | 高性能解析 | 不进入 P0/P1：C++ 且无已批准桥接 |

Schema：Pi 用 typebox 在工具参数边界做校验。原生首版不照搬 JSON Schema 运行时。

- 工具参数校验由各工具自己声明的固定字段表完成，错误结构化返回；不引入通用 Schema 引擎。
- 若 T12 出现第二个需要 Schema 的消费者（例如扩展声明的工具），再评估 C 实现。找到的 C++ 实现 `pboettch/json-schema-validator`（MIT，2.4.0）不支持作为 C 依赖，且未核实 draft 2020-12 一致性。
- 会话与协议版本用显式整数与固定字段，不依赖 Schema 推导。

### 3.4 Allocator / Buffer / 字符串

| 候选 | 许可证 | 结论 |
|---|---|---|
| glibc malloc（默认） | LGPL-2.1+（动态链接场景） | 后续 hosted 组合候选；当前 freestanding 组合不采用 |
| mimalloc | MIT | 备选（若实测需要）；版本核实为 3.5.x |
| jemalloc | BSD-2-Clause | 备选；5.3.x |
| 自写分配器 | 自有 | 只做固定大小 arena 或测试分配器，不做通用分配器 |

决定：P0 首版采用 PAL 映射支持的有界 arena，保留 allocator 接口；系统 malloc 只作为后续单独登记的 hosted 提供者，T11 实现 Buffer/字符串与显式长度、容量上限。替换 allocator 不得修改消费者。不提前引入第三方分配器；只有在实测有收益且记录分配计数后才评估。所有字节数预算保持 `UNBASELINED`。

### 3.5 文件与进程

| 能力 | 方案 | 说明 |
|---|---|---|
| 文件读写 | 直接 POSIX（open/read/write/rename/fsync） + PAL 封装 | 不引入库；路径规范化与错误映射自有 |
| 进程执行 | POSIX `posix_spawn`/`fork`+`execve`，管道与进程组 | 取消用信号与进程组终止；不做“不可撤销副作用”的盲重试 |
| 动态加载 | `dlopen`/`dlsym`（T16 才启用） | 首批静态注册；DSO 需单独 ABI 与卸载验证 |

系统调用只允许出现在登记的 platform/PAL 模块；检查器已有对象级指令检查，文本搜索不作为证明。

### 3.6 终端

| 候选 | 版本/来源 | 许可证 | 结论 |
|---|---|---|---|
| termbox2 | 2.5.0，github.com/termbox/termbox2 | MIT | T28 最小终端需求评估候选，T33 扩展能力前复核 |
| ncurses | 未核实具体版本 | X11/MIT 风格（未在本次核实文本） | 备选；系统库 |

决定（按用户 2026-10-20 调整）：T28 引入首个薄 TUI 所需的最小终端 PAL；优先复用已有平台层，不提前建设通用 UI 框架。若需要终端库，须在 T28 前固定来源/许可证/依赖并登记。T33/T34 完善输入和展示；GUI/Qt/自写渲染器继续延期。

### 3.7 搜索

| 候选 | 版本/许可证 | 结论 |
|---|---|---|
| PCRE2 | 10.47–10.49 区间；BSD-3-Clause WITH PCRE2-exception | 首个需要正则的工具（如 grep 类）再引入 |
| TRE | 未核实 | 备选 |
| 自写匹配 | 自有 | 仅用于固定模式，如 glob/前缀匹配 |

决定：P1 工具集（read/write/edit/bash）不需要正则库。不为假设需求提前引入 PCRE2；搜索能力进入 P2 时再登记。

### 3.8 配置与 YAML

Pi 的 settings 是 JSON，扩展配置使用 YAML。P1 只要求 `required_settings_subset`（JSON）。不引入 YAML 库；未知或未支持的配置键必须显式诊断，不静默回退。

## 4. 随包 vs 系统库

| 方案 | 优点 | 风险 | 适用 |
|---|---|---|---|
| 系统库 | 体积小、发行版修补安全漏洞 | 版本漂移、目标环境缺失 | 开发期与发行版安装 |
| 随包（源码或静态） | 行为固定、可复现 | 体积、供应链与更新责任在我方 | 单文件发布或需要固定版本时 |

首批决定：开发与验证阶段使用系统库并在依赖清单登记最低版本；是否随包在 T40（打包与支持边界）用真实产物决定。两种交付不得使用不同统计口径规避体积预算。

## 5. 仍为 UNKNOWN 的项目

- 所有库的实际体积与性能（无真实构建产物）。
- wolfSSL、BoringSSL/AWS-LC、llhttp、nghttp2、jansson/json-c、TRE 的版本与许可证细节（本次未核实）。
- 目标发行版的最低 OpenSSL/glibc 版本矩阵。
- 跨平台（非 x86_64 Linux）支持。

这些项目在有真实消费者或产物前不进入批准清单。

## 6. 不引入清单（明确理由）

| 能力 | 不引入 | 理由 | 重新评估条件 |
|---|---|---|---|
| JSON Schema 引擎 | — | 单一消费者可用固定字段表；C++ 实现不可用 | 第二个 Schema 消费者出现 |
| 通用事件总线/插件框架 | — | 服务组合已由 admission/service_graph 约束 | 出现跨进程或热更新需求 |
| GUI/渲染器/Qt | 全部 | 路线明确延期 | T33 之后且另行批准 |
| 自研 TLS | — | 禁止 | 永不 |
| Node/Python/JVM 运行时 | 全部 | 最小原生 Agent 约束 | 不属于产品运行时 |

## 7. 决策表

| ID | 能力 | 阶段 | 决定 | 状态 |
|---|---|---|---|---|
| DEP-TLS-01 | TLS | P1 | 系统 OpenSSL 3.x；mbedTLS 3.6 LTS 为随包备选 | CANDIDATE |
| DEP-HTTP-01 | HTTP 客户端 | P1 | libcurl，TLS 后端固定 | CANDIDATE |
| DEP-JSON-01 | JSON | P0 | P0 自写有界最小解析器，T12 必须通过有效/无效 JSON 语料与资源上限；yyjson 保留后续替代评估 | CANDIDATE |
| DEP-SCHEMA-01 | Schema 校验 | P1 | 不引入通用引擎，用固定字段表 | DECIDED |
| DEP-ALLOC-01 | 分配器 | P0 | PAL 支持的有界 arena + allocator 接口；系统 malloc 延期 | CANDIDATE |
| DEP-FS-01 | 文件 | P0 | POSIX + PAL | DECIDED |
| DEP-PROC-01 | 进程 | P1 | POSIX spawn/exec，自有管道 | DECIDED |
| DEP-DL-01 | 动态加载 | P2/T16 | dlopen，静态注册优先 | DEFERRED |
| DEP-TERM-01 | 终端 | P1 最小 / P2 完整 | T28 最小 PAL；按需求评估 termbox2 | CANDIDATE |
| DEP-REGEX-01 | 正则 | P2 | PCRE2 候选 | DEFERRED |
| DEP-YAML-01 | YAML | 未定 | 不引入 | DEFERRED |

“DECIDED” 表示在本文范围内不引入外部库或使用 POSIX；“CANDIDATE” 仍需独立审查后才能进入批准清单。T07 整体不因本文完成，终端/GUI 等延期项按主计划条件保留。

## 8. 验收与后续

本文满足的验收：

- 首批核心开发需要的 TLS/HTTP/JSON/分配/文件/进程有明确归属与替代方案。
- 未登记运行时依赖进入产品；延期项有明确触发条件。
- 无实测的体积与性能标为 UNKNOWN；不自研 TLS。

仍未完成：

- 每个候选库的许可证文件、精确版本与完整性摘要尚未登记进机器可读清单（需在真正引入时完成）。
- T27/T12 的实测（流式、取消、错误、代理、证书）尚未运行，全部 `NOT_RUN`。
- 终端与 GUI 是 P2，不据此声明 T07 完成。

## 9. 来源

- BearSSL overview：https://www.bearssl.org/
- mbedTLS 3.6 LTS release：https://github.com/Mbed-TLS/mbedtls/releases/tag/v3.6.0
- mbedTLS 安全公告（示例）：https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2026-07-tls13-hrr-unadvertised-group
- OpenSSL 下载与路线图：https://www.openssl.org/source、https://www.openssl-library.org/roadmap
- curl 版权与许可：https://curl.se/docs/copyright.html
- yyjson：https://github.com/ibireme/yyjson
- pboettch/json-schema-validator releases：https://github.com/pboettch/json-schema-validator/releases
- PCRE2：https://www.pcre.org/
- termbox2：https://github.com/termbox/termbox2
- mimalloc（Wikipedia 版本信息）：http://restivo.info/wiki/Mimalloc
- jemalloc 打包元数据（许可证）：https://repology.org/tools/project-by?name=jemalloc&name_type=srcname&repo=debian_unstable&target_page=project_packages

## 主审收尾决定（2026-10-20）

T10 没有第三方产品依赖，不链接 libc；T11 只做有界 arena，不自研通用 malloc。T12 按既有路线做最小 JSON 并保留替换接口，不引入未固定版本库。OpenSSL/libcurl/终端等仍是后续候选，不构成 T10 前置批准。表中的版本区间和非官方来源不构成依赖锁定，实际引入前必须核实官方精确来源、许可证与完整性。
