"""CLI argument validation and backend selection; network-free."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

cli = str(Path(sys.argv[1]).resolve())
for backend in ('jsonl', 'sqlite'):
    path = Path('cli-options.' + backend).resolve()
    path.unlink(missing_ok=True)
    flag = ['--sqlite'] if backend == 'sqlite' else []
    created = subprocess.check_output([cli, 'create', str(path), *flag,
                                      '--demo', 'first', '--demo', 'second'], text=True)
    assert created.count('assistant: PanGu deterministic demo response') == 2
    opened = subprocess.check_output([cli, 'open', str(path), *flag], text=True)
    assert created == opened
    tree = subprocess.check_output([cli, 'open', str(path), *flag,
                                    '--switch', '2', '--demo', 'branch', '--tree'], text=True)
    assert 'entry=5 parent=2' in tree
    compacted = subprocess.check_output([cli, 'open', str(path), *flag,
                                         '--switch', '4', '--compact-summary', '3', 'synthetic summary',
                                         '--tree', '--usage'], text=True)
    assert 'kind=compaction kept=3' in compacted
    restored = subprocess.check_output([cli, 'open', str(path), *flag, '--switch', '6'], text=True)
    branch = subprocess.check_output([cli, 'open', str(path), *flag], text=True)
    assert 'user: branch' in branch and 'user: second' not in branch
    queued = subprocess.check_output([cli, 'open', str(path), *flag,
                                       '--steer', 'queued-steer', '--follow-up', 'queued-follow', '--queues'], text=True)
    assert 'Queues: steering=1 follow-up=1' in queued
    pending = subprocess.check_output([cli, 'open', str(path), *flag, '--queues'], text=True)
    assert 'Queues: steering=1 follow-up=1' in pending
    resumed = subprocess.check_output([cli, 'open', str(path), *flag, '--resume-demo', '--queues'], text=True)
    assert resumed.count('user: queued-steer') == 1 and resumed.count('user: queued-follow') == 1
    assert 'Queues: steering=0 follow-up=0' in resumed
    cleared = subprocess.check_output([cli, 'open', str(path), *flag,
                                       '--follow-up', 'discard', '--clear-queue', 'follow-up', '--queues'], text=True)
    assert 'Queues: steering=0 follow-up=0' in cleared
    path.unlink()
path = Path('cli-invalid.jsonl').resolve()
path.unlink(missing_ok=True)
for options in (['--unknown'], ['--demo'], ['--demo', 'one', '--tui-demo']):
    result = subprocess.run([cli, 'create', str(path), *options], capture_output=True)
    assert result.returncode == 2
    assert not path.exists()
env = os.environ.copy()
env.pop('DEEPSEEK_API_KEY', None)
# Missing auth is a request failure after session creation, not a CLI env gate.
# Isolate user credentials and forbid accidental external model access.
with tempfile.TemporaryDirectory() as empty:
    env['PANGU_AGENT_DIR'] = empty
    env['PANGU_DEEPSEEK_ENDPOINT'] = 'http://127.0.0.1:1/chat/completions'
    result = subprocess.run([cli, 'create', str(path), '--deepseek', 'no key'],
                            env=env, capture_output=True, timeout=5)
assert result.returncode != 0
path.unlink(missing_ok=True)
print('CLI repeated input, both backends, argument/no-key failures passed')

env['DEEPSEEK_API_KEY'] = 'synthetic-test-key'
env['PANGU_CONTEXT_TOKENS'] = 'not-a-number'
result = subprocess.run([cli, 'create', str(path), '--deepseek', 'invalid config'], env=env, capture_output=True)
assert result.returncode != 0 and not path.exists()
