# T01/T02 参考冻结与自用范围审查

本轮已开始执行实现计划。这里记录源码与规划的技术验收，不签发控制面升级、兼容认证或发布批准。

## T01：离线来源与恢复

执行入口：`tools/migration/reference_bundle.py`。参考仍以 `reference-baseline.json` 中五个提交为准；没有修改参考仓库。

离线包位置：`/home/xu-yuanhao/Documents/PanGu-artifacts/references/2026-10-19-v1`。该目录在产品工作树之外，不参与产品构建。回执记录 manifest 哈希；内部有每个仓库的 Git bundle 和逐文件清单。

| 参考 | 跟踪文件 | lockfile | 许可证/声明文件 |
|---|---:|---:|---:|
| Pi | 1,961 | 7 | 1 |
| deepseek-harness（包含 vendored Cordis） | 16,297 | 2 | 28 |
| 独立 Cordis | 145 | 0 | 1 |
| VS Code | 20,808 | 68 | 12 |
| pi_agent_rust | 22,157 | 24 | 379 |

实际校验：提交和 origin 与基线一致；拒绝跟踪改动、未跟踪/忽略输入；核对全部文件的 Git blob、模式和 SHA-256；每个 bundle 在全新临时目录离线恢复并重新核对；发布前再次确认参考未改变。

Git 属性可能让工作副本的 Windows 脚本采用 CRLF，因此校验以 Git blob 的实际规范内容为准，仅在属性允许且原字节不匹配时尝试换行规范化。不会无条件清洗所有文件的换行。

VS Code 中有 98 个 filter/LFS 标记文件，包内保留 Git 跟踪的指针/原始 blob，不下载资源，不将这些外部内容当作已准备的 oracle 输入。手册曾引用但已不存在的 Rust baseline JSON 继续保持缺失状态。依赖还未安装；源码可恢复不等于 TS oracle 已可运行，GAP-001 的 T09 部分仍开放。

工具拒绝测试覆盖：离线恢复、链接/许可证/锁文件保留、跟踪/未跟踪/忽略改动、assume-unchanged 隐藏改动、错误 pin/origin/key digest、损坏 bundle/清单、覆盖旧产物、路径逃逸、丢失输入，以及属性声明的换行与未物化 filter。

复验命令：

```sh
python3 tools/migration/test_reference_bundle.py
python3 tools/migration/reference_bundle.py verify --output /home/xu-yuanhao/Documents/PanGu-artifacts/references/2026-10-19-v1
```

包可移动到另一台具备 Python/Git 的环境；`verify --output <移动后的目录>` 不需要联网。校验清单的可信来源仍需受保护策略/维护者审核，不能抵御拥有权限的人同时替换包和回执。

## T02：先做什么

采用计划规定的项目自用范围，没有假设存在某个外部迁移客户。具体记录在 `implementation-scope.json`。

- P0：x86-64/Linux 原生宿主及替换/生命周期测试。
- P1：假 Provider 回放、一个明确的 OpenAI Chat Completions/SSE 协议子集、文本 read/write/edit 和非持久 bash、配置与最小 Session。
- P2：按原 TODO 增加日常 TUI、队列、压缩、资源及原生扩展。
- P3：对具体工作流认证选定 CLI/JSON/RPC；OAuth、TS SDK 和 JS 扩展按实际使用成为必需项，未实现就阻断相应工作流。
- P4：平台、执行位置和其他客户端分别认证。GUI 框架在开始 GUI 工作时再决定。

P1 的 flag、配置键和环境变量子集已列明。其他功能仍在功能总账里，不因首版缩小范围而删除；P1 不作完整 Pi 替代声明。Native 同进程可信边界与显式授权检查沿用主架构，不把 trusted-local 模式称作 OS 沙箱。

## 边读参考边记录的差异

| ID | 源码/文档证据 | 对实施的影响 | 当前结论 |
|---|---|---|---|
| REF-001 | Pi `settings-schema.ts`、`settings-defaults.ts` 与 `docs/settings.md` | 本轮复核更正：当前文档表格已覆盖 `cacheWarming`、`showCacheMissNotices`；Schema 中另有旧键和持久化元数据 | 原遗漏判断已撤回；实际盘点差异及分类见 t03-t04-review.md |
| REF-002 | Pi `docs/cli-integration.md` | print 与 JSON 模式面对 assistant error/abort 的退出码规则不同；length 也有单独规则 | 逐模式建立 case，不用全局“模型错误都非零退出”替代 |
| REF-003 | Pi `rpc-types.ts` 的 `RpcCommand` | 当前有 `clear_queue`、`clone`、`get_entries`、`get_tree` 等表面 | 旧 Rust 历史功能矩阵不能覆盖当前 Pi |
| REF-004 | Pi `settings-manager.ts` 的 `getToolListError` 与 `resolveDefaultTools` | CLI 工具列表和配置列表的混合/加减行为不能混为一套规则 | CLI 与 settings 分别登记、分别验证 |
| REF-005 | Harness `vendor/README.md` 与独立 Cordis `package.json` | Harness 固定 rc.7 并含生命周期修正，独立参考为 rc.11 | 原生生命周期设计核对两者，不把独立 Cordis 的版本号当作 Harness 实际实现 |

以上为静态来源审查，尚未运行 Pi 行为测试。发现不一致时继续登记，不通过改写参考材料消除差异。

## T03 进行中的工具证据

开发期 TypeScript 7.0.2 解析器已在产品目录外按精确版本安装，禁用 npm 生命周期脚本。其新的 AST API 与旧版 `typescript.js` 不同，实现依据已安装包的类型声明核对；没有执行 Pi 源码。提取器保留声明原文和源码摘要，区分 public/private 成员，并在语法错误时拒绝输出。

当前提取 28 个源码文件的 2,265 项 AST 记录，仅为发现证据。重导出闭包、文档反向核对和 feature/case 映射尚未完成，因此 T03 继续保持 IN_PROGRESS。不能把“2,265 项”当作测试通过数或兼容覆盖率。

## T03/T04 完成记录

上面的 28 文件/2,265 项是上一轮中间状态。当前已完成包出口、相对重导出、CLI 入口与文档的静态盘点和差异追踪，见 [T03/T04 审查](t03-t04-review.md)。生产实现与兼容性验证未执行。
