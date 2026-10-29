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
| Generation hooks与请求准入 | `durable_generation.S`/`extensions.S`：beforeRequest临时消息、afterResponse/afterTools观察、onYield原子续接与队列优先；phase固定扩展，挂起/取消释放；声明名单保存到请求及并行/串行Tool输入，效果前拒绝未提供工具 |
| 嵌套Live生命周期 | `tool_live.S`/`durable_tool.S`/`durable_client.S`：pending随创建、running随意图、done随结果；开始事件取最终checkpoint，执行前取消只结束；OUTPUT发布实时块并合并持久UTF-8窗口/丢弃量，done清除进度，Task/Live保留父调用关系 |
| 工具输出合并 | `tool_live.S`以a或t+a提交文本增量，Tool按实际原生Op payload与100KiB/s、至少100ms安排后续刷新；同步details/diagnostic确认后共用暂停期限。模型/嵌套10KB追加及Unicode滚动、零变化零提交、取消重开通过；统一异步元数据调度仍待补齐 |
| 跨块流窗口 | `output_window.S`保存清洗前raw margin及全流计数，head/tail逐块快照与固定Chord的220组流直接一致；Tool已替换旧拼接路径，释放与恢复沿调用级生命周期 |
| 输出清洗与结果元数据 | `output_window.S`清理流窗口/事件控制字符，2464组上游预算/清洗对照；Tool诊断动态渲染支持7000字节，单调计时duration进入结果/嵌套返回/摘要 |
| 工具输出预算 | `output_window.S`移植Chord完整行/UTF-8 head/tail；工具定义携带bytes/lines/retain，默认50KiB/2000行/head、bash tail；动态Tool缓冲、70KiB/零预算、累计丢弃/截断诊断和恢复通过 |
| 工具结果控制 | `durable_tool.S`/`durable_generation.S`/`agent_state.S`：control随模型工具receipt提交，工具轮次合并addTools、最后handoff和全体terminate；工具选择、reset新head和final/postTools队列边界同事务，真实DeepSeek写入后直接结束，SQLite/JSONL增量配置回滚/重开通过 |
| 工具图片请求映射 | `request_context.S`/`mimo_projection.S`：文本与图片分开，连续工具图片按顺序合为后置user/image_url消息，模型input配置控制附图，纯图片使用明确占位；实际编码专项通过，目录能力/用户图片及提供者兼容项仍待补齐 |
| 工具多内容块 | `durable_tool.S`/`extensions.S`：afterTool接受text/image数组，整体文本预算裁切时保留图片顺序，Entry与嵌套程序值在诊断渲染前分别复制；无截断保留各块，空/单text/单image/多块沿Durable输出值语义 |
| 结构化工具契约 | `durable_tool.S`按固定调用声明处理无声明/缺失/不匹配，嵌套错误与模型非致命报告分开；`json_schema_bridge.cpp`仅桥接固定Valijson Draft-7强类型校验，bash声明接通；无效契约在execute前拒绝并完整释放，完整TypeBox语料差异仍待完成 |
| 嵌套结果与错误摘要 | `durable_tool.S`：done槽保存至多500 UTF-16单元的诊断优先错误摘要，保留有效UTF-8；成功/空错误省略字段。省略空输出保存空Content，默认程序值取实际输出而非harness诊断；八组错误/Unicode/NUL及空结果专项通过 |
| 工具最终结果 | `tools.S`/`durable_tool.S`：显式省略output采用报告窗口，零长度保持空结果；afterTool改写文本后去除旧窗口截断提示。返回时撤销执行API并固定duration，后置hook/close等待不计入执行耗时；实际省略/空/错误/替换与200ms hook专项通过 |
| 工具输出API | `tool_execution.S`/`durable_tool.S`：output、retained_output、output_window接入同一调用级流，读取尚未持久刷新的独立快照，tail支持OutputSkip全流计数，head与执行生命周期拒绝边界；固定Chord 260组流及真实产品验证通过 |
| 工具报告进度 | `tool_execution.S`公开details/diagnostic持久提交，模型/嵌套工具共用Live槽和观察；最终结果合并报告details/诊断，中断可恢复；Generation结算清理前轮模型工具槽 |
| 嵌套异常与后代结算 | `tool_execution.S`/`durable_client.S`按Task outcome提供缺结果回退和结束事件；模型/nested父工具结算均清理完整后代Live树，保留无关分支；Task查询支持调度推进中的临时空槽 |
| 中断结果与缺定义取消 | `tool_live.S`/`durable_tool.S`/`task_runner.S`：恢复已提交部分输出/元数据后结算；restart缺定义等待安装，普通取消缺定义任务结算orphaned/missing_task；真实挂起/重开3000字节输出与取消场景通过 |
| 嵌套重启策略 | `task_runner.S`/`tool_execution.S`/`durable_tool.S`：unsafe调用子任务重开先放弃、restart向普通后代传播，已启动/未启动分诊断；safe重跑沿原键恢复并先清空进度，产品父子孙任务与无副作用恢复通过 |
| 工具内嵌套执行 | `tool_execution.S`与调用级工厂：executeTool start/poll、memo、显式/自动键和持久子工具；实际write→read、safe重开复用、结构化返回/诊断、取消及实例释放通过；DSO可通过公开动态符号调用 |
| 工具调用作用域 | `durable_tool.S`/`extensions.S`与产品组合器：按Task隔离扩展快照/gate/hooks/执行租约，扩展父工具挂起时owned子工具真实写入并回传，父子取消独立释放；共享基础提供者/异步上下文互斥 |
| Tool hooks | `extensions.S`/`durable_tool.S`：原生beforeTool/afterTool进入意图前与结果提交前，参数与结果按选中扩展顺序传递，拦截/异常、两次校验、pending/取消/重开；工具结果保留最终参数及hook元数据 |
| 扩展选择与组合 | `extensions.S`：extensions.registry的稳定名称选择、同名工具/section保序覆盖、同步wrapper与失败报告、标签渲染和请求固定选择；工具分派保留未改动的注入提供者，扩展实现进入同一Durable Tool链 |
| Agent配置 | `agent_state.S`：Host agent.state事务写pangu.agent，稳定名称配置、字段清除/去重提交、独立读取；instructions在资源提示后进入Generation请求，tools/modelTools筛选模型声明，执行gate保持原提供者lease并拒绝禁用名字 |
| 扩展任务 | `task_provider.S`：可选Host task.definitions校验/查找，保留内置任务、拒绝重名/无效定义；原生DSO通过公开commit/commit_with执行checkpoint与终态，Scheduler重开后继续，消费者清理先于库卸载 |
| Task/Scheduler | `task_runner.S`：phase/checkpoint、sleep/wake、发现/公平推进、所有权/等待图、取消级联、暂停与进程恢复 |
| 上下文压缩 | `durable_compaction.S`：UTF-16估算/合法Entry切点、结构化摘要、固定范围与配置、持久阶段恢复、原子Submission/任务终态；完整摘要输入采用owned JSON与借用模型视图，Host服务和可选TUI表；容量阈值触发Generation-owned压缩、原子直接Entry放置、等待父任务重开及恢复生成，后台阈值触发/去重/空闲调度 |
| 提示计划 | `system_prompt.S`：已展示section/工具声明重放、最小变化/顺序重排、head后基线与omit；Generation同事务追加pangu.system和request cutoff；异步render挂起/续接、取消和错误保留旧文本 |
| 系统上下文 | `durable_generation.S`：当前OpenAI路径折叠system正文和命名section，覆盖保序、null删除/重加追加，结果进入实际模型提示和历史恢复 |
| 上下文投影 | `durable_generation.S`：请求/历史共用cutoff分页、最新head优先、旧head编辑保留、omit/replace最后生效；超过300条记录与两后端重开通过；工具结果按调用次序整理，首个同ID匹配、孤立过滤与临时缺失回执 |
| 扩展组合与生命周期 | `extensions.S`、`extension_registry.S`：稳定名称选择、工具/section/wrap及Generation/Tool/Compaction hooks；候选原子发布、同名替换、撤销与phase快照，所有者引用延迟释放，Host关闭等待消费者；自定义Task按全安装集合解析，Runner持有invocation/失败迁移定义引用并在phase边界接手 |
| Generation/Tool | `durable_generation.S`、`durable_tool.S`：保存配置、模型请求、工具轮次、原子结果、副作用恢复；真实DeepSeek在SQLite/JSONL通过 |
| Submission/输入队列 | `durable_inputs.S`：去重、admission、one/all steer/follow-up边界、展示写入、reset/stale、失败策略、撤回/状态与持久结算；产品导出inputs服务 |
| 任务会话创建 | `task_access.S`/`agent_state.S`：phase能力校验→runner commit_with→Harness原子stage，显式owner/new/fork、配置与init；真实产品任务验证回滚、归属及过期能力拒绝 |
| 会话创建与前台 | `agent_state.S`/`durable_session.S`/`durable_client.S`：原子默认文档、Agent分叉点/owner继承、agent/init、独立持久UUIDv7与旧会话补建；产品新建/分叉/选择/历史入口，真实DeepSeek在分叉会话写文件；SQLite/JSONL回滚与重开 |
| 模型元数据与压缩策略 | `models.S`、`models_data.S`：固定Pi目录、用户models.json元数据/覆盖、端点/provider选择、默认model.metadata与逐字段compaction.modelOverrides；公开值独立持有 |
| 上下文重置 | `durable_client.S`：conversation.actions/TUI独立reset入口，handoff/空上下文通过现有Submission入队和head切换，保留持久ID用于查询；忙时队列与两后端恢复验证 |
| 会话目录 | `conversation_view.S`/`durable_client.S`：Host/TUI会话列表/记录/命名，ID顺序/owner筛选/游标分页、独立元数据Entry、Unicode与换行规范、名称观察及分叉/清空隔离；两后端重开 |
| 会话视图 | `conversation_view.S`：conversation.view Host/TUI只读入口，统一会话/原始有效Entry/已有Live与UsageDoc/queued Submission，readOnLine保护与分页、重开 |
| 会话用量 | `usage.S`：pangu.usage按models/tools累计，Generation/工具结果/摘要尝试同批提交；响应模型身份优先，计数及已报告cost、可选reasoning/cacheWrite1h，Host usage读取与重开 |
| 模型传输 | `mimo.S`、`mimo_projection.S`、`mimo_stream.S`：OpenAI-compatible SSE、工具批次、增量内容、取消和显式连接注入；参数缓冲清理保留工具注册表，连续请求携带当前声明 |
| 资源选择 | `resource_select.S`：作用域`!`/`+`/`-`规则、文件/SKILL父目录变体、常见glob/brace/range、显式与项目优先；筛选先于名称去重，排除路径不由自动发现重新启用 |
| 资源生命周期 | `resources.S`：Host resources服务、整体候选采纳/失败保留、空闲刷新、独立版本快照与前台展开；可选TUI资源操作表已提供 |
| 设置与资源路径 | `settings.S`：用户/项目JSON层、递归覆盖与错误诊断、独立所有权；配置skills/prompts的普通文件/目录进入技能发现与模板命令，路径相对所属配置目录解析 |
| 提示模板/显式技能 | `prompt_templates.S`：用户/项目Markdown模板、引号参数、位置/全部/默认/切片参数单次替换；前台/模板和/skill:name展开后进入持久Submission，显式技能读取当前正文 |
| 技能资源 | `skills.S`、`skills_posix.c`：用户/项目默认发现、根技能优先、名称/规范路径去重、YAML元数据、校验诊断与转义XML；提示列表→模型read技能/相对资源→write真实闭环 |
| 编码工具 | `read_tool.S`、`write_tool.S`、`edit_text.S`、`edit_tool.S`、`bash_tool.S`、`process.S`：真实读改写运行；read按Pi Durable的50 KiB/2000行分页且无固定扫描上限，模型可见读取剥离UTF-8 BOM、保留NUL并按TextDecoder规则替换非法UTF-8；Edit内部完整快照按需扩容；Assembly文本工作区、diff/patch和内部write请求随文件大小分配，196 KiB文件端到端编辑逐字节回读；模型工具JSON增量动态累积并贯通Generation/工具分派，write完整落盘64 KiB内容；Unicode匹配、异步进程/取消、原始输出文件 |

## 本批验证与集成状态

- Client空闲选择会话时，先核对目标、加载Agent配置和准备输入/事件基线，再替换前台状态；失败保留旧会话/事件，共享Scheduler持续有效。Host的`conversation.client`和TUI可选`pg_tui_runtime_conversations`提供select/current；模型文本及工具进度按持久Task归属过滤，避免跨会话显示。
- SQLite/JSONL专项ASan/UBSan覆盖切换、失败保留、忙时/缺失/关闭拒绝、重开后的冷配置加载与只读切换；Durable/Client相关回归、产品Host/资源组合ASan/UBSan、产品构建与Assembly检查通过。
- 后续组合完整Harness创建/初始化与动态历史映射；前台控件消费由TUI线完成。复验：`python3 -B tools/runtime/build_test.py --conversations`。

## 当前接口边界

正文输入、Generation正文累积和摘要保存/放置已动态化；单次provider事件、reasoning与工具结果仍受当前边界约束。工具JSON参数动态累积至128 KiB，Edit内部完整文件读改写随文件大小分配。K12继续retry期间queued摘要等剩余组合，K13补齐结构化message_update、report及剩余Live事件/观察。TUI消费端的旧历史校验与显示预算由TUI线接入动态入口。其他差异仍保留在核心TODO。

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
python3 -B tools/runtime/test_product_resources.py --live-conversations
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
