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

## K01/K02 汇编宿主组合（2026-10-26）

`runtime/host.S` 实现静态服务依赖预检/排序、受声明限制的注入、完整发布和逆序协作释放、子级继承/局部替换及子级先释放；实际入口组合 workspace/process/files/tools/model/storage/agent。上游函数、原生落点与剩余能力见核心TODO交接区。C入口仅承担服务工厂组合，宿主纯逻辑为手写汇编。

同一Agent自审控制变更：登记host模块/测试与实际构建输入；新增严格限定 `tools/client/tui/*.py` 的client-tool文件类型，原client-build后缀规则保持原值。固定摘要 `2b7e063ed3ac466b099cc5c8e8039bc2320da1f28442062463f04125e3e3466d`，算法为以下路径排序后的[path,文件SHA256]紧凑UTF-8 JSON再取SHA256：

```text
docs/原生重写路线与迁移TODO.md
governance/assembly-first.json
governance/bootstrap-policy.json
governance/runtime-local-modules.json
src/composition/pangu.c
src/runtime/host.S
src/runtime/host.h
src/runtime/host_layout.h
src/runtime/native_plugin.S
src/runtime/native_plugin.h
src/runtime/session.h
tests/runtime/native_plugin_fixture.S
tests/runtime/test_host.c
tests/runtime/test_native_plugin.c
tools/gates/check.py
tools/gates/test_check.py
tools/runtime/build_pangu.py
tools/runtime/build_test.py
tools/runtime/test_pangu_pty.py
```

已补存储装配：`pg_session_source` 使Journal/内存加载成为实际注入服务；Agent依赖其提供者，Journal与目录由提供者清理。完整Session/Storage行为继续按K03/K04推进。宿主关闭增加整棵子树的回调重入预检，避免子级回调关闭父级时先改动兄弟或父级状态。

风险：描述符和服务为借用，调用者须串行操作并保留至close完成；宿主支持父子作用域继承/局部替换和32层嵌套，动态撤销与完整Session/Task/Storage注入仍待K02。提供者stop报错时宿主保留所有权，不假定已释放。

验证：最终host O0/O2 ASan+UBSan通过；Runtime全专项退出0。新增父子作用域后，固定TUI基线的最终组合离线PTY全部通过（Ctrl-D、信号、resize、工作区启动失败、持久闲置重开及精确termios恢复）；检查器17项自测和assembly-first通过。隔离固定TUI基线的真实MiMo编码工作流与system快照恢复通过；bash恢复场景因模型没有调用工具而失败，未接受该项。当前共享树G00因TUI正在新增的tui_sink.h/tui_md.h/.S/test_tui_md.c尚待登记而失败；这些文件尚未纳入本次已审控制包。自审结论：本批核心候选可用于继续本地集成，完整K02仍未验收。

本轮追加复验：宿主最终O0/O2 ASan/UBSan通过；固定1092bdc TUI加当前核心的离线PTY退出0，包含新加的存储目录缺失/日志损坏启动失败和精确termios恢复。重入父级关闭用例确认父级阶段及子级数量不变。

原生库候选追加：`native_plugin.h/.S` 提供版本化清单与装载/卸载，复用系统动态链接器。实施Agent自审新增产品/测试登记、模块依赖及产品链接选项：仅导出pg_host_*作为插件调用ABI，增加libdl链接；既有预算、检查判定及client/adapter布局保持原值。五种实际DSO的装载/失败清理在O0/O2通过，完整Runtime专项退出0；固定TUI基线产品链接与全部七个Host动态导出检查通过。模块不是进程隔离，描述符及上下文的实例语义由原生插件定义，库所有者须在scope结束后完成pg_native_close。本批结论为同一Agent自审可继续本地开发；K02整体验收与K15仍未完成。

## K03 Document身份与值物化（2026-10-26）

`documents.h/.S` 移植地址身份、scope/history/fork语义、版本检查和迁移值的隔离物化。具体上游函数与原生接口见核心TODO；采用同版本实际Chord/json.ts支持直接Pi函数对照。792个地址字节对照、90个语义/版本/迁移结果对照通过。O0/O2原生专项、JSON故障及最终Runtime全专项退出0；O0/O2 ASan/UBSan专项覆盖输出不变、私有复制、回调引用隔离与释放。地址及物化模块已进入产品源登记，事务消费与通用记录存取仍待K03后续接入。

同一Agent自审：新增四文件，登记documents模块及json-c依赖，把专项加入现有Runtime入口；固定摘要 `6b32519b9f65d2a280e81d602ef2af4f24d6b6e422c6cd3fcc4c67e38410efa6`，为以下排序[path,文件SHA256]紧凑UTF-8 JSON再取SHA256：

```text
docs/原生重写路线与迁移TODO.md
governance/assembly-first.json
governance/bootstrap-policy.json
governance/runtime-local-modules.json
src/runtime/documents.S
src/runtime/documents.h
tests/runtime/test_documents.c
tools/runtime/build_test.py
tools/runtime/test_documents.py
```

边界：输入slice借用有效UTF-8；迁移回调遵守借用输入/owned输出的C ABI契约。当前物化入口处理JSON对象，初始化器准入、完整文档生命周期和事务持久化按K03剩余项继续。自审结论为本地继续实现；G00当前仅报告TUI的四个新增文件待登记，未将该失败表述为通过，K03整体验收未完成。

值准入修正：依据Chord的copy/isJsonValue，在两次物化复制前增加有限数值、祖先环检查，允许共享DAG子对象。新增原生256层容器上限，记录为明确原生差异；公开JSON迭代器结构尺寸由专项静态断言验证。实际Chord六类图对照通过；O0/O2 ASan/UBSan验证环、NaN/±Infinity、共享节点、非法输入/迁移返回及256/257层边界。完整Runtime专项退出0。输入图由调用者持有并负责生命周期；无效图不进入json-c递归复制。本次同一Agent自审更新上述固定摘要，既有消息/工具预算和门禁判定保持原值。

## K03 通用批次提交与核心检查点（2026-10-26）

`records.h/.S` 对应Durable Storage.commit、Session同步adopt与settlement后publish。采用 `session-adoption.test.ts` 的行为场景；准备批次先脱离输入，空批次/只读拒绝保持健康， admitted Storage或adoption失败锁存；已经采纳的状态保留。完整表记录校验、查询与具体Storage提供者继续接入，当前消息投影保持现有入口。

O0/O2 ASan/UBSan专项通过：漏/重复adopt、错误序号、采纳错误、采纳前后Storage错误、回调重入、复制失败/部分复制清理和页尾布局。当前Runtime全专项退出0。单独暂存核心27个文件，暂存清单排除未交接TUI工具的登记；工作区保留该登记与全部TUI候选。独立临时树使用已提交TUI基线，G00、17项检查器自测、assembly-first和产品离线PTY全部通过，包含存储打开/重放失败、持久重开与精确终端恢复。

同一Agent自审本检查点，非独立审批。固定源包摘要 `29b45aa5618e74da645eebfa3070cf054e63d0fc9c1b48f933b1a8df287dadc2`，为暂存改动文件（除本记录）按路径排序的[path, staged文件SHA256]紧凑UTF-8 JSON再取SHA256。新增records产品/测试登记及现有Runtime测试入口，未改变现有消息/工具预算与测试基线。上述源包包含前述宿主、原生插件与Document实现；较早的工作区候选摘要描述各自审查时的文件内容。检查点实际源码可由本次Git提交回溯。K03完整事务、K02其余服务与K15尚未完成。

## K03 内存表记录提供者（2026-10-26）

`record_memory.h/.S` 沿Pi MemoryStorage的prepareCommit/checkGlobalIds/applyPreparedCommit移植四类表写入、独立读取和全局mintId，并接入pg_records_commit。原生索引用有序行数组，先准备/容量预留，再无分配应用；输入、已存状态、读取副本分开持有。conversation/entry不可覆盖，task/submission同类型更新，跨表ID冲突拒绝。整数浮点ID与整数共享命名空间。Document revision和扫描/查询继续在同一提供者补齐。

430次实际Pi MemoryStorage差异操作通过；O0/O2 ASan/UBSan覆盖混合/重复/乱序批次、拒绝无半批、容量增长、序号、mint、独立读取、关闭/重开及关闭后读取副本存活。复制部分结果、暂存分配、已有行扩容、读取复制故障均释放并保持原状态；页尾提供者通过。Runtime全专项退出0。暂存核心候选在已提交TUI基线的独立树中G00、assembly-first与产品链接通过；共享树的TUI候选继续按交接集成。Pi/Harness fetch后旧/新提交均仍为当前固定版本，无新增上游修复。

同一Agent自审：四个新增文件及现有模块/源/测试登记，固定暂存源包摘要 `7afd17e5aa6aa7bbcdabc8b87dd2463a70c830c193b62bef8ff1771a83e1bc1c`；算法为本批9个改动路径排序[path, staged文件SHA256]紧凑UTF-8 JSON再取SHA256，不包含本记录。候选清单排除尚未交接的TUI工具登记，工作区保留其登记。风险/原生差异：此ABI的记录ID为正safe整数；采用有序行存储，查询返回owned JSON；调用方串行访问。消息/工具预算及门禁判定保持原值，K03/K04整体仍未验收。

## K03 分页、分叉可见性与Entry元数据（2026-10-26）

同一内存提供者新增四表过滤分页、旧/新游标顺序、fork-aware Entry/commitSeq和latest head读取。采用Pi的scanStart/nextCursor、visibleEntries/visibleEntriesAscending及entry/findLatestHeadMarker；原生Query/Page明确presence bits、枚举、正整数limit和owned快照。游标用二分定位；降序按需访问祖先，升序收集可见段；点读/head取首项终止。实际缺失祖先边界先复现差异，再修正为按需回溯。

1324次实际Pi操作对照、最终O0/O2 ASan/UBSan和Runtime全专项通过。覆盖全状态/owner/kind/bool过滤、七层fork、inclusive bounds、非法游标、短页/head截点、adopt中查询、Query/Page页尾与关闭后快照；分配失败含段缓存、输出数组、部分页追加和Entry复制，失败输出归零。暂存核心在独立已提交TUI基线树中G00、assembly-first与产品构建通过。

同一Agent自审7文件源包摘要 `b4ffb2ae758635c44cdd49b36aed26ff34cb3b665d6e4aa857ddcd5f8560eca9`，算法为排序[path, staged文件SHA256]紧凑UTF-8 JSON再取SHA256，本记录另行保存。本批只扩展当前提供者与对应测试/接口登记；Native扫描按有序行和可见fork段过滤，性能取决于候选范围。Document revision、文档地址查询与完整Tx暂存继续按K03实现，K03整体验收仍未完成。
