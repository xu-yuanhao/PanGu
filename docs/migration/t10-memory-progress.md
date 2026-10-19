# T10 最小内存 PAL 候选

日期：2026-10-20。技术预览验证完成，控制面扩展已获用户批准，仅限固定包的本地实验；T10 整体 IN_PROGRESS。此候选不是对原已批准控制包的原地修改。

[机器证据](t10-memory-evidence.json) · [完整候选登记](t10-memory-candidate.json)。源码、固定控制器、原始构建与运行日志在 `/home/xu-yuanhao/Documents/PanGu-artifacts/t10-memory-h25mtquw`，源码归档为 `source.tar`；证据逐项记录 SHA256。

## 范围与参考

T11 的有界分配器需要最少的平台映射能力，先实现 anonymous read/write map/unmap，不提前实现通用 malloc、Buffer、文件、时间或 Agent Loop。

参考 Pi read.ts 的操作接口与实现分离，以及 Cordis fiber.ts 的资源清理归属；原生内存语义按 Linux x86_64 设计，不翻译 JS 对象系统。系统调用号及标志与本机 `/usr/include/asm/unistd_64.h`、`asm-generic/mman-common.h` 对照，头文件不进入产品构建。

## 接口与边界

保留旧 32 字节 platform.exit ABI；新增独立 platform.memory v1。memory API 为 40 字节：version/size、borrowed context、map/unmap 函数指针、零 reserved。mapping 值为 address + uint64 length，共 16 字节。C 静态断言核对宽度及关键偏移。

map(context, requested_bytes, output) 只支持匿名私有 RW 范围，Linux 提供者向 4096 字节取整；拒绝零长度、取整溢出及已占用输出。成功转交完整范围所有权，输出记录实际长度；失败保持零输出。状态：0 成功、1 参数非法、2 内存不足、5 输出已占用、8 其他 OS 错误，不向消费者泄漏负 errno。

unmap(context, mapping) 要求调用方传入此提供者返回的原始范围。参数错误不发系统调用；OS 释放失败保留描述符及所有权，成功清零。清零后的二次释放拒绝。对伪造指针、复制所有权和跨线程竞态不提供安全保证；不维护全局分配表，不宣称任意 native 插件隔离。

新增 platform-memory 模块只依赖 bootstrap-api；测试组合 memory-probe 显式选择实现。测试的 C 消费者只接收函数表，Assembly 组合承担实现符号绑定。旧 Bootstrap 两模式 ELF 与原已验证产物字节完全相同。

## 实际验证

固定镜像、只读控制器/候选、无网络构建 bootstrap 和 memory-probe 两组合 × debug/release，共四项；完整矩阵、对象/ELF 与保留产物复验通过。原生程序另外在清空环境、无用户目录、无网络的 namespace 运行。

- 正常边界：零初始化、取整、首尾写入、两个独立范围、释放互不影响、64 轮映射/释放、重复释放及非法指针/尺寸值拒绝；两模式退出 0。
- 超大范围触发真实 ENOMEM，错误转为 2，输出保持零。
- seccomp 只拒绝 mmap 并返回 EPERM：两模式确认状态 8、输出不改变。
- seccomp 只拒绝 munmap：两模式确认状态 8、原范围仍可读写且所有权保留。该错误夹具最后由进程退出回收，不冒充正常释放完成。
- 释放后访问：两模式出现预期 SIGSEGV，bubblewrap 返回 139；禁止 core dump。它证明测试范围已失效，不是普通成功用例或需修复的随机崩溃。

早期失败均保留：结构体初始化触发隐式 memcpy/memset，C 侧绑定外部函数触发 GOT 符号，门禁均拒绝。夹具改为显式字段操作及 Assembly 组合绑定，没有新增 libc、外部符号豁免或放宽编译检查。信号夹具首次误以为 wrapper 返回 -11，按真实 bubblewrap 的 128+11 修正为 139；平台实现未因此改变。

## 审批与后续

新控制树 SHA256：`1cc5f7794cd46bc3695bb303ee9025f6426c9f8110dacb21a7b3fbbc170ac687`。

它相对已批准控制树仅修改 `docs/migration/t10-native-candidate.json`：新增内存接口文件、platform-memory 与测试消费者、服务定义和测试组合。门禁脚本、工具链、镜像、旧退出 ABI 及旧产品组合未改。该树仍标候选，没有自行改 APPROVED 或声称已覆盖原审批。

用户已明确审核批准，此固定包可用于后续本地 T10/T11 相关实验；根设计树仍不准入生产源码，远端权限/发布保持 BLOCKED。文件/时间 PAL、完整系统资源管理和后续分配器都未完成，预算仍 UNBASELINED。
