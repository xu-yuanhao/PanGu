# 模型临时快照分配复用测量（2026-11-14）

沿用[首轮基线](core-linux-2026-11-14.md)的代码、输入、消息/字节/事件验收和Linux测量口径，本轮每场景5次，原始样本见[JSON](core-linux-2026-11-14-reuse.json)。不是同步配对的before/after，进程启动/JIT/系统状态可能变化；首轮与本轮Pi耗时也明显变化，不能把两轮差值全部归因于实现。

| 场景 | 宿主 | 工作 ms 中位数 | 工作范围 ms | 峰值 RSS KiB |
|---|---|---:|---:|---:|
| small | pangu | 0.22 | 0.18–1.11 | 2104 |
| small | pi | 3.69 | 3.42–4.61 | 108360 |
| stream_1mib | pangu | 10.12 | 8.05–10.53 | 7028 |
| stream_1mib | pi | 4.36 | 4.15–5.89 | 111548 |
| many_sessions | pangu | 3.07 | 2.92–4.55 | 4760 |
| many_sessions | pi | 26.80 | 24.59–30.41 | 120560 |

实现修改：模型请求的临时Core使用私有allocator，在仍存活的快照之外只保留一个已释放的大块；容量按倍数增长并复用，下次请求/任何失败退出前完全归还原分配器。输入及每次快照仍深拷贝，事件顺序与回调借用生命周期未改变。tool scratch不启用此缓存，独立工具调用转移给宿主的结果保持原allocator上下文，不延长私有pool生命周期。缓存不用于凭据。

首轮1MiB Core工作55.80ms，本轮10.12ms；本轮Pi4.36ms，Core仍约2.3倍，不宣布性能闭合。Native峰值从6208KiB到7028KiB（约增加820KiB）；空间复用以暂时保留与容量取整换取较少分配。仍对每个累计快照复制全部正文，累计字节量随分片数增长，完整链路仍有二次成本。

永久回归：`tests/core/loop.c/stream_allocation_regression`通过真实公开request/sink提交128个累计片段到1MiB，要求宿主大块alloc次数少于40（不再每片分配大块），provider覆写输入后Core视图保持独立，最终消息正确且释放后live=0。原分配失败逐点扫描、取消/重入、工具结果转移和hooks保留用例继续通过。Core含来源差分14/14、Core ASan12/12、集成ASan/UBSan45/45通过；系统依赖不重新插桩。早期测试缺少state.h的编译错误已修，报告采用修复后重新编译的测试结果。

复现命令同基线，最后运行：

```sh
python3 tools/integration/profile_core.py --repeats 5 --output build/core-profile/reuse-final.json
```

这只处理Core临时模型快照的分配热点。完整Pi/默认Agent/TUI、依赖安装体积、多平台、并发与剩余性能差异仍待C06验收。
