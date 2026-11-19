#!/usr/bin/env python3
"""Linux Core resource measurements with equivalent injected-model Pi workloads.

No provider/network/storage/UI claims. Every sample verifies final byte, message
and lifecycle-event totals before retaining measurements. Reports actual runtime
executables, source provenance and workload parameters.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
from resource_sampling import sample

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native', type=Path, default=root / 'build/core-profile/pangu_core_profile')
parser.add_argument('--pi', type=Path, default=root / 'build/pi-full-diff')
parser.add_argument('--output', type=Path, default=root / 'build/core-profile/results.json')
parser.add_argument('--repeats', type=int, default=3)
args = parser.parse_args()
if platform.system() != 'Linux' or not 1 <= args.repeats <= 20:
    parser.error('Linux /proc and 1..20 repeats are required')
native, pi = args.native.resolve(), args.pi.resolve()
manifest = json.loads((pi / 'source-sha256.json').read_text())
for name, expected in manifest.items():
    component, relative = name.split('/src/', 1)
    target = pi / ('agent' if component == 'agent' else f'{component}/src') / relative
    if hashlib.sha256(target.read_bytes()).hexdigest() != expected:
        raise RuntimeError(f'Copied Pi source changed: {name}')
node = subprocess.check_output(['which', 'node'], text=True).strip()


commands = {'pangu': [str(native)], 'pi': [node, str(root / 'tools/integration/profile-core.mjs'), str(pi)]}
scenes = {'small': (1, 1, 16, 256), 'stream_1mib': (1, 1, 256, 4096),
          'many_sessions': (128, 4, 16, 256)}
report = {'platform': platform.platform(), 'machine': platform.machine(),
          'node': subprocess.check_output([node, '--version'], text=True).strip(),
          'compiler': subprocess.check_output(['cc', '--version'], text=True).splitlines()[0],
          'native_sha256': hashlib.sha256(native.read_bytes()).hexdigest(),
          'native_bytes': native.stat().st_size,
          'pi_provenance': json.loads((pi / 'source-version.json').read_text()),
          'scope': 'Core only; no provider SDK/network, Agent storage, TUI, optional plugins or distribution-size comparison',
          'caveats': ['Sequential active sessions retained together; no concurrency claim.',
                      'Peak RSS is /proc VmHWM since exec, sampled with final state retained; idle RSS after instance creation.',
                      'Startup includes process/import/instance creation; native is CMake Release.',
                      'CPU includes startup/work/cleanup; idle ticks have kernel clock granularity.',
                      'Work duration includes final metrics; cleanup separately timed, no warmup excluded.',
                      'Pi TypeScript source entrypoints, not transpiled published distribution.',
                      'Native byte count is executable only, not an installation/dependency footprint.'],
          'commands': commands, 'results': {}}
for name, shape in scenes.items():
    result = {'shape': dict(zip(('instances', 'turns', 'chunks', 'chunk_bytes'), shape)), 'hosts': {}}
    for host, command in commands.items():
        samples = [sample(command, shape) for _ in range(args.repeats)]
        result['hosts'][host] = {'samples': samples,
            'median': {key: statistics.median(s[key] for s in samples)
                       for key in samples[0] if key != 'outcome'}}
    report['results'][name] = result
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(f'Verified resource measurements: {args.output}')
for name, result in report['results'].items():
    print(name, {host: {key: round(data['median'][key], 2)
                       for key in ('startup_ms', 'work_ms', 'peak_rss_kib', 'idle_rss_kib')}
                 for host, data in result['hosts'].items()})
