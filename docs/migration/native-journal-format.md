# 盘古原生事务日志 v1/v2（本地开发）

实现：`src/runtime/journal.S`。这是盘古自己的格式，不是 Pi Durable JSONL，也不导入/覆盖 Pi 数据。参考 Durable 的全批次提交、提交前序列化、失败隔离和尾部恢复语义；选用显式二进制帧，使汇编可直接处理长度与字段，不增加JSON编码依赖，不保存C结构布局或指针。版本尚未稳定，读取v1/v2；未知版本拒绝，不改写已有v1帧。检查点使用v2记录，旧消息/system/reset仍可用v1。

## 文件与所有权

调用者提供可信目录fd和单个basename。拒绝空名、`.`、`..`、含`/`的名字及超过255字节的名字；最终文件使用NOFOLLOW/CLOEXEC/NONBLOCK，创建模式0600，只接受普通文件。父目录fd复制归提供者所有，原fd仍归调用者。

一个打开实例持有独占非阻塞flock到close；锁是协作式单写者协议，不是阻止任意本地机器码的安全沙箱。读写使用显式偏移，不依赖共享文件游标。v1帧上限仍为8544字节、实际最大8530；v2增加固定40字节检查点，编码缓冲8592字节、实际最大8570；文件上限16MiB，超限拒绝新提交，不自动压缩、删历史或改预算。

## 帧

所有整数为无符号little-endian，长度以字节计。每个帧代表一个完整Session变更。

| 偏移 | 长度 | 含义 |
|---|---:|---|
| 0 | 8 | magic：`PGJRNL1`后跟NUL |
| 8 | 4 | 格式版本1或2 |
| 12 | 4 | 包含头部与尾部的总长度 |
| 16 | 8 | 提交序号，从1开始、逐次加1 |
| 24 | 4 | 操作：0 append、1 system、2 reset、3 checkpoint（仅v2） |
| 28 | 4 | append消息数1～2；其他为0 |
| 32 | 8 | 前32字节的FNV-1a 64校验值 |
| 40 | 可变 | 消息payload；v2其后追加40字节检查点 |
| 总长−16 | 8 | 头部及payload（含检查点）的FNV-1a 64校验值 |
| 总长−8 | 8 | 尾标记ASCII `PCOMMIT1` |

FNV-1a 64初值`cbf29ce484222325`，每字节先xor、再乘`100000001b3`，溢出模2^64。校验用于意外损坏检测，不提供加密或防篡改保证。单独的头校验用于在相信帧长度并修复尾部之前识别头损坏。

append的每条消息依次为：role u32、is_error u32（0或1）、text长度u32、call_id长度u16、tool_name长度u16；随后依次为text/id/name原始字节，无NUL终止符、无对齐padding。长度分别最大4095/63/63。恢复时在有界原生结构中补NUL，再经Session校验采纳。system的payload就是原始文本字节（0～4095）；reset无payload。v2仅用于append或checkpoint，后者无消息但有检查点；不能用v2 system/reset绕过活动运行的上下文冻结。调用者必须通过Session提交已校验的批次，Storage回调不是不可信RPC解析入口。

## 确认与恢复

先完整编码再开始IO，全部pwrite完成并fsync成功才返回OK和新序号，随后Session采纳并发布。首次open在返回前同步文件和目录，覆盖新目录项的持久化。预备失败/容量拒绝为REJECTED，不写入；IO开始后的失败保守返回UNCERTAIN，并停止该实例后续写入。即使完整帧已写入但fsync报错，也不能向调用者报告成功或安全回滚。

open只接受新鲜空Session/control，顺序回放连续序号。成功后才绑定该Storage与已恢复序号；失败关闭所有fd、清空临时回放的view、保持调用者control未变。已经打开或含历史的对象不能原地覆盖。

最后不足完整头，或头校验有效但记录尾不完整时，仅截断到上一完整提交并刷盘。完整帧的头/内容校验错误、尾标记错误、非法字段、序号跳跃、未知版本均拒绝，不能当作“可修复尾部”静默删除。IO读取或尾部截断失败也拒绝。

完整帧可能在“已写入、尚未确认给调用者”时随进程退出而留下。重开会接受有效帧，不能假定调用者没收到成功就等于没提交；后续Submission/requestId与任务状态负责消除重复提交和处理在途工作。

## 当前范围与验证

已实现Session消息/system/reset及单运行检查点的文件存储和重开；不含通用Task图/Submission队列、requestId、模型流partial、压缩、自动文件格式迁移或外部副作用结算。交互入口仍暂不启用文件Storage，须先固定/校验原模型、工具与工作目录身份，并完成客户端的恢复活动状态接入。

`tests/runtime/test_journal.c`验证跨进程重开、工具双消息批次、reset、所有最终帧截断位置、完整文件逐字节损坏、带正确校验的非法字段、最大8530字节记录、独占锁、短读/短写/EINTR、写入/刷盘/截断/读取失败、半写后进程退出、路径/文件类型/配额边界。IO测试包装同时检查外部调用入口栈对齐、主动破坏caller-saved；五参数ABI探针检查open/Session提交/close的callee-saved、DF与栈哨兵。测试只在临时目录操作，不修改用户会话。主入口：`python3 -B tools/runtime/build_test.py`。

真实已结束会话的跨进程验证：`python3 -B tools/runtime/build_test.py --live-journal`，需环境中的MIMO_API_KEY。驱动在临时目录分别启动store/recall两个进程，首次写入结束会话，第二次新进程回放后向真实模型提问；不把它称为在途任务续跑。

## v2单运行检查点

检查点位于帧checksum前40字节，无指针或padding，全部little-endian：id u64（偏移0）、phase u32（8）、turns u32（12）、call_index u64（16）、outcome u32（24）、restarts u32（28）、input_index u64（32）。phase为1待模型、2待工具、3已终结、4取消中；无待处理call时call_index为UINT64_MAX。input_index必须引用该运行的user消息；有待处理call必须引用当前历史最后一条tool-call。消息与检查点一起提交、一起回放，单独检查点kind=3不能创造一个没有对应输入的新运行。

当前控制契约：新输入只在无活动运行时准入，原子创建id与MODEL检查点；模型调用前持久化轮次；assistant+call与TOOL检查点原子提交；工具结果与MODEL检查点原子提交；最终响应与TERMINAL/成功原子提交。错误或取消的终态也必须提交，不能仅发终态事件。system/reset不能改活动运行；reset在终结后清除当前检查点视图，物理历史保留。

cancel先提交ABORTING，再停止本地模型/工具续接；取消待执行工具时补取消结果与终结状态。重开ABORTING只做取消结算，不执行工具。`pg_agent_suspend()`只停止本地执行、清除未提交草稿并释放模型请求，保留检查点且不发终态；`pg_agent_resume(agent,0)`显式恢复，不代表后台已有调度器或等待句柄。已终结运行不重跑、不重复发终态。

模型轮次最多16，调用前提交，重开不清零。连续显式恢复上限5，跨进程保存，模型/工具阶段确有进展后清零；普通暂停后再resume也计一次，这是与上游仅统计crash/eviction不同的明确局部规则。达到上限返回2且不调度；显式`pg_agent_resume(agent,1)`重置恢复预算并继续，仍不能清除模型轮次预算。并非无限自动重试。

恢复目前只支持原有只读/replay-safe工具契约。工具已执行但结果未提交时可能重读；结果已提交则不会重复执行。没有外部写入的exactly-once保证。调用者必须提供原模型/工具/工作目录配置；其身份尚未持久化，不能自动采用改变后的环境。运行id来自提交序号，不等于用户requestId或跨历史去重表。

内部ABI已变化：pg_session_control=72、pg_session_change=48、pg_run_checkpoint=40、pg_agent=289048字节，所有消费者同批重编译；模型、pg_tools、工具定义和pg_client布局保持不变，尚无稳定ABI承诺。v1线格式及消息容量不变；v2新增40字节协议字段产生的缓冲增加，不是扩大消息/文件预算。

在途真实验证：`python3 -B tools/runtime/build_test.py --live-resume`。三个独立进程依次收到真实流文本后暂停、重开恢复原输入、再重开回忆；不重复提交原user，不把旧partial当完成消息。纯边界/故障测试另覆盖11个中断/完成/取消/暂停位置、未发布终态但完整帧已写入、恢复上限与显式retry。
