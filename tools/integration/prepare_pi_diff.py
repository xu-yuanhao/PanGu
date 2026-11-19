#!/usr/bin/env python3
"""Prepare isolated, unmodified Pi modules for repeatable Core differential tests.

Replaces manual source copies and a node_modules shim that npm pruned during
an actual partial-json install. No reference files are changed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
reference = root / 'references/pi/packages'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--full-runtime', action='store_true',
                    help='use unmodified pi-ai public entrypoint, including telemetry; no export shim')
args = parser.parse_args()
output = root / ('build/pi-full-diff' if args.full_runtime else 'build/pi-diff')
if args.full_runtime:
    output.mkdir(parents=True, exist_ok=True)
    provenance = {}
    for component, destination in (('agent', 'agent'), ('ai', 'ai/src'),
                                    ('telemetry', 'telemetry/src')):
        source = reference / component / 'src'
        target = output / destination
        shutil.copytree(source, target, dirs_exist_ok=True)
        for path in source.rglob('*'):
            if path.is_file():
                provenance[f'{component}/src/{path.relative_to(source)}'] = hashlib.sha256(path.read_bytes()).hexdigest()
    try:
        commit = subprocess.check_output(['git', '-C', str(reference.parent),
                                          'rev-parse', 'HEAD'], text=True,
                                         stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        commit = None  # Source archives are identified by the file manifest.
    (output / 'source-version.json').write_text(json.dumps({
        'reference_commit': commit,
        'entrypoint': 'unmodified ai/src/index.ts',
        'metadata_adaptation': 'source TypeScript exports; injected-model runtime dependencies only',
    }, indent=2) + '\n')
    # Node's native TypeScript loader consumes the copied original entrypoints.
    # Only package metadata selects this Core-only runtime; provider factories,
    # SDKs, catalogs and OAuth implementations are not exercised or benchmarked.
    agent = json.loads((reference / 'agent/package.json').read_text())
    ai = json.loads((reference / 'ai/package.json').read_text())
    for component in ('ai', 'telemetry'):
        original = json.loads((reference / component / 'package.json').read_text())
        (output / component / 'reference-package.json').write_text(json.dumps(original, indent=2) + '\n')
        package = {'name': original['name'], 'version': original['version'],
                   'type': 'module', 'exports': './src/index.ts', 'dependencies': {}}
        if component == 'ai':
            package['dependencies'] = {'@earendil-works/pi-telemetry': 'file:../telemetry',
                                       'typebox': agent['dependencies']['typebox'],
                                       'partial-json': ai['dependencies']['partial-json']}
        (output / component / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
    utilities = output / 'utils'
    if not utilities.exists():
        utilities.symlink_to('ai/src/utils', target_is_directory=True)
    package = {'private': True, 'type': 'module', 'dependencies': {
        '@earendil-works/pi-ai': 'file:./ai',
        '@earendil-works/pi-telemetry': 'file:./telemetry',
        'typebox': agent['dependencies']['typebox'],
        'partial-json': ai['dependencies']['partial-json']}}
    (output / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
    (output / 'source-sha256.json').write_text(json.dumps(provenance, indent=2, sort_keys=True) + '\n')
    subprocess.run(['npm', 'install', '--prefix', str(output), '--ignore-scripts', '--no-audit', '--no-fund'], check=True)
    print(f'Prepared Pi public-entrypoint differential modules: {output}')
    raise SystemExit(0)
(output / 'agent').mkdir(parents=True, exist_ok=True)
(output / 'utils').mkdir(exist_ok=True)
(output / 'shim').mkdir(exist_ok=True)
for name in ('agent', 'agent-loop', 'stream-fn', 'types', 'proxy'):
    shutil.copyfile(reference / f'agent/src/{name}.ts', output / f'agent/{name}.ts')
for name in ('event-stream', 'identity', 'version', 'transcript', 'text', 'validation', 'streaming-json', 'json-parse', 'overflow'):
    shutil.copyfile(reference / f'ai/src/utils/{name}.ts', output / f'utils/{name}.ts')
shim = {'name': '@earendil-works/pi-ai', 'version': '1.4.0', 'type': 'module', 'exports': './index.js'}
(output / 'shim/package.json').write_text(json.dumps(shim) + '\n')
(output / 'shim/index.js').write_text(''.join(
    f"export * from '../utils/{name}.ts';\n"
    for name in ('event-stream', 'identity', 'transcript', 'validation', 'streaming-json')))
ai = json.loads((reference / 'ai/package.json').read_text())
agent = json.loads((reference / 'agent/package.json').read_text())
package = {'private': True, 'type': 'module', 'dependencies': {
    '@earendil-works/pi-ai': 'file:./shim',
    'typebox': agent['dependencies']['typebox'],
    'partial-json': ai['dependencies']['partial-json'],
}}
(output / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
subprocess.run(['npm', 'install', '--prefix', str(output), '--ignore-scripts', '--no-audit', '--no-fund'], check=True)
print(f'Prepared Pi differential modules: {output}')
