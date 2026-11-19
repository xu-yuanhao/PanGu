# PanGu TUI entry points

The product entry is the Agent CLI with the default presentation plugin:

```sh
build/product/src/agent/pangu_agent_cli create build/product/chat.jsonl --tui
build/product/src/agent/pangu_agent_cli open build/product/chat.jsonl --tui
```

Configure credentials in the process environment. Local acceptance can use the
runtime's private injector, without writing an environment file or exposing key
values:

```sh
python3 examples/agent/with-private.py .private/model-credentials.md -- \
  python3 tests/tui/live.py build/agent/pangu_agent_cli build/tui
```

The opt-in test uses a unique synthetic working directory beneath `build/tui`,
asks DeepSeek to write/read a synthetic file, cancels a long response, continues
on the same session, exits, and reopens the real TUI to check replay. It prints
only acceptance status; no terminal recording or credentials are written.
It is not part of deterministic CTest and requires network/model credentials.

`build/tui/pangu_tui_chat` is explicitly a deterministic Core embedding example,
with a real read-only README tool (`/tool`). It does not replace the product
model/provider/session combination. Its exact `{}` validator is only for this
fixed fixture; the product uses the service layer's mature Schema adapter.

```sh
python3 tools/integration/check.py tui
```

The eight registered tests are `tui_keys`, `tui_buffer`, `tui_editor`,
`tui_history`, `tui_terminal`, `tui_session`, `tui_session_history` and
`tui_resources`. The PTY test drives real terminal input and Core model/tool
execution; it includes immediate submission after cancellation and busy
steer/follow-up, not just an isolated mailbox. It also checks split paste,
window resize, EOF, SIGINT/TERM/HUP restoration and redirected-input diagnostics.

Enter sends (steers while running); Alt+Enter queues a follow-up; Shift+Enter
or Ctrl+J inserts a newline. Ctrl+C/Escape cancels a running response, Ctrl+D on
an empty editor or `/quit` exits. Ctrl+Z / Alt+Z undo/redo, Ctrl+P/N history,
Ctrl+R opens reverse search with a separate empty query; typing/pasting searches
submitted history using Unicode case folding. Ctrl+R/Up visits older distinct
matches; Ctrl+S/Down visits newer matches. Enter accepts a match as an editable
draft; a second Enter sends it. No-match Enter keeps search open. Backspace
removes a query grapheme; Ctrl+U clears it; Escape/Ctrl+C restores the complete
draft and selection without cancelling a running model. Tab and Alt+Enter are
consumed during search. Outside search, Tab completes a token. History retains
the newest 100 prompts, trims Unicode whitespace and skips adjacent duplicates.
Ctrl+P/N preserves the unsent draft, cursor and selection. The `tui_history`
PTY regression checks these keys through the actual terminal reader and chat
submit callback, including multiline/Unicode queries and Tab/search FIFO.
Shift+arrows select; Ctrl+W/U/K kill, Ctrl+Y yanks, Alt+Y rotates the kill ring.
Default slash completions are `/help` and `/quit`; hosts can provide additional
commands/resources via a completion catalog or callback. Resource discovery is
owned by the service plugin, not inferred by TUI. Tab holds later editing and
submission in a copied key mailbox until owner-thread poll applies completion.
Hosts must poll during provider waits before draining commands; cancellation
can bypass this barrier. Resource-enabled hosts expose `/reload`; typing
`/rel<Tab><Enter>` completes before submitting even within one terminal read.
The resource PTY regression covers trusted/denied projects, JSONL/SQLite,
reload/rollback, busy queues, cancellation and reopening.

Saved input history is an explicit driver capability: append
`pg_tui_actions.input_history` and emit certified original USER text through its
synchronous sink in chronological order. Startup stages and atomically installs
that source before entering the terminal; failure preserves existing history,
while `PANGU_TUI_UNSUPPORTED` skips import. Missing callbacks also skip import.
The source must not submit inputs or invoke a model. All imports use the same
trim/dedup/100-entry policy as live prompts. Ordinary transcript rebuilds do not
re-import history, following Pi's `renderInitialMessages(populateHistory=true)`
and `rebuildChatFromMessages()` distinction.

For a host that can prove its saved session contains unexpanded original USER
input, `pg_tui_history_user_messages(pg_session_messages(session), sink, ctx)`
concatenates only USER TEXT blocks, ignoring assistant/tool/system/image content.
The `history-jsonl`/`history-sqlite` modes in `tests/tui/session.c` demonstrate an
actual direct-session caller with that guarantee. `tui_session_history` drives
create/open through real PTYs, verifies draft/selection restoration and Unicode
search, and compares committed snapshots to prove browsing does not resend input.

The default app supplies `input_history` from the runtime's persisted original
USER provenance. JSONL/SQLite reopen restores plain, template and skill commands
as originally entered; resource-expanded model text and pending queues are
excluded. Legacy sessions without original provenance decline import. The
resource PTY checks restored raw commands, multiline Unicode, draft selection,
adjacent duplicate navigation, pending exclusion and unchanged committed messages
after browsing. Runtime owns provenance persistence and acceptance/placement.

`pangu/tui/plugin.h` exports `pg_tui_plugin()` and service `pangu.tui` ABI1.
It depends on the session service and exposes start/run/poll/stop/chat.
Installation pins the session; cleanup stops/joins input, unsubscribes and frees
presentation state before the session dependency lease is released. The run
thread must invoke poll during provider waits. Input-thread callbacks only edit,
render and queue; they never call Core or session APIs. Cancellation and
`agent_end` defer new input until the execution call has actually returned.

CMake targets are `PanGu::tui`, `PanGu::tui_plugin`, `pangu_tui_chat` and
`pangu_check_tui`. The root includes `src/tui/CMakeLists.txt`; the product owner
links the plugin with Agent/services. Core remains independently linkable.
Terminal/editor uses system utf8proc 2.12.0 (MIT/Unicode notices,
https://github.com/JuliaStrings/utf8proc, `PkgConfig::TUI_UTF8PROC`) and libuv
1.52.1 (MIT, https://github.com/libuv/libuv, `PkgConfig::TUI_UV`). POSIX terminal
also links `Threads::Threads`. Root dependency/install/license records are
maintained by the integration owner.

Linux x86-64 PTYs are verified. POSIX is shared with macOS but macOS was not
compiled or run. The separate Win32 console adapter is implemented but was not
compiled or run here. Native IME, clipboard, complete Pi editor word segmentation,
completion menus and the full U04 renderer are not established by these tests.

## Installed C ABI SDK

The root installs the base SDK as `PanGuTUI`, target `PanGu::tui`, export
`lib/cmake/PanGuTUI/PanGuTUITargets.cmake` (respecting GNUInstallDirs).
Its link interface is exactly `PkgConfig::TUI_UTF8PROC`, `PkgConfig::TUI_UV`,
and `Threads::Threads` on non-MSVC builds. No Core runtime or Agent library is
required by the base implementation. `view.h` uses Core message/event types;
the full root install supplies the Core headers alongside the TUI headers.

When Agent is selected (`PANGU_BUILD_AGENT=ON`, or the Agent standalone
`PANGU_AGENT_TUI` combination), the separate `PanGuTUIPresentation` SDK installs
`plugin.h`, `libpangu_tui_plugin.a`, and
`lib/cmake/PanGuTUIPresentation/PanGuTUIPresentationTargets.cmake`.
Its target `PanGu::tui_plugin` publicly links `PanGu::tui`, `PanGu::core`,
`PanGu::agent`, and `PanGu::services`. Core/Services are direct symbol
dependencies; Agent supplies the public session ABI and its dependency closure.
The plugin dispatches session operations through a service table; linking it
does not install the session service at runtime. Install the session plugin
before the TUI plugin as described above.

In a TUI-only build, the plugin static archive still compiles for interface
checks, but its library/header/export are not installed without Agent.
Installation components are `TUI` and `TUIPresentation`; package Config files,
Core headers, dependency discovery and license notices are provided by the root
integration owner. The base Config discovers PkgConfig modules with the exact
names above and Threads on non-MSVC. The presentation Config discovers
PanGuTUI, PanGuAgent and PanGuCore before including its export. External
consumers enable C and link the imported target; no source-tree paths belong in
the installed interface.

## Explicit tool approval composition

The existing app policy remains disabled until the host selects it while idle:

```c
/* After pg_agent_app_open, before pg_agent_app_present. */
if (pg_agent_app_set_approval(app, PG_APP_APPROVAL_ASK) != PG_OK)
    return 1;
pg_status status = pg_agent_app_present(app, pg_tui_plugin(), &config, sizeof config);
```

The default presentation scope registers the services approval provider. Each
selected tool invocation commits asked/decided via the app bridge and displays
readonly tool name, call ID and reason. Press `y` to allow once, `n` to reject,
Escape or Ctrl+C to cancel; Enter and pasted text never authorize. Approval
input preserves the editor draft and stays outside USER history. Cancelled
sessions remain usable. Reopening requires selecting ASK again and never reuses
a recorded grant. `PG_APP_APPROVAL_NEVER` durably rejects without asking.
Stop revokes before leases/scope cleanup; active synchronous answer callbacks
must return before the host disposes their scope. No callback retains slices.

`tests/tui/approval.py` runs real PTYs against the default app/presentation and
an ordinary file-writing tool, with deterministic model protocol events. It
checks JSONL/SQLite effects, rejection/cancellation, fresh requests, durable
audit/history isolation and terminal restoration. It is not a live-model test.
