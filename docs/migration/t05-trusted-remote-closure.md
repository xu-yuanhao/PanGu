# T05 可信远端闭环：候选设计、权限矩阵与阻塞交接

状态：设计候选 `CANDIDATE_NOT_APPROVED`，部署 `BLOCKED`。本文不创建 App、不改规则、不推送控制版本、不执行任何需要新权限的动作。它把 T05 剩余工作拆成可审查的身份、执行器与引导步骤，供维护者决定。

事实依据（2026-10-20 查阅官方文档）：规则集支持把必需状态检查绑定到指定 App；`pull_request_target` 会以提权运行且不得检出不可信代码；创建 check run 仅限 GitHub App。见第 8 节来源。

## 1. 当前状态

| 项 | 现状 | 证据 |
|---|---|---|
| 仓库 | public，`xu-yuanhao/PanGu` | `remote.py` 回读 |
| main 规则集 | id 25399149，PR 审批、必需检查 `pangu/trusted-gates`、禁强推/删除 | `remote.py` |
| 默认分支 | 尚无提交，main ref 不存在 | `vcs.py`：`head=NOT_CREATED` |
| 检查发布者 | 未绑定，`status_check_app_id=NOT_PINNED` | `remote.py` |
| 自动化权限 | 当前自动化凭据仍是管理员 | `remote.py`：`automation_has_admin=true` |
| 受保护执行器 | 未建立 | `remote.py`：`protected_executor=NOT_ESTABLISHED` |

结论：规则存在，但“规则存在”不等于“普通编码身份无法绕过”。T05 不因本设计关闭。

## 2. 角色与权限矩阵（目标）

| 角色 | 身份 | 可做 | 不得做 |
|---|---|---|---|
| 编码提交者 | 受限编码 App（或受限机器账号） | 读写代码分支、创建 PR | 改规则/豁免、改生效控制版本、发布状态检查、合并 |
| 维护者/审批人 | 用户本人账号 | 审批 PR、合并、改规则、批准控制面 | 不得同时作为自己 PR 的独立审查 |
| 检查执行器 | 受保护工作流（固定版本） | 从受保护分支加载控制器，检查 PR head | 执行候选分支脚本、安装候选钩子、加载候选可执行模块 |
| 检查发布者 | 专用 GitHub App（`checks:write`/`statuses:write`，最小仓库范围） | 只发布绑定 head SHA 的结果 | 读写业务代码、改规则、改自身权限 |
| 规则管理者 | 用户账号或受保护管理员角色 | 管理规则集与 bypass | 不参与日常编码 |

关键约束：

- 编码身份不得与检查发布者共用凭据。
- 检查发布者的私钥只存放在受保护执行环境，不进入仓库、日志或普通 CI。
- 若 PR 也由用户账号创建，则用户不能作为该 PR 的独立审批人；需要第二个维护者或受限编码身份。

## 3. 检查名与来源绑定

目标：必需检查 `pangu/trusted-gates` 必须由已安装的检查 App 发布。

官方规则集文档说明：添加必需状态检查时可“select an app as the expected source of status updates”，且该 App 需具备 `statuses:write`、最近提交过 check run 并已与必需检查关联；其他来源设置同名状态时“merging won't be allowed”。

因此：

- 规则集把 `pangu/trusted-gates` 的来源固定为检查 App，而不是“any source”。
- 候选 PR 分支中即使有同名 workflow 发布同名检查，也不满足必需检查的来源条件。
- 必需检查绑定被测 head SHA，不绑定分支名或提交信息。

check run 只能由 GitHub App 创建（REST 文档明确：其他身份只能查看）。它不是普通 token 可伪造的对象。

## 4. 受保护执行入口

固定流程：

1. 触发：可信外部 App/webhook 执行器按固定控制版本调度；不把普通 `pull_request` 合并上下文中的 workflow 当作可信入口。若采用 Actions，另行审核 base-context 调度器：不得执行候选工作流，状态发布凭据只在可信发布阶段可见。
2. 权限：`contents: read`、`checks: write`；不授予 `contents: write` 或 `actions: write`。
3. 取候选：按 head SHA 取候选树，只读。绝不执行候选树中的脚本、构建钩子、包管理器生命周期脚本或原生可执行文件。
4. 控制器：从受保护 ref 检出控制器与工具镜像摘要；校验摘要与批准值一致。
5. 检查：在断网、只读输入、独立输出的环境中运行固定入口；产物写到独立证据目录。
6. 发布：检查 App 以绑定 head SHA、控制版本、工具镜像摘要和证据摘要的方式发布 `pangu/trusted-gates`。
7. 失败：任何未知源码、越界依赖、摘要不符、超时或缺失检查都不发布成功状态。

禁止：

- 用 `pull_request_target` 检出并执行 PR 内容。官方安全文档明确此类提权工作流“must not explicitly check out untrusted code”。
- 在持有 GitHub 凭据的进程或环境里运行候选原生程序。
- 让候选分支的检查结果替代受保护版本的检查结果。

## 5. 首次引导（空仓库）

当前 main 无提交，规则集要求 PR 审批与必需检查，但检查 App 尚未安装。引导必须避免临时关闭规则或伪造绿灯。

候选步骤（待维护者执行）：

1. 维护者安装检查 App 到该仓库，权限限定为 `contents: read`、`checks: write`、`pull_request: read`。
2. 维护者在受保护 workflow 中固定控制器提交与工具镜像摘要；该 workflow 文件本身先通过一次人工审查。
3. 维护者创建初始提交，并记录：提交摘要、控制版本摘要、工具镜像摘要、执行环境说明。
4. 初始检查由检查 App 对固定提交运行并发布；不通过则修正后重跑，不添加 bypass。
5. 将 main 规则集的 bypass 列表保持为空；确认普通编码身份无法修改规则、无法发布该检查、无法合并。
6. 回读证据：规则集、App 安装范围、检查来源、一次真实拒绝（例如越界依赖或删检查的 PR 未获放行）。

引导期间的现实问题：在 App 与 workflow 就绪前，任何“首次 PR”都无法获得合法的必需检查。应先在外部建立可校验未落到 main 的固定提交的执行器，人工审核首个控制包；若当前规则禁止初始 ref 创建，必须由维护者明确批准一次性引导操作及恢复检查，不能承诺只等待 App 就会解除 ref 创建问题；本任务不自行添加 bypass，也不把该例外写成完成。

## 6. 需要维护者决定的事项

| 编号 | 决定 | 选项 | 影响 |
|---|---|---|---|
| R1 | 编码身份形式 | 受限 GitHub App / 受限机器账号 / 用户账号 + 第二审批人 | 决定是否能做独立审批 |
| R2 | 检查发布者 | 新建专用检查 App | 需要安装权限与私钥托管 |
| R3 | 首次引导 | 等执行器就绪 / 记录引导期例外 | 后者必须登记、限期、可审计 |
| R4 | 自动化凭据降权 | 移除当前管理员权限 | 完成后才能声称普通身份不可绕过 |
| R5 | 独立审批人 | 第二维护者或受限编码身份 | 用户自建 PR 时不可自审 |

本任务不执行 R1～R5。它们改变权限与远端控制面，必须由维护者决定并独立审查。

## 7. 解除阻塞后的验收

必须同时具备：

1. 正常 PR 可获得由检查 App 发布的 `pangu/trusted-gates`。
2. 未知源码、越界依赖、删除检查、伪造同名检查、修改发布链这五类攻击各自被真实拒绝，并保存回执。
3. 普通编码身份读取规则集时无管理权限，修改规则集的请求被拒绝。
4. 首次提交与检查绑定同一 head SHA、控制版本和工具镜像摘要。
5. `remote.py` 回读 `status_check_app_id` 为真实 App，`automation_has_admin=false`，`protected_executor` 为已建立。

在此之前，`remote.py` 与 `vcs.py` 的 `BLOCKED` 是本项的真实状态。

## 8. 来源

- 规则集可用规则（含指定 App 作为必需检查来源、bypass 列表）：https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets
- 安全使用 Actions（`pull_request_target`/`workflow_run` 不得检出不可信代码）：https://docs.github.com/en/actions/reference/security/secure-use
- 创建 check run 仅限 GitHub App、`app` 字段：https://docs.github.com/en/rest/checks/runs
- 分支保护/规则集管理权限（仓库管理员或具备 edit repository rules 的自定义角色）：同上规则集页面

## 主审收尾限定

远端引导是合并/发布前置。无远端权限分离时可以在固定本地控制器下准备隔离候选实验，但不能自称生产准入、独立签名或已完成 T05。首次 ref/身份操作须有具体授权，不擅自创建 App、撤销凭据或绕过规则。
