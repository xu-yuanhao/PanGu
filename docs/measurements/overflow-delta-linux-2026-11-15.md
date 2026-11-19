# Linux Release default and overflow combination cost

Measured on 2026-11-15 after the 57-test integration snapshot, Linux x86-64, GCC/G++ 16.2.1, Release `-O3 -DNDEBUG`. Both combinations use the same 163 production/build input files, recorded by SHA-256 in the [comparison manifest](overflow-delta-linux-2026-11-15.json). The reported configuration differs only in `PANGU_BUILD_OVERFLOW=OFF` versus `ON`.

| Measurement, bytes | Default | With overflow | Difference |
|---|---:|---:|---:|
| SDK installed regular files | 4,019,454 | 4,049,251 | +29,797 |
| SDK deterministic tar.gz | 1,366,919 | 1,374,714 | +7,795 |
| Original CLI | 1,356,240 | 1,365,688 | +9,448 |
| Stripped CLI | 1,192,472 | 1,200,728 | +8,256 |
| Stripped CLI gzip | 552,192 | 555,065 | +2,873 |
| Unique resolved system-library bytes | 26,408,080 | 26,408,080 | 0 |

The default SDK has 70 regular files; overflow adds seven. SDK figures include libraries, headers, CMake metadata, notices and CLI. CLI gzip figures include only executable bytes, without notices or archive metadata; they are not installers. System libraries are inspected, never copied into these SDK archives. Full reports contain hashes, install commands and resolved dependencies: [default](sdk-default-linux-2026-11-15.json), [overflow](sdk-overflow-linux-2026-11-15.json).

Both dependency closures contain the same PCRE2 shared file, `/usr/lib/libpcre2-8.so.0.16.1`, 727,336 bytes. The baseline already loads it through this host's libgit2. Overflow directly links PCRE2; a different libgit2 or deployment can therefore have a nonzero dependency increment. This measurement does not establish a universal zero cost.

The installed and stripped CLI in each combination completed deterministic create/open with identical output in four isolated agent directories (4/4). The source manifest remained unchanged through the comparison. This validates Release linking and basic session operation, not runtime classification, startup, RSS, CPU, live-model behavior or a portable distribution. Runtime overflow behavior is covered separately by the integration snapshot; no new live-model run was performed for footprint measurement.

Commands (the default configure also selects Agent/Services/TUI/resources/MCP, with proxy HTTP and images OFF):

```sh
cmake -S . -B build/product-profile -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DPANGU_BUILD_AGENT=ON -DPANGU_BUILD_SERVICES=ON -DPANGU_BUILD_TUI=ON -DPANGU_BUILD_OVERFLOW=OFF -DPANGU_BUILD_PROXY_HTTP=OFF -DPANGU_SERVICES_IMAGES=OFF -DPANGU_SERVICES_MCP=ON -DPANGU_SERVICES_RESOURCES=ON
cmake --build build/product-profile --target pangu_agent_cli --parallel 2
python3 tools/integration/profile_install.py --build-dir build/product-profile --label approval-baseline --strip-tool strip --output docs/measurements/sdk-default-linux-2026-11-15.json
cmake -S . -B build/product-profile -DPANGU_BUILD_OVERFLOW=ON
cmake --build build/product-profile --target pangu_agent_cli --parallel 2
python3 tools/integration/profile_install.py --build-dir build/product-profile --label approval-overflow --strip-tool strip --output docs/measurements/sdk-overflow-linux-2026-11-15.json
```

Use fresh labels for reruns: measurement prefixes are preserved. These replace neither the earlier historical SDK snapshots nor the remaining C06 requirements: other optional combinations, external programs/data, Pi distribution comparison, process resources and platform execution still require measurement.
