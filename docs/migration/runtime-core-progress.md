# 当前原生实现记录

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

最近实现提交 `c314834`、`f66f2a0`、`6d26877`、`58d4e20` 保存对应测试、源代码和实际验证记录。后续批次在各自TODO的交接区记录，再在此更新已经核对的能力。

## 文档与登记核对（2026-10-26）

主文档、开发入口、工程守卫、两份TODO和文件清单由同一Agent自审。核对来源/步骤/文件归属、当前实现基线及链接；G00、assembly-first和16项检查器自测通过。产品源码、工具链、检查规则与预算保持当前值。固定包摘要（上述六个文件与六个删除路径，排序[path, SHA256或null]紧凑UTF-8 JSON）：`c2a98416d2a2532da265a0917b3077d98de6a977930b851f6f3b72fdf98da3fe`。
