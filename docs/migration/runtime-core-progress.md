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
| 前台 Client | `durable_client.S`：严格UTF-8输入、原子创建任务、忙时拒绝、事件借用、取消、暂停重开与历史；专项ASan/UBSan通过 |
| 模型传输 | `mimo.S`、`mimo_projection.S`、`mimo_stream.S`：OpenAI-compatible SSE、工具批次、增量内容、取消和显式连接注入 |
| 编码工具 | `read_tool.S`、`write_tool.S`、`edit_text.S`、`edit_tool.S`、`bash_tool.S`、`process.S`：真实读改写运行、分页/快照、Unicode匹配、异步进程/取消、原始输出文件 |

## 本批验证与集成状态

- Generation失败响应与下一状态原子入库：按当前策略执行有上限指数退避，deadline持久化；重开、提前唤醒和取消保持任务语义；额度/账户错误与上下文溢出优先排除重试。
- 错误Entry保留于PanGu存储，模型请求按Pi过滤error/aborted/deferred assistant；失败终态通过Client传出错误详情。重试前重新prepare配置。
- Runtime完整O0/O2回归、MiMo汇编ABI/传输/分配专项、隔离产品构建、G00与Assembly检查通过；SQLite/JSONL各通过一次故障注入后的真实DeepSeek工具闭环。
- TUI交接的安全输出、工具预览/尾部/展开及历史/搜索/阅读候选已纳入固定集成树；Clang九客户端程序、二十四组PTY通过。真实DeepSeek整机完成四工具编码、PanGu身份、重开续聊与两种执行中退出恢复。U03/U04/U07的完整验收仍按TUI TODO。

## 当前接口边界

模型投影为64条消息、单条4096字节；完整上下文/压缩按K12完成。前台Client当前接受单个运行，队列/Submission与多会话按K10接入。K05/K07继续补齐模型协议、分类错误/重试、deferred/partial、顺序工具与hooks；项目配置和提示资源恢复按K11完成。全部剩余行为保留在核心TODO。

## 复验入口

```bash
python3 -B tools/runtime/build_test.py
python3 -B tools/runtime/build_pangu.py --output /tmp/pangu-native/pangu
python3 -B tools/runtime/test_pangu_pty.py
```

真实模型通过`PANGU_MODEL_ENDPOINT`、`PANGU_MODEL_NAME`、`PANGU_MODEL_API_KEY`注入，默认MiMo连接接受`MIMO_API_KEY`。

```bash
python3 -B tools/runtime/build_test.py --live-generation
python3 -B tools/runtime/test_pangu_pty.py --live
python3 -B tools/runtime/test_pangu_pty.py --live-persistent
python3 -B tools/runtime/test_pangu_pty.py --live-crash
```

PTY工具支持`--binary`选择已构建的集成程序。真实凭据文档保存在本地Git私有目录。
