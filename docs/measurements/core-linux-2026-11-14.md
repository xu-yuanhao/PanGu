# Core 注入模型资源测量（2026-11-14）

Linux x86-64，GCC 16.2.1 / CMake Release；Pi采用 `d5b3de22d0e24a3e9c38a9410d3329b099717283`，Node v26.12.0 原生加载未修改 Agent、pi-ai 公共入口与 telemetry 源码。每组3次，表内为中位数。原始样本、平台、命令与 native SHA-256见 [JSON](core-linux-2026-11-14.json)。

| 场景 | 宿主 | 启动 ms | 工作 ms | 空闲 RSS KiB | 峰值 RSS KiB |
|---|---|---:|---:|---:|---:|
| small | pangu | 0.63 | 0.21 | 1996 | 2148 |
| small | pi | 293.27 | 9.21 | 111480 | 112096 |
| stream_1mib | pangu | 0.99 | 55.80 | 3024 | 6208 |
| stream_1mib | pi | 287.86 | 12.17 | 112368 | 114080 |
| many_sessions | pangu | 1.39 | 8.39 | 2040 | 4812 |
| many_sessions | pi | 282.70 | 37.60 | 110544 | 120964 |

small：1实例×1轮，16个256字节累积片段；stream_1mib：1实例×1轮，256个4096字节片段；many_sessions：128实例保留状态，每实例4轮、每轮16×256字节，顺序执行而非并发。每个样本校验最终消息数、内容字节总数及生命周期事件数；两宿主均在实例建好后等待同一启动输入，运行后保留状态供取样，再释放退出。

测量读取 Linux `/proc/PID/status` 的 VmRSS/VmHWM（exec后高水位）。未采用 wait4.ru_maxrss，因为实验确认该值会包括 exec 前 fork 父进程的高水位。工作耗时包括最终计数、排除清理；进程 CPU 包含启动和清理。等待200ms期间Native CPU ticks中位数均0，Pi为0～10ms，受内核时钟粒度与后台初始化影响，不能据此宣称零CPU。启动含导入及实例创建，非仅函数耗时。

**已发现回退**：1MiB累计流中Core工作约55.80ms，Pi约12.17ms，约4.6倍。`src/core/loop.c/receive`对每个累计快照调用append深拷贝、释放旧快照，`src/core/core.c/pangu_core_append_internal`每次重新分配整条消息；本场景累计复制约128MiB。Pi保留JS字符串/对象共享。下一步需优化原生快照存储与分配，并保持回调借用/保留、取消和OOM原子性；当前未修，不通过改输入、去掉事件或削弱所有权契约消除差异。

这里仅比较注入模型的Core宿主。没有网络、提供者SDK、持久存储、TUI或可选插件；不是完整Pi发行包、默认Agent或安装依赖体积测量。Native可执行文件345048字节也仅是该构建产物，不包含系统C运行时或作为分发体积对比。完整C06仍未验收。

复现：

```sh
python3 tools/integration/prepare_pi_diff.py --full-runtime
cmake -S . -B build/core-profile -DCMAKE_BUILD_TYPE=Release -DPANGU_BUILD_TUI=OFF -DPANGU_BUILD_AGENT=OFF -DPANGU_BUILD_SERVICES=OFF -DBUILD_TESTING=OFF -DPANGU_BUILD_PROFILES=ON
cmake --build build/core-profile --target pangu_core_profile --parallel 2
python3 tools/integration/profile_core.py
```
