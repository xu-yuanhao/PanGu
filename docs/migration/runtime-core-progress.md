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

## K03 Document delta重放候选（2026-10-26）

`delta.h/.S` 对照实际Chord的decoded Op校验/路径安全/applyImmutable/applyImmutableBatches，实现r/s/d/a/t/p/m和多批最终值重放；json-c持有私有工作树，utf8proc辅助UTF-16计数到UTF-8边界转换。调用者值与ops保持不变，失败输出为空；Source pending.merge的同路径追加语义用于连续a一次拼接。

449个实际Chord单/多批结果一致，包含10,000次连续追加；另三条原生ABI差异单列为拒绝验证（UTF-16内部截点、NUL属性路径、超safe数字路径），未计入兼容通过，继续K03/K15核对。O0/O2 ASan/UBSan和直接分配/复制/插入逐点故障通过，最终Runtime全专项退出0。暂存核心在已提交TUI基线的独立树中G00、assembly-first和产品构建通过。640KB/10,000追加的本机单次重放观测由0.3004s降至0.0118s，作为本批优化观测，未设性能门禁。

同一Agent自审9文件源包摘要 `ea3a76d75a8059276f3c7784cbe348f0a8eb6316dc9a867ae30dfc3f2a7f185d`（排序[path, staged文件SHA256]紧凑UTF-8 JSON再取SHA256；本记录另存）。四个新增文件与实际依赖/构建/测试入口登记，复用既有json-c/utf8proc/libc。风险/差异：当前API接受C-string属性路径和safe数字路径、输出保持UTF-8标量；Doc revision存取及事务仍按K03推进。候选清单排除未交接TUI工具登记，工作区保留其登记与候选，K03整体验收仍未完成。

## K03 已提交revision读取（2026-10-26）

- 来源：Pi `d5b3de22` 的 `MemoryStorage.materializeDocument/isAliveAt/isCurrentOnly/documentDeltaBatches`，实际调用 `document`；阅读 `testing/storage-conformance.ts` 的历史、长尾和拷贝场景。
- 实现：现有delta模块新增 `pg_document_revision_read`，手写Assembly选择current/as-of生命周期、最近base和同版本delta尾；调用者借用revision数组，结果独立持有。原生有序revision布局与safe正整数版本明确在头文件。Document写入、地址索引与事务尚待接入。
- 实际验证：96条读取与实际Pi MemoryStorage对照一致；原449条Chord对照及3条已记录原生差异检查仍成立。O0/O2 ASan/UBSan与逐直接分配/插入/部分复制失败检查通过，完整Runtime退出0；Assembly inventory通过。共享G00仍因TUI的4个未登记候选文件失败。
- 控制登记自审：同一实施Agent自审；只扩展现有模块导出/职责及参考语义，未调整门禁或预算。6个候选文件（本记录之前暂存树）的路径/内容SHA256清单摘要为 `7d7aef696589affb0f3440f4451caecc8b8437b59219817de668d60799d4823c`。风险为新接口尚待Storage接入，三条delta ABI差异仍在K03/K15；结论为本地集成候选通过，不代表完整K03完成。

## K03 内存Document原子写入与读取（2026-10-26）

- 对照：Pi `d5b3de22` 的 `storage/memory.ts:prepareDocumentActions/checkDocumentActions/applyDocumentActions/materializeDocument`，调用者 `prepareCommit/applyPreparedCommit/document`，以及 `testing/storage-conformance.ts` 的历史、长尾和拷贝场景。沿用上一批固定参考；当前Pi/Harness远端提交与固定版本相同。
- 实现：现有 `record_memory.S` 手写Document动作合并、全局ID/类型检查、live地址唯一性、版本和历史保留、创建/退役序号及公开 `pg_record_memory_document`。准备阶段为受影响文档生成私有record与revision数组（借用未变revision引用），与普通表一同预留容量后无分配应用；失败不修改可见记录/ID/序号。无需复制所有已存文档值。既有48字节Memory及Query/Page ABI不变。
- 验证：2048条实际MemoryStorage混合操作对照一致；包括Document四种scope/history组合、连续delta、base版本切换、live冲突、退役并重建、重复动作、跨表失败和0～120历史截点。原1324条记录行为仍覆盖。O0/O2 ASan/UBSan专项通过，创建、delta更新+task、退役重建及snapshot读取枚举直接分配/插入/部分复制故障，检查所有权、输出、记录与计数不变；完整Runtime最终树退出0。隔离暂存核心树G00、Assembly inventory与实际产品构建通过；共享树G00仍报告TUI的4个未登记候选文件。
- 同一实施Agent自审：新增实际Document导出及delta依赖登记，构建补链接依赖和故障注入；未修改门禁/预算。8个候选路径与暂存内容SHA256清单摘要（本记录加入前）`33724d06f0441ecfb0e67f11d99406ee82378540e53bfa58086a88f209ec631f`。结论为本地集成候选通过；尚待Document.copy、地址find/scan与完整Tx，K03未完成。JSON delta已记录的三条原生ABI差异仍待解决。

## K03 Document地址查找与分页（2026-10-26）

- 来源：Pi `d5b3de22` 的 `MemoryStorage.findDocument/scanDocuments/isAliveAt`、`addressKey/scopeKey/page`；`testing/storage-conformance.ts` 的同址退役重建与NUL身份字段场景。2026-10-26 fetch Pi/Harness，HEAD与远端仍分别为固定 `d5b3de22` / `22ddee37`，无新修复待采用。
- 实现：现有provider新增手写Assembly `pg_record_memory_find_document/scan_documents`，复用原地址类型与Page，DocumentQuery布局64字节；精确scope/owner/kind/可选key身份，生命周期成员关系不要求content历史可读，返回独立owned record。扫描按有序全局行过滤，升序after游标与limit+1续页语义取自上游。
- 实际验证：累计4922条MemoryStorage操作直接对照一致；O0/O2 ASan/UBSan专项覆盖隔离、current/历史、空key、NUL身份字段及逐直接复制/分配/插入故障清理，完整Runtime退出0。隔离暂存核心树G00、Assembly inventory及产品构建通过；共享树G00仍因4个TUI候选文件未登记失败。
- 同一实施Agent自审：仅新增实际导出/参考登记，未调整门禁与预算；6个候选路径及暂存内容清单SHA256摘要（本记录加入前）`797454a97bac4d76b8b1ad1dc7c15da86ddc247a3b24d86f1cb2d937472b32f3`。结论为本地候选验证通过。当前扫描使用线性候选过滤，Document.copy和Tx仍在K03执行队列，K03未完成。

## K03 Document.copy（2026-10-26）

- 来源：Pi `d5b3de22` 的 `MemoryStorage.resolveDocumentCopies` 与其 `prepareCommit` 调用；`testing/storage-conformance.ts` 的stored-base独立性、ambiguous source及latest/current场景。fetch后Pi/Harness仍为 `d5b3de22` / `22ddee37`，无新修复。
- 实现：手写Assembly在私有输入中先解析copy，检查源ID在全批document命令中不变，读取已提交current/as-of快照；两个scope必须为Conversation，保留kind/key/history/fork，允许目标owner不同。生成独立base后复用现有动作/地址/ID校验和原子应用。内部读取仅用于prepare的既存源；公开读取仍拒绝prepare重入。
- 实际验证：5254条实际MemoryStorage混合操作对照一致；O0/O2 ASan/UBSan专项通过，含逐直接分配/插入/部分复制故障，失败保持源记录/ID/序号和调用者输入。完整Runtime最终树退出0。隔离核心暂存树G00、Assembly inventory及产品构建通过；共享G00仍为4个TUI候选未登记。
- 同一Agent自审：扩展现有Storage职责/参考登记，无新公开布局或门禁变更。6个候选路径与暂存内容清单SHA256摘要（加入本记录前）`f165b21a5ee8c812e89e0f570902a4d3e31c2b4eb4051574d217e97b49b26545`。结论为本地集成候选通过；完整Tx、所有权、迁移提交及三条既有delta ABI差异仍待K03/K15，未声明K03完成。

## K03 事务submission转换候选（2026-10-26）

- 阅读固定Pi `transaction.ts` 的构造/暂存表、submissionChanges、applySubmissionChange、settleSuccess/discard和read/write admission。当前先移植实际转换函数到 `records.S:pg_records_submission_change`；完整事务组装尚待实现和接入。终态保留、input/write放置及done/unanswered结算均使用独立owned JSON候选。
- 实际验证：24组状态与固定上游原函数体一致；测试读取固定源码并只剥离TS类型执行该函数体，未替换行为。5254条现有MemoryStorage操作仍通过。O0/O2 ASan/UBSan验证两段复制及部分复制失败清理、输入不变；完整Runtime退出0。暂存核心树G00、Assembly清单及产品构建通过，共享G00仍为4个TUI候选未登记。
- 同一Agent自审：扩展现有模块导出/职责，未修改门禁；6个候选路径/内容摘要（本记录加入前）`325f9ac708ceae61a206b5d28b64b262388fd31ce6d08020a80b802d913c52a0`。结论为本地候选通过，尚未声称事务实际调用完成或K03完成。Pi/Harness fetch结果仍为固定 `d5b3de22` / `22ddee37`。

## K03 事务批次与submission组装（2026-10-26）

- 来源：Pi `d5b3de22` 的 `Transaction`构造、`#read/#write/#assertOpen`、`settleSuccess/discard`及submissionChanges组装，Session的 `new Transaction/settleSuccess/discard` 提交调用。Pi/Harness fetch后仍为固定 `d5b3de22` / `22ddee37`。
- 实现：现有records模块新增48字节原生Tx，手写Assembly维护私有最终StorageWrite数组，普通表写尝试后禁止public表读，Document暂存保留表读；commit先seal并走现有 `pg_records_commit`，各结果均清理候选，discard可重复调用。注入committed-read同步服务；submission变更逆序查最新暂存候选再读Storage，转换后追加，实际接入上一批转换函数。整个Session mutation line由调用者串行持有。
- 实际验证：真实MemoryStorage组合通过暂存不可见、混合批次采纳、丢弃/重用、read barrier、seal、readonly、Storage失败及Session锁存，queued→placed→done同批落盘与终态保持。O0/O2 ASan/UBSan枚举stage与submission组装直接分配/复制/插入故障；回调partial错误结果清理、Tx回调重入拒绝通过。完整Runtime退出0（最后补充的重入专项再以最终源O0/O2执行通过）；原24条转换函数和5254条MemoryStorage对照继续通过。隔离核心暂存树G00、Assembly清单与产品构建通过；共享G00仍报告4个TUI未登记候选。
- 同一实施Agent自审：新增实际导出与模块职责登记，未调整门禁。5个候选路径/暂存内容清单SHA256摘要（加入本记录前）`f2ae287cc257ec6ce0ed5919abf873ef1243b18d065786fe97b4b1a7c3142650`。结论为本地组合候选通过；typed创建/ID/查询、Document tracker/所有权/迁移和异步操作drain尚待后续接入，当前应用消息投影尚未切到该通用Tx，不声明完整K03完成。

## K03 typed Entry追加（2026-10-26）

- 来源：Pi `d5b3de22` 的 `Transaction.#appendEntry/#requireConversation`、重载入口、调用的Storage mint/copyJson，以及 `#stageConversation` 的created集合。Pi/Harness fetch后仍为固定 `d5b3de22` / `22ddee37`。
- 实现：手写Assembly `pg_record_tx_append_entry` 确认同批创建/已提交Conversation后mint；复制draft并覆盖id/conversationId/byTaskId，head=self使用新ID；先准备独立返回值和完整StorageWrite，再一次追加到Tx私有数组。失败不追加，已mint的ID保留消耗；读/分配失败清理全部partial值，纯原生状态与JSON库胶水编排均为Assembly。
- 实际验证：10条构造语料执行固定上游原方法体和实际Chord copyJson（固定read/mint host）一致，仅证明记录构造；真实MemoryStorage验证同批Conversation追加、committed Conversation追加、缺失拒绝、归属、返回值隔离、丢弃及ID消耗。O0/O2 ASan/UBSan与每项直接分配/插入/复制故障通过，完整Runtime退出0；之后新增的构造对照再单独通过，原24条submission/5254条Storage对照保持通过。隔离核心暂存树G00、Assembly清单和产品构建通过。共享G00仍报告4个TUI候选未登记。
- 同一实施Agent自审：扩展已有实际导出/来源，无门禁或预算变更。6个候选路径/暂存内容清单SHA256摘要（加入本记录前）`dff3402f59d963cc1e0a0ffb23074f13a9946246d85048ab3860dd3265df6b62`。结论为本地候选通过；Conversation typed ownership/fork与完整Document Tx仍待实现，新入口尚待应用消息投影接入，K03未完成。

## K03 typed Conversation记录创建（2026-10-26）

- 来源：Pi `d5b3de22` 的 `Transaction.#stageConversation/#currentTask`，阅读 `session/forks.ts:prepareForkDocumentCopies/collectCopies` 确认后续fork选择依赖；本批实现无parent记录构造。Pi/Harness fetch仍为固定 `d5b3de22` / `22ddee37`。
- 实现：`pg_record_tx_create_conversation` 手写Assembly支持root保留ID1、ownerless与task-owned；普通创建先mint，再查最新暂存task或Storage，owner由task.conversationId/taskId派生；生成完整命令后一次追加，返回独立记录。mint后失败消耗ID符合上游顺序。
- 实际验证：4种构造执行固定Pi实际私有方法体（parent未提供，mint/task/creation-hook固定host）一致；实际MemoryStorage验证同批task、committed task和更新候选优先级、missing owner、root及返回隔离。O0/O2 ASan/UBSan和逐直接分配/插入/部分复制故障通过；完整Runtime退出0，最后补充候选优先级用例另在最终源O0/O2执行通过。原10条Entry、24条submission及5254条Storage对照通过。隔离核心暂存树G00、Assembly清单、产品构建通过；共享G00仍报告4个TUI候选未登记。
- 同一实施Agent自审：扩展实际导出/来源登记，不改变门禁或预算。6个候选路径/暂存内容清单SHA256摘要（加入本记录前）`ac445b23fbacbe99b99c73b3d0b24e143422bc72168e823f1c967dc933c17776`。结论为本地候选通过；fork文档选择/creation hook装配、完整Document Tx及当前应用接入仍待完成，K03未完成。

## K03 fork文档选择（2026-10-26）

- 来源：Pi `d5b3de22` 的 `session/forks.ts:prepareForkDocumentCopies/collectCopies`，Transaction `#stageConversation` 调用，`session-forks.test.ts` 的隐藏Entry和重复asOf/current策略场景。Pi/Harness fetch仍为固定 `d5b3de22` / `22ddee37`。
- 实现：`pg_records_prepare_fork` 手写Assembly，通过32字节Storage服务表注入fork-aware Entry、256项Document scan和mint；第一遍取Entry实际owner/commitSeq的asOf，第二遍取请求parent的current，initial跳过。为child生成独立record/source plans，替换scope/ID并移除生命周期stamp，重复kind/key目标地址拒绝。IDs在错误时不回退。
- 实际验证：5262条累计操作含直接执行固定Pi实际prepareForkDocumentCopies，600个文档分页、继承Entry、NUL family key、initial跳过、隐藏/缺失Entry、策略冲突及失败后mint结果一致。实际MemoryStorage消费plans完成document.copy，读取分叉时旧值。O0/O2 ASan/UBSan及逐直接分配/插入/部分复制和读失败清理通过，完整Runtime退出0；原10条Entry/4条Conversation构造和24条submission对照通过。暂存核心树G00、Assembly清单及产品构建通过；共享G00仍为4个TUI候选未登记。
- 同一实施Agent自审：扩展实际导出/Storage回调布局/参考登记，未修改门禁或预算。6个候选路径/暂存内容清单SHA256摘要（加入本记录前）`321f7c6814c000d3c900966b948c908477eb89da2605d113cb9e6bdf59a1672c`。结论为本地候选通过。Tx fork parent/plan组装与creation hook仍待接入，K03未完成。

## K03 Conversation fork事务装配（2026-10-26）

- 来源：固定Pi `Transaction.#stageConversation/#assemble/#rejectForkSourceWrites`、`session/forks.ts`，`session-forks.test.ts` 的同批源变更及current-policy场景。Pi/Harness fetch仍为 `d5b3de22` / `22ddee37`。
- 实现：`pg_record_tx_fork_conversation` 手写Assembly在私有替换批次中复用typed owner创建、附parent及生成copy plans，成功才替换原Tx数组；既有候选只加引用，不改字段。read/mint局部桥接允许Tx与fork Storage使用不同context。准备失败保留原候选，IDs保持消耗。commit封闭后先检查复制源及parent current-policy冲突，预备错误不进入Storage、不锁存Session。
- 实际验证：真实MemoryStorage完成parent+文档一次提交、历史内容、task-owned fork及不同服务context；返回parent修改不影响提交。missing Entry、fork前后变更源、fork前后新增parent current-policy文档均在准入前拒绝，Session健康。O0/O2 ASan/UBSan枚举fork装配每项直接分配/插入/复制/读取故障，原候选保留、输出清空、所有权清理通过。最终完整Runtime退出0；原5262条Storage/选择及构造对照通过。隔离核心暂存树G00、Assembly清单与产品构建通过；共享G00仍有4个TUI未登记候选。
- 同一实施Agent自审：扩展现有实际导出/语义登记，未调整门禁/预算。5个候选路径/暂存内容清单SHA256摘要（本记录加入前）`d5ce6dedb839d07cc4aad4c375b221397088cdc101abcc547f2fc83108a7e882`。结论为本地候选通过；Conversation creation hook、typed查询、完整Document Tx及应用消息投影接入仍待完成，K03未完成。

## K03 Tx分页与latest-head查询（2026-10-26）

- 来源：固定Pi Transaction `scanConversations/scanEntries/scanTasks/latestHeadMarker`、`#read/#assertOpen`，实际调用的MemoryStorage scan/head仍复用原实现。Pi/Harness fetch仍为固定 `d5b3de22` / `22ddee37`。
- 实现：Assembly `pg_record_tx_scan/latest_head` 通过注入的24字节查询descriptor调用committed Storage；普通表写后与sealed拒绝、callback期间防重入、失败释放partial结果并清空输出。原Query/Page布局112/24字节移至共用records头，Memory提供 `pg_record_memory_queries`，其余实现与过滤/游标语义不变。
- 实际验证：真实MemoryStorage通过各表扫描、fork分页及祖先head、无head/未知会话、read barrier/seal、callback partial错误清理与重入拒绝；O0/O2 ASan/UBSan及逐分配/复制/插入故障通过。初次全量因服务factory的PC32重定位不能链接shared object失败，改为GOT获取函数地址后5262条Storage对照及完整Runtime通过；最后补充无head专项在最终源O0/O2通过。隔离核心暂存树G00、Assembly清单与产品构建通过；共享G00仍为4个TUI候选未登记。
- 同一实施Agent自审：新增实际导出/查询契约登记，移动现有类型定义，不保留双份布局，不修改门禁或预算。7个候选路径/暂存内容清单SHA256摘要（加入本记录前）`c3e2fb7f4114175cfdc1970470a03774ff4cc23e182184ecb17f9a0378d938fc`。结论为本地候选通过；creation hook、request查询与完整Document Tx等仍待实现，K03未完成。

## K03 Conversation创建host（2026-10-26）

- 来源：固定Pi Transaction `#stageConversation`、Session `conversationCreated` 默认与host注入、Harness同名override及 `harness-conversations.test.ts` 创建回调场景。Pi/Harness fetch仍为 `d5b3de22` / `22ddee37`。
- 实现：Assembly `pg_record_tx_create` 统一root/new/fork记录组装后调用注入host。callback可以在本Tx追加/嵌套创建；活跃callback期间commit/discard拒绝。原Tx预留字段用于creation_depth，48字节布局不变。回调错误返回独立CREATION_FAILED结果，输出清空，由外层失败callback discard已暂存候选，符合Session提交错误传播分工。Harness内建文档初始化由其对应模块接入host，本批未宣称已有Live/Inbox等文档。
- 实际验证：实际MemoryStorage验证root/new/fork回调内追加Entry、嵌套创建、空hook及错误路径丢弃，候选阶段无存储写入。O0/O2 ASan/UBSan与逐直接分配/插入/部分复制故障通过；完整Runtime退出0，原5262条Storage/选择与构造对照通过。隔离核心暂存树G00、Assembly清单与产品构建通过。
- TUI交接登记：已读当前U03请求，四个新文件在共享bootstrap清单按client-source/client-test登记，未修改守卫，当前共享G00通过。此工作树登记连同已有PTY工具登记待TUI整体交接后提交；TUI源码未进入本批暂存。登记仅证明文件已列入清单，不代表完整TUI/产品构建验收。
- 同一实施Agent自审：新增实际导出/host类型/creation_depth语义登记，无门禁预算调整。5个核心候选路径/暂存内容清单SHA256摘要（本记录加入前）`64434fa17fed9691485aa373c8a8c799149882ee39004bc9b8444243e2c084e9`。结论为本地候选通过；完整Document Tx、request查询和实际应用接入仍待完成，K03未完成。

## K03 新owned work的最终owner（2026-10-26）

- 来源：Pi `Transaction.#validateOwners/#currentTask/#assemble`、`session-tables.test.ts` 的先创建子会话再abort/结束owner场景。核对Doc获取时发现该前置约束缺口，本批先修复。Pi/Harness fetch仍为固定 `d5b3de22` / `22ddee37`。
- 实现：手写Assembly在Storage commit前校验新Conversation/Task owned work，owner取最后暂存Task，缺失时读committed；missing/terminal/completing/abort-marked拒绝。现有Task更新保留旧owner不是新owned work。原生用未提交ID识别新Task，typed构造使用mint ID；完整Task写入生命周期仍待对应实现。
- 实际验证：48组真实MemoryStorage顺序/状态/abort组合通过；同批新owner创建子会话后结束的整批拒绝、现有任务保留旧owner、owner读取partial分配失败清理通过。27组执行固定Pi原validateOwners方法体一致；原5262条Storage对照通过。O0/O2 ASan/UBSan及完整Runtime通过，最后补充的新owner同批结束专项在最终源O2通过。隔离核心暂存树G00、Assembly清单与产品构建通过；共享G00通过。
- 同一实施Agent自审：扩展现有模块语义/来源登记，无新公开布局、门禁或预算调整。6个候选路径/暂存内容清单SHA256摘要（加入本记录前）`506d15da819549e3975944c60d0f2f4414f85d3610f1f154a6c4636638ce1b84`。结论为本地候选通过；完整Document Tx等仍待完成，K03未完成。

## K03 原生文档编辑日志（2026-10-26）

- 来源：Chord `TrackerImpl.beginChange/ChangeImpl.prepare/abort`，Pi Transaction `#acquire/planDocument/settleSuccess`，`session-documents.test.ts` 的structural no-op、prepare revocation及迁移场景。Pi/Harness fetch仍为固定 `d5b3de22` / `22ddee37`。
- 实现：现有delta模块新增32字节显式操作日志与16字节Prepared。open/append脱离输入，append只入有限JSON数组、路径/操作在snapshot/prepare重放；snapshot返回独立值，prepare成功或失败都封闭句柄，成功转交value/ops；discard不释放已转交对象。每次追加只复制新批次，最终重放复用已有相邻追加合并。该原生边界明确区别JS proxy与其错误时点，不宣称代理/ops规范化兼容。
- 实际验证：448条编辑日志重放与实际Chord oracle一致，3条已登记ABI拒绝另行验证；原449条delta/96条revision对照通过。O0/O2 ASan/UBSan枚举open/append/prepare直接分配/插入/部分复制故障，输入/快照隔离、追加失败保留原操作、prepare封闭与转交所有权通过。真实MemoryStorage完成编辑→Prepared→Tx document.change→提交，旧快照保持原值。完整Runtime退出0；最后补充非空旧操作前缀回退专项在最终源O0/O2通过。隔离核心暂存树G00、Assembly清单与产品构建通过；共享G00通过。
- 同一实施Agent自审：扩展实际导出/原生change布局与参考登记，未改门禁或预算。7个候选路径/暂存内容清单SHA256摘要（本记录加入前）`13ccb0e68b9de89eff46db025c532c5d219ce7f4d6a7cbec9dad90e65f5b2f30`。结论为本地候选通过；版本/checkpoint规划、Doc获取缓存/所有权和完整Tx自动结算尚待接入，K03未完成。

## K03 loaded文档base/delta规划（2026-10-26）

- 来源：Pi固定 `Transaction.planDocument` loaded分支、`#assemble` checkpoint选择、`session-documents.test.ts` 的迁移/非空structural no-op场景。Pi/Harness fetch仍为 `d5b3de22` / `22ddee37`。
- 实现：Assembly `pg_document_plan_change` 将借用Prepared转为独立owned document.change；版本升级必选base，即使ops空；同版非空ops按传入checkpoint决策选base/delta，同版空ops返回无写入。拒绝newer stored version，payload复制与输入隔离。planner不调用predicate，后续统一Doc结算在验证之后计算决策，保持上游回调时序要求。
- 实际验证：72条执行固定上游实际planDocument与checkpoint选择分支一致；O0/O2 ASan/UBSan枚举直接分配/插入/部分复制故障、safe ID/version、新er拒绝和payload隔离。实际MemoryStorage执行迁移materialize→空编辑→v2 base、checkpoint base、普通delta及空编辑不提交，旧快照保持稳定。完整Runtime退出0，原448条change journal/449条delta/96条revision及5262条Storage对照通过（3条既有ABI拒绝单列）。隔离核心暂存树G00、Assembly清单和产品构建通过；共享G00通过。
- 同一实施Agent自审：扩展现有实际导出/来源登记及直接故障注入检查，无门禁预算变更。8个候选路径/暂存内容清单SHA256摘要（加入本记录前）`1818fc0ff5f6821da3a1cb0ee490a451746f30322e54c129fa8ecef452563314`。结论为本地候选通过；Doc获取缓存/所有权、predicate统一调用和自动结算仍待接入，K03未完成。

## K03 current文档载入与编辑句柄（2026-10-26）

- 来源：固定Pi Session `#loadDocument` 未命中缓存分支、Transaction `#acquire` loaded分支、documents `materializeDocument`，实际Chord `track`。Pi/Harness fetch仍为 `d5b3de22` / `22ddee37`。
- 实现：Assembly record_decode及120字节loaded handle，注入24字节find/read服务，current find→current read→身份/定义scope/version检查→隔离物化迁移，保存stored_version/value_version/deltasSinceBase。record_owner持有NativeRecord字符串来源，value独立拥有；Memory提供GOT函数绑定。loaded_begin生成独立操作日志；close/失败清空句柄，Definition按契约借用。
- 实际验证：128条执行实际上游未命中缓存方法体及实际materializeDocument/track，匹配元数据、物化值与find/read顺序，含NUL kind/key、scope/history/fork、版本/迁移、find missing与find后read missing。实际MemoryStorage完成迁移载入→空编辑→base提交、版本拒绝、Storage关闭后独立持有及逐直接分配/读取/部分复制错误清理；O0/O2 ASan/UBSan与完整Runtime通过。原5262条Storage、72条规划、448条编辑等对照继续通过（3条已有ABI拒绝单列）。隔离核心暂存树G00、Assembly清单、产品构建及共享G00通过。
- 同一实施Agent自审：新增实际导出/loaded服务布局与MISSING/STORAGE_FAILED错误登记，不调整门禁/预算。10个候选路径/暂存内容清单SHA256摘要（本记录加入前）`ccc604ec16573fae1fd929ab6d6d731fe086f49a4d9171d6ded8e85117c24227`。结论为本地候选通过；缓存命中/版本仲裁/holds、Doc获取后的统一adoption与checkpoint仍待实现，K03未完成。

## K03 按地址缓存与版本仲裁（2026-10-26）

- 来源：固定Pi Session的loadDocument/touchDocument/dropDocument/sweepDocuments/pinDocument及session-document-cache-bench长期工作负载。Pi/Harness fetch仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly 112字节cache，以成熟json-c linkhash索引完整地址，节点拥有loaded句柄，idle链维护使用时间与pin变化。相同valueVersion复用并检查definition语义，版本切换flush后drop/reload，flush失败保留旧值。时钟/pin服务注入，sweep由Session执行线显式调度；retention可更新，close释放节点/索引。借用期限、Definition生命周期及holds flush分工在头文件明确。
- 实际验证：11步缓存/版本/held-flush与实际Pi原方法体一致；160个NUL family地址跨哈希扩容重复读取无额外Storage调用。101ms触摸粒度、pin/unpin、零retention、条件驱逐和重开通过。O0/O2 ASan/UBSan枚举table/node/insert/读取/部分复制故障；完整Runtime退出0，最后补充definition命中不符最终源O2通过。隔离核心暂存树G00、Assembly清单、产品构建及共享G00通过。
- 同一实施Agent自审：扩展已有实际导出/来源登记，故障注入覆盖linkhash分配/插入，无门禁/预算变更。7个候选路径/暂存内容清单摘要（加入本记录前）：682825f75f16283d8bf4d5900e705578088f2edae871df99b91e292a5faae4c5。结论为本地候选通过；完整adoption、holds持久队列、Doc统一结算及应用接入仍待实现，K03未完成。

## K03 loaded文档adoption（2026-10-26）

- 来源：固定Pi Transaction.adopt的loaded分支、Session.runCommit的同步adopt/发布顺序，Chord Tracker.adopt的base/revision检查与指针交换。Pi/Harness fetch仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly 80字节adoption，准入前复制Prepared值并保留expected值引用，记录stored/value version、deltas、incarnation，避免引用地址复用。apply仅验证预期状态并交换已准备值、调整持久版本/重放计数；空ops不替换物化值，迁移base仍更新stored version并重置计数。重复/过期候选拒绝；discard释放未发布所有权。目标loaded句柄由Session执行线保持有效。
- 实际验证：12个结果执行实际Pi adopt方法体及实际Chord tracker一致；O0/O2 ASan/UBSan覆盖竞争候选、重复采纳、迁移/无编辑identity、输入隔离及准备阶段逐直接分配/部分复制故障。真实MemoryStorage与records adopt/publish组合验证成功、adopt前失败、adopt后后端报错；失败前保留旧值，失败后保留已采纳值并锁存Session，不发布伪成功。apply内启用故障注入记录到0次分配。完整Runtime、隔离核心暂存树G00/Assembly清单/产品构建及共享G00通过。
- 同一实施Agent自审：扩展现有模块实际导出/adoption布局与参考登记，无门禁预算修改。7个候选路径/暂存内容清单摘要（加入本记录前）：1f1597f4aa0a9dd22af0bcb71b8c1c8279b5c250f71f14f8fdba01fd6cd20a42。结论为本地候选通过；统一Doc事务、creation install/retirement evict、checkpoint回调及holds仍待完成，K03未完成。

## K03 多loaded文档统一结算（2026-10-26）

- 来源：固定Pi Transaction.settleSuccess/assemble/adopt/discard、Session.runCommit同步adopt与发布。先prepare所有编辑，再检查生成计划，最后执行checkpoint并准备adoption；Pi/Harness fetch仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly document_tx模块以152字节entry描述借用的loaded/edit及内部Prepared/adoption/write所有权。records提供sealed settlement finalizer；Doc finalizer生成写入并在checkpoint前验证owner/复制源约束，准备完再交Storage。同步adopt依次采纳全部loaded值，再调用原Session adopt；publish保持原回调并在返回时恢复context/hooks。成功/失败均释放未转交候选并封闭编辑；调用参数前置拒绝不改调用者状态。空编辑不提交Storage。
- 实际验证：真实MemoryStorage验证双文档base、delta+普通表混合、空编辑identity、checkpoint拒绝、owner/生成计划复制源错误不调用predicate、原回调恢复。O0/O2 ASan/UBSan逐直接分配/插入/部分复制故障遍历通过，失败保留旧文档且无存储半批；Prepared/编辑清理。补充连续提交时将测试断言从仅首次发布改为每次adopt计数先于publish计数（原顺序要求保留）；最终完整Runtime退出0，原Pi/Chord各对照继续通过。隔离暂存核心树G00、Assembly清单与产品构建、共享G00通过。
- 同一实施Agent自审：新增两个实际产品文件/模块、构建依赖与导出登记，不修改门禁或预算。10个候选路径/暂存内容清单摘要（加入本记录前）：8cda625dcdeb1c1094fca7f0b534d4e6ea2e92f71aaf7fecb01bd96d70a8e80e。TUI待交接登记保留工作树，未混入本核心提交。结论为本地候选通过；自动Doc获取、创建安装/退役驱逐、holds与应用消息投影仍待接入，K03未完成。

## K03 已有Doc自动获取与事务作用域（2026-10-26）

- 来源：固定Pi Transaction.doc的latestDocumentByAddress/draftPromise复用、assertTaskDocumentsOpen，以及loaded acquire/settleSuccess/discard。Pi/Harness fetch仍为d5b3de22 / 22ddee37。
- 实现：document_tx新增48字节scope，open绑定当前record Tx/cache，get按本事务地址复用首次编辑，首次访问从cache取得loaded并分配独立稳定change；结算数组扩展不移动change。finish复用多文档结算，abort丢弃record Tx，均释放scope编辑。显式身份与Definition校验保留；同址有效版本token重复访问采用首次draft及checkpoint，与Pi memoization对应。Task候选terminal先拒绝，包含已获取地址。每个record Tx只绑定一个Doc scope，loaded借用由mutation line保证，scope字段API管理。
- 实际验证：16个family实例跨数组扩容、句柄稳定、重复获取无额外find、批量提交及缓存值更新通过；普通表写后Doc获取仍允许。abort/missing/terminal候选/逐直接分配读取扩容错误清理在O0/O2 ASan/UBSan通过；完整Runtime退出0，原全部Pi/Chord专项继续通过。隔离核心暂存树G00、Assembly清单与产品构建、共享G00通过。
- 同一实施Agent自审：扩展实际导出/作用域契约和来源，无门禁/预算变更。6个候选路径/暂存内容清单摘要（加入本记录前）：4d02e9ed34e2e03ae2982f2f5f4f80b1972a2ec49fdb78f8aebe9d1e48db162f。结论为本地候选通过；缺失地址初始化创建/安装、retirement与holds尚待接入，当前get返回MISSING而非创建；K03整体未完成。TUI未提交工作保持原样。

## K03 新Doc创建记录与initializer服务（2026-10-26）

- 来源：Pi documents.documentCreate、Transaction.doc的family seed复制与acquire初始化顺序；创建时scope owner检查和mint位于周围事务，当前实现记录/initializer两项基础能力。Pi/Harness fetch仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly record_create生成独立owned创建记录，保留scope/owner/history/fork/family身份，无createdAt/retiredAt；initialize通过显式initializer服务复制family seed、调用callback、验证并再次复制其object结果。singleton seed不进入callback，返回值/输入/保留引用所有权隔离，partial失败清理明确。INITIAL_FAILED枚举追加，旧状态值不变。
- 实际验证：189条记录与固定Pi实际documentCreate一致；O0/O2 ASan/UBSan覆盖identity/版本/语义拒绝、seed及initializer保留引用隔离、callback失败和逐直接分配插入复制故障。实际MemoryStorage完成initializer→mint→RecordCreate→Tx create→loaded重读，Storage提供createdAt，调用者修改seed不影响数据。完整Runtime、隔离核心暂存树G00/Assembly清单/产品构建及共享G00通过。
- 同一实施Agent自审：扩展已有实际导出/initializer ABI、故障注入与来源登记，不修改门禁或预算。8个候选路径/暂存内容清单摘要（加入本记录前）：09762ead4f9010272a5885c2e0eda5ac148dcf8b6c7996aa9c3faaebb1af287a。结论为本地候选通过；missing地址自动获取、owner检查串接、缓存安装/retirement/holds尚待实现，K03整体未完成。
