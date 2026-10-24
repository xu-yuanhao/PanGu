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
