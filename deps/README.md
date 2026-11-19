# Linked dependency sources

| Component | Version / upstream | License | Actual consumer |
|---|---|---|---|
| libcurl | system 8.23.0 verified on Linux; minimum API 7.85 | curl license, system package | selected pangu_proxy_http, pangu_services_deepseek and pangu_services_mcp_http; HTTP/TLS and cancellation |
| libuv | system 1.52.1 verified on Linux; minimum 1.44, https://github.com/libuv/libuv | MIT, libuv/LICENSE | optional pangu_services scope/lease synchronization and native library loading; selected services_mcp stdio process/pipes; link PkgConfig::PANGU_UV |
| SQLite | system 3.54.0 verified on Linux, https://sqlite.org/ | Public domain, sqlite/LICENSE | optional pangu_agent session backend; CMake SQLite3::SQLite3 (FindSQLite3 target compatibility alias), not Core |
| utf8proc | system 2.12.0 verified on Linux, https://github.com/JuliaStrings/utf8proc | MIT/Unicode notices, utf8proc/LICENSE.md | selected pangu_tui editor Unicode segmentation/width and pangu_agent token estimates; PkgConfig::TUI_UTF8PROC / PG_AGENT_UTF8PROC |
| Valijson | 1.1.3, https://github.com/tristanpenman/valijson/tree/v1.1.3 | BSD-2-Clause, src/services/vendor/valijson/LICENSE | selected pangu_services_schema only; C++17 implementation behind C ABI for real file-tool Schema validation |
| picojson | 1.3.0, https://github.com/kazuho/picojson/tree/v1.3.0 | BSD-2-Clause, src/services/vendor/picojson/LICENSE | private Valijson adapter with PICOJSON_USE_INT64; not Core |
| GdkPixbuf | system 2.44.8 verified, minimum 2.42, https://gitlab.gnome.org/GNOME/gdk-pixbuf | system package LGPL-2.0-or-later | optional PanGu::services_images -> PkgConfig::PANGU_PIXBUF; PANGU_SERVICES_IMAGES defaults OFF, excludes codec/GLib/glycin closure from basic tools |
| PCRE2 | system 10.49 verified, minimum 10.43, https://www.pcre.org/ | BSD notices and PCRE2 exception, pcre2/ | optional PanGu::overflow -> PkgConfig::PANGU_PCRE2 (libpcre2-8); absent from base Core |
| libgit2 | system 1.9.7, minimum 1.8, https://libgit2.org/ | GPL-2.0-only with linking exception and bundled notices, libgit2/COPYING | selected services_resources ignore-rule matching, PkgConfig::PANGU_GIT2; disabled with resources |
| libyaml | system 0.2.5 verified on Linux, https://pyyaml.org/wiki/LibYAML | MIT, libyaml/LICENSE | selected PanGu::services_resources -> PkgConfig::PANGU_YAML; YAML frontmatter for skills and prompt templates; PANGU_SERVICES_RESOURCES defaults ON |
| yyjson | 0.12.0, https://github.com/ibireme/yyjson/tree/0.12.0 | MIT, yyjson/LICENSE | Core transcript tool-schema parsing/comparison; object code is included in pangu_core |

The three yyjson files were copied unchanged from the 0.12.0 tag. No build-time
network access or system yyjson installation is needed. Its license is installed
with Core. Core has no external shared-library dependency beyond the C runtime.

SHA-256:

```
ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a  yyjson/yyjson.c
175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3  yyjson/yyjson.h
45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947  yyjson/LICENSE
```

The service host uses the system libuv found through pkg-config when PANGU_BUILD_SERVICES is enabled. Its verified Linux version is 1.52.1; the copied license comes from that system package. Core-only configuration leaves services disabled and does not query or link libuv. The root installs PanGuServicesConfig and verifies a C-only installed-package consumer for host/Schema/model service discovery. Cross-platform discovery and optional image loader deployment remain integration work.

Agent session file/storage operations also link system libuv through PkgConfig::PG_AGENT_UV. The JSONL and SQLite backends use shared message/transaction behavior. SQLite 3.54.0 and its public-domain notice were read from this Linux system package; this is not a claim of tested compatibility on other platforms. Services direct DeepSeek protocol additionally links CURL::libcurl and consumes the Core-embedded yyjson implementation.

Schema bridge headers are unmodified vendored inputs under src/services/vendor/ (the service role stages that module closure). Exact archive SHA-256 hashes, licenses and source URLs are recorded in src/services/vendor/README.md. Root integration links PanGu::services_schema for real file-tool validation, replacing its initial fixed-argument fixture; no C++ exception crosses the public C ABI. Core-only configuration does not select that target or query the C++ toolchain.

Optional image services require the matching GdkPixbuf loader deployment (including GLib/GObject/glycin on this Linux system); those system files have not been copied into the product or size-profiled. The exported Services package discovers PANGU_PIXBUF only when that image target was selected. This is separate from the verified base SDK and does not expand Core.

The Agent SDK exports PanGu::agent and PanGu::agent_app through PanGuAgentConfig.
Its static LINK_ONLY dependencies include SQLite3::SQLite3,
PkgConfig::PG_AGENT_UV and PkgConfig::PG_AGENT_UTF8PROC; app also publicly links
PanGu::services. The root selects and installs that implicit services closure even
when PANGU_BUILD_SERVICES is OFF. libuv, SQLite and utf8proc notices are installed
for the Agent-without-TUI combination. The external C-only consumer exercises a
registered model plugin and both session backends plus the installed CLI; system
shared libraries are discovered, not bundled.

The base TUI SDK exports PanGu::tui through PanGuTUIConfig with libuv,
utf8proc and platform thread dependencies. Core headers provide its view adapter
message types; the base archive does not link Core or Agent. The separate
PanGuTUIPresentation package exports PanGu::tui_plugin only in Agent-enabled
compositions, with explicit tui/core/agent/services dependencies. Linux installed
C consumers exercise both packages through real PTYs; the TUI-only combination
is also configured and consumed without Agent, Services, or C++.

Resources add libyaml 0.2.5 only when PanGu::services_resources is selected.
The installed Services config discovers PkgConfig::PANGU_YAML under the same
condition and includes its MIT notice. The installed C service consumer parses
YAML template frontmatter, expands a prompt through the registered service,
and retains a snapshot after plugin disposal. PANGU_SERVICES_RESOURCES=OFF
was configured and built separately with the six base service tests plus an
installed service consumer (7/7); that package skips YAML discovery and does
not export the resource target. This does not claim complete S05 product wiring.

Overflow classification is an optional Core-adjacent library selected by
PANGU_BUILD_OVERFLOW (OFF by default). It compiles the fixed Pi error/exclusion
patterns using PCRE2, with JS-compatible ASCII case/classes, Unicode whitespace
and non-dotall semantics; it does not replace the Core execution driver or
perform retries. PanGuOverflowConfig discovers Core and the actual PCRE2 target.
When selected by Agent, its export conditionally discovers PanGuOverflow; no
provider/storage/UI dependency is added to base Core. Linux native and original
Pi differential tests cover 107 patterns/numeric/boundary vectors and allocation
failure cleanup. System PCRE2 is not rebuilt with sanitizers or copied into the
product; its license notices are installed with the optional package.

Native extension loading uses the existing service-host libuv dependency
(`uv_dlopen/dlsym/dlclose`). External modules use installed public headers and
the injected host function table; they do not link a second host implementation.
Installed Services and Agent consumers compile such modules separately and
exercise tool/model calls, scope disposal and session reopening on Linux.
The dynamic descriptor now requires ABI 4, including current Core message and
callback layouts. ABI 1/2/3 modules must be rebuilt; the loader rejects them before
calling their factory or apply function. The descriptor export symbol retains
its bootstrap name, while the descriptor's numeric ABI controls compatibility.

`PANGU_SERVICES_MCP` selects `PanGu::services_mcp` (currently ON with Services).
Its stdio transport reuses libuv through `PanGu::services`, and JSON is provided
by Core's embedded yyjson. No new runtime dependency or C++ bridge is introduced
by the transport; the caller supplies its Schema validator. The installed C
consumer uses the existing Valijson service and a Python test peer to verify
initialization, discovery, text/image/structured tool results and disposal.
Python is a test fixture dependency, not linked or required by the MCP library;
the configured server executable is a host choice. Windows process-tree support
remains separate service work. Core-only builds do not select MCP.

`PANGU_SERVICES_MCP_HTTP` selects `PanGu::services_mcp_http`, linking the shared
`PanGu::services_mcp_client` protocol/result implementation and `CURL::libcurl`.
The stdio transport uses the same client target. The HTTP module adds no new
third-party library beyond the selected curl/Core/host closure; its curl notice
is installed when the target exists. `PanGuServicesConfig.cmake` discovers CURL
for external consumers. The installed C-only HTTP consumer exercises real
loopback discovery, a Core tool call, result ownership after disposal and
withdrawn registration; the Python peer is a test dependency only. Broader
HTTP/auth/reconnect acceptance belongs to the service role's current handoff.
