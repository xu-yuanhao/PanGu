# T10 首批核心准入包（主审收尾版）

状态：候选已完成静态登记与服务预检，等待独立控制面批准；没有产品源码或生产放行。

机器可读候选：[t10-native-candidate.json](t10-native-candidate.json)。它与空的正式 native-admission.json 分开；采用现有 schema，不新增 source kind，不改设计 G00。

## 最小范围

T10 仅有四个模块：bootstrap-api（接口定义）、platform-linux（Linux exit 提供者）、bootstrap（通用宿主消费者）、composition（role=client 的静态组合程序，无 UI）。源码全部位于各自 src/<module>/ 下，头文件也归接口模块。composition 显式依赖默认平台实现及 bootstrap，将平台函数表传入 bootstrap；bootstrap 只读接口，不引用提供者符号。实际登记由 JSON 给出，product scope 与 root service scope 是不同概念。

bootstrap 只依赖接口，不静态引用 Linux 私有实现。后续实现以组合注入函数表或静态绑定适配完成调用；不得绕过登记直接绑定提供者。入口 pangu_start、Linux exit 符号及精确服务版本均已登记。首版 debug/release，无 libc、第三方库、分配、JSON、模型、网络、TUI 或动态加载。

T11 才新增 allocator 接口及 PAL 映射支持的有界 arena；T12 才新增 Buffer/JSON 必要接口和实现。各项扩展先登记；不为了 T10 创建未来模块或测试孤儿。Buffer/JSON 允许构建或启动时替换，不强求热替换，每个内部 helper 不成为服务。

## 开始 T10 的具体条件

1. 独立批准控制器、工具镜像和本候选摘要；将批准的 registry 作为固定控制器输入。不得由编写修改的 Agent 自签批准。
2. 源码开发在独立 candidate 树进行，使用现有 admission.py 全树扫描、两模式构建与产物复验；固定控制器仍是 design-only。候选源码不可登记成 gate，也不可把源码塞入控制器。
3. 必要架构/输入/符号/ELF 检查必须实际执行；预算 null 只表示 UNBASELINED，不代表性能通过。
4. 新程序在无凭据的独立隔离环境验证；在实际合并或发布前必须完成 GitHub 可信身份与发布链，不能声称本地就是远端不可绕过。

## T10 验收

真实入口与平台绑定执行退出；替代测试提供者不改消费者；未知文件、越界依赖、删检查、篡改产物均拒绝。保留完整两模式构建/复验、实际产物大小和输入摘要。T10 无分配，不能用其结果宣称 allocator、完整插件生命周期或 Agent 已完成。

该包取代旧六模块/libc/root-build-scope 提案。旧报告中的 H07 完成只表示交付候选，不表示该旧候选已获批准。
