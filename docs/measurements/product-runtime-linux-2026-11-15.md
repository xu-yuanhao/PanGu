# Installed CLI/TUI process resources

Linux x86-64, 2026-11-15. These measurements use the exact installed Release CLI executables from the [default/overflow footprint comparison](overflow-delta-linux-2026-11-15.md), verified by matching SHA-256. Both include Agent, Services, TUI, resources and MCP support; the optional classifier is installed/initialized only in the overflow combination. All inputs and agent directories are isolated. The deterministic demo model performs one text turn without executing tools or network requests.

Each combination ran three times in a real PTY. Every sample verified the saved USER input and assistant response by reopening the session, successful process exit and exact terminal-mode restoration. Raw samples, CLI hashes, platform and measurement caveats are in [the JSON report](product-runtime-linux-2026-11-15.json).

| Median measurement | Default | With overflow |
|---|---:|---:|
| Process start → observed command banner, ms | 5.63 | 7.13 |
| Idle RSS, KiB | 9,452 | 9,588 |
| RSS retained after one turn, KiB | 9,652 | 9,844 |
| Peak RSS through one turn, KiB | 9,652 | 9,844 |
| Input → observed usage output, ms | 5.56 | 5.68 |
| Idle CPU tick growth over approximately 500ms | None observed | None observed |

Startup includes the loader, plugin/session/UI setup and PTY observation through the banner. Turn timing includes input handling, deterministic model, storage and rendering through usage output. Filesystem caches are warm; these three-sample medians do not establish a stable 1.50ms startup penalty. RSS/VmHWM come from `/proc/PID/status` after exec, including system-library pages resident as used, rather than summing library file sizes. Peak is sampled while the final session remains live.

This host has 100 CPU ticks per second (10ms granularity). The sampled idle and turn CPU counters did not advance; neither result proves zero CPU consumption. No request latency, live model, external MCP process, file/command tool workload, large stream, many-session or Pi product comparison is claimed. Those remain separate C06 measurements.

Reproduce using preserved snapshot prefixes (the script reuses the existing PTY harness):

```sh
python3 tools/integration/profile_product.py --default-cli build/product-profile/profile-install-all-approval-baseline/bin/pangu_agent_cli --overflow-cli build/product-profile/profile-install-all-approval-overflow/bin/pangu_agent_cli --output build/product-profile/product-runtime.json
```

All six samples passed their behavior checks. The measurement script also passed `python3 -m py_compile tools/integration/profile_product.py`. Production code and the previously passing 57-test integration snapshot were unchanged by this measurement.
