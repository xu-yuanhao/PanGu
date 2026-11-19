# Native service host and direct provider

Public C17 headers: `host.h`, `model.h`, `tools.h`. The administration thread
creates a host, installs descriptors, acquires a service lease and calls the
returned function table with `pg_lease_context()`. Return every lease before
teardown; workers may release leases while the administration thread waits.
Callbacks must not dispose a scope whose lease they own. There is no hidden
background thread or implicit cancellation. Scope handles survive disposal until
host destruction. Scope cleanup waits for users, runs children and injected
consumers first, reverses effects, then releases borrowed dependencies.

Plugin registration publishes to the installing parent namespace. Plain child
scopes keep their registrations local. Newest matching publication wins; an ABI
mismatch is an error rather than falling back to an older incompatible service.
Replacement preserves existing leases; withdrawal reveals the previous binding.
Missing dependencies fail setup, and partial setup rolls back. Dependency removal
disposes consumers; automatic dependency restart belongs to S06 and is absent.

`pangu_services_embed` installs two external descriptors under `execution.driver`,
executes the selected table and verifies restoration after unload. These are
small deterministic driver fixtures, not implementations of Durable. The real
Core/session driver is owned by the Agent runtime (`pangu.agent.core`).

`pg_deepseek_plugin` publishes `pangu.model` ABI 1. Copy its `request_events` to
`pangu_loop_config`, set `has_request_ctx=1`, and use the lease context as
`request_ctx`. Keep `config.ctx` for the host's key resolver, hooks and events.
Keep the lease and immutable `pg_deepseek_config` alive until run returns. Keys
come from `pangu_core_active_api_key()` on each request; a Core `get_api_key`
callback refreshes them. The adapter calls `prepare_payload` after constructing
JSON, `observe_response` after headers before body, and
`observe_provider_event` for each raw JSON chunk. Poll is optional, synchronous,
nonblocking and called from curl progress/body delivery; return failure or call
Core cancellation to stop. Curl is initialized once for process lifetime. Only
the owning application may clean up global curl after all consumers stop.

`pg_file_tools_plugin` publishes `pangu.tools` ABI 1 (`pg_tools_service` contains
Core tool declarations/executors). Supply an injected filesystem environment and
Schema validator; `pg_local_file_environment(cwd)` uses libuv. The local write
lock covers parent creation, writes and close. Read/write presently support whole
text files; edit, offset/limit, bounded truncation, image decoding, search and
process execution are **not implemented**, so this is a partial S03 delivery.
The current read API allocates the complete file. Do not present it as the final
Pi read behavior. `pg_schema_validate` from `<pangu/services/schema.h>` is a production Draft-7
validator using Valijson; link `PanGu::services_schema` and assign it to
`pg_file_tools_config.validate`. It enforces strict types without coercion and
rejects remote references. C++ exceptions are caught at the C ABI boundary. The
file/model regression consumers use this validator. C-only consumers receive the
C++ runtime closure from the schema target.

## Build and checks

```
cmake -S src/services -B build/services -DCMAKE_BUILD_TYPE=Debug
cmake --build build/services --target pangu_check_services -j2
ctest --test-dir build/services -L services --output-on-failure
```

The five CTest programs cover host lifecycle, an external table consumer, five
local HTTP scenarios (tool roundtrip/key refresh/hooks, cancellation, response
hook abort, HTTP 429, missing finish_reason), a Core write/read roundtrip on
space/Unicode paths, and strict Schema regression cases. HTTP fixtures use synthetic credentials. `deepseek_test`
also has `live` mode; inject `PANGU_LIVE_KEY` only into its child environment from
`.private`, and supply the HTTPS endpoint. Its output is status/counters only.
Never record actual request bodies, headers or model text.

The services subtree can also be added by the root build; it reuses `PanGu::core`.
Installation exports `PanGuServicesTargets.cmake`; the parent's package config
must discover libuv with `PkgConfig::PANGU_UV`, CURL::libcurl and PanGu::core first.
Standalone verification exports its test-only minimal Core as well; it does not
claim to install the optional proxy APIs. Root packaging owns the full Core.

## Adopted behavior and dependencies

- Harness `22ddee3792402c3e70ec5f86a63c3e3e895a2f60`,
  `packages/core/scope/src/{index,store}.ts`: scoped visibility, reverse effects,
  shared quiescent teardown. Architecture service/provider/consumer seam.
- Cordis `df4431eb43086bd1f8263a0e8c03e9b7c5233b68`,
  `packages/core/src/{context,registry,fiber,service,events}.ts`,
  `tests/{dispose,service-dispose}.spec.ts`: ownership, failed setup rollback,
  dependency retention while consumers drain. C leases replace JS tracked calls;
  serialized synchronous installation replaces Promise activation.
- Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283`,
  `packages/ai/src/providers/deepseek.ts`, `api/openai-completions.ts`,
  `test/openai-completions-provider-stream-event.test.ts`: direct request,
  reasoning before text, indexed tool deltas, usage, finish and raw chunk hooks.
  The C adapter currently preserves raw incomplete argument JSON during deltas,
  validates complete JSON before tool_end; Pi's partial parsed object projection
  remains a difference. Image/grammar tools and the full compatibility matrix are
  outside this minimum provider and must not silently fall back to text.
- Pi Durable `src/tools/{read,write,path-utils,file-mutation-queue}.ts`,
  `src/env/node.ts`, Coding Agent `src/core/tools/write.ts`: environment seam,
  parent creation, write ordering and cancellation after settled IO. Full read
  truncation/path variants and process parity remain S03 work.

All three sources are MIT; full notices are in `THIRD-PARTY-NOTICES`.

| Dependency | Version/source | License | Actual target |
|---|---|---|---|
| libuv | system 1.52.1, minimum 1.44; https://github.com/libuv/libuv | MIT | `pangu_services` drain/synchronization; `pangu_services_tools` filesystem |
| libcurl | system 8.23.0, minimum 7.85; https://curl.se/libcurl/ | curl | `pangu_services_deepseek` HTTP/TLS/progress |
| Valijson + picojson | vendored 1.1.3 + 1.3.0, pinned archive hashes in `src/services/vendor/README.md` | BSD-2-Clause (each) | `pangu_services_schema` only; C++17 bridge for actual file-tool validation |
| yyjson | existing `deps/yyjson` 0.12.0, https://github.com/ibireme/yyjson/tree/0.12.0 | MIT | provider/tool JSON; symbols linked from Core, no duplicate parser object |

Linux x86-64 GCC 16.2.1: normal 5/5 and full C/C++ ASan/UBSan 5/5 passed. A C-only parent consumer also compiled, linked and ran the validator successfully. Actual DeepSeek:
two requests, tool roundtrip, final text passed; response content redacted.
Windows/macOS/mobile SDK runs have not been performed in this environment.

## S03 文件组合与可选图片

文件插件现在注册 `read`、`write`、`edit`。`read`接受1起始`offset`和非负`limit`，文本最多2000完整行/51200字节，继续位置在结果中，截断元数据在`details_json.truncation`。当前仍完整读取输入文件。`edit`接受`{path, edits:[{oldText,newText}]}`，全部匹配原文件，拒绝重复/重叠/无变化，保留BOM和CRLF；`details_json`携带全文件统一`patch`、`diff`与`firstChangedLine`。文件修改在同一进程内暂全局串行。

配置 `-DPANGU_SERVICES_IMAGES=ON` 单独构建 `PanGu::services_images`，宿主通过同一 `pg_plugin_install` 路径安装 `pg_images_plugin`，随后read可发现 `pangu.images`。GdkPixbuf 2.44.8（最低2.42，LGPL-2.0-or-later，[来源](https://gitlab.gnome.org/GNOME/gdk-pixbuf)）负责解码/方向/缩放/PNG编码；服务使用GLib base64，图像库不进入Core或基础tools链接闭包。宿主可在同一服务ABI下注入自己的图像处理器。当前统一PNG输出，尺寸上限2000x2000/base64上限4.5MiB；与来源的透传/模型限制/坐标提示差异列于TODO。

行为来源：Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283` 的Coding Agent `read.ts`/`truncate.ts`/`edit.ts`/`edit-diff.ts`和Durable `tools/{read,edit}.ts`、`test/tools.test.ts`（read offsets/limits、trailing newline、edit disjoint/overlap/duplicate/BOM+CRLF/concurrent mutation）、`test/tools-images.test.ts`（image block/decode failure）。尚未覆盖全部来源用例，不据此勾选S03。

DeepSeek请求的工具声明只来自Core `pangu_core_active_tool_count/at`。旧config工具字段保留兼容，但prepare替换或清空后绝不fallback。离线HTTP专项覆盖替换名字、实际执行、下一请求清空和首请求空集合。

## Settings and resources (S05 native entry)

Enable `PANGU_SERVICES_RESOURCES` (default ON), link `PanGu::services_resources`,
and include `pangu/services/resources.h`. `examples/services/resources.c` is the
small public-header consumer; `tests/services/resources.c` also selects a model
from the merged settings and sends the resource prompt through a real Core run.
The library links existing services/Core plus libyaml 0.2.5 (`yaml-0.1`, MIT,
https://github.com/yaml/libyaml). The parent owns package dependency discovery
and license installation. OFF skips yaml discovery and all resource targets.

Install `pg_resources_plugin` with explicit absolute `user_directory` (normally
`$HOME/.pangu/agent`) and `cwd`. It copies configuration. Query `pangu.resources`
ABI1 and pass the lease context to its functions. `snapshot` returns an owned,
immutable snapshot; hold it through Core request return, release using
`pg_resource_snapshot_release`. Snapshots survive reload and plugin unload;
readers can inspect them on other threads. Service administration is serialized
and occurs outside Core requests. `reload` swaps only a successfully built
snapshot. `patch_settings` reads latest disk content, merges keys, and commits
by same-directory temporary file, fsync and rename under a directory lock.
A post-commit reload error does not roll disk changes back. Locks time out after
about two seconds and are not automatically stolen after a crashed writer.

Settings precedence: user settings.json, trusted project .pangu/settings.json,
then overrides_json. Nested objects merge; arrays replace, except that a layer
containing only `defaultTools` +name/-name modifiers extends inherited selection.
The snapshot resolves the final tool list, preserving an explicit empty list.
Runtime owns applying model/provider/thinking and tool selection to its loop;
the service does not retain a second model/tool cache.

Resource discovery includes user/project prompts and recursive skills, explicit
`prompts`/`skills` paths, user and ancestor context files (candidate order:
AGENTS.override.md, AGENTS.md, AGENTS.MD, CLAUDE.md, CLAUDE.MD), SYSTEM.md and
APPEND_SYSTEM.md. Project special files override user ones when trusted. A
SKILL.md at a directory root takes precedence over nested skills. Canonical
visited paths prevent symlink cycles and duplicate file loading. Frontmatter is
parsed by libyaml, with diagnostics for invalid YAML, missing descriptions and
name collisions. First discovered resource name wins. Disabled skills remain
available for explicit invocation but stay out of model discovery. Skill bodies
are not eagerly inserted in the system prompt; names/descriptions/locations are.
`pg_resource_expand` supports quoted empty arguments, $N, $@, $ARGUMENTS,
${N:-default}, ${@:-default}, ${ARGUMENTS:-default} and ${@:N[:L]}, with no recursive
substitution. Unknown slash commands pass through.

Host trust decisions 0/1 are explicit. `set_project_trusted` changes the current
session; `remember_project_trust` also writes user trust.json. Configuration -1
loads the nearest remembered canonical ancestor, denying if absent. Untrusted
project .pangu settings/resources are not loaded; ancestor AGENTS context is
still loaded, following the source's separate context path. Trust UI belongs to
the runtime/TUI. `pangu.credentials` ABI1 is a separate service on this plugin:
get_api_key re-reads auth.json per request, resolves literal and $ENV/${ENV}
values (including $$/$! escapes and entry-local env), and falls back to
PROVIDER_API_KEY. Returned key bytes are malloc-owned and must not be logged.
put_api_key atomically writes/removes a provider's api_key entry. Secrets never
enter resource snapshots or diagnostics. OAuth and command-valued credentials
currently return an explicit unsupported/invalid status; they are not executed.

Remaining source coverage is explicit: recursive skills discovery does not yet
apply .gitignore/.ignore/.fdignore rules; linked-worktree context shadowing,
resource enable/disable glob rules and complete source diagnostics/defaults
matrix remain to port. Literal explicit resource paths work, but all package
resource source-selection semantics belong to the later extension integration.
No Windows/macOS/mobile run is claimed. S05 remains unchecked until these source
coverage differences and runtime integration acceptance are resolved.

S05 follow-up: linked-worktree context shadowing now follows the source's
canonical-directory/same-filename rule (including bare/submodule/symlink cases).
Skills discovery now uses libgit2 (`PkgConfig::PANGU_GIT2`, >=1.8; tested 1.9.7)
for .gitignore/.ignore/.fdignore, nested and negated rules. It creates only an
in-memory repository and immutable config backend; no user/project Git config
is written. libgit2 license is GPL-2.0 with its linking exception; the parent
installs COPYING. This supersedes the earlier ignore/worktree coverage gap.
System libgit2's declared `git_config_backend_from_string` is not exported here;
the implementation uses the exported backend registration extension instead.

## Explicit native extension paths

`dynamic.h` is part of `PanGu::services` and reuses its libuv dependency.
`pg_dynamic_plugin_load(parent, cwd, path, config, config_size, &scope, &diag)`
loads one explicit local path. Relative paths require an absolute cwd; paths
are length-delimited, canonicalized and never searched through PATH. Perform
product trust decisions before calling: library constructors execute native code.
This is loading, not discovery, package installation or sandboxing.

Native modules now require `PG_DYNAMIC_ABI == 4`: C04 appends a
`uint32_t presence` field with six defined bits to `pangu_message`, without changing `pangu_tool`.
Rebuild ABI1/2/3 modules. The loader rejects ABI3 at the descriptor check before
calling its factory; the stable `_v1` export name does not select an older ABI.
The regression fixture declares literal ABI3 and aborts if its factory runs.

Service producers need no new flags here: assistant usage is always present,
and Core marks thinking on finalization. File/execution/approval/MCP results do
not supply usage. A tool result intentionally supplying all-zero usage must set
the Core `HAS_USAGE` presence bit; nonzero usage remains inferred for compatibility.

A MODULE exports the const `pangu_plugin_descriptor_v1` descriptor. Its factory
receives the versioned host function table; do not link a second host/Core
implementation into the module. `native-plugin/plugin.c` demonstrates registering
an actual Core tool through that table. Plugin-specific configuration is borrowed
through apply; this example copies its configuration and borrows the validator
context and cleanup counter until scope disposal.

Build `pangu_check_services`, then run from the repository root:

```sh
build/services/pangu_services_dynamic_example "$PWD" build/services/libpangu_services_native_fixture.so
```

The example injects the production Schema validator, loads the module, acquires
its tool service, validates and invokes native_echo, and prints
`external native result`. `tests/services/dynamic.c` additionally drives the
external tool through the actual Core model/tool loop. Keep service leases while
using tool arrays, callback pointers or contexts. Stop/cancel consumers and
release leases before disposing the returned scope; disposal waits for other
leases, rolls back effects in reverse order and closes the library last.
Failures return a NULL scope and a bounded stage diagnostic. Successful unload
restores an older provider. Do not replace loaded binaries on disk before unload.
CLI loading and its trust/configuration policy belong to the runtime owner;
installed SDK MODULE integration belongs to the root integration owner.

## MCP stdio tools

Enable `PANGU_SERVICES_MCP` (default ON) and link `PanGu::services_mcp`.
The optional target uses only the existing host/Core/libuv/yyjson closure.
`pg_mcp_stdio_plugin` installs one explicitly configured local stdio server:
`pg_mcp_stdio_config` supplies an argv vector including its executable, optional
absolute cwd, complete env (NULL inherits), unique service_name and tool_prefix,
and the production `pg_schema_validate` callback. No shell expansion occurs.
The public contract and ownership are in `include/pangu/services/mcp.h`.

Install synchronously spawns the process, negotiates initialize, sends
notifications/initialized, follows all tools/list pages and only then publishes
an ABI1 `pg_tools_service`. Unsupported versions, invalid tool declarations,
repeated cursors, duplicate names and partial discovery roll back the scope and
close/reap the process. A server can negotiate any of the four Pi snapshot
versions, from 2024-11-05 through 2025-11-25.

To keep default local tools, install all source scopes first, then call
`pg_tools_aggregate_install(root, PG_NAME(PG_TOOLS_SERVICE), sources, count,
&aggregate_scope)` from `tools.h`. Sources are ordered pg_dependency entries,
each ABI1: the existing pangu.tools followed by each MCP service_name (and other
tool groups when selected). The aggregate copies declarations and holds injected
leases on the sources. Duplicate tool names fail without publishing. The old
output binding is restored on failure/unload. Acquire the aggregate once per
Core run and release after the run; dispose aggregate before source scopes.
Source removal also automatically withdraws its aggregates before cleaning up.
The aggregate function lives in `PanGu::services`, with no extra target.

`examples/services/mcp.c` is an executable public API consumer. After building
`pangu_check_services`, an isolated fixture example from the repository root is:

```sh
mkdir -p "$PWD/build/services/mcp-demo"
build/services/pangu_services_mcp_example echo '{"text":"hello MCP"}' python3 "$PWD/tests/services/mcp-server.py" example "$PWD/build/services/mcp-demo"
```

It calls the remote tool through pangu_core_call_tool and prints its independent
copied result after releasing the lease and closing the server. The service
fixture additionally executes a model→MCP tool→model loop, local file tools in
the same aggregate, Schema preflight, structured and image results, RPC/tool
errors, cancellation, timeout, late responses and connection reuse.

Calls on one scope are serialized on the Core caller thread; poll supports
same-thread cancellation. Cancelled/timed-out requests send
notifications/cancelled except for initialize. Cancellation does not guarantee
remote rollback. Text and image blocks pass through, embedded resource text
becomes text, and empty content with structuredContent becomes pretty JSON.
The full remote result remains in details_json; rich structured_json survives
standalone call_tool and tool-end callbacks. Core intentionally omits that rich
field from its committed model transcript. Tool RPC errors preserve their
message/code/data (details_json); isError becomes result.is_error; timeout and
connection closure produce distinct safe tool errors. stderr is drained and
discarded. Malformed/oversize NDJSON lines are discarded through the next newline
without failing unrelated pending replies; the response frame limit defaults to
16MiB. Outbound write buffers remain owned until libuv completion.

Stop consumers and release leases before disposing. Cleanup closes stdin, then
escalates TERM/KILL for a stubborn POSIX process group, reaps the direct child and
quiesces pipe/write callbacks. Tests verify that a descendant cannot perform its
delayed filesystem side effect. Windows is explicitly unsupported until its
process-tree adapter exists; other platforms are unverified. The stdio transport itself has no authentication. MCP still lacks OAuth flows,
resources/prompts/roots/sampling, progress UI,
listChanged refresh or live tool reload; reinstall to rediscover. Server
instructions and annotations/outputSchema are not yet exposed/validated. Audio,
resource links and embedded binary resources currently retain full details but
use a generic unsupported-content placeholder, differing from Pi's richer
conversion. S07 remains unchecked. Runtime owns parsing product configuration,
trust decisions, naming and app/CLI lifecycle; stdio business logic stays here.


## MCP Streamable HTTP tools

Enable `PANGU_SERVICES_MCP_HTTP` and link `PanGu::services_mcp_http`.
This is independent of `PANGU_SERVICES_MCP`: HTTP-only builds share the same
`PanGu::services_mcp_client` discovery, Schema callback and result mapping as
stdio. The HTTP target adds libcurl >=7.85 (system 8.23.0, curl license).
Public configuration and lifetime rules are in `pangu/services/mcp.h`.

Install `pg_mcp_http_plugin` with a unique service name, tool prefix, URL and
`pg_schema_validate` callback. Headers and URL are copied during installation;
authentication/poll/validator contexts must survive scope disposal. Token and
unauthorized callbacks run on the calling thread and support one authentication
recovery for each request. They may cancel the Core; cancelled dispatch does not
reach the server. Close reuses the last token without calling either callback.
GET streams are pumped during calls; no background notification worker is
created. HTTP errors expose status, not remote response bodies, to avoid leaking
credentials into model output. Session expiry fails the request without replay.
SSE response recovery uses GET/Last-Event-ID without replaying tools/call.

```sh
build/services/pangu_services_mcp_http_example echo '{"text":"hello HTTP"}' http://127.0.0.1:8080/mcp
ctest --test-dir build/services -R '^services_mcp(_http)?$' --output-on-failure
```

The real local peer in `tests/services/mcp-http.py` covers JSON/SSE fragmentation,
session/version headers, two-page discovery, actual Core model/tool consumers,
rich results and error mapping, cancellation/timeout and reuse, authentication
refresh, stream resumption, server ping and lease-gated close. For installed SDK
consumers, run `python3 tests/services/mcp-http.py CONSUMER --installed`:
the executable receives the complete `/normal` URL as its sole argument. Use
`disable_get_stream=1`, headers `X-PanGu-Fixture: controlled` and
`Authorization: Basic Zml4dHVyZTpwYXNz`. Discover eight tools, call remote `echo`
once with `{"text":"installed HTTP reply"}`, then release leases and dispose.
The peer checks initialization, two pages, one call, one DELETE and no GET or
cancel; it does not run the full protocol suite in installed mode.

### Tool argument normalization

File and execution plugins configured with `validate = pg_schema_validate`
automatically use the C17 `pg_schema_normalize` path when Core runs their tools.
Optional non-nullable null properties are omitted, serialized JSON Schema
primitive types are converted, and Valijson validates the complete converted
value before Core calls execution. Raw model arguments remain in the transcript.
`pg_schema_validate` itself remains strict for existing independent callers.
An injected `validate_arguments` callback overrides the default conversion;
its context must outlive the plugin scope and its sink inputs are synchronous.

`pg_schema_normalize` accepts optional `pg_schema_diagnostics` with a borrowed
tool name and diagnostic sink. `pg_schema_normalize_tool` accepts the current
Core owner and raw call explicitly and reports through `pangu_core_report_error`.
The latter needs an owner-aware Core preflight callback; binding a current owner
through global/TLS state is unsupported. See `schema.h` for lifetime/status rules.

Direct source comparison (requires an existing TypeBox 1.3.27 installation):

```sh
cmake -S src/services -B build/services \
  -DPANGU_SERVICES_PI_SOURCE_NODE_MODULES=/absolute/node_modules
cmake --build build/services --target pangu_check_services -j2
ctest --test-dir build/services -L services --output-on-failure
```

The opt-in test copies the fixed reference `validation.ts` unchanged into the
build directory and runs the actual exported function against the native
normalizer. It never installs packages or edits `references/`. JSON C ABI tools
use the serialized plain-schema branch; TypeBox symbol/transform metadata is
not carried over by serialization. This does not claim the complete TypeBox
runtime/format vocabulary or every JavaScript scalar-number edge case.
