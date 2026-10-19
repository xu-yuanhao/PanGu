# T06 工具链比较与实证

状态：IN_PROGRESS。当前本地技术方案与隔离实验已形成；T05 前置、独立控制面审批和生产策略提升未完成，T06 不整体划线。

机器可读证据：[T05/T06 候选回执](t05-t06-evidence.json)。

## 首版候选决定

x86_64/Linux 首版候选采用 Clang 23.1.3 前端 + GNU as/ld/nm 2.47 + LLVM objdump 23.1.3 + GDB 18.1。C 为 C17；手写汇编只开放 `.S`、GAS 节/符号约定与 `.intel_syntax noprefix`。C 的受控生成汇编不是另一个可手写语言入口。C++ 当前没有已批准模块，不默认开放运行库或桥接；有具体桥接需求时再登记和验证。

| 比较项 | fasm 本地参考 | Clang + GNU as 候选 |
|---|---|---|
| 来源 | `references/fasm`，提交 `225ce90e81e69877ab42505f7713e3d3fd531d42`，版本 1.73.35 | 本机工具二进制/规范路径/完整版本已固定 |
| ELF64/外部符号 | FASM.TXT 2.4.4 明确 ELF64、public/extrn、PLT | 实测 C/Assembly 目标互相解析并生成静态 ELF |
| 真实输入 | 未实现与现有门禁的输入采集 | 合并 Clang 预处理/编译依赖与 GNU as --MD，覆盖 .include/.incbin |
| 调试 | TOOLS/FAS.TXT 描述专用符号格式；GDB/DWARF 链未实测，不能据此断言不支持 | 已实测 C/Assembly 行号、断点及 answer→probe_start 两帧回溯 |
| PIC/DSO | 手册有重定位相关能力，本轮无可执行程序且未自举构建，未实测 | 实际 PIC 对象共享链接通过；不是插件加载/卸载验证 |
| 选择依据 | 保留替代候选，无性能优劣结论 | 当前输入、调试、链接和隔离证据更完整，选为首版候选 |

最初试用 Clang 集成汇编器时发现常规 `-MD` 不记录 `.include`/`.incbin`，而其当前驱动不接受尝试的 assembler dependency-file 参数。因此改为显式 GNU as，不以禁止测试或只信源文本搜索掩盖盲区。Clang 输出 C 汇编使用 `-fno-integrated-as -fno-addrsig`，避免 `.prefalign`/`.addrsig` 等 GNU as 不支持的指令。两个工具的实际命令、原始依赖、生成汇编摘要和对象摘要均进入构建回执。

本轮没有比较汇编器速度，也不推断机器码性能。工具及其运行环境是开发依赖，不进入 Agent 产品依赖。参考实现只读，未复制到产品。

## debug/release 与实际产物

`governance/native-probe-toolchain.json` 固定六个工具和 C/Assembly/assembler/final-link 参数。C 使用 freestanding、禁用隐式 builtin 和 stack protector、PIC、隐藏可见性；debug 使用 `-O0 -g`，release 为 `-Oz`。`.S` 预处理后由 GNU as --64 组装，debug 开 DWARF5；所有汇编/链接告警致失败。路径映射减少临时目录与源码目录对调试输出的影响。当前最终样例为静态 ET_EXEC；生产 PIE/装载策略仍须独立审查，不能由样例替代安全基线。

固定样例入口为平台汇编，通过 SysV 调用 C 函数，用退出码返回 7；汇编根帧具有明确 CFI。它只验证构建/调试链，不是产品引导或 AgentLoop。每次保留对象、ELF、链接 map、反汇编、build.json；对 debug/release 各构建并执行两次，同模式 ELF 摘要必须一致。

`native_build.py` 校验 ELF64/x86_64、实际入口、无动态解释器/动态段、不可执行栈和无 RWX 段。对象系统调用指令限于 platform/host；有限反汇编规则不证明任意机器码安全隔离。产物复验要求外部回执摘要，重算文件全集/源码/配置/对象/ELF/map/反汇编，并用保留对象重新做符号与链接验证。

最新独立实验：`/home/xu-yuanhao/Documents/PanGu-artifacts/toolchain/native-gas-3gfpabe5/experiment/experiment.json`。固定四次退出码、同模式 ELF 一致性、C/Assembly 行号、断点、两帧回溯与 PIC 共享链接均通过。早期 `native-kqzvyrx3` 是集成汇编阶段的历史证据，不能替代最新 GNU as 链的结果。

## 冻结开发工具与隔离复现

`toolchain_image.py` 从本机固定工具构造外部只读镜像，使用 ELF loader 的 --list 采集实际动态装载闭包，同时冻结 Python 标准库、Guile、GDB 与 gconv 运行数据。不运行下载或安装脚本，明确不包含 site-packages 和 Tk GUI。缺失必需动态库或运行资源时拒绝，不能静默跳过。

镜像 1816 个文件、464103488 字节；此体积只属于开发工具环境，不能算作 Agent 发布物，也不能把小型构建样例当作 Agent 体积证明。最新镜像：`PanGu-artifacts/toolchain/frozen-gas-9sf3ohft/image/image.json`，摘要 `9765bb42bfca5bce3787c83801b9201563d59f758b61a327bfb644c3737c1dc4`。

bubblewrap 运行时断网，宿主用户目录不挂载，工具/控制器/候选只读，输出独立。镜像、源码树和策略摘要在运行前后验证。最新隔离回执 `PanGu-artifacts/toolchain/isolated-gas-091mx9lz/output/isolation.json`：十四套、146 项门禁自测全部通过，含真实编译、运行、调试、产物篡改与汇编输入反例。宿主内核和 namespace 启动器仍属于宿主信任，不声称跨内核/跨机器完全复现，也未建立 GitHub 检查发布权限分离。

镜像构建曾发现：只复制共享库会遗漏 gconv 与 GDB Guile 模块，导致隔离 GDB 初始化崩溃/警告。已通过只读 A/B 挂载定位并冻结资源，保留失败日志；没有修改机器配置或降低诊断要求。

## 本轮复验（2026-10-20）

在冻结镜像上重新验证（`toolchain_image.py verify` 回读通过，image.json 摘要不变）：

| 检查 | 结果 |
|---|---|
| Clang `-MD` 对 C 输入记录头文件 | 通过：`main.s: main.c inc.h` |
| GNU as `--MD` 记录 `.include` | 通过：`nested_include.o: nested.inc nested_include.s` |
| GNU as `--MD` 记录 `.incbin` | 通过：`nested_incbin.o: blob.bin nested_incbin.s` |
| Clang 拒绝 GNU as 的 `--MD` | 通过（`unknown argument`），确认必须两阶段 |
| 门禁相关单测 | `test_compile_inputs` 13 项、`test_native_build` 23 项、`test_toolchain_probe` 3 项全部通过 |

工具版本：Clang 23.1.3、GNU as/ld/nm 2.47、LLVM objdump 23.1.3、GDB 18.1、NM 2.47。工具链配置的规范 JSON 摘要为 `0a2715105c19e86fa02bce20f5beb364d23b2ee39a27b55908a4aad4404ec1aa`。

复验细节：两个汇编夹具首轮缺少 `.intel_syntax noprefix`，GNU as 按 AT&T 解析 `mov` 失败；这是夹具错误，已修正，不是工具链缺陷。原始记录在 `/home/xu-yuanhao/Documents/PanGu-artifacts/h03-toolchain-1792427856/reproduce.json`。

复用而非重跑：debug/release 重复构建、C/Assembly 行号、双帧回溯、PIC 共享链接仍引用 `toolchain/native-gas-3gfpabe5/experiment/experiment.json`；146 项与 179 项隔离自测引用各自旧回执，均保留原输入绑定。

仍未证明：跨机器/跨平台复现、DSO 加载卸载、产品 ABI、性能与体积。T06 保持 `IN_PROGRESS`；T05 前置未闭合前不提升为生产工具链。

## 重现与剩余条件

各入口 `--help` 提供完整参数。核心命令为：

```sh
python3 -B tools/gates/toolchain_probe.py --toolchain governance/native-probe-toolchain.json --toolchain-sha256 <规范JSON摘要> --output <不存在的外部目录>
python3 -B tools/gates/toolchain_image.py verify --image <镜像目录> --image-sha256 <image.json摘要>
```

构建、产物复验、隔离实验采集成功仍为 2 / BLOCKED；错误返回 1。这些记录没有生产批准或上线签名。

剩余：完成 T05 的生产源码准入、独立控制面审批及受保护状态发布链；在此后才提升本候选为生产工具链。跨机器复现和其他平台另行认证；T08/T13 验证完整 ABI、静态注册/DSO 导出与生命周期，不把 PIC 共享链接成功等同插件宿主完成。
