# T12 字符串数值转换基础与 Pi 转换参考

2026-10-20，T12 IN_PROGRESS。实施Agent根据用户授权自审自批固定包，仅用于本地实验；同一实施者与审查者，不作为独立审核或生产放行。

[候选](t12-number-string-candidate.json) · [实现与证据](t12-number-string-evidence.json)。目录 `/home/xu-yuanhao/Documents/PanGu-artifacts/t12-pi-number-string-ice294o5`，request.json / controller / candidate / build。控制树SHA256 `fe1fb1b835742a84297178438594b00213050aac2ced3297569335ad425ff3ce`。57个原生文件已归档恢复并复验保留产物绑定；恢复不是重新构建。

## 实现边界

既有pi-arguments模块增加numeric.c和number_string导出，无新模块、服务、依赖或构建模式，旧基础与兼容实现不变。实现ECMAScript Number(string)在严格UTF-8上的数值基础，不是TypeBox TryNumber、Number(value)通用转换、Pi完整coercion或执行授权。

处理ECMAScript空白、空字符串正零、正负号、十进制/指数/前导零/省略小数位、精确大小写Infinity，以及无符号0x/0o/0b整数。非法数值语法成功返回规范化quiet NaN位模式；调用者随后必须应用有限数、integer或fallback策略，不能把NaN当合法工具参数。非法UTF-8返回12，输入/规范化空间超限2，参数/重叠1；失败保持bits不变，scratch可丢弃。

十进制先完整验证语法，去除前导加号、缩减整数前导零、按需补整数/小数零，写入调用者scratch，再使用不变的严格JSON解析和精确binary64转换。没有放宽基础JSON语法。保留其1MiB数值词法限制；原始输入可在调用者上限内更长，例如大量前导零规范化后只需一字节。scratch最多需要input.length+2，无隐式NUL。

进制整数逐位流式处理：保留53位significand加guard及后续sticky，长度饱和1025，按最近/偶数舍入并处理进位溢出；即使已知数值溢出仍检查所有后续字符。没有浮点算术、bigint依赖、动态分配、递归或libc；所有移位范围有界。输入、scratch容量和对齐的8字节bits输出须分离。调用者负责存储和不可变借用，范围算术不认证指针。

## 实际验证

- 2,987条实际Node Number(string)对照，NaN载荷显式归一化；覆盖空白/符号/小数/指数/Infinity/NUL/Unicode、进制与非法位、固定随机语料、精度/舍入/溢出边界。
- 宿主9,978种容量和9项编码/配额/别名拒绝；其中1,760条另与Python数值位结果一致，不把它写成全部语料双oracle共识。
- 原生逐条精确bits和十进制少一字节容量失败；超过1MiB前导零缩短成功、1MiB规范化数值、超限拒绝、巨大进制溢出后非法尾字符、UTF-8/范围/别名和三片页尾检查通过。
- 普通36项冻结构建、48次断网正常运行、4项保护页对照139及12项私有参数拒绝247通过；以前的JSON/Schema/兼容探针与数值格式化全部四分片保留。
- 补充构建36项成功。首次补充运行中，原有json-probe/debug格式化分片1在30秒超时；该ELF与上一包通过产物SHA完全相同。按同样30秒限制单独复验约8.56秒成功，随后在同一批保留构建上新建完整运行目录，48次正常执行、4项保护页和6项移位对照符合0/139/132。没有重建或调整超时/测试，旧失败、单次复验、完整复验及构建来源均绑定证据；超时原因尚未证实，不冒称首轮全绿或已定位根因。

补充UBSan不代表正式sanitizer或汇编全覆盖。严格宿主语法审查发现测试相邻for的缩进歧义，已在冻结前拆行；别名测试缓冲区显式对齐，没有改变语义预期。

## 完整 Pi 转换参考：尚未原生实现

另运行2,250条冻结Pi公共validateToolArguments参考，比较TypeBox构造与plain-schema两种模式、9类目标、必需内部字段、精确语义及错误。记录Kind来源、额外coercion分支；union为参考范围，不表示当前原生Schema已支持。

重要观测：TypeBox integer会截断，scalar可被包装为array；null转string在typed/plain分别可得到"null"/空字符串，"true"转number也有差别。不能以一套自定严格转换覆盖两种路径。

TypeBox1.3.27的Kind为不可枚举字符串键`~kind`，而冻结Pi检查旧Symbol.for(TypeBox.Kind)。因此当前typed Value.Convert后也运行额外JSON coercion。例如Value.Convert保留Infinity为number，但完整Pi仍能经后续string转换得到"Infinity"。最初参考元数据只查询旧Symbol而漏记Kind，已修正元数据和分支记录，转换结果及2987条Number预期均未改，旧元数据记录保留。

这2,250条只建立下一步原生转换的基线，不能与Number(string)原生通过数量相加，也不证明完整validateToolArguments兼容。BigInt后缀/true/false等TypeBox fallback、转换遍历、typed后plain组合、最终Schema校验仍待实现。

完整T12、Provider/wire/session、正式性能/栈/RSS/体积预算、完整G07及受保护服务器准入未完成，native_matrix=PASS与release=BLOCKED同时保留。

源码已公开至 [`codex/t12-pi-number-string`](https://github.com/xu-yuanhao/PanGu/tree/76fb8232b090396e5e2a6a1933f188153b309010)，固定提交 `76fb8232b090396e5e2a6a1933f188153b309010`。发布副本全部preflight输入与固定候选一致，远端ref已回读核对；[发布记录](t12-number-string-source-publication.json)不代表生产准入。
