# 主审收尾与核心开工交接

日期：2026-10-20。状态：候选技术收尾，独立控制面批准与远端部署仍未完成；不能声称生产放行。

## 已修复

- 比较器不再递归抹除业务时间字段；增加事件边界、序号、工具执行闭合检查及安全 case ID。
- Oracle 保存真正原始事件，记录执行失败与摘要；比较器校验外部固定运行回执及原始轨迹，缺失/失败不能通过，保留 raw/normalized/diff。
- 原生 status/op_state 固定宽度，值结构与描述符区分；固定宿主不依赖业务；采用单事件循环注入，不为每服务复制循环。
- T10 缩为接口、平台、通用 bootstrap 与静态 composition 四模块，已有机器可读登记与静态服务预检；不先加 allocator/JSON/libc。
- P0 分配器为有界 arena 的后续 T11 实现；JSON 按 T12 实现有界解析，第三方 HTTP/TLS 不进入 T10 前置。Buffer/JSON 替换点保留。
- 远端方案不再把普通 pull_request workflow 当可信控制入口；空仓库引导需外部固定控制包及维护者的明确动作。

## 本轮实验

原始产物根目录：`/home/xu-yuanhao/Documents/PanGu-artifacts/precore-close-mvyd71fh`。

- oracle-a / oracle-b：修订 runner 分别运行五场景，repeatability 保留外部 pin、命令和比较结果。这是 Pi-vs-Pi，不是 native PASS。
- abi/closure-run.json：修订固定宽度类型后，C/Assembly 布局、错误、资源释放及反向回滚原实验重新构建运行。仅局部互操作实验。
- 最终门禁/迁移/隔离日志及源码快照由该目录的 closure-evidence.json、isolation 请求/输出记录；只按实际结果判断，不用本文件自述替代证据。

## 开工边界与审批包

独立批准对象：本轮 check/oracle/比较器及测试、bootstrap-policy 增量、修订的最小 ABI、T10 候选登记、原已冻结镜像摘要。实际完整文件摘要见外部 closure-evidence.json。编写这些修复的主审 Agent 不自签批准。

批准后按固定 controller/candidate 分离方式开始 T10：先入口与 platform.exit 注入，不实现完整插件宿主、Agent Loop 或 UI。根文件登记策略仍保持 design-only，不把生产源码混入 gate。首份代码在外部候选树，经 admission 两模式编译及复验；普通用户工作树不充当独立可信控制版本。

远端 App 安装、编码身份降权及首次 ref 引导尚未执行，需要具体授权与身份。它们是受保护合并/发布的前置，不以它们已经完成为本地候选实验背书。当前无 commit/push、无生产代码、无凭据修改。

依据根 AGENTS：“控制面变更须独立审核，不能自批”。因此技术收尾不等于自授生产批准；若维护者尚未审核固定控制包，开工状态为 READY_FOR_INDEPENDENT_APPROVAL，而非 READY_FOR_PRODUCTION。
