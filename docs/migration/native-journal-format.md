# 盘古原生事务日志 v1/v2/v3/v4（本地开发）

实现：`src/runtime/journal.S`。这是盘古自己的格式，不是 Pi Durable JSONL，也不导入/覆盖 Pi 数据。参考 Durable 的全批次提交、提交前序列化、失败隔离和尾部恢复语义；选用显式二进制帧，使汇编可直接处理长度与字段，不增加JSON编码依赖，不保存C结构布局或指针。版本尚未稳定，读取v1/v2/v3/v4；未知版本拒绝，不改写已有v1帧。检查点使用v2记录，配置身份使用v3，带requestId的输入准入使用v4，旧消息/system/reset仍用v1。

## 文件与所有权

调用者提供可信目录fd和单个basename。拒绝空名、`.`、`..`、含`/`的名字及超过255字节的名字；最终文件使用NOFOLLOW/CLOEXEC/NONBLOCK，创建模式0600，只接受普通文件。父目录fd复制归提供者所有，原fd仍归调用者。

一个打开实例持有独占非阻塞flock到close；锁是协作式单写者协议，不是阻止任意本地机器码的安全沙箱。读写使用显式偏移，不依赖共享文件游标。v1帧上限仍为8544字节、实际最大8530；v2增加固定40字节检查点，编码缓冲8592字节、实际最大8570；文件上限16MiB，超限拒绝新提交，不自动压缩、删历史或改预算。

## 帧

所有整数为无符号little-endian，长度以字节计。每个帧代表一个完整Session变更。

| 偏移 | 长度 | 含义 |
|---|---:|---|
| 0 | 8 | magic：`PGJRNL1`后跟NUL |
| 8 | 4 | 格式版本1、2、3或4 |
| 12 | 4 | 包含头部与尾部的总长度 |
| 16 | 8 | 提交序号，从1开始、逐次加1 |
| 24 | 4 | 操作：0 append、1 system、2 reset、3 checkpoint（仅v2）、4 profile（仅v3）、5 submission（仅v4） |
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

完整帧可能在“已写入、尚未确认给调用者”时随进程退出而留下。重开会接受有效帧，不能假定调用者没收到成功就等于没提交；带requestId的输入准入可在重开后去重，任务检查点处理在途工作；没有requestId的调用仍由调用者避免重复提交。

## 当前范围与验证

已实现Session消息/system/reset及单运行检查点的文件存储和重开；不含通用Task图/Submission队列、模型流partial、压缩、自动文件格式迁移或外部副作用结算。交互入口默认易失；设置PANGU_SESSION_DIR选择现有私有目录中的session.pgj，即启用文件Storage、配置身份核对和在途恢复。

`tests/runtime/test_journal.c`验证跨进程重开、工具双消息批次、reset、所有最终帧截断位置、完整文件逐字节损坏、带正确校验的非法字段、最大8530字节记录、独占锁、短读/短写/EINTR、写入/刷盘/截断/读取失败、半写后进程退出、路径/文件类型/配额边界。IO测试包装同时检查外部调用入口栈对齐、主动破坏caller-saved；五参数ABI探针检查open/Session提交/close的callee-saved、DF与栈哨兵。测试只在临时目录操作，不修改用户会话。主入口：`python3 -B tools/runtime/build_test.py`。

真实已结束会话的跨进程验证：`python3 -B tools/runtime/build_test.py --live-journal`，需环境中的MIMO_API_KEY。驱动在临时目录分别启动store/recall两个进程，首次写入结束会话，第二次新进程回放后向真实模型提问；不把它称为在途任务续跑。

## v2单运行检查点

检查点位于帧checksum前40字节，无指针或padding，全部little-endian：id u64（偏移0）、phase u32（8）、turns u32（12）、call_index u64（16）、outcome u32（24）、restarts u32（28）、input_index u64（32）。phase为1待模型、2待可安全重放工具、3已终结、4取消中、5待不可安全重放工具；无待处理call时call_index为UINT64_MAX。input_index必须引用该运行的user消息；有待处理call必须引用当前历史最后一条tool-call。phase=5与phase=2具有相同call引用约束；恢复仅phase=2且当前工具仍明确声明safe才重执行，其他中断调用提交错误结果，不自动重写。旧读取器拒绝未知phase=5，不将其当安全工具执行。消息与检查点一起提交、一起回放，单独检查点kind=3不能创造一个没有对应输入的新运行。

当前控制契约：新输入只在无活动运行时准入，原子创建id与MODEL检查点；模型调用前持久化轮次；assistant+call与TOOL检查点原子提交；工具结果与MODEL检查点原子提交；最终响应与TERMINAL/成功原子提交。错误或取消的终态也必须提交，不能仅发终态事件。system/reset不能改活动运行；reset在终结后清除当前检查点视图，物理历史保留。

cancel先提交ABORTING，再停止本地模型/工具续接；取消待执行工具时补取消结果与终结状态。重开ABORTING只做取消结算，不执行工具。`pg_agent_suspend()`只停止本地执行、清除未提交草稿并释放模型请求，保留检查点且不发终态；`pg_agent_resume(agent,0)`显式恢复，不代表后台已有调度器或等待句柄。已终结运行不重跑、不重复发终态。

模型轮次最多16，调用前提交，重开不清零。连续显式恢复上限5，跨进程保存，模型/工具阶段确有进展后清零；普通暂停后再resume也计一次，这是与上游仅统计crash/eviction不同的明确局部规则。达到上限返回2且不调度；显式`pg_agent_resume(agent,1)`重置恢复预算并继续，仍不能清除模型轮次预算。并非无限自动重试。

恢复目前只支持原有只读/replay-safe工具契约。工具已执行但结果未提交时可能重读；结果已提交则不会重复执行。没有外部写入的exactly-once保证。profiled会话必须先以pg_agent_bind_profile核对原配置，再resume；不匹配活动运行时拒绝且不写日志。旧无身份日志仍可由显式底层调用者提供原配置处理，产品交互入口拒绝自动采用这种已有日志。运行id来自提交序号；可选requestId由独立有界记录关联，见v4。

内部ABI已变化：pg_session_control=9304、pg_session_change=48、pg_run_checkpoint=40、pg_agent=298288字节，所有消费者同批重编译；模型、pg_tools、工具定义和pg_client布局保持不变，尚无稳定ABI承诺。v1线格式及消息容量不变；v2新增40字节协议字段产生的缓冲增加，不是扩大消息/文件预算。

在途真实验证：`python3 -B tools/runtime/build_test.py --live-resume`。三个独立进程依次收到真实流文本后暂停、重开恢复原输入、再重开回忆；不重复提交原user，不把旧partial当完成消息。纯边界/故障测试另覆盖11个中断/完成/取消/暂停位置、未发布终态但完整帧已写入、恢复上限与显式retry。

## v3配置身份与交互生命周期

v3只允许kind=4、count=0，不带检查点，payload为1～4096字节身份。新写入的profile来自pg_profile_encode：前32字节依次为PGCFG1后两个NUL（8）、设备号u64（8）、inode u64（8）、工具数量u32（4）、保留零u32（4）；之后provider/model/workspace/implementation各为u32字节长度+原始字节，四项非空且无NUL；每个工具依registry顺序编码name/description/schema，同样长度+字节（描述可空）。无padding、进程指针或密钥，完整身份最多4096字节，最大profile帧4152字节。Session把它作为不透明身份保存，精确长度与字节比较；FNV仅检错，不是身份认证。

静态组合采用规范工作区路径及已打开根fd的设备号/inode、实际工具定义、固定provider契约和当前MIMO_MODEL。固定provider/implementation版本约定包含当前端点、请求选项和只读策略；这些语义若改变必须调整版本或明确迁移，不能静默复用身份。凭据可轮换，不参与身份，也不存日志。此机制防止误用不同配置续跑，不防恶意原生插件篡改内存，不是OS沙箱。系统提示已单独持久化，恢复不重新覆盖。

活动运行不能替换profile；无活动运行时允许显式绑定新配置并提交新profile，相同身份核对不产生写入。reset保留profile。日志open只恢复身份，Agent的进程内verified标记保持0；set_system/reset/submit/resume/模型start在有profile而未核对时拒绝。验证失败不自动重试或丢弃在途工作。

PANGU_SESSION_DIR需指向现有目录，最终目录不跟随符号链接；固定文件session.pgj沿用独占锁和日志校验。目录由用户可信选择，不承诺完整祖先路径沙箱。目录变量未设置仍使用易失模式。启动在进入raw终端和网络请求前核对身份，再自动resume未结束运行；已终结运行不重跑。达到5次恢复上限时停在诊断，底层显式retry API仍可用，当前TUI未提供重试界面。

持久模式通过bridge的可选detach回调绑定pg_agent_suspend：Ctrl-D/外部终止信号退出时停止本地请求、保留检查点，不调用cancel或排空伪造终态；Ctrl-C忙时仍明确abort。默认易失bridge保持原有有限取消排空。bridge尺寸240字节，stop_error记录detach失败，composition将其计入退出码。单所有者、没有后台执行器；关闭后下次进程重开才继续。不会保存流式草稿或完整TUI屏幕；启动只回显当前已提交消息。

专项证据在test_profile.c（身份/配额/不匹配）、test_journal.c（v3最大帧、全部小帧尾切分/逐字节损坏、合法校验的非法版本/kind/count及fsync不确定恢复）、test_runtime_bridge.c（detach不cancel/poll、失败、幂等和ABI）。实际交互验收入口为python3 -B tools/runtime/test_pangu_pty.py --live-persistent；需要环境凭据，临时目录和会话自动删除。

## v4有界输入Submission

`pg_agent_submissions()`返回独立的`pg_submissions`函数表（context/submit/find，24字节）。默认实现依托当前Session事务和单运行执行器；消费者可以替换整张服务表，宿主不解释请求业务。没有队列、通用任务图或阻塞wait句柄，原pg_client.submit仍是无requestId的简单输入入口。

submit先核对配置/控制状态和requestId，再查询已有记录，最后才对新请求检查busy/输入并提交。相同requestId即使新文本不同也返回原id/status，不重复写入、启动模型或发布事件；已结束记录同样去重。find只读，不触发恢复/网络。输出由调用者独立拥有，失败不修改；record为id u64、settled u32、outcome u32、request_id[64]，80字节，只有settled=1时outcome有效。

原生差异：当前只接收input，requestId要求1～63非NUL字节，每个Session最多64个有键记录；无键请求不占记录。记录跨reset与重开保留，满额拒绝新键，不静默淘汰，不以reset绕过去重。该上限是显式本地范围，不声称覆盖上游任意字符串/无限历史。请求键作用域是各自Session。新键准入将用户消息、运行检查点和关联记录同事务提交；确认前查询看不到新记录，不确定结果必须重开。

v4只允许kind=5、count=1、40字节检查点。payload先放requestId长度u32及其原始字节（不带NUL），随后为一条与v1/v2相同的消息，最后附v2相同40字节检查点。准入必须是PG_USER和新MODEL运行，id=当前序号+1；重复键或已有运行上重放kind=5被拒绝。最大当前合法输入记录为4274字节（63字节键+4095字节文本），帧缓冲8592、消息/文件预算不变。后续进度/终态仍用v2检查点；回放时按run id更新对应记录的settled/outcome，旧记录不被后续运行覆盖。未知版本拒绝，不提供降级写入或自动格式转换。

Session控制尾部增加submission_count及64个记录；checkpoint仍40、change仍48、模型可见Session/pg_client/pg_model布局不变，所有内部消费者同批重编译。新增lookup无IO/分配，提交与结果发布仍由原事务边界保证。

验证入口：`python3 -B tools/runtime/build_test.py`包含test_submissions.c及ABI探针；`--live-submission`使用真实MiMo三进程中断/去重恢复/已结束去重与新输入回忆。仅输入去重，不等于外部工具副作用的exactly-once。

## 启动历史展示

组合入口通过pg_tui_bridge_set_history绑定借用的已提交Session视图，记录启动时消息数；TUI的可选history(index,out)回调仅提供有界事实，最多64条、文本4095字节、工具名/ID63字节。客户端先逐条渲染一次，再attach在途运行；历史与执行事件分开，不发run_started/run_finished，不推进模型、提交消息或改写日志。包含已提交的待工具call，显示为call而非宣称已执行；结果从相邻call取得工具关联，系统提示与未提交draft不回显。读取期间视图变化、非法字段、非终止或错误回调会报告错误，不继续启动任务。

展示沿用当前终端scrollback，不新增会话浏览器。文字中的C0（LF/tab除外）、DEL与C1转为可见转义，非法UTF-8字节按字节转义；先合并完整UTF-8 carry再过滤，避免跨分片的编码控制或emoji误处理。截断按原始文本字节/UTF-8边界，转义可能扩展显示字节；会话原始内容和模型投影不变。可信ANSI编辑命令保留。内部tui_runtime_ops由48变56字节、history_entry64、bridge264；tui_state仍32字节，利用原尾部padding记录一次加载。所有客户端消费者重编译，pg_client/模型/日志格式不变。
