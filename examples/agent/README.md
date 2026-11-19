# Agent runtime: R01 and the R02 Core driver

Public C17 ABI: `pangu/agent/session.h`, `plugin.h`, `app.h`.

```sh
cmake -S src/agent -B build/agent -DCMAKE_BUILD_TYPE=Debug -DPANGU_AGENT_TUI=ON
cmake --build build/agent --target pangu_check_agent -j 4
python3 tools/integration/check.py agent --build-dir build/agent
build/agent/pangu_agent_cli create build/agent/demo.jsonl --demo 'hello'
build/agent/pangu_agent_cli open build/agent/demo.jsonl
build/agent/pangu_agent_cli open build/agent/demo.jsonl --tui-demo
```

Real CLI/TUI composition (DeepSeek + registered read/write tools + production
Schema), with private credentials injected only into the child environment:

```sh
python3 examples/agent/with-private.py .private/model-credentials.md -- \
  build/agent/pangu_agent_cli create build/agent/live.jsonl --deepseek 'your task'
python3 examples/agent/with-private.py .private/model-credentials.md -- \
  build/agent/pangu_agent_cli open build/agent/live.jsonl --tui --cwd /absolute/workdir
```

`--sqlite` selects the SQLite snapshot backend. Repeated `--deepseek TEXT`
arguments run consecutive prompts in one session. Environment configuration:
`DEEPSEEK_API_KEY`, optional `PANGU_MODEL`, `PANGU_DEEPSEEK_ENDPOINT` (complete
chat/completions URL), `PANGU_CONTEXT_TOKENS`, `PANGU_RESERVE_TOKENS` (16384),
`PANGU_KEEP_RECENT_TOKENS` (20000). A supplied context capacity enables persisted
automatic compaction; reserve must be smaller than capacity. Without a model
capacity the policy remains disabled. Existing persisted policy survives reopen.
`--no-tools` explicitly omits file capabilities; otherwise real CLI mode requires
the production Schema bridge. SIGINT is consumed by a sig_atomic_t flag; provider
progress checks it and cancels Core on the caller thread. TUI input uses its own
mailbox, with the same caller-thread progress bridge.

`--demo` / `--tui-demo` deliberately use a deterministic model. Do not write credentials into messages/documents. The
runtime never serializes loop configuration, callback contexts or API keys.
The CLI example's plain transcript output is intended for trusted demo input;
the interactive TUI escapes untrusted terminal control characters.

The parent project can use `add_subdirectory(src/agent)` after defining
`PanGu::core`; the directory reuses `PanGu::services` when present. It exposes
`PanGu::agent`, `PanGu::agent_app`, `pangu_agent_cli`, `pangu_check_agent`.
TUI CLI support is enabled when `PanGu::tui_plugin` already exists. The
independent build configures the root as an excluded Core dependency and builds
only targets needed by the agent check, not all Core/services/TUI tests.

## Consumer contract

`pg_session_open` has exclusive create and existing-only open. A missing open
returns `PG_SESSION_NOT_FOUND` with NULL handle and creates no file. Parent
folders must already exist. Read-only mode forbids transactions, run and queue.
Each writable path must have a single owner; external hosts serialize concurrent
opens. All API calls, subscriptions, Core callbacks, cancellation and queue
injection run on that owner thread. The UI reader must only publish commands
into a separate synchronized queue. The provider's same-thread progress/pump
callback drains that queue; it must not block or invoke a second run.

A transaction uses begin/append/set_documents/commit, or rollback. Input is
copied. Read APIs expose only committed messages/documents. JSON documents are
latest-only objects. Commit writes the complete candidate, adopts it, then
publishes `COMMITTED`; an IO failure leaves the draft available for retry and
publishes nothing. Close cancels no running thread: it returns BUSY until the
synchronous run settles, rolls back an idle draft, emits CLOSED, and closes its
storage. Destroy is ignored during run/event dispatch. Snapshot pointers expire
at the next commit or close.

The default `pangu.agent.core` plugin installs `pangu.session` ABI1 and owns its
selected storage and Core driver until scope cleanup. A service lease pins that
lifetime. `pg_agent_app_open` installs requested plugins then this session
plugin. `pg_agent_app_run` queries `pangu.model` ABI1 when the caller supplied no
explicit provider, holding its lease until return and retaining the caller's
independent `loop.ctx`. A NULL/zero tool set resolves optional `pangu.tools`;
explicit arrays pass through unchanged. CLI leases that same file-tool array for
DeepSeek declarations and Core execution, preventing a divergent tool list.

`pg_agent_app_present` installs a presentation plugin after the session, acquires
`pangu.tui` ABI1, calls start/run/stop, releases its lease and disposes the scope.
The TUI's session dependency lease and subscription are released before session
cleanup. `pg_agent_app_poll` is a same-thread UI poll bridge; service adapters
with a status-returning poll should wrap it and return `PANGU_OK`. A model lease
is held while the UI runs. Stop joins the reader before contexts are released.

The bridge saves each final `message_end` before emitting its `LIVE` event. The
per-run Core event callback preserves its original `ctx` and runs after that
save, before session LIVE listeners. Streaming/start/tool updates remain
provisional. Failed persistence cancels Core and returns the storage error;
providers must check cancellation and stop on a failed sink. Already committed
prefix messages survive. Core failure/aborted messages remain actual transcript
artifacts. The optional compaction policy can continue an interrupted model
request once after a committed summary; completed tools are not replayed.

`COMMITTED` is not a second display event. The real TUI adapter consumes LIVE
and replays `pg_session_messages` on startup. `examples/agent/cli.c` shows the
same subscription and replay pattern without TUI dependencies.

## Native storage adaptation and limits

JSONL v1 stores a revision-zero header and one complete JSON snapshot per
commit. A same-directory exclusive temporary file is written/fsynced/closed,
then replaces the log with libuv rename. A failed pre-rename operation leaves
the committed file untouched. All revisions are validated on open; a malformed
or truncated line returns CORRUPT and its one-based line without repair or
mutation, including read-only opens. This is a PanGu checkpoint representation,
not Pi's sidecar format. Directory fsync/power-loss durability, concurrent
writer locking and interrupted-create recovery are not established. Full
snapshots retain history at quadratic storage cost; compaction/indexed Durable
records remain later work. No automatic salvage of a torn tail is performed.

The SQLite snapshot adapter uses an existing-only connection or exclusive
create, a single versioned envelope in `pangu_session`, and BEGIN IMMEDIATE /
COMMIT before adoption. This verifies the actual SQLite link and transactional
snapshot behavior; it is not the full R03 Durable tables, task scheduler or
Memory implementation. JSONL and SQLite do not share disk formats.

All Core message fields are encoded, including tools/sections, binary-safe
length-delimited content, usage/costs, UINT64 duration, optional redacted,
cache-write-1h, reasoning and provider-total provenance. The newly appended
optional fields decode absent in older v1 snapshots as absent, preserving
explicit zero/false in new snapshots.

USER steering/follow-up mailboxes and ONE/ALL modes persist on both backends.
CLI controls: `--steer TEXT`, `--follow-up TEXT`, `--queues`, `--resume`,
`--resume-demo`, `--clear-queue steering|follow-up`. Each finalized input and
its removal from accepted mailboxes commit together; unplaced ALL-batch suffixes
survive reopen. Active enqueue copies into Core before storage; storage failure
uses public allocation-free `pangu_core_truncate_queue` to retain the prior
unselected prefix. No success notification is published on failure. Reentrant
acceptance notifications stay FIFO. Prepared-hook and durable mailbox mixing
currently returns INVALID (run) or BUSY (enqueue). This is default-driver input
durability, without Durable requestId deduplication or task resumption.
Deferred commits, schema/versioned document families,
crash recovery of running work and Core prepared-hook replay are not implemented
by this default driver. The optional Durable driver remains R03/R04. The
resource-backed CLI/app flow below is implemented; full default-product/C05
acceptance still includes the outstanding TUI and provider/resource workflows.

## Tree and context compaction (R05 in progress)

`pangu/agent/tree.h` exposes immutable entries (monotonic id, parent), active
leaf, branch navigation, reset/handoff, manual/model summary, persisted automatic
policy and archived usage. Snapshots append an optional `tree` and
`compaction_policy`; legacy v1 files without them are migrated in memory, then
written on the next commit. Both snapshot backends retain inactive branches.
Reopen validates parent order, marker references and the active context against
the serialized message view. A branch switch moves the leaf; the next message
forms a child, preserving the abandoned path. Reset retains system/tool baseline.

CLI commands execute in argument order:

```sh
build/agent/pangu_agent_cli open FILE --tree --usage
build/agent/pangu_agent_cli open FILE --switch ENTRY_ID --demo 'new branch'
build/agent/pangu_agent_cli open FILE --compact-summary FIRST_KEPT_ID 'host summary'
build/agent/pangu_agent_cli open FILE --handoff 'continue with this context'
# The model-backed variant uses the same credential wrapper as above:
build/agent/pangu_agent_cli open FILE --compact KEEP_RECENT_TOKENS --deepseek 'continue'
```

Compression retains an immutable marker with the source Durable summary wrapper
and a first-kept anchor. Active context is replayed system state + summary +
kept suffix; old entries remain navigable and disk history is not reclaimed.
Cuts are user/assistant boundaries and reject a user injected between a tool
call and its later result. Summary generation uses the source prompts, omits
thinking/system messages from its serialized conversation, caps tool text, forces
`tool_choice=none`, installs no executable tools, and accepts clean STOP text
only. Cancel/error/length leave the session unchanged; an explicit retry is a
new call to the same API. Usage of a successful summarizer is persisted once.

Token selection follows `ai/src/utils/estimate.ts`: UTF-16 code-unit counts via
utf8proc, divided by 3.5, and model usage only when it describes a prefix newer
than the last summary marker. Image estimates are 4800 characters. Tool-schema
JSON uses yyjson; raw tool-call JSON text can conservatively retain whitespace
rather than reconstructing JS numeric serialization. The summary serializer
uses JSON arguments directly and keeps UTF-8 scalar boundaries when truncating.

Tree mutations emit `PG_SESSION_CONTEXT_CHANGED` after persistence, then
COMMITTED. Service ABI1 appends size-checked entry/leaf/switch/reset/compact/usage
slots. `pg_agent_app_compact` resolves and leases the model just like run.
The current TUI does **not** implement tree/compact commands or context-change
presentation: TUI owner tracks them in U05. Durable document fork policies,
named branches, overflow retries, per-branch model properties
and full file-operation compaction metadata remain unimplemented; R05 stays open.

## Executed evidence

Linux x86-64: agent deterministic suite 10/10 (including real PTY lifecycle,
JSONL/SQLite tree switching, host summaries, model-summary cancel/length failure,
automatic policy, Unicode estimate, explicit CLI command order and reopen,
durable queues, reentrant FIFO, ALL-batch partial placement, borrowed-input
ownership and failed active enqueue preserving the existing prefix).
Private process injection was verified without printing values. Actual DeepSeek
CLI performed write/read, a second prompt/read, saved/reopened; then migrated the
old log, generated a real summary, and continued successfully. The TUI owner
separately ran `tests/tui/live.py`: real write/read, long-stream cancel, same-session
continuation, /quit, true TUI reopen replay, terminal restoration and exclusion
of the API key from persisted bytes. These are distinct from parent-reported
whole-product ASan results, which predate this R05 tree change. Agent-only
queue ASan/UBSan/leak checks pass; the reused Core archive is not instrumented.
No other target
platform was executed here.

## Sources and actual dependencies

Pi snapshot `d5b3de22d0e24a3e9c38a9410d3329b099717283`, MIT (root
`third-party/Pi-LICENSE`): Durable `src/types.ts`, `entries.ts`, `documents.ts`,
`session/{session,transaction,forks}.ts`, `harness/{context,compaction}.ts`, `storage/jsonl/{storage,index}.ts`; tests
`jsonl-storage.test.ts`, `session-deferred-commits.test.ts`,
`session-watches.test.ts`. Chord `src/publish.ts` supplies adoption-before-
publication/FIFO rationale; this synchronous API forbids reentrant writes
instead of exposing Promise-held mutation transactions. Coding Agent
`src/core/{sdk,agent-session,model-runtime,session-manager}.ts`, `compaction/{compaction,utils}.ts`, tree-traversal and compaction tests supply session ownership and
final-message save boundaries. Its extensible runtime and Durable's deferred
persistence are not ported wholesale. Source references are read-only.

| Dependency | Verified version / source | License | Link consumer |
|---|---|---|---|
| yyjson | vendored 0.12.0, github.com/ibireme/yyjson | MIT | `pangu_agent` uses symbols embedded by `pangu_core` / `pangu_json`; headers private |
| SQLite | system 3.54.0, sqlite.org | public domain | `pangu_agent` private `SQLite3::SQLite3` (old CMake alias fallback) |
| libuv | system 1.52.1, libuv.org | MIT | `pangu_agent` private `PkgConfig::PG_AGENT_UV`; actual file IO/fsync/rename |
| utf8proc | system 2.12.0, github.com/JuliaStrings/utf8proc | MIT/Unicode | `pangu_agent` private `PkgConfig::PG_AGENT_UTF8PROC`; actual Unicode estimation |
| production Schema | Valijson 1.1.3 + picojson 1.3.0, sources fixed by services vendor/README | BSD-2-Clause | real CLI `PanGu::services_schema` C++17 bridge only |
| DeepSeek/files | libcurl 8.23.0 + existing yyjson/libuv | curl license/MIT | real CLI `PanGu::services_deepseek` + `PanGu::services_tools` |
| services host | PanGu S01 | project + upstream notices | `pangu_agent_app` public `PanGu::services` |
| optional TUI | PanGu U01–U03 | project + upstream notices | CLI `PanGu::tui_plugin`, transitive utf8proc/libuv |

The integration owner maintains root dependency/install/license records. Linux
x86-64 is the only platform executed here. Windows/macOS/mobile remain untested.

## Settings and prompt resources (R02/R06 CLI/app)

With the default `PANGU_SERVICES_RESOURCES=ON` service target, CLI installs
`pg_resources_plugin` before the session. User files come from
`~/.pangu/agent` (`PANGU_AGENT_DIR` or `--agent-dir DIR` overrides it); project
files come from `--cwd DIR` (current directory by default). Project settings,
SYSTEM.md and resources are enabled by explicit `--trust-project`. The CLI
passes that decision without prompting or storing an implicit trust decision.
Service discovery, precedence, frontmatter and template syntax are owned by
`pangu/services/resources.h`; the runtime does not duplicate their loaders.

For example, put `{"defaultProvider":"deepseek","defaultModel":"deepseek-chat"}`
in the user `settings.json`, instructions in `SYSTEM.md`, and
`Please review $1` in `prompts/review.md`. Run:

```sh
build/agent/pangu_agent_cli create build/agent/resource-session.jsonl \
  --agent-dir /absolute/user/.pangu/agent --cwd /absolute/project \
  --trust-project --demo '/review "the parser"' --reload \
  --demo '/skill:example check ownership'
```

`--demo` is deterministic. `--deepseek TEXT` uses the actual selected model
service; its key and optional endpoint still come from the process environment.
`PANGU_MODEL` explicitly overrides `defaultModel`; the explicit `--deepseek`
composition fixes the provider/protocol. A missing DeepSeek model retains the
existing `deepseek-chat` default. `defaultThinkingLevel` supplies the level when
the caller leaves it OFF. SDK hosts select/install their own compatible provider;
settings do not silently switch provider implementations or network endpoints.

`pg_agent_app_run` acquires the optional resource service and an owned immutable
snapshot for the run. It fills unset model fields from settings and expands
USER text: `/skill:name args` becomes Pi's skill/location/base-directory block,
then `/template args` uses service expansion. Unknown commands pass through.
Explicit skill commands work for skills excluded from model discovery.
Expanded text is the committed user input, so it survives JSONL/SQLite reopen.
Queued CLI inputs expand before acceptance, retaining their accepted text.

The request adapter puts the snapshot's system prompt in a temporary model view,
replacing conversational SYSTEM messages without adding persisted context or
extra UI message events. The original cancellation owner, caller `loop.ctx`,
provider context, tools and hooks are preserved. Raw-state legacy `.model`
callbacks cannot represent that request view and are rejected when resources
are installed; use `.request`, `.request_events`, or a model service instead.
Automatic and explicit compaction keep the source's summary-specific system
prompt. Presentation delegates execution through the optional `pg_tui_actions`
table; each submit/compact acquires its own model/tools/resources and holds them
until that request returns. Busy enqueue uses that request's snapshot. TUI sends
raw input once, so accepted expanded messages are not expanded a second time.
`/reload` is available while idle and refreshes the next request; while a request
is running it is rejected rather than sent or queued as model input.

`pg_agent_app_reload_resources` and `--reload` reload outside a request. A failed
reload retains the prior service snapshot; a successful reload affects the next
request, without modifying old persisted input. `pg_agent_app_expand` exposes
same expansion for a host's queue; it returns malloc bytes, released by `free`.
`pg_agent_app_enqueue` provides the complete expand-and-durably-enqueue operation
for CLI, UI actions and same-thread provider/listener callbacks. It retains
message metadata/non-text blocks, and propagates the session's storage/cancel
semantics. During a run it expands against the active snapshot, including when
files change on disk. Without a resource service expansion copies input and
reload returns NOT_FOUND.
With the resource target disabled, app has no resource/YAML link dependency,
and CLI resource-specific options return a capability diagnostic.

Reference: Pi `d5b3de22d0e24a3e9c38a9410d3329b099717283`,
`core/agent-session.ts` prompt/_expandSkillCommand/reloadResources,
`core/sdk.ts`, `core/settings-manager.ts`, and `core/resource-loader.ts`.
The native snapshot provides the skill body at reload time rather than doing a
second file read during expansion. No resource command executes shell code.

`agent_resources` runs the real app/model-service chain and CLI with a local
synthetic streaming model protocol. It checks settings precedence, explicit
trust, templates/skills, reload success/failure, separate contexts/key hook,
automatic summary prompt preservation and both-backend reopen. It sends no
external requests and uses only synthetic credentials.

`pg_agent_app_present` accepts the old `pg_tui_config` prefix ending before
`actions`, full current configs, and larger future configs. It zero-initializes
its local copy and never reads beyond the supplied size; partial tails are
rejected. It injects app-owned actions. App `presenting` protects installation,
start, run, stop and disposal; close and recursive present/public run/compact
remain blocked throughout. Action callbacks run only during the UI run phase,
with a separate request busy flag for run/compact and reload. The default TUI
still supports direct-session hosts without these optional actions.

The resource fixture also runs `pangu_agent_presentation_test` for both backends:
exactly allocated legacy config prefix, truncated-tail rejection, caller/provider
contexts, busy steering/follow-up snapshots, idle reload/rollback, compaction,
cancellation/reuse, reopen, and start/run/install failure cleanup guards. Real
terminal interaction is verified by the TUI owner's `tui_resources` PTY test.

### Request credentials

With resources enabled, CLI installs the resources plugin and app queries its
separate `pangu.credentials` service. User `auth.json` in `--agent-dir` /
`PANGU_AGENT_DIR` / `~/.pangu/agent` and provider environment values are read
on each model request, including tool continuations and compaction. CLI no
longer rejects a missing `DEEPSEEK_API_KEY` before opening the app. Project
`.pangu/auth.json` is never a credential source, even with `--trust-project`.
A malformed/unreadable user auth file stops the request with a generic error.
Credentials are excluded from resource snapshots, messages and logs.

For SDK callers, explicit `loop.get_api_key` keeps its original `loop.ctx` and
takes priority over the credentials service. An empty explicit result, an
empty service result, or service `PG_NOT_FOUND` uses Core's borrowed
`loop.api_key` fallback. Provider `request_ctx` stays separate. The app retains
the service lease through run/compact and wipes its malloc key after Core has
copied it; Core wipes its own request copy. The independent service works
without a resources snapshot. Resolvers run on the synchronous run thread;
nested app calls restore the outer credentials binding.

Resource-OFF CLI still reads `DEEPSEEK_API_KEY` per request. Shell credentials
and OAuth are not implemented. Existing local HTTP fixture can verify that
combination without new scripts:

```sh
python3 tests/agent/resources.py --env-only build/agent/pangu_agent_cli
```

### Overflow recovery

With `PANGU_BUILD_OVERFLOW=ON`, the execution plugin owns the optional
`PanGu::overflow` classifier and publishes `pangu.context_classifier` ABI1.
App acquires this capability for each run and keeps its lease, model/tool
leases and resource snapshot through summarization and continuation. The
installed static `PanGu::agent_app` export then needs `PanGuOverflow`; base
`PanGu::agent` does not link the regex library. A composition can replace the
service, or a direct session host can inject the table from
`pangu/agent/recovery.h`. Explicit session injection wins. Classifier INVALID
and NOMEM terminate recovery, never becoming a false/non-overflow result.

Configure a nonzero model context with the persisted compaction policy (CLI:
`PANGU_CONTEXT_TOKENS`, `PANGU_RESERVE_TOKENS`, `PANGU_KEEP_RECENT_TOKENS`).
An overflow error or recoverable LENGTH response is archived, then excluded
from active context with an append-only omission entry. Summarization uses the
existing source prompt and no executable tools. A successful summary with a
real cut allows one model continuation; a second failed attempt stays visible.
Successful replies with excessive input usage compact without replaying the
reply. New USER input resets the recovery budget. Original
`model_config.metadata_json.maxTokens` supplies the desired output bound; a
context-clamped limit is not substituted for it.

Cancellation and persistence failure stop subsequent phases. An omission that
already committed survives a failed/cancelled summary; a summary that already
committed survives cancelled/failed continuation. Original attempts and usage
remain in the archive and JSONL/SQLite reopen preserves the same projection.
`--tree` reports `kind=omission`; old readers predating that entry cannot open
new omission-bearing snapshots.

Without the overflow target, numeric usage and LENGTH recovery remain enabled
by policy; provider error text is never parsed in Agent. DeepSeek's safe HTTP
413 diagnostic is classifiable when the target is selected; generic HTTP 400
is not interpreted as overflow. No model credentials or provider response body
are copied into recovery metadata. Actual HTTP, repeated truncation, summary
failure, key rotation through summary/retry and both backends are covered by
the existing `tests/agent/resources.py` fixture; OFF mode also checks numeric
recovery using its `--env-only` entry. Deterministic cancellation, commit
failures and reopen/continue are in `tests/agent/tree.c`.

Explicit local native plugins are loaded with repeatable `--plugin LOCAL_PATH`:

```sh
pangu_agent_cli open session.jsonl --cwd /absolute/project \
  --plugin /absolute/plugins/provider.so --deepseek 'hello'
```

Each path explicitly authorizes execution of trusted native code. Relative paths
resolve against `--cwd` (the current directory by default); the service loader
canonicalizes them, without library-name search or package installation. Plugins
load after defaults, in argument order, and must accept NULL/0 configuration.
The loader's bounded stage diagnostic reports failure; no input is run after a
load failure. Previously loaded scopes are cleaned up when the app closes.

SDK hosts call `pg_agent_app_load_plugin` while idle and may provide a plugin's
own configuration. Model/tool registrations use the host's replacement semantics;
a new `pangu.tools` service replaces the previous array. To combine tools, the
providing plugin must publish the desired combined service. Explicit caller loop
callbacks/tools retain their existing precedence over service discovery.
`--deepseek` selects the service-based execution path, so an explicitly loaded
model can replace the default provider without network access; `--demo` always
uses its explicit deterministic callback. `--no-tools` skips the built-in file
plugin; a native plugin can still provide tools.

The app acquires model/tool leases through each synchronous run. Close follows
run completion and releases leases before host disposal, plugin cleanup and
library unload. Paths and plugin configuration are not saved in the session;
reopening history does not execute a plugin again. Discovery, arbitrary CLI
plugin configuration and hot reload are separate remaining operations.

Generated compactions retain Pi file-operation metadata: attempted `read`,
`write` and `edit` calls (including failed operations), plus producer-recorded
`details_json.nestedCalls.calls`, contribute to `readFiles`/`modifiedFiles` and
`<read-files>`/`<modified-files>` summary suffixes. Modified paths suppress reads;
lists use source JavaScript UTF-16 ordering. Only the summarized prefix and the
previous generated summary contribute. Retained tails wait for the next summary;
inactive branches do not contribute. Host-supplied summaries and reset start a
new metadata history. JSONL and SQLite persist both details and XML; no actual
filesystem scan or path normalization is implied. Model response identity fields
`response_model`, `response_id` and `raw_stop_reason` persist as length-delimited
bytes; older records without them read as empty fields.

MCP stdio and Streamable HTTP servers use an explicitly selected trusted configuration file:

```sh
pangu_agent_cli create session.jsonl --cwd /absolute/project \
  --mcp-config mcp.json --deepseek 'use local and remote tools'
```

```json
{"mcpServers":{"docs":{"command":"/absolute/server","args":["--stdio"],"cwd":".","timeout":30},"unused":{"command":"/absolute/other","enabled":false}}}
```

`--mcp-config` authorizes executing these local commands. No user/project MCP
file is loaded automatically, including with `--trust-project`. The whole file
is validated before any child starts. `command` and `args` form an argv vector,
without a shell. Config and per-server relative cwd use the session `--cwd`.
Names use letters/digits/underscore/hyphen; `-` becomes `_` in the direct tool
namespace `mcp__server__tool`, and namespace collisions are rejected. `timeout`
is a positive number of seconds (default 30). For local commands optional `type` is `stdio`;
the child inherits the process environment. HTTP entries use `url` with http(s),
optional `type` (`http` or `streamable-http`), and optional string `headers`:

```json
{"mcpServers":{"remote":{"url":"https://example.com/mcp","headers":{"Authorization":"Bearer ${MCP_TOKEN}"}}}}
```

Header values expand `$NAME` and `${NAME}` from the process environment
(ASCII names `[A-Za-z_][A-Za-z0-9_]*`). `$$` emits a literal `$`, and `$!`
emits a literal `!`; backslashes do not escape interpolation. Invalid braced
names and unclosed braces remain literal, and expanded values are not parsed
again. Missing or empty variables fail the entire configuration, including in
disabled entries, before any server connects; no header is silently omitted.
CLI errors remain generic and do not echo header values. Command values starting
with `!` and resolved CR/LF are rejected; `$!` may produce a literal leading `!`. HTTP entries cannot
contain command/args/cwd. HTTP support requires `PanGu::services_mcp_http` in the
CLI build; enabled HTTP entries otherwise fail before any server starts.
This explicit direct subset rejects env/auth/exposure and unknown fields;
OAuth discovery/refresh and automatic configuration discovery remain unavailable. Diagnostics do
not print configuration contents or child stderr.

Each enabled server installs the Services MCP plugin under its own service name.
The Services aggregate retains the existing local/native `pangu.tools` and every
MCP tool array; duplicates fail instead of shadowing local tools. The app's normal
run lease pins this complete array through Core and model continuations. Polling
allows UI processing and Core cancellation during remote tool waits. A later
startup failure closes all already-started servers before returning. Close
releases consumers before aggregate/source cleanup and child reap. Reopening
without `--mcp-config` reads history without starting servers. MCP=OFF reports
an explicit capability error before session creation. SDK hosts install the
same `pg_mcp_stdio_plugin` and `pg_tools_aggregate_install`, as exercised by
`tests/agent/mcp.c`; the app library itself has no optional MCP link dependency.
