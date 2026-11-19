#!/usr/bin/env python3
"""Measure an actual Linux CMake SDK install and CLI shared-library closure.

Installed files and system dependencies are reported separately; system files
are inspected, never copied. This is not a portable deployment bundle.
"""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--component')
parser.add_argument('--label', default='baseline', help='unique name for this configuration snapshot')
parser.add_argument('--strip-tool', help='also measure a stripped CLI copy with this target-compatible strip tool')
args = parser.parse_args()
if not re.fullmatch(r'[A-Za-z0-9_-]+', args.label):
    parser.error('label must contain only ASCII letters, digits, underscore or dash')
if platform.system() != 'Linux':
    parser.error('Linux ldd measurement is required')
build = args.build_dir.resolve()
cache = (build / 'CMakeCache.txt').read_text()
if 'CMAKE_BUILD_TYPE:STRING=Release' not in cache:
    parser.error('Configure a Release build first')
suffix = (args.component or 'all') + '-' + args.label
prefix = build / ('profile-install-' + suffix)
if prefix.exists():
    parser.error(f'Measurement prefix already exists: {prefix}; use a fresh build/prefix')
command = ['cmake', '--install', str(build), '--prefix', str(prefix), '--config', 'Release']
if args.component:
    command += ['--component', args.component]
subprocess.run(command, check=True, stdout=subprocess.PIPE, text=True)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

files = []
for path in sorted(prefix.rglob('*')):
    if path.is_file() and not path.is_symlink():
        files.append({'path': str(path.relative_to(prefix)), 'bytes': path.stat().st_size,
                      'sha256': digest(path)})
groups = {}
for entry in files:
    category = entry['path'].split('/')[0]
    groups[category] = groups.get(category, 0) + entry['bytes']

# Reproducible gzip SDK archive size; excludes external system dependencies.
buffer = io.BytesIO()
with gzip.GzipFile(fileobj=buffer, mode='wb', mtime=0, filename='') as compressed:
    with tarfile.open(fileobj=compressed, mode='w') as archive:
        for entry in files:
            path = prefix / entry['path']
            info = archive.gettarinfo(str(path), arcname=entry['path'])
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            info.mtime = 0
            with path.open('rb') as contents:
                archive.addfile(info, contents)
package = buffer.getvalue()
args.output.parent.mkdir(parents=True, exist_ok=True)
archive_path = build / ('profile-sdk-' + suffix + '.tar.gz')
archive_path.write_bytes(package)
cli = prefix / 'bin/pangu_agent_cli'
cli_measurement = None
if cli.exists():
    cli_bytes = cli.read_bytes()
    cli_measurement = {'installed_bytes': len(cli_bytes),
                       'gzip_bytes': len(gzip.compress(cli_bytes, mtime=0)),
                       'sha256': digest(cli),
                       'scope': 'CLI executable only; excludes notices, SDK and shared dependencies'}
    if args.strip_tool:
        strip_tool = shutil.which(args.strip_tool)
        if strip_tool is None:
            parser.error(f'Strip tool not found: {args.strip_tool}')
        stripped = build / ('profile-cli-' + suffix + '.stripped')
        strip_command = [strip_tool, '--strip-unneeded', '-o', str(stripped), str(cli)]
        subprocess.run(strip_command, check=True)
        stripped_bytes = stripped.read_bytes()
        cli_measurement['stripped'] = {
            'command': strip_command, 'path': str(stripped),
            'bytes': len(stripped_bytes), 'sha256': digest(stripped),
            'gzip_bytes': len(gzip.compress(stripped_bytes, mtime=0))}
dependencies = []
ldd_output = ''
if cli.exists():
    ldd_output = subprocess.check_output(['ldd', str(cli)], text=True)
    if 'not found' in ldd_output:
        raise RuntimeError('Installed CLI has unresolved shared-library dependencies')
    paths = set()
    for line in ldd_output.splitlines():
        match = re.search(r'(?:=>\s*)?(/\S+)\s+\(', line)
        if match:
            paths.add(Path(match.group(1)).resolve())
    dependencies = [{'path': str(path), 'bytes': path.stat().st_size, 'sha256': digest(path)}
                    for path in sorted(paths)]
report = {'platform': platform.platform(), 'machine': platform.machine(),
          'scope': 'Release SDK install; system shared dependencies inspected separately, not bundled',
          'build_directory': str(build), 'install_command': command,
          'label': args.label,
          'cmake_cache_sha256': digest(build / 'CMakeCache.txt'),
          'configuration': [line for line in cache.splitlines()
                            if re.match(r'(PANGU_|BUILD_SHARED_LIBS:|CMAKE_(BUILD_TYPE|C_FLAGS_RELEASE|CXX_FLAGS_RELEASE):)', line)],
          'installed_files': files, 'installed_bytes': sum(f['bytes'] for f in files),
          'installed_groups': groups, 'sdk_archive_bytes': len(package),
          'sdk_archive_sha256': hashlib.sha256(package).hexdigest(),
          'sdk_archive_path': str(archive_path),
          'cli_executable': cli_measurement,
          'shared_dependencies': dependencies, 'ldd_output': ldd_output,
          'unique_shared_dependency_bytes': sum(f['bytes'] for f in dependencies),
          'caveats': ['SDK includes static development archives and headers; not just application runtime.',
                      'System dependency bytes are unique resolved files reported by ldd, including transitive libraries.',
                      'No external server executable, dynamically loaded extension, rg/fd, trust store or terminal/font data is bundled.',
                      'No Pi distribution, startup, memory or other-platform comparison is claimed.',
                      'Executables/archives are unstripped CMake Release outputs.']}
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: report[key] for key in ('installed_bytes', 'sdk_archive_bytes', 'unique_shared_dependency_bytes')}))
