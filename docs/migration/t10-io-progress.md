# T10 文件与单调时钟 PAL 候选

日期：2026-10-20。状态：技术预览验证完成，新服务固定登记已获用户审核批准，仅限本地实验。原内存包已由用户批准，不代表本扩展自动获批。

[候选登记](t10-io-candidate.json) · [源码与机器证据](t10-io-evidence.json)。原始目录：`/home/xu-yuanhao/Documents/PanGu-artifacts/t10-io-_neth7by`。完整 source.tar、每文件摘要、构建/复验与隔离运行命令均已保留。

## 实现及边界

新增 platform.file / platform.clock v1，由 platform-io 提供；接口归 bootstrap-api，测试客户端通过 64 字节函数表消费。C 断言校验布局，Assembly 组合选择实现。Linux 系统调用与 stat/timespec 布局留在平台模块，不复制到通用消费者。

文件只支持普通文件的只读打开、独占创建、单次读写与关闭。独占创建请求 0600，设置 CLOEXEC、NONBLOCK、NOFOLLOW；已有文件不会被覆盖，末级 symlink、目录与 FIFO 不被当普通文件使用。被 fstat 拒绝后关闭临时描述符，测试以 fd 复用确认清理。借用路径必须是调用方有效的 NUL 结尾字符串。

这些限制不是路径沙箱：中间目录 symlink、绝对路径与 .. 没有被约束，不允许工具层据此声称已实现授权或目录隔离。T25/G08 再建立受控路径与执行边界。

读写返回实际字节数，允许短传输，EOF 成功返回零；非法 fd/buffer/过大长度拒绝，失败输出计数为零。read/write 遇 EINTR 重试，其他 OS 错误为状态 8；ENOENT 为 3、EEXIST 为 4、非普通文件为 9。EINTR 重试尚未做定向注入，不据此宣布完整信号兼容。

close 按 Linux 消费描述符，清零为 -1，错误保留非零状态而不盲目重试。[Linux close 手册](https://man7.org/linux/man-pages/man2/close.2.html)说明关闭错误可能在描述符释放后才报告；这是 Linux 提供者语义，不是跨平台承诺。执行后端必须允许必要清理；若 seccomp 在调用前拒绝 close，不能声称资源已释放，该后端不满足当前清理能力约定。成功 close 也不代表已 fsync 或断电安全。

时钟读取 CLOCK_MONOTONIC 并转换为 uint64 纳秒；检查 timespec 与乘加溢出，错误输出清零。没有使用墙上时间或自研时钟框架，也没有性能阈值承诺。

## 验证

三个组合 bootstrap/memory-probe/io-probe × debug/release 共六项，在冻结镜像、只读输入和断网环境构建及复验。实际执行另外挂载新建隔离工作目录；没有用户文件、凭据或真实工作区写入。

文件用例实际覆盖：缺失、已存在、末级 symlink、目录、FIFO、空路径/非法参数、短读与 EOF、零长度读、写只读 fd 的错误、独占创建写入再读回、关闭后状态、重复关闭、非法 fd 的关闭错误与目录/FIFO 拒绝后的 fd 复用。外部检查 created 字节为 ok、权限 0600、原 input 和 symlink 未改变。时钟连续读取 1000 次不倒退；这不是性能基准。

另以 seccomp 分别强制拒绝 openat/read/write/clock_gettime，四个夹具两模式共八次运行均验证错误及输出状态；读写错误后仍关闭正常句柄，拒绝写入时创建文件保持空。没有测试或声称后端可以禁止 close 后仍保证清理。

原已批准 Bootstrap 与 memory-probe 的两模式 ELF 字节完全不变。io-probe ELF 为 debug 14440 / release 9752 字节，仅用于局部测试，不是 Agent 体积。所有预算继续 UNBASELINED。

## 待审包

控制树 SHA256：`fee6a9e905dc1413339e3af505e43e7e8d75bb3da40acbe7082808982d1b863d`。

相对已批准内存控制包，仅修改既有候选 registry，登记 io 接口、platform-io、测试消费者、服务和组合；门禁、编译参数、镜像及既有 ABI 未修改。用户已明确批准该固定包用于后续本地实验；这不是生产或 GitHub 身份批准，不覆盖新的后续模块。

T10 整体不据此自动完成；完整阶段退出、资源预算和受保护集成仍须各自证据。下一原生能力是 T11 分配器/Buffer，不能把当前裸 syscall 包装当作这些已经实现。

## GitHub 实验源码快照

已按用户要求上传 [experiments/t10-native-pal](https://github.com/xu-yuanhao/PanGu/tree/4d52e0ec380c6b0441e82700c8d0a5bdb1cb8acc)，提交 4d52e0ec380c6b0441e82700c8d0a5bdb1cb8acc。该分支包含固定控制器及实际原生源码，便于恢复和审阅；不是 main 合并、生产批准或自动可信控制器来源。具体内容绑定见 [发布记录](t10-source-publication.json)。
