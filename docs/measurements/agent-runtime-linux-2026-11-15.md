# Agent app and storage resource workloads

Linux x86-64, GCC 16.2.1, CMake Release `-O3 -DNDEBUG`, 2026-11-15. The C17 host links `PanGu::agent_app` and drives real Core execution and JSONL/SQLite stores through public APIs. It injects a cumulative text model; no TUI, model-provider, tool or resource plugin is installed. Overflow is OFF for these Release samples. This is an Agent/storage workload, separate from the [installed default CLI/TUI measurement](product-runtime-linux-2026-11-15.md).

Each scenario/backend ran three times. All 18 samples verified exact message counts, roles, final stop reasons, every content byte and Core event totals. After sampling the retained state, the host closed and reopened every store and verified content again. Raw samples, build options and executable SHA-256 are in [the JSON report](agent-runtime-linux-2026-11-15.json).

| Scenario | Store | Startup ms | Work ms | Idle RSS KiB | Retained RSS KiB | Peak RSS KiB | Store bytes |
|---|---|---:|---:|---:|---:|---:|---:|
| Small stream | JSONL | 2.26 | 0.42 | 5,780 | 5,988 | 5,988 | 14,105 |
| Small stream | SQLite | 2.84 | 0.40 | 7,328 | 7,444 | 7,444 | 16,384 |
| 1 MiB cumulative stream | JSONL | 2.81 | 14.32 | 6,808 | 15,128 | 19,184 | 2,103,065 |
| 1 MiB cumulative stream | SQLite | 3.11 | 14.85 | 8,408 | 18,652 | 24,684 | 2,109,440 |
| 32 sessions × 4 turns | JSONL | 2.92 | 20.72 | 5,924 | 15,172 | 15,172 | 6,235,264 |
| 32 sessions × 4 turns | SQLite | 10.54 | 34.32 | 12,140 | 16,760 | 16,760 | 2,752,512 |

Values are medians. Small: one session/turn, sixteen cumulative 256-byte chunks. Large stream: one session/turn, 256 cumulative 4096-byte chunks. Many sessions: 32 allocated apps/stores retained together, each runs four turns of sixteen 256-byte chunks, sequentially. Each turn adds a seven-byte USER and the complete assistant; expected events are `chunks + 7`. The shared model buffer is included in process memory.

Startup includes loader/plugin setup and creation of all stores. Work includes streaming, durable commits and exact final-state verification. Cleanup timing in the raw data includes close/reopen verification and release. `/proc` VmHWM is sampled before reopen, so the peak column covers the measured workload through retained state. A 200ms idle interval observed no CPU tick growth; kernel tick granularity limits that statement. Process CPU totals use `wait4` user/system time, while RSS deliberately uses `/proc` after exec.

Store bytes are logical regular-file lengths after closing/reopening. JSONL currently writes full committed snapshots; SQLite uses DELETE journal mode and FULL synchronous commits. The four-turn sample already shows a larger JSONL log. It does not establish long-duration growth or power-loss safety; parent-directory fsync and multi-writer support remain separate runtime requirements. Filesystem cache and disk device affect these timings. There is no equivalent full Pi product/storage run here, so this report makes no Pi product comparison.

Validation and reproduction:

```sh
cmake -S . -B build/product-profile -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DPANGU_BUILD_AGENT=ON -DPANGU_BUILD_SERVICES=ON -DPANGU_BUILD_TUI=ON -DPANGU_BUILD_PROFILES=ON -DPANGU_BUILD_OVERFLOW=OFF -DPANGU_BUILD_PROXY_HTTP=OFF -DPANGU_SERVICES_IMAGES=OFF -DPANGU_SERVICES_MCP=ON -DPANGU_SERVICES_RESOURCES=ON
cmake --build build/product-profile --target pangu_agent_profile --parallel 2
python3 tools/integration/profile_agent.py --native build/product-profile/pangu_agent_profile --output build/product-profile/agent-runtime.json
```

The same C host passed all six backend/scenario combinations under ASan/UBSan, including reopened content. Shared sampling helpers were verified with the existing Core/Pi three-scenario pipeline (one repeat) and both installed TUI combinations (one repeat); those compatibility samples remain build-local and do not replace the published baselines. No production implementation changed. Network/tool/MCP subprocess costs, concurrent scheduling, Pi product comparison and other-platform execution remain outstanding C06 work.
