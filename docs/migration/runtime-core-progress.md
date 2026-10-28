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
| Generation/Tool | `durable_generation.S`、`durable_tool.S`：保存配置、模型请求、工具轮次、原子结果、副作用恢复；真实DeepSeek在SQLite/JSONL通过 |
| Submission/输入队列 | `durable_inputs.S`：去重、admission、one/all steer/follow-up边界、展示写入、reset/stale、失败策略、撤回/状态与持久结算；产品导出inputs服务 |
| 前台 Client | `durable_client.S`：严格UTF-8输入、原子创建任务、忙时拒绝、事件借用、取消、暂停重开与历史；专项ASan/UBSan通过 |
| 模型传输 | `mimo.S`、`mimo_projection.S`、`mimo_stream.S`：OpenAI-compatible SSE、工具批次、增量内容、取消和显式连接注入 |
| 设置与资源路径 | `settings.S`：用户/项目JSON层、递归覆盖与错误诊断、独立所有权；配置skills/prompts的普通文件/目录进入技能发现与模板命令，路径相对所属配置目录解析 |
| 提示模板/显式技能 | `prompt_templates.S`：用户/项目Markdown模板、引号参数、位置/全部/默认/切片参数单次替换；前台/模板和/skill:name展开后进入持久Submission，显式技能读取当前正文 |
| 技能资源 | `skills.S`、`skills_posix.c`：用户/项目默认发现、根技能优先、名称/规范路径去重、YAML元数据、校验诊断与转义XML；提示列表→模型read技能/相对资源→write真实闭环 |
| 编码工具 | `read_tool.S`、`write_tool.S`、`edit_text.S`、`edit_tool.S`、`bash_tool.S`、`process.S`：真实读改写运行、分页/快照、Unicode匹配、异步进程/取消、原始输出文件 |

## 本批验证与集成状态

- 用户agent目录/settings.json与项目.pangu/settings.json读取独立层和递归覆盖结果；skills/prompts普通文件/目录按各层所属目录解析，接通既有发现/命令。错误配置层产生诊断并保持另一层可用。
- 设置/路径/实际消费者专项ASan/UBSan、Runtime整批回归与最终产品构建通过；原始JSON模式补充拒绝Markdown frontmatter。Assembly与隔离核心候选G00通过；共享树另有TUI生成缓存的登记提示，保持其工作原位。
- 真实DeepSeek完成配置技能发现/read/write，以及用户配置模板目录和项目相对模板文件的实际提交。资源选择模式、忽略规则、刷新和完整设置消费者继续随K11推进。

## 当前接口边界

模型投影为64条消息、单条4096字节；完整上下文/压缩按K12完成。前台Client当前按调度空闲输出运行终态；独立Inputs服务和可选TUI操作表已提供队列/Submission；TUI交互、多会话及完整查询按K10/K13接入。K05/K07继续补齐模型协议、分类错误/重试、deferred/partial、顺序工具与hooks；项目配置和提示资源恢复按K11完成。全部剩余行为保留在核心TODO。

## 复验入口

```bash
python3 -B tools/runtime/build_test.py
python3 -B tools/runtime/build_pangu.py --output /tmp/pangu-native/pangu
python3 -B tools/runtime/test_pangu_pty.py
```

真实模型通过`PANGU_MODEL_ENDPOINT`、`PANGU_MODEL_NAME`、`PANGU_MODEL_API_KEY`注入，默认MiMo连接接受`MIMO_API_KEY`。

```bash
python3 -B tools/runtime/build_test.py --live-generation
python3 -B tools/runtime/build_test.py --live-partial
python3 -B tools/runtime/build_test.py --live-deferred
python3 -B tools/runtime/build_test.py --live-sequential
python3 -B tools/runtime/build_test.py --live-inputs
python3 -B tools/runtime/test_pangu_pty.py --live
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
