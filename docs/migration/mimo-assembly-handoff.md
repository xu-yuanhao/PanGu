# MiMo 协议适配汇编重构交接（2026-10-25，第二 Agent）

替换 `src/runtime/mimo.c` 的原地手写汇编实现已交付并实际验证；本报告为唯一交接物，未提交、未改共享文件。工作树中主 Agent 的 profile/Session/journal/bridge 在途改动未触碰。

## 任务清单

- [x] 工作树/文件归属检查；固定参考源码阅读，采用语义与差异见下。
- [x] `src/runtime/mimo_layout.h`：本模块实际消费的 pg_mimo/pg_session/pg_message/pg_model_event/pg_model/pg_tool_* 字段与枚举唯一数值，C 侧 `_Static_assert` 对齐公开头与本机 libcurl 8.23.0/json-c 头（含 CURLOPT/CURLINFO/CURLM/CURLMsg 常量与 `struct CURLMsg` 偏移），漂移即编译失败；不手抄私有结构。
- [x] 请求投影（mimo_projection.S）：system/user/assistant/tool_call/tool_result、assistant 文本+call 合并、call/result 按 call_id 严格配对、完整 registry 工具定义（schema 经 json_tokener_parse 嵌入，与旧 C 相同）。未结束草稿（末尾 tool_call 无 result）拒绝；原配额（PG_MESSAGE_LIMIT/PG_TEXT_LIMIT/PG_ID_LIMIT/system_size）与非法历史拒绝不变；request 归实例所有，失败路径释放已建 json-c 对象树不泄漏。
- [x] SSE 单行解码与片段累积（mimo_stream.S）：WAIT/TEXT/CALL/DONE 语义、空行/`:` 注释、`data:` 前缀、`[DONE]` 次序（须先有 finish_reason、仅一次、之后数据拒绝）、JSON 完整性（parse_end 后仅空白）、单 choice、tool_calls 单项 index==0(int)、id/name/arguments 增量追加（null 缺席）、text 整串替换、嵌入 NUL/容量越界/非法类型/未知 finish 理由/重复 finish 全部按原协议拒绝。
- [x] 生命周期（mimo.S）：receive 乘法溢出与 body 容量检查（含末位 NUL 保留）、跨接收块换行、未完成末行（运行中 WAIT/传输结束拒绝）、HTTP 非 200、perform/info_read 失败、正常流结束、工具参数终态完整 object 校验；cancel/close 释放 easy/multi/headers/request，半初始化可安全关闭，重复 close 幂等。start 语义（MIMO_API_KEY 校验 ≤400 无 CR/LF、MIMO_MODEL 缺省 mimo-v2.6-pro、固定端点、max_tokens 512、thinking disabled、60/15s 超时、NOSIGNAL、https 协议白名单）不变。
- [x] 工具 CALL 仅在 finish_reason=tool_calls + [DONE] + transport 终结 + 参数 object 校验全部通过后作为终态事件交给 Loop；transport 错误返回 1，不伪造 DONE。事件 text/call_id/tool_name 借用 pg_mimo 内缓冲，有效期至下一次 decode/start/close（各源文件头注明）。
- [x] 删除 `src/runtime/mimo.c`；独占驱动 `tools/runtime/test_mimo_asm.py` 仅用三份实际汇编源构建；补 ABI/边界/传输故障测试；无 C 胶水（未新增 mimo_curl.c/.h：CURLMsg 等公开结构按头文件布局常量直接消费并断言，无需 C 适配层）。
- [x] 专项+原协议/投影测试+对象导入导出审计通过；真实网络项 NOT_RUN 见下。本报告后停止写文件。

## 参考阅读（提交/文件/函数 → 采用语义 → 原生差异）

- Pi `38ff99177fba21e9c57a670f4e95adcf407c99c7` `packages/durable/src/harness/generation.ts`（streamResponse/classify/startToolRound/abort）及 harness-generation 测试：宿主拥有 partial 提交、重试、compaction、工具轮与终止分类；provider 只投影请求、产出流事件与终态。采用该边界：pg_mimo 不持 Session 提交/恢复/调度。差异：原生单轮单调用、固定容量，无 deferred 轮询、无自动重试、无 usage 记账。
- Pi `packages/ai/src/api/openai-completions.ts`（stream 主循环、buildParams、convertMessages、convertTools、mapStopReason）+ `transform-messages.ts`：messages 投影形状（system 前置、assistant content 为纯字符串、tool_calls=[{id,type:"function",function:{name,arguments}}]、tool 结果带 tool_call_id）、空 assistant（无内容无调用）跳过、仅 choices[0]、finish_reason 映射（stop→stop、tool_calls→toolUse）、参数增量累积不逐段重解析、流无 finish_reason 即错误。差异：上游容忍 length/多工具/图片/thinking，孤儿 call 合成结果；原生按既有有界协议拒绝这些输入，未放宽（见提示词约束）。
- Pi `packages/ai/test/openai-completions-provider-stream-event.test.ts`、`streaming-tool-arguments.test.ts`、`openai-completions-tool-choice.test.ts`、`src/utils/event-stream.ts`：chunk 形状（首片段 id+name、后续仅 arguments 增量、finish 带 usage、`data: [DONE]` 收尾）、终态参数须精确、done/error 互斥且 done 后事件不再投递。原生对应：终态后再收数据=拒绝（更严，保持）。tool_choice/strict 字段原生不带，维持现状不新增。
- Harness `22ddee3792402c3e70ec5f86a63c3e3e895a2f60` `packages/llm/llm/src/index.ts`（LlmRuntime/registerAdapter/replace + ctx.effect disposer）与 bundle 挂载：模型适配器是可替换服务注册，随宿主生命周期释放。采用：pg_mimo 继续作为可替换 `pg_model` 实现，close 释放每次请求传输资源；不引入其配置/路由框架。
- 原生 `mimo.c/.h`、`agent.h`、`session.h`、`tools.h` 与两个既有协议测试：行为基线逐路径对齐（见验证）。旧 C 的确定缺陷按最小修复：json-c 容器分配失败现在释放已建对象并返回 1（旧 C 解引用 NULL 崩溃）；叶子字符串 OOM 行为与旧 C 一致，未新增崩溃面。auth 头由 snprintf 改为定长 memcpy 拼接（前缀 22B + key ≤400 + NUL < 512），输出逐字节相同。
- 本机公开头核对：curl 8.23.0（CURLOPT 数值按 CURLOPTTYPE_* 展开、CURLINFO_RESPONSE_CODE=0x200002、CURLM_OK=0、CURLMSG_DONE=1、CURLE_OK=0、CURLMsg{0,8,16}；注意该版本枚举类型名为 `CURLINFO` 大写）；json-c（json_type 0/3/4/5/6、tokener_success=0、PLAIN=0、json_bool=int、get_string_len 返回 int、array_length 返回 size_t）。curl_easy_setopt/curl_easy_getinfo 为变参：整数/指针变参在 rdx、调用前 al=0，全部调用点按此实现。

## 交付文件与符号

新增：`src/runtime/mimo.S`、`src/runtime/mimo_projection.S`、`src/runtime/mimo_stream.S`、`src/runtime/mimo_layout.h`、`tests/runtime/test_mimo_asm.c`、`tests/runtime/mimo_abi.S`、`tests/runtime/test_mimo_transport.c`、`tools/runtime/test_mimo_asm.py`、本报告。删除：`src/runtime/mimo.c`（回溯用 Git 历史）。修改共享文件：无。

- 公开导出（恰为冻结集合，nm -g 审计）：`pg_mimo_model`、`pg_mimo_bind_tools`（mimo.S）；`pg_mimo_encode_request`（mimo_projection.S）；`pg_mimo_decode_line`（mimo_stream.S）。内部 `li_*` 符号均为文件局部。
- 导入（nm 审计）：仅公开 json-c/libcurl/libc 函数与上述跨文件公开符号；无旧 C 算法依赖、无其他未定义符号。
- `pg_mimo` 公开布局逐字段冻结（mimo.h 未改动，mimo_layout.h 断言 139480 字节全部偏移）；pg_model 40 字节聚合经 sret 返回；事件/回调 ABI、工具 registry 布局不变。
- 本模块仅消费 pg_session 的 messages/count/system_prompt/system_size 与 pg_message 的 role/size/text/call_id/tool_name、pg_tool_registry 的 entries/count 与 definition 的 name/description/schema；这些字段在 mimo_layout.h 有断言，主 Agent 若改公开字段导致编译失败即如实报告，不覆盖头文件。

## 验证记录（实际命令与退出码）

- `python3 -B tools/runtime/test_mimo_asm.py` → rc=0，`ALL MIMO ASM RUNS PASSED`。4 套件 × {O0, O0+ubsan, O2, O2+ubsan} 全部 rc=0：原 `test_mimo_stream.c`/`test_projection.c` 未改动全过；`test_mimo_asm.c`+`mimo_abi.S`（18 个 libc/json-c 调用点经 `--wrap` 先转发真实实现再破坏 caller-saved 并在入口断言对齐，含 rsp≡8 检查；probe2/probe3 哨兵校验 callee-saved/DF/栈，corrupt 负例可检出；页尾输入与输出分别贴 PROT_NONE 保护页；投影/解码边界与容量、事件借用语义）；`test_mimo_transport.c`（脚本化 curl 替身：setopt 逐项捕获并转发真实 libcurl 且全部返回 CURLE_OK、perform/getinfo/info_read/add_handle 故障注入；行跨块拆分、CRLF、终态次序、HTTP 500、perform 错误、info_read 三种失败、截断末行、参数非法、缺 [DONE]、接收乘法溢出、容量 131070/131071/131072 精确边界、easy/multi 初始化失败的半初始化关闭、cancel、重复 close 释放计数恰好一次）。
- `python3 -B tools/runtime/build_test.py`（删除 mimo.c 前的共享基线）→ rc=0 全 PASS。
- `python3 -B tools/gates/check.py`（删除 mimo.c 后）→ rc=1：G00 FAIL 仅为本批 8 个未登记新文件与已删除登记项 `src/runtime/mimo.c`，无其他失败项；预期在途状态，待主 Agent 登记后复跑。
- 工具链：`/usr/bin/clang-23`，`-std=c11`，GAS Intel 语法 `.S` 经 clang 汇编，PIE/RIP 相对寻址，`.note.GNU-stack` 非可执行栈；无新增 CPU 指令要求。
- UBSan 仅覆盖 C 夹具，不检查汇编访存；汇编侧证据为 wrap 破坏/对齐断言/保护页/清理计数。
- 真实 MiMo：`MIMO_API_KEY` 环境未设置 → **NOT_RUN**（不搜密钥；协议确定性测试不冒充真实模型验收），主 Agent 集成时按现有 live 测试补。
- 过程修复（均已复验）：mimo_projection.S `li_field` 初始版本未把 value 装入 rdi（外部调用前寄存器装载错误，首轮测试即捕获修复）；测试侧修正 6 处自身期望错误（双 choice 误写、重复 finish 期望反了、无 id 的 tool_calls finish 应拒绝、分片 JSON 多余引号、[DONE]/空行消费次数、slist NULL 释放计数）与 pg_model_event/输入行同页尾相互覆盖的测试布置错误。

## 给主 Agent 的精确登记差异（不提供双版本选项）

1. `governance/bootstrap-policy.json`：删 `"src/runtime/mimo.c": "native-source"`；增 `"src/runtime/mimo.S"`、`"src/runtime/mimo_projection.S"`、`"src/runtime/mimo_stream.S"`（native-source），`"src/runtime/mimo_layout.h"`（native-source，同 tools_layout.h 先例），`"tests/runtime/test_mimo_asm.c"`、`"tests/runtime/mimo_abi.S"`（native-test，同 agent_abi.S 先例）、`"tests/runtime/test_mimo_transport.c"`（native-test），`"tools/runtime/test_mimo_asm.py"`（development-tool），`"docs/migration/mimo-assembly-handoff.md"`（document）。
2. `governance/assembly-first.json`：删 `"src/runtime/mimo.c"`（pending-inplace-assembly）条目；三份 `.S` 可按 tools.S/read_tool.S 条目形状登记为 assembly-core。
3. `governance/runtime-local-modules.json`：模块 `model.mimo.local.v0` 的 sources 由 `mimo.c/mimo.h` 改为 `mimo.S/mimo_projection.S/mimo_stream.S/mimo.h/mimo_layout.h`；exports 四项不变；dependencies 维持 libcurl/json-c 描述。
4. `tools/runtime/build_pangu.py` SOURCES：`src/runtime/mimo.c` → 三个 `.S`（implementation() 按 stem 解析不变）。
5. `tools/runtime/build_test.py`：两处 `implementation('src/runtime/mimo')` 链接自动跟随 SOURCES；如需把 test_mimo_asm/transport 纳入主测试矩阵自行决定（独占驱动已覆盖 O0/O2/UBSan）。
6. 链接输入无变化（libcurl/json-c 原有）。

## 剩余限制与待集成

- cancel 后再 next 的行为与旧 C 相同（close 不重置 delivered，宿主协议上 cancel 后不再 poll）；如控制面需要可在主 Agent 侧处理，未越界修改。
- next 每轮调用 getinfo（同旧 C 轮询形状）。
- 真实网络回归（live_mimo/PTY/恢复链路）与统一提交、共享登记、G00 复跑由主 Agent 集成时完成。
- 本批不声称完整 Pi provider 兼容、真实模型验收或生产放行。

## 主Agent集成复核（2026-10-25）

交接初稿不等于最终实现已通过。主Agent实际读取三份源码与固定参考调用，复跑原16次专项后发现并修复：工具参数游标跨json_tokener_get_error保存在caller-saved rcx；若干未挂入JSON树的容器失败路径泄漏；普通assistant分支把lookahead下标作为role，真实恢复后的追问初始化失败。另补完整finish_reason长度/嵌入NUL拒绝、原始NUL接收拒绝、header追加失败保留所有权，以及setopt/getinfo错误处理。

投影采用局部所有权位与统一失败清理，必需叶子/插入/序列化失败均拒绝；新增test_mimo_alloc.c覆盖75个失败点，最终ASan通过。构建测试已显式链接三份MiMo汇编源，单个implementation(mimo)不会自动补入另外两份源；原交接第5项的自动链接表述不准确。新版专项含5套件×4模式=20次执行，主测试含MiMo ABI/运输故障/分配所有权三组。新增回归与临时负例可检出游标缺陷、遗漏清理和普通对话角色错误。

当前已完成登记集成，mimo.c及pending-inplace-assembly登记删除，无C备份。真实MiMo PTY普通多轮/工具续答/取消/历史/持久恢复，以及三进程带请求ID的去重恢复均通过；实际命令、限制和固定摘要记录于runtime-core-progress.md。本节为同一主Agent的审核/修复与验证，不声称独立审核或完整生产/Pi兼容放行。
