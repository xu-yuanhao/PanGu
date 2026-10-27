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

## K03 新Doc缓存安装（2026-10-26）

- 来源：固定Pi d5b3de22 的Transaction.adopt新incarnation分支、Session.host.install/touchDocument；本批fetch Pi/Harness仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly 32字节install候选与32字节reservation；prepare核对未带lifecycle stamp的创建记录，隔离record/value，预分配node和createdAt整数。reserve通过json-c公开lh_table_resize预留容量，apply确认sequence后设置整数、插入索引并接入all/idle链；discard释放未安装所有权。保持cache112字节与旧函数表布局。reservation要求同一Session执行线且期间只允许共用预留的安装操作；host随后维护pin。
- 实际验证：真实MemoryStorage同批创建32个Doc，确认后install，再get全部零find/read命中，record/value/version/createdAt与输入隔离符合源语义。重复install、重复地址、已关闭缓存、过期count、非法sequence、带createdAt/retiredAt输入拒绝；逐直接分配/插入/复制故障与预留失败清理通过。O0/O2 ASan/UBSan通过；apply计数只有一次预留容量内的linkhash插入，直接分配/复制/resize wrapper均未调用。完整Runtime退出0，最终语义检查补充后O2专项通过；原Pi/Chord差异基线保持。共享及隔离核心暂存树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：扩展已有模块导出/来源及专项wrap，无门禁预算变更。6个候选路径内容清单摘要（加入本记录前）：bd91064c5e37a89d72edb4eb245fd4dca718a5dc97eb21d626ca2dbf0a21f98b。结论为本地候选通过。缺失地址自动创建与统一Doc Tx结算尚需接线；新incarnation的retirement/holds仍属于K03。TUI未提交文件保留原样。

## K03 Doc自动创建与统一结算（2026-10-26）

- 来源：固定Pi d5b3de22 的Transaction.doc/acquire(created)、planDocument(created)、adopt与Session.install；读取session-documents.test.ts的首次base、重复acquire、首次family seed用例，以及session-adoption的确认边界。本批Pi/Harness fetch仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly pg_document_tx_doc使用56字节request注入initializer/mint/checkpoint；missing地址先验证conversation/task owner，再隔离初始化、mint、创建私有loaded和稳定edit。重复地址复用首次edit；统一finalizer为新Doc生成create/base，即使未编辑也写初值；在所有checkpoint后预备install并统一reserve，Storage确认后批量安装，再调用原Session adopt/publish。新Doc私有内存随finish/abort清理；scope48/entry152及旧函数表布局保持。抽出共用entry追加逻辑供已有/新建路径使用。
- 实际验证：16个family文档跨数组扩容、同址重复只初始化/mint一次、批量提交后直接命中缓存、混合新旧Doc及普通表写通过。owner缺失/terminal在initializer前拒绝，同批新conversation/task允许创建；初始化失败、只读、abort及逐直接分配/插入/复制错误清理通过。后端adopt前失败无缓存安装，adopt后失败保留已提交/采纳状态并锁存Session，均不发布伪成功。O0/O2 ASan/UBSan专项、完整Runtime与原Pi/Chord对照退出0；最后补充后端失败用例两优化级均通过。共享G00/Assembly清单、隔离核心暂存树G00/Assembly清单/产品构建通过。
- 同一实施Agent自审：扩展实际导出/来源，records新增隐藏owner查询仅供mutation line调用；未改门禁、预算或TUI文件。6个候选路径内容清单摘要（本记录加入前）：58e2e01a160ff262750abf00347c8301ba7d8da6e9b77586d3b69015fc73be86。结论为本地候选通过；retireDoc、同址替换、Task终态自动退休及holds仍待下一生命周期步骤完成，K03未完成。TUI候选及其待交接登记保持工作树。

## K03 显式Doc退休与同址替换（2026-10-26）

- 来源：固定Pi d5b3de22 的Transaction.retireDoc/findRetirement/planDocument/adopt、Session.cached/evict，以及session-documents.test.ts的“retires documents and creates a new incarnation”“retires without acquisition”用例。本批Pi/Harness fetch与本地仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly pg_document_tx_retire，事务条目区分已编辑/仅record退休，逆序查询最新incarnation；退休后doc跳过旧cache并初始化新draft。record-only查找优先缓存，只查find而不read/materialize/migrate。最终批次将每个旧incarnation的最终内容与retire一起写入；新建同批退休写create+retire而不安装。确认时先采纳所有已有编辑，再按ID驱逐退休节点，最后安装新live节点。新增reserved eviction保持索引容量并同步reservation计数；cache112/scope48/entry152等旧布局不变。
- 实际验证：原地编辑→退休→替代、未获取值直接退休、新版本token退休而不迁移、缺失no-op、新建同批退休、8代同址替换、32地址整批替换通过。32节点采纳阶段拦截到32次insert，直接分配/复制/resize均0；索引清空后保留预留容量。只读/abort、逐直接分配/插入/部分复制故障，以及backend确认前/后报错通过；确认前保留旧cache，确认后保留新事实并锁存Session，不发布伪成功。批量测试发现空edit被重复指针检查误判，已改为只检查非空edit唯一性，原检查强度保留。O0/O2 ASan/UBSan、最终完整Runtime/原Pi-Chord对照、共享及隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：扩展已有实际导出/来源与直接documents依赖，无门禁/预算变更。7个候选路径内容清单摘要（本记录加入前）：c8fa60ca3b0064f66f023a17d3f2d226b7ef5b8568b0747b6719fa12bef7ddb4。结论为本地候选通过；Task终态对全部Doc自动退休、fork-copy获取、holds及完整K03仍待接入。TUI候选及待交接登记保留工作树。

## K03 Task终态文档自动退休（2026-10-26）

- 来源：固定Pi d5b3de22 Transaction.assemble的terminalTaskIds/retiring、最终Task候选与committed terminal/conversation核对；session-tables.test.ts的terminal candidate拒绝与terminal settlement（含同批创建Doc）场景。本批fetch Pi/Harness仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly pg_document_tx_settle注入16字节scanner；finish复用同一路径。封存编辑并验证批次后，按最新staged Task候选发现terminal，核对committed Task身份/状态/Conversation；标记事务内文档、通过256项current scan分页追加definition-free退休，使用成熟json-c pointer-key linkhash对safe ID去重。新Task跳过committed scan；已提交Task在缺scanner时准入前拒绝。公共query布局64字节移至documents.h，Memory扫描void*契约可直接绑定。scope48/entry152/cache112与旧函数表保持。发现阶段数组扩展后同步更新结算context，失败/成功清理覆盖新增退休项。
- 实际验证：530个已提交文档、两个terminal Task、一个保持running的Task跨四页扫描；525个旧Doc及同批创建的1个新Doc退休，另5个存活。包含已编辑、仅cached、显式退休、未加载Doc以及新Task同批终态；scan期间编辑已sealed，checkpoint位于发现之后。重复terminal、Conversation变更、错误Task身份、缺失scanner、部分输出读取错误、扫描错误/错误scope/空next页/不前进cursor、只读均原子拒绝。两类逐直接分配/插入/部分复制故障遍历通过，失败保留原存储和旧缓存值。O0/O2 ASan/UBSan、完整Runtime与原Pi/Chord基线退出0；最后身份核对补充后两优化级专项通过。最终隔离核心暂存树G00/Assembly清单/产品构建及共享G00/Assembly清单通过。
- 同一实施Agent自审：扩展实际导出/来源与故障注入wrapper，scanner完全注入，产品不绑定具体Memory实现；无门禁/预算变更。9个候选路径内容清单摘要（本记录加入前）：2a23b2130ccd1e0d65f27ffa92b4bcdd7c3c1117def0d12c3dd43c07b435b49c。结论为本地候选通过；fork-copy事务获取、holds与完整K03仍待接入，应用Task调度属于K06。TUI未提交工作和待交接登记保留。

## K03 fork-copy同事务获取（2026-10-26）

- 来源：固定Pi d5b3de22 Transaction.acquireForkCopy、planDocument(created)、rejectForkSourceWrites，session-forks.test.ts的typed migration/override、current/as-of源写入拒绝、copied retirement/recreate。本批fetch Pi/Harness仍为d5b3de22 / 22ddee37。
- 实现：手写Assembly按地址查找已staged copy，用原计划source id/at读取选定快照，核对source身份、kind/key/history/fork并按目标definition物化；复用copy目标ID，首次获取不调用initializer/mint，重复获取复用draft。结算原位将copy换为单一create/base；私有句柄保留原copy身份，全部生成计划后继续核对复制源未被修改，再运行checkpoint。原source冲突扫描抽为共用隐藏函数；安全JSON数值解析复用已有helper。未获取值的copy退休保留copy写入、不读取值；已获取副本按create/base退休，均支持同址重建。既有scope48/entry152和外部函数表不变。
- 实际验证：真实fork Conversation→as-of/current副本获取/编辑→同批提交/缓存安装，as-of采用分叉点旧值，current采用最新值；版本迁移和override合并到单一base，checkpoint计数0，源当前值保持不变。已获取/未获取副本退休与同址重建四种组合通过；current/as-of复制源修改在两种获取顺序均准入前拒绝，checkpoint不运行。十三类missing/partial/wrong-ID/kind/scope/fork/newer/version/point错误拒绝且不回退初始化。三类逐直接分配/插入/替换/部分复制故障、确认后adoption失败清理通过。O0/O2 ASan/UBSan、完整Runtime与原Pi/Chord基线通过；最后checkpoint/新版本退休token补充两优化级专项通过。最终隔离核心暂存树G00/Assembly清单/产品构建及共享G00/Assembly清单通过。
- 同一实施Agent自审：扩展既有来源/职责登记和array_put_idx故障注入，公开get/doc接口原地扩展，无门禁/预算变更。8个候选路径内容清单摘要（本记录加入前）：672ff0e11e243da16af6f5599f5b1d8fe4f67a77c20e856925c8a4b573f7725f。结论为本地候选通过；holds、统一Session/查询整合及完整K03仍待完成。TUI未提交工作和待交接登记保留。

## K03 PendingOps与held内容构造（2026-10-26）

- 来源：固定Pi d5b3de22 的Chord PendingOps/merge/pathKey、Durable.storeHeld，阅读delta-pending.test.ts的跨路径叶子折叠、结构屏障、根替换、MAX_PENDING_OPS及成本/所有权用例，以及session-deferred-commits的holding/storing调用。Pi/Harness fetch仍为d5b3de22 / 22ddee37；MIT原文与来源保存在docs/licenses/pi.md。
- 实现：新增手写pending.S；32字节队列、16字节take/held content。首批保留完整操作数量，开始折叠后a+a、s覆盖和string s+a跨独立路径合并，结构操作清索引，r丢弃前缀；超过10000项进入replace，take或首项r恢复。成熟json-c对象承担叶子索引；路径数字归一化保持1/1.0等价，字符串/数字和分段地址区分，包含NUL/有效UTF-8。Native按owned契约复制输入，通过操作数组浅复制和索引副本准备候选，失败保留原队列。storeHeld先折叠extra、构造delta/选定base/overflow base+root op，全部成功才清空原队列；结果拥有独立引用。
- 实际验证：901次队列push/take/size与实际固定Chord类一致；54组held store执行实际Pi函数（仅移除TypeScript类型）并使用实际PendingOps，content/ops/排空状态一致。O0/O2 ASan/UBSan覆盖输入修改隔离、输出/页尾、first/batch/folding及base/delta/replace逐直接分配/插入/序列化/部分复制故障，失败保持指针与内容。path key计数验证首批0次、折叠每个新leaf一次、结构操作0次；replace忽略追加和take转移不分配。真实MemoryStorage验证规划出的delta、root delta与选定base，值及deltasSinceBase计数一致。最终完整Runtime/原Pi-Chord基线通过（既有3条delta ABI拒绝单列），共享及隔离核心暂存树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：登记新ASM文件、许可与实际构建输入/导出，扩展直接故障注入；未改门禁预算。11个候选路径内容清单摘要（本记录加入前）：c1c440818c659724534c8c33b60f819232a2632828948e1cb61ed26e8770324d。结论为本地候选通过；本批是holds队列与内容函数，Session deferred eligibility/内存采纳、pin、deadline/flush及完整K03继续接入。TUI候选及五项待交接登记保留工作树，未混入本提交。

## K03 deferred资格与内存采纳路由（2026-10-26）

- 来源：固定Pi d5b3de22 Transaction.holdable/adoptHeld、Session.runCommit的empty/read-only/hold-vs-Storage分支，session-deferred-commits.test.ts的混合写入、迁移、只读和holding场景。本批Pi/Harness fetch仍为d5b3de22 / 22ddee37。
- 实现：手写records.commit_routed在最终验证后、Storage前选择内存路由，验证Session状态并阻止回调重入；Doc.deferred的40字节prepare/apply/discard/observe服务接收封存的候选。仅全批已存在latest/initial Conversation Doc且stored/value版本相同时延迟，checkpoint base也可延迟；创建/退休/迁移/其它写入整批落盘。独立prepare_memory不预留持久增量，defer retarget保留值交换并固定持久版本/计数；回退时恢复base/delta的持久元数据。旧枚举/结构/函数表不变。Host prepare成功后才交换值、安装持有队列、通知内存观察者；持久adopt/publish及sequence保持。准备失败保留旧状态，内部apply失败锁存Session且保留首个失败原因。
- 实际验证：24个普通/内存采纳结果执行实际Pi adopt/adoptHeld及Chord tracker一致，含MAX_SAFE计数；修复复用持久准备导致的计数上限提前拒绝，混合落盘仍保持原边界。实际Native PendingOps测试provider验证多次deferred累计、checkpoint不重置持久计数、最早deadline/Infinity参数、pin阻止sweep、空编辑无事件、跨语义只读项、多文档以及随后真实MemoryStorage flush。新增/混合Entry、Session/Task/rewindable/current Doc、迁移、退休和只读回退通过。新/已有hold两类逐直接分配故障保留原值/队列/deadline；prepare部分失败、apply错误、准备中Session失败和回调重入拒绝通过。O0/O2 ASan/UBSan专项、最终完整Runtime及原Pi/Chord基线通过（原3条ABI拒绝单列）；共享及最终隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：新增独立接口和实际导出/来源登记，无门禁/预算变更。12个候选路径内容清单摘要（本记录加入前）：4ad52d49b4d61048cf1b9174c85d4a61a5521a5036bb8330bc96a343b509503b。结论为本地候选通过；本批接通路由与内存采纳，原生holds宿主、正常提交takeHeld合并、版本切换前flush及timer/pin关闭生命周期继续实现，K03未完成。TUI候选及待交接登记保留工作树。

## K03 原生holds队列宿主（2026-10-26）

- 来源：固定Pi d5b3de22 session.ts的#hold/#release/#armHoldTimer及runCommit调用路径，session-deferred-commits.test.ts的连续holding、checkpoint和期限场景；本批fetch后Pi/Harness仍为d5b3de22 / 22ddee37。
- 实现：新增手写holds.S与独立holds.h；动态持有项借用cache-owned loaded，自有PendingOps。prepare复制旧队列引用并以既有原子push准备新快照，持久状态和旧队列不变；apply转移全部准备队列并touch缓存，全程不分配。候选锁保持到discard，拒绝重入/重复采纳。每实例累积操作、sticky base及最早期限；uint64溢出和Infinity饱和为UINT64_MAX。pin查询供组合cache hooks使用，确认持久化后release释放队列和pin，失败Session可discard_all后关闭缓存。所有新接口独立，旧ABI和TUI入口保持。
- 实际验证：24个文档两轮连续编辑，结果/折叠队列/所有权、最早deadline/Infinity/溢出、pin阻止sweep、重入prepare及取消候选、checkpoint sticky base通过；真实MemoryStorage保存base后释放pin。新建/已有holds两种路径共52个直接分配/插入/部分复制故障，失败保留旧值、队列、期限及Session健康，O0/O2 ASan/UBSan通过。最终完整Runtime O0/O2 UBSan和固定Pi/Chord差异基线通过；既有3条delta ABI拒绝继续单列。共享及隔离暂存核心树G00/Assembly清单通过，隔离产品构建通过。首次隔离产品命令把输出设在checkout内被构建器拒绝，改为独立临时输出后通过。
- 同一实施Agent自审：仅新增holds服务及实际汇编构建/导出/来源登记、Runtime专项与TODO，检查器/预算未变。8个候选路径内容清单摘要（加入本记录前）：5e00a8d947245c7ca6aaf829e7b8516f3413d6727fdcccc3a84f38e2ad748457。结论为本地候选通过。后续接正常事务takeHeld合并、批量持久flush及版本切换/关闭/timer服务；K03尚未完成。TUI未交接文件与登记保留工作树。

## K03 holds统一持久flush（2026-10-26）

- 来源：Pi d5b3de22 Session.#storeHolds/#release、runCommit调用Transaction.takeHeld，session-deferred-commits.test.ts的base/delta选择、存储失败与stored subscribers场景。本批fetch后Pi/Harness仍为d5b3de22 / 22ddee37。
- 实现：holds.S新增独立pg_document_holds_flush。先复制PendingOps准备完整document.change数组及持久计数，统一records.commit；准备失败不排空旧队列。同步adopt只更新已持有loaded的持久重放计数，内存值指针/版本保持；成功settlement后释放所有队列与pin并touch缓存，再调用原publish。内存observer不重复通知。Session hooks/context在所有路径恢复，准入后后端/采纳错误锁存Session、保留holds供失败清理；原生delta计数上限在准入前拒绝，选定base可重置。
- 实际验证：24文档两轮延迟修改一次flush，逐文档读取真实MemoryStorage值和计数一致；base/delta、值身份、成功前pin/成功后解pin、adopt→release→publish顺序、回调重入拒绝、空holds/只读及计数上限通过。拒绝、漏adopt、重复adopt、错误序号、consumer失败及adopt后后端拒绝六类错误全部fail-stop，hooks恢复且值不回滚。新增37个直接分配/插入/部分复制故障逐个验证旧队列/值保留（准备失败健康，后端失败锁存）。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及原Pi/Chord差异基线通过；既有3条delta ABI边界单列。共享与最终隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：仅扩展holds接口/实现、实际records依赖和导出/来源、专项及TODO，无门禁或预算改动。5个候选路径内容清单摘要（本记录加入前）：9b568c9389f2374772c63712791dd874ffeba65b250b7a4b767355041f37dd8c。结论为本地候选通过；正常事务takeHeld合并、版本切换/关闭与timer服务继续接入，K03尚未完成。TUI未交接文件与登记保留工作树。

## K03 正常事务takeHeld与延迟回退合并（2026-10-26）

- 来源：Pi d5b3de22 Transaction.takeHeld/#insertBeforeRetirement/adopt、Session.runCommit与session-deferred-commits.test.ts的storing held changes。Pi/Harness本批fetch前后仍为d5b3de22 / 22ddee37。
- 实现：新增32字节注入merge服务；Doc Tx完成所有checkpoint后准备合并，再准备值采纳。手写holds服务复制待写队列，与own ops折叠，使用最终prepared值；本次checkpoint base覆盖队列内容。私有Storage数组按旧write身份替换，或在同ID退休之前插入。read-only acquired Doc仅随其它写入落盘；空批次及无关holds保持。合并后复核转换副本的源冲突。确认后按原value guard采纳值/持久计数，在cache eviction前释放匹配holds，维持原生loaded借用有效期；准入后失败遵循Session fail-stop且不回滚已采纳事实。原有结构/字段/枚举保持，独立merge/deferred_merge入口提供服务组合。deferred在合并前判定holdable，符合条件继续内存采纳，混合/退休整批合并落盘，固定回退决定。
- 实际验证：继续编辑的held+own折叠、read+Entry、纯读和无关Entry保留holds、仅record退休/编辑后退休、本次base优先/held sticky base、多个文档部分释放及最早剩余期限、先插入退休写再按身份替换后续write、deferred继续与混合/退休回退通过。真实MemoryStorage校验最终值/计数/退休身份，退休释放无悬空cache借用。六种后端/采纳失败保持fail-stop，adopt后的事实不回滚。单编辑69、仅record退休45、多编辑131个直接分配/插入/部分复制故障，失败保留旧值/队列/pin并恢复原hooks。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及原Pi/Chord差异基线通过，既有3条delta ABI边界单列；隔离核心暂存树G00/Assembly清单与产品构建通过。
- 同一实施Agent自审：新增独立服务与结算导出、来源/实际依赖登记及专项，检查器/预算未改。7个候选路径内容清单摘要（本记录加入前）：8ac98702f1f19dd0d90eeaf8b4dbfd618c092fccb3025d66f9e697581bb64bcd。结论为本地候选通过；版本切换前flush、正常关闭及deadline/timer服务继续接入，K03尚未完成。TUI未交接文件与登记保留工作树。

## K03 版本重载前单实例flush与holds结束（2026-10-26）

- 来源：Pi d5b3de22 Session.#loadDocument/#storeHolds/close及session-deferred-commits.test.ts的reload-before-drop、close/failure行为；本批fetch前后Pi/Harness仍为d5b3de22 / 22ddee37。
- 实现：手写flush_one复用既有统一持久flush，按incarnation只选择目标队列、持久计数与释放；其它holds/pin/deadline保持。Cache在版本flush回调维持独占guard，touch_flushing仅在该guard内开放确认后的pin触达，并以子guard拒绝clock/pin回调嵌套触达；通用cache API仍拒绝重入。后端失败保留旧cache loaded，成功后才驱逐并从已保存值迁移重载。新增holds_finish用于宿主停止admission后的队列结束：健康时排空，失败时跳过写入，所有结果清理/关闭owner，保留首个Session错误并支持重复结束；宿主随后关闭Cache/Storage。
- 实际验证：两个held文档中只保存被重载者，另一队列/期限保持；真实MemoryStorage完成v1 held→v2迁移→回到v1→v2编辑/base提交，数据和版本一致。后端拒绝保留旧loaded，缺失incarnation无写，独立touch入口在普通/嵌套guard下拒绝。版本重载62个直接分配/插入/部分复制故障，分别覆盖保存前保留旧队列和保存后重载失败的可恢复状态。结束服务65个故障点覆盖健康、既有失败、提交失败与重复结束，释放全部queue/pin且保留首因。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过，既有3条delta ABI边界继续单列；共享与隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：独立增量接口/实际导出与来源登记，旧结构/函数表/枚举保持，检查器/预算未改。7个候选路径内容清单摘要（本记录加入前）：a53a16c02469bf88a8fe59fc7c1618fa930d4f2a5105df3675a949e8b06f30a4。结论为本地候选通过；deadline/timer及完整Session admission/关闭组合、查询/只读视图继续接入，K03尚未完成。TUI未交接文件与登记保留工作树。

## K03 holds前台定时器控制器（2026-10-26）

- 来源：Pi d5b3de22 Session.#armHoldTimer/#storeHolds/close、timers.ts的Timers.foreground契约及session-deferred-commits.test.ts的deadline/queued store场景。本批Pi/Harness fetch前后仍为d5b3de22 / 22ddee37。
- 实现：新增手写hold_timer.S及独立72字节controller/32字节provider ABI。消费注入的mandatory foreground/cancel/Session enqueue；sync在held变更/释放后维护最早有限期限，仅deadline改变时取消/重设，Infinity无handle，过期期限映射0 delay。定时交付只标记fired并入队，既有fired ticket保留到job运行；串行job统一保存所有当前holds，不重复内存观察。stop先封闭timer并取消handle，已入队ticket保留借用并返回pending，job排空时不写Storage，随后stop完成，宿主再finish holds并关闭Cache/Storage。partial handle在arm失败时取消，queue拒绝/持久保存错误保留首个Session失败。
- 实际验证：相同deadline复用、更早期限重设、Infinity/过期、重复交付不重复入队、Session忙时只入队、fired后继续deferred且不重设、批量保存最新内容、关闭期间排空跳过写入、既有失败、provider拒绝/partial handle/queue拒绝通过。67个直接分配/插入/部分复制故障覆盖controller/provider分配和已入队持久批次失败。O0/O2 ASan/UBSan专项、最终完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过；既有3条delta ABI边界单列。共享与隔离核心暂存树G00/Assembly清单、隔离产品构建通过。一次专项使用旧fixture的固定delta计数断言失败，修正timer fixture的checkpoint依赖后按最终树重跑通过。
- 同一实施Agent自审：只登记新ASM/头、实际构建依赖/导出、专项及TODO，检查器/预算保持。8个候选路径内容清单摘要（本记录加入前）：7f3e6f11e86cfc620e04c54054eab10d80bf2101f9cecb42b708289a4300e38a。结论为本地候选通过。controller提供者的原生Timers/队列由K06实现，Session实际组合负责held变更后的sync与完整关闭admission；K03总验收尚未完成。TUI未交接文件与登记保留工作树。

## K03 Durable Session组合与关闭

- 来源：固定Pi d5b3de22 Session.runCommit/#loadDocument/close/fail及相关session-deferred-commits/adoption/documents测试。fetch发现origin/main前进至f9daca3480732d0042b1df3a6e27221f597a620d；已核对本批Session/Transaction/Timers源码diff为0，固定工作树/差异基线保持d5b3de22。新增ae823bc的工具全名/双下划线后缀归一化优先排序及回归已映射K11；Harness仍为22ddee37。
- 实现：新增手写durable_session.S和192字节组合ABI，绑定既有records/cache/holds/timer及后端close。begin/settle/abort维护单一已接收事务和Doc借用；deferred/merge复用原服务，timer设定在内存观察前、释放后在持久观察前同步。Cache组合原clock/pin服务，版本切换只flush目标并调整deadline。idle为队列派发提供执行线状态。close先封闭新工作，已接收工作仍可正常/deferred结算或abort；等待timer队列清理后，结束holds、关闭cache/恢复hooks、轮询后端close，重复调用复用结果。后端错误保留已有失败原因。
- 首因修复：新联调在采纳内重设timer分配失败，暴露records后续adoption/backend协议错误覆盖已有原因。对照Pi fail first-wins，records在adopt_bad和终态fail边界均保留首因并返回该原因；字段/枚举/结构保持。新增consumer allocation、backend已有cause和callback自行fail后返回三类回归。
- 实际验证：自动deadline设定/释放、忙时不派发、admission封闭、已接收工作在关闭中deferred保存或abort、timer pending清理、内存/持久观察者内请求close、后端pending/错误、重复close与hooks恢复通过。21个deferred/设timer、37个close清理、71个已持久采纳后重设timer分配/插入/部分复制故障，共129个位置，验证旧状态/已采纳事实与首因；重复绑定及Doc scope失败时record候选清理通过。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及原Pi/Chord差异基线通过，既有3条delta ABI边界继续单列；共享及隔离核心暂存树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：新ASM与头、实际构建/依赖/导出登记及必要首因修复，无门禁/预算变更。11个候选路径内容清单摘要（本记录加入前）：d368d7b42ada694cf3f7eba19ea478dc01c401172b0ec626434b1b1fbc60d8bf。结论为本地候选通过。后续K03历史/只读视图与查询、K06实际Timers/队列提供者及K14产品组合继续按TODO推进，K03总验收尚未完成。TUI未交接文件与登记保留工作树。

## K03 当前与Entry历史只读快照

- 来源：Pi d5b3de22 Session.snapshot/snapshotAsOf/GuardedStorage、documents.materializeDocument及session-document-retention/session-checkpoints-migrations测试。本批fetch后HEAD=d5b3de22、origin/main=f9daca3，本批Session/Documents源码diff为0；Harness HEAD/origin仍为22ddee37。
- 实现：新增手写document_reader.S；当前同版本cache_peek读已采纳值（包含deferred），返回独立owned JSON；有事务借用时miss/版本变化返回PENDING供队列重试，历史读始终等待执行线。snapshot_as_of解析可见Entry的实际conversationId/commitSeq，使用共享loaded_open_at完成point查询、语义核对与隔离迁移，无cache/迁移写回；缺失Doc和不可见Entry区分。现有loaded_open/cache_get复用同一实现，旧结构/函数表/枚举保持。读取guard在每次provider调用前后保留首因/阻止失败后的后续调用；current的临时provider代理始终恢复，close等待已接收读取完成。Storage正常返回missing content使该快照报错且保持Session健康，与实际GuardedStorage区分。
- 实际验证：126组直接执行实际snapshotAsOf方法、GuardedStorage类及materializeDocument，Native的值/原会话/point/调用序列/failed结果一致。真实MemoryStorage验证创建前、历史base/delta、退休、同址重建、fork祖先Entry/不可见后续Entry、空/NUL family key、迁移不写回、readonly读取、deferred快照与版本重载、输出不别名及关闭后仍存活。12类读取guard用例覆盖部分输出、错误Entry元数据/seq、backend错误、回调内Session失败后不再read、missing content健康错误、关闭等待当前读及provider恢复。冷当前21/热当前1/历史22共44个直接分配/插入/部分复制故障通过。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan与固定Pi/Chord差异基线通过（既有3条delta ABI边界单列）；共享及隔离核心暂存树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：新reader服务与实际构建/导出/依赖/来源登记，当前definition按cache契约借用至eviction/close，历史provider/definition仅借用到返回。11个候选路径内容清单摘要（本记录加入前）：14bf42075595277731db65a5b1f5979c1cb5164409ffb82d97300d3ad57cc40b。结论为本地候选通过；K03剩余事务查询/只读边界与总验收继续推进，Harness视图按K13实施。TUI未交接文件与登记保留工作树。

## K03 conversation/request索引与Tx读取

- 来源：Pi d5b3de22 Transaction.submissionByRequest/#read与MemoryStorage.commit/submissionByRequest。fetch后固定HEAD仍d5b3de22、origin/main=f9daca3，Harness HEAD/origin为22ddee37。
- 实现：手写record_memory按原始submission写入顺序准备json-c索引，先删本ID仍持有的旧key，再安装新conversation/request key；ID候选压缩不改变索引覆盖顺序，winner移除不恢复历史被覆盖项。成熟json-c承载索引，canonical JSON tuple编码分段/NUL键；数值conversation归一化。rows私有allocation prefix拥有索引，容量预留/索引构造在采纳前完成，成功一起安装并在close释放；公开Memory48/Row32保持。独立request query/provider及Tx入口复用table-write barrier、sealed/reentry拒绝与partial输出清理，返回独立owned记录。
- 实际验证：实际Pi MemoryStorage新增80轮随机批次、640次conversation/request查询；总5982操作差异通过，覆盖相同ID多次写入、乱序ID、覆盖winner后改名/移除、conversation变化、空/NUL/Unicode key。Tx在普通文档写后仍可读，任何table-write尝试后拒绝，sealed/partial错误/reentry/输出隔离通过。新索引/row分配33、旧索引原子更新60、Storage查询8、Tx查询8，共109个直接分配/插入/序列化/部分复制故障保持旧rows/index/sequence；新增string_len与序列化故障包装也扩展既有Runtime路径覆盖。最终O0/O2 ASan/UBSan专项通过；完整Runtime O0/O2 UBSan和固定Pi/Chord差异基线通过，新增首次索引专项随后两优化级复验；既有3条delta ABI边界保持单列。共享与隔离核心树G00/Assembly清单、隔离产品构建通过。共享DSO首次构建发现函数地址PC-relative relocation，改用GOTPCREL后实际差异复验通过。
- 同一实施Agent自审：仅扩展既有模块/导出/来源登记，旧布局及门禁/预算保持。9个候选路径内容清单摘要（本记录加入前）：4d4c0267ef094408ce92098d797e64b8e61cf8d9a0034bd7386e332864f5c5d8。结论为本地候选通过；K03类型化Entry与剩余构造/查询边界继续推进，总验收未完成。TUI未交接文件与登记保留工作树。

## K03 类型化Entry提交态读取

- 来源：固定Pi d5b3de22 Transaction.entry(first,second)和#read；本批fetch后origin/main仍为f9daca3，Harness为22ddee37，固定源保持d5b3de22。
- 实现：手写pg_record_tx_entry复用通用提交态读取，仅在可选kind指定时按JSON字符串类型、长度和字节过滤；空kind区别于不指定，嵌入NUL不截断。错误/不匹配释放结果，保持table-write barrier、sealed和provider reentry约束。独立slice/导出，原ABI保持。
- 实际验证：20组执行实际Pi Transaction.entry方法结果一致；真实MemoryStorage覆盖空/NUL kind、缺失/错误类型、不指定kind、write-after-read barrier、sealed与部分深复制失败清理。O0/O2 ASan/UBSan专项及完整Runtime O0/O2 UBSan、固定Pi/Chord差异基线通过；既有3条delta ABI边界继续单列。隔离核心树G00/Assembly清单和产品构建通过。
- 同一实施Agent自审：仅扩展既有records导出/来源、专项和TODO，门禁/预算保持。6个候选路径内容清单摘要（本记录加入前）：a981a0833623b8f010a187c2b367e4fe5cc4292454f53c7f3ffb4b23a2e47200。结论为本地候选通过；K03剩余构造/候选状态/查询及整体验收继续推进。TUI工作树保持独立。

## K03 原始Submission创建

- 来源：固定Pi d5b3de22 Transaction.createSubmission/#requireConversation/#write及harness-submissions的raw记录调用。本批fetch后Pi HEAD/origin仍d5b3de22/f9daca3，Harness为22ddee37。
- 实现：手写pg_record_tx_create_submission先通过已有owner查询核对提交态/本批新conversation，再mint、复制有限JSON draft并覆盖ID，准备独立返回快照和submission候选。失败不改变先前候选，已mint ID保持消耗；写尝试固定public table-read barrier。原始创建不执行额外admission策略，候选可继续placement/settlement，最终复用统一Storage批次。旧ABI保持。
- 实际验证：16组实际Pi createSubmission方法差异结果及mint调用次数一致；真实MemoryStorage覆盖提交态/本批root、输入/返回值修改不污染候选、创建后同批unanswered、missing owner不mint及read barrier。10个直接分配/插入/部分复制故障释放全部部分输出并保持旧候选/Session健康。O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过（既有3条delta ABI边界单列）；共享及隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：既有模块新增独立导出/来源登记、专项和TODO，门禁/预算不变。6个候选路径内容清单摘要（本记录加入前）：a5c35d183c5c771573252e1d68093f3763889952edf5e35eb8adfd17831b0081。结论为本地候选通过；K03剩余Task候选、构造/查询语义继续推进，总验收未完成。TUI工作树保持独立。

## K03 已写候选Task/Conversation快照

- 来源：Pi d5b3de22 Transaction.stagedTasks/stagedConversations与#taskEntry/#committedTask；本批fetch后Pi HEAD/origin=d5b3de22/f9daca3，Harness=22ddee37。
- 实现：手写records候选快照入口，Task以json-c kptr hash映射ID至输出位置，首次写入定序、后续替换最新记录；Conversation顺序保留。内部临时数组仅持有候选引用，最终深复制后返回owned输出，所有分配失败保留原Tx状态。查询不触发Storage，允许table writes后的候选检查；旧ABI保持。
- 实际验证：10组执行实际Pi stagedTasks/stagedConversations方法，Task顺序由写入建立，候选值和顺序一致。100次Task写入覆盖40个ID以及3个Conversation，验证最新值、输入/输出隔离、empty/sealed及Task143/Conversation5共148个直接分配/插入/部分复制故障。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan和固定Pi/Chord差异基线通过；既有3条delta ABI边界单列。共享与隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：既有模块独立导出/来源登记、专项和TODO，无门禁/预算变化。6个候选路径内容清单摘要（本记录加入前）：1729dcf375fb0f30d865882455bcfaa38ac1aa9b5935500f126a74b0b15ddbb5。结论为本地候选通过；完整Task缓存顺序仍待接入：Pi committedTask先读可预插入#tasksById，当前仅首次写入定序，此边界未作为完全等价通过。随后补Task读取缓存/候选状态及K03总验收。TUI工作树保持独立。

## K03 Task提交态缓存与首次访问顺序

- 来源：Pi d5b3de22 Transaction.#taskEntry/#committedTask/#currentTask/stagedTasks；本批fetch后Pi HEAD/origin=d5b3de22/f9daca3，Harness为22ddee37。
- 实现：公开Task读与内部currentTask fallback复用每ID缓存，成功/missing/provider拒绝各只读一次；返回独立副本，读写首次访问建立候选枚举顺序。json-c有序私有slot绑定事务候选数组，保持Tx48 ABI；Task最新候选按slot顺序输出，只读过而未写的ID不输出。分叉与holds批次重建转交私有元数据，Storage深复制省略元数据，commit/discard在候选数组删除前显式释放。先读后写候选顺序与Pi的差异已接通。
- 所有权修复：最初在json-c userdata destructor内递归释放元数据触发引用计数断言，独立非空array探针也复现此重入问题；改为Tx显式清理/转交并在Storage拷贝时排除内部metadata。故障路径及最终树重新验证，无该问题残留。对照脚本的TypeScript拼接与ctypes局部Tx类型声明错误均在工具中修正，未计入通过结果。
- 实际验证：执行实际Pi #taskEntry/#committedTask/stagedTasks方法对照四种先读顺序，重复成功/缺失/拒绝的值、provider次数和最终候选顺序一致；已有10组候选数组、5982个MemoryStorage操作与其余固定Pi/Chord基线保持通过。原生验证返回隔离、busy/reentry、缺失和错误缓存、分叉批次重建后顺序/释放；10个缓存直接分配/插入/部分复制故障通过，候选枚举故障范围扩展Task184/Conversation5。最终O0/O2 ASan/UBSan专项及完整Runtime O0/O2 UBSan通过，分叉保留用例随后两优化级补充复验；既有3条delta ABI边界继续单列。共享及隔离核心树G00/Assembly清单与隔离产品构建通过。
- 同一实施Agent自审：仅既有records/holds内部元数据生命周期、来源/实际模块登记、专项及TODO，公开布局保持，门禁/预算未改。7个候选路径内容清单摘要（本记录加入前）：9ca4cf7d56daf26f7c2d7754fd0949437f007d4a82f811eba7e1982238125300。结论为本地候选通过；后续补Task setTask终态/conversation/时间戳与createTask，K03总验收尚未完成。TUI工作树保持独立。

## K03 setTask候选替换与生命周期时间戳

- 来源：固定Pi d5b3de22 Transaction.setTask/#stampTimes/#taskEntry、session-tables终态/conversation拒绝和harness-tasks生命周期时间戳调用。本批fetch后Pi HEAD/origin=d5b3de22/f9daca3，Harness为22ddee37。
- 实现：手写pg_record_tx_set_task按同ID最新候选核对terminal/conversation，再复制并原地替换最新write；首次访问slot与原Tx ABI保持。时间戳candidate优先于input，null视为缺失，仅在running/terminal缺少对应字段时调用注入double时钟；有限小数/负值保持，非有限结果拒绝。clock回调在Tx busy下运行，失败保留旧候选并保持table-read barrier，返回/原记录不别名。
- 实际验证：80组执行实际Pi setTask/#stampTimes，terminal/conversation拒绝、candidate/input/null优先、小数时间、clock调用次数及最终候选一致。真实MemoryStorage验证pending→running→waiting→running→terminal，仅两次clock且跨阶段保留startedAt/endedAt；旧候选terminal拒绝后最终统一提交。NaN clock拒绝保留waiting候选；10个直接分配/插入/部分复制故障保留先前write，包含新增json_object_new_double故障包装。O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan与固定Pi/Chord差异基线通过，新增NaN用例随后两优化级专项复验；既有3条delta ABI边界单列。共享及隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：仅既有records独立导出/来源、专项和TODO，旧结构/枚举及门禁/预算保持。7个候选路径内容清单摘要（本记录加入前）：739e2221795de02309107137cbf84638cf6e39515a58bbac04f4e5f38e449461。结论为本地候选通过；随后移植createTask初始化/owner/输入和时间来源，K03总验收未完成。TUI工作树保持独立。

## K03 createTask submission trace/links准备

- 来源：固定Pi d5b3de22 Transaction.createTask开始的submissionIds options派生分支，位于owner/conversation/initial/mint之前。本批fetch后Pi HEAD/origin=d5b3de22/f9daca3，Harness为22ddee37。
- 实现：手写pg_record_tx_task_options按调用者submission顺序取最新staged候选或Storage提交态。首项trace缺失/null时选root，后续存在trace形成links（包含显式null），空列表移除原trace/links，省略submissionIds则保持原options。返回独立owned options，错误释放部分读取结果和候选输出，原writes保持；作为createTask的写尝试前置阶段固定table-read barrier。旧ABI保持。
- 实际验证：18组执行实际createTask分支的options及provider调用序列，与Native一致；真实MemoryStorage验证候选覆盖、旧trace/links覆盖、missing/root/null/空列表与输出所有权。12个直接分配/插入/部分复制故障保留原候选/Session健康。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过（既有3条delta ABI边界单列）；共享与隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：既有records独立导出/来源、专项和TODO，原结构/枚举及门禁/预算保持。6个候选路径内容清单摘要（本记录加入前）：e42c60320fa00d0dd53f1afcba58c0f835d9845501e9982f6545443432db818c。结论为本地候选通过；完整createTask仍需接owner/conversation、initializer、mint及pending记录构造，本批未将完整创建视为完成。K03总验收保持未完成，TUI工作树保持独立。

## K03 完整createTask pending创建

- 来源：固定Pi d5b3de22 Transaction.createTask/#currentTask/#requireConversation/#write，TaskCreationOptions、session-tables测试及Generation/Tool创建调用者。本批fetch后Pi HEAD/origin=d5b3de22/f9daca3，相关Durable事务/Session无差异；Harness为22ddee37。
- 实现：手写pg_record_tx_create_task，以独立40字节服务描述符注入initial/mint/telemetry/default scope；先准备submission追踪，再取最新候选/缓存owner，检查child background/conversation，核对conversation，然后initial→mint→trace inject→pending记录→私有暂存。输入、checkpoint和trace均隔离，null输入与可选trace提供者可用；异常清理partial callback/JSON输出，保留旧候选，已mint的ID保持消费。既有ABI不变，最终owner状态继续通过统一提交核验。
- 实际验证：73组执行实际Pi完整createTask方法，完整record、Storage/initial/mint/inject顺序及错误一致；前批18组submission选项对照保留。真实MemoryStorage完成创建→统一提交→独立读取、同批owner/child、无注入器、null输入，以及同批owner变terminal时子任务提交拒绝。49个直接分配/插入/部分复制故障保留prior候选/健康Session，增加boolean分配故障包装。最初测试initializer对局部deep-copy partial输出漏清理，ASan定位后修复测试callback，最终O0/O2 ASan/UBSan专项和完整Runtime O0/O2 UBSan均通过；固定Pi/Chord基线保持通过。共享及隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：仅records独立接口/导出/来源、相关专项与布尔分配测试包装、核心TODO；既有布局、门禁、预算保持。7个候选路径内容清单摘要（本记录加入前）：c81f87b31d4bdc1d6237daa5daca48352460e5d348ae574a17b773c39dd7bf49。隔离核心树fec483c1d72e95005d4c084106f5701e1cfd6496验证。结论为本地候选通过；已勾选类型化Task创建/替换，K03整体验收与任务phase/调度、整机组合继续按TODO执行。TUI工作树保持独立。

## K03 Storage点读/ID失败守卫

- 来源：Pi d5b3de22 SessionImpl与GuardedStorage.#call/close，harness-storage-failure参数错误/取消/失败场景。fetch后Pi HEAD/origin=d5b3de22/f9daca3，Session/Transaction无差异；Harness为22ddee37。
- 实现：手写pg_durable_storage_read/mint，独立56字节借用服务描述符适配既有Tx读取/分配回调。读取参数错误与取消豁免由服务分类，其他异常锁存Storage失败；mint异常与无效ID不豁免。调用前后检查首因，跨失败结果释放，正常missing的partial结果也释放。busy阻止provider回调中关闭后端和同描述符重入；closing已准入事务仍能完成读取/分配，回调结束恢复原Session busy。旧ABI保持。
- 实际验证：56组执行实际Pi GuardedStorage完整类，对照首次及后续结果、provider次数、request/cancel豁免与跨失败首因一致。原生补充close回调、closing已准入事务、重入、mint无效ID、partial owned结果清理；MemoryStorage通过guard作为Tx.read_context验证Submission创建/提交、读取和mint故障锁存。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及固定Pi/Chord基线通过；共享和隔离核心树G00/Assembly、隔离产品构建通过。差异工具初次未链接durable_session及其依赖，补齐专项共享库构建输入后重跑通过。
- 同一实施Agent自审：既有Session模块独立导出/服务类型、json-c依赖登记、专项和TODO，门禁/预算及旧布局保持。6个候选路径内容清单摘要（本记录加入前）：0e0ad7a336a7f020541482255bb0f3a3c9aabe1589a2d1fcda681c021ebc5a64；隔离核心树e5e8bc041591b24b1ff11d9f0b9d57f7b3907801。结论为本地候选通过。分页/head/request/document统一守卫、宿主失败关闭与整机服务仍按后续步骤接入，K03总验收保持未完成。TUI工作树独立保留。

## K03 分页/head/request/Entry失败守卫

- 来源：Pi d5b3de22 GuardedStorage的四表scan、findLatestHeadMarker、submissionByRequest、entry及#call，Transaction对应查询调用者与harness-storage-failure无效请求/取消用例。fetch后Pi HEAD/origin=d5b3de22/f9daca3，Harness为22ddee37。
- 实现：手写四种查询适配，独立48字节pg_durable_queries借用既有Storage guard；分页/head/request绑定现有Tx查询接口，Entry返回fork-aware record和序号。共享read completion函数替换前批点读重复分支；正常missing/异常/crossed失败清理partial结果，分页metadata与Entry sequence只在成功时转交。既有结构布局保持。
- 实际验证：32组原生四类查询×成功/missing/backend/request/cancel/crossed/reentry/close，验证同一首因与busy恢复；MemoryStorage通过Tx分页/head/request以及fork-aware Entry读取验证实际服务组合，无效分页拒绝后再次正常读取。3个分页直接分配/部分复制故障锁存失败并清空所有输出。O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan、56组实际Pi GuardedStorage及其余固定Pi/Chord差异基线通过；共享与隔离核心树G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：仅既有Session独立查询接口/导出、专项与TODO，共享失败函数收敛；门禁、预算和旧ABI保持。5个候选路径内容清单摘要（本记录加入前）：5026e3bf9565d393ed817affc03cc979640e2a0824cec1434103d8abe669f000；隔离核心树d3c95eaa6a0ac3cc555a7bbbb7ef79dd3834c3ee。结论为本地候选通过；Document提供者统一守卫和公开观察/只读完整验收继续推进，K03总验收未完成。TUI工作树独立保留。

## K03 Document提供者统一失败守卫

- 来源：Pi d5b3de22 GuardedStorage.findDocument/document/scanDocuments/#call、Session.snapshot/snapshotAsOf及失败/参数错误/取消测试。fetch发现origin由f9daca3更新89c921d，Session/Transaction无变化；新assignJson原型污染与URL/stream/终端加固已在核心TODO记录后续采用点。Harness保持22ddee37，固定对照HEAD保持d5b3de22。
- 实现：手写Document find/read/scan/历史Entry适配，独立48字节描述符共享既有guard、query dispatch、read completion；历史前端conversation/id转换为后端id/conversation，保持commit sequence。非致命参数错误/取消以EXEMPT明确传给外层快照，外层不重复锁存Storage失败；普通错误和跨失败仍遵循首因/partial清理。旧结构布局、回调签名保持。
- 实际验证：35组MemoryStorage当前/历史Document查询，覆盖成功、参数错误、取消、后端失败、跨失败及参数/取消后重新读取成功；页面metadata与partial JSON清理正确。首次测试误将guard已经锁存的Snapshot结果期望成普通Storage错误，改为实际SessionFailed语义后重新运行。最终O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan、固定Pi/Chord差异基线通过；共享及隔离核心G00/Assembly、隔离产品构建通过。
- 同一实施Agent自审：既有Session/reader独立导出与来源、专项、TODO，守卫/预算与旧ABI保持。6个候选路径内容清单摘要（本记录加入前）：cc4a169f1e93bc8e05b6e611aa469cbe4d8a918b51e0a4c2f22f55a716c41b24；隔离核心树7c1aa424985fb0f666111b53c4c40df5da1f13ad。结论为本地候选通过；公开观察/只读及K03整体验收继续，TUI工作树独立保留。

## K03 原生只读Session事务验收

- 来源：Pi d5b3de22 Session.#runCommit空写入/readOnly/deferred分支及harness-read-only测试的事务拒绝/保持健康场景；fetch后origin为89c921d，Session来源无差异，Harness保持22ddee37。
- 实际验证：16组真实Durable Session普通/deferred事务：空批成功；文档编辑/Entry/Task/Submission/retire/new Doc/混合写入拒绝，MemoryStorage内容与序号、Session采纳/发布、holds/timer均保持。拒绝后旧快照、空事务和关闭成功，新Doc不可见；创建时消费ID符合现有源语义。O0/O2 ASan/UBSan专项及共享G00/Assembly检查通过。
- 本批仅补充现有Session实现的完整只读事务验证，产品源码与模块登记未变；不重复产品构建。2个候选路径内容清单摘要（本记录加入前）：c8e20794e115800f4f8dec50c3d60415d39b79eda46adc51d267919fc9bb0b69。自审结论为只读事务边界通过；Harness只读打开/暂停调度/恢复随对应步骤验证，K03总验收保持未完成。TUI工作树保留。

## K03 同步listener宿主

- 来源：Pi d5b3de22 subscribeCommits/observeCommits/subscribeClose、#publish/#notifyClose与contained，以及session-documents同步/取消/关闭用例。fetch后Pi origin为89c921d，固定Session来源无差异，Harness保持22ddee37。
- 实现：手写listener注册/取消/独立channel分发/一次关闭；callback+context身份去重，取消紧缩保持顺序，容量按几何增长，通知先复制snapshot。internal错误保留首因并锁存adoption失败，外部和close错误报告且继续通知；callback同步返回错误码，publication/context仅借用。分配失败保留注册/closed状态可重试；旧ABI保持。
- 实际验证：三类通知身份/顺序/快照，callback内增删、已取消仍执行本轮而下一轮消失、内部首因/外部报告、重入拒绝、close一次及注册/分发/close三个直接分配失败。1000次注册/取消容量保持8，无空槽累计。最终O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan和固定Pi/Chord差异基线通过；共享及隔离核心G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：既有Session独立类型/导出/来源、专项与TODO；门禁、预算、既有ABI保持。5个候选路径内容清单摘要（本记录加入前）：ce68835540ea7a58dfc3a60526d68a7715118c83aef57906e5b309dce421b849；隔离核心树75d1b232131aa5e79d40a3a190331e09aec9229f。结论为listener宿主本地通过；observed/stored publication构造、Session接线与Document watch继续推进，完整观察与K03总验收保持未完成。TUI工作树保留。

## K03 observed/stored publication构造

- 来源：Pi d5b3de22 #publish/#hold/#storeHolds、publication/DocumentCommitChange类型与deferred观察测试；fetch后origin保持89c921d，相关Session来源无差异，Harness保持22ddee37。
- 实现：手写publication准备/释放/分发，普通表过滤保留原序、排除Document StorageWrites，追加预备文档集合。observed/stored输入集合相同时保留输出共享身份，否则分别形成含/不含已观察held内容的集合；memory仅observed、held flush仅stored，正常提交内部先外部后。owned输出隔离于输入，失败返回零输出；类型匹配核对字节长度，含NUL伪前缀不误匹配。
- 实际验证：16组执行实际Pi #publish构造分支，数据及共享身份一致；六组stored/memory/held×共享/分离集合验证与25个直接分配/插入/部分复制故障通过。O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan和固定Pi/Chord差异基线通过；共享及隔离核心树G00/Assembly、隔离产品构建通过。
- 同一实施Agent自审：既有Session独立类型/导出/来源、专项和TODO；旧ABI、门禁、预算保持。6个候选路径内容清单摘要（本记录加入前）：7f9ea48d0da8e8d9641092ac40d09b738629bb6a844ea5e14785b1d6b3e32ec5；隔离核心树f7e7e82d02eab7acd05e01102fe481659348acba。结论为publication构造/分发本地通过；实际Doc adoption/holds变更构造、Session接线与Document watch仍待接入，完整观察与K03总验收未完成。TUI工作树保留。

## K03 Document计划通知描述

- 来源：Pi d5b3de22 describe/publishes/adopt/adoptHeld与Session.#storeHolds，原生DocTx的Prepared/安装/退休驱逐路径。fetch后Pi origin为89c921d，固定来源无变化，Harness为22ddee37。
- 实现：手写独立通知描述入口，退休优先于copy，退休null value/空ops/无version；copy保留source；创建空ops，loaded更新与迁移保留prepared ops；Session-scope省略conversation。输出独立拥有record/value/ops，避免退休驱逐后读取已释放loaded句柄。入口接收最终record与上层解析的conversation，生命周期stamp/自动计划捕获继续接入。
- 实际验证：36组执行实际Pi describe函数，对照创建/更新/空ops迁移/退休/copy及优先级、缺省字段；原生输入/输出隔离和13个直接分配/插入/部分复制故障通过。最初局部代码label与rodata同名，汇编器拒绝后改为独立key label，最终树重跑通过。O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan与固定Pi/Chord差异基线通过；共享/隔离核心G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：既有Session独立描述契约/导出/来源、专项和TODO；旧ABI、门禁、预算保持。6个候选路径内容清单摘要（本记录加入前）：38208b691ed1723b676568ecf23c927c8563b9103dac6ca8b3282233fb3d787a；隔离核心树2cccbb834825ccbe65a70f34d6a021e51603cc1b。结论为计划通知描述本地通过；finalizer预备、adoption补stamp与Session/holds实际通知接线、Document watch继续，K03总验收未完成。TUI工作树保留。

## K03 DocTx持久提交实际通知管线

- 来源：Pi d5b3de22 Transaction.adopt/describe与Session.#runCommit/#publish，对照原生DocTx finalizer/adoption/install/retire/publish生命周期。fetch后Pi origin保持89c921d，Harness保持22ddee37。
- 实现：手写pg_document_tx_observed与40字节通知宿主，持久结算可组合held merge；finalizer末尾Storage前prepare，确认时无分配adopt补seq，native采纳/安装/驱逐后publish，任何已尝试prepare的路径都discard。既有原Session adopt/publish钩子保留，旧结构布局不变；内部结算上下文显式持有descriptor/token/attempted标志。
- 实际验证：真实MemoryStorage编辑、退休、partial prepare失败、adopt失败和Storage失败五条路径；准备实际Document通知、确认时原地补退休stamp、驱逐后仍可发布独立record，失败不发布且token清理一次，首因/Storage序号符合实际结果。测试最初误写错误枚举名及json-c setter成功值，按公开头修正后重新验证。O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过；共享与隔离核心G00/Assembly清单、隔离产品构建通过。
- 同一实施Agent自审：既有DocTx独立服务/导出/来源、专项和TODO；公开旧ABI、门禁、预算保持。5个候选路径内容清单摘要（本记录加入前）：98e5534713256962f5bd4be4d709efddeb129d4035528fc16df5a5344575ae30；隔离核心树d0f4f829e37264e10ecaa78212ece4178a165b0e。结论为持久DocTx通知管线本地通过；默认Session通知宿主、deferred/holds通知集合及Document watch继续接入，K03总验收未完成。TUI工作树保留。

## K03 publication无分配确认

- 来源：Pi d5b3de22 Transaction.adopt创建/退休生命周期字段与Session.#publish seq，实际DocTx通知管线；fetch后Pi origin保持89c921d、Harness保持22ddee37。
- 实现：手写publication确认，先校验两个owned通道和Document生命周期字段，再更新seq及零createdAt/retiredAt占位值；已有正数stamp保留，shared对象可重复遍历而不改变原stamp。确认不分配，调用者在准备/发布之间仅确认一次。类型匹配按完整字节，NUL伪前缀不作Document处理；memory-only无stored通道拒绝。
- 实际验证：共享/分离通道的新建/退休/copy stamp、输入隔离、损坏后通道/非法seq拒绝且前通道保持，确认期间分配计数0。真实DocTx通知host已组合Document notice→完整publication prepare→confirm，覆盖编辑、退休、partial prepare失败、adoption失败、Storage失败与一次清理。O0/O2 ASan/UBSan专项、完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过；共享及隔离核心G00/Assembly、隔离产品构建通过。
- 同一实施Agent自审：既有Session独立确认导出/契约、DocTx组合专项、来源与TODO；旧ABI、门禁和预算保持。5个候选路径内容清单摘要（本记录加入前）：4e00f76b9c2a9edca095693474a93a598d349d77b4cb731dcbf1ea6486c00ac2；隔离核心树93a86ba590a10551d46f70d6d26f2221171a78f7。结论为确认阶段本地通过；默认宿主自动计划枚举/任务owner解析/held集合捕获和Document watch继续，K03总验收未完成。TUI工作树保留。

## K03 默认持久通知宿主与实际Session接入

- 来源：Pi d5b3de22 adopt/describe/takeHeld/#currentTask、Session.#publish/#storeHolds与deferred通知用例；fetch后Pi origin保持89c921d，Harness保持22ddee37。
- 实现：手写默认通知宿主，自动枚举DocTx entries及未加载raw copy，独立record/value/ops，解析三种scope/任务所属会话，创建/退休stamp在Storage确认补seq。held合并克隆pending队列计算完整stored ops而不触动权威holds，观察集合只保留本次新操作；base/delta均保持源通知语义。无commit订阅者不分配通知，跨Session listeners拒绝。实际Durable Session持久结算新入口保持旧hooks、active Tx释放、held merge及timer同步。
- 实际验证：13组默认宿主行为、2组实际Session普通/held提交与关闭；无订阅分配计数0、跨Session拒绝。72个普通及141个held分配/插入/部分复制故障通过。最初测试把Session/task定义错误附加conversation策略，按实际scope修正；仅读且无写事务按源为空提交，不冲刷held，测试加入真实Entry写入后验证takeHeld差异。最终O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan和固定Pi/Chord差异基线通过；共享及隔离核心G00/Assembly、隔离产品构建通过。
- 同一实施Agent自审：既有Session默认服务/独立入口、records内部Task owner解析、专项与来源/TODO；旧ABI、门禁和预算保持。6个候选路径内容清单摘要（本记录加入前）：0f17a340af170e6038d8951d4b8f2752c9a199562a89615c927ce6ce88bd5c74；隔离核心树a7f5aac822c4452f6dc54dabe1de4980e66e9517。结论为默认持久通知本地通过；deferred memory、holds独立flush、close通知和Document watch继续接入，K03总验收未完成。TUI工作树保留。

## K03 deferred实际内存通知与持久回退

- 来源：Pi d5b3de22 adoptHeld/holdable和Session.#hold/#runCommit，deferred发布与只读用例。fetch后Pi origin由89c921d推进363ad78（模型配置），相关Session/Transaction无变化；K05后续采用点已登记。固定HEAD保持d5b3de22，Harness为22ddee37。
- 实现：手写deferred observed结算、默认双路通知服务和实际Session入口。准备阶段持有独立memory/stored payload，seq=0确认只选memory，不分配/改持久序号；值与holds采纳、既有observer/timer衔接后只通知内部。混合写入/held回退走普通seq确认和完整持久通知。所有partial与已选token由现有生命周期一次释放，旧ABI保持。
- 实际验证：实际Session memory-only/混合/已有held回退/空事务/只读拒绝五组流程与关闭，memory无seq且不通知durable，持久版本/Storage序号保持，fallback通知完整。首次内存/混合回退/已有held三个故障组合分别60/83/77，共220个直接分配/插入/部分复制故障通过。O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过；共享及隔离核心G00/Assembly、隔离产品构建通过。
- 同一实施Agent自审：既有DocTx/Session独立接口与默认服务、私有token双payload、专项/来源/TODO；公开旧ABI、门禁、预算保持。7个候选路径内容清单摘要（本记录加入前）：47e4d850297794c522b62fc4761702435e5dd780de465059b165d39be68420c1；隔离核心树bce5e133691cd6d591a055ae4df5cdddcde6b6f0。结论为deferred内存通知与回退本地通过；独立holds flush/close通知、Document watch继续，K03总验收未完成。TUI工作树保留。

## K03 独立holds flush持久通知

- 本批固定来源：Pi d5b3de22 #storeHolds，holds规划/采纳/释放路径。手写通用flush观察宿主，在每个选中hold的既有storeHeld结果上捕获完整ops，不额外重放PendingOps；默认服务生成owned stored-only通知，确认后按固定基线在释放pin后发布，原Session hooks保持。partial open/append/prepare/adopt/Storage结果统一discard。
- 实际验证：base/delta×all/selected/missing六组与64个直接分配/插入/部分复制故障，内部memory观察不重复，payload值/ops/seq正确，失败前保留held队列或遵循Storage fail-stop。最终O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan、固定Pi/Chord差异基线通过；共享/隔离核心G00/Assembly、隔离产品构建通过。
- 上游检查发现8b0caefc2260a28d2cec0a7ce3daef1d3de24776，已核对新Session #runCommit/#publish/#storeHolds/#attachDocument/#observeDocument：发布转至Storage adopt，内部观察同步、host microtask dispatch；watch跟随地址并新增observeDoc。来源/必要采用点及新测试入口已列入TODO下一优先项。本批保留d5b3de22固定证据，不声称符合这一新语义，后续将原地调整通知时序后再做自动timer/reload/close接线与watch。Harness保持22ddee37。
- 同一实施Agent自审：既有holds/Session独立通知接口/默认服务、专项及来源/TODO；旧ABI、门禁和预算保持。7个候选路径内容清单摘要（本记录加入前）：1f142ec46e6de18ed19a5d1d83f72b1459d926d9c3e72667bfb22705d54c28e3；隔离核心树f3d9ee95a51a5587bac7d5ca7bbeac4a67058dee。结论为固定基线独立flush捕获通过，K03总验收保持未完成。TUI工作树保留。

## K03 Pi 8b0caef host dispatch队列

- 新固定来源：Pi 8b0caefc2260a28d2cec0a7ce3daef1d3de24776 #notifyHosts/#publish/#runCommit，session-adoption/session-observers；fetch后origin保持8b0caef，基础/存储既有差异基线继续d5b3de22，Harness保持22ddee37。
- 实现：手写独立40字节dispatch owner，prepare保留immutable publication并分配owned token，enqueue无分配/无回调转交FIFO；显式drain是原生microtask边界，按派发时订阅集合执行，回调可追加后续事件。prepared与queued所有权区分，跨队列/重复入队/取消queued/reentry拒绝，snapshot分配失败原事件保留可retry；所有资源排空后才能close，listeners必须覆盖此借用期。
- 实际验证：执行真实8b0caef #notifyHosts方法，Native的FIFO、late subscription、callback内取消/新增及追加事件顺序逐项一致。原生测试enqueue分配计数0且不内联回调、publication引用存活、外队列token、重入、准备/dispatch分配故障和retry、prepared未提交时close等待及discard清理。O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan及固定Pi/Chord差异基线通过；共享/隔离核心G00/Assembly、隔离产品构建通过。
- 同一实施Agent自审：既有Session独立队列契约/导出、新提交来源、专项与TODO，旧ABI、门禁和预算保持。6个候选路径内容清单摘要（本记录加入前）：e0be5092809e2ef7aeb9e64d5b453bb9eb098e0e1716e66e63c99e4cb55777f7；隔离核心树48f903c8a15081259d61d8a518965902edb7e7e4。结论为新host dispatch队列本地通过；实际DocTx/flush adoption发布、观察者错误结算与新watch继续接入，未将新adoption总语义/K03总验收勾选。TUI工作树保留。

## K03 8b0caef实际adoption发布与错误优先级

- 固定来源：Pi 8b0caef #runCommit/#publish/#notifyHosts/#storeHolds、session-adoption/session-observers；fetch后origin为6c384aa（ai workspace解析），本批Durable/Chord函数无变化；Harness保持22ddee37。
- 实现：DocTx与holds通知publish原地进入native adopt之后；新增同步observer首错捕获与queued adoption服务，内部观察前无分配排队host通知，在Storage同步步骤外派发。后端适配器保持真实Storage状态，在错误分类前排空微任务；observer错误留到结算后处理，Storage错误优先并报告observer错误。adopt后拒绝保留已发布结果，队列own publication至派发；memory通知同步、混合落盘复用相同服务，旧ABI保持。
- 实际验证：五组真实MemoryStorage adopt/observed/adopted/host/resolve顺序、adopt前/后拒绝、observer错误及双错误优先级；三组held flush及三组实际Session stored/memory/混合路径。直接执行新Pi实际#runCommit/#publish/#notifyHosts方法，五组矩阵一致。普通/memory/held故障分别73/62/65次fail-stop或拒绝，另2次host snapshot失败在回调前安全重试并完整派发，未作为拒绝或遗漏。一个旧故障断言按成功memory采纳后Session失败但提交结果保留的语义修正。差异probe首次缺少未执行分支的私有#hold声明，补齐测试stub后重跑；调试导入产生的空__pycache__目录由G00拦截，清理后检查通过。
- 最终O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan、固定Pi/Chord差异基线与新8b0caef方法矩阵通过；共享/隔离核心G00/Assembly、隔离产品构建通过。
- 同一实施Agent自审：既有DocTx/holds通知时点、Session独立queued服务/错误捕获/Storage适配器、来源/专项/TODO；公开旧布局、门禁和预算保持。10个候选路径内容清单摘要（本记录加入前）：8feb50c8bbf8a460235f9a2744730a55ba8a64babb900dd420202f2a405c40e6；隔离核心树7bd21f05ab4bc8ec228b504225436f886583eb08。结论为实际adoption发布与错误结算本地通过；自动timer/reload/close通知接线、地址跟随watch/observeDoc继续，K03总验收未完成。TUI工作树保留。

## K03 定时、重载与关闭的自动持久通知

- 来源：固定Pi 8b0caef的Session.#armHoldTimer/#storeHolds/#loadDocument/close与deferred/adoption测试；fetch后Pi origin保持6c384aa、Harness origin/master保持22ddee37。
- 实现：手写timer扩展控制器借用holds通知服务，自动timer job、选定版本重载、健康close使用同一adoption发布；失败close只释放。基础timer/Session布局保持，扩展所有权与结束时点在公开头声明，新增两个导出随模块登记。
- 实际验证：automatic_flush_tests四种实际Session路径通过，内部观察一次且持久通知仅保存时一次；检查序号、内容、迁移、timer释放与重复关闭。O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan及固定Pi/Chord和8b方法差异通过。共享及隔离核心G00/Assembly、隔离产品构建通过。生成候选摘要首次被Git中文路径转义影响，改用NUL分隔读取后完成，未影响源或测试。
- 同一实施Agent自审：8个候选路径内容清单摘要（本记录加入前）97f96b64772a7288f4f6711e9b5de3b666f9b62f56c095408560dbb365aaf9b4；隔离核心树aa6e978f7c4dce7ec1bf8949703da0378d43b4da。结论为自动保存通知本地通过；close listener及地址跟随watch/observeDoc继续。K03全量未完成，TUI工作保留。

## K03 Session关闭通知与最终释放

- 来源：固定Pi 8b0caef的close/#notifyClose/subscribeClose；实际执行#notifyClose，核对停timer先于回调、快照取消不跳过、首回调错误报告后继续及重入一次性。
- 手写begin_close保存外层dispatch guard，快照分配成功后封闭订阅并移除close集合，保留internal/durable观察直到已接收工作和holds保存完成。独立close_notified在Session封闭准入后停timer/发close，复用实际分阶段关闭，最终排空host queue及释放注册。公开布局保持，新增导出在现有模块登记。
- 实际验证：直接与观察内部两条begin-close路径、回调取消/错误/重入、快照分配失败重试；四条实际Session自动保存路径核对一次关闭事件/报告、健康close持久通知、失败close不写和重复close。O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan、固定Pi/Chord差异和新增8b实际方法probe通过；共享与隔离核心G00/Assembly及隔离产品构建通过。
- 同一实施Agent自审：6个候选路径内容清单摘要（本记录加入前）81c3f261c068ccae93f779e36253b13f0228edaf0ab8890493d0cac8360b5686；隔离核心树74e3cd6eb5c90e6bd5f68343dd31d06f305d9ff8。结论为关闭通知生命周期本地通过；地址跟随watch/同步observeDoc继续，K03全量未完成。TUI工作保留。

## K03 逻辑地址观察状态转换

- 固定Pi 8b0caef #observeDocument/atAddress及watchDoc/observeDoc调用者，阅读session-observers退休/重建、session-watches地址与版本用例。Pi origin保持6c384aa，Harness origin/master保持22ddee37。
- 手写独立观察状态：checked loaded初始化后独立地址/值所有权，缓存驱逐后仍可用；按incarnation或完整逻辑地址跟随，退休保留last value身份并清除状态值，重建/版本变化生成root replacement，同版本空ops跳过。fallible事件准备完成后才替换状态。现有服务与ABI保持，4个导出登记到现有模块。
- 实际验证：三scope、kind/key/owner过滤、驱逐后转换、退休value身份与重复退休、重建/版本/空ops、12个直接分配/插入/复制故障；直接执行固定上游#observeDocument/atAddress，11组逐事件结果与原生一致。初始测试误为Session/Task definition附加Conversation策略，被实际cache检查拒绝；按scope改正夹具后重跑全部验证。
- 最终O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan与来源差异通过，共享/隔离核心G00和Assembly及隔离产品构建通过。隔离核心树6f64b04a96604d2c76d50c90ed3126c5fe44f87a；其后仅为本组件TODO加完成勾选，6路径最终内容清单摘要（本记录加入前）5937a89c56dd249089b131798f8921c0d74f109c841538311965401ba8eb7202，候选树3747dbe485180ed7d974d365e3f0d6364ce8581c。
- 同一实施Agent自审结论：地址跟随转换本地通过；下一批连接实际Session监听、pin/unpin、detach/owner结束/close及watch消费。K03完整观察项保持未勾选，TUI工作保留。

## K03 同步observer订阅与回调生命周期

- 固定Pi 8b0caef #observeDocument/guardObserver/endGuarded/#notifyClose与observeDoc调用者，阅读observer detach/end/error/terminal用例；fetch后Pi origin保持6c384aa、Harness保持22ddee37。
- 手写binding消费已有owned地址状态，登记internal/close监听，按incarnation组合注入pin/unpin；detach在同一publication/registry快照中立即生效，独立release拒绝正在派发的caller storage释放。host回调错误report、detach/end并包含end错误，保持Session健康；Task终态在本批文档变化后结束。实际DocTx修改/退休通过adoption通知抵达binding。
- 实际验证：六组生命周期矩阵、三种attach allocation/clone/registry故障、两个真实DocTx提交；直接执行Pi #observeDocument/guardObserver/#notifyClose与原生六组pin/advance/end/report结果一致。O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan与来源差异通过，共享/隔离核心G00/Assembly及隔离产品构建通过。
- 同一实施Agent自审：6候选路径内容清单摘要（本记录加入前）6f4b6c6416f0d52b2e08c2eb1c8d86fe8c30eb3648f58e5194f98a8419dd88c8；隔离核心树7bcbc74f0c61b4769d1c33da4038f9e8c4af8114。结论为同步订阅生命周期本地通过；native cache pin计数/idle、Session获取/取消与watch消费继续接入，K03全量未完成。TUI工作保留。

## K03 observer pin计数与真实缓存idle

- 固定Pi 8b0caef #pinDocument与#observeDocument；实际执行#pinDocument核对双owner计数、幂等释放、最后owner进入idle、同址替换后旧release不touch。上游Pi origin保持6c384aa、Harness保持22ddee37。
- 手写计数/布尔查询复用当前observer订阅集合，无额外索引分配；登记与暂定pin先于注入pin/touch，移除先于unpin，失败清除暂定计数并回滚附着。新增cache按incarnation条件touch，保持旧guard和缺失no-op语义。
- 实际验证：真实cache双observer阻止sweep、最后detach恢复idle并evict、旧identity/零ID、首次pin失败回滚及已附着pin转移失败内部锁存/关闭清理，现有observer回调同步核对native计数。新增失败用例发现新分支清除暂定pin后误落入成功返回，修正跳转保留错误后重跑；未将初次失败记为通过。
- 最终O0/O2 ASan/UBSan、完整Runtime O0/O2 UBSan与来源差异通过；共享/隔离核心G00/Assembly及隔离产品构建通过。同一实施Agent自审8路径候选清单摘要（本记录加入前）10ed59895d84eea78526b6ecc247d73bada4c17150227079e44be4f3214fbffd；隔离核心树23802aeee10c825ff4f42725146bf857d6709268。结论为计数与缓存idle组合本地通过；Session获取/取消及watch消费继续，K03全量未完成。TUI工作保留。
