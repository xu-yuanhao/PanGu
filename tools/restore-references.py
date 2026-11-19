#!/usr/bin/env python3
"""Restore pinned read-only references. Run from any directory; requires Git/network."""
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

root = Path(__file__).resolve().parents[1]
refs = root / 'references'
refs.mkdir(exist_ok=True)
for item in json.loads((root / 'tools/references.json').read_text()):
    target = refs / item['name']
    if target.exists():
        head = subprocess.check_output(['git', '-C', str(target), 'rev-parse', 'HEAD'], text=True).strip()
        dirty = subprocess.check_output(['git', '-C', str(target), 'status', '--porcelain'], text=True).strip()
        if head != item['commit'] or dirty:
            raise SystemExit(f'Refusing to overwrite existing {target}')
    else:
        subprocess.run(['git', 'init', str(target)], check=True)
        subprocess.run(['git', '-C', str(target), 'remote', 'add', 'origin', item['url']], check=True)
        subprocess.run(['git', '-C', str(target), 'fetch', '--depth=1', 'origin', item['commit']], check=True)
        subprocess.run(['git', '-C', str(target), 'checkout', '--detach', 'FETCH_HEAD'], check=True)
    if item['source_rev']:
        actual = (target / 'SOURCE_REV').read_text().strip()
        if actual != item['source_rev']:
            raise SystemExit(f'SOURCE_REV mismatch: {target}')
    print(item['name'], item['commit'])

bun = json.loads((root / 'tools/bun-reference.json').read_text())
target = refs / 'bun-porting'
target.mkdir(exist_ok=True)
for item in bun['files']:
    path = target / item['name']
    data = path.read_bytes() if path.exists() else urllib.request.urlopen(item['url']).read()
    if hashlib.sha256(data).hexdigest() != item['sha256']:
        raise SystemExit(f'Bun source hash mismatch: {path}')
    if not path.exists():
        path.write_bytes(data)
# API metadata can change formatting/current PR state; fixed source files above are hashed.
for name, url in bun['metadata'].items():
    path = target / name
    if not path.exists():
        path.write_bytes(urllib.request.urlopen(url).read())
if not (target / 'SOURCE.md').exists():
    (target / 'SOURCE.md').write_text(bun['source'])
print('References restored. No existing worktree was overwritten.')
