# 当前原生实现记录

当前执行批次与下一交付物见[核心 TODO 交接区](../原生重写路线与迁移TODO.md#核心交接区)，本文件保存已验证能力基线。

记录基线：`58d4e20`（2026-10-26）。实现按 [核心 TODO](../原生重写路线与迁移TODO.md) 与 [TUI TODO](../TUI移植TODO.md) 接续。下表记录已经实际验证的能力，可直接复用。

| 能力 | 实际入口与验证范围 |
|---|---|
| Loop/Session/Submission | 手写 `agent.S`、`session.S`、`submissions.S`；提交确认后采纳、requestId去重、唯一终态、取消/暂停、连续会话 |
| 持久化 | `journal.S`、`journal_posix.c`、`profile.S`；v1–v5帧、锁/fsync、截断/损坏拒绝、配置身份核对和跨进程恢复；格式见 [日志协议](native-journal-format.md) |
| 批量工具 | 最多8调用原子提交、顺序结果、safe/unsafe恢复、取消与异步清理；200个真实日志/进程退出组合通过 |
| MiMo | 手写 `mimo.S`、`mimo_projection.S`、`mimo_stream.S`；真实SSE、分片工具参数、批量工具、取消、持久恢复 |
| read | 手写分页/完整快照；1-based offset/limit，4095字节/2000行页、1 MiB扫描边界；真实续读与2000行后编辑通过 |
| write/edit | 手写参数准备、校验/授权/执行、目标匹配、BOM/换行/未改行保留；1300文本案例、91参数案例与Pi对照；真实写入/编辑/回读和unsafe恢复 |
| edit定位 | `58d4e20`：替换块数、按实际原文/结果公共前缀计算的首个LF行；真实MiMo第2001行结果核对通过 |
| bash | 手写进程服务与工具；输出双流UTF-8、超时/取消、3072字节/2000行尾部、0600原始输出文件、16 MiB捕获界限；真实命令与中断恢复 |
| 提示文件 | `prompt.S`与显式文件配置；真实暂停后删除源文件、重开使用已提交提示、完成后缺失源启动失败且日志不变 |
| 容量前置 | `6d26877`：工具执行前为全部结果及后续回复预留槽；真实MiMo恰好可容纳时写入并回复，少一槽时零工具执行 |
| 薄TUI | 手写输入/状态/渲染/桥接；真实模型对话/工具/取消/持久历史，PTY退出/信号/resize/termios恢复 |

## 当前边界与待核对项

- 会话内存当前为64条消息、单条4096字节存储；当前文本工具按该边界工作。K03/K08/K12负责完整记录、工具和长上下文行为。
- 主要产品模型提供者为MiMo Token Plan CN；K05负责完整对话模型层与协议适配。
- `--live-prompt-file` 的严格口令遵循仍可失败。请求和journal中的system已核对，实际Pi Agent也复现严格输出违背；K05/K11继续区分请求构造与模型行为。
- 当前工作区的TUI输入/渲染候选归U01/U02，集成以实际交接和测试结果更新。

## 复验入口

```bash
python3 -B tools/runtime/build_test.py
python3 -B tools/runtime/test_mimo_asm.py
python3 -B tools/runtime/test_tools_asm.py
bash tools/client/tui/build.sh run-tests
python3 -B tools/runtime/build_pangu.py --output /tmp/pangu-native/pangu
python3 -B tools/runtime/test_pangu_pty.py
```

真实模型入口使用环境中的 `MIMO_API_KEY`：

```bash
python3 -B tools/runtime/build_test.py --live-capacity
python3 -B tools/runtime/build_test.py --live-snapshot-edit
python3 -B tools/runtime/test_pangu_pty.py --live-workflow
python3 -B tools/runtime/test_pangu_pty.py --live-batch
python3 -B tools/runtime/test_pangu_pty.py --live-prompt-snapshot
```

最近实现提交 `c314834`、`f66f2a0`、`6d26877`、`58d4e20` 保存对应测试、源代码和实际验证记录。当前批次只在各自TODO交接区维护；此表在集成能力变化时更新。

## 已提交Durable与宿主能力

以下为截至`3e78a14`的模块能力摘要，来源、具体变更与当时实际验证结果由Git保存。完整移植状态以核心TODO的集成验收为准。

| 能力组 | 实际入口 |
|---|---|
| 插件组合 | `host.S`、`native_plugin.S`与`src/composition/`：依赖排序、父子作用域、局部替换、协作清理、本地库入口 |
| 记录与事务 | `records.S`、`record_memory.S`：conversation/entry/task/submission、原子准备/采纳、分页与索引；类型化候选、提交屏障、时间戳与所有权 |
| 文档 | `documents.S`、`document_tx.S`、`document_reader.S`：地址身份、版本/迁移、revision重放、编辑/退休/fork-copy、缓存及当前/历史读取 |
| 延迟持久化 | `pending.S`、`holds.S`、`hold_timer.S`：变更折叠、pin、定时保存、混合提交与失败清理 |
| Session与观察 | `durable_session.S`：事务准入、分阶段关闭、首因保留、通知队列、同步观察、Watch获取/交付/取消与readOnLine |
| SQLite存储 | `record_sqlite.S/h`：原生批次事务、持久后采纳、重开恢复、只读与过期写入者拒绝；通过现有Storage/Session接口提交，查询复用恢复后的Memory |

这些模块已有专项、来源差异及Runtime集成验证记录；B01负责接通完整持久后端与重开，B02负责持久任务和整机执行链。当前未提交的Session失败入口候选由B01统一核对。

## 历史探针清理（2026-10-27）

删除15个包含`cases.h/graph-cases.h`的历史探针目录，共71文件、24,845,721字节；核对产品源、工具/构建配置及目录外源码均无消费者，同步移除bootstrap登记。当前Runtime/TUI回归测试保留。G00通过；当前共享树产品构建在TUI候选`tui_view.S:426/439`的内存操作数表达式报错，本次保留该候选并记录构建失败。

## 全工作区交接提交（2026-10-27）

接入TUI Markdown产品源登记与Session失败入口导出，修正`tui_view.S`两处Clang常量表达式写法。G00、产品构建和Session/内存记录专项（O0 ASan/UBSan）通过。当前TUI专项在`test_tui_md`的paragraphs用例后崩溃；PTY在multiline_edit_submit收到目标文本前进程退出，均为本轮真实失败，后续TUI修复以此为起点。本次按用户要求保存并推送全部核心/TUI候选。

## B01 SQLite接入（2026-10-27）

`record_memory.S`在完整准备后增加持久化屏障，拒绝时保持记录/序号，成功后直接应用并通知；默认内存提交沿用同一实现。`record_sqlite.S`通过成熟SQLite/json-c C ABI实现原生提交与顺序恢复。实际Session混合事务重开结果与固定Pi SQLite对照通过，Runtime O0/O2、SQLite ASan/UBSan专项及产品构建通过。原生SQLite文件为批次日志，回放成本见核心TODO当前交接；完整K04验收继续由B01完成。

## B01 JSONL与PanGu身份（2026-10-27）

`record_jsonl.S`接通原生Storage/Session提交、task/doc sidecar和主提交标记、独占写租约、撕裂/未确认尾部恢复、终态任务和当前checkpoint恢复维护；平台C仅处理目录布局。Memory持久化屏障借用已解析copy的分离批次，默认内存路径保持原实现。Runtime O0/O2、JSONL ASan/UBSan、产品构建及独立上游行为对照通过；当前实际差异保留在核心TODO。PanGu使用自身数据协议与`.pangu`命名空间，提示词继承参考机制并适配PanGu身份与实际操作入口。

## B01完整Session关闭与即时维护（2026-10-27）

SQLite/JSONL与实际Durable Session、Document Tx、holds、timer及cache组合验证通过：延迟内容关闭保存、混合批次、已接收事务在关闭中结算、已入队timer排空、失败关闭与版本迁移重开。JSONL在采纳后回收终态Task/current-only checkpoint，临时重写失败保持提交成功并由重开维护重试。验证与当前批次见核心TODO交接区。
