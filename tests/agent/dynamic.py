"""Explicit native paths through product CLI, without external model access."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
cli, module = [str(Path(p).resolve()) for p in sys.argv[1:]]
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    env = os.environ.copy()
    env['PANGU_AGENT_DIR'] = tmp
    env['PANGU_DEEPSEEK_ENDPOINT'] = 'http://127.0.0.1:1'
    env.pop('PANGU_CONTEXT_TOKENS', None)
    for suffix in ('jsonl', 'db'):
        path = root / ('session.' + suffix)
        flags = ['--sqlite'] if suffix == 'db' else []
        args = [cli, 'create', str(path), *flags, '--cwd', str(Path(module).parent),
                '--plugin', './' + Path(module).name, '--deepseek', 'native input']
        run = subprocess.run(args, env=env, capture_output=True, text=True, timeout=5)
        assert run.returncode == 0, (run.stdout, run.stderr)
        assert 'native executed' in run.stdout and 'native completed' in run.stdout
        assert run.stdout.count('native cleanup') == 1
        reopened = subprocess.check_output([cli, 'open', str(path), *flags], env=env, text=True)
        assert 'native executed' in reopened and 'native completed' in reopened
        assert 'native cleanup' not in reopened  # Paths are not silently reloaded.
        # Sequential explicit paths replace services and clean up both modules.
        run = subprocess.run([cli, 'open', str(path), *flags, '--plugin', module,
                              '--plugin', module, '--deepseek', 'again'],
                             env=env, capture_output=True, text=True, timeout=5)
        assert run.returncode == 0, (run.stdout, run.stderr)
        assert run.stdout.count('native cleanup') == 2
        run = subprocess.run([cli, 'open', str(path), *flags, '--plugin', module,
                              '--plugin', str(root / 'missing'), '--deepseek', 'never'],
                             env=env, capture_output=True, text=True, timeout=5)
        assert run.returncode != 0 and run.stdout.count('native cleanup') == 1
        assert 'native plugin load failed' in run.stderr
        assert 'never' not in subprocess.check_output([cli, 'open', str(path), *flags], env=env, text=True)
    for tail in (['--plugin'], ['--plugin', '']):
        run = subprocess.run([cli, 'create', str(root / 'invalid'), *tail], env=env, capture_output=True)
        assert run.returncode == 2 and not (root / 'invalid').exists()
print('CLI native relative/absolute/order/failure/close/two-backend reopen passed')
