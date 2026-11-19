#!/usr/bin/env python3
"""Linux Agent app/storage resource measurements with verified reopen."""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import statistics
import tempfile
from resource_sampling import sample

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--repeats', type=int, default=3)
args = parser.parse_args()
if platform.system() != 'Linux' or not 1 <= args.repeats <= 20:
    parser.error('Linux /proc and 1..20 repeats are required')
native = args.native.resolve()
cache_path = native.parent / 'CMakeCache.txt'
cache = cache_path.read_text() if cache_path.exists() else ''
scenes = {'small': (1, 1, 16, 256), 'stream_1mib': (1, 1, 256, 4096),
          'many_sessions': (32, 4, 16, 256)}
report = {'platform': platform.platform(), 'machine': platform.machine(),
          'native': str(native), 'native_sha256': hashlib.sha256(native.read_bytes()).hexdigest(),
          'configuration': [line for line in cache.splitlines() if line.startswith(
              ('CMAKE_BUILD_TYPE:', 'CMAKE_C_FLAGS_RELEASE:', 'PANGU_BUILD_', 'PANGU_SERVICES_'))],
          'scope': 'Agent app/Core execution plugin, injected model, real JSONL/SQLite stores; no TUI or provider/tool/resource plugins',
          'caveats': ['Sessions remain allocated together; execution is sequential, not concurrent.',
                      'Startup includes process/plugins and opening all new stores.',
                      'Work includes model streaming, durable commits and exact final-message verification.',
                      'Cleanup includes closing and reopening every store with byte-exact content verification.',
                      'Peak RSS is /proc VmHWM through retained final state; excludes subsequent reopen/exit.',
                      'Store bytes are logical regular-file lengths after close/reopen, not filesystem allocated blocks.',
                      'No Pi product comparison, live model/network, actual tools, external MCP child or other-platform result.'],
          'results': {}}
for name, shape in scenes.items():
    scene = {'shape': dict(zip(('instances','turns','chunks','chunk_bytes'), shape)), 'backends': {}}
    for backend in ('jsonl', 'sqlite'):
        samples = []
        for _ in range(args.repeats):
            with tempfile.TemporaryDirectory(prefix='pangu-agent-profile-') as directory:
                result = sample([str(native), backend, directory], shape)
                result['store_bytes'] = sum(p.stat().st_size for p in Path(directory).rglob('*') if p.is_file())
                samples.append(result)
        scene['backends'][backend] = {'samples': samples,
            'median': {key: statistics.median(row[key] for row in samples)
                       for key in samples[0] if key != 'outcome'}}
        print(name, backend, json.dumps(scene['backends'][backend]['median']), flush=True)
    report['results'][name] = scene
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
