# 首个原生 Agent 核心切片

2026-10-24：已经开始实际 Runtime 开发。运行 `python3 -B tools/runtime/build_test.py`，无需凭据/网络即可构建执行工具往返。产物在临时目录，执行后清理。

实现：调用者持有的有界内存 transcript；注入式 Model/Tools；逐次 poll 的 Loop；用户输入→模型文本→工具调用→工具结果→第二轮模型文本→唯一运行终态。客户端接口在 `src/contracts/client.h`，不依赖终端。源码与单独模块登记在 `governance/runtime-local-modules.json`。

参考：本地 Pi 2ec55d808d45 的 `packages/agent/src/agent-loop.ts`、`types.ts`，主要学习输入、流、工具结果回填与终结，而非逐句翻译。服务用显式函数表组合；没有把具体厂商或UI加入Loop。此版本不声称复刻完整Pi。

直接验证：O0/O2、Wall/Wextra/Werror/pedantic、UBSan不可恢复模式全部编译和运行通过。测试实际覆盖工具往返的上下文与关联、拒绝不执行、模型错误/start失败/非法事件、忙时拒绝、流中取消/启动前取消、重复取消、请求close及空/无效输入。首次测试失败是测试代码在终态后调用poll覆盖了事件，再读取outcome；修正测试先保存结果后通过，没有修改产品终态逻辑迁就测试。

G00检查器新增精确注册的 native-source/native-test/development-tool 类型，限定在本批Runtime路径及合法后缀；原有登记和默认拒绝保留。新增两项准入正/反例，`python3 -B tools/gates/test_check.py` 15项通过。旧design-only策略标签保留兼容，但其库存检查不再被表述为全部仓库只有设计文档；完整生产认证仍未授予。

边界：最多64条消息、每条最多4095字节、ID/工具名最多63字节、每run最多16次模型请求；无隐式堆分配。尚未复用早期平台/allocator候选，实现先使用标准C类型和调用者存储，后续接已有基础服务时保持行为测试。测试是hosted libc/UBSan程序，不是freestanding发布二进制。

已知限制：一个assistant轮只支持一个工具调用，CALL提交该轮；文本与调用记录当前是简化内部投影，未覆盖完整Pi内容块。输入尚无UTF-8语义校验；会话无树/持久化/清理策略。工具执行接口目前必须及时返回，仅供无副作用替身，不可直接接阻塞Shell或网络；真正异步操作和工具安全管线尚未完成。取消发生在tool_started之后、执行之前时可以直接得到run_cancelled，不另生成tool_finished，TUI以run终态清理活动状态。模型必须遵守非阻塞next约定；poll无事件时客户端负责等待，Runtime本身不调度IO。

下一项：强化容量/协议边界并接入非阻塞Provider与工具操作接口；与另一Agent的薄TUI桥接。未宣称T19/T22/T23/T24/T28整体验收、真实模型成功或完整生产放行。

## 同一 Agent 自审

依据用户授权，由实施Agent自审自批本次局部开发登记，不是独立审核。批准新增实际Runtime源码、受限登记类型与精确构建入口；不调整旧预算和基线，不批准生产发布。风险是内部契约仍会演进、现有构建未提供完整模块依赖/符号认证；固定文件摘要如下（本记录自身排除）。

```text
867032b4fa0cbbbeaf4394c4b24dbae1677346d948fbc069295de6f64045222a  src/contracts/client.h
1db6acf9d8a28080895438e3fac2cbdc926a5d0ffe766df25c083fcfe1241954  src/runtime/agent.h
8f27637ad6c8d20c0f645ec18eb245c21d412d2bde1f51766721f5cd0f87b842  src/runtime/agent.c
0cc952bf89bce92f539cbeffc52415374c6062bbd09b1d7648fd44fb3a340636  tests/runtime/test_agent.c
92405628c6761a995faa95fa85e78cd0dc23ac136b2cd14718d91bebb1568115  tools/runtime/build_test.py
ebb484502e0decc76df94ade712b83e19c44ee17ddacc9ce7caff3316558c527  governance/runtime-local-modules.json
3dd4c38788b788389b390b744eaebb4c44e142839ccfff2a3cb8a71631db4bd4  governance/bootstrap-policy.json
ba0960566e8b1661ef689c8c1d3ab7909b1b5391dfeed0654128852f0dd93b6a  tools/gates/check.py
ee9a95ae7babed20b5292da9ce9949452c09b7161da8c411222063b49d280319  tools/gates/test_check.py
```

## 工具注册与工作目录只读工具（继续开发）

本轮阅读了Pi `packages/agent/src/agent-loop.ts` 的prepareToolCall（按名查找→参数校验→beforeToolCall→执行）以及 `packages/coding-agent/src/core/tools/read.ts` 的读取/输出上限思路；同时阅读deepseek-harness `packages/core/tools/src/index.ts` 的注册表、模型展示与执行管线分离。实现为原生小型注册表，不移植整套TS钩子框架。

- 新增 `src/runtime/tools.h/.c`：最多16个工具；名称/Schema基本结构/回调/重复名检查，初始化失败保留原表。定义和上下文借用，调用者须保证生命周期且运行期间不可修改。
- 模型声明和执行器在组合入口绑定同一registry；MiMo不再包含固定echo声明。可见性与权限分开：声明不自动授权，执行仍经过JSON对象语法→工具validate回调→authorize回调→execute。这里不是通用JSON Schema解释器，工具作者必须提供与声明相符的语义校验。
- 新增 `src/runtime/read_tool.h/.c`：显式根目录fd，逐段openat/O_NOFOLLOW；拒绝绝对路径、空/dot路径段、任意层符号链接和非普通文件。最多4095字节文本，超限明确失败；拒绝NUL和非法UTF-8。只读，不实现write/edit/Shell。

实际验证：O0/O2 UBSan模式下Loop、SSE、registry与read四组程序全部通过。registry覆盖未知工具、重复定义和失败不覆盖、非法对象/参数、拒绝不执行、顺序断言及超大输出；read覆盖普通/子目录文件、路径遍历/绝对路径、文件/目录符号链接、额外参数、输出配额、二进制/非法UTF-8和关闭后的根目录。

真实MiMo再次通过registry版本echo往返。随后只声明read，以临时目录sample.c作为唯一测试文件，真实模型调用一次read并返回文件内 `PANGU_READ_OK_9137`，SSE拆成3段，HTTP200、执行次数1、会话累计标记自动验证通过、进程exit0。临时目录完成后删除；没有读取用户其他文件、执行Shell或修改TUI目录。

可复验：`python3 -B tools/runtime/build_test.py`。真实只读测试先准备临时sample.c及相应标记，环境 `PG_LIVE_READ_ROOT` 指向该目录，并通过安全环境注入 `MIMO_API_KEY` 后运行同一脚本 `--live`；默认live仍是echo，PG_LIVE_CANCEL仍是流中取消。

限制：这是T24/T25子集，有意拒绝Pi允许的绝对路径/符号链接，未支持offset/limit分页、图片、完整UTF-8/BOM/CRLF转换或权限交互。小文件读取仍是同步POSIX调用，网络文件系统可能阻塞；不可据此声称硬实时/异步IO或OS沙箱。普通Native插件是可信代码，注册表不是内存隔离。根目录、挂载和硬链接由调用者的工作区/系统权限约束，不防御恶意特权进程或任意内核挂载。json-c仍是工具适配的局部依赖。

同一Agent自审自批本轮模块、依赖和文件登记；不改变门禁检查器、预算和参考基线。最终15项G00检查器自测与diff格式检查通过；全仓G00当前失败来自并行TUI新增的尚未登记源码/测试/构建产物目录（含tools/tools/client），未清理、登记或覆盖对方文件，等待交接后统一处理。没有声称完整T24/T25/生产准入完成。

本轮固定源码/控制摘要（本记录自身排除；替代上一版本对应文件摘要）：

```text
c24f070a75c1e4db6ecee1714ef1bed8aee1d51dbac1648b0ae405043bd072b1  src/runtime/tools.c
6dc27f067c1db57c7d8758845769450a396d1f7e233c7958951912f370c06f52  src/runtime/tools.h
daae5ffaa8f88c1f5dfd774f0682e0a687f2c2c64d0dcad60b0c9a024fda991d  src/runtime/read_tool.c
6e2afb9d57867e27248e33607ec1391599ca5610dbe74f177234f71481c63142  src/runtime/read_tool.h
a84344f43d72aa200cf237a4d63fa1de3a28f62a666e4aaccc161dadcf9f850d  src/runtime/mimo.c
9749db9eb3ec26282da26dd388fe87f08283e6b99e023b5b5daaf13d51244e89  src/runtime/mimo.h
b4d748e3d3b014dd803992f548cf2dff7f9f54612ef684140e52ae2354a5e305  tests/runtime/test_tools.c
aa265a4d3c8d763705dc4dbdb6de2834137cbd50bba2be1caf2963130c5547f0  tests/runtime/test_read_tool.c
c848df23f0a08c98cd89791bfd20866fcaa4581a2f4152b6a0c0a79eba9a0a02  tests/runtime/live_mimo.c
acc089d2835504d85c9824034b3efa44244d36eec792202297a6701971b3f083  tools/runtime/build_test.py
14d8af60b7bd9ba5a9c09303ea4810c9578d5008559b4a3d3971a3757b2d6199  governance/runtime-local-modules.json
aa7c4458df9b315762454c406925db609335858b242106a74961b4065fceb06e  governance/bootstrap-policy.json
```

## 连续会话、系统上下文和模型消息投影

参考Pi `packages/agent/src/agent.ts` 的初始system消息、`packages/ai/src/api/openai-completions.ts` 的助手文本/工具块投影与空助手省略，新增：

- `pg_agent_set_system`：仅空闲时复制有界系统提示，运行时拒绝修改。
- `pg_agent_reset_session`：仅空闲时清空会话，保留system和注入服务；不会悄悄在容量满时删除历史。
- 模型投影合并同一轮助手文本与工具调用，跳过空助手；拒绝悬空工具调用、孤立工具结果和错误call_id。新增投影测试入口不需要网络或凭据。
- 错误终态text说明阶段/容量/轮次原因，不透传供应商原始响应或凭据。工具返回超大长度时转成关联错误结果，再由模型续接，避免留下悬空调用。

客户端 `src/contracts/client.h` 及TUI文件均未修改。新增内部API不宣称稳定插件ABI。内存会话仍为有界线性记录，不是完整Pi Session树；未支持JSONL、Compaction、资源发现和完整Context服务。

真实MiMo多轮验证（`python3 -B tools/runtime/build_test.py --live-session`，凭据安全环境注入）：

```text
session_run outcome=0 http=200 deltas=1 context_verified=1 messages=2
session_run outcome=0 http=200 deltas=2 context_verified=1 messages=4
session_run outcome=0 http=200 deltas=2 context_verified=1 messages=2
session_run outcome=2 http=200 deltas=1 context_verified=1 messages=4
session_run outcome=0 http=200 deltas=1 context_verified=1 messages=6
live_session_exit=0
```

五次请求分别验证ACK、次轮回忆标记、清空后NO_CONTEXT、首片段后取消、取消后READY。取消用例的context_verified=1来自未设置文本预期，不代表校验了取消文本；它实际检查cancelled、收到片段及请求句柄释放。另在当前消息投影下重新验证真实read工具往返：HTTP200、工具1次、3片段、标记正确、exit0。

最终O0/O2 UBSan下五组程序（Loop/SSE/registry/read/projection）均通过，含busy时拒绝reset/system修改、reset保留system、工具输出超限转错误结果。G00检查器15项自测通过；全仓G00实际exit1、48条未登记项，来自并行TUI及其生成目录/产物，未修改对方内容。diff格式检查通过。

同一Agent按用户授权自审批准本轮局部实现和精确文件/导出登记，不是独立审核；现有门禁/预算未调整，整阶段/生产准入未完成。旧摘要属于旧版本，当前固定摘要如下（本记录自身排除）：

```text
8370032c9529987bc66c451040cdb06b44893901c280a70af386ab2c68cd02e2  src/runtime/agent.h
27f6a07a41a948c8dc25ea24058eae0de19841e989d5925d3b85f5a303516ce5  src/runtime/agent.c
e8114401b403d5da2f982555378b5e283703df92dc386a92e8ea1e40b0ef9368  src/runtime/mimo.h
a3242bcf5fb0c970fca6233188bf03d6056fccba182009ae1ffbbf55677fd2fa  src/runtime/mimo.c
9319d2805926c7953b3fb9387e93eeafd1c9e0684b2368fd7cbc947f01c3bea8  tests/runtime/test_agent.c
946815d8826366085222f6d852147f536b9336b9bcfc6c8de04d4b55e266b147  tests/runtime/test_projection.c
f9479d81a7b06fee4d1669ec9f3a297b8b475cb5153e77aa04e745e7965df700  tests/runtime/live_session.c
b7198e6d5a92016fd5f773615f79cf5da1c24502a30e64a3bb9ebf9d31f0187f  tools/runtime/build_test.py
002065a1447a451ae1598cea62f86ea72f9889bf9d732179d9ddf1cc1efc33b5  governance/runtime-local-modules.json
db4eb45399b70336dcb8f4a15cc8ba9ba07d2e88d542557adc02f99ad0671e18  governance/bootstrap-policy.json
```

## 2026-10-25：真实Runtime接入薄TUI

已交付首个可交互组合 `src/composition/pangu.c`，显式选择C Loop、MiMo、工具注册表、工作区只读read和TUI。运行时客户端桥接只依赖pg_client契约，不读取或复制Session/Agent私有状态。生产组合构建清单不包含fake_runtime或测试main；原假源保留用于客户端单测和演示。

参考Pi `packages/tui/src/terminal.ts` 的终端状态保存/恢复及start/stop边界，结合现有Loop取消语义，修复客户端唤醒管道EAGAIN空转，增加旧信号处理器保存/恢复、静态信号标志重置和退出时shutdown。桥接在退出时请求取消并有限推进，当前本地Loop在至多三次poll内确认取消；该桥接不承诺适配任意异步远程Runtime的关闭协议。

实际构建：`python3 -B tools/runtime/build_pangu.py --output /tmp/pangu-native/pangu`。构建输出强制在checkout外，使用当前登记的clang-23/libcurl/json-c；组合是hosted C程序，不是freestanding发布物。入口仅支持workspace参数与help，不扩成全功能CLI；凭据只从环境读取，缺失凭据或非交互终端明确拒绝。

PTY验证脚本 `tools/runtime/test_pangu_pty.py`：

- 无网络路径：缺失密钥返回2；真实终端中Ctrl-D、SIGTERM、SIGWINCH后Ctrl-D退出均为0，逐项比较完整tcgetattr结果与启动前完全一致。SIGWINCH用例同时覆盖原唤醒管道死循环。
- `--live`：真实MiMo经实际TUI发出read，读取临时sample.c，回填并续答；比较工具完成行之后的模型输出，确认包含 `PANGU_PTY_READ_5208`，不以工具预览代替模型续答证明。
- `--live`：第二轮要求模型输出STREAM_BEGIN；测试先丢掉输入回显及run_started之前数据，真正收到模型标记后才发送Ctrl-C，看到cancelled，再Ctrl-D退出；终端属性完整恢复，测试exit0。没有将输入回显当作模型输出，也没有使用假模型。

核心O0/O2 UBSan六组程序（Loop、SSE、registry、read、projection、bridge）通过；原TUI五组单测通过；G00及16项检查器自测通过。桥接测试覆盖忙拒绝、事件枚举映射、唯一成功终态、活跃请求shutdown取消和重复shutdown。

本轮客户端输入上限仍为1023字节，Runtime消息4095字节；主动采用更小编辑上限，不隐式截断已提交消息。没有实现会话持久化/清空UI、多行编辑、完整Markdown、write/edit/bash，完整T28/P1与生产准入保持未完成。终端恢复不涵盖SIGKILL。下一步按只读体验反馈完善核心工具/会话能力。

同一Agent自审自批局部组合模块、桥接及构建登记；门禁只为精确登记新增composition源码路径，没有调整旧预算或参考基线，不是独立审核。以下摘要固定本轮候选（记录自身排除）：

```text
4961e973315b78395cc2995da58a4ee81ca1c45d131db55e4ab3de0d2e13336b  AGENTS.md
b001fbd98bdc4284c01ad1b43101b5b0d5068c494ceed754f9294a87fa72fa33  src/composition/pangu.c
d62cd8dc42fa7699d03f4d835427b4195e8c2de2319c8f1b99fdcda7ee94719a  src/client/tui/runtime_bridge.h
804ec6dca0f42d966fba4df2293bab7026f9e7d6ae38e2116ad9ec0bfb392f19  src/client/tui/runtime_bridge.c
91d70fc34939f637015ad3f0867e8f372d801823eff62e724993f303d743aca7  src/client/tui/tui_app.c
312e757a569e7f2df76a47f168f3a5fbbece63a194d85a9fc31c7538874ea34f  tests/client/tui/test_runtime_bridge.c
fddb5890c4bfed25da63ddecd9199edd0d2cf4435dab2308381b4b3490d2f8b8  tools/runtime/build_pangu.py
e25b2f3f32e5f98271e436286db7f09b979e99adc1138a5a616e515ee62cdaf6  tools/runtime/test_pangu_pty.py
f012dd95484e607e2e8cd3d1cca3bfbd50264f117d1e5ceb566d96104e3bbb88  tools/runtime/build_test.py
e130052ee891429643938800163b86cad5a981e644c22cbc2a78f1f69c2bf184  tools/gates/check.py
5f939c88b0a261cb97545b706a383392a7b7e25e123576ea8d3f3388437d4a52  tools/gates/test_check.py
d29c46d4f53c759bdc67194c184bdf9d18b2ee889a84841d3b49ed961e30dbf1  governance/bootstrap-policy.json
8b56ad89109af207133fd8269e1b5144865fb724e87b6a068f8f2203df12084a  governance/runtime-local-modules.json
904b626dd97f82439137b134178574bfc179e95e1a42d411d2975a722047739c  docs/migration/task-ledger.json
```

## 2026-10-25：汇编优先与单版本原地重构决定

依据用户明确纠正：Assembly第一、C第二、C++第三；不保留被替代C版本或源码对照。旧TUI派发文档已删除，旧登记/有效链接已移除；新任务为 `docs/并行开发任务提示词-汇编优先TUI重构.md`，第二Agent负责TUI UTF-8/输入/状态/渲染/事件流程的逐模块.S替换，主Agent负责Runtime、公共契约、桥接/组合和共享控制。

AGENTS、主架构、门禁、总TODO和账本同步，删除“C承担主要流程”“最后按热点补汇编”的有效指令。现存9个C核心文件登记为待原地替换；内容摘要用于阻止继续扩写，没有另存源码。迁移完成在同批删除C与旧构建项，加入手写.S并更新登记，不允许C双后端/兼容层或挪到tests/legacy。回溯只用Git历史。必要C平台/外部库/静态组合胶水逐文件说明；无新增C++。

新增局部语言检查及主构建输入核对：实际SOURCES必须等于语言登记，待替换C不能变更内容，C/C++桥接必须有合法类别/原因；G00调用该检查。检查不证明汇编算法、手写来源或性能，不能把当前9项待迁移算成汇编重构完成。G00精确支持登记后的.S，未放开未知文件或旧预算。

实际验证：G00通过；检查器16项自测通过；语言检查5项正/反例通过；实际交互程序构建通过；无网络PTY的Ctrl-D/SIGTERM/SIGWINCH后退出及完整termios恢复通过；diff格式检查通过。此轮没有改运行核心算法，没有声称汇编实现已完成，没有为了文档变更新增真实模型调用。

同一Agent按用户授权自审自批本轮语言与文件/构建控制差异，不是独立审核。风险：清单中的9个现存C核心仍待原地替换，类型登记不能自动识别伪装的C算法；新任务要求实际实现/ABI/边界/链接审查。固定摘要如下，记录自身排除：

```text
c00b6fac5eb4897ac6c8a164bc045ee6c7e48167c708b8b60182154b8921a930  AGENTS.md
c59007ad30ee56f6e82e7f494438557480c18a4b4efd5fd8968ea5337d5667c4  docs/并行开发任务提示词-汇编优先TUI重构.md
e96d5510a431c29973d98866e20284c552675e6e44183c7dd97d537259c2a008  governance/assembly-first.json
2e66c1cd2c6ffb585257c38ad49decd0a0c328bc277c97cddfdd3ecd08174964  governance/bootstrap-policy.json
2f7895a37bd5ffaf3154f515ebc5c1bc664610441d07f2615d8fd54e982465cc  tools/gates/assembly_first.py
5f86fef5ed5ac5f6dec6bf4c7b207cb4ce04d071c1048a06718e9da5454aaad9  tools/gates/test_assembly_first.py
6338c87767908c194a26bab5f3c5952a88105f19d6572f1aaede65da5165a88b  tools/gates/check.py
0344b703980910d14762f0a470460d7d7349c78bfa68d0d22636c4269bf35e03  tools/gates/test_check.py
ddf24ec1efe5be1d6e14b4f4c95c432994570134d06da53a4dfdc3064d572389  tools/runtime/build_pangu.py
```

## 2026-10-25：后续Agent默认行为的入口整改

检查范围为根AGENTS、主架构、门禁/主路线/TUI体验与历史交接入口、当前模块/语言策略、主构建与核心测试脚本；对迁移文档作关键词复核，区分历史事实与现行指令。没有修改另一Agent独占的TUI实现/测试/本地构建脚本，也未改正在执行的新派发文档。

发现并整改：

1. 主文档原则/协议、门禁语言和TUI体验仍有“Assembly/C”“核心和热点”等模糊入口，现统一为手写Assembly核心、必要C胶水、批准C++桥接；可选外部插件语言能力不是默认核心用C的许可。
2. 主路线末尾仍把design-only/独立审批及尚无原生产物当现状。现行未决项改为完整验收范围，2026-10-20旧阶段说明明确历史属性，不覆盖当前自审/核心开发。前期交接标题标明不再派发；不改写历史测试结果或批准范围。
3. 主构建写死C文件列表、核心测试直接选C实现。现以已登记语言清单作为唯一产品输入，用无后缀模块定位唯一实现；缺少或重复实现拒绝，不按.c优先或自动fallback。汇编替换只需同批更新有效清单和删除旧源，脚本不会继续强选C。TUI本地build.sh归另一Agent此次迁移负责，未越权编辑。
4. MiMo整个文件被列作external-library胶水，但含自身SSE/消息投影/状态算法。改列待原地汇编替换，迁移后只允许必要外部接口封装。待迁移数量从9变10，是分类纠正，不是新增一套实现。
5. “参考Pi”没有足够明确的开工动作。AGENTS/主架构/门禁/路线现要求先读固定对应函数、直接调用、相关测试，再记录采用语义、原生差异和验证。模块表补具体提交/源码/测试入口。来源登记不等于已阅读，更不等于行为兼容；无法用机器可靠证明AI真的理解源码，不新建形式化阅读审批平台，依靠具体行为证据复核。

实际验证：G00通过、16项库存检查器自测通过、6项语言检查/实现选择测试通过；核心六组O0/O2 UBSan回归通过；主程序构建通过；无网络PTY的Ctrl-D/SIGTERM/窗口变化后退出及termios完全恢复通过。另以临时手写GAS函数+C ABI测试驱动，在当前编译参数下编译链接执行成功，确认脚本参数不排斥.S；不是产品汇编迁移完成。本轮未进行真实模型调用，未改TUI源或参考仓库。

同一Agent按授权自审本轮入口、语言分类、模块来源与脚本差异，不是独立审核。保留C/Cpp合理胶水和开发测试驱动，不采用行数配额。语言政策仍未证明汇编手写来源/内部委托行为，需看实际实现及链接。旧摘要属于旧版本，本轮候选摘要如下（记录自身排除）：

```text
9b1df11c48288475f81298af80ffa39501f80fe0998a6142403e3bcd5de2f1f1  AGENTS.md
4cf2cd69130ba8a7b063f83dfa0ec8a1331fe798a2acd76aad95443d4253df40  盘古 Pangu 项目完整文档.md
72420c91ef27434146e3bfef38eeafb7f41161630a75071ca7635dc21416df9e  docs/工程门禁与架构守卫.md
c3f1c83655838c7fee191171d9e5dd778d7da6c2a2e67993c0976d6d6fc85f71  docs/原生重写路线与迁移TODO.md
9fa164b0cbbafd3ec557e27cbb82fb7066fdb55580f7cfadfada374c6e0cdeb8  docs/TUI体验参考与设计原则.md
ed5e9fb67dbaf1e6402cfceedc777258eb0cf15add194e200c614342db8ff32e  docs/前期工作交接TODO.md
17bceaac21ab85a2f512fa99adc1272f1af25380fdad94b3efc0776290999e95  governance/assembly-first.json
8b1a6cd7b7d8f25ba5c934efe7d5aaf98d6429dd90e3a017c1077dc0576a4b34  governance/runtime-local-modules.json
4794c8cd942313ca4bb07d67a3cdb8155eee2d6c33bcad6a7a52da3613a54257  docs/migration/task-ledger.json
d207e2246149e6e114239735ce0ed079e1e04ed0304097b5fcc1e418365161c6  tools/runtime/build_pangu.py
752d812f9331919ced83ed0f87035761cd2a7f067b047e227cc8b15c9d647753  tools/runtime/build_test.py
2bda7276b99d3de15aa89d47d6c8576c86cf3f7c9e8856f19d518ba47686d7f9  tools/gates/test_assembly_first.py
```

并行状态补充：上述G00通过是本轮检查时的结果。提交前另一Agent新增 `src/client/tui/tui_layout.h`，最终复核G00实际exit1：该头文件未登记。只提交本Agent负责的13个入口/控制/脚本文件，没有暂存、修改或登记TUI进行中产物；等待其交接。此状态不应被报告为最终全仓G00通过。

## 2026-10-25：Agent Loop和内存会话原地汇编重写

`src/runtime/agent.S`为手写GAS实现，删除`src/runtime/agent.c`及其有效构建/模块登记；没有另留C版本。公开C ABI未改，布局/枚举常量由编译期sizeof/offsetof断言验证。仅必要的append提交时序注释；没有编译器生成实现或调用旧C核心的壳。

开工参考：固定Pi `2ec55d808d455b1a64a2bf7888198207a096120a` 的 `packages/agent/src/agent-loop.ts` 内层流→工具结果→续接及error/aborted终结，`packages/agent/test/agent-loop.test.ts` 的tool calls/results用例。采用其工具结果加入历史再请求模型的基本语义；保留当前有界、单工具/轮与简化事件契约，不声称移植Pi全部队列/钩子/并发功能。

实现涵盖初始化、客户端动作、有限poll、模型/工具调用、文本容量、取消/错误终结、系统提示和会话清空。消息内容写完再提交count。模型/工具由函数表注入；ASM对象没有未解析的外部符号，四个公开导出保持原名。`size`观察到对象text合计2690字节、data/bss为0（含只读数据的对象统计，不是整个Agent部署体积或性能基线）。

实际验证：

- 核心七组程序O0/O2 UBSan构建执行全部通过：原Loop/SSE/registry/read/projection/bridge，以及新增ASM专项。UBSan不证明汇编访存安全，另做下列直接测试。
- 新ABI测试检查RBX/RBP/R12–R15、DF与栈哨兵；回调主动破坏caller-saved寄存器，入口验证栈对齐；execute覆盖经栈传递的容量和结果长度地址。故意破坏RBX/DF的正对照返回预期mask65。
- Agent存储分别贴左右保护页；验证初始化、完整工具往返、精确事件顺序、4095字节消息/系统提示、重叠系统提示、reset及取消，输出结构前后哨兵不变。
- 真实MiMo使用当前ASM Loop读取临时sample.c：HTTP200、工具执行1次、累计模型输出标记正确、exit0。连续会话五步（记忆、回忆、清空、流中取消、取消后再输入）均符合预期，live_session_exit0。

库存仅新增测试`.S`类型支持，维持精确路径/后缀和原预算；16项库存自测通过，语言准入测试沿用。当前工作区G00因另一Agent正在删除/新增的TUI源码未交接登记而失败；本次不修改或暂存其文件。提交树验证另见后续实际结果，不把当前工作区失败说成全仓通过。

语言清单把本模块改为assembly-core，余下待迁移C逐项保留未完成状态。T19/T23仍IN_PROGRESS，单工具子集不是完整Pi/阶段认证。下一项继续工具注册/参数流程等核心原地汇编迁移。

同一Agent按用户授权自审本次实现、模块/文件登记及测试类型扩展，不是独立审核；没有调整参考基线、行为断言或预算。固定内容摘要如下（记录自身排除）：

```text
6bbcf85b74e94d5e72716d230de2d2a5daa303b9b7ea4660552b40d23318e8cd  src/runtime/agent.S
0c9a2d92228b80e134ac58c01d237b37238fc649b14fe81e83d7fcf18d10f982  src/runtime/agent_layout.h
95c04acea441618e90a36fa0800cc8d7bf6fb724a04411771dd07fad0558f1ce  tests/runtime/agent_abi.S
ff1a0796674d8c585edfeeb38bbf580098e739c6d004247c60dea43e49e812fb  tests/runtime/test_agent_asm.c
8f50c5c1c11760132785790c7c99416aac40f797d9a1d90580842d53272d430f  tools/gates/check.py
90e809c72d78d76cd48a77ed7eefe5151a49449941c52695dcd4db7a746b013c  tools/gates/test_check.py
a169e502fbf32659b2ae6b5a79bb842b2d70c1c52783769b1c4fe606a2682979  tools/runtime/build_test.py
59cbff77343e9b70804cfbd7a3aa0e4b7ef1a52bafb5e31e5ee4e8179c2f11de  governance/assembly-first.json
e6b8178c5e566194ffac802a7b8bba28e5e50e341d2f390f8865bddba078636c  governance/bootstrap-policy.json
326f4ae7ec8c0b08cbce7ab56c3fca36ee7330573c7dc392dd85807e329893c7  governance/runtime-local-modules.json
9b6fd75bd4c449dc77d7fc028fae34d31c125decefc8d37d2fa0c525ee0cfae3  docs/migration/task-ledger.json
37684c92702035ed34672d889ce99ccb4c924ba42f4d3ce4900d4a1b155e0b4e  docs/原生重写路线与迁移TODO.md
```

提交候选复验：仅用本Agent暂存内容检验，未包含另一Agent未交接的TUI改动；该提交树G00、核心七组O0/O2测试、无网络PTY及真实MiMo PTY（read续答、收到文本后取消、终端恢复）全部通过。检验目录完成后自动删除。当前共享工作区的TUI改动仍由另一Agent负责，不据此声称其汇编版本已验收。

## 2026-10-25：Durable主基座与微内核边界修订

用户明确选择Durable主基座，旧Coding Agent辅助，Harness/Cordis负责应用层微内核与组合。当前仅完成文档/队列修订，尚未完成核心语义改造；agent.S可变内存会话不能称为Durable兼容。取消等待与明确abort、事务失败不发布、检查点恢复及工具副作用处理进入近期队列，现有TUI cancel保持明确abort。

Pi参考从2ec55d808d455b1a64a2bf7888198207a096120a干净快进到38ff99177fba21e9c57a670f4e95adcf407c99c7；Harness从687f282e8625c6b8d11ff58211a90524be2e4317更新至origin/master（精确提交见下），首次误用origin/main未成功，随后改用实际master。更新不改变历史基线或宣称接受全部修复。读取Durable README概念/恢复、Submissions.submit→commitWith、Conversation.reset/commit/abort及恢复测试入口；任务恢复测试目前只完成入口定位，尚未运行TS套件，不能称兼容验证。读取Harness docs/architecture.md的Cordis、Core packages、Turn flow与Session log：领域功能由插件提供。

同一Agent自审本次文档控制变更：不放宽预算/测试；风险是原核心仍缺Durable语义，明确保持未完成。git diff --check通过；G00实际失败于另一Agent正在迁移的TUI新增汇编未登记/旧C已删除，本批不代其验收，也不修改门禁隐藏失败。固定摘要（不含本记录）：

```text
Harness 22ddee3792402c3e70ec5f86a63c3e3e895a2f60
7f7d6d966016ec2a04cae0940ac8c83058cc4b2d1acdd6a04b6a366de238e48d  AGENTS.md
ab3c281079000af3707f7bebf31d796289fac9be2e6b4e11f3c998bfc29fcf2c  盘古 Pangu 项目完整文档.md
bf46950c6739e0106532fb9698c60180f4bf7ae3cc7bee6e5e28d9ddce962627  docs/工程门禁与架构守卫.md
1367841178e8c7a9ba0bf9d53032b1ddd0d5648fae94de8f68dbbd0be34a3c00  docs/原生重写路线与迁移TODO.md
```

## 2026-10-25：Durable边界首批——草稿与结束消息分离

阅读固定Pi 38ff99177fba21e9c57a670f4e95adcf407c99c7：generation.ts的streamResponse→PartialChanges与classify→answer/startToolRound、convertPartial；session.ts的Storage.commit→adopt→publish；submissions.ts的wait/abort；harness-partial-changes.test.ts的分片/提交失败恢复用例。采用live partial不直接改Entry的边界。最新CHANGELOG还明确进度可deferred（默认persistWithinMs=1000），故修正“任何展示均先落盘”的过强陈述；持久事实和临时预览分开。

agent.S原地新增4248字节有界draft，不在request start插入空assistant；文本只修改draft，成功DONE或有效CALL才追加结束消息。CALL先验证标识/参数/整个往返容量，再追加assistant与call；没有中途回调。结束/取消/错误清除draft。既有消息偏移和pg_client接口不变，pg_agent尺寸从280440变为284688，消费者须同批重编译；不承诺稳定ABI。没有第二份核心实现。

有意未完成差异：临时文本事件仍为进程内预览；取消/错误草稿丢弃，不实现Durable的已提交partial→aborted entry。仍无Storage事务、requestId、任务检查点、重开恢复或等待句柄；取消仍明确abort，不把poll缺席称为自动后台调度。此批只建立后续提交服务的生成边界，不声称Durable兼容。

验证：核心七组O0/O2 UBSan构建执行通过，新增预览不入模型历史、错误/取消/无效参数无半消息、已结束消息不随下一轮变化、draft清除；汇编ABI/寄存器/栈/保护页及满容量原用例通过。当前共享工作区G00因并行TUI迁移未登记而失败，不修改其文件或放宽检查。真实模型和隔离候选结果另追加实际回执。

最终回执：真实MiMo五步会话HTTP200，记忆/回忆/清空/文本后取消/重新提交通过，live_session_exit=0。隔离候选G00通过（生产准入仍BLOCKED），无网络PTY Ctrl-D、SIGTERM、resize退出及精确termios恢复通过，不含并行TUI汇编改动。

同一Agent自审自批：参考选择/模块来源与核心边界调整，不修改检查器或预算；风险是pg_agent尺寸变化、异常partial暂时丢弃和未实现持久化。首次追加摘要脚本因Git中文路径转义失败，修正为NUL分隔后重记，不影响上述测试。固定摘要（排除本记录）：

```text
7f7d6d966016ec2a04cae0940ac8c83058cc4b2d1acdd6a04b6a366de238e48d  AGENTS.md
d2fa9edb77376c21928438115629264e69bbefb9593cec074e33eaec9176645b  docs/原生重写路线与迁移TODO.md
7cfc09b878049381459b280db488c3cb40fb7aea9ce9317cdcdb1d25c3b60f1e  docs/工程门禁与架构守卫.md
563a0b84a3e1c21d3ac8b5dc6564e41f00b2d4772dcd842c78ac4a075cb7cb2b  governance/runtime-local-modules.json
88367fd6f957e156e678df4c67d0f945d9319ba090210ea69c35fc54cbd5e992  src/runtime/agent.S
94bccda5cf4638f7474b2d1bd0747959ba0e412e29f701d78021252d0abe7b65  src/runtime/agent.h
d43c3a088b3380b975a93034b7fe5ef2a092ff7899b9626a3d6ec5f9d2d76d7a  src/runtime/agent_layout.h
849fb554253347c4837708cdf51c01efca5b8b2cf5071b37fc82e0a69975f29c  tests/runtime/test_agent.c
9579829fb10c5a544a75c6c41941b7669db9d9e9f86630fa2460d2c3c9596b2d  tests/runtime/test_agent_asm.c
f1bdf9dde75aa770ab23d9e0928598adb5081704e4868ba30799d56be90d833f  盘古 Pangu 项目完整文档.md
```

## 2026-10-25：Durable提交边界——Storage确认后采纳

参考Pi 38ff99177fba21e9c57a670f4e95adcf407c99c7：读取MemoryStorage.prepareCommit→apply→adopt，session.ts事务settleSuccess→Storage.commit→adopt→publish，以及storage-conformance.ts的adoption和跨Entry/Task/Submission失败批次用例。采用先校验/分离、整批确认、再采纳/发布；本批同步原生接口用返回确认代替上游异步adopt回调，单所有者内确认与采纳之间没有await/回调或另一提交。

新增session.S/session.h/session_layout.h：最多两条有界消息/一个system变更/reset，分离输入批次，校验容量/角色/文本终止符/工具标识、序号溢出；Storage返回OK且序号恰为expected+1才采纳。REJECTED表示无写入，视图与序号不变；UNCERTAIN、异常返回或错误确认序号将控制状态置failed，拒绝后续写入；busy拒绝重入。默认显式绑定pg_session_volatile_commit，Session采纳是其唯一内存状态，无独立持久副本。外部Storage必须复制/序列化借用批次，不能保留指针或抛异常，也不能把不确定结果当REJECTED。

agent.S所有输入、system/reset、成功响应、工具结果改走此服务。assistant与call是单个两条消息批次，确认前不发tool_started、不执行工具；结果确认前不发tool_finished或续模型。输入拒绝不发run_started且可重新提交；运行中存储拒绝/不确定则终结为ERROR并停止后续写入（不盲目重试已执行工具）。取消产生的工具结果也必须提交，失败不能伪报取消成功。工具仍限已有只读/无副作用子集，未实现外部副作用exactly-once。

无新增C核心、无旧实现副本，无生产外部依赖；Session默认实现直接由Loop调用，尚未完成完整Session/Task服务替换和通用插件宿主。pg_agent由284688增至288968字节；事务分离区固定8552字节栈空间（至多两条记录），消费者须重编译，非稳定ABI。新增接口/实现/测试均登记到现有清单，T19/T23仍IN_PROGRESS，T20未完成。

本轮新增测试覆盖Storage替换、原视图在回调中仍旧值、重入拒绝、源数据独立、第二条无效时整批拒绝、system/reset失败不改变状态、容量与序号溢出、不确定结果/错误确认序号隔离；运行时分别注入输入、call批次、工具结果、最终响应、取消结果存储失败，检查事件次序/工具是否执行/失败后禁止输入。新增Storage回调汇编破坏caller-saved寄存器与栈对齐断言，并用ABI探针检查pg_session_commit；沿用Agent两侧保护页与满文本容量。首次八组O0/O2 UBSan执行通过。真实MiMo read已实际HTTP200：工具执行1次，最终marker_verified=1。最终候选和会话回执随后按实际结果追加。

最终候选回执：仅归档本Agent暂存树（不含并行TUI汇编工作）。G00通过，生产准入仍BLOCKED；无网络PTY Ctrl-D/SIGTERM/resize退出及精确termios恢复通过；真实MiMo五步会话HTTP200、记忆/回忆/reset/文本后取消/再次提交通过，live_session_exit=0；真实PTY read续答与流中取消/终端恢复通过。八组核心O0/O2 UBSan测试已在当前相同核心内容完成。共享工作区G00实际仍因另一Agent的TUI源码替换未交接登记而失败，不声称该工作树完整验收。

实际汇编对象检查：session.o text=718、data/bss=0，无未定义导入；agent.o text=3077、data/bss=0，仅导入pg_session_commit/pg_session_volatile_commit。没有跳回C事务核心。测试目录与对象均自动清理。

同一Agent自审自批本批控制面：新增模块/文件登记、语言清单、实际构建/测试链接依赖和T19进展，不变更检查器/预算/生产准入。风险：pg_agent尺寸及内部契约变化；Storage同步且需迅速返回，不能冒充任意异步后端；默认内存仍无落盘/重启恢复；运行中失败须停止且不能重放工具；外部可信Native实现须遵守全批次确认及借用生命周期。未完成的文件Storage、Task/Submission、requestId及恢复继续保持未勾选。固定摘要（不含本记录）：

```text
cc4e76fef156e859366153d3c4433509ffb168658d36312c58d8c14519fb0b27  docs/migration/task-ledger.json
b951540e7b4dcee562b788ba283cb182a0fa32055b61067c4d7f96b59c078638  docs/原生重写路线与迁移TODO.md
b3c1d252289bb1f0358d93ba3fa8dc59027aa1b32886017d7faa8e0b4485a242  governance/assembly-first.json
d30c98ea660931988d88197854deb24d33ef7318d2ebfbf1d0078eae6b9d9617  governance/bootstrap-policy.json
472aca50fef2e269bace2ed9241cb49b947f6b1e8977b7d39ca46187aa4df78b  governance/runtime-local-modules.json
96c605332257b6f98b128101daa8a915db1dbc8c9f7c53efc9ab949cc583f778  src/runtime/agent.S
df23bd3805addee8641a68160b50dc3060d88fe8d23165eee2e0b68121f1e0ad  src/runtime/agent.h
e1c3cbabfbfe547cad0f47c9d1c786f9def8d1a7489ac51f08723926ee06d1ff  src/runtime/agent_layout.h
547a3e3053e231802a4e65476c8260645c77e6cd77533c4d95da96e0506e0840  src/runtime/session.S
05f85fbcbd71089860177c2a0a458bd544fa391615f2e2e18f75f368abdb2c5a  src/runtime/session.h
5f9230d919c35c700f567c4a986f1f3cb211d49ca59bf50b5ebd44435747d93c  src/runtime/session_layout.h
f97cc402cdc0ec43170c3a2e24b9a3a89d3436edd82215c98fc0272215c775f9  tests/runtime/agent_abi.S
0e3c5f8b177ef643031667bd1d5e723503d06ec350b2c877d3440e74bdc76923  tests/runtime/live_mimo.c
d871cb6bab3f97436924e8b03a6fd6b3b3c404bc7b0b004498b76fa41f339005  tests/runtime/live_session.c
f813df95443641103b5661cd7f5eb194e13f1f576446b96697575b5d7eeb5259  tests/runtime/test_agent_asm.c
7323727ea799917f6e2d8a0e929dd5d52464e77e4fa5fa7dd81b0c1cd77d90ab  tests/runtime/test_session_commit.c
7ec3f4d1dd2a568cc81f904c20adf4a4780543a6afccf027642bb4271dfcf7f2  tools/runtime/build_test.py
d1bde6a535f8ead4563d9f2f3b41b56b12e366ec37eb305a5441c6e7d58df825  盘古 Pangu 项目完整文档.md
```

## 2026-10-25：第二Agent TUI汇编交接已集成

主Agent复核交接并切换共享文件/模块/语言登记；独立构建对象直接输出仓库外。六组TUI、八组核心O0/O2 UBSan、全工作区G00、真实MiMo PTY工具续答/流中取消与终端恢复全部通过；原TUI五个C实现删除、六个汇编实现生效，runtime_bridge仍待迁移。生产认证未完成。控制面同一主Agent按用户授权自审自批，不伪称独立审核、不放宽门禁；平台与UI已知限制保留在交接。固定摘要（不含本记录，删除项由Git提交记录）：

```text
4a78ad1fdb8a38ec9a27911a47771e1dde5886672bcd0715afbbf498a889bfdb  docs/migration/task-ledger.json
a9be4b547e759d29873f950bea28d740e0406764b5bb2a7ad42785de06ea4041  docs/migration/tui-assembly-handoff.md
2755adbf0263b64962fc4020321c360cdc5327277423e822a865a51db773b961  docs/原生重写路线与迁移TODO.md
7537d52dec64e30ee9ccf9813c897e1077930ab310a2c344b6fa3ee62404159c  governance/assembly-first.json
f86c518f6362267a53bcb33f2dcde2fbd7867016fc23b0f56cad9ccb7e3078f0  governance/bootstrap-policy.json
c1767d51f278051fd3d3d63ad45b4884e6a25a05d17feb10fdc646b7abb1b45f  governance/runtime-local-modules.json
10a63f11947aad7c0651f9c42d359aa2bad6da5371a662151c434a31931a1613  src/client/tui/tui_app.S
41888972338dfc53d12cb3dc465122675a5ba7e0a3593d61e7bed2b67e413342  src/client/tui/tui_input.S
a7785b934f45d1a7157d99f6c7d27d54aadfe98d4836a43e230addc8571dfff5  src/client/tui/tui_input.h
b346003c0788da51d2c8155dfbef0db8f362b413835693910d9aeea004a0f7bc  src/client/tui/tui_layout.h
30751f31a6f763c06ff950f3356687eec4849d6482e56ae45cc4c2bf0911f926  src/client/tui/tui_state.S
27919664d291ddd4870a5e26aa1496805e93855118e7b9a8f97125a00572b990  src/client/tui/tui_state.h
537d94409a96c3f98f7b29bc1471bcc5a481de256040806884f730faccdde03c  src/client/tui/tui_term.S
24f5cc9cc738b779137f1edd901da985bd68693f93702fa4e33bbaf27760587a  src/client/tui/tui_term.c
6a4a1e5c492726d0ed6947067acce63ac8bf90b1b56980ef7f392f9d9f9ef439  src/client/tui/tui_utf8.S
a9412685663bf08f761f9d14fe39a51577e1efc050fe25b74b1e49ff3a0a2427  src/client/tui/tui_view.S
5a2884b006464088b6b977570e9302fa6fb7d871750896c9330fbb987edb97a9  tests/client/tui/test_tui_abi.c
fd79244b76e0f5885ca85ce8d09bfa3de8d2ca9286aa1feb29845e095207cf80  tests/client/tui/test_tui_view.c
0eafc3b166d1a9cb124a2864b707e4af7ff441010c096f8e5ca9a26d9c35b94d  tools/client/tui/build.sh
```

## 2026-10-25：更新并行派发范围

已完成TUI派发文档删除，历史交接保留并标注派发已退役；新任务为工具注册/read的手写汇编原地迁移。主Agent独占Durable存储/任务/恢复与共享清单，第二Agent独占tools/read与专项测试，冻结工具公开布局，不添加写入/命令/异步能力。任务明确源码参考、文件归属、单版本替换、平台胶水限制、安全边界、ABI/容量验证和真实交接要求。未执行新任务，不将文档视为实现。

同一Agent按授权自审自批登记与任务归属调整，无门禁/预算放宽。G00通过，git diff --check通过；仅文档/清单修改，未重复运行产品测试。固定摘要（不含本记录）：

```text
0c3fbbcb00a6f5e804550c763bbbd24d7dd800c665234c1865b21161c2d7361e  docs/并行开发任务提示词-工具核心汇编重构.md
9ed691ebf24471927469468c8be60ce1737913d44b230faa1fc4f468d3c164a8  governance/bootstrap-policy.json
79df87a19a349576bf7cc9e875cb476fb4eae0cad79d108ddb1e59f9353c79ab  docs/migration/task-ledger.json
d3b0a3a79c4ddc9456ec392330a34ff147e22f60a18a4f2c0849877ffbc1e5af  docs/原生重写路线与迁移TODO.md
449acddee056db19a8687a7f1a6e2bee3cb50844d266323bf518d31dea9f6fe9  docs/migration/tui-assembly-handoff.md
```

## 2026-10-25：Goal持续推进——原生文件Storage与事务级重开

主Agent独占Durable存储/恢复，未修改第二Agent正在迁移的tools/read。参考Pi 38ff99177fba21e9c57a670f4e95adcf407c99c7的JsonlStorage.commit/recover/readLines，以及jsonl-storage.test.ts的准备失败无IO、append前/后/短写poison、完整marker恢复与坏尾拒绝用例；采用全批次确认与故障后必须重开，有意改为原生显式帧而非JSONL。格式文档记录确切字节、版本、校验、配额和恢复策略；不复制C结构/padding/指针，不增加JSON库依赖。

journal.S手写编解码、FNV头/全帧校验、文件锁/逐偏移IO/短写与EINTR推进/fsync确认/重开回放/不完整尾截断；journal_posix.c仅公开struct stat的类型/大小适配。目录fd复制、最终路径NOFOLLOW、普通文件、0600创建与初次文件/目录同步；独占协作锁，不宣称OS沙箱。任何IO后的不确定结果停止该实例；完整已写但未确认的帧可在重开时恢复，故下一步必须补Submission/requestId与任务检查点，不能重复执行副作用。

默认交互组合暂未启用文件Storage，也未声称在途任务恢复；该服务已作为真实产品源码/依赖/构建输入登记，并在测试组合由Session替换绑定。新鲜Session成功打开后绑定已恢复序号，失败不发布部分回放且control未变。16MiB日志/8544字节帧/最多两条消息批次是明确局部上限，不新增压缩/数据库/侧文件框架。事务恢复的模型/工具状态与身份参数尚不在日志中，不能把它当完整Durable。

验证：九组核心O0/O2 UBSan执行通过，日志测试覆盖全部最终帧切分、完整文件每字节损坏、重算校验后非法字段、8530字节最大帧、跨进程重开/独占锁、短读短写/EINTR、pwrite/fsync/pread/ftruncate失败、半写后进程_exit及路径/类型/配额；IO回调检查栈对齐并破坏caller-saved，五参数ABI探针检查callee-saved/DF/栈哨兵，故障探针负例检验其可检测性。汇编对象text=2444/data=bss=0；导入仅Session接口、公开libc IO与单个stat胶水，不回跳C日志核心。

真实MiMo跨进程落盘测试实际通过：store新进程HTTP200/verified=1/sequence=3/messages=2，recall另一个新进程HTTP200/verified=1/sequence=5/messages=4。凭据仅进进程环境，日志和报告不含密钥。测试目录在结束后删除。完整候选门禁/PTY和自审摘要另追加最终回执；工作树G00当前因第二Agent未交接的新tools/read汇编文件而失败，不为此提前登记或声称其实现通过。

最终候选回执：只归档本Agent暂存树，G00通过（生产准入仍BLOCKED），PTY Ctrl-D/SIGTERM/resize与精确termios恢复通过。未包含第二Agent未交接的tools/read新增汇编，未声称共享工作树G00已通过。Goal保持进行中，接下来实现任务检查点与在途恢复，文件回放不是完整目标完成。

控制面由同一主Agent按用户授权自审自批：新增真实Storage实现/头/平台胶水/测试/格式文档，更新精确构建输入与T20局部状态；不修改检查器，不放宽现有预算或断言。新增16MiB本地日志边界独立于未认证的生产性能预算。风险包括同步IO延迟、FNV仅检错、协作锁、未知版本拒绝、未实现任务/幂等/副作用恢复；已记录且交互组合暂不启用日志。固定摘要（不含本记录）：

```text
da08599330e78506a6383d34feef328eb625b2769e8235c3ff2f8231ad806469  docs/migration/native-journal-format.md
dc49afbb3c45ed72ace9773421f38d9577efe8d516d8ade7d70afa1d6f27e180  docs/migration/task-ledger.json
9eff42515f6f867738ccddbaf2683f9142c8eb3cc4029fc2ca8d8a1edf2b9b79  docs/原生重写路线与迁移TODO.md
6e69fda786449fd2c7a395e0f67e881c4e0140ddca590fba8d0f74cd6bc6ddf7  governance/assembly-first.json
eaedc90d407c1c7f15f3e850b21d43405b0187db8e69b4f58f858ef9c68694c3  governance/bootstrap-policy.json
a6edf12e9258f17ec418f375cf0e23063ca594625b732652443f515e5989455b  governance/runtime-local-modules.json
76b6b50b1e21b454d204d99735f2ae4aecf01b9c2155d8c1b9d8e002e4d45abd  src/runtime/journal.S
5b5443bb197ea6725c74e6777a399b7b35b43b47eef2368cc577124d41bb30b0  src/runtime/journal.h
c58d343e5bb9183c62668dea8e13f90490efb13b1d3b34ec6e8960502129f352  src/runtime/journal_layout.h
3fc941bbe0fe4d44742e365188f9e54ddf5caa30e694a1e4a7ccb85ea2789975  src/runtime/journal_posix.c
2064c8bf7a9499230b577a47326cf2fa04ce6756af5574851088b461b8d56b43  tests/runtime/journal_abi.S
77de5d0ae0a96c27f5185063b2688769844a2371bdd09692eece782a3643a656  tests/runtime/live_journal.c
57a71d56ccddac8818ddec761777eedf4335de6313fbc4b30ba704b8a8d5d588  tests/runtime/test_journal.c
578c8a6e986fbb0dcd40e6b0a1cf4432c3f4e0f2bb3c72bea5774d891a8f1488  tools/runtime/build_test.py
```

## 2026-10-25：单运行检查点、暂停与在途恢复

继续主Agent独占的Durable核心任务，没有修改第二Agent的tools/read文件。参考固定Pi 38ff99177fba21e9c57a670f4e95adcf407c99c7：GenerationTask.prepare提交request checkpoint及model/thinking/options/cutoff再执行；TaskScheduler.open将未完成任务置回pending并保留恢复计数；harness-tasks-recovery.test.ts的“resumes abort work after close at every direct-task abort stage”“crash after the mark before the run joins”等用例。采用消息与阶段同事务、取消意图先提交、已终结不重跑与恢复预算，不声称移植通用Scheduler/任务图。

新增40字节pg_run_checkpoint（id/phase/turns/call_index/outcome/restarts/input_index），Session提交可附检查点或单独提交检查点。事务先校验输入引用/待处理call引用/阶段/预算/终态不复活/活动上下文冻结，再分离数据并提交；回调中可见的view和run仍是旧值，确认后一起采纳。reset仅在非活动时清空当前视图/检查点，日志历史保留。内部change/control/agent尺寸分别48/72/289048，模型、工具定义、pg_tools和pg_client布局不变，所有消费者重编译。

agent.S新增显式pg_agent_resume(agent,retry)和pg_agent_suspend：准入将user与MODEL检查点原子提交；每次模型调用前持久化轮次，assistant+call与TOOL检查点同提交，工具结果交回MODEL，最终answer与TERMINAL成功同提交。cancel先提交ABORTING；取消待工具时补取消结果后终结，恢复ABORTING不执行工具。suspend释放本地请求并丢弃未提交草稿，保持工作待恢复，不伪造终态。16轮/5次连续恢复预算跨进程，显式retry只恢复恢复预算、不清轮次；普通暂停后再恢复也计一次，与上游仅统计crash不同，已明示。

日志读取v1/v2，旧v1保留；有检查点的append或checkpoint-only写v2，尾前增加40字节。v1原有8544缓冲/8530有效帧上限未放宽；v2为新协议字段分配8592缓冲，实际最大8570，文本/消息/文件容量不变。补v2最大帧、检查点区域截断与重算校验的非法字段验证。格式与限制详见native-journal-format.md。

实际验证：十组核心O0/O2 UBSan均通过；11个跨进程中断/完成/取消/暂停位置，含模型开始/partial、待工具、工具已执行但结果未存、结果已存、已终结、取消意图及终态帧已写但未发布。只读工具未记结果时可重读，已记结果不会重复；不承诺写副作用exactly-once。错误注入按新增模型检查点事务调整位置，保留原调用/发布/拒绝断言；新增checkpoint旧值可见性与非法字段断言。ABI/保护页原用例仍通过，新增resume/suspend ABI探针。

真实MiMo三进程：interrupt HTTP200/seq3/messages1/MODEL；resume HTTP200/verified1/seq6/messages2/TERMINAL；recall HTTP200/verified1/seq9/messages4/TERMINAL。候选最终G00、十组测试、无网络PTY Ctrl-D/SIGTERM/resize及termios恢复、真实PTY read/续答/取消/termios恢复全部通过；生产准入仍BLOCKED。共享工作树G00因第二Agent正在替换的tools/read缺旧C/未登记新S而失败，因此候选只取本Agent暂存树，不覆盖/暂存其工作，不声称新工具实现通过。

汇编对象核对：agent text4160、session1287、journal2694字节，data/bss均0，未添加C核心；导入仅既有Session接口、公开libc IO和stat薄胶水。一次失败测试遗留的自有临时journal夹具目录已删除；验证目录均自动清理。

尚未完成：requestId/跨历史幂等、通用任务图、可替换完整调度服务、保存partial、持久化并验证原model/tools/workspace配置身份与TUI恢复入口。resume要求调用者显式提供原配置，当前自动交互持久化仍未启用；不能把这个子集称完整Durable。Goal保持active，继续配置身份与客户端恢复接入。

同一Agent按用户授权自审自批接口/模块登记、v2格式及测试/计划差异，不伪称独立审核。风险为内部ABI变化、配置身份尚由调用者保证、只读可重放范围、同步IO和不确定结果必须重开；未放宽原消息/轮次/文件预算或更改检查器。固定摘要（不含本记录）：

```text
e8b9057fbddf1c5b397cbbc756cadde50bacbc13cff0f2f624808d870ac79c44  docs/migration/native-journal-format.md
2a3115d78662f6702b6cf7da3e108c503c444f545dc69ca6432f1a9097a81bf0  docs/migration/task-ledger.json
e6a1dce459bc0a8e324ebaf9528cf0e988612abfb78d2f552cfc4d0ed0f9ff24  docs/原生重写路线与迁移TODO.md
25b8da7ee6f0d4d984a5def67dfac5ccf810409d7face797650bf4f904e2637c  governance/bootstrap-policy.json
2e746bacd8be3f5313eb43098001ee9bbf396f89e67582d10d669e5fcc55dafe  governance/runtime-local-modules.json
a0edc280cb7cfee4388eb6e95993362d010b0331fd0507d5a35f208301b0edf0  src/runtime/agent.S
5038c515435dc1aea4ae78343a2e87d10129ae0a9f3c1fecd9976d272c8f6521  src/runtime/agent.h
becc56e5707bd8eac0ab9470950d4729ee0530f4eefa9977c1a9660595a95fcb  src/runtime/agent_layout.h
b23be8bf798d710ad95ce902eb3b57b089c8709a0440a5d94c69c85fcee8d48c  src/runtime/journal.S
9ecb1c09a6c8781c26985e8d480a7eefaccb6bd236848a50fd820c1a2abb59e2  src/runtime/journal.h
b446fdb957cceb6f016d903c35085838d36ff4fd030254ec52716072d11b7a39  src/runtime/journal_layout.h
470c44f9d1dca0f71dc3e8d946cee9c129b139a5056d96f9f6e55bde4ea68c31  src/runtime/session.S
7c1d7be8b698493edbfb4933868d2bf6737fd59462bc3c7647575abcf7a47cef  src/runtime/session.h
b33b65e01c4b661d607eedf42caaae4152a32b2e4a57f6d7a0d799478bac381d  src/runtime/session_layout.h
9c0e765122c807a114c63c294d5bacfc1b70ad7932c919d9771bb5a042f006ea  tests/runtime/live_journal.c
44aa8c5982070c33e585e498f61b61ed72619874862ec7d6919f1c9f7a79bf5a  tests/runtime/test_agent_asm.c
e9bcf2d52c0894bea98e4b137c363816bef033b34a568d96e5d093ad980781b2  tests/runtime/test_agent_resume.c
8ec6f7e176aff36e6d195677f79e765c89ce3777ccde1e2e7d4309dbcc13987f  tests/runtime/test_journal.c
68b64596ec5ce86513a1a9697f7c5b97ff950e7128ad4999378cfea6b0a58ed7  tests/runtime/test_session_commit.c
7344ecb89007024ca154933b741d185a3f69254e5c3b5b1676b6bf5fb40c7508  tools/runtime/build_test.py
eb770cf38be442de27b84b626588125099f55ecdb9821b23c09ce8419400e4e5  盘古 Pangu 项目完整文档.md
```

## 2026-10-25：工具汇编交接与恢复客户端接入

第二Agent已交接tools/read，主Agent读取交接、关键实现及独占驱动，完成共享文件/语言/模块登记和产品编译切换。tools.c/read_tool.c删除，补原有tools_layout/read_tool_layout断言和专项进入主测试。独占驱动12次运行复验通过；主Agent发现并修复schema.type的decoded NUL前缀误接受：先校验get_string_len==6再strcmp，补object\u0000extra拒绝回归；不改普通object或尾空白用例。当前工作区G00恢复通过。

主Agent原地删除runtime_bridge.c并手写runtime_bridge.S，ABI布局头静态核对216字节bridge/72字节TUI event/32字节client及枚举，不留C算法副本。读取固定Pi 38ff991的durable/harness/view.ts观察挂载状态与harness.ts显式abort接口；无同构上游C ABI适配器，具体映射按现有原生契约。run_started现在同步bridge.active，支持不经submit的恢复运行；tui_state_attach只消费真实初始run_started并设置忙态，app首次阻塞前调用；异常首事件或poll错误置fatal，不伪造输入/终态。正常闲置poll的旧契约不变。默认shutdown仍是现有有限取消排空；持久入口尚需配置身份和退出保留策略，不声称已开放TUI持久会话。

新增桥接回调栈对齐/主动破坏caller-saved、callee-saved/DF/栈哨兵、贴保护页216字节对象与63字节无终止ID切片；覆盖忙拒绝、已停止拒绝、初始化失败不修改对象、无效ID/枚举/NULL带长度文本、终态状态映射、8次退出排空不假造完成。恢复后run_started→busy→cancel→唯一终态用真实Agent接口验证；TUI状态无submit初始接入与异常首事件新增回归。

第二Agent指出TUI旧abi_probe2/3参数槽覆盖保存的r15/r14已确认：探针改为独立32字节scratch，新增外层探针自检包括rbp。临时恢复旧错误的负例会被新自检拒绝；修复后六组客户端测试通过。旧探针不能作为可靠调用方寄存器保持证据，本轮使用修正后测试结果，不掩盖该缺陷。

实际复验：核心十一组O0/O2 UBSan全部通过（工具专项新纳入）；客户端六组通过；G00通过；产品无网络PTY Ctrl-D/SIGTERM/resize和termios恢复通过；真实MiMo PTY read/模型续答、流中取消与精确termios恢复通过。对象text：bridge698（无未定义导入）、tools1536、read2145字节，data/bss均0；其余导入为公开json-c/libc。语言清单剩一个待迁移C核心mimo.c，必要C平台/组合胶水另列，不称全项目纯汇编。

控制面同一主Agent按用户授权自审自批，仅切换实际源/依赖/测试、标记已交接任务；不修改检查器或放宽预算。风险仍为Linux x86-64公开ABI常量、同步只读工具和未完成持久配置绑定；T24/T25/生产整体验收不勾选完成。Goal继续，下一步配置身份与持久TUI入口。固定摘要随后记录，不含本报告自身。

本批固定摘要（排除本记录，删除项由Git提交记录）：

```text
a6308e76302d51ea32f215107aeba2c1b6ea07ec0dcf042f7f2e4311cfbad6c6  docs/migration/task-ledger.json
80ded12f0beb534261837501e40041d034701c4d34dca93a2be66f9323b5167e  docs/migration/tools-assembly-handoff.md
c02317c1901de89ac4a89bacb418293aad23779d8b3367664dff3facdcb4212f  docs/原生重写路线与迁移TODO.md
37b80c3e088b6181585a009ba4b2cb35baa918fcdcf0026785e0b6b7194ea94b  docs/并行开发任务提示词-工具核心汇编重构.md
47ced420f88899ff64bc104744e43f4da762ca949507fab554d7c1684a474460  governance/assembly-first.json
fe4512e2f6a10de64235cc7a38e20af59c5e2ca05f745d208de2f1bce3ca40cb  governance/bootstrap-policy.json
ae482f24769551b2dd512c6e7d364d1e5a787e86995d38cfe96b9235be91b3aa  governance/runtime-local-modules.json
90d39bde75ec96844cfc04716e5ed5c0f539c0942b8bd60611446216e84ac185  src/client/tui/runtime_bridge.S
7301c89c516af8a64f30afc6b1165846a1797fe8a8a2b7972bf4b3c82f838562  src/client/tui/runtime_bridge_layout.h
f0cbdeee14008e9b229c9cbe70cf42c143b319e014e5c7f35e4e372103834a20  src/client/tui/tui_app.S
5f2d7278364677d5b9d6ea81b82f863ddd6c4146273ff7db94085b88fdc93d63  src/client/tui/tui_state.S
9e58ed1046d317e101a73401d6a24959bdaf3074b39938a696d8af6f35ab0002  src/client/tui/tui_state.h
b80b2d7c45a59f0bb30b16e250df49ebbb37d33cf91ca227a55debce5e91e190  src/runtime/read_tool.S
b6345a84e5d220291748d9ad9027e7077606393273a64ca09b68bb7b81639f2c  src/runtime/read_tool_layout.h
86e25e9552d2b97c8b902ebe64c3be515ce5c00c3adee11a7f7524f4bfea6a90  src/runtime/tools.S
ecada54d377488392be9b0aa76a3ba5bc6493f2ca1c8a6c42f56d204c649f885  src/runtime/tools_layout.h
29cb4dd5e114d4c4d3da091e823d79366d032d27753b7423d6c2bbbc664898db  tests/client/tui/test_runtime_bridge.c
bb7c1a21e399aa8def70eeb7da47c624d894aef5e409d9e1309a7db4780248e7  tests/client/tui/test_tui_abi.c
506646841dbb2ff4fc00f95bcee173c60b461eee807d36ee7b77256caa2270e4  tests/client/tui/test_tui_state.c
321a923c6a63fbfb9cf9106b070a669781c4428c1e00f35075a875a18506fca4  tests/runtime/runtime_bridge_abi.S
2f139ff059f5d1e370a505dfc396eaad72d82656986c31628c70ac05995bc541  tests/runtime/test_tools_asm.c
69aa65a885f46f9f9c6297b8c22d4670475b8141565a0fc5ab74da4fc3e1fec3  tools/runtime/build_test.py
09f8c75a05ef283a57136a6e9e2216d5bd580150c09430fe79f3a1baba20082f  tools/runtime/test_tools_asm.py
```

## 2026-10-25：下一轮并行任务——MiMo协议汇编重构

按用户要求删除已退役派发文档，新增 `docs/并行开发任务提示词-MiMo协议汇编重构.md`。旧TUI派发此前已删除；本次删除当前已完成的tools/read派发，历史交接报告保留。第二Agent独占MiMo协议/投影/生命周期及专项测试，主Agent继续配置身份、持久会话/恢复与共享集成；不派发或声称新实现已完成。任务固定Assembly优先、单版本删除旧C、模型ABI/配置语义、具体源码参考、故障/容量/ABI验证与交接停止条件。

同一主Agent按授权自审自批任务/清单调整，不伪称独立审核。风险为共享工作树与主Agent尚在修改控制结构，因此禁止第二Agent更改主控文件或硬编码无关Agent/control尺寸；冻结provider实际消费契约。未放宽门禁、预算或产品断言。

实际验证：git diff --check通过。当前共享树G00失败，原因是本轮先前尚未集成登记的profile.h/profile.S/profile_layout.h/test_profile.c四个文件；未删除或提前批准这些在途实现。用HEAD临时归档仅叠加本次任务/计划/清单四个文件并删除旧任务后，文档候选G00通过，生产准入仍BLOCKED。仅文档修改，未重复运行产品测试。

固定摘要（不含本报告）：

```text
8ff7a0c71d2bc4da82c4e55f22b30cc2cc983a8574469eeb939a87d25aeeb949  docs/并行开发任务提示词-MiMo协议汇编重构.md
bef6bac17b16efa40684498990dd2f0ee7c611b59149d02db953461221857f09  docs/原生重写路线与迁移TODO.md
9dc4551bbd154807bdf47ff798d3ae3855fcf661b0cc07d081098820542da02e  docs/migration/task-ledger.json
94d56b56dc48dd92f9bb9f431d6c155e0afa70172d1cb856357124c3765a50cb  governance/bootstrap-policy.json
```

## 2026-10-25：配置身份、持久TUI与主文档收敛

参考固定Pi 38ff99177fba21e9c57a670f4e95adcf407c99c7的GenerationTask.prepare/request、agent配置解析和harness-generation-recovery.test.ts中重开仍使用已准备选项、关闭后重发原消息的测试。采用原配置固定、关闭不等于abort；原生用显式公开配置身份匹配替代存储完整TS对象，当前单运行/只读工具，不声称完整Durable。

profile.S手写最多4096字节的长度编码，覆盖provider/model/workspace/实现契约/dev/inode及实际工具定义，不含密钥或指针。Session/日志v3保存；活动运行不可更换，Agent重开先核对再执行。相同身份无额外提交，空闲可显式替换；已有无身份日志的自动交互采用被拒绝。control=4176、agent=293160字节，模型/工具/client ABI不变。bridge=240字节，可选detach回调暂停本地执行，不cancel或伪造终态；默认易失行为保留。C只补组合、realpath/fstat等公开平台结构适配，没有新增C核心。

实际产品PANGU_SESSION_DIR选择现有目录中的session.pgj；启动核对配置、恢复未结束运行，持久退出保留任务，Ctrl-C忙时仍明确abort。保存系统提示且恢复不覆盖，达到恢复上限诊断停下。未实现TUI显式retry界面、历史屏幕回放、requestId去重、通用Task/Scheduler及外部写副作用结算；身份不是防恶意本机代码的安全认证。

实际验证：核心十二组O0/O2 UBSan通过；新增4096字节配置上限、profile未核对/不匹配拒绝、v3最大帧/全部小帧尾切分与逐字节损坏/重算校验后非法字段/fsync不确定重开，编码器四参数ABI探针及探针负例、bridge detach错误/幂等/回调ABI通过。客户端六组通过。产品PTY离线退出/信号/resize/精确termios恢复及持久idle重开无重复提交通过；真实MiMo流中Ctrl-D退出保留MODEL、换模型/工作目录拒绝且日志未变、另进程自动恢复成功、已结束不重跑、第三进程回忆通过。另跑真实read→续答和流中Ctrl-C取消也通过。G00通过，生产准入仍BLOCKED，git diff --check通过。

真实验证最初两次要求60条事实，恢复已收到真实文本但最终进入统一模型协议错误；第一版测试未显示错误终态而等到观察超时，随后补错误诊断。未取得该响应原始finish_reason，不能声称精确根因；当前provider对length结束的拒绝由原测试覆盖。最终完成验收改用当前请求预算内的12条短事实，成功断言、max_tokens和provider实现均未放宽；长输出/错误分类仍是MiMo适配器的限制，不计为成功恢复证据。

按用户本轮决定，将主文档从2276行原地重写为51行，仅保留最小Agent定义、参考分工、Assembly优先、可替换组合、TUI方向和运行入口，不另存旧稿。AGENTS/守卫/TODO同步说明旧计划不自动成为必做产品需求，不再从早期设想扩张范围。历史技术证据保留，主文档不再复述阶段体系。新并行任务保持MiMo模块独占，主Agent不改该实现及其两个测试。

同一主Agent按用户授权自审自批本次源码/模块登记、v3字段与控制变更，不伪称独立审核。检查器/预算/原测试拒绝不放宽；新增身份与ABI风险由边界和真实恢复覆盖。对象text字节：profile413/session1418/agent4501/journal2939/bridge780，data/bss均0；profile/session/bridge无未定义导入，Agent仅Session调用，journal仍为公开IO与stat薄胶水，无旧C核心回跳。

固定变更包SHA256（不含本报告，按暂存差异路径排序的[path,文件SHA256]列表做紧凑UTF-8 JSON摘要，删除项为null）：

```text
334f59bf992bfecd599a06f39a682db406ba26ac4895fa9af15d4c8b6d261ee3  34个路径
5d7091a01f2df5b8b44d9401525bb6bdb7821ba16aea1662a0fff24c9ba3f3f9  AGENTS.md
d829079cf433eb09e55aa5234523dbb3a7fab4005628f9e72197fb9f5e2a21b1  盘古 Pangu 项目完整文档.md
75dbd34b37215cae566e2fb546c841b61a8dab6fc56ae37aace0df2415dddef5  governance/bootstrap-policy.json
41c1d6d42b35ccf6031bdb7aa2511f192f571f10e5d7c4850a0c25907232ca78  governance/assembly-first.json
536bbf4a2ad9863f36fba5a74071dd3c315b4e74acd51430fe3af79e584a0f6b  governance/runtime-local-modules.json
```

Goal保持进行中；下一步继续以参考源码推进实际Task/Submission服务边界，MiMo协议汇编由并行Agent完成后集成。

## 2026-10-25：有界输入Submission与requestId去重

读取Pi 38ff99177fba21e9c57a670f4e95adcf407c99c7的Submissions.submit→admitSubmission、SubmissionDraft/Record，以及harness-submissions.test.ts的deduplicates request IDs per conversation before any write、reacquires a submission after reopen and settles it durably。采用去重先于busy/写入、同键不同文本返回原记录、关闭重开后保留状态；原生只做当前input，不复制队列、write型submission、遥测或阻塞wait框架。

新增手写submissions.S无IO/分配的键查询，pg_agent_submissions返回可替换函数表；默认准入复用现有Session原子提交。kind=5把请求键、单条user与新MODEL检查点同提交，确认前不可见；后续检查点按运行id更新settled/outcome，reset保留历史键。最大64个键、1～63非NUL字节，满额拒绝新键、不静默遗忘，无键客户端行为保留。来源缓冲先分离；回调改变原输入后仍返回已采纳记录，不再回读原键。

日志新增v4仅用于kind=5，payload为u32键长+键+一条旧格式消息+旧40字节检查点，最大当前输入帧4274字节，8592帧缓冲/16MiB文件/消息预算不变。读取v1～v4，未知版本拒绝；旧二进制不能读取新帧，不自动降级。control=9304、agent=298288、record=80、服务表=24字节；checkpoint=40/change=48、模型可见Session、pg_model/pg_client/工具布局不变，内部消费者同批重编译。没有外部副作用exactly-once、通用调度或完整Durable兼容声明。

实际验证：13组核心O0/O2 UBSan通过；包含忙前重复、不同内容/无效新内容同键复用、成功/错误/取消状态、reset/多键重开、满64键与无键继续、63字节保护页键/80字节页尾输出/4095字节输入、直接事务重复或非法工具字段拒绝。提交回调验证旧记录不可见/重入拒绝/源数据变化、REJECTED/UNCERTAIN无部分采纳；v4每个尾切分/逐字节损坏/正确校验的非法字段、fsync失败及确认前进程_exit重开去重通过。新增6参数/结构返回/查询ABI探针及负例自检，原Agent页尾/回调守卫保持通过。

真实MiMo三进程--live-submission实际HTTP200：中断seq3/messages1/MODEL；重开先去重再resume，成功seq6/messages2/TERMINAL；第三进程已结束去重无网络/写入，再新键回忆成功seq9/messages4。完整候选树9597f2dac8720aad878b44cc862186f5203ce48e在临时归档复验G00、13组核心、6组客户端、离线PTY及真实--live-persistent全部通过；生产准入仍BLOCKED。共享工作树G00因并行Agent新增未交接MiMo文件而失败，这些文件不在本次暂存树，不提前登记或声称其通过。git diff --cached --check通过。

同一主Agent按用户授权自审自批模块/源登记与v4变更，不伪称独立审核；不改检查器、不放宽既有预算/断言。风险为内部ABI增长、有界键容量、只支持input和格式单向升级，已明确范围；状态表与查找不引入C核心。对象text：lookup103/session1874/agent4932/journal3233字节，data/bss均0；导入仅既有Session/lookup/公开IO/stat胶水。主文档仅更新一处当前能力描述，未扩张其简短定义。

固定变更包SHA256（不含本记录；按暂存路径排序的[path,文件SHA256]列表，以紧凑UTF-8 JSON取摘要）：

```text
549f17e24251cdd52e97ec4b345ee6a7cb8165f750c633b35da2e82e8bad1413  22个路径
```

Goal继续；MiMo重构由并行Agent完成后再集成，不重复实现或覆盖其文件。

## 2026-10-25：持久会话启动历史与终端数据转义

读取固定Pi 38ff99177fba21e9c57a670f4e95adcf407c99c7的Durable Views.#build/advance及harness-view.test.ts的hydrates active entries、shares unchanged parts；旧Coding Agent的renderInitialMessages→renderSessionEntries→renderSessionItems。采用先挂载已提交事实、再观察运行的边界，不重送输入或伪造执行事件。另读utils/ansi.ts及ansi-utils.test.ts的OSC/CSI/不完整序列案例，tui/program-status.ts及测试的控制字符规则。原生选择可见转义而非复制TS组件树/正则或直接删除整段序列，保留LF/tab。

新增可选history(index,out)只读启动接口，桥接显式借用已提交Session视图并固定count；最多64条、文本4095、工具字段63字节，结果核对相邻call关联。客户端一次性渲染user/assistant/call/result后再attach，history不调用submit/poll/cancel、不改运行状态或日志，系统提示和未提交draft不回显。非法字段、快照count变化、错误或不终止回调失败关闭，不继续执行任务。当前仅scrollback，不含历史浏览器、搜索、会话选择或持久编辑草稿。

展示纯逻辑保持手写汇编。C0（LF/tab除外）、DEL、C1和非法UTF-8以可见转义输出；按完整UTF-8序列扫描，不让畸形序列藏住控制字符。跨事件carry先拼成完整序列再过滤；复核发现旧终态会原样冲出残留半序列，也已转义并补回归。可信编辑ANSI不变；只改展示，Session原始数据/模型投影、日志格式、模型/pg_client ABI均不变。内部ops=56/history_entry=64/bridge=264，state仍32，原padding存一次加载标志；所有消费者重编译。没有新增C算法、第三方依赖或文件。

实际验证：13组核心O0/O2及6组客户端通过；历史零执行/只加载一次、64条截止、非法字段/回调错误/非终止、工具关联/快照变化、桥接对象与64字节输出页尾、caller-saved破坏/栈对齐/寄存器探针通过。ANSI/plain展示、中文/emoji/C1/ESC的全部两段切分、非法前缀和终态残留carry回归通过。首次候选3c82540ae6ab2224805d402416b1d0c32b5c2c4d完整验证后，仅补View终态carry转义及其测试；最终候选c7e127270b5aedbb9008b2aca9531370134613fb重跑G00、6组客户端、离线PTY与真实MiMo --live-persistent/--live均通过。真实恢复只回显已提交输入、终结重开回显user/assistant、未重跑、回忆成功且历史加载前后日志字节相同；read续答/取消/精确termios恢复通过。生产准入仍BLOCKED。

同一主Agent按授权自审自批客户端内部ABI与模块公开视图依赖/导出登记，不伪称独立审核；检查器/预算/原拒绝断言不放宽。风险为借用快照要求单所有者、内部消费者须重编译和展示字节转义扩展（最多4倍原始文本）；不声称是完整终端模拟器或安全沙箱。对象text：bridge1172/state885/view3796/app1961字节，bridge无未定义导入；app原信号状态data12字节保持。

并行MiMo交接文件已出现，本批不纳入其新源或mimo.c改动；候选从本Agent暂存树归档验证，未声称共享工作树G00通过。主文档保持51行。固定变更包SHA256（不含本记录，排序[path,文件SHA256]紧凑UTF-8 JSON）：

```text
c855bf7e422785363b3dc129abb31de7d9f82a042d8deda7796f13f695d19720  22个路径
```

Goal继续，下一步审核并集成MiMo汇编交接，不把交接自述当实际验证通过。

## 2026-10-25：MiMo协议汇编交接集成

主Agent读取mimo.S/projection/stream、对应头/专项及Pi 38ff991的openai-completions stream/convertMessages/mapStopReason，沿用generation的provider/宿主边界。第二Agent交接被复验并修复，不直接采信报告通过字段。正式构建输入改为三份手写汇编；测试驱动显式链接三份源（单stem解析不自动包含另两份），新增所有权专项进入主验证。原mimo.c、语言pending角色及旧源码登记删除，无C备份/回跳；四个公开导出、pg_mimo/pg_model/事件/Session视图布局不变。

复核修复：工具参数parse_end跨json_tokener_get_error的rcx易失寄存器；未挂入JSON树的容器/叶子、插入/序列化失败清理；finish_reason decoded NUL前缀、原始NUL接收；slist追加失败遗失旧链及curl setopt/getinfo错误未检查。JSON树使用局部所有权位，成功挂入父对象才释放本地所有权，不新增框架或依赖。真实验证又发现普通assistant分支复用了lookahead索引为role：前两进程中断/恢复成功，第三进程追问初始化失败；新增普通两/三消息回归先确实失败，角色从当前消息重载后真实完整链路通过。未放宽任何原断言、预算或结束理由；max_tokens512及length拒绝仍为当前局部限制。

实际验证：原交接16次复验通过；修复后5套件×O0/O2×普通/UBSan=20次全部通过，UBSan设不恢复；主核心16组O0/O2 UBSan通过（新增MiMo ABI/运输/所有权三组），客户端6组通过。所有权75个构造/插入/序列化逐点失败、真实析构观察与最终O2 ASan通过。最初探针误认为userdata删除通知只发生一次，json-c0.19的迭代容器释放实际重复通知并复用context；经GDB确认后按对象身份计唯一释放、不读取context，未降低存活对象归零断言，ASan也验证无实际泄漏/非法释放。另修正新测试中slist脚本变量声明顺序的编译错误；均不计作通过证据。

两份仅在临时目录构造的负例：恢复rcx缺陷→运输工具参数测试退出-6，遗漏未挂入消息清理→所有权测试退出-6；普通对话角色错误也在修复前被新断言退出-6检出。负例/产物均在临时目录删除，未保留旧实现。对象text字节为lifecycle1895/projection3018/stream1610，data/bss均0；导出恰四个既有函数，导入仅公开libcurl/json-c/libc及自身两函数，无旧C核心依赖。

真实MiMo用全汇编协议代码实际通过：PTY持久流中退出/身份不匹配拒绝/自动恢复/历史只读回显/第三进程回忆，另有read工具续答、流中取消与精确termios恢复；--live-submission三进程HTTP200，seq3/messages1/MODEL→seq6/messages2/TERMINAL→seq9/messages4，重复请求未新增写入/网络。失败的首次追问测试明确不计为成功，其修复后重跑整链通过。G00及assembly-first检查通过，当前交互组合无待迁移C核心角色，仅composition/termios/stat三份必要C胶水；不等于全仓都是汇编。生产准入仍BLOCKED，完整Pi provider与跨平台兼容未完成。

同一主Agent按用户授权自审自批源码/构建/语言/模块登记；未改检查器或预算，历史阶段状态不自动提升。协议仍是单choice/单工具、有界缓冲和同一公开配置，未更换模型、TLS端点或凭据来源。主文档仅补当前MiMo汇编适配一句、保持51行，派发标记完成；下一项按实际Agent/Durable需求推进。

固定变更包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON，删除项null）：

```text
0d3e6b49c336384e0e31e74a3bef55e95cbd8bcd564cfefa5c4e6a1cf2788cd9  19个路径
```

Goal保持进行中，当前不是完整最小Coding Agent或生产交付完成。


## 2026-10-25：write工具与不可重放调用恢复

读取固定Pi 38ff991的tools/write.ts、file-mutation-queue.ts、index.ts、env/node.ts::writeFile，harness/tool.ts的intent/execute恢复分支与tools.test.ts、harness-tools-recovery.test.ts。采用创建/覆盖、递归父目录和默认unsafe策略；只有已记录与当前策略均safe才重跑，否则提交“interrupted and may have partially run”错误，继续模型轮。当前同步单执行器无需并发mutation queue；不宣称异步取消中写入、并发队列或完整Durable Task兼容。盘古有界read显式声明safe是原生策略选择，Pi内置read并未声明safe，不把该差异说成逐项一致。

新增手写write_tool.S及契约/布局断言，复用json-c与公开POSIX接口：相对路径、拒绝dot/空段/符号链接/非普通文件及多硬链接文件，调用者持有可信根fd；父目录创建后逐级打开，文件truncate后处理短写/EINTR并同步文件/父目录，所有退出清理fd。参数总量仍小于4096字节；允许JSON转义NUL内容。失败可能留下目录、空文件或部分内容，不提供写事务、抗敌对并发目录迁移沙箱或外部副作用exactly-once保证。

工具定义64字节（新增replay_safe）、服务32字节（新增纯策略查询），registry和Agent布局/初始化/所有调用方一并更新；内部调用者须重编译，没有双实现或旧ABI兼容壳。调用意图与assistant/call原子提交：phase2记录safe，新增phase5记录unsafe，同为40字节检查点；恢复还要求当前服务明确返回1，NULL/未知/非法值均不重跑。取消优先于恢复执行，已提交结果不再执行。现有JSONL/通用调度等范围不扩张。

实际验证：核心17组O0/O2 UBSan全部通过，客户端6组通过，write专项O2 ASan/UBSan通过。路径/类型/输出容量、UTF-8/NUL、覆盖/空内容、短写/EINTR/零写/ENOSPC/部分写、truncate及fsync失败与重复失败后fd计数检查通过。跨进程原11个中断点分别以safe/unsafe运行，加存量/当前safe与unsafe、NULL/非法值、执行前后16个组合及恢复tool_started后取消；旧断言在safe模式保持原要求，unsafe明确不得重复副作用。

真实MiMo：最终普通问候文本通过实际TUI写入generated/answer.txt→read工具回读→精确文件字节/unsafe日志意图→重开历史与日志、mtime不变，termios精确恢复。先前标记内容请求出现未调用工具的失败，另次通过；最终复验又明确收到模型将标记视作注入/水印而拒绝。改为普通文件内容后仍保留实际工具调用、字节、日志与不重复改写断言；拒绝请求不计为写入成功。既有真实read续答、流中取消和持久退出/身份不匹配拒绝/自动恢复/跨进程回忆也通过。

组合层启用read/write，工具配置身份改为read-write-v1；旧工具配置的在途运行仍拒绝静默切换。闲置会话才更新组合层固定system提示，避免保留“不能写文件”；离线有效旧提示夹具验证旧帧保留、仅追加一次更新、再次重开不重复提交。在途提示不被改写。主文档保持51行，TODO仅勾选此子集，T23/T25仍IN_PROGRESS；edit/bash及完整阶段验收未完成。

同一Agent按用户授权自审自批本批源码、内部服务ABI、模块/语言/文件登记及证据；未修改检查器、预算或放宽既有行为断言。G00、assembly-first、diff检查通过；生产准入仍BLOCKED，本地通过不提供服务器侧防绕过保证。无新依赖、C备用或自动生成汇编。

本批收尾fetch后，Pi与Harness工作树均干净，HEAD与各自上游一致（ahead/behind均0）；仍采用38ff991/22ddee3固定版本，没有未评估的上游变更。

固定变更包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON，删除项null）：`7f0ef752ba948bdd65e1b7bceb3b1433edd37578cfc4a55c8f73b407b4728512`（35个路径）。


## 2026-10-25：edit文本内核移植（文件工具入口待接）

读取Pi 38ff991的tools/edit.ts::createEditTool/prepareEditArguments、edit-diff.ts::applyEditsToNormalizedContent及匹配/归一化/行块恢复helpers，直接调用的withFileMutationQueue/env.writeFile和tools.test.ts编辑用例。先实现手写edit_text.S纯文本内核：同一原文批量匹配、NFKC后的重复检查、稳定排序/重叠拒绝、模糊匹配按实际范围保留未改动行块、BOM和CRLF恢复；不是仅做简单字符串替换。Unicode NFKC/迭代复用已安装utf8proc 2.12.0公开API（MIT/Expat），新增依赖/实际构建登记；不自写Unicode数据库。Loop无此依赖，当前交互构建链接该模块。

实际验证：1300例以固定Pi实际edit-diff.ts函数为oracle，O0/O2均为成功输出逐字节一致或双方拒绝。测试只剥离TS类型并移除未调用的diff展示库import，源码与固定提交逐字节核对；没有重写一份“预期算法”作oracle，references不进入产品构建且未修改。主核心现18组O0/O2 UBSan通过，专项O2 ASan/UBSan通过；caller output失败不变、alias、NUL/非法UTF-8、页尾输入/输出、合计输入与中间/结果配额、calloc及各NFKC分配失败均检查。实际对象text3153字节、data/bss0，唯一导出pg_edit_text_apply，导入仅公开libc/utf8proc函数；离线实际TUI退出/终端恢复/持久闲置重开与提示更新通过。G00、assembly-first、diff检查通过，未改检查器或放宽原断言。

边界：UTF-8无NUL，内容、合计edit字节、归一化中间/结果均最多4095字节，1～16个替换；并非完整无界Pi/全部Unicode版本兼容证明，错误文字/结构化diff不在本次共识范围。当前尚无edit工具参数准备/文件IO入口，未把文本对照说成真实MiMo编辑闭环；T25保持IN_PROGRESS、完整edit条目保持未完成。下一步复用注入的现有read/write服务接入工具，移植canonical edits及模型常见输入准备，沿用unsafe恢复，再跑真实模型文件编辑。

同一Agent按用户授权自审自批本批文本模块、Unicode依赖与文件/语言/构建登记。主文档仍51行，只补实际构建依赖；未新增C核心、备用实现或认证框架。生产准入仍BLOCKED。收尾fetch后Pi/Harness仍分别38ff991/22ddee3，与各自上游一致、工作树干净，无新上游代码待采用。

固定变更包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON，删除项null）：`b54b3a3be468c8b6da549583458d54241e27a078e97a82857d851d710e1616d2`（12个路径）。


## 2026-10-25：edit文件工具接入与真实验证

复读固定Pi 38ff991的edit.ts::prepareEditArguments/createEditTool、tools.test.ts参数修复/批量冲突用例及harness/tool.ts的unsafe恢复分支。新增手写edit_tool.S：canonical edits[]、单对象、JSON字符串数组/对象、顶层oldText/newText及与已有数组追加；解析树自持有、切片借用至调用结束，不修改原始参数。通过注入pg_tools调用read/write，保留内层校验/授权，不复制POSIX文件后端。同步单执行器的owner串行约束代替进程内异步mutation queue；外部并发文件锁不在本次范围。

审核修复两类准备边界：可识别的单项编辑若为空oldText/未知属性等非法，不得丢弃后只执行附带legacy项；有效canonical edits附带不成对/类型不对的legacy字段时按Pi忽略。后者由直接调用实际Pi prepare函数的差异测试先检出，随后修正实现及本批测试中原先过严的假设；未加差异豁免，未知非legacy属性仍按原生schema拒绝。91例实际prepare+文本函数与原生文件结果共识通过；成功字节相同，拒绝保持原文件。原1300文本案例继续通过。

实际验证：核心19组O0/O2 UBSan通过；edit工具O2 ASan/UBSan与91例对照通过。17个JSON解析/构造/插入/序列化失败点、参数不变、读写授权次序、无匹配/重复/重叠/无改动不写、路径拒绝、带引号文件名、JSON转义后超额不写、错误长度、页尾与fd回收均检查。真实文件已写成功但回执报错时返回“may have partially changed”；跨进程在write已刷盘、tool-result尚未提交时退出，重开保持unsafe检查点，提交interrupted错误并续答，文件内容/mtime及内层read/write计数均未变化，证明恢复未再次执行编辑。

真实MiMo最终版本实际通过：TUI用edit完成两处配置修改、read回读并续答，BOM/CRLF/未改动空格逐字节保持；phase5意图及终态落盘，重开历史不重新运行，日志与文件mtime不变、termios恢复、凭据不入日志。组合read/write/edit三工具，配置身份read-write-edit-v1；旧在途配置仍拒绝静默切换。主文档保持51行，近期edit工具项勾选；结构化diff/patch展示、一般异步队列、完整T25和bash未完成，未宣称全部Pi兼容。本批无新外部依赖。

同一Agent按授权自审自批工具模块/组合/文件/语言登记，未修改检查器、预算或既有通过基线；G00、assembly-first与diff检查通过，生产准入仍BLOCKED。对象text3560字节、data/bss0，唯一导出pg_edit_tool_definition，无直接POSIX文件操作导入。

收尾fetch发现Pi从38ff991新增4个提交，干净工作树已快进至0eea54f11c2c8d45ec3dfaa40522030539ab28f1。已读差异：旧Coding Agent的带slash provider/id解析、NPM symlink prefix，以及no-env测试脚本和Windows入口图路径规范；本批Durable edit/harness文件均未变，固定38ff991函数oracle字节核对仍通过，暂无本批原生修复需采用。Harness仍22ddee3，上游一致；不把参考工作树更新当成所有新行为已移植。

固定变更包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON，删除项null）：`59ecf34f9cd8fd8fa4c16d0e84573649b116eab7fee62e69907af3a0ef19a651`（14个路径）。


## 2026-10-25：非阻塞进程后端（bash工具尚待接入）

读取固定Pi 0eea54f的tools/bash.ts、env/node.ts::exec/killProcessTree/waitForChildProcess及env-node.test.ts、tools.test.ts命令/取消/超时/继承stdio用例，辅读旧Coding Agent的child-process.ts。新增手写process.S与可替换pg_process_ops：成熟libc posix_spawn负责启动/exec，汇编负责独立stdout/stderr非阻塞块、退出码/信号、单调时钟超时、取消/关闭与资源回收。未自行实现fork/exec框架，也未把命令运行放进同步工具execute阻塞TUI。

采用无默认超时、进程组SIGKILL、信号退出映射128+signal、正常退出后100ms输出空闲收尾（有新输出则重置）。原生差异：超时从start前的时间戳计算、由next协作观察；取消/错误时确认leader退出后最多排空100ms，防止持续的外部输出拖住关闭。waitid WNOWAIT保留leader为可等待状态直到最终waitpid，避免自己提前回收导致PID复用；要求默认SIGCHLD、owner不得外部回收或改其处置。子进程工作目录/环境显式提供，NULL环境不继承，stdin=/dev/null、无关FD关闭；这不是工作目录沙箱/守护进程监管，正常结束的后台后代不承诺全部清理。

实际验证：核心20组O0/O2 UBSan通过，进程专项O2 ASan/UBSan通过。真实OS命令验证非零/信号退出、二进制/大块双流、stdin EOF、cwd/env/信号默认值/FD继承、stdout已关但leader未退、忙拒绝/复用/终态一次、取消/超时/协作close后leader已回收。27个启动失败点保持实例不变、FD计数不变、无命令副作用/遗留子进程；另测read/waitid/waitpid EINTR/EAGAIN、IO/clock错误、kill失败重试、命令与实例页尾、低编号FD。持续输出取消收尾用read故障注入模拟，不冒充真实逃逸进程或模型验证；正常后代持续写与安静持有stdio均用真实进程检查。测试修正了即时终态可能被驱动丢弃的等待方式，未放宽操作语义断言。

当前宿主GNU libc 2.44；只是使用已有libc的新接口，无新第三方库/C核心。product builder新增对process_layout.h的C语法/sizeof/offset/alignment断言检查，防止opaque spawn存储布局不符时仍链接运行。实际组合构建与离线PTY终端/持久闲置检查通过；对象text2450字节、data/bss0，实例4160字节，只有公开libc依赖。原始输出允许二进制/UTF-8分块，消费方负责解码、保留/溢出；尚未接Runtime/registry异步工具协议或bash入口，本批没有真实MiMo命令闭环，T26保持IN_PROGRESS、bash项未勾选。下一步接异步工具生命周期，再完成bash输出与真实交互。

同一Agent按授权自审自批进程服务、实际构建/文件/语言/依赖登记。G00、assembly-first、diff检查通过；生产准入仍BLOCKED，不宣称完整平台/性能/服务器侧防绕过认证。主文档仍51行，未扩张额外框架。

收尾fetch发现Pi新增d5b3de22d0e24a3e9c38a9410d3329b099717283，干净树已快进。已读generation及两份测试差异：请求thinking level写入assistant消息/deferred checkpoint，旧缺失字段不补造；本批process/bash参考文件未变。当前MiMo固定thinking disabled、原生无thinkingLevel/deferred字段，尚无对应配置迁移，不伪称该新增语义已移植；未来引入该能力时需采用请求级别的固定记录与恢复语义。Harness仍22ddee3。

固定变更包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON，删除项null）：`2e5dd6bd81dc9fdefec56be86f798dbfebaab7b1703f101214f41de8cf3c750e`（11个路径）。


## 2026-10-25：文档收敛与异步工具协议子集

主文档压缩为34行，只保留目标、参考分工、开发原则和TUI方向；旧清单不自动扩大需求，具体实现开发时查对应源码与测试。

继续此前进程后端接入。参考Pi d5b3de22的tools/bash.ts::runCommand、env/node.ts::exec的onAbort/emit/settle、harness/tool.ts::execute/abort及harness-tools.test.ts“answers an aborted tool with only its durable output”。采用取消先请求、回收后终结、执行意图持久化及unsafe恢复不重跑。原生差异：当前无通用Task图和持久进度slot；OUTPUT只是临时事件，最终结果由提供者明确提交，重启丢失未提交输出，不冒充上游durable进度。真实进程测试提供者在取消后返回已保留的输出，再由Agent原子提交错误结果；不是上游abort进度slot行为完全兼容。

手写汇编registry与Agent支持start/next/cancel/close、临时输出和单操作所有权；close确认释放后才提交结果/终态，清理错误保留所有权重试。TUI接入临时工具输出及控制字符转义，shutdown改为0已释放/1待清理，主循环每10ms继续推进，取消只发一次。持久模式detach反复suspend到释放，不生成durable abort。桥接错误保留stop_error和所有权，不再八次poll后假装关闭；永久无法清理时退出也不会错误宣称完成，当前没有后台监管/强制超时丢弃。同步工具沿用原行为，ABI布局同步至调用者、MiMo描述和profile，使用者需同步重建。

实际验证：核心常规构建测试O0/O2退出0；真实OS进程专项O2 ASan/UBSan通过，覆盖进度、忙拒绝、延迟/错误关闭、取消、暂停/unsafe恢复、abort存储失败后回收，以及通过真实TUI桥接退出/挂起20次延迟关闭、最终pid已回收/重复shutdown幂等。TUI测试退出0；实际组合构建、离线PTY退出/信号/termios恢复通过。真实MiMo/TUI edit→read回读、BOM/CRLF/未改动字节、unsafe intent及重开不重复改写通过。异步单元模型是确定性替身，本批没有真实MiMo异步命令验证；bash适配、流解码和输出保留/溢出仍待完成，未开放产品异步bash，未声称完整Durable或生产准入。

同一Agent按授权自审自批新增测试登记、构建集成、模块说明与内部接口调整，未修改检查器或放宽基线。开放产品bash前须完成剩余输出/参数处理和真实模型验证。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`8650d0b6c50a3f05cbe8cbae17172a1dc2b27c6bd192b6e6f9997e572569e883`。G00、assembly-first与diff检查通过，生产准入状态未变。


## 2026-10-25：bash逐流输出解码

已读固定Pi d5b3de22的env/decode.ts::StreamDecoder.decode、env/node.ts::exec的独立stdout/stderr解码与结束flush；读取env-line-scan.test.ts的“keeps a U+FEFF that does not start the stream”和任意分块测试、env-node.test.ts的分块进程输出用例。采用UTF-8 replacement、保留未完成前缀、只去掉首个BOM，非法前缀后的BOM保留。

新增手写stream_decode.S，28字节调用者状态；每流独立实例、最多4096字节输入，输出容量明确要求3*n+3，结束flush后封闭，拒绝容量不足保持状态不变。无堆分配/运行时依赖，不复刻Node框架。只是命令输出内部helper，不增设插件框架或通用编码服务；bash工具尚未消费该接口。

实际验证：140477条逐块输出与固定Pi实际StreamDecoder通过（Node直接导入只读参考decode.ts，源摘要固定；不是另写TS预言机），包括所有256个单字节和65536种双字节的整块/拆块、特殊前缀/中途BOM、随机及4096字节坏输入。核心O0/O2 UBSan完整构建运行退出0，专项O2 ASan/UBSan通过；另测双流状态隔离、容量拒绝不变、页尾状态/输入/输出及重复final。实际产品组合构建通过，G00、assembly-first与diff检查通过。该批没有改变模型或现有工具调用行为，未做新的真实模型验证；现有进程与协议专项不等于bash已完成。

同一Agent按授权自审自批helper模块、文件/语言登记与测试构建；未修改检查器/预算或放宽基线。风险/限制：调用者须初始化并逐流维护状态，输入输出和状态不得重叠，原始完整输出保留与溢出策略仍须在bash适配中完成。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`5535cf683f1af81e5b395882294b2b31e6ebf25f5fab37b9aabe6c0fbd4fbabc`。


## 2026-10-25：bash本地工具与真实命令闭环

阅读固定Pi d5b3de22的tools/bash.ts::validateTimeout/prepareExecution/runCommand/createBashTool，env/node.ts::exec的双流解码、onAbort、spill、finishSpill和settle，以及tools.test.ts的bash状态/超时/环境用例、env-node-spill.test.ts的背压测试。fetch确认Pi仍d5b3de22，Harness仍22ddee3（其上游分支为master），参考源码未改。

新增手写bash_tool.S，借用可替换pg_process_ops；参数、调用状态、双流解码、尾部保留、raw文件和释放都在汇编，C只在入口显式组合。采用非持久/bin/bash、cwd/env、可选秒超时（无默认）、非零/信号/超时/取消错误、临时输出和完整原始输出文件。原生差异：当前消息槽4096字节，保留3072字节/2000行UTF-8尾部，开头放退出状态和文件路径以供TUI短摘要显示；没有上游structuredOutput、prepare hook或PowerShell兼容。超时正小数按毫秒截断，最小1ms，上限2147483647ms。执行不构成目录沙箱。

完整raw输出按stdout/stderr到达顺序写入mkostemp的0600文件；普通小结果在发布前关闭并删除临时文件，截断才保留路径。采用从启动便写临时文件，避免丢失溢出前缀，未照搬Node异步文件流；每次poll最多处理一个4096字节进程块，普通文件IO仍可能等待磁盘。捕获硬上限16 MiB，超过即取消并以partial文件错误返回，不声称文件完整。取消/暂停借用现有协作回收；未发布就detach时删除临时文件，删除失败保留所有权供重试。已发布的完整/partial临时文件由使用者管理，不保证/tmp清理或主机重启后仍存在，不作为Session的持久附件。

默认组合只给命令PATH=/usr/bin:/bin、LANG=C.UTF-8，未继承模型key或父环境；已有profile实现身份改为read-write-edit-bash-v1，旧在途配置不能静默换成新工具集。命令本身无沙箱限制，环境不继承不等于任意同用户命令无法访问主机资源。参数严格解析并拒绝decoded NUL（包含会被json-c键截断的参数名），普通转义反斜线文本仍可使用。

实际验证：核心常规构建O0/O2 UBSan退出0，bash专项O2 ASan/UBSan退出0。真实进程覆盖空命令、双流、非零/信号、超时、取消/重复取消、未发布关闭、UTF-8/BOM/坏字节、原始文件字节/0600权限、3072字节/2000行/emoji边界、16 MiB停止、FD计数、页尾上下文/输出、mkostemp失败、三处JSON分配失败、短写/EINTR/零写/ENOSPC、close/unlink错误及删除重试。五次确定性单元模型驱动真实6000字节命令，Agent收齐进度并提交带完整文件路径的结果；该模型是单元替身，不冒充真实MiMo。sanitizer主要覆盖C测试/库边界，汇编内存边界由长度断言与保护页用例验证。

真实MiMo/TUI：命令写文件与环境key未继承、exit7错误结果、Ctrl-C取消后无late副作用、unsafe intent和已终结重开不重复写通过；6000字节raw文件完整且0600、可见摘要显示路径、Ctrl-D协作退出保持TOOL_UNSAFE，跨进程恢复返回interrupted且原副作用不重复，通过。启动/信号/resize/termios、闲置持久重开检查也通过。联调发现末尾诊断会被TUI snippet隐藏，结果元数据已前置。两次早期长输出联调未通过路径断言，当时失败响应留存不足，未全部定位，不能将后续通过写成那些失败均已修复。

完整read→edit→write→bash汇编构建/测试的真实用例已加入--live-workflow，当前**未通过**：初版测试汇编立即数写法已单独修正并验证可构建；随后无网络内容泄露的临时诊断驱动记录HTTP200、tool_calls分块从index0进入index1，当前单调用MiMo解析器即拒绝，尚未到达完整构建。官方[MiMo Chat Completions文档](https://mimo.mi.com/docs/en-US/api/chat/openai-api)亦明确tool_calls可以含多个调用。没有放宽单调用拒绝测试、丢弃后续调用或把完整工作流标成通过；下一批需参考Pi完整assistant工具集合，接入有界批量调用及Durable提交/恢复，再复验这一用例。诊断驱动只是/tmp中的临时测试客户端，未进入产品；T26/T28与完整P1继续IN_PROGRESS。

同一Agent按授权自审自批bash服务、实际语言/文件/模块/构建登记、四工具组合和profile身份更新；未改变检查器、预算基线或生产准入。接口新导出只有pg_bash_tool_init/definition，对象text/rodata合计3604字节、relocation只读表64字节、bss0；调用者状态15592字节，另借用原进程提供者。G00、assembly-first和diff检查通过。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`c18bdba071e1a6cb75d8c5e8359fc30fd3eb31632bc5fd98255eb4a33ff27ffd`。


## 2026-10-25：批量工具的原子存储基础

读取固定Pi d5b3de22的harness/generation.ts::classify/startToolRound、tools顺序阶段和readCalls/createToolTask，以及harness-tools.test.ts的顺序/并行trace和顺序工具状态用例。采用先提交完整assistant调用集合、再逐项推进并等待结果；当前原生先走顺序路线，不声称上游默认并行或通用Task图已移植。fetch后Pi仍d5b3de22、Harness仍22ddee3，未改参考源码。

Session新增有界checkpointed批次（3～9记录，对应assistant+最多8 calls），先分离/验证再Storage确认后采纳；无CP的大批次拒绝。工具组提交后不能追加新的calls，活动CP只能指向下一未有结果项；重复ID、跳序、错误结果ID、漏结果后直接终结均拒绝。结果单条或取消时整组补齐，都受同一事务约束。单调用兼容不变，也补齐is_error取值检查与journal一致。

新wire v5仅kind append/count3～9且固定40字节CP；消息wire字段不变。旧v1～v4消息数/帧长度限制未放宽，旧读取器遇v5拒绝；消息4096、会话64条及日志16 MiB上限不变。新功能需要的编码缓冲38328字节，最大字段批次实际38229字节。Session本地帧38392、journal提交38360、open85976字节，open还嵌套Session；明确增加的临时栈成本，入口逐页探测而不一次跨过guard。没有新持久状态副本、依赖、门禁或审批系统。详见原生日志格式文档v5节。

实际验证：完整常规核心O0/O2 UBSan退出0；最终专项O0 UBSan及O2 ASan/UBSan退出0；离线真实产品PTY的退出/信号/resize/termios和持久闲置重开通过。专项涵盖9条全部最大字段、源批次被Storage回调修改时仍分离采纳、reject/uncertain不采纳、边界容量、重复ID/跳序/错配/漏结果、顺序结果每步重开及8个取消结果原子终结、独立进程回放、全部小批次尾切分/逐字节损坏、有效checksum的非法版本/kind/count/CP、旧版本及v5超长头不当作torn修复、三处半写/末尾失败与完整帧fsync不确定恢复。32 KiB小栈下的入口guard预期SIGSEGV，未创建日志文件，证明会触碰中间保护页。Sanitizer主要检查C/库边界，汇编由保护页、字段/长度与实际IO用例验证。

同一Agent按授权自审自批新增测试登记、构建集成、Session聚合契约和新v5范围。旧格式上限保持原值，不以扩大旧预算让用例通过；新增栈/帧开销已明确。G00、assembly-first与diff检查通过，生产准入未改变。本批没有再次调用真实模型；这只是必要存储基础，当前MiMo仍拒绝index1，Agent批量调度、projection、TUI历史与--live-workflow依然未完成，接下来继续对应实现。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`9f4bea059d2b4b8faea6a09ebbeead99cab7fad59795687f19c6708440679c26`。


## 2026-10-25：MiMo完整多调用响应与Model批次事件

读取Pi d5b3de22的ai/src/api/openai-completions.ts::ensureToolCallBlock/逐tool_calls迭代及流结束分类，tool-choice.test.ts混合文本/多工具分片fixture，以及Durable generation.ts::startToolRound。采用按调用索引分离参数、完整assistant调用集合到达后才交给执行层。原生范围仍要求明确连续整数index0～7、函数工具与完整JSON object参数；未声称移植Pi缺index按ID补救、ID重绑定、custom/reasoning等全部变体，原有id/name片段追加规则保留。fetch确认Pi仍d5b3de22、Harness仍22ddee3。

Model契约新增PG_MODEL_BATCH：2～8个pg_model_call的完整终态，含arguments/id/name借用切片，整体有效期至next/close；event用union保留原56字节与各旧字段偏移，pg_model表仍40字节。MiMo一调用仍输出原CALL，多调用由独立slot累积，按index排序；finish检查身份完整/无洞/唯一，transport完成且finish+DONE后，统一校验每个参数再发布一个BATCH，不先交付可用前缀。任何Model.next/解码错误锁定本请求，后续next继续失败，start才清除。单/多调用共用参数校验路径，无第二套实现。

pg_mimo原字段偏移保留，追加7个参数槽、8个切片view及count，由139480增至169440字节（+29960）；所有内部调用者同批重建，不再把旧“布局冻结”注释误作不可扩展的稳定ABI承诺。没有扩大每项4096/ID64、wire接收131072或API max_tokens512等既有范围，也未添加依赖或运行时框架。

实际验证：常规核心O0/O2 UBSan通过；独立MiMo驱动O0/O2及UBSan组合通过；ABI/page-guard与transport专项O2 ASan/UBSan通过。新增同分片多个调用、分片交错/index1先到、2/8调用、各槽最大4095参数/63字节身份、末槽参数/ID/name溢出不写view、整数范围/类型错误、缺索引/缺身份/重复ID/末项非object或不完整JSON/第9项/缺DONE全组拒绝、8个终态token分配失败点均不发布前缀，复用start清空额外状态。扩展的外部ABI破坏回调包含get_int64/数组访问/长度等；旧“index1立即非法”测试改为合法未完成分片后finish拒绝，并补新越界/不完整整组用例，未删除对应失败边界。

ASan首次查出旧test_start_setup在第二次start后未close就让下个用例清零provider，泄漏12640字节curl相关分配；修复夹具关闭并断言两次资源释放后复验通过，没有关闭LeakSanitizer或修改产品清理来掩盖。真实模型入口python3 -B tools/runtime/build_test.py --live-batch已通过：HTTP请求直接使用真实MiMo，收到同响应两个read调用并验证两个独立path；只是Model层验证，没有执行工具，也不是完整TUI工作流。凭据仅在子进程环境中，未写源码/日志。实际产品组合构建通过。

同一Agent按授权自审自批Model事件扩展、provider尾部内存、登记/测试驱动和相应测试契约调整；不是独立审核。G00、assembly-first和diff检查通过，生产准入不变。当前Agent仍不处理BATCH、历史请求投影仍按单调用配对；完整--live-workflow尚未通过，下一批接Loop的整组提交/顺序执行/取消/恢复及投影，不能把本次真实元数据验证替代执行验收。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`4c4d78acc0e0d7d5ef2502e8f04ce2cff41038f84dd917fb1cc36ede644bbe64`。


## 2026-10-25：批量工具历史请求投影

读取固定Pi d5b3de22的openai-completions.ts::convertMessages及其请求构造调用点、tool-choice和tool-result-images测试，Durable generation.ts::startToolRound。采用同轮完整tool_calls归属一条assistant、随后各工具结果的结构；原生当前无图像与reasoning投影，只接受连续1～8调用及同序完整结果。上游允许的其他上下文变体未声称兼容。

原地扩展手写mimo_projection.S；先验证整组调用身份、唯一ID、参数长度和结果关联，再建一个tool_calls数组。空assistant仍跳过或与下一组融合。结果按组内位置关联，不再误认“前一条必须为call”；孤儿、漏结果、错配、重复ID、第9调用和无终止身份字段整组拒绝。栈仍128字节，使用原未用两槽保存组边界，不增加持久状态、依赖或ABI。

专项O0/O2及各UBSan组合通过；projection与逐分配失败专项O2 ASan/UBSan通过。新增2～8调用、后续第二轮、各项ID投影和拒绝边界；8调用逐一覆盖207处分配/插入/序列化失败，无遗留JSON所有权；单调用75点保持通过。完整核心O0/O2 UBSan构建运行退出0。本批没有调用真实模型；Agent消费BATCH、顺序执行/取消/恢复及TUI历史仍未接通，完整工作流仍未验收。

同一Agent按授权自审自批模块范围与清单说明更新，没有修改检查器、预算或生产准入。G00、assembly-first和diff检查通过。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`004a95b298e9ab7bec88c7eac479a1fbb01ffc541c755b7769dbc49a2b0597f4`。


## 2026-10-25：批量Agent顺序执行与完整真实工作流

继续读取Pi d5b3de22 Durable generation.ts::startToolRound（完整assistant同事务、sequential pending后缀）及对应工具轮测试；请求投影参考沿用上一节。手写Agent支持单CALL与2～8 BATCH共用意图/执行/结果路径；借用调用全部复制到现有草稿arena后才提交，关闭Model后不依赖原切片。预留assistant、calls及所有results容量，不截断后缀。完整意图先存，逐项结果与下一项safe/unsafe检查点一并提交，最后才进入Model请求。

取消先有abort意图，活动工具按既有异步协议释放，再将当前与未开始后缀结果同事务终结；逐项发布结束事实，不为未执行项伪造开始。单独记录本地取消结果是否已提交，防止结果已落盘但结束事件未发布时的取消误索引。恢复从检查点扫描组末尾，已完成结果不重做；当前项仅stored/current均safe才重跑，unsafe返回interrupted，之后继续后缀；持久abort恢复补齐错误结果。TUI历史从连续结果回到对应调用组取名称，不再要求前条必为call。

调用者Agent由298336增至328088字节（+29752）：原draft/pending改为同址9-message union，另加组末尾和本地取消发布标记，不保留第二份调用真相；commits及后续内部偏移统一更新，ABI静态断言和保护页用例同步。Model/event公共表布局、Session消息/日志格式和资源预算不变。工具保持顺序，不移植通用Task图或默认并行。

验证：2～8项正常执行与每项开始前、结果提交后/事件前、结束事件后取消；Model.close清空借用view后仍可执行；尾项坏参数/身份、重复ID与非法批次数全组不执行。8个活动位置的fresh Agent重建，safe/unsafe及已存abort分支通过；这是相同已提交Session/control恢复，跨进程/日志编码仍由既有存储专项覆盖，不能冒充所有Agent批量崩溃窗口都已测。另有8项异步工具、各项输出中取消、close先pending再释放，用例通过。专项Agent O2 ASan/UBSan通过。完整核心O0/O2 UBSan构建运行退出0，ABI/寄存器/栈对齐及两种保护页放置检查通过。

真实MiMo：--live-batch-execute通过，第一次工具开始时断言已提交两个calls，两次read实际完成后继续生成终态；没有替换Model或要求拆成单调用。--live-workflow通过：实际TUI read/edit/write/bash，构建并运行手写汇编测试，校验精确源文件/脚本及未修改内容。离线PTY退出、信号、resize、termios及持久闲置重开同时通过。凭据仅子进程环境，未入库。之前单调用解析阻断的完整工作流现有直接通过证据；不据此宣称所有Pi功能、完整T26/T28或生产发布通过。

同一Agent按授权自审自批Agent内部arena/偏移、模块范围、测试和真实入口构建扩展；无新文件/依赖/门禁，也未放宽失败边界。G00、assembly-first和diff检查通过，生产准入不变。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`34991c069cf492c9c61affbe9b511edaaee574d2d6ee354e3d2c2bfaa1c1ac3a`。


## 2026-10-25：批量工具真实日志与进程恢复补验

对照固定Pi d5b3de22 Durable generation.ts::startToolRound及harness-tools.test.ts的sequential trace和abort结果用例；盘古继续采用有界顺序运行与已提交结果边界，不声称移植上游全部持久工具输出。fetch确认Pi仍d5b3de22、Harness仍22ddee3；无参考源码变更。

扩展现有test_agent_resume.c，在真实journal与fork进程中验证2/8项、每个活动位置、stored/current safe两种策略的200个组合：意图提交后执行前退出、外部文件副作用后结果提交前退出、结果提交后事件前退出、abort意图提交后退出，以及取消结果整组帧写入但尚未确认/发布时退出。重开核对全部结果ID/错误状态、逐项副作用实际次数、最终checkpoint和再次idle重开无提交/执行。safe允许重试当前不确定项，unsafe保守提交interrupted（包括无法证明已开始的当前项），已完成项不重复；没有把副作用exactly-once或机器断电作为保证。

新增现有PTY脚本的--live-batch入口，实际MiMo/TUI发出同响应两个read并执行；检查真实v5帧确为assistant+两个calls、结果的两个文件标记/ID，重开显示两项call/result名称与ID、无run_started，日志字节不变。实际运行退出0，且凭据未进入journal。专项进程恢复O2 ASan/UBSan退出0；完整核心O0/O2 UBSan退出0；离线实际PTY退出/信号/resize/termios/持久idle同时通过。本批未修改产品实现，不把这些测试覆盖外推为所有崩溃窗口或生产准入完成。

整理近期队列：已实现的有界Session/Storage/Submission接口不再挂在未完成的通用Task框架下；下一项是现有read的大文件分页缺口，参考Durable offset/limit，在当前消息预算内实现。无需新增框架或门禁。同一Agent自审该测试入口与进度说明，非独立审核。G00与diff检查通过。固定包SHA256（不含本记录；排序[path,文件SHA256]紧凑UTF-8 JSON）：`a9f2ca5c8d55ca9ed6155ea66577b7aaa29bb5f39eb25d6ee391655f9784af6f`。
