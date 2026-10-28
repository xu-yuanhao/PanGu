# 当前原生实现记录

实际进度与下一交付物见[核心 TODO](../原生重写路线与迁移TODO.md#核心交接区)。完整移植以两份 TODO 的全部验收为准。

## 当前调用链

`src/composition/pangu.c` 通过原生 Host 组合工作区、进程、文件/工具、模型和 Durable Session；`durable_client.S` 将终端输入原子写成 User Entry 与 Generation，再推进 Task/Scheduler。Generation 保存请求配置/cutoff，投影已提交上下文，驱动模型流；assistant 与 owned Tool/wait 同批提交，结果入库后由后继 Generation 继续回答。Runtime/TUI 使用既有 submit/cancel/poll/history 接口。

默认存储为工作区 `.pangu` 中的 PanGu JSONL；`PANGU_SESSION_DIR` 可选目录。退出暂停调度，排空模型/工具资源，再关闭 Session/存储；重开从持久任务继续。凭据由环境注入，持久配置保存公开模型/端点/工作区/提示词。

| 能力 | 实现入口与已验证范围 |
|---|---|
| 插件组合 | `host.S`、`native_plugin.S`：依赖排序、父子作用域、局部替换、协作释放、本地库入口 |
| 记录/文档/Session | `records.S`、`record_memory.S`、`documents.S`、`document_tx.S`、`document_reader.S`、`pending.S`、`holds.S`、`hold_timer.S`、`durable_session.S`：原子提交/采纳、查询/分页、revision、延迟保存、观察与关闭 |
| 持久存储 | `record_sqlite.S`、`record_jsonl.S`：混合事务、重开、损坏/截断、失败锁存；JSONL task/doc sidecar维护 |
| 任务图 | `task_graph.S`：Host task.graph/TUI可选只读查询，live任务分页、裁剪状态/owner/wait、所属会话分页与注册元数据分类；Session readOnLine保护，重开图一致 |
| Tool hooks | `extensions.S`/`durable_tool.S`：原生beforeTool/afterTool进入意图前与结果提交前，参数与结果按选中扩展顺序传递，拦截/异常、两次校验、pending/取消/重开；工具结果保留最终参数及hook元数据 |
| 扩展选择与组合 | `extensions.S`：extensions.registry的稳定名称选择、同名工具/section保序覆盖、同步wrapper与失败报告、标签渲染和请求固定选择；工具分派保留未改动的注入提供者，扩展实现进入同一Durable Tool链 |
| Agent配置 | `agent_state.S`：Host agent.state事务写pangu.agent，稳定名称配置、字段清除/去重提交、独立读取；instructions在资源提示后进入Generation请求，tools/modelTools筛选模型声明，执行gate保持原提供者lease并拒绝禁用名字 |
| 扩展任务 | `task_provider.S`：可选Host task.definitions校验/查找，保留内置任务、拒绝重名/无效定义；原生DSO通过公开commit/commit_with执行checkpoint与终态，Scheduler重开后继续，消费者清理先于库卸载 |
| Task/Scheduler | `task_runner.S`：phase/checkpoint、sleep/wake、发现/公平推进、所有权/等待图、取消级联、暂停与进程恢复 |
| 上下文压缩 | `durable_compaction.S`：UTF-16估算/合法Entry切点、结构化摘要、固定范围与配置、持久阶段恢复、原子Submission/任务终态；完整摘要输入采用owned JSON与借用模型视图，Host服务和可选TUI表；容量阈值触发Generation-owned压缩、原子直接Entry放置、等待父任务重开及恢复生成，后台阈值触发/去重/空闲调度 |
| 系统上下文 | `durable_generation.S`：当前OpenAI路径折叠system正文和命名section，覆盖保序、null删除/重加追加，结果进入实际模型提示和历史恢复 |
| 上下文投影 | `durable_generation.S`：请求/历史共用cutoff分页、最新head优先、旧head编辑保留、omit/replace最后生效；超过300条记录与两后端重开通过；工具结果按调用次序整理，首个同ID匹配、孤立过滤与临时缺失回执 |
| Generation/Tool | `durable_generation.S`、`durable_tool.S`：保存配置、模型请求、工具轮次、原子结果、副作用恢复；真实DeepSeek在SQLite/JSONL通过 |
| Submission/输入队列 | `durable_inputs.S`：去重、admission、one/all steer/follow-up边界、展示写入、reset/stale、失败策略、撤回/状态与持久结算；产品导出inputs服务 |
| 前台 Client | `durable_client.S`：严格UTF-8输入、原子创建任务、忙时拒绝、事件借用、取消、暂停重开与历史；专项ASan/UBSan通过 |
| 模型元数据与压缩策略 | `models.S`、`models_data.S`：固定Pi目录、用户models.json元数据/覆盖、端点/provider选择、默认model.metadata与逐字段compaction.modelOverrides；公开值独立持有 |
| 会话视图 | `conversation_view.S`：conversation.view Host/TUI只读入口，统一会话/原始有效Entry/已有Live与UsageDoc/queued Submission，readOnLine保护与分页、重开 |
| 会话用量 | `usage.S`：pangu.usage按models/tools累计，Generation/工具结果/摘要尝试同批提交；响应模型身份优先，计数及已报告cost、可选reasoning/cacheWrite1h，Host usage读取与重开 |
| 模型传输 | `mimo.S`、`mimo_projection.S`、`mimo_stream.S`：OpenAI-compatible SSE、工具批次、增量内容、取消和显式连接注入 |
| 资源选择 | `resource_select.S`：作用域`!`/`+`/`-`规则、文件/SKILL父目录变体、常见glob/brace/range、显式与项目优先；筛选先于名称去重，排除路径不由自动发现重新启用 |
| 资源生命周期 | `resources.S`：Host resources服务、整体候选采纳/失败保留、空闲刷新、独立版本快照与前台展开；可选TUI资源操作表已提供 |
| 设置与资源路径 | `settings.S`：用户/项目JSON层、递归覆盖与错误诊断、独立所有权；配置skills/prompts的普通文件/目录进入技能发现与模板命令，路径相对所属配置目录解析 |
| 提示模板/显式技能 | `prompt_templates.S`：用户/项目Markdown模板、引号参数、位置/全部/默认/切片参数单次替换；前台/模板和/skill:name展开后进入持久Submission，显式技能读取当前正文 |
| 技能资源 | `skills.S`、`skills_posix.c`：用户/项目默认发现、根技能优先、名称/规范路径去重、YAML元数据、校验诊断与转义XML；提示列表→模型read技能/相对资源→write真实闭环 |
| 编码工具 | `read_tool.S`、`write_tool.S`、`edit_text.S`、`edit_tool.S`、`bash_tool.S`、`process.S`：真实读改写运行、分页/快照、Unicode匹配、异步进程/取消、原始输出文件 |

## 本批验证与集成状态

- beforeTool在意图前运行并二次校验；最终参数进入既有checkpoint与工具结果，assistant原记录保持。afterTool在原生结果/元数据快照后、提交前运行，异常或不合法结果报告后保留前项结果。
- 产品`--native-tasks --sanitize`通过顺序/参数/结果链、Scheduler重开、拦截与异常无效果、非法参数二次拒绝、挂起/取消及原生库清理；原有SQLite/JSONL Durable Tool/恢复专项及Runtime整批退出0。
- `--live-extensions`真实DeepSeek发起write(original.txt)，插件挂起后改写为hooked.txt/HOOK_ARGUMENTS，结果经afterTool后继续回答；同场景保留资源刷新、Agent配置、扩展native_echo。最终产品构建、G00/Assembly检查通过。

## 当前接口边界

正文输入、Generation正文累积和摘要保存/放置已动态化。provider单次事件、reasoning、工具参数/结果仍有现有边界，按K05/K08完成；K12继续retry期间queued摘要等剩余组合，K13补齐结构化message_update、report及剩余Live事件/观察。TUI消费端的旧历史校验与显示预算由TUI线接入动态入口。其他差异仍保留在核心TODO。

## 复验入口

```bash
python3 -B tools/runtime/build_test.py
python3 -B tools/runtime/build_pangu.py --output /tmp/pangu-native/pangu
python3 -B tools/runtime/test_pangu_pty.py
python3 -B tools/runtime/test_product_resources.py
python3 -B tools/runtime/test_product_resources.py --native-tasks --sanitize
```

真实模型通过`PANGU_MODEL_ENDPOINT`、`PANGU_MODEL_NAME`、`PANGU_MODEL_API_KEY`注入，默认MiMo连接接受`MIMO_API_KEY`。

```bash
python3 -B tools/runtime/build_test.py --live-generation
python3 -B tools/runtime/build_test.py --live-partial
python3 -B tools/runtime/build_test.py --live-deferred
python3 -B tools/runtime/build_test.py --live-sequential
python3 -B tools/runtime/build_test.py --live-inputs
python3 -B tools/runtime/build_test.py --live-compaction
python3 -B tools/runtime/test_pangu_pty.py --live
python3 -B tools/runtime/test_product_resources.py --live
python3 -B tools/runtime/test_product_resources.py --live-agent
python3 -B tools/runtime/test_product_resources.py --live-extensions
python3 -B tools/runtime/test_product_resources.py --live-compaction
python3 -B tools/runtime/test_pangu_pty.py --live-settings
python3 -B tools/runtime/test_pangu_pty.py --live-templates
python3 -B tools/runtime/test_pangu_pty.py --live-skills
python3 -B tools/runtime/test_pangu_pty.py --live-paths
python3 -B tools/runtime/test_pangu_pty.py --live-persistent
python3 -B tools/runtime/test_pangu_pty.py --live-crash
python3 -B tools/runtime/test_pangu_pty.py --live-config-recovery
python3 -B tools/runtime/test_pangu_pty.py --live-thinking
python3 -B tools/runtime/test_pangu_pty.py --live-resources
python3 -B tools/runtime/test_pangu_pty.py --live-plugin
```

PTY工具支持`--binary`选择已构建的集成程序。真实凭据文档保存在本地Git私有目录。
