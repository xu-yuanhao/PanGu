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
