# T08 原生服务、ABI 与数据模型契约草案

状态：候选契约 `CANDIDATE_NOT_APPROVED`。本文定义跨边界规则、C 声明草案和最小服务边界，用于独立实现者互操作与后续 T10～T16 实施。它不是生产头文件，也未证明生命周期、并发或持久化正确。生产头文件入库必须先过准入审核。

依据：Pi 固定提交 `2ec55d808d455b1a64a2bf7888198207a096120a` 的 `packages/agent/src/agent-loop.ts`、`types.ts`；Cordis `packages/core/src/fiber.ts`、`registry.ts`、`service.ts`；Harness `packages/core/agent-loop/src/index.ts` 的服务注入与 effect 注册模式。

## 1. 边界规则

| 规则 | 内容 |
|---|---|
| 语言 | 跨边界只用 C17；不使用 STL、异常、RTTI、编译器私有对象、裸结构持久化 |
| 固定宽度 | 只用 `stdint.h` 的 `uint8_t/uint16_t/uint32_t/uint64_t/int32_t/int64_t`；布尔用 `uint8_t` |
| 对齐 | 所有公开结构按自然对齐；显式填充字段，禁止依赖编译器隐式填充的顺序 |
| 版本 | 每个可扩展描述符首字段为 `uint16_t abi_version` + `uint16_t reserved0` + `uint32_t struct_size`；调用方在传入前填 `struct_size`，首版精确匹配，否则拒绝；slice/bytes/handle/service_id 是固定布局值类型，无此前缀 |
| 所有权 | 每个指针参数注明 `borrowed`（调用期间有效）或 `owned`（转移且由接收方释放） |
| 释放 | 由分配方释放；跨服务传递使用显式 allocator 或 `release` 回调 |
| 错误 | 函数返回 `pangu_status`；不使用 `errno` 作为跨边界契约 |
| 取消 | 取消是请求，不是完成；完成与资源释放分别可见 |
| 线程 | 默认单线程调用；跨线程回调须在服务描述符中显式声明 |

## 2. 基础类型

```c
typedef int32_t pangu_status;
enum {
    PANGU_OK = 0,
    PANGU_E_INVALID_ARGUMENT = 1,
    PANGU_E_NOMEM = 2,
    PANGU_E_NOT_FOUND = 3,
    PANGU_E_ALREADY_EXISTS = 4,
    PANGU_E_BUSY = 5,
    PANGU_E_CANCELLED = 6,
    PANGU_E_TIMEOUT = 7,
    PANGU_E_IO = 8,
    PANGU_E_UNSUPPORTED = 9,
    PANGU_E_STATE = 10,
    PANGU_E_OVERFLOW = 11
};

typedef struct pangu_slice {
    const uint8_t *data;   /* borrowed, may be NULL only when len == 0 */
    uint32_t len;
    uint32_t reserved;
} pangu_slice;

typedef struct pangu_bytes {
    uint8_t *data;         /* owned by the holder */
    uint32_t len;
    uint32_t capacity;
} pangu_bytes;

typedef struct pangu_handle {
    uint64_t value;        /* opaque; 0 means invalid */
} pangu_handle;

typedef struct pangu_allocator {          /* x86_64: 40 bytes */
    uint16_t abi_version;
    uint16_t reserved0;
    uint32_t struct_size;
    void *ctx;             /* borrowed by every call, not owned by the allocator user */
    void *(*alloc)(void *ctx, uint32_t size, uint32_t align);
    void *(*realloc)(void *ctx, void *old_ptr, uint32_t old_size,
                     uint32_t new_size, uint32_t align);
    void (*free)(void *ctx, void *ptr, uint32_t size, uint32_t align);
} pangu_allocator;
```

规则：

- `alloc(0)` 可返回非 NULL 或 NULL，调用方不得解引用零长度内存。
- `realloc` 的 `old_size`/`new_size` 必须由调用方如实提供，用于计数与泄漏检测。
- 从某 allocator 分配的内存必须回到同一 allocator；不提供隐式全局释放。

## 3. 服务描述符与注册

```c
#define PANGU_SERVICE_ID_LEN 32
#define PANGU_SERVICE_VERSION(major, minor) \
    ((((uint32_t)(major)) << 16) | ((uint32_t)(minor)))

typedef struct pangu_service_id {
    char name[PANGU_SERVICE_ID_LEN];  /* NUL-terminated ASCII, e.g. "allocator" */
    uint32_t version;                 /* exact match required */
} pangu_service_id;

typedef struct pangu_service_desc {       /* x86_64: 88 bytes; id at 8, instance at 64 */
    uint16_t abi_version;
    uint16_t reserved0;
    uint32_t struct_size;
    pangu_service_id id;
    uint32_t role;                    /* 1=definition 2=provider 3=consumer */
    uint32_t required;                /* 1=required, 0=optional */
    const pangu_allocator *allocator; /* borrowed for activation lifetime */
    void *instance;                   /* provider-owned opaque state */
    pangu_status (*activate)(void *instance);
    void (*deactivate)(void *instance);  /* must not fail; releases resources */
} pangu_service_desc;
```

语义：

- 服务 ID 与版本精确匹配；不提供范围兼容。
- `required=1` 的依赖缺失时不进入激活状态，返回 `PANGU_E_NOT_FOUND`，已激活的服务保持可用。
- 提供者唯一；重复注册返回 `PANGU_E_ALREADY_EXISTS`。
- 消费者只持有接口定义，不引用提供者内部符号。替换提供者只改组合。
- 激活按依赖拓扑顺序；失败时按已激活的逆序 `deactivate`。
- `deactivate` 不返回值，失败只能记录；资源释放必须幂等。

## 4. Root scope 首版

首版只支持 root scope：

- 一个进程内一个 root scope；作用域嵌套、隔离与热重载不在首版。
- scope 拥有 allocator、日志、事件、时钟等基础服务；业务服务向 scope 注册。
- scope 销毁先对依赖方 `deactivate`，再释放服务。
- 不允许服务在注册时隐式获取另一个服务的私有实现。

## 5. 异步操作与取消

```c
typedef uint32_t pangu_op_state;
enum {
    PANGU_OP_PENDING = 0,
    PANGU_OP_RUNNING = 1,
    PANGU_OP_DONE = 2,
    PANGU_OP_FAILED = 3,
    PANGU_OP_CANCELLED = 4
};

typedef struct pangu_op {
    uint16_t abi_version;
    uint16_t reserved0;
    uint32_t struct_size;
    pangu_handle handle;
    pangu_op_state state;
    uint32_t reserved1;
} pangu_op;

typedef struct pangu_op_callbacks {
    uint16_t abi_version;
    uint16_t reserved0;
    uint32_t struct_size;
    void *ctx;
    void (*on_update)(void *ctx, pangu_slice chunk);
    void (*on_complete)(void *ctx, pangu_status status);
} pangu_op_callbacks;
```

规则：

- `cancel` 只设置请求；状态可能变为 `CANCELLED`，也可能在完成检查点变为 `DONE`/`FAILED`。
- 回调在拥有该操作的服务线程/事件循环上调用；不得在回调内销毁同一操作。
- 完成回调之后不再有更新回调；实现必须丢弃迟到的更新。
- 首版采用一个由组合注入的事件循环服务；消费者不各自复制事件循环。T15 再实现调度，不在 T10 提前增加线程。

## 6. 事件草案

ABI、wire 与存储版本分别管理：

| 版本 | 范围 | 规则 |
|---|---|---|
| ABI 版本 | 结构布局与函数签名 | 改变布局必须提升；旧消费者不得静默运行 |
| wire 版本 | 进程间/RPC 消息 | 与 ABI 独立；可映射但不自动兼容 |
| 存储版本 | 会话文件 | 只追加、不原地改写；升级走新版本记录 |

Agent 事件（草案，非全部实现）：

| 事件 | 关键字段 | 顺序约束 |
|---|---|---|
| `agent_start` | run id | 最先 |
| `turn_start` | turn index | 每次请求前 |
| `message_start/end` | role、content | 成对；assistant 可先 update |
| `message_update` | delta | 仅 assistant 流式期间 |
| `tool_execution_start` | call id、name | 在对应 `message_end` 之后 |
| `tool_execution_update` | call id、partial | 仅该调用执行期间 |
| `tool_execution_end` | call id、result、is_error、duration | 完成或失败后一次 |
| `turn_end` | message、tool results | 工具结果之后 |
| `agent_end` | messages | 最后；监听器结算后才算 idle |

并行工具：`start` 按源顺序；`end` 按完成顺序；工具结果消息按源顺序发出。这个约束来自 Pi `agent-loop.ts` 的 `executeToolCallsParallel`，是 T09 对照用例的依据。归一化只允许已声明的独立事件交换次序，不能删除事件。

## 7. Agent / Session / Tools / Provider 边界

| 组件 | 责任 | 非责任 |
|---|---|---|
| Host/Bootstrap | 进程入口、服务解析、资源归属、清理 | 模型厂商、工具实现、UI |
| Allocator/Buffer | 分配、容量、显式长度 | 业务对象 |
| JSON | 解析/序列化、错误位置 | Schema 语义 |
| Model/Provider | 请求构造、流式事件、取消、错误分类 | 重试策略、会话存储 |
| Agent Loop | 回合、工具批次、事件顺序 | 文件/网络直接访问 |
| Session | 消息、树、持久化 | 模型调用 |
| Tools | 校验、执行、结果 | 权限决策（由策略服务） |

每个组件都是可替换服务；不把每个 helper 拆成插件。Agent Loop 与 Provider 不进入固定宿主业务。

## 8. 最小模块与依赖图（候选）

```text
composition (后续组合入口，不是固定宿主业务)
  selects host + interface definitions + platform/foundation/runtime providers
host -> bootstrap-api (只依赖通用契约)
providers -> interface definitions
agent-loop -> model/session/tools/event definitions (不绑定具体提供者)
```

允许边由显式登记及服务绑定决定，不采用笼统的层级规则。接口模块不登记实现翻译单元。首条构建组合由 T10 准入包确定，本图不构成模块登记。

## 9. 未决问题

1. `pangu_slice` 的借用生命周期如何在回调边界表达：是否需要 generation/epoch 校验。
2. 取消与完成竞争的具体测试：迟到的 update、完成前取消、取消后释放。
3. Session 存储版本升级与回滚：是否允许同文件多版本记录并存。
4. 事件 ID 与工具调用 ID 的生成：进程内单调、跨恢复稳定。
5. 是否允许 DSO 插件在首版静态注册之外加载；ABI 卸载与重载未设计。
6. 日志切片与敏感字段脱敏的归属。
7. allocator 计数与 guard page 探针的接口是否进入公开边界。

## 10. 独立消费者实验（已运行，局部通过）

实验目录在项目外：`/home/xu-yuanhao/Documents/PanGu-artifacts/handoff-h00-1792423753/experiment`。源码为 `pangu_contract.h`、`provider.c`（C 提供者）、`entry.S`（汇编消费者）、`support.c`（freestanding memcpy/memset）；不接入产品构建，也不是生产头文件。

命令（Clang 23.1.3 / GNU as 2.47 / GNU ld 2.47）：

```sh
clang -std=c17 -O0 -g -ffreestanding -fno-stack-protector -fPIC -fno-builtin -Wall -Wextra -Werror -c provider.c -o provider.o
clang -std=c17 -O0 -g -ffreestanding -fno-stack-protector -fPIC -fno-builtin -Wall -Wextra -Werror -c support.c -o support.o
clang -E -x assembler-with-cpp entry.S -o entry.s
as --64 entry.s -o entry.o
ld -static -e _start -o interop entry.o provider.o support.o
./interop
```

结果（回执摘要 `c1cae95209902f11952e0f0e29a643f834bafb3f4522c6572155b998b7fbe051`）：

| 检查 | 结果 |
|---|---|
| 构建 | 全部退出 0，`-Werror` 无告警 |
| ELF | ELF64、ET_EXEC、x86-64、无 INTERP、栈不可执行、无未定义符号 |
| `sizeof(pangu_allocator)` | 40 |
| `sizeof(pangu_service_desc)` | 88 |
| `offsetof(service_desc, id)` | 8 |
| `offsetof(service_desc, instance)` | 64 |
| 提供者分配、汇编消费者校验 Buffer | 通过（len 16，字节 1..16） |
| 同一 allocator 释放、计数器归零 | 通过 |
| 激活失败反向清理 | 通过，顺序 `ABba` |
| 错误 `struct_size` / 空描述符 | 通过，`PANGU_E_INVALID_ARGUMENT` |

实际结构尺寸（x86_64）：`pangu_slice` 16、`pangu_bytes` 16、`pangu_op` 24、`pangu_op_callbacks` 32。

实验发现并已修正：

1. 首轮断言写成 48/56 字节，与实际 40/88/8/64 不符。检查器暴露了错误，修正的是断言与文档字段表，不是实现。
2. `-fno-builtin` 下 Clang 仍会为结构体赋值与复制生成 `memcpy`/`memset` 调用。freestanding 宿主必须显式提供这些符号，不能假设隐式 libc。
3. 反向清理顺序初版期望错误；实际是激活 A、B 后按 B、A 撤销（`ABba`）。

未证明：生产 ABI 稳定性、线程/重入、异步取消竞争、DSO 加载卸载、会话与 wire 格式、任何体积或性能预算。实验为同用户目录，不构成权限隔离。

因此 T08 只完成契约草案与布局/所有权/回滚的局部互操作验证，整体仍未完成。

## 主审收尾修正（2026-10-20）

跨 ABI 的 status/op_state 固定为 int32_t/uint32_t，枚举常量不决定 ABI 宽度。历史实验仍只证明历史头文件，不能冒充本修订验证。Buffer/JSON 都保留构建或启动时可替换接口，模块内部 helper 可直接调用；无需热替换或第二个生产实现。服务描述符是后续草案，T10 不实现注册器。

修订后的固定宽度头文件已重新构建并执行原互操作实验，原始日志位于主审收尾报告列出的新目录；原始旧头文件/旧回执不回写。它仍不是完整 G03/G04 或生产准入。
