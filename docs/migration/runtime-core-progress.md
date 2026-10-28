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
| Task/Scheduler | `task_runner.S`：phase/checkpoint、sleep/wake、发现/公平推进、所有权/等待图、取消级联、暂停与进程恢复 |
| 上下文压缩 | `durable_compaction.S`：UTF-16估算/合法Entry切点、结构化摘要、固定范围与配置、持久阶段恢复、原子Submission/任务终态；完整摘要输入采用owned JSON与借用模型视图，Host服务和可选TUI表；容量阈值触发Generation-owned压缩、原子直接Entry放置、等待父任务重开及恢复生成，后台阈值触发/去重/空闲调度 |
| 系统上下文 | `durable_generation.S`：当前OpenAI路径折叠system正文和命名section，覆盖保序、null删除/重加追加，结果进入实际模型提示和历史恢复 |
| 上下文投影 | `durable_generation.S`：请求/历史共用cutoff分页、最新head优先、旧head编辑保留、omit/replace最后生效；超过300条记录与两后端重开通过；工具结果按调用次序整理，首个同ID匹配、孤立过滤与临时缺失回执 |
| Generation/Tool | `durable_generation.S`、`durable_tool.S`：保存配置、模型请求、工具轮次、原子结果、副作用恢复；真实DeepSeek在SQLite/JSONL通过 |
| Submission/输入队列 | `durable_inputs.S`：去重、admission、one/all steer/follow-up边界、展示写入、reset/stale、失败策略、撤回/状态与持久结算；产品导出inputs服务 |
| 前台 Client | `durable_client.S`：严格UTF-8输入、原子创建任务、忙时拒绝、事件借用、取消、暂停重开与历史；专项ASan/UBSan通过 |
| 模型元数据与压缩策略 | `models.S`、`models_data.S`：固定Pi目录、用户models.json元数据/覆盖、端点/provider选择、默认model.metadata与逐字段compaction.modelOverrides；公开值独立持有 |
| 会话用量 | `usage.S`：pangu.usage按models/tools累计，Generation/工具结果/摘要尝试同批提交；响应模型身份优先，计数及已报告cost、可选reasoning/cacheWrite1h，Host usage读取与重开 |
| 模型传输 | `mimo.S`、`mimo_projection.S`、`mimo_stream.S`：OpenAI-compatible SSE、工具批次、增量内容、取消和显式连接注入 |
| 资源选择 | `resource_select.S`：作用域`!`/`+`/`-`规则、文件/SKILL父目录变体、常见glob/brace/range、显式与项目优先；筛选先于名称去重，排除路径不由自动发现重新启用 |
| 资源生命周期 | `resources.S`：Host resources服务、整体候选采纳/失败保留、空闲刷新、独立版本快照与前台展开；可选TUI资源操作表已提供 |
| 设置与资源路径 | `settings.S`：用户/项目JSON层、递归覆盖与错误诊断、独立所有权；配置skills/prompts的普通文件/目录进入技能发现与模板命令，路径相对所属配置目录解析 |
| 提示模板/显式技能 | `prompt_templates.S`：用户/项目Markdown模板、引号参数、位置/全部/默认/切片参数单次替换；前台/模板和/skill:name展开后进入持久Submission，显式技能读取当前正文 |
| 技能资源 | `skills.S`、`skills_posix.c`：用户/项目默认发现、根技能优先、名称/规范路径去重、YAML元数据、校验诊断与转义XML；提示列表→模型read技能/相对资源→write真实闭环 |
| 编码工具 | `read_tool.S`、`write_tool.S`、`edit_text.S`、`edit_tool.S`、`bash_tool.S`、`process.S`：真实读改写运行、分页/快照、Unicode匹配、异步进程/取消、原始输出文件 |

## 本批验证与集成状态

- Host observation服务和可选TUI绑定提供已提交{live,usage}快照，读取不推进Scheduler、不写记录；返回JSON由调用者释放。
- SQLite/JSONL重开后的累计用量及只读序列检查、实际Host初始/压缩终态快照、TUI绑定/解绑边界通过。
- Runtime整批回归和实际产品compaction-hooks场景通过；完整增量事件继续按K13接入。

## 当前接口边界

正文输入、Generation正文累积和摘要保存/放置已动态化。provider单次事件、reasoning、工具参数/结果仍有现有边界，按K05/K08完成；K12继续retry期间queued摘要等剩余组合，K13接入客户端用量及完整Live事件/观察。TUI消费端的旧历史校验与显示预算由TUI线接入动态入口。其他差异仍保留在核心TODO。

## 复验入口

```bash
python3 -B tools/runtime/build_test.py
python3 -B tools/runtime/build_pangu.py --output /tmp/pangu-native/pangu
python3 -B tools/runtime/test_pangu_pty.py
python3 -B tools/runtime/test_product_resources.py
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
