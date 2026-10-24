# MiMo 首次真实模型验证

2026-10-24：通过 `src/runtime/mimo.c` 的可替换 C Provider 和现有 C Agent Loop完成真实模型工具往返。没有用Python或外部curl进程替代产品实现；Python仅用于构建/启动测试与注入环境凭据。

## 实际结果

首次以 mimo-v2.5-pro 请求返回 HTTP400，未执行工具；没有保存服务端响应或判定具体错误原因。随后以 mimo-v2.6-pro 请求成功，O0与最终O2各完成一轮完整真实工具往返（每轮两次模型请求）：

```text
tool_started
tool_finished
text: PANGU_TOOL_OK_7429
terminal=0 http=200 tool_executions=1
exit=0
```

测试提示要求调用echo一次并返回工具结果；echo只返回固定无副作用标记。真实模型发出调用，Runtime通过授权回调匹配echo后执行、将带call_id的工具结果加入上下文，再次请求模型并收到正确标记。这不是假模型回放。测试代码目前以终态成功判断退出码；工具次数与标记由本轮输出复核，尚未加入自动化断言。

## 运行入口与实现范围

凭据从 `MIMO_API_KEY` 环境变量读取，可用 `MIMO_MODEL` 覆盖默认mimo-v2.6-pro。运行：

```sh
python3 -B tools/runtime/build_test.py --live
```

请通过本地安全凭据方式提供环境变量，不把密钥放入命令参数、仓库、报告或截图。测试未将密钥写入仓库文件。固定目标为官方Token Plan中国集群HTTPS地址，不接受任意URL，不跟随重定向，使用系统证书验证。接口参考：[官方Token Plan接入](https://mimo.mi.com/docs/en-US/tokenplan/Token%20Plan/quick-access)。

初始版本是完整响应适配；本轮已改为SSE增量适配，文本delta到达后逐段交给Runtime，工具参数分片累计后才提交调用。libcurl multi每次poll推进请求，取消移除请求并释放资源；连接超时15秒、请求超时60秒、响应小于128KiB、文本/参数/ID沿用Runtime上限。DNS/平台后端是否全程非阻塞尚未专项验证。不支持多个工具调用，明确失败；不把length等截断终止原因判为正常完成。thinking关闭，尚不覆盖thinking内容与其回传协议。

当前工具声明固定为测试echo，不是完整的动态工具注册/Schema适配，不可接真实Shell工具；工具实现在独立测试入口。JSON错误、HTTP错误、传输失败当前映射运行错误，客户端详细诊断待补。json-c/libcurl分配失败、完整协议畸形响应与取消竞态尚未系统验证。真实模型首片段后取消现已验证；终端恢复和TUI集成未完成。

## 依赖、自审与下一步

本轮引入已安装的libcurl8.23.0与json-c0.19作为可替换hosted Provider的局部依赖，许可证及影响登记在 `governance/runtime-local-modules.json`；C Loop本身不依赖它们。平台打包、传递依赖固定、体积预算和生产依赖批准未完成，不能据此声称最小freestanding发布物或完整T27通过。后续可用已有原生JSON服务替换json-c，不能把局部适配变成隐式宿主依赖。

同一Agent按用户授权自审自批本轮局部源码/构建/依赖登记，不是独立审查；未修改检查算法、放宽测试或基线。原runtime-core-progress固定包摘要属于上一版本，以下摘要记录本轮最终候选。直接验证：原有O0/O2 UBSan回归通过，真实O0/O2工具往返通过；最终库存检查及格式检查结果见本轮完成报告。下一步继续完善工具/会话语义并与TUI桥接，不扩展无关门禁。

```text
83859a80da080b47e9c4e15e9dbb3840151f55fd40c2fcbd212af6e2105a4627  src/runtime/mimo.c
5fdf7db2c786c1cb16ff04a34013a0e9e4607382c4ef650fc2bb39b413cb25eb  src/runtime/mimo.h
ab1d976238fcd208a179aea9ecb90d4aafd6e9d8f9dbd7dbddd1f042ab8de08d  tests/runtime/live_mimo.c
bee056e10b80536df63bfe637885b589879763ab144ad7f423c436e5721f8efb  tools/runtime/build_test.py
a1e767c26ea742060e505c24dcc84e2f0370d63555e02af4dead2eb46e9b1d9b  governance/runtime-local-modules.json
6494baf24b64d4d8fa0bbd9447abeb0b7c788047402c7b55116cca4cbeab7678  governance/bootstrap-policy.json
```

## SSE增量与取消（本轮更新）

按Pi `packages/ai/src/api/openai-completions.ts` 的流消费、content delta、function.arguments累积和finish_reason校验实现。没有复制SDK或引入Node。客户端 `src/contracts/client.h` 未改变，另一Agent可继续按原约定集成。

真实MiMo O2工具往返结果为3个文本片段 `P`、`ANGU_TOOL_OK_742`、`9`，HTTP200、工具执行1次、成功终态；本轮测试入口增加自动验证会话累计文本包含完整标记、工具次数恰好1。另一次请求在首个文本片段 `I` 到达后取消，HTTP200、cancelled终态、工具执行0次；退出码0还要求easy/multi句柄均释放。没有以假模型替代这两次网络验证。

无网络解析测试与Runtime回归在O0/O2 UBSan模式运行，覆盖文本/UTF-8、工具参数分片、无finish_reason就DONE拒绝、重复DONE、非法JSON/尾随垃圾、length截断、第二工具index拒绝与参数容量。Runtime增加16轮终止、消息容量和超长输入检查，工具ID拒绝控制字符。原测试驱动100步只覆盖短流程，新增16轮用例将驱动上限设为256步；未调整产品16轮上限或门禁时限。

边界：只支持本Provider采用的逐行单JSON `data:`事件（含CRLF/注释/空行），不宣称实现完整通用SSE多行事件规范。响应总字节上限128KiB，逐步消费时不回收已用字节；超限失败。工具须等待明确tool_calls终止、DONE和传输成功后提交，参数须是完整JSON对象；思考模式仍关闭，尚未增加usage或多个工具。SSE解析器自测针对完整data行和工具JSON分片，不宣称穷举全部TCP切分。系统JSON库分配失败处理和完整生产依赖审核仍待补。

同一Agent自审批准此次局部流式实现与测试登记；没有修改门禁算法/预算，尚未授予整阶段或生产认证。下一版固定摘要替代先前版本摘要，仅绑定下列文件当前内容。

补充：取消已宣布但未执行的工具现在补入带call_id的错误结果，发tool_finished(cancelled)后发唯一run_finished(cancelled)，避免后续模型请求出现悬空工具调用；对应回归确认执行次数0。原runtime-core-progress中的旧取消限制由此替代。为工具调用预留结果槽位，容量不足时拒绝提交调用记录。

最终全仓G00在并行开发时返回失败，原因是另一Agent正在创建尚未登记的src/client/tui、tests/client/tui、tools/client/tui目录与文件；本Agent没有修改对方文件或扩大登记范围。15项G00检查器自测通过，Runtime/解析测试以本轮命令实际输出为准。TUI交接后统一登记，不能把当前库存检查描述为通过。

```text
47a55c80b5a5dc71c25c901d8c3fd413a91c3eab8fb81250fa55c71f033996c5  src/runtime/agent.c
6bdd113ef00ce66ba1e039c50f2c3de1f29621bb86fad70ce4847fe71dfc6f63  src/runtime/mimo.c
186517c7410910736143b821d517ece19b4c4ae0fe0065721f3b0faead6a2066  src/runtime/mimo.h
3077324eb8a2366f19f47901c330435e072fe64c39cbd35c9b65332ecdb9c972  tests/runtime/test_agent.c
f76d8203666e672f1c4ccdbcdca2061d30d2f11d6f5c82a5a073222b0a9416c0  tests/runtime/test_mimo_stream.c
437ee50e135dfcbe5f48745c0b838b35a610351eb24f865546f100258bf57488  tests/runtime/live_mimo.c
6aef63b20c2e35cf4772ff6e3943b90ae7b26dbb1deaff431061d066e29fa92e  tools/runtime/build_test.py
734fe1015a2b8375ed36a2c7494dbba2a3de8b17449d5e67db4b4fec04aa797d  governance/runtime-local-modules.json
7fb8de72dce314c28230f4af05eb31c2ad19629398d10f7c279c7b1cfd9dbc38  governance/bootstrap-policy.json
```
